// ============================================================
// LEDControl.h — Control del NeoPixel WS2812B
// IoT Lámpara WiFi v10
// ============================================================
#pragma once
#include <Adafruit_NeoPixel.h>
#include "Config.h"

class LEDControl {
public:
  void begin() {
    strip.begin();
    strip.setBrightness(200);
    strip.show();
  }

  // Encender con color hex "#RRGGBB" y brillo 0-255 — todos los LEDs del anillo
  void setColor(const char* hexColor, uint8_t brightness = 200) {
    strip.setBrightness(brightness);
    uint32_t c = hexToColor(hexColor);
    for (int i = 0; i < strip.numPixels(); i++) {
      strip.setPixelColor(i, c);
    }
    strip.show();
    _isOn = true;
    _brightness = brightness;
    strlcpy(_currentColor, hexColor, sizeof(_currentColor));
  }

  void turnOff() {
    for (int i = 0; i < strip.numPixels(); i++) {
      strip.setPixelColor(i, 0);
    }
    strip.show();
    _isOn = false;
  }

  // Blink: alterna encendido/apagado por `seconds` segundos
  void blink(int seconds, const char* color) {
    bool     prevOn    = _isOn;
    uint8_t  prevBri   = _brightness;
    char     prevColor[8];
    strlcpy(prevColor, _currentColor, sizeof(prevColor));

    unsigned long end   = millis() + (unsigned long)seconds * 1000UL;
    bool          state = true;
    while (millis() < end) {
      if (state) { setColor(color, prevBri); } else { turnOff(); }
      state = !state;
      delay(300);
    }

    // Restaurar estado previo
    if (prevOn) { setColor(prevColor, prevBri); }
    else        { turnOff(); }
    _isOn = prevOn;
    _brightness = prevBri;
    strlcpy(_currentColor, prevColor, sizeof(_currentColor));
  }

  bool     isOn()          const { return _isOn; }
  const char* getColor()   const { return _currentColor; }
  uint8_t  getBrightness() const { return _brightness; }

  // Aplica el estado desde la configuración remota
  void applyConfig(const char* powerState, const char* colorHex, int brightness = 200) {
    if (strcmp(powerState, "ON") == 0) { setColor(colorHex, (uint8_t)brightness); }
    else                               { turnOff(); }
  }

private:
  Adafruit_NeoPixel strip = Adafruit_NeoPixel(NEOPIXEL_COUNT, NEOPIXEL_PIN, NEO_GRB + NEO_KHZ800);
  bool    _isOn      = false;
  uint8_t _brightness = 200;
  char    _currentColor[8] = "#FFFFFF";

  uint32_t hexToColor(const char* hex) {
    // Soporta "#RRGGBB"
    const char* h = (hex[0] == '#') ? hex + 1 : hex;
    long value = strtol(h, nullptr, 16);
    uint8_t r = (value >> 16) & 0xFF;
    uint8_t g = (value >>  8) & 0xFF;
    uint8_t b =  value        & 0xFF;
    return strip.Color(r, g, b);
  }
};
