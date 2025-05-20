// ===================== Project: Health Monitoring IoT Device =====================
// Author: Md Adeeb Eqbal
// Description: Monitors temperature, heart rate, and vibration using ESP32, displays data on OLED, and uploads to Blynk.
// ================================================================================

// --------------------------- Blynk Configuration ---------------------------
#define BLYNK_TEMPLATE_ID      "TMPL3qCXhZ9ma"
#define BLYNK_TEMPLATE_NAME    "Quickstart Device"
#define BLYNK_AUTH_TOKEN       "dXmFHwIz21Yfak1gWPZOcIo3x3o_B66J"

#include <WiFi.h>
#include <WiFiClient.h>
#include <BlynkSimpleEsp32.h>

// --------------------------- Sensor Libraries ---------------------------
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <Adafruit_MLX90614.h>
#include "MAX30105.h"
#include "heartRate.h"

// --------------------------- Display Configuration ---------------------------
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);

// --------------------------- Sensor Configuration ---------------------------
Adafruit_MLX90614 mlx = Adafruit_MLX90614();
MAX30105 particleSensor;

#define SW420_PIN 4 // Vibration sensor pin

// --------------------------- WiFi Credentials ---------------------------
char ssid[] = "Pie";
char pass[] = "1234567890";

// --------------------------- Blynk Timer ---------------------------
BlynkTimer timer;

// --------------------------- Heart Rate Variables ---------------------------
const byte RATE_SIZE = 4; // For averaging heart rate
byte rates[RATE_SIZE];
byte rateSpot = 0;
long lastBeat = 0;
double beatsPerMinute;
int beatAvg;

// --------------------------- Temperature Variables ---------------------------
double new_emissivity = 0.98;
float ambient_temp = 0.0;
float object_temp = 0.0;

// --------------------------- Vibration Variables ---------------------------
int vibrationState = 0;
int vibrationCount = 0;

// --------------------------- Miscellaneous ---------------------------
long irValue;
long print_time = 0;

// ================================================================================
//                              Function Definitions
// ================================================================================

// --------------------------- Display Initialization ---------------------------
void initDisplay() {
  display.begin(SSD1306_SWITCHCAPVCC, 0x3C);
  display.clearDisplay();
  display.setTextSize(2);
  display.setTextColor(WHITE);
  display.setCursor(0, 0);
  display.println("Welcome");
  display.println("Initializing...");
  display.display();
  delay(2000);
  display.clearDisplay();
  display.setTextSize(1);
}

// --------------------------- MLX90614 Initialization ---------------------------
void initMLX() {
  if (!mlx.begin()) {
    Serial.println("Error connecting to MLX sensor. Check wiring.");
    display.setTextSize(1);
    display.setCursor(0, 17);
    display.println("Error: MLX sensor");
    display.display();
    while (1);
  }
}

// --------------------------- MAX30105 Initialization ---------------------------
void initMAX30105() {
  if (!particleSensor.begin(Wire, I2C_SPEED_FAST)) {
    Serial.println("MAX30105 not found. Check wiring/power.");
    while (1);
  }
  Serial.println("Place your index finger on the sensor with steady pressure.");
  particleSensor.setup();
  particleSensor.setPulseAmplitudeRed(0x08);
  particleSensor.setPulseAmplitudeGreen(0);
}

// --------------------------- Display Data ---------------------------
void showDataOnDisplay() {
  display.clearDisplay();
  if (irValue < 50000) {
    display.setCursor(0, 0);
    display.println("No finger?");
  } else {
    display.setTextSize(1);
    display.setCursor(0, 0);
    display.print("Body Temp: ");
    display.print(object_temp, 1);
    display.println(" C");
    display.setCursor(0, 20);
    display.print("Heart Rate: ");
    display.print(beatAvg);
    display.println(" BPM");
    // Optionally display vibration state
    // display.setCursor(0, 40);
    // display.print("Vibration: ");
    // display.println(vibrationState ? "Yes" : "No");
  }
  display.display();
}

// --------------------------- Serial Debug Output ---------------------------
void printDebugInfo() {
  Serial.print("Vibration: "); Serial.println(vibrationState);
  Serial.print("Ambient Temp: "); Serial.print(ambient_temp); Serial.print(" F\t");
  Serial.print("Body Temp: "); Serial.print(object_temp); Serial.println(" C");
  Serial.print("IR: "); Serial.print(irValue);
  Serial.print(", BPM: "); Serial.print(beatsPerMinute);
  Serial.print(", Avg BPM: "); Serial.println(beatAvg);
}

// --------------------------- Heart Rate Calculation ---------------------------
void updateHeartRate() {
  irValue = particleSensor.getIR();
  if (checkForBeat(irValue)) {
    long delta = millis() - lastBeat;
    lastBeat = millis();
    beatsPerMinute = 60 / (delta / 1000.0);

    if (beatsPerMinute < 255 && beatsPerMinute > 20) {
      rates[rateSpot++] = (byte)beatsPerMinute;
      rateSpot %= RATE_SIZE;
      beatAvg = 0;
      for (byte x = 0; x < RATE_SIZE; x++) beatAvg += rates[x];
      beatAvg /= RATE_SIZE;
    }
  }
}

// --------------------------- Temperature Reading ---------------------------
void updateTemperature() {
  ambient_temp = mlx.readAmbientTempF();
  object_temp = mlx.readObjectTempC();
}

// --------------------------- Blynk Virtual Pin Handler ---------------------------
BLYNK_WRITE(V0) {
  int value = param.asInt();
  Serial.print("V0 value: ");
  Serial.println(value);
}

// --------------------------- Blynk Timer Event ---------------------------
void blynkTimerEvent() {
  Blynk.virtualWrite(V1, vibrationCount);
  Blynk.virtualWrite(V2, beatAvg);
  Blynk.virtualWrite(V3, ambient_temp);
  Blynk.virtualWrite(V4, object_temp);
}

// --------------------------- Blynk Initialization ---------------------------
void initBlynk() {
  Blynk.begin(BLYNK_AUTH_TOKEN, ssid, pass);
  timer.setInterval(1100L, blynkTimerEvent);
  timer.setInterval(300L, updateTemperature);
}

// ================================================================================
//                                   Setup
// ================================================================================

void setup() {
  Serial.begin(115200);
  while (!Serial);

  pinMode(SW420_PIN, INPUT);

  initBlynk();
  Serial.println("Initializing Display...");
  initDisplay();

  Serial.println("Initializing MLX90614...");
  initMLX();

  Serial.println("Initializing MAX30105...");
  initMAX30105();

  Serial.print("Emissivity: ");
  Serial.println(mlx.readEmissivity());
  mlx.writeEmissivity(0.95);

  Serial.println("=========================================");
}

// ================================================================================
//                                   Main Loop
// ================================================================================

void loop() {
  vibrationState = digitalRead(SW420_PIN);

  // Vibration detection logic
  if (vibrationState == HIGH) {
    vibrationCount++;
    if (vibrationCount > 5) {
      Blynk.logEvent("vibration");
      Serial.println("Alert: Vibration detected!");
      display.clearDisplay();
      display.println("Alert: Vibration!");
      display.display();
      vibrationCount = 0;
    }
  } else {
    vibrationCount = 0;
  }

  updateTemperature();
  updateHeartRate();

  // Display and serial print every second
  if (millis() - print_time > 1000) {
    showDataOnDisplay();
    printDebugInfo();
    print_time = millis();
  }

  // Alerts for abnormal readings
  if (object_temp > 100 || beatAvg > 115) {
    Blynk.logEvent("vibration");
  }

  Blynk.run();
  timer.run();
}
