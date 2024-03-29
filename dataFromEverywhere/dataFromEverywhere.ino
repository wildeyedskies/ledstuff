#include <ssl_client.h>
#include <FastLED.h>
//#include <Arduino.h>
//
#include <WiFiMulti.h>
#include <HTTPClient.h>
#include <WiFiClientSecure.h>
#include <WiFi.h>
#include "secrets.h"

FASTLED_USING_NAMESPACE

#if defined(FASTLED_VERSION) && (FASTLED_VERSION < 3001000)
#warning "Requires FastLED 3.1 or later; check github for latest code."
#endif

#define DATA_PIN    26 // PIN A0 is GPIO 26
#define LED_TYPE    WS2812B
#define COLOR_ORDER GRB
#define NUM_LEDS    10

// Set web server port number to 80
WiFiServer server(80);
WiFiClient client;

// Current time
unsigned long currentTime = millis();
// Previous time
unsigned long previousTime = 0; 
// Define timeout time in milliseconds (example: 2000ms = 2s)
const long timeoutTime = 2000;

// Variable to store the HTTP request
String header;

CRGB leds[NUM_LEDS];
CRGB rainbow[6] = {CRGB::Purple, CRGB::Blue, CRGB::Green, CRGB::Yellow, CRGB::Orange, CRGB::Red};
int i = 0;

#define BRIGHTNESS          50
#define FRAMES_PER_SECOND    6

WiFiMulti WiFiMulti;
// defined in secrets.h
char ssid[] = SECRET_SSID;   // your network SSID (name) 
char pass[] = SECRET_PASS;   // your network password

// Not sure if WiFiClientSecure checks the validity date of the certificate. 
// Setting clock just to be sure...
void setClock() {
  configTime(0, 0, "pool.ntp.org", "time.nist.gov");

  Serial.print(F("Waiting for NTP time sync: "));
  time_t nowSecs = time(nullptr);
  while (nowSecs < 8 * 3600 * 2) {
    delay(500);
    Serial.print(F("."));
    yield();
    nowSecs = time(nullptr);
  }

  Serial.println();
  struct tm timeinfo;
  gmtime_r(&nowSecs, &timeinfo);
  Serial.print(F("Current time: "));
  Serial.print(asctime(&timeinfo));
}

void fillLEDs(CRGB color) {
  for (int i = 0; i < NUM_LEDS; i++) {
    leds[i] = color;
  }
}

// put your setup code here, to run once:
void setup() {
  Serial.begin(115200);
  Serial.println();

  WiFi.mode(WIFI_STA);
  WiFiMulti.addAP(SECRET_SSID, SECRET_PASS);

  // tell FastLED about the LED strip configuration
  FastLED.addLeds<LED_TYPE,DATA_PIN,COLOR_ORDER>(leds, NUM_LEDS).setCorrection(TypicalLEDStrip);
  FastLED.setBrightness(BRIGHTNESS);
  delay(3000); // 3 second delay for recovery
  
  fillLEDs(CRGB::Red);
  FastLED.show();

//   wait for WiFi connection
  Serial.println("Waiting for WiFi to connect...");
  while ((WiFiMulti.run() != WL_CONNECTED)) {
    Serial.println(".");
  }
  Serial.println(" connected");
  leds[0] = CRGB::Orange;
  FastLED.show();

  setClock();
  leds[0] = CRGB::Green;
  FastLED.show();
  // start the web server  
  server.begin();
}

void loop() {
  // Cycle through rainbow
  cycleRainbow(i);
  i++;

  webServerLoop();
  webLoop();
  
  FastLED.show();
  delay(1000/FRAMES_PER_SECOND);
}

void webLoop(){
  WiFiClientSecure *client = new WiFiClientSecure;
  if(client) {
    client -> setCACert(rootCACertificate);

    {
      // Add a scoping block for HTTPClient https to make sure it is destroyed before WiFiClientSecure *client is 
      HTTPClient https;
  
      Serial.print("[HTTPS] begin...\n");
      if (https.begin(*client, "https://jigsaw.w3.org/HTTP/connection.html")) {  // HTTPS
        Serial.print("[HTTPS] GET...\n");
        // start connection and send HTTP header
        int httpCode = https.GET();
  
        // httpCode will be negative on error
        if (httpCode > 0) {
          // HTTP header has been send and Server response header has been handled
          Serial.printf("[HTTPS] GET... code: %d\n", httpCode);
  
          // file found at server
          if (httpCode == HTTP_CODE_OK || httpCode == HTTP_CODE_MOVED_PERMANENTLY) {
            String payload = https.getString();
            Serial.println(payload);
          }
        } else {
          Serial.printf("[HTTPS] GET... failed, error: %s\n", https.errorToString(httpCode).c_str());
        }
  
        https.end();
      } else {
        Serial.printf("[HTTPS] Unable to connect\n");
      }

      // End extra scoping block
    }
  
    delete client;
  } else {
    Serial.println("Unable to create client");
  }

  Serial.println();
  Serial.println("Waiting 1s before the next round...");
  delay(1000);
}

void cycleRainbow(int i) {
  fillLEDs(rainbow[i%6]);
}


void webServerLoop() {
  WiFiClient client = server.available();   // Listen for incoming clients

  if (client) { // If a new client connects,
    currentTime = millis();
    previousTime = currentTime;

    bool previousNewline = false;
    while (client.connected() && currentTime - previousTime <= timeoutTime) {  // loop while the client's connected
      currentTime = millis();
      if (client.available()) { // if there's bytes to read from the client,
        char c = client.read();
        header += c;
        if (c == '\n' && previousNewline == true) {
          // HTTP headers always start with a response code (e.g. HTTP/1.1 200 OK)
          // and a content-type so the client knows what's coming, then a blank line:
          client.println("HTTP/1.1 200 OK");
          client.println("Connection: close");
          client.println();
          client.println();

          if (header.indexOf("GET /on") >= 0) {
            leds[0] = CRGB::Purple;
            FastLED.show();
          } else if (header.indexOf("GET /off") >= 0) {
            leds[0] = CRGB::Black;
            FastLED.show();          
          }

          break;
        } else if (c == '\n') {
          previousNewline = true;
        }
      }
    }
    // Clear the header variable
    header = "";
    // Close the connection
    client.stop();
  }
}
