#include <Wire.h>
#include "MAX30105.h"
#include <OneWire.h>
#include <DallasTemperature.h>

#define SDA_PIN 22
#define SCL_PIN 23
#define TEMP_PIN 4

MAX30105 sensor;
OneWire oneWire(TEMP_PIN);
DallasTemperature ds18b20(&oneWire);

float temperatura = 0;
unsigned long ultimaTemp = 0;

void setup() {
  Serial.begin(115200);
  Wire.begin(SDA_PIN, SCL_PIN);

  if (!sensor.begin(Wire, I2C_SPEED_FAST)) {
    Serial.println("No se encontro el MAX30102");
    while (1);
  }

  // Brillo LED, promedio, modo (2 = rojo + IR), muestras/s,
  // ancho de pulso y rango del ADC.
  sensor.setup(60, 4, 2, 100, 411, 4096);

  ds18b20.begin();
  ds18b20.setWaitForConversion(false);
  ds18b20.requestTemperatures();
}

void loop() {
  // Solicita una nueva conversion sin bloquear el programa.
  if (millis() - ultimaTemp >= 1000) {
    temperatura = ds18b20.getTempCByIndex(0);
    ds18b20.requestTemperatures();
    ultimaTemp = millis();
  }

  uint32_t ir = sensor.getIR();
  uint32_t red = sensor.getRed();

  Serial.print("IR: ");
  Serial.print(ir);
  Serial.print("  |  Red: ");
  Serial.print(red);
  Serial.print("  |  Temp: ");
  Serial.print(temperatura, 2);
  Serial.println(" C");

  delay(100);
}
