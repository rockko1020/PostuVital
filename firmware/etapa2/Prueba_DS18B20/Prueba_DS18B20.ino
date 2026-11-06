#include <OneWire.h>
#include <DallasTemperature.h>

#define TEMP_PIN 4

OneWire oneWire(TEMP_PIN);
DallasTemperature ds18b20(&oneWire);

void setup() {
  Serial.begin(115200);
  ds18b20.begin();
  Serial.print("Sensores DS18B20 encontrados: ");
  Serial.println(ds18b20.getDeviceCount());
}

void loop() {
  ds18b20.requestTemperatures();
  float temperatura = ds18b20.getTempCByIndex(0);

  Serial.print("Temp: ");
  Serial.print(temperatura, 2);
  Serial.println(" C");

  delay(1000);
}
