#include <Wire.h>
#include "MAX30105.h"

#define SDA_PIN 22
#define SCL_PIN 23

MAX30105 sensor;

void setup() {
  Serial.begin(115200);
  Wire.begin(SDA_PIN, SCL_PIN);

  if (!sensor.begin(Wire, I2C_SPEED_FAST)) {
    Serial.println("No se encontro el MAX30102");
    while (1);
  }

  // Intensidad LED, promedio, modo (2 = rojo + IR),
  // muestras/s, ancho de pulso y rango del ADC.
  sensor.setup(60, 4, 2, 100, 411, 4096);
}

void loop() {
  uint32_t ir = sensor.getIR();
  uint32_t red = sensor.getRed();

  Serial.print("IR: ");
  Serial.print(ir);
  Serial.print("  |  Red: ");
  Serial.println(red);

  delay(100);
}
