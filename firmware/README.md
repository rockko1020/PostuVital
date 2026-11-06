# Firmware del ESP32

En la etapa 2 se probaron por separado el sensor óptico MAX30102, el termómetro DS18B20 y la conexión WiFi del ESP32. Se conserva también un programa de lectura conjunta, que requiere validación física.

## Programas de la etapa 2

| Sketch | Descripción | Verificación |
|---|---|---|
| [`Prueba_MAX30102`](etapa2/Prueba_MAX30102/Prueba_MAX30102.ino) | Imprime las lecturas ópticas IR y Red cada 100 ms. | Evidencia en el reporte |
| [`Prueba_DS18B20`](etapa2/Prueba_DS18B20/Prueba_DS18B20.ino) | Imprime la temperatura en grados Celsius. | Evidencia en el reporte |
| [`Prueba_WiFi`](etapa2/Prueba_WiFi/Prueba_WiFi.ino) | Se conecta a WiFi y prueba una solicitud HTTP. | Evidencia en el reporte |
| [`Lectura_Integrada`](etapa2/Lectura_Integrada/Lectura_Integrada.ino) | Lee IR, Red y temperatura en un mismo sketch. | **Pendiente de probar simultáneamente** |

Cada `.ino` está dentro de una carpeta del **mismo nombre**, como requiere Arduino IDE. Son programas alternativos: abre y carga **uno a la vez**.

## Hardware y conexiones

| Componente | ESP32 |
|---|---|
| MAX30102 SDA | GPIO 22 |
| MAX30102 SCL | GPIO 23 |
| MAX30102 VIN / GND | 3V3 / GND |
| DS18B20 DATA | GPIO 4 |
| DS18B20 VCC / GND | 3V3 / GND |

En el montaje reportado se usó una resistencia pull-up de **10 kΩ** entre DATA del DS18B20 y 3V3. Consulta también el [diagrama](../docs/etapa2/diagrama_conexiones.png).

## Cómo ejecutar

1. Selecciona la placa **ESP32 Dev Module** y su puerto en Arduino IDE.
2. Instala las librerías **SparkFun MAX3010x Pulse and Proximity Sensor Library**, **OneWire** y **DallasTemperature**. `Wire`, `WiFi` y `HTTPClient` vienen con el soporte del ESP32.
3. Abre cualquiera de los archivos `.ino`, compila y sube ese sketch a la placa.
4. Abre el monitor serial a **115200 baudios**.
5. Para `Prueba_WiFi`, reemplaza los valores `TU_RED_WIFI` y `TU_CONTRASENA` **solo en tu copia local**. No subas credenciales reales al repositorio.

## Alcance actual

Los números IR y Red son datos crudos, **no** frecuencia cardiaca ni porcentaje de SpO2. La conexión WiFi probada no envía todavía lecturas a MQTT o Node-RED. El MPU6050 también se mantiene como parte de la arquitectura futura.
