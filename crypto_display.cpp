// Crypto Display V.8
// Nav: Home / Weather / Notes / Status
// Full version
// - KP dots replaced with Low / Medium / High / Extreme text
// - KP level text uses same small font as wind direction and stays inside the box
// - Wind + direction added to Weather page
// - Wind direction uses Accent color
// - Weather sun event field automatically shows Sunrise or Sunset, whichever is next
// - Uptime added to Status page

#include <WiFi.h>
#include <HTTPClient.h>
#include <WiFiClientSecure.h>
#include <TFT_eSPI.h>
#include <time.h>
#include <ArduinoJson.h>
#include <WebServer.h>
#include <Update.h>
#include <Preferences.h>
#include <SPI.h>
#include <XPT2046_Touchscreen.h>
#include <math.h>
#include <PubSubClient.h>
#include "Secrets.h"

// =========================================================
// WIFI & NETWORK CONFIG
// =========================================================
String wifiSSID = WIFI_SSID;
String wifiPass = WIFI_PASS;
bool apModeActive = false;

// MQTT runtime config
String mqttServer = MQTT_SERVER;
int mqttPort = MQTT_PORT;
String mqttUser = MQTT_USER;
String mqttPass = MQTT_PASS;
bool mqttEnabled = false;

// =========================================================
// DISPLAY / TOUCH
// =========================================================
TFT_eSPI tft;

const int ROT = 2;
const bool INV = false;

#define TOUCH_CS  33
#define TOUCH_IRQ 36

static const int T_SCK  = 25;
static const int T_MISO = 39;
static const int T_MOSI = 32;

SPIClass touchSPI(VSPI);
XPT2046_Touchscreen ts(TOUCH_CS);

static const int TOUCH_X_MIN = 562;
static const int TOUCH_X_MAX = 3604;
static const int TOUCH_Y_MIN = 544;
static const int TOUCH_Y_MAX = 3720;

static const bool TOUCH_SWAP_XY = false;
static const bool TOUCH_FLIP_X  = false;
static const bool TOUCH_FLIP_Y  = false;

// =========================================================
// WEB / STORAGE
// =========================================================
WebServer server(80);
Preferences prefs;

// =========================================================
// SPRITES
// =========================================================
TFT_eSprite sprClock = TFT_eSprite(&tft);
TFT_eSprite sprSmall = TFT_eSprite(&tft);

// =========================================================
// LOCATION
// =========================================================
float LAT = 52.5200f;
float LNG = 13.4050f;
String locationName = "Berlin";

// =========================================================
// THEME
// =========================================================
uint16_t COL_BG        = 0x08A3;
uint16_t COL_PANEL     = 0x1106;
uint16_t COL_PANEL_ALT = 0x18C7;
uint16_t COL_STROKE    = 0x31EC;
uint16_t COL_TEXT      = 0xEF7D;
uint16_t COL_DIM       = 0x94B2;
uint16_t COL_MUTED     = 0x94B2;
uint16_t COL_ACCENT    = 0x5EFA;

const uint16_t COL_GREEN  = TFT_GREEN;
const uint16_t COL_YELLOW = 0xFFE0;
const uint16_t COL_RED    = TFT_RED;
const uint16_t COL_BLUE   = 0x041F;

String textColorKey = "standard";
String unitKey = "metric"; // metric = C/mm, imperial = F/in
String regionFormatKey = "europe"; // europe = 24h + dd.mm.yyyy, us = 12h + mm/dd/yyyy
String timezoneKey = "europe_central";
String currentLang = "ua"; // "ua" or "en"

// =========================================================
// LAYOUT
// =========================================================
const int SCREEN_W = 240;
const int SCREEN_H = 320;
const int TOPBAR_H = 34;
const int NAV_H    = 44;

const int HOME_GRID_Y1 = 120;
const int HOME_GRID_Y2 = 198;
const int HOME_WIDGET_H = 70;

const int HOME_TIMER_X = 124;
const int HOME_TIMER_Y = HOME_GRID_Y1;
const int HOME_TIMER_W = 108;
const int HOME_TIMER_H = HOME_WIDGET_H;
const int TIMER_MENU_X = 20;
const int TIMER_MENU_Y = 68;
const int TIMER_MENU_W = 200;
const int TIMER_MENU_H = 194;
const int TIMER_DONE_X = 26;
const int TIMER_DONE_Y = 92;
const int TIMER_DONE_W = 188;
const int TIMER_DONE_H = 108;
const int PAGE_ROW1_Y = 42;
const int PAGE_ROW2_Y = 120;
const int PAGE_ROW3_Y = 198;
const int PAGE_WIDGET_H = HOME_WIDGET_H;

// =========================================================
// WIDGETS
// =========================================================
String buddyNickname = "";

enum HomeWidgetType {
  HOME_WIDGET_WEEK = 0,
  HOME_WIDGET_TIMER,
  HOME_WIDGET_RAIN,
  HOME_WIDGET_OUTDOOR,
  HOME_WIDGET_KP,
  HOME_WIDGET_UV,
  HOME_WIDGET_WIND,
  HOME_WIDGET_SUN,
  HOME_WIDGET_BTC,
  HOME_WIDGET_ETH
};

const int HOME_SLOT_COUNT = 4;
HomeWidgetType homeWidgetSlots[HOME_SLOT_COUNT] = {
  HOME_WIDGET_WEEK,
  HOME_WIDGET_TIMER,
  HOME_WIDGET_RAIN,
  HOME_WIDGET_OUTDOOR
};

String cacheHomeSlots[HOME_SLOT_COUNT];

// =========================================================
// STATE
// =========================================================
enum Page {
  PAGE_HOME = 0,
  PAGE_WEATHER = 1,
  PAGE_CRYPTO = 2,
  PAGE_FUEL = 3
};
const int TOTAL_PAGES = 4;

Page currentPage = PAGE_HOME;
Page lastDrawnPage = (Page)-1;

unsigned long lastClockTick = 0;
unsigned long lastDataTick  = 0;

const unsigned long CLOCK_TICK_MS = 1000;
const unsigned long DATA_TICK_MS  = 30UL * 1000UL;

bool pageDirty = true;
bool dataDirty = true;

// cache
String cacheClock = "";
String cacheTemp = "";
String cacheRain = "";
String cacheWeek = "";
String cacheHomeEmpty1 = "";
String cacheHomeEmpty2 = "";
String cacheFocusTimer = "";
String cacheTimerMenu = "";
String cacheTimerDone = "";
String cacheTimerDoneCountdown = "";
String cacheTimerDoneFlash = "";

String lastWifiText = "";
String lastSignalText = "";
String lastIpText = "";
String lastUptimeText = "";
String lastTempText = "";
String lastRainText = "";
String lastUvText = "";
String lastUvLevelText = "";
String lastKpText = "";
String lastKpLevelText = "";
String lastWindText = "";
String lastWindDirText = "";
String lastNextSunLabel = "";
String lastNextSunTime = "";

String lastBtcPrice = "--";
String lastEthPrice = "--";

const char* homeWidgetKey(HomeWidgetType type) {
  switch (type) {
    case HOME_WIDGET_WEEK:    return "week";
    case HOME_WIDGET_TIMER:   return "timer";
    case HOME_WIDGET_RAIN:    return "rain";
    case HOME_WIDGET_OUTDOOR: return "outdoor";
    case HOME_WIDGET_KP:      return "kp";
    case HOME_WIDGET_UV:      return "uv";
    case HOME_WIDGET_WIND:    return "wind";
    case HOME_WIDGET_SUN:     return "sun";
    case HOME_WIDGET_BTC:     return "btc";
    case HOME_WIDGET_ETH:     return "eth";
    default:                  return "week";
  }
}

const char* homeWidgetLabel(HomeWidgetType type, bool isUa = false) {
  if (isUa) {
    switch (type) {
      case HOME_WIDGET_WEEK:    return "Номер тижня";
      case HOME_WIDGET_TIMER:   return "Таймер";
      case HOME_WIDGET_RAIN:    return "Опади";
      case HOME_WIDGET_OUTDOOR: return "Температура";
      case HOME_WIDGET_KP:      return "KP індекс";
      case HOME_WIDGET_UV:      return "UV індекс";
      case HOME_WIDGET_WIND:    return "Вітер";
      case HOME_WIDGET_SUN:     return "Схід / Захід сонця";
      case HOME_WIDGET_BTC:     return "Ціна Bitcoin";
      case HOME_WIDGET_ETH:     return "Ціна Ethereum";
      default:                  return "Тиждень";
    }
  }
  switch (type) {
    case HOME_WIDGET_WEEK:    return "Week";
    case HOME_WIDGET_TIMER:   return "Timer";
    case HOME_WIDGET_RAIN:    return "Rain";
    case HOME_WIDGET_OUTDOOR: return "Outdoor";
    case HOME_WIDGET_KP:      return "KP index";
    case HOME_WIDGET_UV:      return "UV index";
    case HOME_WIDGET_WIND:    return "Wind";
    case HOME_WIDGET_SUN:     return "Sun event";
    case HOME_WIDGET_BTC:     return "Bitcoin";
    case HOME_WIDGET_ETH:     return "Ethereum";
    default:                  return "Week";
  }
}

HomeWidgetType homeWidgetFromKey(const String& key) {
  if (key == "week") return HOME_WIDGET_WEEK;
  if (key == "timer") return HOME_WIDGET_TIMER;
  if (key == "rain") return HOME_WIDGET_RAIN;
  if (key == "outdoor") return HOME_WIDGET_OUTDOOR;
  if (key == "kp") return HOME_WIDGET_KP;
  if (key == "uv") return HOME_WIDGET_UV;
  if (key == "wind") return HOME_WIDGET_WIND;
  if (key == "sun") return HOME_WIDGET_SUN;
  if (key == "btc") return HOME_WIDGET_BTC;
  if (key == "eth") return HOME_WIDGET_ETH;
  return HOME_WIDGET_WEEK;
}

const char* homeSlotLabel(int slot, bool isUa = false) {
  if (isUa) {
    switch (slot) {
      case 0: return "Вгорі ліворуч";
      case 1: return "Вгорі праворуч";
      case 2: return "Внизу ліворуч";
      case 3: return "Внизу праворуч";
      default: return "Слот";
    }
  }
  switch (slot) {
    case 0: return "Top left";
    case 1: return "Top right";
    case 2: return "Bottom left";
    case 3: return "Bottom right";
    default: return "Slot";
  }
}

const char* timezonePosixByKey(const String& key) {
  if (key == "utc") return "UTC0";
  if (key == "atlantic_azores") return "AZOT1AZOST,M3.5.0/0,M10.5.0/1";
  if (key == "europe_west") return "WET0WEST,M3.5.0/1,M10.5.0";
  if (key == "uk") return "GMT0BST,M3.5.0/1,M10.5.0";
  if (key == "europe_central") return "CET-1CEST,M3.5.0/2,M10.5.0/3";
  if (key == "europe_east") return "EET-2EEST,M3.5.0/3,M10.5.0/4";
  if (key == "africa_south") return "SAST-2";
  if (key == "middle_east_gulf") return "GST-4";
  if (key == "india") return "IST-5:30";
  if (key == "thailand") return "ICT-7";
  if (key == "china") return "CST-8";
  if (key == "us_eastern") return "EST5EDT,M3.2.0/2,M11.1.0/2";
  if (key == "us_central") return "CST6CDT,M3.2.0/2,M11.1.0/2";
  if (key == "us_mountain") return "MST7MDT,M3.2.0/2,M11.1.0/2";
  if (key == "us_arizona") return "MST7";
  if (key == "us_pacific") return "PST8PDT,M3.2.0/2,M11.1.0/2";
  if (key == "alaska") return "AKST9AKDT,M3.2.0/2,M11.1.0/2";
  if (key == "hawaii") return "HST10";
  if (key == "canada_atlantic") return "AST4ADT,M3.2.0/2,M11.1.0/2";
  if (key == "brazil_east") return "BRT3";
  if (key == "argentina") return "ART3";
  if (key == "asia_tokyo") return "JST-9";
  if (key == "korea") return "KST-9";
  if (key == "australia_perth") return "AWST-8";
  if (key == "australia_darwin") return "ACST-9:30";
  if (key == "australia_sydney") return "AEST-10AEDT,M10.1.0,M4.1.0/3";
  if (key == "new_zealand") return "NZST-12NZDT,M9.5.0/2,M4.1.0/3";
  return "CET-1CEST,M3.5.0/2,M10.5.0/3";
}

const char* timezoneLabelByKey(const String& key) {
  if (key == "utc") return "UTC";
  if (key == "atlantic_azores") return "Azores";
  if (key == "europe_west") return "Western Europe";
  if (key == "uk") return "United Kingdom";
  if (key == "europe_central") return "Central Europe";
  if (key == "europe_east") return "Eastern Europe";
  if (key == "africa_south") return "South Africa";
  if (key == "middle_east_gulf") return "Gulf / UAE";
  if (key == "india") return "India";
  if (key == "thailand") return "Thailand / ICT";
  if (key == "china") return "China";
  if (key == "us_eastern") return "US Eastern";
  if (key == "us_central") return "US Central";
  if (key == "us_mountain") return "US Mountain";
  if (key == "us_arizona") return "US Arizona";
  if (key == "us_pacific") return "US Pacific";
  if (key == "alaska") return "Alaska";
  if (key == "hawaii") return "Hawaii";
  if (key == "canada_atlantic") return "Canada Atlantic";
  if (key == "brazil_east") return "Brazil East";
  if (key == "argentina") return "Argentina";
  if (key == "asia_tokyo") return "Japan";
  if (key == "korea") return "South Korea";
  if (key == "australia_perth") return "Australia West";
  if (key == "australia_darwin") return "Australia Central";
  if (key == "australia_sydney") return "Australia East";
  if (key == "new_zealand") return "New Zealand";
  return "Central Europe";
}

String sanitizeTimezoneKey(const String& key) {
  const char* supported[] = {
    "utc", "atlantic_azores", "europe_west", "uk", "europe_central", "europe_east",
    "africa_south", "middle_east_gulf", "india", "thailand", "china",
    "us_eastern", "us_central", "us_mountain", "us_arizona", "us_pacific",
    "alaska", "hawaii", "canada_atlantic", "brazil_east", "argentina",
    "asia_tokyo", "korea", "australia_perth", "australia_darwin",
    "australia_sydney", "new_zealand"
  };
  for (const char* supportedKey : supported) {
    if (key == supportedKey) return key;
  }
  return "europe_central";
}

void applyDeviceTimezoneByKey(const String& key) {
  timezoneKey = sanitizeTimezoneKey(key);
  setenv("TZ", timezonePosixByKey(timezoneKey), 1);
  tzset();
}

void appendTimezoneOptions(String& page, const String& selectedKey) {
  struct TimezoneGroup {
    const char* label;
    const char* keys[8];
    int count;
  };

  const TimezoneGroup groups[] = {
    {"Global", {"utc"}, 1},
    {"Europe", {"atlantic_azores", "europe_west", "uk", "europe_central", "europe_east"}, 5},
    {"Africa & Middle East", {"africa_south", "middle_east_gulf"}, 2},
    {"Asia", {"india", "thailand", "china", "asia_tokyo", "korea"}, 5},
    {"North America", {"us_eastern", "us_central", "us_mountain", "us_arizona", "us_pacific", "alaska", "hawaii", "canada_atlantic"}, 8},
    {"South America", {"brazil_east", "argentina"}, 2},
    {"Australia & Oceania", {"australia_perth", "australia_darwin", "australia_sydney", "new_zealand"}, 4}
  };

  for (const TimezoneGroup& group : groups) {
    page += "<optgroup label='";
    page += group.label;
    page += "'>";
    for (int i = 0; i < group.count; i++) {
      const char* key = group.keys[i];
      page += "<option value='";
      page += key;
      page += "'";
      if (selectedKey == key) page += " selected";
      page += ">";
      page += timezoneLabelByKey(key);
      page += "</option>";
    }
    page += "</optgroup>";
  }
}

void getHomeSlotRect(int slot, int& x, int& y, int& w, int& h) {
  const int xs[HOME_SLOT_COUNT] = {8, 124, 8, 124};
  const int ys[HOME_SLOT_COUNT] = {HOME_GRID_Y1, HOME_GRID_Y1, HOME_GRID_Y2, HOME_GRID_Y2};
  x = xs[slot];
  y = ys[slot];
  w = 108;
  h = HOME_WIDGET_H;
}

void appendHomeWidgetOptions(String& page, const String& selectedKey, bool isUa = false) {
  const HomeWidgetType types[] = {
    HOME_WIDGET_WEEK,
    HOME_WIDGET_TIMER,
    HOME_WIDGET_RAIN,
    HOME_WIDGET_OUTDOOR,
    HOME_WIDGET_KP,
    HOME_WIDGET_UV,
    HOME_WIDGET_WIND,
    HOME_WIDGET_SUN,
    HOME_WIDGET_BTC,
    HOME_WIDGET_ETH
  };

  for (HomeWidgetType type : types) {
    const char* key = homeWidgetKey(type);
    page += "<option value='";
    page += key;
    page += "'";
    if (selectedKey == key) page += " selected";
    page += ">";
    page += homeWidgetLabel(type, isUa);
    page += "</option>";
  }
}

void clearHomeSlotCaches() {
  for (int i = 0; i < HOME_SLOT_COUNT; i++) {
    cacheHomeSlots[i] = "";
  }
}

// Focus timer
bool focusMenuOpen = false;
bool focusTimerRunning = false;
bool focusTimerFinished = false;
unsigned long focusEndMs = 0;
unsigned long focusDurationSec = 0;
unsigned long focusRemainingSec = 0;
bool timerDoneDialogOpen = false;
unsigned long timerDoneDialogStartedMs = 0;
const unsigned long TIMER_DONE_DIALOG_MS = 60UL * 1000UL;
bool flashModeEnabled = false;
int timerPresetMin[6] = {1, 5, 10, 15, 25, 30};
bool wifiEnabled = true;
bool wifiConnectInProgress = false;
unsigned long wifiConnectStartedMs = 0;
const unsigned long WIFI_CONNECT_TIMEOUT_MS = 15000UL;

// Weather
static float tempC = NAN;
static float tempMinC = NAN;
static float tempMaxC = NAN;
static float precipMm = NAN;
static float windSpeedMs = NAN;
static float windDirectionDeg = NAN;
static float uvIndex = NAN;
static time_t lastWeatherFetch = 0;
static const uint32_t WEATHER_INTERVAL_SEC = 10 * 60;

// KP-index
static float kpIndex = NAN;
// Crypto Variables
String cryptoSymbols[5] = {"BTCUSDT", "ETHUSDT", "XRPUSDT", "LTCUSDT", "SOLUSDT"};
float cryptoPrices[5] = {NAN, NAN, NAN, NAN, NAN};
float cryptoPrevPrices[5] = {NAN, NAN, NAN, NAN, NAN};
float cryptoChanges[5] = {NAN, NAN, NAN, NAN, NAN};
int cryptoIntervalSec = 60;

// Fuel Variables
static float brentPrice = NAN;
static float a95Price = NAN;
static float dieselPrice = NAN;
static time_t lastFuelFetch = 0;
const int FUEL_INTERVAL_SEC = 3600;
static time_t lastSunFetch = 0;
static time_t lastKpFetch = 0;
static time_t lastCryptoFetch = 0;
const int CRYPTO_INTERVAL_SEC = 300;
static const uint32_t KP_INTERVAL_SEC = 10 * 60;

// Sunrise / Sunset
static int sunriseMin = -1;
static int sunsetMin  = -1;
static int lastSunYmd = -1;
static time_t lastSyncTime = 0;

// =========================================================
// SLEEP / BACKLIGHT
// =========================================================
const int BACKLIGHT_PIN = 21;

bool sleepDimmed = false;
bool sleepOff = false;
bool manualDimMode = false;

unsigned long lastInteractionMs = 0;

int sleepIntervalMin = 10;
int sleepOffDelaySec = 60;

const int BL_FULL = 255;
const int BL_DIM  = 18;
const int BL_OFF  = 0;
const int FLASH_BL_LOW = 20;
const int FLASH_BL_HIGH = 255;

void wakeDisplay(bool clearManualMode = true);

int sanitizeTimerMinutes(int value);

// =========================================================
// HELPERS
// =========================================================
static int ymdFromLocal(time_t t) {
  struct tm tmLocal;
  localtime_r(&t, &tmLocal);
  return (tmLocal.tm_year + 1900) * 10000 + (tmLocal.tm_mon + 1) * 100 + tmLocal.tm_mday;
}

static int minutesFromLocalEpoch(time_t t) {
  struct tm tmLocal;
  localtime_r(&t, &tmLocal);
  return tmLocal.tm_hour * 60 + tmLocal.tm_min;
}

static int minutesNowLocal() {
  time_t now = time(nullptr);
  struct tm tmNow;
  localtime_r(&now, &tmNow);
  return tmNow.tm_hour * 60 + tmNow.tm_min;
}

static bool useUsRegionFormat() {
  return regionFormatKey == "us";
}

static String formatClockParts(const struct tm& tmValue, bool withSeconds) {
  char buf[20];
  const char* pattern = useUsRegionFormat()
    ? (withSeconds ? "%I:%M:%S %p" : "%I:%M %p")
    : (withSeconds ? "%H:%M:%S" : "%H:%M");
  strftime(buf, sizeof(buf), pattern, &tmValue);
  return String(buf);
}

static String formatDateParts(const struct tm& tmValue) {
  char buf[32];
  strftime(buf, sizeof(buf), useUsRegionFormat() ? "%a %m/%d/%Y" : "%a %d.%m.%Y", &tmValue);
  return String(buf);
}

static String formatMinuteOfDay(int minOfDay) {
  if (minOfDay < 0) return "--:--";
  if (useUsRegionFormat()) {
    int hour24 = minOfDay / 60;
    int minute = minOfDay % 60;
    int hour12 = hour24 % 12;
    if (hour12 == 0) hour12 = 12;
    char buf[12];
    snprintf(buf, sizeof(buf), "%d:%02d %s", hour12, minute, hour24 >= 12 ? "PM" : "AM");
    return String(buf);
  }
  char buf[6];
  snprintf(buf, sizeof(buf), "%02d:%02d", minOfDay / 60, minOfDay % 60);
  return String(buf);
}

static String tempText() {
  if (isnan(tempC)) return unitKey == "imperial" ? "--.-F" : "--.-C";

  if (unitKey == "imperial") {
    float f = tempC * 9.0f / 5.0f + 32.0f;
    return String(f, 1) + "F";
  }

  return String(tempC, 1) + "C";
}

static String formatDisplayTemp(float value) {
  if (isnan(value)) return "--";

  if (unitKey == "imperial") {
    float f = value * 9.0f / 5.0f + 32.0f;
    return String((int)roundf(f)) + "F";
  }

  return String((int)roundf(value)) + "C";
}

static String tempRangeText() {
  return "H:" + formatDisplayTemp(tempMaxC) + "  L:" + formatDisplayTemp(tempMinC);
}

static String rainText() {
  if (isnan(precipMm)) return unitKey == "imperial" ? "--.--in" : "--.-mm";

  if (unitKey == "imperial") {
    float inches = precipMm / 25.4f;
    return String(inches, 2) + "in";
  }

  return String(precipMm, 1) + "mm";
}

static String windText() {
  if (isnan(windSpeedMs)) return unitKey == "imperial" ? "--.-mph" : "--.-m/s";

  if (unitKey == "imperial") {
    float mph = windSpeedMs * 2.236936f;
    return String(mph, 1) + "mph";
  }

  return String(windSpeedMs, 1) + "m/s";
}

static String windDirectionText() {
  if (isnan(windDirectionDeg)) return "--";

  const char* dirs[] = {"N", "NE", "E", "SE", "S", "SW", "W", "NW"};
  int idx = (int)roundf(windDirectionDeg / 45.0f) % 8;
  return String(dirs[idx]) + " " + String((int)roundf(windDirectionDeg)) + "deg";
}

static String kpText() {
  return isnan(kpIndex) ? "Kp --" : "Kp " + String(kpIndex, 1);
}

static String kpLevelText() {
  if (isnan(kpIndex)) return "--";
  if (kpIndex < 3.0f) return "Low";
  if (kpIndex < 5.0f) return "Medium";
  if (kpIndex < 7.0f) return "High";
  return "Extreme";
}

static String uvText() {
  return isnan(uvIndex) ? "UV --" : "UV " + String(uvIndex, 1);
}

static String uvLevelText() {
  if (isnan(uvIndex)) return "--";
  if (uvIndex < 3.0f) return "Low";
  if (uvIndex < 6.0f) return "Moderate";
  if (uvIndex < 8.0f) return "High";
  if (uvIndex < 11.0f) return "Very High";
  return "Extreme";
}

static String nextSunLabel() {
  int nowMin = minutesNowLocal();
  if (sunriseMin < 0 || sunsetMin < 0) return "Sun";
  if (nowMin < sunriseMin) return "Sunrise";
  if (nowMin < sunsetMin) return "Sunset";
  return "Sunrise";
}

static String nextSunTimeText() {
  int nowMin = minutesNowLocal();
  if (sunriseMin < 0 || sunsetMin < 0) return "--:--";
  if (nowMin < sunriseMin) return formatMinuteOfDay(sunriseMin);
  if (nowMin < sunsetMin) return formatMinuteOfDay(sunsetMin);
  return formatMinuteOfDay(sunriseMin);
}

static String htmlEscape(const String& s) {
  String out;
  out.reserve(s.length());
  for (size_t i = 0; i < s.length(); i++) {
    char c = s[i];
    if (c == '&') out += "&amp;";
    else if (c == '<') out += "&lt;";
    else if (c == '>') out += "&gt;";
    else if (c == '"') out += "&quot;";
    else out += c;
  }
  return out;
}

static String cssColorFrom565(uint16_t color) {
  uint8_t r = ((color >> 11) & 0x1F) * 255 / 31;
  uint8_t g = ((color >> 5) & 0x3F) * 255 / 63;
  uint8_t b = (color & 0x1F) * 255 / 31;
  char buf[8];
  snprintf(buf, sizeof(buf), "#%02X%02X%02X", r, g, b);
  return String(buf);
}

static String accentPreviewCss(const String& key) {
  if (key == "standard") return cssColorFrom565(0xEF7D);
  if (key == "ice")      return cssColorFrom565(0xEFFF);
  if (key == "white")    return cssColorFrom565(TFT_WHITE);
  if (key == "cyan")     return cssColorFrom565(0x5EFA);
  if (key == "mint")     return cssColorFrom565(0x07F0);
  if (key == "green")    return cssColorFrom565(TFT_GREEN);
  if (key == "blue")     return cssColorFrom565(0x3D9F);
  if (key == "purple")   return cssColorFrom565(0xA2F5);
  if (key == "pink")     return cssColorFrom565(0xF97F);
  if (key == "orange")   return cssColorFrom565(0xFD20);
  if (key == "amber")    return cssColorFrom565(0xFEA0);
  if (key == "red")      return cssColorFrom565(TFT_RED);
  return cssColorFrom565(0xEF7D);
}

static String themePreviewCss(const String& key) {
  if (key == "slate")    return cssColorFrom565(0x08A3);
  if (key == "deep")     return cssColorFrom565(0x0000);
  if (key == "nordic")   return cssColorFrom565(0x0864);
  if (key == "forest")   return cssColorFrom565(0x0208);
  if (key == "coffee")   return cssColorFrom565(0x18A3);
  if (key == "soft")     return cssColorFrom565(0x10A2);
  if (key == "midnight") return cssColorFrom565(0x0008);
  if (key == "graphite") return cssColorFrom565(0x1082);
  if (key == "garnet")   return cssColorFrom565(0x1004);
  if (key == "ochre")    return cssColorFrom565(0x20E1);
  return cssColorFrom565(0x08A3);
}

static String formatTimerClock(unsigned long totalSec) {
  unsigned long minutes = totalSec / 60UL;
  unsigned long seconds = totalSec % 60UL;

  char buf[10];
  snprintf(buf, sizeof(buf), "%02lu:%02lu", minutes, seconds);
  return String(buf);
}

static String focusHintText() {
  if (focusTimerFinished) return "Tap to reset";
  if (focusTimerRunning) return String((focusDurationSec / 60UL)) + " min session";
  return "Tap to start";
}

static String formatElapsedText(unsigned long totalSec) {
  unsigned long minutes = totalSec / 60UL;
  if (minutes == 0) return "< 1 minute elapsed";
  if (minutes == 1) return "1 minute elapsed";
  return String(minutes) + " minutes elapsed";
}

static String weekNumberText() {
  time_t now = time(nullptr);
  struct tm tmNow;
  localtime_r(&now, &tmNow);
  char buf[4];
  strftime(buf, sizeof(buf), "%V", &tmNow);
  return String(buf);
}

static String timerDoneCountdownText() {
  if (!timerDoneDialogOpen) return "";

  unsigned long elapsedMs = millis() - timerDoneDialogStartedMs;
  unsigned long remainingMs = (elapsedMs >= TIMER_DONE_DIALOG_MS) ? 0 : (TIMER_DONE_DIALOG_MS - elapsedMs);
  unsigned long remainingSec = (remainingMs + 999UL) / 1000UL;
  return String("Auto close in ") + String(remainingSec) + "s";
}

static String homeTitleText() {
  return buddyNickname.length() > 0 ? buddyNickname : "Crypto Display";
}

int sanitizeTimerMinutes(int value) {
  return constrain(value, 1, 180);
}

void resetFocusTimer() {
  focusTimerRunning = false;
  focusTimerFinished = false;
  focusMenuOpen = false;
  timerDoneDialogOpen = false;
  focusEndMs = 0;
  focusDurationSec = 0;
  focusRemainingSec = 0;
  cacheFocusTimer = "";
  clearHomeSlotCaches();
  cacheTimerMenu = "";
  cacheTimerDone = "";
  cacheTimerDoneCountdown = "";
  cacheTimerDoneFlash = "";
}

void startFocusTimer(unsigned long minutes) {
  focusDurationSec = minutes * 60UL;
  focusRemainingSec = focusDurationSec;
  focusEndMs = millis() + (focusDurationSec * 1000UL);
  focusTimerRunning = true;
  focusTimerFinished = false;
  focusMenuOpen = false;
  timerDoneDialogOpen = false;
  cacheFocusTimer = "";
  clearHomeSlotCaches();
  cacheTimerMenu = "";
  cacheTimerDone = "";
  cacheTimerDoneCountdown = "";
  cacheTimerDoneFlash = "";
}

void dismissTimerDoneDialog() {
  if (!timerDoneDialogOpen) return;
  timerDoneDialogOpen = false;
  timerDoneDialogStartedMs = 0;
  cacheTimerDone = "";
  cacheTimerDoneCountdown = "";
  cacheTimerDoneFlash = "";
  if (!sleepDimmed && !sleepOff) setBacklight(BL_FULL);
  pageDirty = true;
}

void openTimerDoneDialog() {
  timerDoneDialogOpen = true;
  timerDoneDialogStartedMs = millis();
  cacheTimerDone = "";
  cacheTimerDoneCountdown = "";
  wakeDisplay();
}

void updateFocusTimerState() {
  if (!focusTimerRunning) return;

  unsigned long now = millis();
  if ((long)(focusEndMs - now) <= 0) {
    focusRemainingSec = 0;
    focusTimerRunning = false;
    focusTimerFinished = true;
    focusMenuOpen = false;
    cacheFocusTimer = "";
    clearHomeSlotCaches();
    cacheTimerMenu = "";
    openTimerDoneDialog();
    return;
  }

  unsigned long remainingMs = focusEndMs - now;
  unsigned long nextRemainingSec = (remainingMs + 999UL) / 1000UL;
  if (nextRemainingSec != focusRemainingSec) {
    focusRemainingSec = nextRemainingSec;
    cacheFocusTimer = "";
    clearHomeSlotCaches();
  }
}

void updateTimerDoneDialogState() {
  if (!timerDoneDialogOpen) return;
  if (millis() - timerDoneDialogStartedMs >= TIMER_DONE_DIALOG_MS) {
    dismissTimerDoneDialog();
  }
}

void setBacklight(int value) {
  value = constrain(value, 0, 255);
  analogWrite(BACKLIGHT_PIN, value);
}

void wakeDisplay(bool clearManualMode) {
  sleepDimmed = false;
  sleepOff = false;
  if (clearManualMode) manualDimMode = false;
  lastInteractionMs = millis();
  setBacklight(BL_FULL);
  pageDirty = true;
}

void enterSleepDim() {
  if (sleepDimmed || sleepOff || manualDimMode) return;
  sleepDimmed = true;
  setBacklight(BL_DIM);
}

void enterSleepOff() {
  if (sleepOff) return;
  sleepOff = true;
  sleepDimmed = true;
  setBacklight(BL_OFF);
  pageDirty = true;
}

void toggleSleepMode() {
  if (manualDimMode) {
    wakeDisplay();
    return;
  }

  manualDimMode = true;
  sleepDimmed = true;
  sleepOff = false;
  setBacklight(BL_DIM);
  pageDirty = true;
}

void handleAutoSleep() {
  if (focusMenuOpen || timerDoneDialogOpen) return;
  if (sleepIntervalMin <= 0) return;

  unsigned long now = millis();
  unsigned long dimAfterMs = (unsigned long)sleepIntervalMin * 60UL * 1000UL;
  unsigned long offAfterMs = dimAfterMs + ((unsigned long)sleepOffDelaySec * 1000UL);

  if (!sleepDimmed && !sleepOff && now - lastInteractionMs > dimAfterMs) {
    enterSleepDim();
  }

  if (sleepDimmed && !sleepOff && now - lastInteractionMs > offAfterMs) {
    enterSleepOff();
  }
}

// =========================================================
// THEME / SETTINGS
// =========================================================
void applyThemeByKey(const String& accentKey, const String& bgKey) {
  if (accentKey == "standard")    COL_ACCENT = 0xEF7D;
  else if (accentKey == "cyan")   COL_ACCENT = 0x5EFA;
  else if (accentKey == "ice")    COL_ACCENT = 0xEFFF;
  else if (accentKey == "white")  COL_ACCENT = TFT_WHITE;
  else if (accentKey == "mint")   COL_ACCENT = 0x07F0;
  else if (accentKey == "green")  COL_ACCENT = TFT_GREEN;
  else if (accentKey == "blue")   COL_ACCENT = 0x3D9F;
  else if (accentKey == "purple") COL_ACCENT = 0xA2F5;
  else if (accentKey == "pink")   COL_ACCENT = 0xF97F;
  else if (accentKey == "orange") COL_ACCENT = 0xFD20;
  else if (accentKey == "amber")  COL_ACCENT = 0xFEA0;
  else if (accentKey == "red")    COL_ACCENT = TFT_RED;
  else                            COL_ACCENT = 0x5EFA;

  if (bgKey == "slate") {
    COL_BG = 0x08A3; COL_PANEL = 0x1106; COL_PANEL_ALT = 0x18C7; COL_STROKE = 0x31EC;
  } else if (bgKey == "deep") {
    COL_BG = 0x0000; COL_PANEL = 0x0841; COL_PANEL_ALT = 0x1082; COL_STROKE = 0x2945;
  } else if (bgKey == "nordic") {
    COL_BG = 0x0864; COL_PANEL = 0x10C6; COL_PANEL_ALT = 0x1908; COL_STROKE = 0x3A2D;
  } else if (bgKey == "forest") {
    COL_BG = 0x0208; COL_PANEL = 0x0ACB; COL_PANEL_ALT = 0x134D; COL_STROKE = 0x2D72;
  } else if (bgKey == "coffee") {
    COL_BG = 0x18A3; COL_PANEL = 0x2945; COL_PANEL_ALT = 0x39C7; COL_STROKE = 0x5A89;
  } else if (bgKey == "soft") {
    COL_BG = 0x10A2; COL_PANEL = 0x1924; COL_PANEL_ALT = 0x2145; COL_STROKE = 0x3A49;
  } else if (bgKey == "midnight") {
    COL_BG = 0x0008; COL_PANEL = 0x0011; COL_PANEL_ALT = 0x0018; COL_STROKE = 0x3A7F;
  } else if (bgKey == "graphite") {
    COL_BG = 0x1082; COL_PANEL = 0x18C3; COL_PANEL_ALT = 0x2104; COL_STROKE = 0x4208;
  } else if (bgKey == "garnet") {
    COL_BG = 0x1004; COL_PANEL = 0x1886; COL_PANEL_ALT = 0x20E8; COL_STROKE = 0x41AC;
  } else if (bgKey == "ochre") {
    COL_BG = 0x20E1; COL_PANEL = 0x3184; COL_PANEL_ALT = 0x4226; COL_STROKE = 0x632B;
  } else {
    COL_BG = 0x08A3; COL_PANEL = 0x1106; COL_PANEL_ALT = 0x18C7; COL_STROKE = 0x31EC;
  }
}

void applyTextColorByKey(const String& key) {
  textColorKey = key;

  if (key == "standard") {
    COL_TEXT = 0xEF7D; COL_DIM  = 0x94B2;
  } else if (key == "white") {
    COL_TEXT = TFT_WHITE; COL_DIM = 0xBDF7;
  } else if (key == "ice") {
    COL_TEXT = 0xEFFF; COL_DIM = 0x9D7F;
  } else if (key == "mint") {
    COL_TEXT = 0x07F0; COL_DIM = 0x05EC;
  } else if (key == "orange") {
    COL_TEXT = 0xFD20; COL_DIM = 0xBA26;
  } else if (key == "amber") {
    COL_TEXT = 0xFEA0; COL_DIM = 0xBCE0;
  } else if (key == "green") {
    COL_TEXT = TFT_GREEN; COL_DIM = 0x86E8;
  } else if (key == "cyan") {
    COL_TEXT = 0x5EFA; COL_DIM = 0x3D96;
  } else if (key == "blue") {
    COL_TEXT = 0x3D9F; COL_DIM = 0x22B1;
  } else if (key == "purple") {
    COL_TEXT = 0xA2F5; COL_DIM = 0x79ED;
  } else if (key == "red") {
    COL_TEXT = TFT_RED; COL_DIM = 0xB9E7;
  } else if (key == "pink") {
    COL_TEXT = 0xF97F; COL_DIM = 0xC2F1;
  } else {
    COL_TEXT = 0xEF7D;
    COL_DIM  = 0x94B2;
    textColorKey = "standard";
  }
}

void loadStoredSettings() {
  prefs.begin("Crypto Display", false);

  String accent = prefs.getString("accent", "cyan");
  String bg     = prefs.getString("bg", "slate");
  String txt    = prefs.getString("text", "standard");

  buddyNickname    = prefs.getString("nickname", "");
  locationName     = prefs.getString("locname", "Berlin");
  LAT              = prefs.getFloat("lat", 52.5200f);
  LNG              = prefs.getFloat("lng", 13.4050f);
  sleepIntervalMin = prefs.getInt("sleepMin", 10);
  unitKey          = prefs.getString("units", "metric");
  regionFormatKey  = prefs.getString("region", "europe");
  timezoneKey      = sanitizeTimezoneKey(prefs.getString("tz", "europe_central"));
  flashModeEnabled = prefs.getBool("flashMode", false);
  wifiEnabled      = prefs.getBool("wifiEnabled", true);

  for (int i = 0; i < HOME_SLOT_COUNT; i++) {
    String key = String("homeSlot") + String(i);
    homeWidgetSlots[i] = homeWidgetFromKey(prefs.getString(key.c_str(), homeWidgetKey(homeWidgetSlots[i])));
  }

  for (int i = 0; i < 6; i++) {
    String key = String("timer") + String(i);
    timerPresetMin[i] = sanitizeTimerMinutes(prefs.getInt(key.c_str(), timerPresetMin[i]));
  }

  for (int i = 0; i < 5; i++) {
    String key = String("crypto") + String(i);
    cryptoSymbols[i] = prefs.getString(key.c_str(), cryptoSymbols[i]);
  }

  currentLang       = prefs.getString("lang", "ua");
  if (currentLang != "ua" && currentLang != "en") currentLang = "ua";
  cryptoIntervalSec = prefs.getInt("crypto_int", 60);
  wifiSSID          = prefs.getString("wifi_ssid", WIFI_SSID);
  wifiPass          = prefs.getString("wifi_pass", WIFI_PASS);
  mqttEnabled       = prefs.getBool("mqtt_en", false);
  mqttServer        = prefs.getString("mqtt_srv", MQTT_SERVER);
  mqttPort          = prefs.getInt("mqtt_prt", MQTT_PORT);
  mqttUser          = prefs.getString("mqtt_usr", MQTT_USER);
  mqttPass          = prefs.getString("mqtt_pwd", MQTT_PASS);

  if (unitKey != "metric" && unitKey != "imperial") unitKey = "metric";
  if (regionFormatKey != "europe" && regionFormatKey != "us") regionFormatKey = "europe";
  buddyNickname.trim();
  applyThemeByKey(accent, bg);
  applyTextColorByKey(txt);
  applyDeviceTimezoneByKey(timezoneKey);
}

void resetDataCaches() {
  tempC = NAN;
  precipMm = NAN;
  windSpeedMs = NAN;
  windDirectionDeg = NAN;
  kpIndex = NAN;
  for (int i=0; i<5; i++) {
    cryptoPrices[i] = NAN;
    cryptoChanges[i] = NAN;
  }
  sunriseMin = -1;
  sunsetMin  = -1;
  lastSunYmd = -1;
  lastSunFetch = 0;
  lastKpFetch = 0;
  lastCryptoFetch = 0;
  dataDirty = true;
  pageDirty = true;
}

// =========================================================
// TOUCH
// =========================================================
bool readTouchXY(int& sx, int& sy) {
  if (!ts.touched()) return false;

  TS_Point p = ts.getPoint();
  if (p.z < 80 || p.z > 4000) return false;

  int x = map(p.x, TOUCH_X_MIN, TOUCH_X_MAX, 0, SCREEN_W);
  int y = map(p.y, TOUCH_Y_MIN, TOUCH_Y_MAX, 0, SCREEN_H);

  x = constrain(x, 0, SCREEN_W - 1);
  y = constrain(y, 0, SCREEN_H - 1);

  if (TOUCH_SWAP_XY) { int tmp = x; x = y; y = tmp; }
  if (TOUCH_FLIP_X)  x = (SCREEN_W - 1) - x;
  if (TOUCH_FLIP_Y)  y = (SCREEN_H - 1) - y;

  sx = x;
  sy = y;
  return true;
}

bool touchNewPress(int& tx, int& ty) {
  static bool wasDown = false;
  static unsigned long lastPressMs = 0;

  bool down = false;
  int x = 0, y = 0;

  if (readTouchXY(x, y)) down = true;

  bool fire = false;
  unsigned long now = millis();

  if (down && !wasDown && (now - lastPressMs > 220)) {
    fire = true;
    lastPressMs = now;
    tx = x;
    ty = y;
  }

  wasDown = down;
  return fire;
}

// =========================================================
// API
// =========================================================
bool fetchSunriseSunset() {
  if (WiFi.status() != WL_CONNECTED) return false;

  WiFiClientSecure client;
  client.setInsecure();

  String url = String("https://api.sunrise-sunset.org/json?lat=") + String(LAT, 4) +
               "&lng=" + String(LNG, 4) + "&formatted=0";

  HTTPClient http;
  if (!http.begin(client, url)) return false;

  int code = http.GET();
  if (code != 200) {
    http.end();
    return false;
  }

  String body = http.getString();
  http.end();

  StaticJsonDocument<1024> doc;
  if (deserializeJson(doc, body)) return false;

  const char* sunriseStr = doc["results"]["sunrise"];
  const char* sunsetStr  = doc["results"]["sunset"];
  if (!sunriseStr || !sunsetStr) return false;

  auto parseIsoToEpochUTC = [](const char* iso) -> time_t {
    int Y, M, D, h, m, s;
    if (sscanf(iso, "%d-%d-%dT%d:%d:%d", &Y, &M, &D, &h, &m, &s) != 6) return (time_t)-1;

    struct tm t{};
    t.tm_year = Y - 1900;
    t.tm_mon  = M - 1;
    t.tm_mday = D;
    t.tm_hour = h;
    t.tm_min  = m;
    t.tm_sec  = s;

    char* oldTz = getenv("TZ");
    String old = oldTz ? String(oldTz) : String("");

    setenv("TZ", "UTC0", 1);
    tzset();
    time_t epoch = mktime(&t);

    if (old.length()) setenv("TZ", old.c_str(), 1);
    else unsetenv("TZ");
    tzset();

    return epoch;
  };

  time_t srEpoch = parseIsoToEpochUTC(sunriseStr);
  time_t ssEpoch = parseIsoToEpochUTC(sunsetStr);
  if (srEpoch < 0 || ssEpoch < 0) return false;

  sunriseMin = minutesFromLocalEpoch(srEpoch);
  sunsetMin  = minutesFromLocalEpoch(ssEpoch);
  lastSunYmd = ymdFromLocal(time(nullptr));
  lastSyncTime = time(nullptr);
  return true;
}

void ensureSunTimesForToday() {
  time_t nowT = time(nullptr);
  int ymd = ymdFromLocal(nowT);

  if ((sunriseMin < 0 || sunsetMin < 0 || ymd != lastSunYmd) &&
      WiFi.status() == WL_CONNECTED) {
    if (fetchSunriseSunset()) dataDirty = true;
  }
}

void ensureSun() {
  ensureSunTimesForToday();
  time_t nowT = time(nullptr);
  if (lastSunFetch == 0 && WiFi.status() == WL_CONNECTED) {
    if (fetchSunriseSunset()) {
      lastSunFetch = nowT;
      dataDirty = true;
    }
  }
}

bool fetchWeather() {
  if (WiFi.status() != WL_CONNECTED) return false;

  WiFiClientSecure client;
  client.setInsecure();

  String url = String("https://api.open-meteo.com/v1/forecast?latitude=") + String(LAT, 4) +
               "&longitude=" + String(LNG, 4) +
               "&current=temperature_2m,wind_speed_10m,wind_direction_10m,uv_index" +
               "&hourly=precipitation" +
               "&daily=temperature_2m_max,temperature_2m_min" +
               "&forecast_days=1&timezone=auto&wind_speed_unit=ms";

  HTTPClient http;
  if (!http.begin(client, url)) return false;

  int code = http.GET();
  if (code != 200) {
    http.end();
    return false;
  }

  String body = http.getString();
  http.end();

  StaticJsonDocument<4096> doc;
  if (deserializeJson(doc, body)) return false;

  tempC = doc["current"]["temperature_2m"] | NAN;
  windSpeedMs = doc["current"]["wind_speed_10m"] | NAN;
  windDirectionDeg = doc["current"]["wind_direction_10m"] | NAN;
  uvIndex = doc["current"]["uv_index"] | NAN;
  tempMaxC = NAN;
  tempMinC = NAN;

  JsonArray maxTemps = doc["daily"]["temperature_2m_max"];
  JsonArray minTemps = doc["daily"]["temperature_2m_min"];
  if (maxTemps && !maxTemps.isNull() && maxTemps.size() > 0) tempMaxC = maxTemps[0] | NAN;
  if (minTemps && !minTemps.isNull() && minTemps.size() > 0) tempMinC = minTemps[0] | NAN;

  JsonArray times = doc["hourly"]["time"];
  JsonArray precs = doc["hourly"]["precipitation"];

  if (times && precs) {
    time_t nowT = time(nullptr);
    struct tm tmNow;
    localtime_r(&nowT, &tmNow);

    char key[20];
    strftime(key, sizeof(key), "%Y-%m-%dT%H:00", &tmNow);

    int idx = -1;
    for (int i = 0; i < (int)times.size(); i++) {
      const char* t = times[i];
      if (t && String(t).startsWith(key)) {
        idx = i;
        break;
      }
    }
    if (idx < 0) idx = 0;
    precipMm = precs[idx] | NAN;
  }

  lastWeatherFetch = time(nullptr);
  lastSyncTime = lastWeatherFetch;
  return true;
}

void ensureWeather() {
  time_t nowT = time(nullptr);
  if ((isnan(tempC) || isnan(tempMinC) || isnan(tempMaxC) || isnan(precipMm) || isnan(windSpeedMs) || isnan(windDirectionDeg) || isnan(uvIndex) ||
       (nowT - lastWeatherFetch) > WEATHER_INTERVAL_SEC) &&
      WiFi.status() == WL_CONNECTED) {
    if (fetchWeather()) dataDirty = true;
  }
}

bool fetchKpIndex() {
  if (WiFi.status() != WL_CONNECTED) return false;

  WiFiClientSecure client;
  client.setInsecure();

  HTTPClient http;
  if (!http.begin(client, "https://services.swpc.noaa.gov/products/noaa-planetary-k-index.json")) {
    return false;
  }

  int code = http.GET();
  if (code != 200) {
    http.end();
    return false;
  }

  String body = http.getString();
  http.end();

  int lastRow = body.lastIndexOf('[');
  if (lastRow < 0) return false;

  int firstComma = body.indexOf(',', lastRow);
  if (firstComma < 0) return false;

  int q1 = body.indexOf('"', firstComma);
  if (q1 < 0) return false;
  int q2 = body.indexOf('"', q1 + 1);
  if (q2 < 0) return false;

  String kpStrLocal = body.substring(q1 + 1, q2);
  kpIndex = kpStrLocal.toFloat();

  lastKpFetch = time(nullptr);
  lastSyncTime = lastKpFetch;
  return true;
}

void ensureKpIndex() {
  time_t nowT = time(nullptr);
  if ((isnan(kpIndex) || (nowT - lastKpFetch) > KP_INTERVAL_SEC) &&
      WiFi.status() == WL_CONNECTED) {
    if (fetchKpIndex()) dataDirty = true;
  }
}

String formatCryptoPrice(float price) {
  if (isnan(price)) return "--";
  if (price >= 1000.0f) {
    return String(price, 2) + "$";
  } else if (price >= 1.0f) {
    return String(price, 2) + "$";
  } else if (price >= 0.1f) {
    return String(price, 3) + "$";
  } else if (price >= 0.001f) {
    return String(price, 4) + "$";
  } else if (price >= 0.00001f) {
    return String(price, 6) + "$";
  } else {
    return String(price, 8) + "$";
  }
}

String sanitizeCryptoSymbol(String sym) {
  sym.trim();
  sym.toUpperCase();
  if (sym.length() == 0) return "BTCUSDT";
  if (!sym.endsWith("USDT") && !sym.endsWith("BUSD") && !sym.endsWith("USDC") && !sym.endsWith("FDUSD") && !sym.endsWith("EUR")) {
    sym += "USDT";
  }
  return sym;
}

bool fetchCrypto() {
  if (WiFi.status() != WL_CONNECTED) return false;

  WiFiClientSecure client;
  client.setInsecure();

  String symbolsParam = "%5B";
  for (int i=0; i<5; i++) {
    String sym = cryptoSymbols[i];
    sym.trim();
    sym.toUpperCase();
    symbolsParam += "%22" + sym + "%22";
    if (i < 4) symbolsParam += ",";
  }
  symbolsParam += "%5D";

  String url = "https://api.binance.com/api/v3/ticker/24hr?symbols=" + symbolsParam;

  HTTPClient http;
  http.setTimeout(5000);
  if (!http.begin(client, url)) {
    return false;
  }
  http.setUserAgent("ESP32-Crypto-Display");
  
  if (String(BINANCE_API_KEY).length() > 0) {
    http.addHeader("X-MBX-APIKEY", BINANCE_API_KEY);
  }

  int code = http.GET();
  if (code != 200) {
    http.end();
    return false;
  }

  String body = http.getString();
  http.end();

  // Binance 24hr ticker for multiple symbols returns an array of objects
  DynamicJsonDocument doc(4096);
  if (deserializeJson(doc, body)) return false;

  for (JsonObject item : doc.as<JsonArray>()) {
    String symbol = item["symbol"].as<String>();
    float price = item["lastPrice"].as<float>();
    float change = item["priceChangePercent"].as<float>();
    
    for (int i=0; i<5; i++) {
      String target = cryptoSymbols[i];
      target.trim();
      target.toUpperCase();
      if (symbol.equalsIgnoreCase(target)) {
        if (!isnan(cryptoPrices[i])) {
          cryptoPrevPrices[i] = cryptoPrices[i];
        }
        cryptoPrices[i] = price;
        cryptoChanges[i] = change;
      }
    }
  }

  lastCryptoFetch = time(nullptr);
  lastSyncTime = lastCryptoFetch;
  return true;
}

void ensureCrypto() {
  time_t nowT = time(nullptr);
  bool needsFetch = false;
  for (int i=0; i<5; i++) {
    if (isnan(cryptoPrices[i])) needsFetch = true;
  }

  if ((needsFetch || (nowT - lastCryptoFetch) >= (time_t)cryptoIntervalSec) &&
      WiFi.status() == WL_CONNECTED) {
    if (fetchCrypto()) dataDirty = true;
  }
}

bool fetchFuelPrices() {
  if (WiFi.status() != WL_CONNECTED) return false;
  WiFiClientSecure client;
  client.setInsecure();
  HTTPClient http;

  // 1. Brent Oil
  if (http.begin(client, "https://query1.finance.yahoo.com/v8/finance/chart/BZ=F")) {
    http.setUserAgent("Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/120.0.0.0 Safari/537.36");
    http.setTimeout(4000);
    if (http.GET() == 200) {
      String body = http.getString();
      StaticJsonDocument<2048> doc;
      if (!deserializeJson(doc, body)) {
        brentPrice = doc["chart"]["result"][0]["meta"]["regularMarketPrice"].as<float>();
      }
    }
    http.end();
  }

  // 2. Ukraine Gas
  if (http.begin(client, "https://index.minfin.com.ua/markets/fuel/")) {
    http.setUserAgent("Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/120.0.0.0 Safari/537.36");
    http.setTimeout(4000);
    http.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);
    if (http.GET() == 200) {
      String body = http.getString();
      int idx95 = body.indexOf("А-95");
      if (idx95 > 0) {
        int tdIdx = body.indexOf("<big>", idx95);
        if (tdIdx > 0) {
          int endIdx = body.indexOf("</big>", tdIdx);
          String valStr = body.substring(tdIdx + 5, endIdx);
          valStr.replace(",", ".");
          a95Price = valStr.toFloat();
        }
      }
      int idxDiesel = body.indexOf("Дизельне паливо");
      if (idxDiesel > 0) {
        int tdIdx = body.indexOf("<big>", idxDiesel);
        if (tdIdx > 0) {
          int endIdx = body.indexOf("</big>", tdIdx);
          String valStr = body.substring(tdIdx + 5, endIdx);
          valStr.replace(",", ".");
          dieselPrice = valStr.toFloat();
        }
      }
    }
    http.end();
  }

  lastFuelFetch = time(nullptr);
  return true;
}

void ensureFuel() {
  time_t nowT = time(nullptr);
  if ((isnan(brentPrice) || (nowT - lastFuelFetch) > FUEL_INTERVAL_SEC) && WiFi.status() == WL_CONNECTED) {
    if (fetchFuelPrices()) dataDirty = true;
  }
}

// =========================================================
// DRAW HELPERS
// =========================================================
void drawCard(int x, int y, int w, int h, bool accent = false) {
  tft.fillRoundRect(x, y, w, h, 10, COL_PANEL);
  tft.drawRoundRect(x, y, w, h, 10, accent ? COL_ACCENT : COL_STROKE);
}

void drawTopBar(const String& title = "") {
  tft.fillRect(0, 0, SCREEN_W, TOPBAR_H, COL_PANEL_ALT);
  tft.drawFastHLine(0, TOPBAR_H - 1, SCREEN_W, COL_STROKE);

  tft.setTextDatum(TL_DATUM);
  tft.setTextColor(COL_TEXT, COL_PANEL_ALT);
  tft.drawString(title, 10, 9, 2);

  const int bs = 25;
  const int bx = SCREEN_W - bs - 6;
  const int by = 4;

  uint16_t bg = (sleepDimmed || sleepOff) ? COL_ACCENT : COL_PANEL;
  uint16_t fg = (sleepDimmed || sleepOff) ? TFT_BLACK : COL_TEXT;

  tft.fillRoundRect(bx, by, bs, bs, 7, bg);
  tft.drawRoundRect(bx, by, bs, bs, 7, COL_ACCENT);

  const int cx = bx + 12;
  const int cy = by + 12;

  tft.drawCircle(cx, cy + 1, 6, fg);
  tft.drawFastHLine(cx - 4, cy - 5, 9, bg);
  tft.drawFastVLine(cx, cy - 7, 6, fg);
}

void drawNavBar() {
  const int y = SCREEN_H - NAV_H;
  tft.fillRect(0, y, SCREEN_W, NAV_H, COL_PANEL_ALT);
  tft.drawFastHLine(0, y, SCREEN_W, COL_STROKE);

  int dotRadius = 4;
  int spacing = 18;
  int startX = (SCREEN_W - (TOTAL_PAGES - 1) * spacing) / 2;

  for (int i = 0; i < TOTAL_PAGES; i++) {
    int cx = startX + i * spacing;
    int cy = y + NAV_H / 2;
    if (i == (int)currentPage) {
      tft.fillCircle(cx, cy, dotRadius, COL_ACCENT);
    } else {
      tft.fillCircle(cx, cy, dotRadius, COL_STROKE);
    }
  }
}

void makeSpriteCard(TFT_eSprite& spr, int w, int h, bool accent = false) {
  spr.setColorDepth(16);
  spr.createSprite(w, h);
  spr.fillSprite(COL_BG);
  spr.fillRoundRect(0, 0, w, h, 10, COL_PANEL);
  spr.drawRoundRect(0, 0, w, h, 10, accent ? COL_ACCENT : COL_STROKE);
}

void pushSpriteAndDelete(TFT_eSprite& spr, int x, int y) {
  spr.pushSprite(x, y, COL_BG);
  spr.deleteSprite();
}

void drawCleanSunIcon(TFT_eSprite& spr, int cx, int cy, uint16_t c) {
  spr.fillCircle(cx, cy, 4, c);
  spr.drawLine(cx, cy - 9, cx, cy - 7, c);
  spr.drawLine(cx, cy + 7, cx, cy + 9, c);
  spr.drawLine(cx - 9, cy, cx - 7, cy, c);
  spr.drawLine(cx + 7, cy, cx + 9, cy, c);
  spr.drawLine(cx - 6, cy - 6, cx - 5, cy - 5, c);
  spr.drawLine(cx + 5, cy - 5, cx + 6, cy - 6, c);
  spr.drawLine(cx - 6, cy + 6, cx - 5, cy + 5, c);
  spr.drawLine(cx + 5, cy + 5, cx + 6, cy + 6, c);
}

void drawMoonIcon(TFT_eSprite& spr, int cx, int cy, uint16_t c) {
  spr.fillCircle(cx, cy, 6, c);
  spr.fillCircle(cx + 4, cy - 2, 6, COL_PANEL);
}

// =========================================================
// HOME SPRITES
// =========================================================
void drawClockCardSprite(bool force = false) {
  const int x = 8, y = PAGE_ROW1_Y, w = 224, h = HOME_WIDGET_H;

  time_t now = time(nullptr);
  struct tm tmNow;
  localtime_r(&now, &tmNow);

  String timeBuf = formatClockParts(tmNow, true);
  String dateBuf = formatDateParts(tmNow);

  String sr = formatMinuteOfDay(sunriseMin);
  String ss = formatMinuteOfDay(sunsetMin);

  String combined = timeBuf + "|" + dateBuf + "|" + sr + "|" + ss + "|" +
                    String(COL_ACCENT) + "|" + String(COL_TEXT);

  if (!force && combined == cacheClock) return;
  cacheClock = combined;

  makeSpriteCard(sprClock, w, h, true);

  sprClock.setTextDatum(TL_DATUM);

  sprClock.setTextColor(COL_TEXT, COL_PANEL);
  if (useUsRegionFormat()) {
    int splitAt = timeBuf.lastIndexOf(' ');
    String clockMain = splitAt > 0 ? timeBuf.substring(0, splitAt) : timeBuf;
    String clockSuffix = splitAt > 0 ? timeBuf.substring(splitAt + 1) : "";
    sprClock.drawString(clockMain, 10, 11, 4);
    if (clockSuffix.length() > 0) {
      int suffixX = 10 + sprClock.textWidth(clockMain, 4) + 4;
      sprClock.drawString(clockSuffix, suffixX, 18, 2);
    }
  } else {
    sprClock.drawString(timeBuf, 10, 11, 4);
  }

  sprClock.setTextColor(COL_DIM, COL_PANEL);
  sprClock.drawString(dateBuf, 10, 45, 2);

  drawCleanSunIcon(sprClock, 151, 22, COL_ACCENT);
  drawMoonIcon(sprClock, 151, 50, COL_ACCENT);

  sprClock.setTextColor(COL_ACCENT, COL_PANEL);
  sprClock.drawString(sr, 165, 15, 2);
  sprClock.drawString(ss, 165, 43, 2);

  pushSpriteAndDelete(sprClock, x, y);
}

void drawMetricSprite(int x, int y, int w, int h, const char* label, const String& value, String& cache, bool force = false, const String& detail = "") {
  String combined = String(label) + "|" + value + "|" + detail + "|" + String(COL_PANEL) + "|" +
                    String(COL_STROKE) + "|" + String(COL_TEXT);

  if (!force && combined == cache) return;
  cache = combined;

  makeSpriteCard(sprSmall, w, h, true);

  sprSmall.setTextDatum(TL_DATUM);
  sprSmall.setTextColor(COL_DIM, COL_PANEL);
  sprSmall.drawString(label, 10, 8, 2);

  sprSmall.setTextColor(COL_TEXT, COL_PANEL);
  sprSmall.drawString(value, 10, 31, 4);

  if (detail.length() > 0) {
    sprSmall.setTextColor(COL_ACCENT, COL_PANEL);
    sprSmall.drawString(detail, 10, 55, 1);
  }

  pushSpriteAndDelete(sprSmall, x, y);
}

void drawWeatherStyleMetricSprite(int x, int y, int w, int h, const char* label, const String& value, String& cache, bool force = false, const String& detail = "") {
  String combined = String(label) + "|" + value + "|" + detail + "|" + String(COL_PANEL) + "|" +
                    String(COL_STROKE) + "|" + String(COL_TEXT);

  if (!force && combined == cache) return;
  cache = combined;

  makeSpriteCard(sprSmall, w, h, true);

  sprSmall.setTextDatum(TL_DATUM);
  sprSmall.setTextColor(COL_DIM, COL_PANEL);
  sprSmall.drawString(label, 10, 8, 2);

  sprSmall.setTextColor(COL_TEXT, COL_PANEL);
  sprSmall.drawString(value, 10, 28, 4);

  if (detail.length() > 0) {
    sprSmall.setTextColor(COL_ACCENT, COL_PANEL);
    sprSmall.drawString(detail, 10, 52, 1);
  }

  pushSpriteAndDelete(sprSmall, x, y);
}

void drawSunEventWidget(int x, int y, int w, int h, String& cache, bool force = false) {
  String label = nextSunLabel();
  String value = nextSunTimeText();
  String combined = label + "|" + value + "|" + String(COL_PANEL) + "|" +
                    String(COL_STROKE) + "|" + String(COL_TEXT) + "|" + String(COL_ACCENT);

  if (!force && combined == cache) return;
  cache = combined;

  makeSpriteCard(sprSmall, w, h, true);

  sprSmall.setTextDatum(TL_DATUM);
  sprSmall.setTextColor(COL_DIM, COL_PANEL);
  sprSmall.drawString(label, 10, 8, 2);

  sprSmall.setTextColor(COL_TEXT, COL_PANEL);
  if (useUsRegionFormat()) {
    int splitAt = value.lastIndexOf(' ');
    String mainValue = splitAt > 0 ? value.substring(0, splitAt) : value;
    String suffix = splitAt > 0 ? value.substring(splitAt + 1) : "";
    sprSmall.drawString(mainValue, 10, 30, 4);
    if (suffix.length() > 0) {
      int suffixX = 10 + sprSmall.textWidth(mainValue, 4) + 3;
      sprSmall.drawString(suffix, suffixX, 35, 2);
    }
  } else {
    sprSmall.drawString(value, 10, 30, 4);
  }

  pushSpriteAndDelete(sprSmall, x, y);
}

void drawPlaceholderSprite(int x, int y, int w, int h, const char* label, String& cache, bool force = false) {
  String combined = String(label) + "|" + String(COL_PANEL) + "|" + String(COL_STROKE) + "|" + String(COL_TEXT);

  if (!force && combined == cache) return;
  cache = combined;

  makeSpriteCard(sprSmall, w, h, true);

  sprSmall.setTextDatum(TL_DATUM);
  sprSmall.setTextColor(COL_DIM, COL_PANEL);
  sprSmall.drawString(label, 10, 8, 2);

  sprSmall.setTextColor(COL_STROKE, COL_PANEL);
  sprSmall.drawString("Empty", 10, 31, 2);

  pushSpriteAndDelete(sprSmall, x, y);
}

void drawFocusTimerWidget(int x, int y, int w, int h, String& cache, bool force = false) {
  String value = formatTimerClock(focusRemainingSec);
  String hint = focusHintText();
  String combined = value + "|" + hint + "|" + String(focusMenuOpen ? 1 : 0) +
                    "|" + String(COL_PANEL) + "|" + String(COL_ACCENT) + "|" + String(COL_TEXT);

  if (!force && combined == cache) return;
  cache = combined;

  makeSpriteCard(sprSmall, w, h, true);

  sprSmall.setTextDatum(TL_DATUM);
  sprSmall.setTextColor(COL_DIM, COL_PANEL);
  sprSmall.drawString("Timer", 10, 8, 2);

  if (focusMenuOpen) {
    sprSmall.setTextColor(COL_TEXT, COL_PANEL);
    sprSmall.drawString("Select", 10, 22, 4);
    sprSmall.setTextColor(COL_DIM, COL_PANEL);
    sprSmall.drawString("duration", 10, 44, 2);
  } else {
    sprSmall.setTextColor(focusTimerFinished ? COL_GREEN : COL_TEXT, COL_PANEL);
    sprSmall.drawString(value, 10, 24, 4);
    sprSmall.setTextColor(COL_DIM, COL_PANEL);
    sprSmall.drawString(hint, 10, 49, 1);
  }

  pushSpriteAndDelete(sprSmall, x, y);
}

void drawHomeSlotWidget(int slot, bool force = false) {
  int x, y, w, h;
  getHomeSlotRect(slot, x, y, w, h);

  switch (homeWidgetSlots[slot]) {
    case HOME_WIDGET_WEEK:
      drawMetricSprite(x, y, w, h, "Week", weekNumberText(), cacheHomeSlots[slot], force);
      break;
    case HOME_WIDGET_TIMER:
      drawFocusTimerWidget(x, y, w, h, cacheHomeSlots[slot], force);
      break;
    case HOME_WIDGET_RAIN:
      drawWeatherStyleMetricSprite(x, y, w, h, "Rain", rainText(), cacheHomeSlots[slot], force);
      break;
    case HOME_WIDGET_OUTDOOR:
      drawWeatherStyleMetricSprite(x, y, w, h, "Outdoor", tempText(), cacheHomeSlots[slot], force, tempRangeText());
      break;
    case HOME_WIDGET_KP:
      drawWeatherStyleMetricSprite(x, y, w, h, "KP index", kpText(), cacheHomeSlots[slot], force, kpLevelText());
      break;
    case HOME_WIDGET_UV:
      drawWeatherStyleMetricSprite(x, y, w, h, "UV index", uvText(), cacheHomeSlots[slot], force, uvLevelText());
      break;
    case HOME_WIDGET_WIND:
      drawWeatherStyleMetricSprite(x, y, w, h, "Wind", windText(), cacheHomeSlots[slot], force, windDirectionText());
      break;
    case HOME_WIDGET_SUN:
      drawSunEventWidget(x, y, w, h, cacheHomeSlots[slot], force);
      break;
    case HOME_WIDGET_BTC:
      drawWeatherStyleMetricSprite(x, y, w, h, "BTC Price", isnan(cryptoPrices[0]) ? "--" : 
String(cryptoPrices[0], 0) + "$", cacheHomeSlots[slot], force);
      break;
    case HOME_WIDGET_ETH:
      drawWeatherStyleMetricSprite(x, y, w, h, "ETH Price", isnan(cryptoPrices[1]) ? "--" : 
String(cryptoPrices[1], 0) + "$", cacheHomeSlots[slot], force);
      break;
  }
}

void drawFocusMenuOverlay(bool force = false) {
  String combined = String(focusTimerRunning ? 1 : 0) + "|" + String(focusTimerFinished ? 1 : 0) +
                    "|" + String(COL_PANEL_ALT) + "|" + String(COL_PANEL) + "|" + String(COL_ACCENT);
  if (!force && combined == cacheTimerMenu) return;
  cacheTimerMenu = combined;

  tft.fillRect(0, 0, SCREEN_W, SCREEN_H, COL_BG);
  tft.fillRoundRect(TIMER_MENU_X, TIMER_MENU_Y, TIMER_MENU_W, TIMER_MENU_H, 12, COL_PANEL_ALT);
  tft.drawRoundRect(TIMER_MENU_X, TIMER_MENU_Y, TIMER_MENU_W, TIMER_MENU_H, 12, COL_ACCENT);

  tft.setTextColor(COL_TEXT, COL_PANEL_ALT);
  tft.drawString("Start timer", TIMER_MENU_X + 14, TIMER_MENU_Y + 12, 2);
  tft.setTextColor(COL_DIM, COL_PANEL_ALT);
  tft.drawString("Choose a session length", TIMER_MENU_X + 14, TIMER_MENU_Y + 34, 1);

  const int btnW = 74;
  const int btnH = 28;
  const int col1X = TIMER_MENU_X + 14;
  const int col2X = TIMER_MENU_X + 112;
  const int row1Y = TIMER_MENU_Y + 54;
  const int row2Y = TIMER_MENU_Y + 88;
  const int row3Y = TIMER_MENU_Y + 122;

  String labels[6];
  const int xs[] = {col1X, col2X, col1X, col2X, col1X, col2X};
  const int ys[] = {row1Y, row1Y, row2Y, row2Y, row3Y, row3Y};
  for (int i = 0; i < 6; i++) {
    labels[i] = String(timerPresetMin[i]) + " min";
  }

  for (int i = 0; i < 6; i++) {
    tft.fillRoundRect(xs[i], ys[i], btnW, btnH, 8, COL_PANEL);
    tft.drawRoundRect(xs[i], ys[i], btnW, btnH, 8, COL_STROKE);
    tft.setTextColor(COL_TEXT, COL_PANEL);
    tft.drawCentreString(labels[i].c_str(), xs[i] + btnW / 2, ys[i] + 7, 2);
  }

  const int actionY = TIMER_MENU_Y + 160;
  const int actionW = 152;
  const int actionX = TIMER_MENU_X + 24;
  const char* actionLabel = focusTimerRunning ? "Stop" : (focusTimerFinished ? "Reset" : nullptr);
  uint16_t actionColor = focusTimerRunning ? COL_RED : COL_ACCENT;

  if (actionLabel) {
    tft.fillRoundRect(actionX, actionY - 4, actionW, 26, 8, COL_PANEL);
    tft.drawRoundRect(actionX, actionY - 4, actionW, 26, 8, actionColor);
    tft.setTextColor(actionColor, COL_PANEL);
    tft.drawCentreString(actionLabel, actionX + actionW / 2, actionY + 4, 2);
  } else {
    tft.setTextColor(COL_DIM, COL_PANEL_ALT);
    tft.drawCentreString("Tap outside to close", TIMER_MENU_X + TIMER_MENU_W / 2, TIMER_MENU_Y + TIMER_MENU_H - 13, 1);
  }
}

void drawTimerDoneOverlay(bool force = false) {
  if (!timerDoneDialogOpen) return;

  String elapsed = formatElapsedText(focusDurationSec);
  String countdown = timerDoneCountdownText();
  bool flashOn = flashModeEnabled && ((millis() / 300UL) % 2UL == 0);
  setBacklight(flashModeEnabled ? (flashOn ? FLASH_BL_HIGH : FLASH_BL_LOW) : BL_FULL);
  String combined = elapsed + "|" + String(COL_PANEL_ALT) + "|" + String(COL_ACCENT) + "|" + String(COL_TEXT);
  String flashKey = String(flashOn ? 1 : 0);
  if (force || combined != cacheTimerDone || flashKey != cacheTimerDoneFlash) {
    cacheTimerDone = combined;
    cacheTimerDoneFlash = flashKey;
    cacheTimerDoneCountdown = "";

    uint16_t backdrop = flashOn ? COL_ACCENT : COL_BG;
    uint16_t panelBorder = flashOn ? TFT_WHITE : COL_ACCENT;
    tft.fillRect(0, 0, SCREEN_W, SCREEN_H, backdrop);
    tft.fillRoundRect(TIMER_DONE_X, TIMER_DONE_Y, TIMER_DONE_W, TIMER_DONE_H, 12, COL_PANEL_ALT);
    tft.drawRoundRect(TIMER_DONE_X, TIMER_DONE_Y, TIMER_DONE_W, TIMER_DONE_H, 12, panelBorder);

    tft.setTextColor(COL_TEXT, COL_PANEL_ALT);
    tft.drawCentreString("Timer complete", TIMER_DONE_X + TIMER_DONE_W / 2, TIMER_DONE_Y + 14, 2);
    tft.setTextColor(COL_ACCENT, COL_PANEL_ALT);
    tft.drawCentreString(elapsed, TIMER_DONE_X + TIMER_DONE_W / 2, TIMER_DONE_Y + 42, 2);
    tft.setTextColor(COL_DIM, COL_PANEL_ALT);
    tft.drawCentreString("Tap anywhere to acknowledge", TIMER_DONE_X + TIMER_DONE_W / 2, TIMER_DONE_Y + 68, 1);
  }

  if (force || countdown != cacheTimerDoneCountdown) {
    cacheTimerDoneCountdown = countdown;
    tft.fillRect(TIMER_DONE_X + 28, TIMER_DONE_Y + 82, TIMER_DONE_W - 56, 12, COL_PANEL_ALT);
    tft.setTextColor(COL_DIM, COL_PANEL_ALT);
    tft.drawCentreString(countdown.c_str(), TIMER_DONE_X + TIMER_DONE_W / 2, TIMER_DONE_Y + 82, 1);
  }
}

// =========================================================
// PAGES
// =========================================================
void drawHomePageFull() {
  tft.fillScreen(COL_BG);
  drawTopBar(homeTitleText());
  drawNavBar();

  cacheClock = "";
  cacheHomeEmpty1 = "";
  cacheHomeEmpty2 = "";
  cacheFocusTimer = "";
  for (int i = 0; i < HOME_SLOT_COUNT; i++) {
    cacheHomeSlots[i] = "";
  }

  pageDirty = false;
  lastDrawnPage = PAGE_HOME;

  drawClockCardSprite(true);
  for (int i = 0; i < HOME_SLOT_COUNT; i++) {
    drawHomeSlotWidget(i, true);
  }
  if (focusMenuOpen) drawFocusMenuOverlay(true);
  if (timerDoneDialogOpen) drawTimerDoneOverlay(true);
}

void updateHomeDynamic() {
  if (timerDoneDialogOpen) {
    drawTimerDoneOverlay(false);
    return;
  }

  if (focusMenuOpen) {
    drawFocusMenuOverlay(false);
    return;
  }

  drawClockCardSprite(false);
  for (int i = 0; i < HOME_SLOT_COUNT; i++) {
    drawHomeSlotWidget(i, false);
  }
}

void drawWeatherPageFull() {
  tft.fillScreen(COL_BG);
  drawTopBar("Weather");
  drawNavBar();

  drawCard(8, PAGE_ROW1_Y, 108, PAGE_WIDGET_H, true);
  drawCard(124, PAGE_ROW1_Y, 108, PAGE_WIDGET_H, true);
  drawCard(8, PAGE_ROW2_Y, 108, PAGE_WIDGET_H, true);
  drawCard(124, PAGE_ROW2_Y, 108, PAGE_WIDGET_H, true);
  drawCard(8, PAGE_ROW3_Y, 108, PAGE_WIDGET_H, true);
  drawCard(124, PAGE_ROW3_Y, 108, PAGE_WIDGET_H, true);

  pageDirty = false;
  lastDrawnPage = PAGE_WEATHER;

  lastTempText = "";
  lastRainText = "";
  lastUvText = "";
  lastUvLevelText = "";
  lastKpText = "";
  lastKpLevelText = "";
  lastWindText = "";
  lastWindDirText = "";
  lastNextSunLabel = "";
  lastNextSunTime = "";
}

void updateWeatherDynamic() {
  String t = tempText();
  String tr = tempRangeText();
  String tempCombined = t + "|" + tr;
  if (tempCombined != lastTempText) {
    tft.fillRect(18, PAGE_ROW1_Y + 30, 88, 30, COL_PANEL);
    tft.setTextColor(COL_DIM, COL_PANEL);
    tft.drawString("Outdoor", 18, PAGE_ROW1_Y + 8, 2);
    tft.setTextColor(COL_TEXT, COL_PANEL);
    tft.drawString(t, 18, PAGE_ROW1_Y + 30, 4);
    tft.setTextColor(COL_ACCENT, COL_PANEL);
    tft.drawString(tr, 18, PAGE_ROW1_Y + 54, 1);
    lastTempText = tempCombined;
  }

  String r = rainText();
  if (r != lastRainText) {
    tft.fillRect(134, PAGE_ROW1_Y + 30, 88, 24, COL_PANEL);
    tft.setTextColor(COL_DIM, COL_PANEL);
    tft.drawString("Rain", 134, PAGE_ROW1_Y + 8, 2);
    tft.setTextColor(COL_TEXT, COL_PANEL);
    tft.drawString(r, 134, PAGE_ROW1_Y + 30, 4);
    lastRainText = r;
  }

  String u = uvText();
  String ul = uvLevelText();
  if (u != lastUvText || ul != lastUvLevelText || dataDirty) {
    tft.fillRect(18, PAGE_ROW2_Y + 30, 88, 30, COL_PANEL);
    tft.setTextColor(COL_DIM, COL_PANEL);
    tft.drawString("UV index", 18, PAGE_ROW2_Y + 8, 2);
    tft.setTextColor(COL_TEXT, COL_PANEL);
    tft.drawString(u, 18, PAGE_ROW2_Y + 28, 4);
    tft.setTextColor(COL_ACCENT, COL_PANEL);
    tft.drawString(ul, 18, PAGE_ROW2_Y + 52, 1);
    lastUvText = u;
    lastUvLevelText = ul;
  }

  String w = windText();
  String wd = windDirectionText();
  if (w != lastWindText || wd != lastWindDirText) {
    tft.fillRect(134, PAGE_ROW2_Y + 30, 88, 30, COL_PANEL);
    tft.setTextColor(COL_DIM, COL_PANEL);
    tft.drawString("Wind", 134, PAGE_ROW2_Y + 8, 2);
    tft.setTextColor(COL_TEXT, COL_PANEL);
    tft.drawString(w, 134, PAGE_ROW2_Y + 28, 4);
    tft.setTextColor(COL_ACCENT, COL_PANEL);
    tft.drawString(wd, 134, PAGE_ROW2_Y + 52, 1);
    lastWindText = w;
    lastWindDirText = wd;
  }

  String nl = nextSunLabel();
  String nt = nextSunTimeText();
  if (nl != lastNextSunLabel || nt != lastNextSunTime) {
    tft.fillRect(18, PAGE_ROW3_Y + 24, 88, 30, COL_PANEL);
    tft.setTextColor(COL_DIM, COL_PANEL);
    tft.drawString(nl, 18, PAGE_ROW3_Y + 8, 2);
    tft.setTextColor(COL_TEXT, COL_PANEL);
    if (useUsRegionFormat()) {
      int splitAt = nt.lastIndexOf(' ');
      String sunMain = splitAt > 0 ? nt.substring(0, splitAt) : nt;
      String sunSuffix = splitAt > 0 ? nt.substring(splitAt + 1) : "";
      tft.drawString(sunMain, 18, PAGE_ROW3_Y + 26, 4);
      if (sunSuffix.length() > 0) {
        int suffixX = 18 + tft.textWidth(sunMain, 4) + 3;
        tft.drawString(sunSuffix, suffixX, PAGE_ROW3_Y + 31, 2);
      }
    } else {
      tft.drawString(nt, 18, PAGE_ROW3_Y + 26, 4);
    }
    lastNextSunLabel = nl;
    lastNextSunTime = nt;
  }

  String k = kpText();
  String kl = kpLevelText();
  if (k != lastKpText || kl != lastKpLevelText || dataDirty) {
    tft.fillRect(134, PAGE_ROW3_Y + 30, 88, 30, COL_PANEL);
    tft.setTextColor(COL_DIM, COL_PANEL);
    tft.drawString("KP index", 134, PAGE_ROW3_Y + 8, 2);
    tft.setTextColor(COL_TEXT, COL_PANEL);
    tft.drawString(k, 134, PAGE_ROW3_Y + 28, 4);
    tft.setTextColor(COL_ACCENT, COL_PANEL);
    tft.drawString(kl, 134, PAGE_ROW3_Y + 52, 1);
    lastKpText = k;
    lastKpLevelText = kl;
  }
}

void drawCurrentPageFull() {
  switch (currentPage) {
    case PAGE_HOME:    drawHomePageFull(); break;
    case PAGE_WEATHER: drawWeatherPageFull(); break;
    case PAGE_CRYPTO:  drawPageCryptoFull(); break;
    case PAGE_FUEL:    drawPageFuelFull(); break;
  }

  if (focusMenuOpen && currentPage == PAGE_HOME) drawFocusMenuOverlay(true);
  if (timerDoneDialogOpen) drawTimerDoneOverlay(true);
}

void updateCurrentPageDynamic() {
  if (timerDoneDialogOpen) {
    drawTimerDoneOverlay(false);
    return;
  }

  if (focusMenuOpen && currentPage == PAGE_HOME) {
    drawFocusMenuOverlay(false);
    return;
  }

  switch (currentPage) {
    case PAGE_HOME:    updateHomeDynamic(); break;
    case PAGE_WEATHER: updateWeatherDynamic(); break;
    case PAGE_CRYPTO:  
      ensureCrypto();
      if (dataDirty) {
        updateCryptoDynamic();
        dataDirty = false;
      }
      break;
    case PAGE_FUEL:
      ensureFuel();
      if (dataDirty) {
        updateFuelDynamic();
        dataDirty = false;
      }
      break;
  }
}

bool handleFocusMenuTouch(int x, int y) {
  if (!focusMenuOpen) return false;

  if (x < TIMER_MENU_X || x >= TIMER_MENU_X + TIMER_MENU_W || y < TIMER_MENU_Y || y >= TIMER_MENU_Y + TIMER_MENU_H) {
    focusMenuOpen = false;
    cacheTimerMenu = "";
    pageDirty = true;
    return true;
  }

  struct ButtonHit {
    int x;
    int y;
    int w;
    int h;
    int minutes;
  };

  const ButtonHit buttons[] = {
    {34, 124, 74, 28, timerPresetMin[0]},
    {132, 124, 74, 28, timerPresetMin[1]},
    {34, 158, 74, 28, timerPresetMin[2]},
    {132, 158, 74, 28, timerPresetMin[3]},
    {34, 192, 74, 28, timerPresetMin[4]},
    {132, 192, 74, 28, timerPresetMin[5]}
  };

  for (const ButtonHit& btn : buttons) {
    if (x >= btn.x && x < btn.x + btn.w && y >= btn.y && y < btn.y + btn.h) {
      startFocusTimer(btn.minutes);
      pageDirty = true;
      return true;
    }
  }

  if ((focusTimerRunning || focusTimerFinished) && x >= 44 && x < 196 && y >= 224 && y < 250) {
    if (focusTimerRunning || focusTimerFinished) {
      resetFocusTimer();
    } else {
      focusMenuOpen = false;
      cacheTimerMenu = "";
    }
    pageDirty = true;
    return true;
  }

  return true;
}

bool handleHomeTouch(int x, int y) {
  if (currentPage != PAGE_HOME) return false;

  if (focusMenuOpen) return handleFocusMenuTouch(x, y);

  for (int slot = 0; slot < HOME_SLOT_COUNT; slot++) {
    if (homeWidgetSlots[slot] != HOME_WIDGET_TIMER) continue;

    int slotX, slotY, slotW, slotH;
    getHomeSlotRect(slot, slotX, slotY, slotW, slotH);

    if (x >= slotX && x < slotX + slotW && y >= slotY && y < slotY + slotH) {
      if (focusTimerFinished) {
        resetFocusTimer();
      } else {
        focusMenuOpen = true;
        cacheFocusTimer = "";
        clearHomeSlotCaches();
        cacheTimerMenu = "";
      }
      pageDirty = true;
      return true;
    }
  }

  return false;
}

bool handleTimerDoneDialogTouch(int x, int y) {
  (void)x;
  (void)y;
  if (!timerDoneDialogOpen) return false;
  dismissTimerDoneDialog();
  return true;
}

void drawCryptoCard(int i) {
  int startY = 45;
  int rowH = 46;
  int padding = 4;
  int y = startY + i * rowH;
  
  tft.fillRoundRect(padding, y, SCREEN_W - padding*2, rowH - padding, 8, COL_PANEL);
  
  String displaySym = cryptoSymbols[i];
  if (displaySym.endsWith("USDT")) displaySym = displaySym.substring(0, displaySym.length() - 4);
  
  tft.setTextDatum(TL_DATUM);
  tft.setTextColor(COL_TEXT, COL_PANEL);
  tft.drawString(displaySym, padding + 10, y + 10, 2);
  
  if (isnan(cryptoPrices[i])) {
    tft.setTextDatum(TR_DATUM);
    tft.setTextColor(COL_MUTED, COL_PANEL);
    tft.drawString("--", SCREEN_W - padding - 10, y + 6, 2);
  } else {
    tft.setTextDatum(TR_DATUM);
    tft.setTextColor(COL_ACCENT, COL_PANEL);
    tft.drawString(formatCryptoPrice(cryptoPrices[i]), SCREEN_W - padding - 10, y + 6, 2);
    
    float change = cryptoChanges[i];
    uint16_t cColor = (change >= 0) ? TFT_GREEN : TFT_RED;
    String cStr = (change >= 0 ? "+" : "") + String(change, 2) + "%";
    
    // Immediate price tick direction indicator
    if (!isnan(cryptoPrevPrices[i]) && cryptoPrices[i] != cryptoPrevPrices[i]) {
      if (cryptoPrices[i] > cryptoPrevPrices[i]) {
        cStr += " ^";
      } else {
        cStr += " v";
      }
    }
    tft.setTextColor(cColor, COL_PANEL);
    tft.drawString(cStr, SCREEN_W - padding - 10, y + 22, 1);
  }
}

void drawPageCryptoFull() {
  tft.fillScreen(COL_BG);
  drawTopBar("Crypto Ticker");
  drawNavBar();

  for (int i = 0; i < 5; i++) {
    drawCryptoCard(i);
  }

  pageDirty = false;
  lastDrawnPage = PAGE_CRYPTO;
}

void updateCryptoDynamic() {
  for (int i = 0; i < 5; i++) {
    drawCryptoCard(i);
  }
}

void drawFuelCards() {
  int padding = 4;
  int startY = 50;

  // Brent Oil Card
  tft.fillRoundRect(padding, startY, SCREEN_W - padding*2, 60, 8, COL_PANEL);
  tft.setTextDatum(TL_DATUM);
  tft.setTextColor(COL_TEXT, COL_PANEL);
  tft.drawString("Brent Crude Oil", padding + 10, startY + 10, 2);
  tft.setTextDatum(TR_DATUM);
  if (isnan(brentPrice)) {
    tft.setTextColor(COL_MUTED, COL_PANEL);
    tft.drawString("--", SCREEN_W - padding - 10, startY + 10, 4);
  } else {
    tft.setTextColor(COL_ACCENT, COL_PANEL);
    tft.drawString(String(brentPrice, 2) + "$", SCREEN_W - padding - 10, startY + 10, 4);
  }

  // Ukraine Gas Card
  startY += 70;
  tft.fillRoundRect(padding, startY, SCREEN_W - padding*2, 100, 8, COL_PANEL);
  
  tft.setTextDatum(TC_DATUM);
  tft.setTextColor(COL_TEXT, COL_PANEL);
  tft.drawString("Ukraine Fuel Prices", SCREEN_W/2, startY + 10, 2);

  // A-95
  tft.setTextDatum(TL_DATUM);
  tft.setTextColor(COL_TEXT, COL_PANEL);
  tft.drawString("A-95:", padding + 20, startY + 40, 2);
  tft.setTextDatum(TR_DATUM);
  tft.setTextColor(COL_ACCENT, COL_PANEL);
  tft.drawString(isnan(a95Price) ? "--" : String(a95Price, 2) + " UAH", SCREEN_W - padding - 20, startY + 40, 2);

  // Diesel
  tft.setTextDatum(TL_DATUM);
  tft.setTextColor(COL_TEXT, COL_PANEL);
  tft.drawString("Diesel:", padding + 20, startY + 70, 2);
  tft.setTextDatum(TR_DATUM);
  tft.setTextColor(COL_ACCENT, COL_PANEL);
  tft.drawString(isnan(dieselPrice) ? "--" : String(dieselPrice, 2) + " UAH", SCREEN_W - padding - 20, startY + 70, 2);
}

void drawPageFuelFull() {
  tft.fillScreen(COL_BG);
  drawTopBar("Fuel & Oil");
  drawNavBar();
  drawFuelCards();

  pageDirty = false;
  lastDrawnPage = PAGE_FUEL;
}

void updateFuelDynamic() {
  drawFuelCards();
}

// =========================================================
// NAVIGATION
// =========================================================
void handleNavTouch(int x, int y) {
  // Navigation is now done by tapping left or right edges of the screen
  // Ignore header touches
  if (y < 40) return;

  // Left 30% of screen = previous
  if (x < SCREEN_W * 0.3) {
    int nextIdx = (int)currentPage - 1;
    if (nextIdx < 0) nextIdx = TOTAL_PAGES - 1;
    currentPage = (Page)nextIdx;
    pageDirty = true;
  } 
  // Right 30% of screen = next
  else if (x > SCREEN_W * 0.7) {
    int nextIdx = (int)currentPage + 1;
    if (nextIdx >= TOTAL_PAGES) nextIdx = 0;
    currentPage = (Page)nextIdx;
    pageDirty = true;
  }
}

// =========================================================
// WEB SERVER
// =========================================================
void handleRoot() {
  String accent = prefs.getString("accent", "cyan");
  String bg     = prefs.getString("bg", "slate");
  String txt    = prefs.getString("text", "standard");
  String units  = prefs.getString("units", "metric");
  String region = prefs.getString("region", "europe");
  String tz     = sanitizeTimezoneKey(prefs.getString("tz", "europe_central"));
  String nickname = prefs.getString("nickname", "");
  bool flashMode = prefs.getBool("flashMode", false);
  currentLang   = prefs.getString("lang", "ua");
  if (currentLang != "ua" && currentLang != "en") currentLang = "ua";
  bool isUa = (currentLang == "ua");

  String homeSlotKeys[HOME_SLOT_COUNT];
  for (int i = 0; i < HOME_SLOT_COUNT; i++) {
    homeSlotKeys[i] = prefs.getString((String("homeSlot") + String(i)).c_str(), homeWidgetKey(homeWidgetSlots[i]));
  }

  String page;
  page.reserve(28000);

  page += "<!doctype html><html><head>";
  page += "<meta charset='utf-8'>";
  page += "<meta name='viewport' content='width=device-width,initial-scale=1'>";
  page += "<title>Crypto Display</title>";
  page += "<style>";
  page += ":root{color-scheme:dark;}";
  page += "body{margin:0;background:linear-gradient(180deg,#0b1018 0%,#111827 100%);color:#edf2f7;font-family:system-ui,-apple-system,sans-serif;}";
  page += ".wrap{max-width:980px;margin:0 auto;padding:28px 16px 36px;}";
  page += ".hero{margin-bottom:18px;padding:20px;border:1px solid #243244;border-radius:20px;background:linear-gradient(135deg,#111927 0%,#172235 100%);box-shadow:0 10px 30px rgba(0,0,0,.22);}";
  page += ".hero h1{font-size:30px;margin:0 0 8px 0;}";
  page += ".hero p{margin:0;color:#a9b7c9;font-size:14px;}";
  page += ".ip{display:inline-block;padding:8px 14px;border-radius:999px;background:#0b1220;border:1px solid #334155;color:#dbe7f5;font-size:13px;}";
  page += ".layout{display:grid;grid-template-columns:1.15fr .85fr;gap:16px;align-items:start;}";
  page += ".stack{display:grid;gap:16px;}";
  page += ".panel{background:#171b22;border:1px solid #2d3748;border-radius:18px;padding:18px;margin:0;}";
  page += ".panel-toggle{width:100%;display:flex;align-items:center;justify-content:space-between;gap:12px;background:none;border:none;color:#edf2f7;padding:0;margin:0;cursor:pointer;text-align:left;}";
  page += ".panel-toggle:hover{color:#ffffff;}";
  page += ".panel-toggle h2{flex:1;}";
  page += ".panel-chevron{font-size:18px;color:#8ea3ba;transition:transform .18s ease;}";
  page += ".panel.collapsed .panel-chevron{transform:rotate(-90deg);}";
  page += ".panel-body{margin-top:12px;}";
  page += ".panel.collapsed .panel-body{display:none;}";
  page += ".panel h2{margin:0 0 6px 0;font-size:18px;}";
  page += ".panel p{margin:0 0 14px 0;color:#94a3b8;font-size:13px;line-height:1.45;}";
  page += ".grid{display:grid;grid-template-columns:1fr 1fr;gap:14px;}";
  page += ".grid-3{display:grid;grid-template-columns:1fr 1fr 1fr;gap:14px;}";
  page += ".label{display:block;font-size:13px;margin:0 0 8px 0;color:#a0aec0;font-weight:600;}";
  page += "textarea,input,select{width:100%;border-radius:12px;border:1px solid #334155;background:#0b1220;color:#edf2f7;padding:12px;box-sizing:border-box;font:inherit;}";
  page += "textarea{min-height:170px;resize:vertical;}";
  page += "button.submit-btn{margin-top:18px;background:#38bdf8;border:none;color:#001018;padding:14px 20px;border-radius:12px;font-weight:800;font-size:16px;cursor:pointer;width:100%;font:inherit;transition:background .2s;}";
  page += "button.submit-btn:hover{background:#7dd3fc;}";
  page += ".muted{font-size:13px;color:#94a3b8;line-height:1.45;}";
  page += ".footer-note{margin-top:10px;font-size:12px;color:#7f92a8;}";
  page += ".settings-block{margin-top:18px;padding-top:16px;border-top:1px solid #2b3545;}";
  page += ".settings-block:first-of-type{margin-top:0;padding-top:0;border-top:none;}";
  page += ".settings-title{display:block;margin:0 0 6px 0;font-size:14px;font-weight:700;color:#edf2f7;letter-spacing:.02em;}";
  page += ".settings-desc{margin:0 0 12px 0;font-size:12px;color:#8ea3ba;line-height:1.45;}";
  page += ".color-stack{display:grid;gap:12px;}";
  page += ".color-row{display:grid;grid-template-columns:120px 1fr;gap:12px;align-items:center;}";
  page += ".color-meta{display:flex;align-items:center;justify-content:space-between;gap:10px;}";
  page += ".color-meta .label{margin:0;color:#dbe7f5;}";
  page += ".color-value{font-size:12px;color:#8ea3ba;white-space:nowrap;}";
  page += ".swatch-row{display:flex;flex-wrap:wrap;gap:8px;}";
  page += ".swatch{width:22px;height:22px;border-radius:999px;border:1px solid rgba(255,255,255,.18);cursor:pointer;position:relative;box-sizing:border-box;}";
  page += ".swatch input{display:none;}";
  page += ".swatch.active{box-shadow:0 0 0 2px #67e8f9, 0 0 0 5px rgba(103,232,249,.18);}";
  page += ".swatch.active::after{content:'';position:absolute;inset:5px;border-radius:999px;border:1px solid rgba(0,16,24,.45);}";
  page += ".timer-slot-grid{display:grid;grid-template-columns:1fr 1fr 1fr;gap:10px;margin-top:14px;}";
  page += ".timer-slot{border:1px solid #334155;border-radius:12px;background:#0b1220;padding:10px 10px 12px 10px;}";
  page += ".timer-slot-head{font-size:12px;color:#8ea3ba;margin-bottom:8px;font-weight:600;}";
  page += ".timer-slot-input{display:flex;align-items:center;gap:8px;}";
  page += ".timer-slot input{padding:10px 12px;text-align:center;font-weight:700;}";
  page += ".timer-unit{font-size:12px;color:#8ea3ba;white-space:nowrap;}";
  page += "@media(max-width:820px){.layout{grid-template-columns:1fr;}.grid,.grid-3,.timer-slot-grid{grid-template-columns:1fr;}.color-row{grid-template-columns:1fr;}}";
  page += "</style></head><body><div class='wrap'>";
  page += "<div class='hero'>";
  page += "<div style='display:flex;justify-content:space-between;align-items:flex-start;flex-wrap:wrap;gap:12px;'>";
  page += "<div>";
  page += "<h1>Crypto Display</h1>";
  page += "<p>" + String(isUa ? "Налаштуйте Crypto Display як власного настільного помічника з крипто-трекером, віджетами, кольорами та розумними інструментами." : "Shape Crypto Display into your own desk companion with live crypto tracking, widgets, colors, and smart tools.") + "</p>";
  page += "</div>";
  page += "<div style='display:flex;align-items:center;gap:10px;'>";
  page += "<label class='label' style='margin:0;font-size:13px;'>" + String(isUa ? "Мова:" : "Language:") + "</label>";
  page += "<select name='lang' form='settingsForm' onchange='document.getElementById(\"settingsForm\").submit();' style='width:auto;padding:8px 12px;font-size:13px;border-radius:8px;'>";
  page += "<option value='ua'" + String(isUa ? " selected" : "") + ">🇺🇦 Українська</option>";
  page += "<option value='en'" + String(!isUa ? " selected" : "") + ">🇬🇧 English</option>";
  page += "</select>";
  page += "</div></div>";
  page += "<div style='margin-top:14px;display:flex;gap:12px;flex-wrap:wrap;align-items:center;'>";
  page += "<span class='ip'>IP: " + (WiFi.status() == WL_CONNECTED ? WiFi.localIP().toString() : WiFi.softAPIP().toString()) + "</span>";
  page += "<a href='/update' style='color:#38bdf8;text-decoration:none;font-size:13px;font-weight:600;padding:8px 14px;border-radius:999px;border:1px solid #0284c7;background:#0369a120;'>⚡ " + String(isUa ? "Оновлення прошивки (OTA) &rarr;" : "OTA Firmware Update &rarr;") + "</a>";
  page += "</div></div>";

  page += "<form id='settingsForm' method='POST' action='/save'>";
  page += "<div class='layout'><div class='stack'>";

  page += "<div class='panel' data-panel='theme'>";
  page += "<button type='button' class='panel-toggle' aria-expanded='true'><h2>" + String(isUa ? "Тема та кольори" : "Theme and color") + "</h2><span class='panel-chevron'>&#9662;</span></button>";
  page += "<div class='panel-body'>";
  page += "<p>" + String(isUa ? "Кольорова гама та візуальний стиль оформлення дисплея." : "Colors and visual style for the display.") + "</p>";
  page += "<div class='grid'>";

  page += "<div style='grid-column:1 / -1;' class='color-stack'>";

  page += "<div class='color-row'><div class='color-meta'><label class='label'>" + String(isUa ? "Акцент" : "Accent") + "</label><span class='color-value' id='accent-value'>";
  page += accent;
  page += "</span></div><div class='swatch-row'>";
  page += "<label class='swatch" + String(accent=="standard"?" active":"") + "' style='background:" + accentPreviewCss("standard") + ";'><input type='radio' name='accent' value='standard'" + String(accent=="standard"?" checked":"") + "></label>";
  page += "<label class='swatch" + String(accent=="ice"?" active":"") + "' style='background:" + accentPreviewCss("ice") + ";'><input type='radio' name='accent' value='ice'" + String(accent=="ice"?" checked":"") + "></label>";
  page += "<label class='swatch" + String(accent=="white"?" active":"") + "' style='background:" + accentPreviewCss("white") + ";'><input type='radio' name='accent' value='white'" + String(accent=="white"?" checked":"") + "></label>";
  page += "<label class='swatch" + String(accent=="cyan"?" active":"") + "' style='background:" + accentPreviewCss("cyan") + ";'><input type='radio' name='accent' value='cyan'" + String(accent=="cyan"?" checked":"") + "></label>";
  page += "<label class='swatch" + String(accent=="mint"?" active":"") + "' style='background:" + accentPreviewCss("mint") + ";'><input type='radio' name='accent' value='mint'" + String(accent=="mint"?" checked":"") + "></label>";
  page += "<label class='swatch" + String(accent=="green"?" active":"") + "' style='background:" + accentPreviewCss("green") + ";'><input type='radio' name='accent' value='green'" + String(accent=="green"?" checked":"") + "></label>";
  page += "<label class='swatch" + String(accent=="blue"?" active":"") + "' style='background:" + accentPreviewCss("blue") + ";'><input type='radio' name='accent' value='blue'" + String(accent=="blue"?" checked":"") + "></label>";
  page += "<label class='swatch" + String(accent=="purple"?" active":"") + "' style='background:" + accentPreviewCss("purple") + ";'><input type='radio' name='accent' value='purple'" + String(accent=="purple"?" checked":"") + "></label>";
  page += "<label class='swatch" + String(accent=="pink"?" active":"") + "' style='background:" + accentPreviewCss("pink") + ";'><input type='radio' name='accent' value='pink'" + String(accent=="pink"?" checked":"") + "></label>";
  page += "<label class='swatch" + String(accent=="orange"?" active":"") + "' style='background:" + accentPreviewCss("orange") + ";'><input type='radio' name='accent' value='orange'" + String(accent=="orange"?" checked":"") + "></label>";
  page += "<label class='swatch" + String(accent=="amber"?" active":"") + "' style='background:" + accentPreviewCss("amber") + ";'><input type='radio' name='accent' value='amber'" + String(accent=="amber"?" checked":"") + "></label>";
  page += "<label class='swatch" + String(accent=="red"?" active":"") + "' style='background:" + accentPreviewCss("red") + ";'><input type='radio' name='accent' value='red'" + String(accent=="red"?" checked":"") + "></label>";
  page += "</div></div>";

  page += "<div class='color-row'><div class='color-meta'><label class='label'>" + String(isUa ? "Текст" : "Text") + "</label><span class='color-value' id='text-value'>";
  page += txt;
  page += "</span></div><div class='swatch-row'>";
  page += "<label class='swatch" + String(txt=="standard"?" active":"") + "' style='background:" + accentPreviewCss("standard") + ";'><input type='radio' name='text' value='standard'" + String(txt=="standard"?" checked":"") + "></label>";
  page += "<label class='swatch" + String(txt=="ice"?" active":"") + "' style='background:" + accentPreviewCss("ice") + ";'><input type='radio' name='text' value='ice'" + String(txt=="ice"?" checked":"") + "></label>";
  page += "<label class='swatch" + String(txt=="white"?" active":"") + "' style='background:" + accentPreviewCss("white") + ";'><input type='radio' name='text' value='white'" + String(txt=="white"?" checked":"") + "></label>";
  page += "<label class='swatch" + String(txt=="cyan"?" active":"") + "' style='background:" + accentPreviewCss("cyan") + ";'><input type='radio' name='text' value='cyan'" + String(txt=="cyan"?" checked":"") + "></label>";
  page += "<label class='swatch" + String(txt=="mint"?" active":"") + "' style='background:" + accentPreviewCss("mint") + ";'><input type='radio' name='text' value='mint'" + String(txt=="mint"?" checked":"") + "></label>";
  page += "<label class='swatch" + String(txt=="green"?" active":"") + "' style='background:" + accentPreviewCss("green") + ";'><input type='radio' name='text' value='green'" + String(txt=="green"?" checked":"") + "></label>";
  page += "<label class='swatch" + String(txt=="blue"?" active":"") + "' style='background:" + accentPreviewCss("blue") + ";'><input type='radio' name='text' value='blue'" + String(txt=="blue"?" checked":"") + "></label>";
  page += "<label class='swatch" + String(txt=="purple"?" active":"") + "' style='background:" + accentPreviewCss("purple") + ";'><input type='radio' name='text' value='purple'" + String(txt=="purple"?" checked":"") + "></label>";
  page += "<label class='swatch" + String(txt=="pink"?" active":"") + "' style='background:" + accentPreviewCss("pink") + ";'><input type='radio' name='text' value='pink'" + String(txt=="pink"?" checked":"") + "></label>";
  page += "<label class='swatch" + String(txt=="orange"?" active":"") + "' style='background:" + accentPreviewCss("orange") + ";'><input type='radio' name='text' value='orange'" + String(txt=="orange"?" checked":"") + "></label>";
  page += "<label class='swatch" + String(txt=="amber"?" active":"") + "' style='background:" + accentPreviewCss("amber") + ";'><input type='radio' name='text' value='amber'" + String(txt=="amber"?" checked":"") + "></label>";
  page += "<label class='swatch" + String(txt=="red"?" active":"") + "' style='background:" + accentPreviewCss("red") + ";'><input type='radio' name='text' value='red'" + String(txt=="red"?" checked":"") + "></label>";
  page += "</div></div>";

  page += "<div class='color-row'><div class='color-meta'><label class='label'>" + String(isUa ? "Фон" : "Theme") + "</label><span class='color-value' id='bg-value'>";
  page += bg;
  page += "</span></div><div class='swatch-row'>";
  page += "<label class='swatch" + String(bg=="slate"?" active":"") + "' style='background:" + themePreviewCss("slate") + ";'><input type='radio' name='bg' value='slate'" + String(bg=="slate"?" checked":"") + "></label>";
  page += "<label class='swatch" + String(bg=="deep"?" active":"") + "' style='background:" + themePreviewCss("deep") + ";'><input type='radio' name='bg' value='deep'" + String(bg=="deep"?" checked":"") + "></label>";
  page += "<label class='swatch" + String(bg=="nordic"?" active":"") + "' style='background:" + themePreviewCss("nordic") + ";'><input type='radio' name='bg' value='nordic'" + String(bg=="nordic"?" checked":"") + "></label>";
  page += "<label class='swatch" + String(bg=="forest"?" active":"") + "' style='background:" + themePreviewCss("forest") + ";'><input type='radio' name='bg' value='forest'" + String(bg=="forest"?" checked":"") + "></label>";
  page += "<label class='swatch" + String(bg=="coffee"?" active":"") + "' style='background:" + themePreviewCss("coffee") + ";'><input type='radio' name='bg' value='coffee'" + String(bg=="coffee"?" checked":"") + "></label>";
  page += "<label class='swatch" + String(bg=="soft"?" active":"") + "' style='background:" + themePreviewCss("soft") + ";'><input type='radio' name='bg' value='soft'" + String(bg=="soft"?" checked":"") + "></label>";
  page += "<label class='swatch" + String(bg=="midnight"?" active":"") + "' style='background:" + themePreviewCss("midnight") + ";'><input type='radio' name='bg' value='midnight'" + String(bg=="midnight"?" checked":"") + "></label>";
  page += "<label class='swatch" + String(bg=="graphite"?" active":"") + "' style='background:" + themePreviewCss("graphite") + ";'><input type='radio' name='bg' value='graphite'" + String(bg=="graphite"?" checked":"") + "></label>";
  page += "<label class='swatch" + String(bg=="garnet"?" active":"") + "' style='background:" + themePreviewCss("garnet") + ";'><input type='radio' name='bg' value='garnet'" + String(bg=="garnet"?" checked":"") + "></label>";
  page += "<label class='swatch" + String(bg=="ochre"?" active":"") + "' style='background:" + themePreviewCss("ochre") + ";'><input type='radio' name='bg' value='ochre'" + String(bg=="ochre"?" checked":"") + "></label>";
  page += "</div></div>";

  page += "</div>";

  page += "</div></div></div>";

  page += "<div class='panel' data-panel='settings'>";
  page += "<button type='button' class='panel-toggle' aria-expanded='true'><h2>" + String(isUa ? "Параметри пристрою" : "Settings") + "</h2><span class='panel-chevron'>&#9662;</span></button>";
  page += "<div class='panel-body'>";
  page += "<p>" + String(isUa ? "Поведінка системи, регіон та таймери." : "Core behavior and timer setup.") + "</p>";
  page += "<div class='settings-block'>";
  page += "<span class='settings-title'>" + String(isUa ? "Загальні" : "General") + "</span>";
  page += "<div class='grid'>";
  page += "<div><label class='label'>" + String(isUa ? "Ім'я пристрою" : "Buddy nickname") + "</label><input name='nickname' maxlength='24' value='" + htmlEscape(nickname) + "'></div>";
  page += "<div><label class='label'>" + String(isUa ? "Авто-сон дисплея" : "Auto sleep interval") + "</label><select name='sleepMin'>";
  page += "<option value='0'"  + String(sleepIntervalMin==0?" selected":"")  + ">" + String(isUa ? "Ніколи" : "Never") + "</option>";
  page += "<option value='1'"  + String(sleepIntervalMin==1?" selected":"")  + ">" + String(isUa ? "1 хвилина" : "1 minute") + "</option>";
  page += "<option value='5'"  + String(sleepIntervalMin==5?" selected":"")  + ">" + String(isUa ? "5 хвилин" : "5 minutes") + "</option>";
  page += "<option value='10'" + String(sleepIntervalMin==10?" selected":"") + ">" + String(isUa ? "10 хвилин" : "10 minutes") + "</option>";
  page += "<option value='30'" + String(sleepIntervalMin==30?" selected":"") + ">" + String(isUa ? "30 хвилин" : "30 minutes") + "</option>";
  page += "<option value='60'" + String(sleepIntervalMin==60?" selected":"") + ">" + String(isUa ? "1 година" : "1 hour") + "</option>";
  page += "</select><div class='muted' style='margin-top:8px;'>" + String(isUa ? "Сон спочатку затемнює екран, а через 60 секунд повністю вимикає підсвітку." : "Sleep dims the screen first, then turns it fully off after 60 seconds.") + "</div></div>";
  page += "<div><label class='label'>" + String(isUa ? "Система вимірювання" : "Measurement system") + "</label><select name='units'>";
  page += "<option value='metric'"   + String(units=="metric"?" selected":"")   + ">" + String(isUa ? "Метрична: °C / мм" : "Celsius / mm") + "</option>";
  page += "<option value='imperial'" + String(units=="imperial"?" selected":"") + ">" + String(isUa ? "Імперська: °F / дюйми" : "Fahrenheit / inches") + "</option>";
  page += "</select></div>";
  page += "<div><label class='label'>" + String(isUa ? "Формат дати" : "Date format") + "</label><select name='region'>";
  page += "<option value='europe'" + String(region=="europe"?" selected":"") + ">" + String(isUa ? "Європейський: дд.мм.рррр" : "European: dd.mm.yyyy") + "</option>";
  page += "<option value='us'" + String(region=="us"?" selected":"") + ">" + String(isUa ? "Американський: мм/дд/рррр" : "US: mm/dd/yyyy") + "</option>";
  page += "</select></div>";
  page += "<div><label class='label'>" + String(isUa ? "Часовий пояс" : "Time zone") + "</label><select name='tz'>";
  appendTimezoneOptions(page, tz);
  page += "</select></div>";
  page += "</div>";
  page += "</div>";
  page += "<div class='settings-block'><span class='settings-title'>" + String(isUa ? "Таймери фокусування" : "Timer") + "</span><div class='settings-desc'>" + String(isUa ? "Шість швидких інтервалів для спливаючого меню таймера." : "Choose the six quick timers shown in the popup menu.") + "</div><div class='timer-slot-grid'>";
  for (int i = 0; i < 6; i++) {
    page += "<div class='timer-slot'><div class='timer-slot-head'>" + String(isUa ? "Слот " : "Slot ") + String(i + 1) + "</div><div class='timer-slot-input'><input type='number' min='1' max='180' name='timer" + String(i) + "' value='" + String(timerPresetMin[i]) + "'><span class='timer-unit'>" + String(isUa ? "хв" : "min") + "</span></div></div>";
  }
  page += "</div>";
  page += "<div style='margin-top:14px;'><span class='settings-title'>" + String(isUa ? "Поведінка сповіщень" : "Alert behavior") + "</span><label style='display:flex;align-items:center;gap:10px;color:#edf2f7;'><input type='checkbox' name='flashMode' value='1'" + String(flashMode ? " checked" : "") + " style='width:auto;'>" + String(isUa ? "Блимати екраном після завершення таймера" : "Flash screen when timer ends") + "</label></div></div>";
  
  page += "<div class='settings-block'><span class='settings-title'>" + String(isUa ? "Налаштування криптовалют" : "Crypto Page Settings") + "</span><div class='settings-desc'>" + String(isUa ? "Введіть 5 тікерів торгових пар Binance (наприклад, BTCUSDT, DOGEUSDT, SOLUSDT)." : "Enter 5 Binance ticker symbols (e.g., BTCUSDT, DOGEUSDT).") + "</div><div class='timer-slot-grid'>";
  for (int i = 0; i < 5; i++) {
    page += "<div class='timer-slot'><div class='timer-slot-head'>" + String(isUa ? "Токен " : "Token ") + String(i + 1) + "</div><div class='timer-slot-input'><input type='text' name='crypto" + String(i) + "' value='" + htmlEscape(cryptoSymbols[i]) + "'></div></div>";
  }
  page += "</div>";
  page += "<div style='margin-top:12px;'><label class='label'>" + String(isUa ? "Інтервал оновлення цін" : "Crypto Refresh Interval") + "</label><select name='crypto_interval'>";
  page += "<option value='30'" + String(cryptoIntervalSec==30?" selected":"") + ">" + String(isUa ? "30 секунд" : "30 seconds") + "</option>";
  page += "<option value='60'" + String(cryptoIntervalSec==60?" selected":"") + ">" + String(isUa ? "1 хвилина (рекомендовано)" : "1 minute (recommended)") + "</option>";
  page += "<option value='120'" + String(cryptoIntervalSec==120?" selected":"") + ">" + String(isUa ? "2 хвилини" : "2 minutes") + "</option>";
  page += "<option value='300'" + String(cryptoIntervalSec==300?" selected":"") + ">" + String(isUa ? "5 хвилин" : "5 minutes") + "</option>";
  page += "</select></div>";
  page += "</div>";
  page += "<div class='settings-block'><span class='settings-title'>" + String(isUa ? "Локація (погода)" : "Location") + "</span><div class='settings-desc'>" + String(isUa ? "Використовується для погоди та сходу/заходу сонця." : "Used for weather data and sun times.") + "</div><div class='grid-3'>";
  page += "<div><label class='label'>" + String(isUa ? "Назва міста" : "Location name") + "</label><input name='locname' value='" + htmlEscape(locationName) + "'></div>";
  page += "<div><label class='label'>" + String(isUa ? "Широта (Latitude)" : "Latitude") + "</label><input name='lat' value='" + String(LAT, 6) + "'></div>";
  page += "<div><label class='label'>" + String(isUa ? "Довгота (Longitude)" : "Longitude") + "</label><input name='lng' value='" + String(LNG, 6) + "'></div>";
  page += "</div><div class='footer-note'>" + String(isUa ? "Приклад Київ: широта 50.4501, довгота 30.5234." : "Example Berlin: latitude 52.5200, longitude 13.4050.") + "</div></div>";
  page += "</div></div>";

  page += "<div class='panel' data-panel='widgets'>";
  page += "<button type='button' class='panel-toggle' aria-expanded='true'><h2>" + String(isUa ? "Налаштування віджетів" : "Widget Customization") + "</h2><span class='panel-chevron'>&#9662;</span></button>";
  page += "<div class='panel-body'>";
  page += "<p>" + String(isUa ? "Оберіть, які віджети показувати у 4 слотах на головному екрані під годинником." : "Choose which widgets appear in the four Home slots below the clock card.") + "</p>";
  page += "<div class='grid'>";
  for (int i = 0; i < HOME_SLOT_COUNT; i++) {
    page += "<div><label class='label'>";
    page += homeSlotLabel(i, isUa);
    page += "</label><select name='homeSlot";
    page += String(i);
    page += "'>";
    appendHomeWidgetOptions(page, homeSlotKeys[i], isUa);
    page += "</select></div>";
  }
  page += "</div>";
  page += "</div></div>";

  page += "<div class='panel' data-panel='network'>";
  page += "<button type='button' class='panel-toggle' aria-expanded='true'><h2>" + String(isUa ? "Мережа та MQTT" : "Network & MQTT Settings") + "</h2><span class='panel-chevron'>&#9662;</span></button>";
  page += "<div class='panel-body'>";
  page += "<p>" + String(isUa ? "Керування параметрами Wi-Fi та зв'язком із брокером MQTT." : "Manage your WiFi and Home Assistant / Mosquitto MQTT broker credentials.") + "</p>";
  page += "<div class='settings-block'><span class='settings-title'>" + String(isUa ? "Параметри Wi-Fi" : "Wi-Fi Credentials") + "</span><div class='settings-desc'>" + String(isUa ? "Зміна мережі без перепрошивки пристрою." : "Update network credentials without reflashing firmware.") + "</div>";
  page += "<div class='grid'>";
  page += "<div><label class='label'>Wi-Fi SSID</label><input type='text' name='wifi_ssid' value='" + htmlEscape(wifiSSID) + "'></div>";
  page += "<div><label class='label'>" + String(isUa ? "Wi-Fi Пароль" : "Wi-Fi Password") + "</label><input type='password' name='wifi_pass' placeholder='" + String(isUa ? "Залиште пустим, щоб не змінювати" : "Leave blank to keep current") + "' value=''></div>";
  page += "</div></div>";
  page += "<div class='settings-block'><span class='settings-title'>MQTT Broker</span><div class='settings-desc'>" + String(isUa ? "Телеметрія для Home Assistant або Mosquitto." : "Optional telemetry reporting to Home Assistant or Mosquitto.") + "</div>";
  page += "<label style='display:flex;align-items:center;gap:10px;color:#edf2f7;margin-bottom:12px;'><input type='checkbox' name='mqtt_enabled' value='1'" + String(mqttEnabled ? " checked" : "") + " style='width:auto;'>" + String(isUa ? "Увімкнути MQTT" : "Enable MQTT") + "</label>";
  page += "<div class='grid-3'>";
  page += "<div><label class='label'>" + String(isUa ? "Сервер / IP" : "Server / Host") + "</label><input type='text' name='mqtt_server' value='" + htmlEscape(mqttServer) + "' placeholder='192.168.1.100'></div>";
  page += "<div><label class='label'>" + String(isUa ? "Порт" : "Port") + "</label><input type='number' name='mqtt_port' value='" + String(mqttPort) + "'></div>";
  page += "<div><label class='label'>" + String(isUa ? "Користувач" : "Username") + "</label><input type='text' name='mqtt_user' value='" + htmlEscape(mqttUser) + "'></div>";
  page += "</div>";
  page += "<div style='margin-top:10px;'><label class='label'>" + String(isUa ? "Пароль MQTT" : "Password") + "</label><input type='password' name='mqtt_pass' placeholder='" + String(isUa ? "Залиште пустим, щоб не змінювати" : "Leave blank to keep current") + "' value=''></div>";
  page += "</div></div></div>";

  page += "</div><div class='stack'>";

  page += "<button type='submit' class='submit-btn'>" + String(isUa ? "💾 Зберегти налаштування в Crypto Display" : "Save to Crypto Display") + "</button>";
  page += "</div></div></form>";
  page += "<script>";
  page += "var colorNames={accent:{standard:'Standard',ice:'Ice',white:'White',cyan:'Cyan',mint:'Mint',green:'Green',blue:'Blue',purple:'Purple',pink:'Pink',orange:'Orange',amber:'Amber',red:'Red'},text:{standard:'Standard',ice:'Ice',white:'White',cyan:'Cyan',mint:'Mint',green:'Green',blue:'Blue',purple:'Purple',pink:'Pink',orange:'Orange',amber:'Amber',red:'Red'},bg:{slate:'Slate',deep:'Deep black',nordic:'Nordic blue',forest:'Forest',coffee:'Coffee',soft:'Soft dark',midnight:'Midnight',graphite:'Graphite',garnet:'Garnet',ochre:'Ochre'}};";
  page += "var panelStorageKey='Crypto Display-panel-state-v1';";
  page += "document.querySelectorAll('.swatch input').forEach(function(input){";
  page += "input.addEventListener('change',function(){";
  page += "document.querySelectorAll('.swatch input[name=\"'+input.name+'\"]').forEach(function(peer){";
  page += "peer.closest('.swatch').classList.toggle('active', peer.checked);";
  page += "});";
  page += "var valueEl=document.getElementById(input.name+'-value');";
  page += "if(valueEl&&colorNames[input.name]&&colorNames[input.name][input.value]){valueEl.textContent=colorNames[input.name][input.value];}";
  page += "});";
  page += "});";
  page += "function readPanelState(){try{return JSON.parse(localStorage.getItem(panelStorageKey)||'{}');}catch(e){return {};}}";
  page += "function writePanelState(state){localStorage.setItem(panelStorageKey,JSON.stringify(state));}";
  page += "function applyPanelState(panel,collapsed){panel.classList.toggle('collapsed',collapsed);var btn=panel.querySelector('.panel-toggle');if(btn){btn.setAttribute('aria-expanded',collapsed?'false':'true');}}";
  page += "var savedPanelState=readPanelState();";
  page += "document.querySelectorAll('.panel[data-panel]').forEach(function(panel){";
  page += "var panelId=panel.getAttribute('data-panel');";
  page += "if(Object.prototype.hasOwnProperty.call(savedPanelState,panelId)){applyPanelState(panel,!!savedPanelState[panelId]);}";
  page += "});";
  page += "document.querySelectorAll('.panel-toggle').forEach(function(btn){";
  page += "btn.addEventListener('click',function(){";
  page += "var panel=btn.closest('.panel');";
  page += "var collapsed=!panel.classList.contains('collapsed');";
  page += "applyPanelState(panel,collapsed);";
  page += "var state=readPanelState();";
  page += "var panelId=panel.getAttribute('data-panel');";
  page += "if(panelId){state[panelId]=collapsed;writePanelState(state);}";
  page += "});";
  page += "});";
  page += "</script>";
  page += "</div></body></html>";

  server.send(200, "text/html; charset=utf-8", page);
}

void handleSave() {
  if (server.hasArg("lang")) {
    String l = server.arg("lang");
    if (l == "ua" || l == "en") {
      currentLang = l;
      prefs.putString("lang", currentLang);
    }
  }

  String newAccent = server.hasArg("accent") ? server.arg("accent") : "cyan";
  String newBg     = server.hasArg("bg") ? server.arg("bg") : "slate";
  String newText   = server.hasArg("text") ? server.arg("text") : "standard";
  String newUnits  = server.hasArg("units") ? server.arg("units") : "metric";
  String newRegion = server.hasArg("region") ? server.arg("region") : "europe";
  String newTz     = server.hasArg("tz") ? server.arg("tz") : timezoneKey;
  String newLoc    = server.hasArg("locname") ? server.arg("locname") : locationName;
  String newNickname = server.hasArg("nickname") ? server.arg("nickname") : buddyNickname;
  HomeWidgetType newHomeSlots[HOME_SLOT_COUNT];
  for (int i = 0; i < HOME_SLOT_COUNT; i++) {
    String key = String("homeSlot") + String(i);
    String currentKey = homeWidgetKey(homeWidgetSlots[i]);
    newHomeSlots[i] = homeWidgetFromKey(server.hasArg(key) ? server.arg(key) : currentKey);
  }

  float newLat = server.hasArg("lat") ? server.arg("lat").toFloat() : LAT;
  float newLng = server.hasArg("lng") ? server.arg("lng").toFloat() : LNG;

  newLoc.trim();
  newNickname.trim();

  if (newLoc.length() == 0) newLoc = "Unknown";
  if (newNickname.length() > 24) newNickname = newNickname.substring(0, 24);
  if (newUnits != "metric" && newUnits != "imperial") newUnits = "metric";
  if (newRegion != "europe" && newRegion != "us") newRegion = "europe";
  newTz = sanitizeTimezoneKey(newTz);

  int newSleepMin = server.hasArg("sleepMin") ? server.arg("sleepMin").toInt() : sleepIntervalMin;
  sleepIntervalMin = constrain(newSleepMin, 0, 120);
  bool newFlashMode = server.hasArg("flashMode");

  bool locationChanged =
    (fabsf(newLat - LAT) > 0.0001f) ||
    (fabsf(newLng - LNG) > 0.0001f) ||
    (newLoc != locationName);

  buddyNickname = newNickname;
  locationName = newLoc;
  LAT = newLat;
  LNG = newLng;
  unitKey = newUnits;
  regionFormatKey = newRegion;
  timezoneKey = newTz;
  flashModeEnabled = newFlashMode;
  for (int i = 0; i < HOME_SLOT_COUNT; i++) {
    homeWidgetSlots[i] = newHomeSlots[i];
  }

  for (int i = 0; i < 6; i++) {
    String key = String("timer") + String(i);
    int currentValue = timerPresetMin[i];
    int nextValue = server.hasArg(key) ? server.arg(key).toInt() : currentValue;
    timerPresetMin[i] = sanitizeTimerMinutes(nextValue);
  }

  for (int i = 0; i < 5; i++) {
    String key = String("crypto") + String(i);
    if (server.hasArg(key)) {
      String newSym = sanitizeCryptoSymbol(server.arg(key));
      cryptoSymbols[i] = newSym;
      prefs.putString(key.c_str(), newSym);
    }
  }

  // Invalidate crypto prices so they re-fetch fresh data for updated symbols
  for (int i = 0; i < 5; i++) {
    cryptoPrices[i] = NAN;
    cryptoChanges[i] = NAN;
  }
  lastCryptoFetch = 0;

  if (server.hasArg("crypto_interval")) {
    int ci = server.arg("crypto_interval").toInt();
    if (ci >= 15 && ci <= 3600) {
      cryptoIntervalSec = ci;
      prefs.putInt("crypto_int", cryptoIntervalSec);
    }
  }

  if (server.hasArg("wifi_ssid") && server.arg("wifi_ssid").length() > 0) {
    String newSSID = server.arg("wifi_ssid");
    newSSID.trim();
    wifiSSID = newSSID;
    prefs.putString("wifi_ssid", wifiSSID);
  }
  if (server.hasArg("wifi_pass") && server.arg("wifi_pass").length() > 0) {
    wifiPass = server.arg("wifi_pass");
    prefs.putString("wifi_pass", wifiPass);
  }

  bool newMqttEnabled = server.hasArg("mqtt_enabled");
  mqttEnabled = newMqttEnabled;
  prefs.putBool("mqtt_en", mqttEnabled);

  if (server.hasArg("mqtt_server")) {
    mqttServer = server.arg("mqtt_server");
    mqttServer.trim();
    prefs.putString("mqtt_srv", mqttServer);
  }
  if (server.hasArg("mqtt_port")) {
    int p = server.arg("mqtt_port").toInt();
    if (p > 0 && p < 65536) {
      mqttPort = p;
      prefs.putInt("mqtt_prt", mqttPort);
    }
  }
  if (server.hasArg("mqtt_user")) {
    mqttUser = server.arg("mqtt_user");
    mqttUser.trim();
    prefs.putString("mqtt_usr", mqttUser);
  }
  if (server.hasArg("mqtt_pass") && server.arg("mqtt_pass").length() > 0) {
    mqttPass = server.arg("mqtt_pass");
    prefs.putString("mqtt_pwd", mqttPass);
  }
  prefs.putString("accent", newAccent);
  prefs.putString("bg", newBg);
  prefs.putString("text", newText);
  prefs.putString("units", unitKey);
  prefs.putString("region", regionFormatKey);
  prefs.putString("tz", timezoneKey);
  prefs.putString("nickname", buddyNickname);
  prefs.putString("locname", locationName);
  prefs.putFloat("lat", LAT);
  prefs.putFloat("lng", LNG);
  prefs.putInt("sleepMin", sleepIntervalMin);
  prefs.putBool("flashMode", flashModeEnabled);
  for (int i = 0; i < HOME_SLOT_COUNT; i++) {
    String key = String("homeSlot") + String(i);
    prefs.putString(key.c_str(), homeWidgetKey(homeWidgetSlots[i]));
  }
  for (int i = 0; i < 6; i++) {
    String key = String("timer") + String(i);
    prefs.putInt(key.c_str(), timerPresetMin[i]);
  }

  applyThemeByKey(newAccent, newBg);
  applyTextColorByKey(newText);
  applyDeviceTimezoneByKey(timezoneKey);
  if (!sleepDimmed && !sleepOff) setBacklight(BL_FULL);

  pageDirty = true;
  dataDirty = true;

  cacheClock = "";
  cacheHomeEmpty1 = "";
  cacheHomeEmpty2 = "";
  cacheFocusTimer = "";
  cacheTimerMenu = "";
  cacheTimerDone = "";
  for (int i = 0; i < HOME_SLOT_COUNT; i++) {
    cacheHomeSlots[i] = "";
  }

  lastTempText = "";
  lastRainText = "";
  lastKpText = "";
  lastKpLevelText = "";
  lastWindText = "";
  lastWindDirText = "";
  lastNextSunLabel = "";
  lastNextSunTime = "";
  lastUptimeText = "";

  if (locationChanged) resetDataCaches();

  server.sendHeader("Location", "/");
  server.send(303);
}

void setupWebServer() {
  server.on("/", HTTP_GET, handleRoot);
  server.on("/save", HTTP_POST, handleSave);

  // OTA Firmware Update Endpoints
  server.on("/update", HTTP_GET, []() {
    String lang = prefs.getString("lang", "ua");
    bool isUa = (lang == "ua");
    server.sendHeader("Connection", "close");
    String updateHtml = F("<!doctype html><html><head><meta name='viewport' content='width=device-width,initial-scale=1'>");
    updateHtml += "<title>" + String(isUa ? "Crypto Display — Оновлення прошивки" : "Crypto Display - Firmware Update") + "</title>";
    updateHtml += F("<style>"
      "body{font-family:-apple-system,BlinkMacSystemFont,sans-serif;background:#0d1117;color:#c9d1d9;display:flex;align-items:center;justify-content:center;height:100vh;margin:0;}"
      ".card{background:#161b22;padding:32px;border-radius:12px;border:1px solid #30363d;max-width:420px;width:90%;text-align:center;box-shadow:0 8px 30px rgba(0,0,0,0.6);}"
      "h2{color:#58a6ff;margin-top:0;font-size:22px;}"
      "p{color:#8b949e;font-size:14px;line-height:1.5;}"
      "input[type=file]{margin:24px 0;width:100%;color:#c9d1d9;padding:10px;border:1px dashed #30363d;border-radius:6px;background:#0d1117;box-sizing:border-box;}"
      "input[type=submit]{background:#238636;color:#ffffff;border:none;padding:12px 20px;border-radius:6px;font-size:16px;cursor:pointer;width:100%;font-weight:600;}"
      "input[type=submit]:hover{background:#2ea043;}"
      ".back{display:inline-block;margin-top:18px;color:#58a6ff;text-decoration:none;font-size:14px;}"
      "</style></head><body>");
    updateHtml += "<div class='card'><h2>⚡ " + String(isUa ? "Бездротове OTA-оновлення" : "OTA Firmware Update") + "</h2>";
    updateHtml += "<p>" + String(isUa ? "Оберіть скомпільований файл <code>firmware.bin</code> для оновлення прошивки по повітрю без USB." : "Upload compiled <code>firmware.bin</code> to wirelessly update your Crypto Display without USB.") + "</p>";
    updateHtml += F("<form method='POST' action='/update' enctype='multipart/form-data'>"
      "<input type='file' name='update' accept='.bin' required>");
    updateHtml += "<input type='submit' value='" + String(isUa ? "Прошити пристрій зараз" : "Flash Firmware Now") + "'>";
    updateHtml += "</form><a href='/' class='back'>&larr; " + String(isUa ? "Назад до панелі керування" : "Back to Dashboard") + "</a></div></body></html>";
    server.send(200, "text/html; charset=utf-8", updateHtml);
  });

  server.on("/update", HTTP_POST, []() {
    server.sendHeader("Connection", "close");
    server.send(200, "text/plain", (Update.hasError()) ? "OTA FAILED" : "OTA SUCCESS! Rebooting Crypto Display...");
    delay(1000);
    ESP.restart();
  }, []() {
    HTTPUpload& upload = server.upload();
    if (upload.status == UPLOAD_FILE_START) {
      Serial.printf("OTA Update: %s\n", upload.filename.c_str());
      if (!Update.begin(UPDATE_SIZE_UNKNOWN)) {
        Update.printError(Serial);
      }
    } else if (upload.status == UPLOAD_FILE_WRITE) {
      if (Update.write(upload.buf, upload.currentSize) != upload.currentSize) {
        Update.printError(Serial);
      }
    } else if (upload.status == UPLOAD_FILE_END) {
      if (Update.end(true)) {
        Serial.printf("OTA Success: %u bytes\n", upload.totalSize);
      } else {
        Update.printError(Serial);
      }
    }
  });

  server.begin();
}

// =========================================================
// MQTT
// =========================================================
WiFiClient mqttWiFiClient;
PubSubClient mqttClient(mqttWiFiClient);
long lastMqttReconnectAttempt = 0;

boolean reconnectMQTT() {
  if (!mqttEnabled || mqttServer.length() == 0) return false;
  if (mqttClient.connect("CryptoDisplay", mqttUser.c_str(), mqttPass.c_str())) {
    mqttClient.publish("crypto_display/status", "online");
  }
  return mqttClient.connected();
}

// =========================================================
// SETUP / LOOP
// =========================================================
void waitForNtpTime() {
  time_t now = time(nullptr);
  unsigned long t0 = millis();
  while (now < 1700000000 && millis() - t0 < 10000) {
    delay(200);
    now = time(nullptr);
  }
}

void startSetupAp() {
  WiFi.disconnect();
  WiFi.mode(WIFI_AP);
  WiFi.softAP("CryptoDisplay-Setup");
  apModeActive = true;
  IPAddress apIP = WiFi.softAPIP();
  
  tft.fillScreen(COL_BG);
  drawTopBar("WiFi Setup Mode");
  tft.setTextDatum(MC_DATUM);
  tft.setTextColor(COL_TEXT, COL_BG);
  tft.drawString("WiFi Not Connected", SCREEN_W / 2, 70, 4);
  tft.drawString("Connect phone to:", SCREEN_W / 2, 120, 2);
  tft.setTextColor(COL_ACCENT, COL_BG);
  tft.drawString("CryptoDisplay-Setup", SCREEN_W / 2, 145, 4);
  tft.setTextColor(COL_TEXT, COL_BG);
  tft.drawString("Open browser at:", SCREEN_W / 2, 195, 2);
  tft.setTextColor(COL_GREEN, COL_BG);
  tft.drawString(apIP.toString(), SCREEN_W / 2, 225, 4);
  tft.setTextColor(COL_DIM, COL_BG);
  tft.drawString("Save WiFi in web settings", SCREEN_W / 2, 270, 2);
}

void beginWiFiConnect() {
  if (!wifiEnabled) {
    WiFi.disconnect(true, true);
    WiFi.mode(WIFI_OFF);
    wifiConnectInProgress = false;
    return;
  }

  WiFi.mode(WIFI_STA);
  WiFi.setSleep(false);
  WiFi.begin(wifiSSID.c_str(), wifiPass.c_str());
  wifiConnectInProgress = true;
  wifiConnectStartedMs = millis();
}

void connectWiFi(bool waitForConnection = true) {
  beginWiFiConnect();
  if (!waitForConnection) return;

  unsigned long start = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - start < 12000) {
    delay(200);
  }
  wifiConnectInProgress = false;

  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("WiFi connection failed. Starting Setup AP mode...");
    startSetupAp();
  }
}

void updateWiFiConnectionState() {
  if (!wifiEnabled || !wifiConnectInProgress) return;

  wl_status_t status = WiFi.status();
  if (status == WL_CONNECTED) {
    wifiConnectInProgress = false;
    apModeActive = false;
    ensureSunTimesForToday();
    ensureSun();
    ensureWeather();
    ensureKpIndex();
    ensureCrypto();
    dataDirty = true;
    pageDirty = true;
    return;
  }

  if (millis() - wifiConnectStartedMs >= WIFI_CONNECT_TIMEOUT_MS) {
    wifiConnectInProgress = false;
    pageDirty = true;
  }
}

void setWifiEnabled(bool enabled) {
  wifiEnabled = enabled;
  prefs.putBool("wifiEnabled", wifiEnabled);

  if (!wifiEnabled) {
    wifiConnectInProgress = false;
    WiFi.disconnect(true, true);
    WiFi.mode(WIFI_OFF);
  } else {
    beginWiFiConnect();
  }

  dataDirty = true;
  pageDirty = true;
}

void setup() {
  Serial.begin(115200);

  pinMode(BACKLIGHT_PIN, OUTPUT);
  analogWrite(BACKLIGHT_PIN, BL_FULL);
  pinMode(27, OUTPUT);
  digitalWrite(27, HIGH);

  loadStoredSettings();
  setBacklight(BL_FULL);
  lastInteractionMs = millis();

  delay(200);

  tft.init();
  tft.setRotation(ROT);
  tft.invertDisplay(INV);
  tft.setSwapBytes(true);

  tft.fillScreen(TFT_BLACK);
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.drawString("Booting Crypto Display...", 10, 10, 2);

  touchSPI.begin(T_SCK, T_MISO, T_MOSI);
  pinMode(TOUCH_CS, OUTPUT);
  digitalWrite(TOUCH_CS, HIGH);
  ts.begin(touchSPI);
  ts.setRotation(ROT);

  tft.drawString("Connecting WiFi...", 10, 34, 2);
  connectWiFi(true);

  if (WiFi.status() == WL_CONNECTED) {
    tft.drawString("Syncing time...", 10, 58, 2);
    configTzTime(timezonePosixByKey(timezoneKey),
                 "pool.ntp.org", "time.google.com", "time.cloudflare.com");
    waitForNtpTime();

    ensureSunTimesForToday();
    ensureSun();
    ensureKpIndex();
    ensureCrypto();

    if (mqttEnabled && mqttServer.length() > 0) {
      mqttClient.setServer(mqttServer.c_str(), mqttPort);
      mqttClient.setSocketTimeout(1);
    }
  }

  setupWebServer();

  if (!apModeActive) {
    pageDirty = true;
    dataDirty = true;

    drawCurrentPageFull();
    updateCurrentPageDynamic();
  }

  lastClockTick = millis();
  lastDataTick = millis();

  Serial.print("Crypto Display web: http://");
  Serial.println(WiFi.status() == WL_CONNECTED ? WiFi.localIP().toString() : WiFi.softAPIP().toString());
}

void loop() {
  server.handleClient();
  
  if (WiFi.status() == WL_CONNECTED && mqttEnabled && mqttServer.length() > 0) {
    if (!mqttClient.connected()) {
      long now = millis();
      if (now - lastMqttReconnectAttempt > 30000) {
        lastMqttReconnectAttempt = now;
        if (reconnectMQTT()) {
          lastMqttReconnectAttempt = 0;
        }
      }
    } else {
      mqttClient.loop();
    }
  }

  updateWiFiConnectionState();
  updateFocusTimerState();
  updateTimerDoneDialogState();
  handleAutoSleep();

  int tx = 0, ty = 0;
  if (touchNewPress(tx, ty)) {
    lastInteractionMs = millis();

    if (sleepOff) {
      if (manualDimMode) {
        sleepOff = false;
        sleepDimmed = true;
        setBacklight(BL_DIM);
        pageDirty = true;
      } else {
        wakeDisplay();
      }
      return;
    }

    if (handleTimerDoneDialogTouch(tx, ty)) {
      return;
    }

    if (tx >= SCREEN_W - 36 && ty <= TOPBAR_H) {
      toggleSleepMode();
      pageDirty = true;
    } else {
      if (sleepDimmed) {
        if (!manualDimMode) {
          wakeDisplay();
        } else {
          if (!handleHomeTouch(tx, ty)) {
            handleNavTouch(tx, ty);
          }
        }
      } else {
        if (!handleHomeTouch(tx, ty)) {
          handleNavTouch(tx, ty);
        }
      }
    }
  }

  if (!apModeActive) {
    if (millis() - lastDataTick >= DATA_TICK_MS) {
      lastDataTick = millis();
      ensureSunTimesForToday();
      ensureWeather();
      ensureKpIndex();
      ensureCrypto();
      ensureFuel();
    }

    if (pageDirty || lastDrawnPage != currentPage) {
      drawCurrentPageFull();
      updateCurrentPageDynamic();
      pageDirty = false;
      dataDirty = false;
    }

    if (millis() - lastClockTick >= CLOCK_TICK_MS) {
      lastClockTick = millis();
      updateCurrentPageDynamic();
      dataDirty = false;
    }
  }

  delay(10);
}
