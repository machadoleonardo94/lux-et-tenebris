#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <Adafruit_NeoPixel.h>

#define LED_PIN 15
#define NUM_LEDS 40

Adafruit_NeoPixel strip(NUM_LEDS, LED_PIN, NEO_GRB + NEO_KHZ800);

void setup() {
  // Initialize serial communication for debugging
  Serial.begin(115200);
  delay(100);
  // Disable WiFi modem at boot to save power
  WiFi.mode(WIFI_OFF);
  WiFi.forceSleepBegin();
  Serial.println("WiFi modem disabled");
  
  Serial.println("\n\nESP8266 LED Control Started");
  Serial.println("===========================");
  
  strip.begin();
  strip.show(); // Initialize all pixels to 'off'
  Serial.println("LED initialized on GPIO15");
  
}

int position = 0;
int power=5;

void loop() {
  power = (sin(millis() / 1000.0) * 25 + 30); // Vary power between 5 and 55
    strip.setPixelColor(position, strip.Color(power, 0, 0)); // Red
    strip.show();
    position++;
    position %= NUM_LEDS;
    delay(25);
  }

