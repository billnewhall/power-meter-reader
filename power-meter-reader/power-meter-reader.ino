/*
 * Power meter reader
 * 
 * Reads IR pulses from power meter.
 * Each pulse is 1 Watt-hour (Wh)
 *  
 * After programming using serial port, cycle power or
 * hit reset on NodeMCU or OTA programming might not work.
 *
 * Tested on:
 * https://www.amazon.com/gp/product/B01IK9GEQG/ref=ppx_yo_dt_b_search_asin_title?ie=UTF8&th=1
 * 
 * To program ESP8266 with Arduino IDE:
 *   
 * File > Preferences > Additional Manager URLs, add:
 * http://arduino.esp8266.com/stable/package_esp8266com_index.json
 *
 * Tools > Board Manager > ESP8266, install software for Arduino IDE
 *
 * For MQTT:
 * Install library pubsubclient
 * https://github.com/knolleary/pubsubclient
 * (not paho)
 *
 * W. Newhall 12/2019 (original)
*/

// For WiFi and MQTT
#include <ESP8266WiFi.h>
#include <PubSubClient.h>

// For OTA programming via web browser
#include <WiFiClient.h>
#include <ESP8266WebServer.h>
#include <ESP8266mDNS.h>
#include <ESP8266HTTPUpdateServer.h>

// For private information (WiFi password, SSID, etc.)
#include "secrets.h"

// User Defined
char *WIFI_SSID               = SECRETS_WIFI_SSID;        // WiFi network SSID (#define in secrets.h)
char *WIFI_PASSWORD           = SECRETS_WIFI_PASSWORD;    // WiFi network password (#define in secrets.h)
const char *DEVICE_HOST_NAME  = "ESP_POWER_METER";        // Device host name on network
IPAddress                     broker(192, 168, 1, 105);   // IP address of MQTT broker
const char *MQTT_CLIENT_ID    = "PowerMeter";             // ID of this MQTT client (must be unique)
const char *TOPIC_JSON        = "Device/PowerMeter/JSON"; // Topic to publish JSON to 

// Configuration
#define   IR_DET_PIN        D1    // Digital pin connected to IR detector comparator output
#define   LED_PIN           D0    // LED pin
#define   TIME_BTWN_MSG_MS  10000 // Time interval between sending MQTT messages
#define   MSG_STR_BUF_SIZE  512   // Char array buffer for the MQTT message to send

// Globals
unsigned long PulseCounter        = 0;
unsigned long LastPulseTime       = 0;
unsigned long NextToLastPulseTime = 0;
bool LEDstate = false;

// For OTA programming via web browser
ESP8266WebServer httpServer(80);      // Web server for OTA updates
ESP8266HTTPUpdateServer httpUpdater;  // OTA update server

// For MQTT
WiFiClient wclient;             // WiFi client object for MQTT
PubSubClient client(wclient);   // Setup MQTT client

// ISR error without following line (https://forum.arduino.cc/index.php?topic=616264.0)
// This puts ISR in RAM (avoids crashes)
void ICACHE_RAM_ATTR ISR_IR_Pulse_Detected();

// Main setup
void setup( ) {
  Serial.begin(115200);               // Start serial communication at 115200 baud
  pinMode(LED_PIN, OUTPUT);           // Configure the LED pin as an output
  digitalWrite( LED_PIN, false);      // Light up the LED
  setupWifi();                        // Connect to network
  setupOTA();                         // Set up over-the-air (OTA) progamming
  client.setServer(broker, 1883);     // Setup MQTT

  // Interrupt on edge of IR pulse
  attachInterrupt( digitalPinToInterrupt( IR_DET_PIN), ISR_IR_Pulse_Detected, FALLING);
  
  Serial.println("Completed setup.");
}

void loop() {
  char strMsg[MSG_STR_BUF_SIZE];
  static unsigned long NextTime = 0;
  unsigned long _PulseCounter = 0;
  float Power_W = 0.0;
  float energy_kWh = 0.0;
  String StringMsg;
  String jsonMsg;

  httpServer.handleClient();  // Handle OTA programming

  if (!client.connected())  // Reconnect if connection is lost
  {
    reconnect();
  }
  client.loop();

  // Send MQTT message every time interval (defined by TIME_BTWN_MSG_MS)
  if( NextTime <= millis( ) ) {
    NextTime = millis( )+ TIME_BTWN_MSG_MS;
    _PulseCounter = PulseCounter;
    Power_W = 1.0 / ( (float)(LastPulseTime - NextToLastPulseTime)/1000.0 ) * 3600.0;
    energy_kWh = (float)_PulseCounter/1000.0;

    // Publish JSON
    StringMsg = (String)"{" +
            "\"power_W\": "           + (String)Power_W       + ", " +
            "\"energy_kWh\": "         + (String)energy_kWh   + ", " +
            "\"lastPulseTime_ms\": "  + (String)LastPulseTime +
            "}";

    StringMsg.toCharArray( strMsg, MSG_STR_BUF_SIZE );
    client.publish( TOPIC_JSON, strMsg, true );  // true = retained message
    Serial.println( (String)TOPIC_JSON + ": " + StringMsg );
  }
}

void ISR_IR_Pulse_Detected( ) {
  PulseCounter++;
  NextToLastPulseTime = LastPulseTime;
  LastPulseTime = millis( );
  digitalWrite( LED_PIN, LEDstate );
  LEDstate = !LEDstate; 
}

/// Connect to WiFi network
void setupWifi() {
  Serial.print("\r\nConnecting to ");
  Serial.println(WIFI_SSID);
  WiFi.hostname(DEVICE_HOST_NAME);      // Name for this device on the network
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD); // Connect to network
//  while (WiFi.status() != WL_CONNECTED) { // Wait for connection
//    delay(500);
//    Serial.print(".");
//  }
  while (WiFi.waitForConnectResult() != WL_CONNECTED) {
    Serial.println("Connection Failed. Rebooting...");
    delay(5000);
    ESP.restart();
  }
  Serial.println();
  Serial.println("WiFi connected");
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());
}

// Setup OTA programming
void setupOTA( ) {
  MDNS.begin(DEVICE_HOST_NAME);       // Multicast DNS to reply to .local nomain names
  httpUpdater.setup(&httpServer);     
  httpServer.begin();
  MDNS.addService("http", "tcp", 80);
  Serial.printf("HTTPUpdateServer ready. Open http://%s.local/update in your browser.\n", DEVICE_HOST_NAME);
}

// Reconnect to MQTT client
void reconnect() {
  unsigned long nextTime = 0;
  
  // Loop until we're reconnected
  while (!client.connected()) {
    Serial.println("Connecting to MQTT broker.");
    // Attempt to connect
    if (client.connect(MQTT_CLIENT_ID)) {
      Serial.println("Connected.");
      Serial.print("Publishing to: ");
      Serial.println(TOPIC_JSON);
      Serial.println('\n');
    } 
    else {
      Serial.println("Waiting 5 sec to attempt reconnect to MQTT broker.");
      // Wait 5 seconds before retrying
      nextTime = millis() + 5000;   // Next time to try to connect
      while(millis() < nextTime) {
          httpServer.handleClient();  // Handle OTA programming
      }
    }
  }
}
