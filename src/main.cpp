// Import required libraries
#include "ESP8266WiFi.h"
#include <NTPClient.h>
#include <FastLED.h>
#include "Adafruit_MQTT.h"
#include "Adafruit_MQTT_Client.h"


/************************* WiFi Access Point *********************************/
const char* ssid = "BestWiFi";
const char* password = "REPLACE_ME";


/************************* Adafruit.io Setup *********************************/
#define AIO_SERVER      "io.adafruit.com"
#define AIO_SERVERPORT  1883
#define AIO_USERNAME    "carljh"
#define AIO_KEY         "REPLACE_ME"
const long utcOffsetInSeconds = 3600;


/************ Global State (you don't need to change this!) ******************/
// Create an ESP8266 WiFiClient class to connect to the MQTT server.
WiFiClient client;

// Setup the MQTT client class by passing in the WiFi client and MQTT server and login details.
Adafruit_MQTT_Client mqtt(&client, AIO_SERVER, AIO_SERVERPORT, AIO_USERNAME, AIO_USERNAME, AIO_KEY);


/****************************** Feeds ***************************************/
// Setup a feed called 'onoff' for subscribing to changes to the button
Adafruit_MQTT_Subscribe wakeupalarm = Adafruit_MQTT_Subscribe(&mqtt, AIO_USERNAME "/feeds/wakeup", MQTT_QOS_1);
Adafruit_MQTT_Subscribe permalight = Adafruit_MQTT_Subscribe(&mqtt, AIO_USERNAME "/feeds/bedroomlight", MQTT_QOS_1);


/************************* LED Setup *********************************/
#define NUM_LEDS 58
#define DATA_PIN 5
CRGB leds[NUM_LEDS];

bool alarmIsTriggered = false;
bool lightIsOn = false;
unsigned long alarmCooldown = 0;
const long gracePeriod = 1000 * 60 * 60;

CHSV sun( 80, 255, 0);

CRGB white(255,255,255);
CRGB black(0,0,0);


void setup(void)
{ 
  // Start Serial
  Serial.begin(115200);
  WiFi.begin(ssid, password);
  
  while (WiFi.waitForConnectResult() != WL_CONNECTED) {
    Serial.println("Connection Failed! Rebooting...");
    delay(5000);
    ESP.restart();
  }

    
  FastLED.addLeds<WS2812, DATA_PIN, RGB>(leds, NUM_LEDS);

  Serial.println("Ready");
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());

  wakeupalarm.setCallback(wakeupcallback);
  mqtt.subscribe(&wakeupalarm);

  permalight.setCallback(permalightcallback);
  mqtt.subscribe(&permalight);
 
  for(int i = 0; i < NUM_LEDS; i++) { leds[i] = black; }
  FastLED.show();

}

void loop() {

  MQTT_connect();

  mqtt.processPackets(10000);

  if(! mqtt.ping()) {
    mqtt.disconnect();
  }

  if (alarmIsTriggered) {
    increaseTransition(sun);
    for(int i = 0; i < NUM_LEDS; i++) { leds[i] = sun; }  
    FastLED.show();
  }
}

void increaseTransition(CHSV & sun) {
  if (sun.value < 255) {
    sun.v += 3;
  } else if ((sun.value >= 255) && (sun.saturation > 0)) {
    sun.saturation -=3;
  } else {
    
    if (millis() - alarmCooldown >= gracePeriod) {
     alarmIsTriggered = false;
     sun.value = 0;
    }
  }
}

void wakeupcallback(char *data, uint16_t len) {
  Serial.print("Hey we're in a wakeup callback!");
  alarmIsTriggered = true;
  alarmCooldown = millis();
}

void permalightcallback(char *data, uint16_t len) {
  Serial.print("Hey we're in a manual lightswitch callback!\n");
  lightIsOn = not(lightIsOn);
  
  if (lightIsOn) {
    for(int i = 0; i < NUM_LEDS; i++) { leds[i] = white; }
  }
  else {
    for(int i = 0; i < NUM_LEDS; i++) { leds[i] = black; }
  }
  FastLED.show();
}

void MQTT_connect() {
  int8_t ret;

  if (mqtt.connected()) {
    return;
  }

  Serial.print("Connecting to MQTT... ");

  uint8_t retries = 3;
  while ((ret = mqtt.connect()) != 0) { // connect will return 0 for connected
       Serial.println(mqtt.connectErrorString(ret));
       Serial.println("Retrying MQTT connection in 10 seconds...");
       mqtt.disconnect();
       delay(10000);  // wait 10 seconds
       retries--;
       if (retries == 0) {
         // basically die and wait for WDT to reset me
         while (1);
       }
  }
  Serial.println("MQTT Connected!");
}