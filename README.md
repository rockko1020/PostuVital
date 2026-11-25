# PostuVital

Pulsera tipo reloj para **adultos mayores** que monitorea **frecuencia cardiaca**, **saturación de oxígeno (SpO2)**, **temperatura de la piel** y **movimiento**, detecta **caídas** y muestra los datos en tiempo real en un dashboard con alertas. Más adelante tendrá un botón SOS, detección de inactividad y un motor de vibración para avisar al usuario.

> **Cambio de rumbo (noviembre 2026):** la primera versión era una banda al cuello para medir la postura cervical. El profesor pidió una pulsera, así que el proyecto se reorientó. El documento de la Etapa 1 y su diagrama (`docs/`) corresponden a esa primera versión.

**Materia:** Implementación de internet de las cosas (Gpo 501)

## Equipo 1 · Pulso Binario

| Integrante | Matrícula | Rol | Capas |
|---|---|---|---|
| Rodrigo Herrera Ávila | A01803050 | Hardware y conectividad del lado del ESP32: circuito, lectura de sensores y envío por MQTT | 1 y 2 |
| Alejandro Lazo de la Vega Martínez | A01804700 | Conectividad del lado del servidor, datos y visualización: broker, Node-RED, base de datos y dashboard | 2, 3 y 4 |

## Cómo funciona

```
ESP32 --MQTT QoS 1 (reto2026/postuvital/data)--> Mosquitto --> Node-RED --> api.php --> MySQL (postuvital)
                                                                  |
                                                                  +--> Dashboard (Node-RED, /ui; en construcción)
```

El ESP32 lee los sensores, arma un JSON con ArduinoJson y lo publica por MQTT cada 5 segundos (de inmediato si detecta una caída). Node-RED lo recibe, lo reenvía a `api.php`, que lo guarda en MySQL y abre o cierra las alertas según los umbrales de la tabla `threshold`, y alimenta el dashboard (en construcción).

Ejemplo de mensaje:

```json
{"device_id":"postuvital","mac":"68:09:47:E0:57:9C","ts":1795636784,"spo2":93.0,"hr":68,"temp_c":33.8,"acc_g":1.05,"roll":102.7,"pitch":-19.7,"fall":false,"alarm":false}
```

| Campo | Unidad | Tipo | Rango normal |
|---|---|---|---|
| `device_id` | - | texto | `postuvital` |
| `ts` | segundos (Unix) | entero | hora real por NTP |
| `spo2` | % | decimal | 95 - 100 |
| `hr` | lpm | entero | 60 - 100 |
| `temp_c` | °C | decimal | 36.1 - 37.2 (referencia del reto; se ajustará para la muñeca, que marca ~33 - 35 °C) |
| `acc_g` | g | decimal | cerca de 1 g en reposo |
| `roll`, `pitch` | grados | decimal | informativos |
| `fall` | - | booleano | `false` |
| `alarm` | - | booleano | `false` |

Si un sensor no tiene lectura válida, ese campo no se envía.

## Tecnología

| Capa | Componente | Tecnología |
|---|---|---|
| 1. Percepción | Pulso y SpO2 | MAX30102 (abajo de la caja, tocando la muñeca) |
| 1. Percepción | Temperatura de la piel | DS18B20 |
| 1. Percepción | Movimiento y caídas | MPU6050 (dentro de la caja) |
| 2. Red | Microcontrolador | ESP32 DevKit (WiFi 2.4 GHz) |
| 2. Red | Protocolo y broker | MQTT con QoS 1, Mosquitto |
| 3. Procesamiento | Recepción, dashboard y reenvío | Node-RED |
| 3. Procesamiento | Guardado y alertas por umbral | PHP (`api.php`) |
| 3. Procesamiento | Base de datos | MySQL / MariaDB (XAMPP) |
| 4. Aplicación | Dashboard | Dashboard de Node-RED |
| 5. Negocio | Usuarios | Adultos mayores y sus cuidadores |

## Avances

- [x] **Etapa 1:** ficha de equipo y arquitectura v1 (documento en `docs/`, entregado el 06/10/2026).
- [x] **Etapa 2:** pruebas individuales del MAX30102 y el DS18B20 por monitor serial, y conectividad WiFi con respuesta HTTP 200 ([reporte y evidencias](docs/etapa2/README.md)).
- [x] **Cableado y lectura de sensores** (MAX30102, DS18B20 y MPU6050) en protoboard.
- [x] **Esquema de datos y payload JSON** con ArduinoJson.
- [x] **Etapa 3:** esquema de datos, payload JSON con ArduinoJson y publicación por MQTT con QoS 1 y Last Will en `reto2026/postuvital/data` y `.../status` ([documentación y evidencias](docs/etapa3/README.md)).
- [x] **MQTT → Node-RED → MySQL:** los mensajes recibidos se guardan en la base de datos.
- [ ] **Dashboard de Node-RED:** en construcción.
- [x] **Detección de caída simulada** con alerta que se abre y se cierra sola.
- [ ] Armado de la pulsera: botón SOS, motor de vibración y caja.
- [ ] Detección de inactividad y caída física real; ajustar umbrales.
- [ ] Esquema de base de datos y tópicos por sensor para la pulsera.
- [ ] Funcionar sin la laptop (broker en internet) y conexión a la red Tec-IoT.

## Estructura del repositorio

```
PostuVital/
├── docs/        Etapa 1 (versión inicial, banda al cuello), Etapa 2 (etapa2/) y Etapa 3 (etapa3/)
├── firmware/    Sketch actual del ESP32 y sketches de prueba de la Etapa 2 (Arduino IDE)
├── mqtt/        Configuración del broker Mosquitto
├── nodered/     Flujo de Node-RED
└── db/          Esquema de MySQL y api.php (guarda las lecturas)
```

## Cómo probarlo

1. Importar `db/schema.sql` en MySQL (**ojo:** empieza con `DROP DATABASE`, borra los datos) y cambiar `CAMBIA_ESTA_CONTRASENA` y `CAMBIA_ESTA_CLAVE` en `db/api.php` y en el flujo de Node-RED.
2. Arrancar Mosquitto: `mosquitto -c mqtt/mosquitto_pv.conf`.
3. Importar `nodered/flujo_postuvital.json` en Node-RED; el dashboard (en construcción) queda en `http://localhost:1880/ui`.
4. En `firmware/enviarTemperatura_GET/enviarTemperatura_GET.ino` poner `WIFI_SSID`, `WIFI_PASS` y la IP de la laptop en `MQTT_HOST` (2.4 GHz). Subirlo al ESP32.
5. Ver los mensajes: `mosquitto_sub -h <IP de la laptop> -t "reto2026/+/#" -v`.

## Etapa 3: esquema de datos y MQTT

- [Documentación de la Etapa 3](docs/etapa3/README.md): tabla de variables, ejemplos de payload, configuración de MQTT, QoS y evidencias.
- [Sketch del ESP32](firmware/enviarTemperatura_GET/enviarTemperatura_GET.ino), [configuración de Mosquitto](mqtt/mosquitto_pv.conf), [flujo de Node-RED](nodered/flujo_postuvital.json) y [esquema de MySQL](db/schema.sql).

## Etapa 2: montaje y primeras lecturas

- [Reporte de cierre de la Etapa 2](docs/etapa2/Cierre_Etapa_2.pdf), con credenciales ocultas.
- [Diagrama de conexiones y fotografías](docs/etapa2/README.md).
- [Sketches de prueba](firmware/README.md): MAX30102, DS18B20, WiFi y lectura conjunta.

En esa etapa los valores IR y Red del MAX30102 eran datos crudos, sin convertir a pulsaciones ni a SpO2; la conversión se hizo después en el sketch actual.

> Este repositorio es público: **nunca** se suben contraseñas, claves ni capturas del IDE que las muestren. Los archivos llevan valores de relleno.
