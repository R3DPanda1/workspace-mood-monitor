#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
SwitchBot CO2 Sensor -> OneM2M MoodMonitor
Minimal implementation with CO2 ppm to mg/m3 conversion
"""

import os
import sys
import time
import uuid
import requests
import asyncio
from typing import Dict, Optional
from datetime import datetime

# Environment Configuration
SWITCHBOT_MAC = os.getenv("SWITCHBOT_MAC", "B0:E9:FE:DE:49:2B").upper()
BLE_ADAPTER = os.getenv("BLE_ADAPTER", "hci0")
# Reuse MN-CSE configuration variables
CSE_HOST = os.getenv("MNCSE_HOST", "192.168.113.238")
CSE_PORT = os.getenv("MNCSE_PORT", "8081")
CSE_NAME = os.getenv("MNCSE_NAME", "room-mn-cse")
ORIGINATOR = os.getenv("ORIGINATOR", "CMoodMonitor")
POLL_INTERVAL = int(os.getenv("POLL_INTERVAL", "60"))

# OneM2M Constants
BASE_URL = f"http://{CSE_HOST}:{CSE_PORT}"
CSE_PATH = f"/{CSE_NAME}"
ROOM_CONTAINER = os.getenv("ROOM_CONTAINER", "Room01")
# AE paths
AE_PATH = f"/{CSE_NAME}/moodMonitorAE"
# Container inside AE
CONTAINER_PATH = f"{AE_PATH}/{ROOM_CONTAINER}"
# Device paths (flexContainers inside container)
DEVICE_PATH = f"{CONTAINER_PATH}/deviceAirQualityMonitor"
SENSOR_PATH = f"{DEVICE_PATH}/airQualitySensor"
BATTERY_PATH = f"{DEVICE_PATH}/battery"

# SwitchBot Constants
SWITCHBOT_MFG_ID = 0x0969

try:
    from bleak import BleakScanner
except ImportError:
    print("ERROR: Install bleak: pip install bleak", file=sys.stderr)
    sys.exit(1)


def log(msg: str) -> None:
    """Simple timestamped logging"""
    print(f"[{datetime.now().strftime('%Y-%m-%d %H:%M:%S')}] {msg}", flush=True)


def ppm_to_mg_m3(ppm: float, temp_celsius: float = 25.0) -> float:
    """
    Convert CO2 from ppm to mg/m^3
    Formula: mg/m^3 = (ppm x molecular_weight) / molar_volume
    CO2 molecular weight: 44.01 g/mol
    Molar volume at temp: 24.45 L/mol at 25C (adjustable)
    """
    molecular_weight_co2 = 44.01  # g/mol
    molar_volume = 24.45 * (273.15 + temp_celsius) / 298.15  # Adjusted for temperature
    return round((ppm * molecular_weight_co2) / molar_volume, 2)


async def scan_switchbot(mac: str, adapter: Optional[str] = None, timeout: int = 20) -> Dict[str, float]:
    """Scan for SwitchBot CO2 sensor and decode telemetry"""
    scanner = BleakScanner(adapter=adapter) if adapter else BleakScanner()
    devices = await scanner.discover(timeout=timeout)

    for dev in devices:
        if dev.address.upper() != mac:
            continue

        log(f"DEBUG: Found device {dev.address}")

        # Extract manufacturer data
        mfg_data = {}
        try:
            if hasattr(dev, "metadata") and isinstance(dev.metadata, dict):
                mfg_data = dev.metadata.get("manufacturer_data", {}) or {}
            elif hasattr(dev, "details") and isinstance(dev.details, dict):
                props = dev.details.get('props', {}) or {}
                mfg_data = props.get('ManufacturerData', {}) or {}
        except Exception as e:
            log(f"DEBUG: Error extracting manufacturer data: {e}")
            continue

        # Decode SwitchBot data
        for mfg_id, data in mfg_data.items():
            mfg_id = int(mfg_id) if not isinstance(mfg_id, int) else mfg_id

            # Log all manufacturer data for debugging
            if mfg_id == SWITCHBOT_MFG_ID and isinstance(data, (bytes, bytearray)):
                hex_data = ' '.join(f'{b:02X}' for b in data)
                log(f"DEBUG: MFG_ID={mfg_id:04X}, Length={len(data)}, Data=[{hex_data}]")

                # Check minimum length
                if len(data) < 15:
                    log(f"DEBUG: Data too short (need 15 bytes, got {len(data)}), skipping")
                    continue

                # Log individual byte positions for CO2
                log(f"DEBUG: Byte[0]={data[0]:02X} (device type?), Byte[1]={data[1]:02X}")
                log(f"DEBUG: Byte[13]={data[13]:02X}, Byte[14]={data[14]:02X} -> CO2={(data[13] << 8) | data[14]}")

                # Temperature: byte[8-9]
                frac = (data[8] & 0x0F) * 0.1
                integer = (data[9] & 0x7F)
                temp = integer + frac
                if not (data[9] & 0x80):
                    temp = -temp

                # Humidity: byte[10]
                humidity = float(data[10] & 0x7F)

                # CO2: byte[13-14]
                co2_ppm = float((data[13] << 8) | data[14])

                log(f"DEBUG: Decoded -> Temp={temp}C, Humidity={humidity}%, CO2={co2_ppm}ppm")

                # Battery: from service data if available
                battery = None
                svc_data = {}
                try:
                    if hasattr(dev, "metadata"):
                        svc_data = dev.metadata.get("service_data", {}) or {}
                    elif hasattr(dev, "details"):
                        svc_data = dev.details.get('props', {}).get('ServiceData', {}) or {}

                    for svc_uuid, sdata in svc_data.items():
                        if isinstance(sdata, (bytes, bytearray)) and len(sdata) >= 3:
                            hex_svc = ' '.join(f'{b:02X}' for b in sdata)
                            log(f"DEBUG: Service UUID={svc_uuid}, Data=[{hex_svc}]")
                            battery = float(sdata[2] & 0x7F)
                            break
                except Exception as e:
                    log(f"DEBUG: Error extracting service data: {e}")

                result = {
                    "temperature": round(temp, 1),
                    "humidity": humidity,
                    "co2_ppm": co2_ppm,
                }
                if battery is not None:
                    result["battery"] = battery

                return result

    log(f"DEBUG: No valid SwitchBot data found for MAC {mac}")
    return {}


def read_sensor() -> Dict[str, float]:
    """Read SwitchBot sensor data"""
    try:
        return asyncio.run(scan_switchbot(SWITCHBOT_MAC, adapter=BLE_ADAPTER, timeout=20))
    except Exception as e:
        raise RuntimeError(f"BLE scan failed: {e}")


def onem2m_request(method: str, path: str, payload: Optional[dict] = None, ty: Optional[int] = None) -> requests.Response:
    """Generic OneM2M HTTP request"""
    url = f"{BASE_URL}{path}"
    
    content_type = "application/json"
    if ty is not None:
        content_type = f"application/json;ty={ty}"
    
    headers = {
        "X-M2M-Origin": ORIGINATOR,
        "X-M2M-RI": str(uuid.uuid4())[:10],
        "X-M2M-RVI": "3",
        "Accept": "application/json",
        "Content-Type": content_type,
    }

    try:
        if method == "GET":
            return requests.get(url, headers=headers, timeout=10)
        elif method == "POST":
            return requests.post(url, headers=headers, json=payload, timeout=10)
        elif method == "PUT":
            return requests.put(url, headers=headers, json=payload, timeout=10)
        else:
            raise ValueError(f"Unsupported method: {method}")
    except Exception as e:
        log(f"Request error ({method} {path}): {e}")
        raise


def wait_for_cse(max_attempts: int = 30) -> bool:
    """Wait for CSE to become available"""
    log(f"Waiting for CSE at {CSE_HOST}:{CSE_PORT}...")
    for _ in range(max_attempts):
        try:
            r = onem2m_request("GET", CSE_PATH)
            if r.status_code in (200, 403):
                log("CSE is ready")
                return True
        except Exception:
            pass
        time.sleep(2)
    log("ERROR: CSE not available")
    return False


def announce_sensor() -> bool:
    """Announce the airQualitySensor to the cloud IN-CSE"""
    log("Announcing airQualitySensor to cloud IN-CSE")
    payload = {
        "cod:aiQSr": {
            "at": ["/id-cloud-in-cse"],
            "aa": ["tempe", "humiy", "co2"]
        }
    }

    try:
        r = onem2m_request("PUT", SENSOR_PATH, payload)
        if r.status_code in (200, 204):
            log("Sensor announced successfully")
            return True
        else:
            log(f"Sensor announcement failed: {r.status_code} - {r.text}")
            return False
    except Exception as e:
        log(f"Sensor announcement error: {e}")
        return False


def setup_hierarchy() -> bool:
    """
    Setup OneM2M hierarchy (idempotent):
    1. Create ACP (acpMoodMonitor)
    2. Create AE (moodMonitorAE)
    3. Create Container (Room01) inside AE
    4. Create deviceAirQualityMonitor flexContainer inside Container
    5. Create airQualitySensor moduleClass inside flexContainer
    6. Create battery moduleClass inside flexContainer
    7. Announce airQualitySensor to cloud IN-CSE
    """

    # 1. Create ACP
    log("Creating ACP: acpMoodMonitor")
    payload = {
        "m2m:acp": {
            "pv": {
                "acr": [
                    {
                        "acop": 63,
                        "acor": ["/id-cloud-in-cse/CAdmin"]
                    },
                    {
                        "acop": 63,
                        "acor": ["CMoodMonitor"]
                    }
                ]
            },
            "pvs": {
                "acr": [
                    {
                        "acop": 63,
                        "acor": ["CMoodMonitor"]
                    },
                    {
                        "acop": 63,
                        "acor": ["/id-cloud-in-cse/CAdmin"]
                    }
                ]
            },
            "rn": "acpMoodMonitor"
        }
    }
    try:
        r = onem2m_request("POST", CSE_PATH, payload, ty=1)
        if r.status_code not in (201, 409):
            log(f"ACP creation failed: {r.status_code} - {r.text}")
    except Exception as e:
        log(f"ACP creation error (may already exist): {e}")

    # 2. Create AE (at CSE root)
    log("Creating AE: moodMonitorAE")
    payload = {
        "m2m:ae": {
            "acpi": [f"{CSE_NAME}/acpMoodMonitor"],
            "api": "Nmoodmonitor.fhtw.at",
            "rn": "moodMonitorAE",
            "srv": ["3"],
            "rr": True
        }
    }
    try:
        r = onem2m_request("POST", CSE_PATH, payload, ty=2)
        if r.status_code not in (201, 409):
            log(f"AE creation failed: {r.status_code} - {r.text}")
    except Exception as e:
        log(f"AE creation error (may already exist): {e}")

    # 3. Create Container inside AE
    log(f"Creating Container: {ROOM_CONTAINER} inside AE")
    payload = {
        "m2m:cnt": {
            "acpi": [f"{CSE_NAME}/acpMoodMonitor"],
            "mbs": 10000,
            "mni": 10,
            "rn": ROOM_CONTAINER
        }
    }
    try:
        r = onem2m_request("POST", AE_PATH, payload, ty=3)
        if r.status_code not in (201, 409):
            log(f"Container creation failed: {r.status_code} - {r.text}")
    except Exception as e:
        log(f"Container creation error (may already exist): {e}")

    # 4. Create deviceAirQualityMonitor flexContainer inside Container
    log("Creating flexContainer: deviceAirQualityMonitor inside Container")
    payload = {
        "cod:dAQMr": {
            "acpi": [f"{CSE_NAME}/acpMoodMonitor"],
            "cnd": "org.onem2m.common.device.deviceAirQualityMonitor",
            "rn": "deviceAirQualityMonitor"
        }
    }
    try:
        r = onem2m_request("POST", CONTAINER_PATH, payload, ty=28)
        if r.status_code not in (201, 409):
            log(f"Device flexContainer creation failed: {r.status_code} - {r.text}")
    except Exception as e:
        log(f"Device flexContainer error (may already exist): {e}")

    # 5. Create airQualitySensor moduleClass inside flexContainer
    log("Creating moduleClass: airQualitySensor inside flexContainer")
    payload = {
        "cod:aiQSr": {
            "acpi": [f"{CSE_NAME}/acpMoodMonitor"],
            "cnd": "org.onem2m.common.moduleclass.airQualitySensor",
            "rn": "airQualitySensor",
            "lbl": [
                "room:Room01",
                "desk:Desk01",
                "sensor:air"
            ]
        }
    }
    try:
        r = onem2m_request("POST", DEVICE_PATH, payload, ty=28)
        if r.status_code not in (201, 409):
            log(f"Sensor moduleClass creation failed: {r.status_code} - {r.text}")
    except Exception as e:
        log(f"Sensor moduleClass error (may already exist): {e}")

    # 6. Create battery moduleClass inside flexContainer
    log("Creating moduleClass: battery inside flexContainer")
    payload = {
        "cod:bat": {
            "acpi": [f"{CSE_NAME}/acpMoodMonitor"],
            "cnd": "org.onem2m.common.moduleclass.battery",
            "rn": "battery",
            "lvl": 100
        }
    }
    try:
        r = onem2m_request("POST", DEVICE_PATH, payload, ty=28)
        if r.status_code not in (201, 409):
            log(f"Battery moduleClass creation failed: {r.status_code} - {r.text}")
    except Exception as e:
        log(f"Battery moduleClass error (may already exist): {e}")

    # 7. Announce airQualitySensor to cloud IN-CSE
    # Wait for MN-CSE to IN-CSE connection to be established
    log("Waiting 10 seconds for MN-CSE to IN-CSE connection...")
    time.sleep(10)
    announce_sensor()

    log("Hierarchy setup complete")
    return True


def update_sensor(temp: float, humidity: float, co2_ppm: float) -> bool:
    """Update airQualitySensor with temperature, humidity, and CO2 (converted to mg/m^3)"""
    co2_mg_m3 = ppm_to_mg_m3(co2_ppm, temp)

    payload = {
        "cod:aiQSr": {
            "tempe": temp,
            "humiy": humidity,
            "co2": co2_mg_m3
        }
    }

    try:
        r = onem2m_request("PUT", SENSOR_PATH, payload)
        if r.status_code in (200, 204):
            log(f"Sensor updated: Temp={temp}C, Humidity={humidity}%, CO2={co2_ppm}ppm ({co2_mg_m3}mg/m^3)")
            return True
        else:
            log(f"Sensor update failed: {r.status_code} - {r.text}")
            return False
    except Exception as e:
        log(f"Sensor update error: {e}")
        return False


def update_battery(level: int) -> bool:
    """Update battery level"""
    payload = {
        "cod:bat": {
            "lvl": level
        }
    }

    try:
        r = onem2m_request("PUT", BATTERY_PATH, payload)
        if r.status_code in (200, 204):
            log(f"Battery updated: {level}%")
            return True
        else:
            log(f"Battery update failed: {r.status_code} - {r.text}")
            return False
    except Exception as e:
        log(f"Battery update error: {e}")
        return False


def main() -> None:
    """Main execution loop"""
    log("=== SwitchBot CO2 -> OneM2M MoodMonitor ===")
    log(f"Sensor MAC: {SWITCHBOT_MAC}")
    log(f"CSE: {CSE_HOST}:{CSE_PORT}")
    log(f"Poll interval: {POLL_INTERVAL}s")
    log("Note: CO2 values converted from ppm to mg/m^3")

    # Wait for CSE
    if not wait_for_cse():
        sys.exit(1)

    # Setup hierarchy
    log("\n--- Setting up OneM2M hierarchy ---")
    if not setup_hierarchy():
        log("Failed to setup hierarchy")
        sys.exit(1)

    # Main sensor loop
    log("\n--- Starting sensor loop ---")
    consecutive_errors = 0
    last_data = {}

    while True:
        try:
            # Read sensor
            data = read_sensor()
            if not data:
                raise RuntimeError("No data from sensor")

            # Validate ranges
            temp = data.get("temperature")
            humidity = data.get("humidity")
            co2_ppm = data.get("co2_ppm")
            battery = data.get("battery")

            if temp is None or humidity is None or co2_ppm is None:
                raise RuntimeError("Missing required sensor data")

            if not (-40 <= temp <= 85):
                log(f"Temperature out of range: {temp}C")
                raise ValueError("Temperature out of range")

            if not (0 <= humidity <= 100):
                log(f"Humidity out of range: {humidity}%")
                raise ValueError("Humidity out of range")

            if not (0 <= co2_ppm <= 50000):
                log(f"CO2 out of range: {co2_ppm}ppm")
                raise ValueError("CO2 out of range")

            # Update sensor data (always update for simplicity)
            sensor_key = (temp, humidity, co2_ppm)
            if last_data.get("sensor") != sensor_key:
                if update_sensor(temp, humidity, co2_ppm):
                    last_data["sensor"] = sensor_key
                    consecutive_errors = 0

            # Update battery if available and changed
            if battery is not None and 0 <= battery <= 100:
                if last_data.get("battery") != battery:
                    if update_battery(int(battery)):
                        last_data["battery"] = battery

        except Exception as e:
            consecutive_errors += 1
            log(f"Error: {e}")

        # Exit if too many consecutive errors
        if consecutive_errors >= 10:
            log("Too many consecutive errors, exiting")
            sys.exit(1)

        time.sleep(POLL_INTERVAL)


if __name__ == "__main__":
    main()