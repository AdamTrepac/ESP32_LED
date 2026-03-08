#include <Arduino.h>
#include <FastLED.h>

// put function declarations here:
int myFunction(int, int);

#define LED_PIN     12
#define NUM_LEDS    30
#define BRIGHTNESS  30
#define LED_TYPE    WS2812B
#define COLOR_ORDER GRB

CRGB leds[NUM_LEDS];

void setup() {
    // put your setup code here, to run once:
    int result = myFunction(2, 3);
    Serial.begin(9600);
    Serial.println("Yo what's up dawgggsss");

    FastLED.addLeds<LED_TYPE, LED_PIN, COLOR_ORDER>(leds, NUM_LEDS);
    FastLED.setBrightness(BRIGHTNESS);

}

void loop() {
    // put your main code here, to run repeatedly:
    static uint8_t hue = 0;
    fill_rainbow(leds, NUM_LEDS, hue++, 7);
    FastLED.show();
    delay(50);

}

// put function definitions here:
int myFunction(int x, int y) {
    return x + y;
}