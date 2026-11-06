# Etapa 2: montaje, lectura por serial y conectividad WiFi

Esta carpeta contiene las evidencias del cierre de la etapa 2.

- [Reporte completo](Cierre_Etapa_2.pdf): incluye descripción del montaje, capturas del monitor serial, análisis del ruido del MAX30102, reflexiones y prueba de WiFi. La captura con el código WiFi se preparó para publicación ocultando las credenciales.
- [Diagrama de conexiones](diagrama_conexiones.png): ESP32, MAX30102 y DS18B20.
- [Fotografía del montaje MAX30102](evidencias/montaje_max30102.png).
- [Fotografía del montaje DS18B20](evidencias/montaje_ds18b20.png).
- [IR/Red sin dedo](evidencias/max30102_sin_dedo.png) y [con dedo](evidencias/max30102_con_dedo.png).
- [Prueba de temperatura](evidencias/ds18b20_temperatura_1.png) y [otra captura](evidencias/ds18b20_temperatura_2.png).

## Resultado de las pruebas

- El MAX30102 fue detectado por I2C en la dirección `0x57` y produjo señales IR y Red que aumentaron al colocar el dedo.
- El DS18B20 entregó lecturas de temperatura con la resistencia pull-up de 10 kΩ utilizada en el montaje.
- El ESP32 se conectó a WiFi, obtuvo una dirección IP y recibió una respuesta HTTP 200 en la prueba.

**Limitación:** las pruebas físicas de los sensores se documentaron por separado. También existe un [sketch de lectura integrada](../../firmware/etapa2/Lectura_Integrada/Lectura_Integrada.ino), pero aún no se incluye evidencia de su funcionamiento simultáneo. Las mediciones IR/Red no son todavía valores calculados de SpO2 o frecuencia cardiaca; la conectividad WiFi no transmite datos de sensores aún.
