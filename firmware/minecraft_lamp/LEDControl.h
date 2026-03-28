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

  // Encender con color hex "#RRGGBB"
  void setColor(const char* hexColor) {
    uint32_t c = hexToColor(hexColor);
    strip.setPixelColor(0, c);
    strip.show();
    _isOn   = true;
    strlcpy(_currentColor, hexColor, sizeof(_currentColor));
  }

  void turnOff() {
    strip.setPixelColor(0, 0);
    strip.show();
    _isOn = false;
  }

  // Blink: alterna encendido/apagado por `seconds` segundos
  // Al terminar restaura el estado previo.
  // Usa millis() internamente → NO es bloqueante en sentido estricto,
  // pero llamar durante el blink pausa el handler de movimiento (acceptable).
  void blink(int seconds, const char* color) {
    bool  prevOn    = _isOn;
    char  prevColor[8];
    strlcpy(prevColor, _currentColor, sizeof(prevColor));

    unsigned long end   = millis() + (unsigned long)seconds * 1000UL;
    bool          state = true;
    while (millis() < end) {
      if (state) { setColor(color); } else { turnOff(); }
      state = !state;
      delay(300); // parpadeo cada 300ms (aceptable dentro del blink)
    }

    // Restaurar estado previo
    if (prevOn) { setColor(prevColor); }
    else        { turnOff(); }
    _isOn = prevOn;
    strlcpy(_currentColor, prevColor, sizeof(_currentColor));
  }

  bool  isOn()         const { return _isOn; }
  const char* getColor() const { return _currentColor; }

  // Aplica el estado desde la configuración remota
  void applyConfig(const char* powerState, const char* colorHex) {
    if (strcmp(powerState, "ON") == 0) { setColor(colorHex); }
    else                               { turnOff(); }
  }

private:
  Adafruit_NeoPixel strip = Adafruit_NeoPixel(NEOPIXEL_COUNT, NEOPIXEL_PIN, NEO_GRB + NEO_KHZ800);
  bool _isOn = false;
  char _currentColor[8] = "#FFFFFF";

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
