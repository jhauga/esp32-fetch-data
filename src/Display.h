// Display.h
// Display abstraction layer.
//
// The firmware targets character/graphic displays but should not be locked to
// one driver. `DisplayDriver` is the common interface; `LcdI2cDisplay` is the
// always-compiled implementation for the 16x2 (or 20x4) I2C LCD used by the
// reference circuit. Unknown display types degrade gracefully to
// `SerialDisplay` so the program keeps running (and stays debuggable) instead
// of failing to boot.
//
// Additional drivers (SSD1306 / SH1107 OLED, parallel LCD, ILI9341 TFT, MAX7219
// matrix) are gated behind build flags so the default firmware compiles with no
// extra libraries. Enable one by setting the matching `display.type` in
// config.json and defining its flag (see platformio.ini):
//
//   display.type        build flag             class
//   ------------------  ---------------------  -------------------------
//   lcd_i2c             (always built)         LcdI2cDisplay
//   oled_ssd1306        USE_SSD1306            Ssd1306Display
//   oled_sh1107         USE_SH1107             Sh1107Display
//   lcd                 USE_LCD_PARALLEL       LcdParallelDisplay
//   tft_ili9341         USE_ILI9341            Ili9341Display
//   tft_ili9341_touch   USE_ILI9341_TOUCH      Ili9341TouchDisplay
//   matrix_max7219      USE_MAX7219            Max7219Display
#pragma once

#include <Arduino.h>
#include <LiquidCrystal_I2C.h>
#include <Wire.h>

#include "Config.h"

namespace display_detail {

// Truncate a single line to `width`, adding an ellipsis when the content does
// not fit. Optionally collapses embedded newlines into spaces. Shared by every
// driver so text fitting stays identical across hardware.
inline String fitLine(const String& data, uint8_t width, bool preserveNewlines) {
  String line = data;
  if (!preserveNewlines) {
    line.replace("\r", " ");
    line.replace("\n", " ");
  }
  line.trim();

  if (line.length() <= width) {
    return line;
  }
  if (width <= 3) {
    return line.substring(0, width);
  }
  return line.substring(0, width - 3) + "...";
}

}  // namespace display_detail

// Common contract every display driver must satisfy.
class DisplayDriver {
 public:
  virtual ~DisplayDriver() = default;
  virtual bool begin() = 0;
  virtual void showLines(const String& line1, const String& line2) = 0;
  virtual void clear() = 0;
  virtual void setBacklight(bool on) = 0;
  virtual void powerOff() = 0;  // clear + backlight off ahead of sleep
  virtual uint8_t columns() const = 0;
  virtual uint8_t rows() const = 0;

 protected:
  // Fit a line to this display's width using the shared helper.
  String fitLine(const String& data, bool preserveNewlines) const {
    return display_detail::fitLine(data, columns(), preserveNewlines);
  }
};

// Built-in 16x2 / 20x4 I2C LCD driver. Covers both "LCD 16x2 (I2C)" and
// "LCD 20x4 (I2C)" -- the geometry is taken from config (columns/rows).
class LcdI2cDisplay : public DisplayDriver {
 public:
  explicit LcdI2cDisplay(const AppConfig& cfg)
      : cfg_(cfg),
        lcd_(cfg.lcdAddress, cfg.lcdColumns, cfg.lcdRows) {}

  bool begin() override {
    Wire.begin(cfg_.i2cSda, cfg_.i2cScl);
    lcd_.init();
    lcd_.backlight();
    lcd_.clear();
    return true;
  }

  void showLines(const String& line1, const String& line2) override {
    lcd_.backlight();
    writeRow(0, line1);
    if (cfg_.lcdRows > 1) {
      writeRow(1, line2);
    }
  }

  void clear() override { lcd_.clear(); }

  void setBacklight(bool on) override {
    if (on) {
      lcd_.backlight();
    } else {
      lcd_.noBacklight();
    }
  }

  void powerOff() override {
    lcd_.clear();
    lcd_.noBacklight();
  }

  uint8_t columns() const override { return cfg_.lcdColumns; }
  uint8_t rows() const override { return cfg_.lcdRows; }

 private:
  // Blank the row first so leftover characters never bleed into a short line.
  void writeRow(uint8_t row, const String& data) {
    lcd_.setCursor(0, row);
    for (uint8_t i = 0; i < cfg_.lcdColumns; ++i) {
      lcd_.print(' ');
    }
    lcd_.setCursor(0, row);
    lcd_.print(fitLine(data, cfg_.preserveNewlines));
  }

  const AppConfig& cfg_;
  LiquidCrystal_I2C lcd_;
};

// ---------------------------------------------------------------------------
// Optional driver: SSD1306 OLED (I2C). Renders the two lines with Adafruit_GFX.
// ---------------------------------------------------------------------------
#ifdef USE_SSD1306
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

class Ssd1306Display : public DisplayDriver {
 public:
  explicit Ssd1306Display(const AppConfig& cfg)
      : cfg_(cfg), oled_(kWidth, kHeight, &Wire, -1) {}

  bool begin() override {
    Wire.begin(cfg_.i2cSda, cfg_.i2cScl);
    if (!oled_.begin(SSD1306_SWITCHCAPVCC, cfg_.lcdAddress)) {
      Serial.println("SSD1306 init failed; check i2cAddress/wiring.");
      return false;
    }
    oled_.clearDisplay();
    oled_.setTextColor(SSD1306_WHITE);
    oled_.setTextSize(1);
    oled_.display();
    return true;
  }

  void showLines(const String& line1, const String& line2) override {
    oled_.clearDisplay();
    oled_.setTextSize(1);
    oled_.setTextColor(SSD1306_WHITE);
    oled_.setCursor(0, 0);
    oled_.print(fitLine(line1, cfg_.preserveNewlines));
    if (cfg_.lcdRows > 1) {
      oled_.setCursor(0, 16);
      oled_.print(fitLine(line2, cfg_.preserveNewlines));
    }
    oled_.display();
  }

  void clear() override {
    oled_.clearDisplay();
    oled_.display();
  }

  void setBacklight(bool on) override { oled_.dim(!on); }

  void powerOff() override {
    oled_.clearDisplay();
    oled_.display();
  }

  uint8_t columns() const override { return cfg_.lcdColumns; }
  uint8_t rows() const override { return cfg_.lcdRows; }

 private:
  static constexpr int16_t kWidth = 128;
  static constexpr int16_t kHeight = 64;
  const AppConfig& cfg_;
  Adafruit_SSD1306 oled_;
};
#endif  // USE_SSD1306

// ---------------------------------------------------------------------------
// Optional driver: Grove SH1107 OLED (I2C, 128x128). Adafruit_GFX text render.
// ---------------------------------------------------------------------------
#ifdef USE_SH1107
#include <Adafruit_GFX.h>
#include <Adafruit_SH110X.h>

class Sh1107Display : public DisplayDriver {
 public:
  explicit Sh1107Display(const AppConfig& cfg)
      : cfg_(cfg), oled_(kWidth, kHeight, &Wire) {}

  bool begin() override {
    Wire.begin(cfg_.i2cSda, cfg_.i2cScl);
    if (!oled_.begin(cfg_.lcdAddress, /*reset=*/true)) {
      Serial.println("SH1107 init failed; check i2cAddress/wiring.");
      return false;
    }
    oled_.setRotation(1);
    oled_.clearDisplay();
    oled_.setTextColor(SH110X_WHITE);
    oled_.setTextSize(1);
    oled_.display();
    return true;
  }

  void showLines(const String& line1, const String& line2) override {
    oled_.clearDisplay();
    oled_.setTextSize(1);
    oled_.setTextColor(SH110X_WHITE);
    oled_.setCursor(0, 0);
    oled_.print(fitLine(line1, cfg_.preserveNewlines));
    if (cfg_.lcdRows > 1) {
      oled_.setCursor(0, 16);
      oled_.print(fitLine(line2, cfg_.preserveNewlines));
    }
    oled_.display();
  }

  void clear() override {
    oled_.clearDisplay();
    oled_.display();
  }

  void setBacklight(bool) override {}  // OLED has no separate backlight rail

  void powerOff() override {
    oled_.clearDisplay();
    oled_.display();
  }

  uint8_t columns() const override { return cfg_.lcdColumns; }
  uint8_t rows() const override { return cfg_.lcdRows; }

 private:
  static constexpr int16_t kWidth = 128;
  static constexpr int16_t kHeight = 128;
  const AppConfig& cfg_;
  Adafruit_SH1107 oled_;
};
#endif  // USE_SH1107

// ---------------------------------------------------------------------------
// Optional driver: parallel character LCD (LiquidCrystal). Covers the non-I2C
// "LCD 16x2" and "LCD 20x4" -- geometry and wiring come from config.
// ---------------------------------------------------------------------------
#ifdef USE_LCD_PARALLEL
#include <LiquidCrystal.h>

class LcdParallelDisplay : public DisplayDriver {
 public:
  explicit LcdParallelDisplay(const AppConfig& cfg)
      : cfg_(cfg),
        lcd_(cfg.lcdRsPin, cfg.lcdEnPin, cfg.lcdD4Pin, cfg.lcdD5Pin,
             cfg.lcdD6Pin, cfg.lcdD7Pin) {}

  bool begin() override {
    lcd_.begin(cfg_.lcdColumns, cfg_.lcdRows);
    lcd_.clear();
    return true;
  }

  void showLines(const String& line1, const String& line2) override {
    writeRow(0, line1);
    if (cfg_.lcdRows > 1) {
      writeRow(1, line2);
    }
  }

  void clear() override { lcd_.clear(); }
  void setBacklight(bool) override {}  // backlight is wired to a fixed rail
  void powerOff() override { lcd_.clear(); }

  uint8_t columns() const override { return cfg_.lcdColumns; }
  uint8_t rows() const override { return cfg_.lcdRows; }

 private:
  void writeRow(uint8_t row, const String& data) {
    lcd_.setCursor(0, row);
    for (uint8_t i = 0; i < cfg_.lcdColumns; ++i) {
      lcd_.print(' ');
    }
    lcd_.setCursor(0, row);
    lcd_.print(fitLine(data, cfg_.preserveNewlines));
  }

  const AppConfig& cfg_;
  LiquidCrystal lcd_;
};
#endif  // USE_LCD_PARALLEL

// ---------------------------------------------------------------------------
// Optional driver: ILI9341 2.8" TFT-LCD (hardware SPI). The touch variant
// reuses this rendering; the touch overlay is an input device handled by the
// action subsystem, not the DisplayDriver, so the screen output is identical.
// ---------------------------------------------------------------------------
#if defined(USE_ILI9341) || defined(USE_ILI9341_TOUCH)
#include <Adafruit_GFX.h>
#include <Adafruit_ILI9341.h>
#include <SPI.h>

namespace display_detail {

inline void renderTftLines(Adafruit_ILI9341& tft, const AppConfig& cfg,
                           const String& line1, const String& line2) {
  tft.fillScreen(ILI9341_BLACK);
  tft.setTextColor(ILI9341_WHITE);
  tft.setTextSize(3);
  tft.setCursor(6, 20);
  tft.print(fitLine(line1, cfg.lcdColumns, cfg.preserveNewlines));
  if (cfg.lcdRows > 1) {
    tft.setCursor(6, 70);
    tft.print(fitLine(line2, cfg.lcdColumns, cfg.preserveNewlines));
  }
}

}  // namespace display_detail
#endif  // USE_ILI9341 || USE_ILI9341_TOUCH

#ifdef USE_ILI9341
class Ili9341Display : public DisplayDriver {
 public:
  explicit Ili9341Display(const AppConfig& cfg)
      : cfg_(cfg), tft_(cfg.spiCsPin, cfg.spiDcPin, cfg.spiRstPin) {}

  bool begin() override {
    tft_.begin();
    tft_.setRotation(cfg_.tftRotation);
    tft_.fillScreen(ILI9341_BLACK);
    return true;
  }

  void showLines(const String& line1, const String& line2) override {
    display_detail::renderTftLines(tft_, cfg_, line1, line2);
  }

  void clear() override { tft_.fillScreen(ILI9341_BLACK); }
  void setBacklight(bool) override {}  // LED pin is wired to a fixed rail
  void powerOff() override { tft_.fillScreen(ILI9341_BLACK); }

  uint8_t columns() const override { return cfg_.lcdColumns; }
  uint8_t rows() const override { return cfg_.lcdRows; }

 private:
  const AppConfig& cfg_;
  Adafruit_ILI9341 tft_;
};
#endif  // USE_ILI9341

#ifdef USE_ILI9341_TOUCH
// Same ILI9341 panel as Ili9341Display for output. Touch input is read by the
// action subsystem (out of scope for the DisplayDriver contract), so rendering
// is intentionally identical to the non-touch driver.
class Ili9341TouchDisplay : public DisplayDriver {
 public:
  explicit Ili9341TouchDisplay(const AppConfig& cfg)
      : cfg_(cfg), tft_(cfg.spiCsPin, cfg.spiDcPin, cfg.spiRstPin) {}

  bool begin() override {
    tft_.begin();
    tft_.setRotation(cfg_.tftRotation);
    tft_.fillScreen(ILI9341_BLACK);
    return true;
  }

  void showLines(const String& line1, const String& line2) override {
    display_detail::renderTftLines(tft_, cfg_, line1, line2);
  }

  void clear() override { tft_.fillScreen(ILI9341_BLACK); }
  void setBacklight(bool) override {}
  void powerOff() override { tft_.fillScreen(ILI9341_BLACK); }

  uint8_t columns() const override { return cfg_.lcdColumns; }
  uint8_t rows() const override { return cfg_.lcdRows; }

 private:
  const AppConfig& cfg_;
  Adafruit_ILI9341 tft_;
};
#endif  // USE_ILI9341_TOUCH

// ---------------------------------------------------------------------------
// Optional driver: MAX7219 LED dot-matrix chain (hardware SPI via MD_Parola).
// A short chain only fits a few characters, so the fetched value (line2) is
// preferred and fitted to the configured width.
// ---------------------------------------------------------------------------
#ifdef USE_MAX7219
#include <MD_MAX72XX.h>
#include <MD_Parola.h>
#include <SPI.h>

class Max7219Display : public DisplayDriver {
 public:
  explicit Max7219Display(const AppConfig& cfg)
      : cfg_(cfg),
        matrix_(MD_MAX72XX::FC16_HW, cfg.spiCsPin,
                cfg.matrixDevices > 0 ? cfg.matrixDevices : 1) {}

  bool begin() override {
    matrix_.begin();
    matrix_.setIntensity(4);
    matrix_.displayClear();
    return true;
  }

  void showLines(const String& line1, const String& line2) override {
    const String& source =
        (cfg_.lcdRows > 1 && line2.length() > 0) ? line2 : line1;
    // MD_Parola keeps the pointer it is handed, so the text must outlive the
    // call -- hold it in a member.
    lastText_ = fitLine(source, cfg_.preserveNewlines);
    matrix_.displayClear();
    matrix_.displayText(lastText_.c_str(), PA_LEFT, 0, 0, PA_PRINT,
                        PA_NO_EFFECT);
    // Static text shows on the first animate; scrolling would need repeated
    // displayAnimate() calls from the main loop.
    matrix_.displayAnimate();
  }

  void clear() override { matrix_.displayClear(); }
  void setBacklight(bool on) override { matrix_.setIntensity(on ? 4 : 0); }

  void powerOff() override {
    matrix_.displayClear();
    matrix_.displayShutdown(true);
  }

  uint8_t columns() const override { return cfg_.lcdColumns; }
  uint8_t rows() const override { return cfg_.lcdRows; }

 private:
  const AppConfig& cfg_;
  MD_Parola matrix_;
  String lastText_;
};
#endif  // USE_MAX7219

// Fallback driver for unaccounted display types. Mirrors output to the serial
// monitor so the device remains usable and observable.
class SerialDisplay : public DisplayDriver {
 public:
  explicit SerialDisplay(const AppConfig& cfg) : cfg_(cfg) {}

  bool begin() override {
    Serial.println("Using SerialDisplay fallback (no hardware driver).");
    if (!cfg_.externalDisplayHandler.isEmpty()) {
      Serial.print("Configured external display handler: ");
      Serial.println(cfg_.externalDisplayHandler);
    }
    return true;
  }

  void showLines(const String& line1, const String& line2) override {
    Serial.println("---- display ----");
    Serial.println(fitLine(line1, cfg_.preserveNewlines));
    if (cfg_.lcdRows > 1) {
      Serial.println(fitLine(line2, cfg_.preserveNewlines));
    }
    Serial.println("-----------------");
  }

  void clear() override {}
  void setBacklight(bool) override {}
  void powerOff() override {}
  uint8_t columns() const override { return cfg_.lcdColumns; }
  uint8_t rows() const override { return cfg_.lcdRows; }

 private:
  const AppConfig& cfg_;
};

// Factory: select a driver from config, falling back gracefully on unknowns.
// The optional drivers are only routable when their build flag is defined; with
// no flags set this resolves exactly as the original firmware did (lcd_i2c or
// the serial fallback).
inline DisplayDriver* createDisplay(const AppConfig& cfg) {
  String type = cfg.displayType;
  type.toLowerCase();

  if (type == "lcd_i2c") {
    return new LcdI2cDisplay(cfg);
  }

#ifdef USE_SSD1306
  if (type == "oled_ssd1306") {
    return new Ssd1306Display(cfg);
  }
#endif
#ifdef USE_SH1107
  if (type == "oled_sh1107") {
    return new Sh1107Display(cfg);
  }
#endif
#ifdef USE_LCD_PARALLEL
  if (type == "lcd") {
    return new LcdParallelDisplay(cfg);
  }
#endif
#ifdef USE_ILI9341
  if (type == "tft_ili9341") {
    return new Ili9341Display(cfg);
  }
#endif
#ifdef USE_ILI9341_TOUCH
  if (type == "tft_ili9341_touch") {
    return new Ili9341TouchDisplay(cfg);
  }
#endif
#ifdef USE_MAX7219
  if (type == "matrix_max7219") {
    return new Max7219Display(cfg);
  }
#endif

  // Unknown/unaccounted display (or a driver whose build flag is not set): keep
  // running via the serial fallback and let the configured external handler
  // take over if the project provides one.
  Serial.print("Unrecognized or unbuilt display type '");
  Serial.print(cfg.displayType);
  Serial.println("'; falling back to SerialDisplay.");
  return new SerialDisplay(cfg);
}
