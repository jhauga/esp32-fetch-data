// esp32-fetch-data
// Fetch a data file over WiFi and print it to a configurable display.
//
// Behavior is driven by config.json (LittleFS) with safe built-in defaults:
//   1. Low power     - the display is dark while idle and the device can power
//                      down between fetches (deep/light sleep) to extend battery
//                      life. A button press (or other configured action) brings
//                      it back, acting like the power source was switched on.
//   2. Offline cache - the last good reading is stored in NVS. When a fetch
//                      fails, that cached value is shown instead of an error.
//   3. Configuration - displays, timing, the trigger action, and the shutdown
//                      method are all set in config.json.
//   4. Action-gated  - button/sensor triggers never fetch on their own. A cold
//                      power-on idles (display off) and waits; the fetch runs
//                      only when the action fires (e.g. the button is pressed).
//   5. Display off   - the display lights up only to show a fetch result. After
//                      display.displayTimeMs it turns off and shows nothing
//                      until the next action.
///////////////////////////////////////////////////////////////////////////////

#include <Arduino.h>
#include <HTTPClient.h>
#include <WiFi.h>
#include <driver/gpio.h>
#include <driver/rtc_io.h>
#include <esp_sleep.h>

#include "Arduino_ota.h"
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

// Turn the display fully off (clear + backlight off) so it draws no light and
// shows nothing. This is the idle state: dark until the next action.
void displayOff() {
  if (g_display) {
    g_display->powerOff();
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

  // Only a 2xx response carries real data. A negative code is a transport
  // error; a 4xx/5xx (e.g. 404) returns an error page we must NOT display as
  // data -- treat both as a failed fetch so the cached value is shown instead.
  if (httpCode < 200 || httpCode >= 300) {
    if (httpCode > 0) {
      Serial.printf("HTTP GET failed: status %d\n", httpCode);
    } else {
      Serial.print("HTTP GET failed: ");
      Serial.println(http.errorToString(httpCode));
    }
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

// An action fired: fetch and show the result, hold it on screen for the
// configured window, then turn the display off so it idles dark again.
//
// In the sleep modes this is also where the battery-friendly OTA opportunity
// runs: WiFi is still up from the fetch, so the device offers an update (a brief
// listen window or a proxy pull) before powering down, then sleeps as usual.
// When OTA is not built in, ota::run is a no-op and the cycle is unchanged.
void runActionCycle() {
  runFetchCycle();

  bool otaHeldDisplay = false;
  if (g_config.shutdownMethod != ShutdownMethod::None) {
    otaHeldDisplay = ota::run(g_config);
  }
  if (!otaHeldDisplay) {
    delay(g_config.displayTimeMs);
  }

  displayOff();
}

// True when the configured trigger waits on a GPIO (button/sensor). Timer/None
// wake on the RTC timer and need no pin.
bool actionUsesWakePin() {
  return g_config.actionType == ActionType::Button ||
         g_config.actionType == ActionType::Sensor;
}

// The GPIO the active trigger waits on (button or sensor pin).
int actionWakePin() {
  return g_config.actionType == ActionType::Sensor ? g_config.sensorPin
                                                   : g_config.buttonPin;
}

// Level on the wake pin that counts as "fired", for light-sleep GPIO wake. The
// sensor is treated as active-high; the button follows its configured polarity.
gpio_int_type_t actionWakeLevel() {
  if (g_config.actionType == ActionType::Sensor) {
    return GPIO_INTR_HIGH_LEVEL;
  }
  return g_config.buttonActiveLow ? GPIO_INTR_LOW_LEVEL : GPIO_INTR_HIGH_LEVEL;
}

// Configures the wake source for the configured action and enters deep sleep.
// The display is turned off first so the device sleeps showing nothing.
//
// Deep sleep can only wake from a GPIO via ext0, which is limited to RTC-capable
// pins. The caller guarantees the wake pin is RTC-capable before selecting deep
// sleep (see resolveShutdownMethod); a non-RTC pin is routed to light sleep
// instead, which can wake from any GPIO.
void enterDeepSleep() {
  displayOff();

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

// "Always on" loop used when shutdown method is "none": the MCU stays awake but
// the display is kept dark, lighting up only to show a fetch. Poll the button
// (and refresh on the timer interval), then blank the display once its window
// elapses.
void runPollingLoop() {
  static int lastButtonState = HIGH;
  static unsigned long lastRefreshAt = 0;
  static bool displayActive = false;
  static unsigned long shownAt = 0;

  if (g_config.actionType == ActionType::Button) {
    const int currentButtonState = digitalRead(g_config.buttonPin);
    const int pressedState = g_config.buttonActiveLow ? LOW : HIGH;
    const int releasedState = g_config.buttonActiveLow ? HIGH : LOW;

    if (lastButtonState == releasedState && currentButtonState == pressedState) {
      runFetchCycle();
      displayActive = true;
      shownAt = millis();
    }
    lastButtonState = currentButtonState;
  }

  if (g_config.actionType == ActionType::Timer &&
      millis() - lastRefreshAt >= g_config.timerIntervalMs) {
    lastRefreshAt = millis();
    runFetchCycle();
    displayActive = true;
    shownAt = millis();
  }

  // Once the display window elapses, turn the display off and leave it dark
  // until the next action.
  if (displayActive && millis() - shownAt >= g_config.displayTimeMs) {
    displayOff();
    displayActive = false;
  }

  delay(20);
}

// Returns the shutdown method the device can actually honor with the configured
// trigger pin. Deep sleep wakes from a button/sensor only through ext0, which is
// limited to RTC-capable GPIOs; if the configured pin is not RTC-capable, deep
// sleep would never wake on a press. In that case we transparently fall back to
// light sleep, which can wake from any GPIO -- so any valid pin keeps working
// (just without the deepest power state). RTC pins and timer/none triggers are
// returned unchanged.
ShutdownMethod resolveShutdownMethod() {
  if (g_config.shutdownMethod == ShutdownMethod::DeepSleep &&
      actionUsesWakePin() &&
      !rtc_gpio_is_valid_gpio(static_cast<gpio_num_t>(actionWakePin()))) {
    Serial.printf(
        "GPIO %d is not RTC-capable, so deep sleep cannot wake on it; "
        "using light sleep instead (wakes from any GPIO).\n",
        actionWakePin());
    return ShutdownMethod::LightSleep;
  }
  return g_config.shutdownMethod;
}

}  // namespace

void setup() {
  Serial.begin(115200);
  delay(50);

  loadConfig(g_config);

  // WiFi credential provisioning. The first USB flash ships a config.json with
  // the real network credentials, which are saved to NVS here. Later (OTA)
  // firmware can then leave config.json's credentials empty -- the image and any
  // redistributed config carry no secrets -- and the device falls back to the
  // credentials stored on-device. NVS survives OTA, which only rewrites the app
  // partition. See storage::saveWifiCredentials for the no-empty/no-churn rules.
  if (!g_config.wifiSsid.isEmpty()) {
    storage::saveWifiCredentials(g_config.wifiSsid, g_config.wifiPassword);
  } else {
    g_config.wifiSsid = storage::loadWifiSsid();
    g_config.wifiPassword = storage::loadWifiPassword();
    if (!g_config.wifiSsid.isEmpty()) {
      Serial.println("Using WiFi credentials provisioned in NVS.");
    }
  }

  g_display = createDisplay(g_config);
  g_display->begin();

  // Configure the trigger pin for polling/level reads.
  if (g_config.actionType == ActionType::Button) {
    pinMode(g_config.buttonPin,
            g_config.buttonActiveLow ? INPUT_PULLUP : INPUT_PULLDOWN);
  } else if (g_config.actionType == ActionType::Sensor) {
    pinMode(g_config.sensorPin, INPUT);
  }

  // Pick the shutdown method the configured trigger pin can actually support, so
  // a non-RTC button/sensor pin still wakes the device (via light sleep) instead
  // of sleeping forever. Everything below reads g_config.shutdownMethod.
  g_config.shutdownMethod = resolveShutdownMethod();

  const esp_sleep_wakeup_cause_t wakeCause = esp_sleep_get_wakeup_cause();
  Serial.printf("Boot (wake cause: %d).\n", static_cast<int>(wakeCause));

  // Button and sensor are user-initiated triggers: the program runs only when
  // that action fires, never on its own. On a cold power-on there is no wake
  // source, so we idle (display off) and wait for the action instead of
  // fetching. A wake caused by the action -- or a timer/none trigger -- fetches.
  const bool wokeFromAction = wakeCause == ESP_SLEEP_WAKEUP_EXT0 ||
                              wakeCause == ESP_SLEEP_WAKEUP_EXT1 ||
                              wakeCause == ESP_SLEEP_WAKEUP_TIMER;
  const bool actionIsUserTriggered =
      g_config.actionType == ActionType::Button ||
      g_config.actionType == ActionType::Sensor;
  const bool shouldFetch = !actionIsUserTriggered || wokeFromAction;

  if (shouldFetch) {
    runActionCycle();  // fetch, hold for the window, then turn the display off
  } else {
    Serial.println("Idle: waiting for action to fetch.");
    displayOff();  // nothing shown until an action occurs
  }

  if (g_config.shutdownMethod == ShutdownMethod::DeepSleep) {
    enterDeepSleep();  // does not return
  }
  // ShutdownMethod::None / LightSleep fall through to loop().

  // Always-on (`none`) mode keeps a continuous OTA listener so updates are
  // available any time, not just in the post-fetch window. The sleep modes use
  // that window instead (handled in runActionCycle) and skip this. OTA needs an
  // active WiFi link, so connect first, then idle dark while it listens.
  if (ota::enabled() && g_config.shutdownMethod == ShutdownMethod::None &&
      ensureWifi()) {
    ota::begin(g_config);
    displayOff();
  }
}

void loop() {
  ota::handle();  // services OTA when enabled; an inline no-op otherwise

  if (g_config.shutdownMethod == ShutdownMethod::None) {
    ota::poll(g_config);  // periodic auto-refresh on steady power (periodic mode)
    runPollingLoop();
    return;
  }

  // LightSleep: stay dark and napping until the action fires, then fetch, show
  // the result for the configured window, blank the display, and nap again.
  if (g_config.shutdownMethod == ShutdownMethod::LightSleep) {
    if (g_config.actionType == ActionType::Timer ||
        g_config.actionType == ActionType::None) {
      esp_sleep_enable_timer_wakeup(
          static_cast<uint64_t>(g_config.timerIntervalMs) * 1000ULL);
    } else {
      // Light sleep can wake from a GPIO level on any digital pin (not just the
      // RTC-capable ones ext0 requires), so the button/sensor works on whatever
      // valid pin config.json names.
      const int wakePin = actionWakePin();
      gpio_wakeup_enable(static_cast<gpio_num_t>(wakePin), actionWakeLevel());
      esp_sleep_enable_gpio_wakeup();
    }

    displayOff();  // dark while sleeping
    esp_light_sleep_start();
    runActionCycle();  // woke from the action: fetch, hold, then blank again
  }
}
