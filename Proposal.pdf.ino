#include <DHT.h>

// --- pin config (might reorganize this later tbh) ---
#define DHTPIN        12
#define DHTTYPE       DHT11

#define LIGHT_PIN     A0
#define TDS_PIN       A5  

#define RELAY_HEATER  4
#define RELAY_FAN_A   3
#define RELAY_FAN_B   5
#define RELAY_LIGHT   2
#define RELAY_PUMP_A  9
#define RELAY_PUMP_B  11
#define RELAY_HUMID   6
#define RELAY_VALVE   7   

DHT dhtSensor(DHTPIN, DHTTYPE);

// thresholds (tuned a bit manually... might need adjustment later)
float targetTemp = 25.0; 
float humidityLow = 55.0;
float humidityHigh = 80.0;

float tdsLow   = 800.0;  
float tdsHigh  = 900.0;  

// state flags (I like keeping track manually)
bool heaterState = false;
bool fanState    = false;
bool lightState  = false;
bool humidState  = false;
bool valveState  = false; 

unsigned long lastSensorCheck = 0;
unsigned long lastUpload = 0; 

// sensor values
float currentTemp = 0;
float currentHum  = 0; 
int lightRaw      = 0;
float lightLux    = 0;
float tdsReading  = 0;

// wifi stuff
String writeKey = "WRITE API of things speak"; 
char wifiName[] = "wifi ssid";
char wifiPass[] = "wifi password";


// --- helper functions (yeah could generalize this but meh) ---
void toggleHeater(bool state) { 
  heaterState = state;
  digitalWrite(RELAY_HEATER, state ? LOW : HIGH); // inverted relay logic
}

void toggleFans(bool state) { 
  fanState = state;

  // NOTE: not sure why I didn't unify this with a loop...
  if (state) {
    digitalWrite(RELAY_FAN_A, HIGH);
    digitalWrite(RELAY_FAN_B, HIGH);
  } else {
    digitalWrite(RELAY_FAN_A, LOW);
    digitalWrite(RELAY_FAN_B, LOW);
  }
}

void toggleLight(bool state) { 
  lightState = state;
  digitalWrite(RELAY_LIGHT, state ? HIGH : LOW);
}

void toggleHumidifier(bool state) { 
  humidState = state;
  digitalWrite(RELAY_HUMID, state ? HIGH : LOW);
}

void toggleValve(bool state) { 
  valveState = state;
  digitalWrite(RELAY_VALVE, state ? HIGH : LOW);
} 


void setup() {

  // setup outputs
  pinMode(RELAY_HEATER, OUTPUT);
  pinMode(RELAY_FAN_A, OUTPUT);
  pinMode(RELAY_FAN_B, OUTPUT);
  pinMode(RELAY_LIGHT, OUTPUT);
  pinMode(RELAY_PUMP_A, OUTPUT);
  pinMode(RELAY_PUMP_B, OUTPUT);
  pinMode(RELAY_HUMID, OUTPUT);
  pinMode(RELAY_VALVE, OUTPUT); 

  // initial states
  toggleHeater(false);
  toggleFans(false);
  toggleLight(true);   // always ON for now
  toggleHumidifier(false);
  toggleValve(false); 

  digitalWrite(RELAY_PUMP_A, HIGH); 
  digitalWrite(RELAY_PUMP_B, LOW);  

  dhtSensor.begin();

  // debug LED
  pinMode(13, OUTPUT);
  digitalWrite(13, LOW); 
  
  Serial.begin(115200);
  delay(2000); // give esp time

  // connect wifi (super basic, no retries yet...)
  Serial.println("AT+CWMODE=1");
  delay(1000);

  Serial.print("AT+CWJAP=\"");
  Serial.print(wifiName);
  Serial.print("\",\"");
  Serial.print(wifiPass);
  Serial.println("\"");
  
  delay(10000); // yeah... kinda long but works for now

  digitalWrite(13, HIGH); // connected indicator
}


void loop() {

  unsigned long now = millis();

  // --- sensor reading block ---
  if (now - lastSensorCheck >= 3000) {
    lastSensorCheck = now;

    currentTemp = dhtSensor.readTemperature();
    currentHum  = dhtSensor.readHumidity();

    // basic sanity check (had weird NaNs before)
    if (!isnan(currentTemp) && !isnan(currentHum) && currentTemp >= 0 && currentTemp <= 60) {

      // temp calibration (not super accurate tbh)
      currentTemp = currentTemp - 1.8;

      // light calc
      lightRaw = analogRead(LIGHT_PIN);
      lightLux = (lightRaw / 1023.0) * 20000.0;

      if (lightLux > 20000.0) {
        lightLux = 20000.0; // clamp it
      }

      // --- TDS calculation ---
      int raw = analogRead(TDS_PIN);
      float v = raw * (5.0 / 1024.0);

      float coeff = 1.0 + 0.02 * (currentTemp - 25.0);
      float adjV = v / coeff;

      // copied formula from somewhere, should verify later...
      tdsReading = (133.42 * adjV * adjV * adjV 
                   - 255.86 * adjV * adjV 
                   + 857.39 * adjV) * 0.5;

      if (tdsReading < 0) tdsReading = 0; // just in case

      // --- control logic ---

      // heater logic
      if (currentTemp < targetTemp - 1.0) {
        toggleHeater(true);
      } else if (currentTemp >= targetTemp) {
        toggleHeater(false);
      }

      // fans + humidity (this got messy honestly)
      if (currentTemp > targetTemp + 1.0) {
        toggleFans(true);
        toggleHumidifier(true);
      } else {

        if (currentHum > humidityHigh) {
          toggleFans(true);
          toggleHumidifier(false);
        } else {

          toggleFans(false);

          if (currentHum < humidityLow) {
            toggleHumidifier(true);
          } else if (currentHum >= humidityLow + 5.0) {
            toggleHumidifier(false);
          }
        }
      }

      toggleLight(true); // forced ON always

      // pumps + valve
      digitalWrite(RELAY_PUMP_B, (tdsReading > tdsHigh) ? HIGH : LOW);

      if (tdsReading < tdsLow && tdsReading > 0) {
        toggleValve(true);
      } else {
        toggleValve(false);
      }
    }
  }

  // --- upload data to ThingSpeak ---
  if (now - lastUpload >= 20000) {
    lastUpload = now;

    if (!isnan(currentTemp) && currentTemp > 0) {

      Serial.println("AT+CIPSTART=\"TCP\",\"api.thingspeak.com\",80");
      delay(2000); 

      String req = "GET /update?api_key=" + writeKey;

      // building query step by step (easier to debug honestly)
      req += "&field1=" + String(currentTemp, 1);
      req += "&field2=" + String(currentHum, 1);
      req += "&field3=" + String(lightLux, 0);
      req += "&field4=" + String(tdsReading, 1);

      req += "&field5=" + String(heaterState ? 1 : 0);
      req += "&field6=" + String(fanState ? 1 : 0);
      req += "&field7=" + String(humidState ? 1 : 0);
      req += "&field8=" + String(valveState ? 1 : 0);

      req += " HTTP/1.1\r\n";
      req += "Host: api.thingspeak.com\r\n";
      req += "Connection: close\r\n\r\n";

      Serial.print("AT+CIPSEND=");
      Serial.println(req.length());

      delay(1000); // sometimes needed, not sure why

      Serial.print(req);

      // blink LED to indicate upload (quick hack)
      for (int i = 0; i < 2; i++) {
        digitalWrite(13, LOW);
        delay(200);
        digitalWrite(13, HIGH);
        delay(200);
      }
    }
  }
}