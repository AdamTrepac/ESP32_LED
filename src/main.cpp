#include <Arduino.h>
#include <FastLED.h>

namespace {

constexpr uint8_t kLedPin = 12;
constexpr uint16_t kLedCount = 60;
constexpr uint8_t kDefaultBrightness = 64;
constexpr uint8_t kDefaultSpeed = 120;
constexpr uint16_t kFramesPerSecond = 120;
constexpr EOrder kColorOrder = GRB;
constexpr uint16_t kMaxMilliAmps = 3500;

CRGB leds[kLedCount];

enum class Pattern : uint8_t {
  Solid = 0,
  Glow,
  Rainbow,
  Fire,
};

struct ControllerState {
  Pattern pattern = Pattern::Solid;
  CRGB color = CRGB::Blue;
  uint8_t brightness = kDefaultBrightness;
  uint8_t speed = kDefaultSpeed;
};

ControllerState state;

uint8_t heat[kLedCount];
uint32_t lastFrameMs = 0;
uint32_t lastSerialPollMs = 0;
uint8_t rainbowHue = 0;
float glowPhase = 0.0f;

const __FlashStringHelper* patternName(Pattern pattern) {
  switch (pattern) {
    case Pattern::Solid:
      return F("solid");
    case Pattern::Glow:
      return F("glow");
    case Pattern::Rainbow:
      return F("rainbow");
    case Pattern::Fire:
      return F("fire");
  }

  return F("unknown");
}

void printStatus() {
  Serial.print(F("Pattern="));
  Serial.print(patternName(state.pattern));
  Serial.print(F(" Brightness="));
  Serial.print(state.brightness);
  Serial.print(F(" Speed="));
  Serial.print(state.speed);
  Serial.print(F(" Color=("));
  Serial.print(state.color.r);
  Serial.print(F(","));
  Serial.print(state.color.g);
  Serial.print(F(","));
  Serial.print(state.color.b);
  Serial.println(F(")"));
}

void printHelp() {
  Serial.println(F("Commands:"));
  Serial.println(F("  status"));
  Serial.println(F("  pattern solid|glow|rainbow|fire"));
  Serial.println(F("  brightness 0-255"));
  Serial.println(F("  speed 1-255"));
  Serial.println(F("  color R G B"));
}

bool tryParsePattern(const String& name, Pattern& pattern) {
  if (name.equalsIgnoreCase("solid")) {
    pattern = Pattern::Solid;
    return true;
  }
  if (name.equalsIgnoreCase("glow")) {
    pattern = Pattern::Glow;
    return true;
  }
  if (name.equalsIgnoreCase("rainbow")) {
    pattern = Pattern::Rainbow;
    return true;
  }
  if (name.equalsIgnoreCase("fire")) {
    pattern = Pattern::Fire;
    return true;
  }
  return false;
}

uint8_t frameIntervalMs() {
  return map(state.speed, 1, 255, 40, 8);
}

void applySolid() {
  fill_solid(leds, kLedCount, state.color);
}

void applyGlow() {
  glowPhase += 0.01f + (static_cast<float>(state.speed) / 255.0f) * 0.08f;
  if (glowPhase > TWO_PI) {
    glowPhase -= TWO_PI;
  }

  const float wave = (sinf(glowPhase) + 1.0f) * 0.5f;
  const uint8_t level = 20 + static_cast<uint8_t>(wave * 235.0f);
  const CRGB scaled = state.color.nscale8_video(level);
  fill_solid(leds, kLedCount, scaled);
}

void applyRainbow() {
  rainbowHue += map(state.speed, 1, 255, 1, 6);
  fill_rainbow(leds, kLedCount, rainbowHue, 255 / max<uint16_t>(kLedCount, 1));

  for (uint16_t i = 0; i < kLedCount; ++i) {
    leds[i] = blend(leds[i], state.color, 96);
  }
}

void applyFire() {
  const uint8_t cooling = map(state.speed, 1, 255, 85, 30);
  const uint8_t sparking = map(state.speed, 1, 255, 40, 130);

  for (uint16_t i = 0; i < kLedCount; ++i) {
    heat[i] = qsub8(heat[i], random8(0, ((cooling * 10) / kLedCount) + 2));
  }

  for (int i = kLedCount - 1; i >= 2; --i) {
    heat[i] = (heat[i - 1] + heat[i - 2] + heat[i - 2]) / 3;
  }

  if (random8() < sparking) {
    const uint8_t y = random8(min<uint16_t>(7, kLedCount));
    heat[y] = qadd8(heat[y], random8(160, 255));
  }

  for (uint16_t i = 0; i < kLedCount; ++i) {
    const CRGB warm = HeatColor(heat[i]);
    leds[i] = blend(warm, state.color, 72);
  }
}

void renderPattern() {
  switch (state.pattern) {
    case Pattern::Solid:
      applySolid();
      break;
    case Pattern::Glow:
      applyGlow();
      break;
    case Pattern::Rainbow:
      applyRainbow();
      break;
    case Pattern::Fire:
      applyFire();
      break;
  }
}

void handleCommand(const String& rawCommand) {
  String command = rawCommand;
  command.trim();

  if (command.isEmpty()) {
    return;
  }

  Serial.print(F("> "));
  Serial.println(command);

  if (command.equalsIgnoreCase("help")) {
    printHelp();
    return;
  }

  if (command.equalsIgnoreCase("status")) {
    printStatus();
    return;
  }

  String keyword = command;
  const int firstSpace = command.indexOf(' ');
  String value = "";

  if (firstSpace >= 0) {
    keyword = command.substring(0, firstSpace);
    value = command.substring(firstSpace + 1);
    value.trim();
  }

  if (keyword.equalsIgnoreCase("pattern")) {
    Pattern pattern;
    if (!tryParsePattern(value, pattern)) {
      Serial.println(F("Unknown pattern. Use: solid, glow, rainbow, fire"));
      return;
    }
    state.pattern = pattern;
    printStatus();
    return;
  }

  if (keyword.equalsIgnoreCase("brightness")) {
    const long brightness = value.toInt();
    if (brightness < 0 || brightness > 255) {
      Serial.println(F("Brightness must be 0-255"));
      return;
    }
    state.brightness = static_cast<uint8_t>(brightness);
    FastLED.setBrightness(state.brightness);
    printStatus();
    return;
  }

  if (keyword.equalsIgnoreCase("speed")) {
    const long speed = value.toInt();
    if (speed < 1 || speed > 255) {
      Serial.println(F("Speed must be 1-255"));
      return;
    }
    state.speed = static_cast<uint8_t>(speed);
    printStatus();
    return;
  }

  if (keyword.equalsIgnoreCase("color")) {
    int r = 0;
    int g = 0;
    int b = 0;
    const int parsed = sscanf(value.c_str(), "%d %d %d", &r, &g, &b);
    if (parsed != 3 || r < 0 || r > 255 || g < 0 || g > 255 || b < 0 || b > 255) {
      Serial.println(F("Color must be: color R G B"));
      return;
    }

    state.color = CRGB(static_cast<uint8_t>(r), static_cast<uint8_t>(g), static_cast<uint8_t>(b));
    printStatus();
    return;
  }

  Serial.println(F("Unknown command. Type 'help'"));
}

void pollSerialCommands() {
  if (!Serial.available()) {
    return;
  }

  const String command = Serial.readStringUntil('\n');
  handleCommand(command);
}

}  // namespace

void setup() {
  Serial.begin(115200);
  delay(250);

  FastLED.addLeds<WS2812B, kLedPin, kColorOrder>(leds, kLedCount);
  FastLED.setBrightness(state.brightness);
  FastLED.setMaxPowerInVoltsAndMilliamps(5, kMaxMilliAmps);
  FastLED.clear(true);

  Serial.println(F("ESP32 LED controller starting"));
  printHelp();
  printStatus();
}

void loop() {
  const uint32_t now = millis();

  if (now - lastSerialPollMs >= 10) {
    lastSerialPollMs = now;
    pollSerialCommands();
  }

  if (now - lastFrameMs < frameIntervalMs()) {
    return;
  }

  lastFrameMs = now;
  renderPattern();
  FastLED.show();
  FastLED.delay(1000 / kFramesPerSecond);
}
