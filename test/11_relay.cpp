#include <Arduino.h>
const byte relay = 22;

void setup() {
    Serial.begin(115200);
    pinMode(relay, OUTPUT);
    digitalWrite(relay, HIGH);
}

void loop(){
    digitalWrite(relay, LOW);
    Serial.println("Relay Aktif");
    delay(5000);
    digitalWrite(relay, HIGH);
    Serial.println("Relay Nonaktif");
    delay(5000);
}