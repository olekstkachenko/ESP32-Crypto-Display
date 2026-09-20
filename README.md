# Crypto Display 📈⚡

**Crypto Display** is a sleek ESP32-based smart desk companion built around the **ESP32-2432S028R** (Cheap Yellow Display / CYD) 2.8" color touchscreen. It turns an affordable ESP32 display into a multi-functional real-time desktop dashboard.

---

## ✨ Features

- 🇺🇦 **Bilingual Interface (UA / EN)**: Full Ukrainian and English localization switchable on the fly with persistent memory.
- 📱 **Captive Hotspot / SoftAP Fallback**: If Wi-Fi fails to connect, device automatically creates `CryptoDisplay-Setup` hotspot (IP `192.168.4.1`) so you can configure Wi-Fi directly from your smartphone.
- ⚡ **Wireless OTA Updates**: Flash new firmware `.bin` files wirelessly over the air via `http://<device-ip>/update` without plugging in USB.
- 🪙 **Live Crypto Ticker**: Tracks real-time prices, tick trend indicators (`^` / `v`), & 24h percentage change for 5 customizable Binance trading pairs (e.g. BTC, ETH, SOL, XRP, DOGE, PEPE) with intelligent dynamic decimal precision.
- ⏱️ **Clock & Productivity**: NTP time synchronization, regional date formats (Europe 24h / US 12h), customizable focus timer with presets.
- ⛅ **Weather & Astronomy**: Live outdoor temperature, precipitation, wind speed & direction, UV index, geomagnetic KP index, and automatic next sunrise/sunset tracking.
- 🛢️ **Fuel & Commodities**: Real-time Brent Crude Oil price (Yahoo Finance) + Ukraine fuel market prices (A-95 & Diesel via Minfin).
- 🌐 **Built-in Web Portal**: Modern browser dashboard at `http://<device-ip>` to configure tokens, themes, accent colors, units, and widgets without recompiling.
- 📡 **Smart MQTT Client**: Non-blocking telemetry reporting status to Home Assistant or Mosquitto.
- 🚀 **Flicker-Free UI**: Smooth, card-based dynamic updates without full-screen blanking.
- 💻 **PlatformIO & Arduino IDE Support**: Ready-to-build with PlatformIO (zero library setup required) or Arduino IDE.

---

## 🛠️ Hardware Supported

- **Board**: ESP32 CYD (ESP32-2432S028R) 2.8" TFT with Touchscreen (ST7789/ILI9341 driver, XPT2046 touch).
- **Interface**: VSPI / HSPI SPI bus.
- **Power**: Standard USB Type-C or Micro-USB cable.

---

## 🚀 Quick Start

### 1. Configure WiFi & Secrets
Copy or edit [Secrets.h](Secrets.h):
```cpp
const char* WIFI_SSID = "YOUR_WIFI_SSID";
const char* WIFI_PASS = "YOUR_WIFI_PASSWORD";
```

### 2. Build & Flash
- **PlatformIO (Recommended)**: Open folder in VS Code with PlatformIO extension and click **Upload** (or run `pio run --target upload`).
- **Arduino IDE**: See [SETUP_GUIDE.md](SETUP_GUIDE.md) for library details and `User_Setup.h` configuration.

### 3. Personalize
After booting, open the device IP in your browser to select your favorite crypto tokens, change colors, and manage widgets.
