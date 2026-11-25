# firmware

Sketch del ESP32 DevKit (Arduino IDE): `enviarTemperatura_GET/enviarTemperatura_GET.ino`.

Lee el MAX30102 (pulso y SpO2), el DS18B20 (temperatura de la piel) y el MPU6050 (movimiento y caídas), arma el payload JSON con ArduinoJson y lo publica por MQTT con QoS 1 en `reto2026/postuvital/data`. Publica `online`/`offline` (Last Will) en `reto2026/postuvital/status`. El envío corre en otra tarea del ESP32 para no frenar la lectura de sensores ni la detección de caídas.

**Antes de subirlo**, cambiar los valores de relleno:
- `WIFI_SSID` y `WIFI_PASS`: la red WiFi (2.4 GHz).
- `MQTT_HOST`: la IP de la laptop donde corre Mosquitto (`ipconfig` → Dirección IPv4; no usar `localhost`).

**Librerías:** MAX30105 (SparkFun), OneWire, DallasTemperature, MQTT (Joel Gaehwiler, arduino-mqtt), ArduinoJson 6, y WiFi (incluida con el ESP32). La detección de SpO2 usa `spo2_algorithm.h`.

**Pines del prototipo en protoboard:** I2C (MAX30102 y MPU6050) en SDA 19 / SCL 21, DS18B20 en GPIO 4. En la pulsera final se proponen SDA 21 / SCL 22, botón SOS en GPIO 27 y motor de vibración en GPIO 26.
