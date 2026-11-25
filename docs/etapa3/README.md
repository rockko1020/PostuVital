# Etapa 3: esquema de datos y publicación por MQTT

Esta carpeta contiene la documentación del cierre de la etapa 3 ([documento de entrega completo](Cierre_Etapa_3.docx), con capturas y respuestas a las preguntas de cierre): el esquema JSON del proyecto, el código que arma el payload en el ESP32 y la publicación por MQTT con evidencias.

PostuVital es una **pulsera tipo reloj para adultos mayores** que monitorea signos vitales y movimiento y detecta caídas. Las pruebas de esta etapa se hicieron con el prototipo en protoboard.

## Parte 1. Esquema de datos

Se partió del esquema plano de referencia del reto (`device_id`, `ts`, `spo2`, `hr`, `temp_c`, `alarm`), manteniendo `device_id` y `ts`, y se agregaron las variables propias del equipo (movimiento y caída).

| Variable | Campo | Unidad | Tipo | Rango normal |
|---|---|---|---|---|
| Identificador del equipo | `device_id` | - | texto | `postuvital` |
| Marca de tiempo | `ts` | segundos (Unix) | entero | hora real por NTP |
| Oximetría | `spo2` | % | decimal | 95 - 100 |
| Frecuencia cardiaca | `hr` | lpm | entero | 60 - 100 |
| Temperatura (piel de la muñeca) | `temp_c` | °C | decimal | 36.1 - 37.2 (referencia del reto) |
| Adicional: aceleración total | `acc_g` | g | decimal | informativa (cerca de 1 g en reposo) |
| Adicional: orientación de la muñeca (lateral) | `roll` | grados | decimal | informativa |
| Adicional: orientación de la muñeca (frontal) | `pitch` | grados | decimal | informativa |
| Adicional: caída detectada | `fall` | - | booleano | `false` |
| Alarma general | `alarm` | - | booleano | `false` |
| Dirección MAC del ESP32 | `mac` | - | texto | `68:09:47:E0:57:9C` |

`alarm` vale `true` si hay caída o si `spo2`, `hr` o `temp_c` salen de su rango. `acc_g`, `roll` y `pitch` son informativas y no disparan la alarma. Si un sensor no tiene lectura válida, ese campo no se envía. El campo `inicio` aparece solo en el primer mensaje tras encender, para abrir una sesión nueva en la base de datos.

En la pulsera final el rango de temperatura se ajustará: la piel de la muñeca marca menos (unos 33 - 35 °C) que la temperatura corporal.

### Ejemplos de payload

- [Valores normales](ejemplos/payload_normal.json)
- [Con caída y alarma](ejemplos/payload_caida.json)

### Código que arma el payload

El sketch del ESP32 está en [`firmware/enviarTemperatura_GET/enviarTemperatura_GET.ino`](../../firmware/enviarTemperatura_GET/enviarTemperatura_GET.ino) (sin contraseñas). Arma el JSON con ArduinoJson (`StaticJsonDocument`, `serializeJson`) y lo imprime con el prefijo `[JSON]` en el monitor serial.

## Parte 2. Publicación por MQTT

| Elemento | Valor |
|---|---|
| Broker | Mosquitto individual, en la laptop del equipo (puerto 1883); config en [`mqtt/mosquitto_pv.conf`](../../mqtt/mosquitto_pv.conf) |
| Tópico de datos | `reto2026/postuvital/data` |
| Tópico de estado | `reto2026/postuvital/status` (`online` / `offline`, retenido) |
| Client ID | `ESP32-postuvital` |
| Periodo de publicación | cada 5 s (inmediato, en menos de 1 s, si hay caída) |
| QoS | 1 |
| Librerías | `MQTT` (arduino-mqtt) y `ArduinoJson` 6 |

**QoS 1:** el broker confirma cada mensaje y el ESP32 sabe si llegó. Una caída es un evento crítico y no debe perderse; un reintento que duplique una lectura no causa problema, porque cada mensaje lleva su `ts`. QoS 0 no confirma nada y QoS 2 agrega más tráfico del que el proyecto necesita. Se usó la librería `MQTT` y no `PubSubClient`, porque esta última solo publica con QoS 0.

**Last Will:** al conectar, el ESP32 registra el testamento `offline` en `reto2026/postuvital/status`. Si se desconecta sin avisar, el broker lo publica solo; al reconectar, el ESP32 publica `online`.

## Evidencias

- [Arranque: WiFi y broker conectados (24/11/2026)](evidencias/01_arranque_conectado_al_broker_24nov.png). El primer mensaje de esta captura trae `"ts":0` porque el ESP32 aún no tenía la hora por NTP; después se corrigió y el sketch espera la hora real (hasta 30 s) antes del primer envío.
- [Monitor serial con los payloads JSON y `[MQTT] Publicado` (25/11/2026)](evidencias/02_monitor_serial_json_25nov.png).
- [`mosquitto_sub` recibiendo los mensajes en `reto2026/postuvital/data` (25/11/2026)](evidencias/03_mosquitto_sub_25nov.png).
- [Datos guardados en MySQL (24/11/2026)](evidencias/04_base_de_datos_mysql_24nov.png).

Para ver los mensajes: `mosquitto_sub -h <IP de la laptop> -t "reto2026/+/#" -v`.

## Resultado y limitaciones

- El ESP32 arma un payload JSON válido y lo publica con QoS 1 cada 5 s; el broker lo recibe y Node-RED lo guarda en MySQL.
- En las capturas `alarm` sale `true` porque el DS18B20 mide la temperatura ambiente (22 - 23 °C) y no la de la piel, que queda fuera del rango. El SpO2 varía entre lecturas por el ruido del sensor y falta en algunos mensajes por mal contacto.
- El **dashboard está en construcción**; no se incluye evidencia de él en esta etapa.
- Aún no se probó una caída física real, y los umbrales son de partida.
- La conexión a la red Tec-IoT no ha funcionado (señal débil y errores de asociación); las pruebas se hicieron con el hotspot de un celular.
- Quedan para las siguientes etapas: botón SOS, motor de vibración, detección de inactividad y un esquema de base de datos y tópicos por sensor para la pulsera.
