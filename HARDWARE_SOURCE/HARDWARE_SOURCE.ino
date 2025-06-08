#include "stone.h"

// === Pin Definitions ===
const int STEP_PIN = 11;
const int DIR_PIN = 10;
const int EN_PIN = 9;
const int BUTTON_PIN = 12;
const int PRESSURE_PIN_A = A5;
const int PRESSURE_PIN_B = A4;
const int IE_PIN = A2;
const int CONTROL_PIN = A1;
const int RR_PIN = A0;

// === Helper Function for Pressure Calibration ===
float readPressureCmH2O(int analogPin) {
    const float Vcc = 5.0;
    const float offset = 0.2;       // sensor offset voltage at 0 kPa
    const float sensitivity = 0.45; // V/kPa (MPX5010 sensor)

    int raw = analogRead(analogPin);
    float voltage = (raw / 1023.0) * Vcc;
    float pressure_kPa = (voltage - offset) / sensitivity;
    float pressure_cmH2O = pressure_kPa * 10.1972;  // convert kPa to cmH2O

    return pressure_cmH2O;
}

// === Ventilator Class ===
class Ventilator {
public:
    int tidalVolume;
    int respiratoryRate;
    int rrDelay;
    float ieRatio;
    int tidalControl;
    int peep, pip;
    float pressureA, pressureB;

    Ventilator() : tidalVolume(0), respiratoryRate(0), rrDelay(0),
                   ieRatio(0), tidalControl(0), peep(1023), pip(0),
                   pressureA(0), pressureB(0) {}

    void readSensors() {
        pressureA = readPressureCmH2O(PRESSURE_PIN_A);
        pressureB = readPressureCmH2O(PRESSURE_PIN_B);

        peep = min(peep, (int)pressureB);
        pip  = max(pip, (int)pressureB);
    }

    void updateParameters() {
        int ieRaw = analogRead(IE_PIN);
        ieRatio = map(ieRaw, 0, 1023, 0, 400);

        int controlVar = analogRead(CONTROL_PIN);
        tidalControl = map(controlVar, 0, 1023, 500, 2400);
        tidalVolume  = map(tidalControl, 200, 1500, 150, 550);

        int rrRaw = analogRead(RR_PIN);
        rrDelay = map(rrRaw, 0, 1023, 170, 1000);
        respiratoryRate = map(rrDelay, 200, 600, 30, 10);
    }

    void forwardStroke() {
        digitalWrite(DIR_PIN, HIGH);
        for (int x = 0; x < tidalControl; x++) {
            digitalWrite(STEP_PIN, HIGH);
            delayMicroseconds(rrDelay + ieRatio);
            digitalWrite(STEP_PIN, LOW);
            delayMicroseconds(rrDelay + ieRatio);
        }
        delay(500);
    }

    void reverseStroke() {
        digitalWrite(DIR_PIN, LOW);
        while (digitalRead(BUTTON_PIN) == LOW) {
            digitalWrite(STEP_PIN, HIGH);
            delayMicroseconds(rrDelay);
            digitalWrite(STEP_PIN, LOW);
            delayMicroseconds(rrDelay);
        }
    }
};

// === Display Manager ===
class DisplayManager {
public:
    void send(const Ventilator& v) {
        int temperature = random(19, 23); // Simulated temperature
        float ieDisplay = (v.rrDelay + v.ieRatio) / (float)v.rrDelay;

        sendLabel("tv", v.tidalVolume);
        sendLabel("br", v.respiratoryRate);
        sendLabel("ie", ieDisplay, 1);
        sendLabel("peep", v.peep);
        sendLabel("pip", v.pip);

        sendProgressBar("x", v.tidalVolume);
        sendProgressBar("xy", v.tidalVolume);
        sendProgressBar("xyz", v.tidalVolume);

        sendLabel("temp", temperature);

        sendLineSeries("line_series1", v.pressureA);
        sendLineSeries("line_series2", v.pressureB);
    }

private:
    void sendLabel(const char* widget, float value, int precision = 0) {
        Serial.print("ST<{\"cmd_code\":\"set_value\",\"type\":\"label\",\"widget\":\"");
        Serial.print(widget);
        Serial.print("\",\"value\":");
        Serial.print(value, precision);
        Serial.print(",\"format\":\"");
        Serial.print(precision == 0 ? "%d" : "%.1f");
        Serial.println("\"}>ET");
    }

    void sendProgressBar(const char* widget, int value) {
        Serial.print("ST<{\"cmd_code\":\"set_value\",\"type\":\"progress_bar\",\"widget\":\"");
        Serial.print(widget);
        Serial.print("\",\"value\":");
        Serial.print(value);
        Serial.println(",\"format\":\"%d\"}>ET");
    }

    void sendLineSeries(const char* widget, float value) {
        Serial.print("ST<{\"cmd_code\":\"set_value\",\"type\":\"line_series\",\"widget\":\"");
        Serial.print(widget);
        Serial.print("\",\"mode\":\"push\",\"value\":");
        Serial.print(value, 1);
        Serial.println(",\"format\":\"%.1f\"}>ET");
    }
};

// === Instantiate Objects ===
Ventilator ventilator;
DisplayManager display;

void setup() {
    Serial.begin(115200);

    pinMode(BUTTON_PIN, INPUT);
    pinMode(STEP_PIN, OUTPUT);
    pinMode(DIR_PIN, OUTPUT);
    pinMode(EN_PIN, OUTPUT);

    digitalWrite(EN_PIN, LOW);
    delay(2000);
    digitalWrite(DIR_PIN, LOW);

    // Home stepper motor at start
    while (digitalRead(BUTTON_PIN) == LOW) {
        digitalWrite(STEP_PIN, HIGH);
        delayMicroseconds(170);
        digitalWrite(STEP_PIN, LOW);
        delayMicroseconds(170);
    }
}

void loop() {
    ventilator.readSensors();
    ventilator.updateParameters();
    display.send(ventilator);
    if (digitalRead(EN_PIN) == HIGH) {
        ventilator.forwardStroke();
        ventilator.reverseStroke();
    }
}
