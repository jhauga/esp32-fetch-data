// Display.h
// Display abstraction layer.
//
// The firmware targets character displays but should not be locked to one
// driver. `DisplayDriver` is the common interface; `LcdI2cDisplay` is the
// built-in implementation for the 16x2 I2C LCD used by the reference circuit.
// Unknown display types degrade gracefully to `SerialDisplay` so the program
// keeps running (and stays debuggable) instead of failing to boot.
#pragma once

#include <Arduino.h>
#include <LiquidCrystal_I2C.h>
#include <Wire.h>

#include "Config.h"

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
  // Truncate a single line to the display width, adding an ellipsis when the
  // content does not fit. Optionally collapses newlines into spaces.
  String fitLine(const String& data, bool preserveNewlines) const {
    String line = data;
    if (!preserveNewlines) {
      line.replace("\r", " ");
      line.replace("\n", " ");
    }
    line.trim();

    const uint8_t width = columns();
    if (line.length() <= width) {
      return line;
    }
    if (width <= 3) {
      return line.substring(0, width);
    }
    return line.substring(0, width - 3) + "...";
  }
};

// Built-in 16x2 (or configured size) I2C LCD driver.
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
inline DisplayDriver* createDisplay(const AppConfig& cfg) {
  String type = cfg.displayType;
  type.toLowerCase();

  if (type == "lcd_i2c") {
    return new LcdI2cDisplay(cfg);
  }

  // Unknown/unaccounted display: keep running via the serial fallback and let
  // the configured external handler take over if the project provides one.
  Serial.print("Unrecognized display type '");
  Serial.print(cfg.displayType);
  Serial.println("'; falling back to SerialDisplay.");
  return new SerialDisplay(cfg);
}
