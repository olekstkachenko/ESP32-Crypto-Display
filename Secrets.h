#ifndef SECRETS_H
#define SECRETS_H

// =========================================================
// SECRETS & API KEYS
// =========================================================

const char* WIFI_SSID = "YOUR_WIFI_SSID";       // Replace with your WiFi network name
const char* WIFI_PASS = "YOUR_WIFI_PASSWORD";   // Replace with your WiFi password

// Binance API Key
// Replace with your actual key if needed, or leave empty for public endpoints
const char* BINANCE_API_KEY = "";

// MQTT Configuration
const char* MQTT_SERVER = "192.168.1.100"; // Replace with your Mosquitto IP
const int MQTT_PORT = 1883;
const char* MQTT_USER = "";                // Set if your broker requires auth
const char* MQTT_PASS = "";

#endif // SECRETS_H
