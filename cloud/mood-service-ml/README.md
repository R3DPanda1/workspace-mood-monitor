# Mood Service ML

**Machine Learning-powered Mood Score Computation**

This service replaces the heuristic-based mood scoring with a machine learning model that learns optimal workspace conditions from real data. It maintains full compatibility with the oneM2M infrastructure while providing more accurate mood predictions.

## Overview

The ML mood service:
- Receives sensor telemetry via oneM2M notifications (CO₂, noise, light, temperature, humidity, occupancy)
- Uses a trained scikit-learn model to predict mood scores (0-100)
- Blends ML predictions with a heuristic fallback for robustness
- Applies runtime calibration to avoid harsh low scores
- Controls LED feedback via oneM2M PUT commands
- Stores predictions in PostgreSQL (`fact_mood_ml` table)

## Architecture

```
oneM2M Notification → Feature Extraction → ML Model → Calibration → LED Control
                           ↓                  ↓            ↓
                    Latest-value cache    Heuristic   PostgreSQL
                                          Fallback    fact_mood_ml
```

### Key Features

**Intelligent Feature Handling:**
- Per-room/desk latest-value cache with 15-minute TTL
- Missing sensor values filled from cache or defaults
- Handles synonym normalization (temperature/temp, humidity/rh, etc.)

**Robust Scoring:**
- ML model predictions blended with heuristic baseline
- Runtime calibration via environment variables
- Softening function to avoid extreme scores
- Configurable thresholds for focus/neutral/tired labels

**Production-Ready:**
- Lazy model loading
- Graceful fallback if model unavailable
- Comprehensive logging
- Debug mode for tuning

## Configuration

### Environment Variables

| Variable | Default | Description |
|----------|---------|-------------|
| `MOOD_MODEL_PATH` | `mood_model.pkl` | Path to trained model file |
| `ML_BLEND_HEURISTIC` | `0.2` | Blend weight (0=pure ML, 1=pure heuristic) |
| `SOFTENING_CENTER` | `60` | Score center for softening function |
| `SOFTENING_FACTOR` | `0.9` | Multiplier for distance from center |
| `SCORE_BIAS` | `10` | Positive bias to lift borderline scores |
| `THRESHOLD_FOCUS` | `70` | Score threshold for "focus" label |
| `THRESHOLD_NEUTRAL` | `40` | Score threshold for "neutral" label |
| `MOOD_ML_DEBUG` | `0` | Enable debug output (1) |
| `LATEST_TTL_SEC` | `900` | Cache TTL for latest sensor values (seconds) |
| `DATABASE_URL` | `postgresql://...` | PostgreSQL connection string |
| `CSE_BASE` | `http://acme:8080/...` | oneM2M CSE base URL |
| `CSE_ORIGIN` | `admin:admin` | oneM2M credentials |

### Runtime Tuning

The service supports runtime calibration without rebuilding the model:

```yaml
# docker-compose.yml
environment:
  - ML_BLEND_HEURISTIC=0.2    # 80% ML, 20% heuristic
  - SOFTENING_CENTER=60        # Pull scores toward 60
  - SOFTENING_FACTOR=0.9       # Gentle softening
  - SCORE_BIAS=10              # Add +10 to lift borderline cases
  - THRESHOLD_FOCUS=70         # Focus if score >= 70
  - THRESHOLD_NEUTRAL=40       # Neutral if 40 <= score < 70
```

## ML Model Training Tutorial

### Prerequisites

- Python 3.9+
- Access to historical sensor data (PostgreSQL or CSV)
- Labeled mood scores or ground truth data

### Step 1: Prepare Training Data

The model expects 6 features in this order:
1. `co2` - CO₂ concentration (ppm)
2. `noise` - Noise level (dB)
3. `lux` - Light intensity (lux)
4. `temp` - Temperature (°C)
5. `rh` - Relative humidity (%)
6. `occ` - Occupancy count

Create a training script `train_model.py`:

```python
import numpy as np
import pandas as pd
from sklearn.ensemble import RandomForestRegressor
from sklearn.model_selection import train_test_split
from sklearn.metrics import mean_squared_error, r2_score
import joblib

# Load your data (example: CSV with sensor readings and mood scores)
# You can also query from PostgreSQL fact_telemetry table
data = pd.read_csv('training_data.csv')

# Expected columns: co2, noise, lux, temp, rh, occ, mood_score
# mood_score should be your ground truth (0-100)

# Feature matrix (must be in this exact order!)
X = data[['co2', 'noise', 'lux', 'temp', 'rh', 'occ']].values

# Target variable (mood scores 0-100)
y = data['mood_score'].values

# Split data
X_train, X_test, y_train, y_test = train_test_split(
    X, y, test_size=0.2, random_state=42
)

# Train model (you can experiment with different models)
model = RandomForestRegressor(
    n_estimators=100,
    max_depth=10,
    min_samples_split=5,
    random_state=42
)

model.fit(X_train, y_train)

# Evaluate
y_pred = model.predict(X_test)
mse = mean_squared_error(y_test, y_pred)
r2 = r2_score(y_test, y_pred)

print(f"Mean Squared Error: {mse:.2f}")
print(f"R² Score: {r2:.3f}")

# Save model
joblib.dump(model, 'mood_model.pkl')
print("Model saved to mood_model.pkl")
```

### Step 2: Prepare Training Data

#### Option A: Export from PostgreSQL

```bash
# Export telemetry and existing mood scores
docker exec -i onem2m_postgres psql -U onem2m -d onem2m -c "
COPY (
  SELECT
    t.co2, t.noise, t.lux, t.temp, t.rh, t.occ,
    m.score as mood_score
  FROM fact_telemetry t
  JOIN fact_mood m ON t.ts_cse = m.ts_cse
  WHERE t.co2 IS NOT NULL
    AND t.noise IS NOT NULL
    AND t.lux IS NOT NULL
    AND t.temp IS NOT NULL
    AND t.rh IS NOT NULL
  ORDER BY t.ts_cse
) TO STDOUT WITH CSV HEADER
" > training_data.csv
```

#### Option B: Create Synthetic Training Data

If you don't have ground truth, create synthetic data based on known good/bad conditions:

```python
import numpy as np
import pandas as pd

np.random.seed(42)
n_samples = 1000

# Generate synthetic sensor data
data = {
    'co2': np.random.uniform(400, 1500, n_samples),
    'noise': np.random.uniform(30, 80, n_samples),
    'lux': np.random.uniform(50, 800, n_samples),
    'temp': np.random.uniform(18, 28, n_samples),
    'rh': np.random.uniform(30, 70, n_samples),
    'occ': np.random.randint(0, 10, n_samples),
}

# Create synthetic mood scores based on heuristic rules
def synthetic_mood(row):
    score = 100 * (
        (1200 - row['co2']) / 800 * 0.25 +
        (80 - row['noise']) / 50 * 0.20 +
        (row['lux'] - 100) / 700 * 0.20 +
        1.0 * 0.15 +  # temp factor (simplified)
        1.0 * 0.10 +  # humidity factor (simplified)
        min(1.0, row['occ'] / 5.0) * 0.10
    )
    # Add some noise
    score += np.random.normal(0, 5)
    return max(0, min(100, score))

df = pd.DataFrame(data)
df['mood_score'] = df.apply(synthetic_mood, axis=1)
df.to_csv('training_data.csv', index=False)
print(f"Created {n_samples} synthetic training samples")
```

### Step 3: Train the Model

```bash
# Install dependencies locally
pip install numpy pandas scikit-learn joblib

# Run training script
python train_model.py
```

Expected output:
```
Mean Squared Error: 42.15
R² Score: 0.853
Model saved to mood_model.pkl
```

### Step 4: Deploy the Model

```bash
# Copy trained model to mood-service-ml directory
cp mood_model.pkl cloud/mood-service-ml/

# Rebuild and restart the service
cd cloud
docker-compose build mood-ml
docker-compose up -d mood-ml

# Verify model loaded
docker logs mood-ml | grep "Loaded ML mood model"
```

### Step 5: Monitor and Tune

Enable debug mode to compare ML vs heuristic predictions:

```yaml
# docker-compose.yml
environment:
  - MOOD_ML_DEBUG=1
```

Check logs:
```bash
docker logs -f mood-ml
```

You'll see debug output in the returned mood CIN:
```json
{
  "score": 67,
  "label": "neutral",
  "led_color": "#FFAA00",
  "_debug": {
    "ml_score": 62.4,
    "heuristic_score": 71.2,
    "pre_calibrated": 64.2,
    "softened": 73.8,
    "blend": 0.2
  }
}
```

Adjust calibration parameters based on observed behavior.

## Alternative ML Models

### Gradient Boosting (Higher Accuracy)

```python
from sklearn.ensemble import GradientBoostingRegressor

model = GradientBoostingRegressor(
    n_estimators=200,
    learning_rate=0.1,
    max_depth=5,
    random_state=42
)
```

### Neural Network (For Large Datasets)

```python
from sklearn.neural_network import MLPRegressor

model = MLPRegressor(
    hidden_layer_sizes=(64, 32),
    activation='relu',
    solver='adam',
    max_iter=500,
    random_state=42
)
```

### Linear Regression (Baseline)

```python
from sklearn.linear_model import LinearRegression

model = LinearRegression()
```

## Feature Engineering Ideas

Enhance predictions by adding derived features:

```python
# Example: add time-of-day features
data['hour'] = pd.to_datetime(data['timestamp']).dt.hour
data['is_workday'] = (data['hour'] >= 9) & (data['hour'] <= 17)

# Add interaction terms
data['co2_noise'] = data['co2'] * data['noise']
data['temp_rh'] = data['temp'] * data['rh']

# Add rolling averages (if time-series)
data['co2_rolling_avg'] = data['co2'].rolling(window=5).mean()

X = data[['co2', 'noise', 'lux', 'temp', 'rh', 'occ',
          'hour', 'is_workday', 'co2_noise', 'temp_rh']].values
```

**Note:** If you add features, update `app.py` to extract them from telemetry.

## API Endpoints

### POST /notify

Receives oneM2M notifications with sensor data.

**Request:**
```json
{
  "m2m:sgn": {
    "nev": {
      "rep": {
        "m2m:cin": {
          "con": {
            "co2": 650,
            "noise": 45,
            "lux": 300,
            "temp": 22.5,
            "rh": 42,
            "occ": 2,
            "room": "room-101",
            "desk": "desk-A"
          }
        }
      }
    }
  }
}
```

**Response:**
```json
{
  "result": "ok",
  "mood": {
    "score": 78,
    "label": "focus",
    "led_color": "#80FF00",
    "ts": 1705234567,
    "confidence": 0.87
  }
}
```

### GET /latest-mood

Retrieves the latest mood CIN from the CSE.

**Query Parameters:**
- `room` (optional): Room identifier

**Response:**
```json
{
  "latest": {
    "score": 78,
    "label": "focus",
    "led_color": "#80FF00",
    "ts": 1705234567
  }
}
```

## Database Schema

The service writes to `fact_mood_ml` table:

```sql
CREATE TABLE fact_mood_ml (
    id SERIAL PRIMARY KEY,
    parent_path VARCHAR(512),
    ci_rn VARCHAR(128),
    ts_cse TIMESTAMPTZ NOT NULL,
    score INT NOT NULL,
    label VARCHAR(32),
    confidence FLOAT,
    room_id INT,
    device VARCHAR(64),
    led_color VARCHAR(16),
    inserted_at TIMESTAMPTZ DEFAULT now(),
    UNIQUE(parent_path, ci_rn)
);
```

This allows comparing ML predictions (`fact_mood_ml`) vs heuristic predictions (`fact_mood`).

## Troubleshooting

**Model not loading:**
```bash
# Check if file exists
docker exec mood-ml ls -lh /app/mood_model.pkl

# Check logs
docker logs mood-ml | grep -i model
```

**Poor predictions:**
- Collect more training data
- Enable debug mode and compare ML vs heuristic
- Adjust `ML_BLEND_HEURISTIC` to favor heuristic (0.3-0.5)
- Retrain with better ground truth labels

**Extreme scores:**
- Increase `SOFTENING_FACTOR` (0.95)
- Adjust `SOFTENING_CENTER` to desired average
- Add `SCORE_BIAS` to lift all scores

**Missing sensor values:**
- Check cache TTL (`LATEST_TTL_SEC`)
- Verify default values in `app.py:_DEFAULTS`
- Ensure telemetry includes room/desk labels for proper caching

## Performance

- **Latency:** ~10-50ms per prediction (model + DB write)
- **Memory:** ~100-200MB (depends on model size)
- **CPU:** Minimal (<5% on multi-core systems)

For high-throughput scenarios, consider:
- Batch predictions
- Redis caching
- Model quantization
- Dedicated GPU inference (for neural networks)

## License

MIT

## Authors

**Team VibeTribe** - 2025 International oneM2M Hackathon
