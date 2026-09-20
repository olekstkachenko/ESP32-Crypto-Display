import pytest
import requests
import os

# To run tests, set the environment variable ESP_IP
# Example: $env:ESP_IP="192.168.1.100"; pytest test_web_api.py
ESP_IP = os.getenv("ESP_IP", "192.168.1.100")
BASE_URL = f"http://{ESP_IP}"

def test_home_page_reachable():
    """Test if the main settings page is accessible and returns Crypto Display branding."""
    try:
        response = requests.get(BASE_URL, timeout=5)
        assert response.status_code == 200
        assert "Crypto Display" in response.text
    except requests.exceptions.ConnectionError:
        pytest.skip(f"Device at {ESP_IP} is not reachable. Is it connected to Wi-Fi?")

def test_save_settings():
    """Test if the ESP32 accepts POST requests to save settings and configure widgets."""
    payload = {
        "notes": "Test Note from pytest",
        "nickname": "Crypto Test",
        "theme": "slate",
        "accent": "blue",
        "text_color": "standard",
        "unit": "metric",
        "region_format": "europe",
        "timezone": "europe_central",
        "auto_sleep": "0",
        "flash_done": "1",
        "lat": "50.45",
        "lng": "30.52",
        "loc_name": "Kyiv",
        "slot0": "btc",
        "slot1": "eth",
        "slot2": "timer",
        "slot3": "outdoor"
    }
    
    try:
        response = requests.post(f"{BASE_URL}/save", data=payload, timeout=5)
        assert response.status_code == 200
        # ESP32 WebServer typically returns a simple HTML confirmation or redirect
        assert "saved" in response.text.lower() or "href=\"/\"" in response.text.lower()
    except requests.exceptions.ConnectionError:
        pytest.skip(f"Device at {ESP_IP} is not reachable.")
