/*  
 * WiFi Plant Sensor
 * Matt Clarke 
 * 2020
*/

#include <SPI.h>
#include <WiFi101.h>
#include <LowPower.h>

#include "arduino_secrets.h"

// Soil sensor
const int SOIL_INPUT = A0; // Analogue 0 -- sig
const int SOIL_POWER = 7; // Digital 7 -- power
const int SOIL_SAMPLES = 5;
const int SOIL_HIGH = 827;
const int SOIL_LOW = 145;

// WiFi handling
const String WIFI_SSID = SECRET_WIFI_SSID;
const String WIFI_PASS = SECRET_WIFI_PASS;
const int WIFI_TIMEOUT = 3000;
const IPAddress CLIENT_ADDR(SECRET_SERVER_ADDRESS);
WiFiClient client;
int request_state = 0;

// Battery monitoring
const float V_MAX = 405;
const float V_MID = 370;
const float V_MIN = 330;
const float V_WARN = 345;

// General
const int SLEEP_TIME = 1800; // 30mins

void setup() {
  Serial.begin(9600);   // open serial over USB
  
  // Configure WiFi library
  WiFi.setPins(8,2,A3,-1);
  WiFi.setTimeout(WIFI_TIMEOUT);

  // check for the presence of the shield:
  if (WiFi.status() == WL_NO_SHIELD) {
    Serial.println("WiFi not present");
    while (true); // Stop.
  }
  
  // Setup moisture sensor
  pinMode(SOIL_POWER, OUTPUT); //Set D7 as an OUTPUT
  digitalWrite(SOIL_POWER, LOW); //Set to LOW so no power is flowing through the sensor
}

void loop()  {  
  if (request_state == 0) {
      // Always end connection and sleep no matter what
      request_state = 1;
      
      // Startup connection
      if (!wireless_connect()) {
        Serial.println("Startup failed");
        return;
      } else {
        // Slight delay for any setup
        delay(100);
        Serial.println("Connected to WiFi");
      }

      // Get soil state
      int moisture = readSoilPercentage();
      if (moisture == 0) {
        // This reading is too low, probably not in soil
        // Just come back around in half the usual interval
        return;
      }
      
      // Make request
      if (!notify_remote(CLIENT_ADDR, 8080, moisture, getBattVoltage())) {
        Serial.println("Connection failed");
      }
  } else if (request_state == 1) {
      // End connection
      wireless_end();
      Serial.println("Done");

      // Sleep the device
      lp_delay(SLEEP_TIME);

      // Check charge status - don't want to totally drain the battery
      float battery = getBattVoltage();
      
      if (battery <= V_MIN) {
        request_state = -1;
      } else {
        // Reset back to start again
        request_state = 0;
      }
  } else if (request_state == -1) {
      // Enter a 'sleepy' state for low battery status
      while(true) {
        if (getBattVoltage() >= V_MID) {
          request_state = 0;
        } else {
          lp_delay(SLEEP_TIME);
        }
      }
  }
}

/**
 * Sleeps the device in low power mode
 * 
 * Delay must be a multiple of 8s
 */
void lp_delay(int s) {
  // Allow any pending serial writes to finish
  delay(100);
  
  unsigned int passed = 0;

  while (passed < s) {
    LowPower.powerDown(SLEEP_8S, ADC_OFF, BOD_OFF);
    passed += 8;
  }
}

int readSoilPercentage() {
  // Turn on power to sensor
  digitalWrite(SOIL_POWER, HIGH);
  delay(10);

  // Take readings
  int val = 0;
  for (int i = 0; i < SOIL_SAMPLES; i++) {
    val += analogRead(SOIL_INPUT);
    delay(10);
  }

  // Turn off power to sensor
  digitalWrite(SOIL_INPUT, LOW);
  
  // Generate average
  val = (val / SOIL_SAMPLES);

  // Convert to %
  int result = 0;
  if (val > SOIL_HIGH) {
    result = 100;
  } else if (val < SOIL_LOW) {
    result = 0;
  } else {
    result = map(val, SOIL_LOW, SOIL_HIGH, 0, 100);
  }

  return result;
}

int wireless_connect() {  
  WiFi.begin(WIFI_SSID, WIFI_PASS);
  await_status(WL_CONNECTED);

  return WiFi.status() == WL_CONNECTED;
}

void await_status(const int statusVal) {
  int start = millis();
  int timeout = 0;
  
  while (WiFi.status() != statusVal && timeout == 0) {
    // Slight wait for connection status change
    delay(100);

    // Ensure timeout not exceeded
    timeout = millis() - start > WIFI_TIMEOUT ? 1 : 0;
  }
}

void wireless_end() {
  WiFi.end();
  delay(200);

  // Note: calling status() after end() will re-initialise the WiFi module
}

int notify_remote(const IPAddress host, const int port, int soil, int battery) {  
  if (client.connect(host, port)) {
    send_characteristic(host, "CurrentRelativeHumidity", soil);
    client.stop();
    delay(100);
  } else {
    return 0;
  }

  if (client.connect(host, port)) {
    send_characteristic(host, "StatusLowBattery", battery < V_WARN ? 1 : 0);
    client.stop();
    delay(100);
  } else {
    return 0;
  }

  return 1;
}

/**
 * Sends the characteristic with the given value
 */
void send_characteristic(const IPAddress host, String characteristic, int value) {
  Serial.print("Sending ");
  Serial.println(characteristic);
  
  String data = "{\"characteristic\":\"" + characteristic + "\",\"value\":" + value + "}";

  client.println("POST /moisture-sensor HTTP/1.1");
  client.println("Content-Type: application/json");
  client.print("Content-Length: ");
  client.println(data.length());
  client.print("Host: ");
  for (int i = 0; i < 4; i++) {
    client.print(host[i]);
    if (i < 3) {
      client.print(".");
    }
  }

  client.println();
  client.println("Connection: keep-alive");
  client.println();

  client.println(data);
  client.flush();

  delay(500);
}

/////////////////////////////////////////////
// Battery voltage
/////////////////////////////////////////////

// Calculate the battery voltage
// http://forum.arduino.cc/index.php?topic=133907.0
int getBattVoltage(void) {
  const long InternalReferenceVoltage = 1100L;
  ADMUX = (0 << REFS1) | (1 << REFS0) | (0 << ADLAR) | (1 << MUX3) | (1 << MUX2) | (1 << MUX1) | (0 << MUX0);
  delay(10);
  
  ADCSRA |= _BV( ADSC );
  while ( ( (ADCSRA & (1 << ADSC)) != 0 ) );
  int result = (((InternalReferenceVoltage * 1024L) / ADC) + 5L) / 10L;
  if (result > 450) {
    // Battery state is charging
  }

  return result;
}
