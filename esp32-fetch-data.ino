// esp32-fetch-data
// Fetch a data file over WiFi and print it to a configurable display.
//
// Behavior is driven by config.json (LittleFS) with safe built-in defaults:
//   1. Low power     - the device wakes, fetches, shows the result, then
//                      powers down via deep sleep. A button press (or other
//                      configured action) wakes it again, acting like the
//                      power source was switched back on.
//   2. Offline cache - the last good reading is stored in NVS. When a fetch
//                      fails, that cached value is shown instead of an error.
//   3. Configuration - displays, timing, the trigger action, and the shutdown
//                      method are all set in config.json.
///////////////////////////////////////////////////////////////////////////////

#include <HTTPClient.h>
#include <WiFi.h>
#include <driver/rtc_io.h>
#include <esp_sleep.h>

#include "Config.h"
#include "Display.h"
#include "Storage.h"

namespace {

AppConfig g_config;
DisplayDriver* g_display = nullptr;

// Result of a fetch attempt: either fresh data or a reason it failed.
struct FetchResult {
  bool ok = false;
  String value;
};

String progressDots(uint8_t count) {
  String dots;
  for (uint8_t i = 0; i < count; ++i) {
    dots += '.';
  }
  return dots;
}

// Strip leading markdown hashes and surrounding whitespace from the payload.
String sanitizeData(const String& rawData) {
  String cleaned = rawData;
  if (!g_config.preserveNewlines) {
    cleaned.replace("\r", " ");
    cleaned.replace("\n", " ");
  }
  cleaned.trim();

  while (cleaned.startsWith("#")) {
    cleaned.remove(0, 1);
    cleaned.trim();
  }

  return cleaned.length() > 0 ? cleaned : "<no data>";
}

void showScreen(const String& line1, const String& line2) {
  if (g_display) {
    g_display->showLines(line1, line2);
  }
}

// Connects to WiFi, surfacing progress on the display. Returns false on timeout.
bool ensureWifi() {
  if (WiFi.status() == WL_CONNECTED) {
    return true;
  }

  WiFi.mode(WIFI_STA);
  WiFi.begin(g_config.wifiSsid.c_str(), g_config.wifiPassword.c_str());

  const unsigned long startedAt = millis();
  uint8_t dotCount = 0;

  while (WiFi.status() != WL_CONNECTED) {
    showScreen("Connecting WiFi", progressDots(dotCount));
    delay(250);
    dotCount = (dotCount + 1) % 4;

    if (millis() - startedAt > 20000UL) {
      Serial.println("WiFi connect timed out.");
      return false;
    }
  }

  Serial.print("Connected. IP: ");
  Serial.println(WiFi.localIP());
  showScreen("WiFi connected", "Fetching data");
  delay(500);
  return true;
}

// Performs the HTTP GET and returns the sanitized body, or a failure result.
FetchResult fetchData() {
  FetchResult result;

  if (WiFi.status() != WL_CONNECTED) {
    return result;  // ok == false
  }

  HTTPClient http;
  http.useHTTP10(true);
  http.setTimeout(g_config.httpTimeoutMs);

  if (!http.begin(g_config.dataUrl)) {
    Serial.println("HTTP begin failed.");
    return result;
  }

  const int httpCode = http.GET();
  if (httpCode <= 0) {
    Serial.print("HTTP GET failed: ");
    Serial.println(http.errorToString(httpCode));
    http.end();
    return result;
  }

  result.value = sanitizeData(http.getString());
  result.ok = true;
  http.end();
  return result;
}

// One full cycle: connect, fetch, display. On failure, fall back to the last
// cached reading with a brief connection-error notice (goal 2).
void runFetchCycle() {
  showScreen("Fetched Data", "Loading...");

  FetchResult result;
  if (ensureWifi()) {
    result = fetchData();
  }

  if (result.ok) {
    storage::saveLastData(result.value);
    Serial.print("Fetched data: ");
    Serial.println(result.value);
    showScreen("Fetched Data", result.value);
    return;
  }

  // Connection/fetch failure -> show notice, then the cached value.
  Serial.println("Fetch failed; using stored value.");
  showScreen("Connection Error", "last fetched value");
  delay(g_config.connectionErrorMs);

  String cached = storage::loadLastData();
  if (cached.isEmpty()) {
    cached = "<none>";
  }
  showScreen("Fetch memory:", cached);
}

// Configures the wake source for the configured action and enters deep sleep.
void enterDeepSleep() {
  if (g_display) {
    g_display->powerOff();
  }

  switch (g_config.actionType) {
    case ActionType::Button: {
      const int level = g_config.buttonActiveLow ? 0 : 1;
      if (g_config.buttonActiveLow) {
        rtc_gpio_pullup_en(static_cast<gpio_num_t>(g_config.buttonPin));
        rtc_gpio_pulldown_dis(static_cast<gpio_num_t>(g_config.buttonPin));
      } else {
        rtc_gpio_pulldown_en(static_cast<gpio_num_t>(g_config.buttonPin));
        rtc_gpio_pullup_dis(static_cast<gpio_num_t>(g_config.buttonPin));
      }
      esp_sleep_enable_ext0_wakeup(
          static_cast<gpio_num_t>(g_config.buttonPin), level);
      Serial.printf("Deep sleep: wake on button (GPIO %d).\n",
                    g_config.buttonPin);
      break;
    }
    case ActionType::Sensor: {
      esp_sleep_enable_ext0_wakeup(
          static_cast<gpio_num_t>(g_config.sensorPin), 1);
      Serial.printf("Deep sleep: wake on sensor (GPIO %d).\n",
                    g_config.sensorPin);
      break;
    }
    case ActionType::Timer: {
      esp_sleep_enable_timer_wakeup(
          static_cast<uint64_t>(g_config.timerIntervalMs) * 1000ULL);
      Serial.printf("Deep sleep: wake on timer (%lu ms).\n",
                    g_config.timerIntervalMs);
      break;
    }
    case ActionType::None: {
      esp_sleep_enable_timer_wakeup(
          static_cast<uint64_t>(g_config.timerIntervalMs) * 1000ULL);
      break;
    }
  }

  Serial.flush();
  esp_deep_sleep_start();
}

// "Always on" fallback loop used when shutdown method is "none": poll the
// button (and refresh on the timer interval) without ever sleeping.
void runPollingLoop() {
  static int lastButtonState = HIGH;
  static unsigned long lastRefreshAt = 0;

  if (g_config.actionType == ActionType::Button) {
    const int currentButtonState = digitalRead(g_config.buttonPin);
    const int pressedState = g_config.buttonActiveLow ? LOW : HIGH;
    const int releasedState = g_config.buttonActiveLow ? HIGH : LOW;

    if (lastButtonState == releasedState && currentButtonState == pressedState) {
      runFetchCycle();
    }
    lastButtonState = currentButtonState;
  }

  if (g_config.actionType == ActionType::Timer &&
      millis() - lastRefreshAt >= g_config.timerIntervalMs) {
    lastRefreshAt = millis();
    runFetchCycle();
  }

  delay(20);
}

}  // namespace

void setup() {
  Serial.begin(115200);
  delay(50);

  loadConfig(g_config);

  g_display = createDisplay(g_config);
  g_display->begin();

  // Configure the trigger pin for polling/level reads.
  if (g_config.actionType == ActionType::Button) {
    pinMode(g_config.buttonPin,
            g_config.buttonActiveLow ? INPUT_PULLUP : INPUT_PULLDOWN);
  } else if (g_config.actionType == ActionType::Sensor) {
    pinMode(g_config.sensorPin, INPUT);
  }

  const esp_sleep_wakeup_cause_t wakeCause = esp_sleep_get_wakeup_cause();
  Serial.printf("Boot (wake cause: %d).\n", static_cast<int>(wakeCause));

  // Sleep-based modes treat every boot/wake as "power on" and fetch immediately.
  // The always-on mode preserves the classic behavior: a button device idles
  // until pressed, while timer/none triggers fetch right away.
  const bool isAlwaysOn = g_config.shutdownMethod == ShutdownMethod::None;
  const bool shouldFetch =
      !isAlwaysOn || g_config.actionType != ActionType::Button;

  if (shouldFetch) {
    runFetchCycle();
    delay(g_config.shutdownDelayMs);
  } else {
    showScreen("Press button", "to get data");
  }

  if (g_config.shutdownMethod == ShutdownMethod::DeepSleep) {
    enterDeepSleep();  // does not return
  }
  // ShutdownMethod::None / LightSleep fall through to loop().
}

void loop() {
  if (g_config.shutdownMethod == ShutdownMethod::None) {
    runPollingLoop();
    return;
  }

  // LightSleep: keep the awake window short, then nap until the next action.
  if (g_config.shutdownMethod == ShutdownMethod::LightSleep) {
    if (g_config.actionType == ActionType::Timer ||
        g_config.actionType == ActionType::None) {
      esp_sleep_enable_timer_wakeup(
          static_cast<uint64_t>(g_config.timerIntervalMs) * 1000ULL);
    } else {
      const int level = g_config.buttonActiveLow ? 0 : 1;
      esp_sleep_enable_ext0_wakeup(
          static_cast<gpio_num_t>(g_config.actionType == ActionType::Sensor
                                      ? g_config.sensorPin
                                      : g_config.buttonPin),
          g_config.actionType == ActionType::Sensor ? 1 : level);
    }
    if (g_display) {
      g_display->setBacklight(false);
    }
    esp_light_sleep_start();
    if (g_display) {
      g_display->setBacklight(true);
    }
    runFetchCycle();
    delay(g_config.shutdownDelayMs);
  }
}
