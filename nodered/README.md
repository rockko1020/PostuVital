# nodered

`flujo_postuvital.json`: flujo de Node-RED exportado, sin claves. Se suscribe a `reto2026/+/data`, separa los datos para el dashboard (pulso, SpO2, temperatura y movimiento, en `/ui`) y los manda por POST a `db/api.php` (`http://localhost/postuvital/api.php`), que los guarda en MySQL y abre o cierra las alertas según los umbrales.

**Para usarlo:**
1. Importar el JSON en Node-RED (menú → Import).
2. En el nodo de configuración del broker (`Mosquitto PC`) poner la IP de la laptop donde corre Mosquitto. En el archivo está la IP del hotspot usado en las pruebas (`172.20.10.2`).
3. En el nodo `Preparar para MySQL (API)` cambiar `CAMBIA_ESTA_CLAVE` por la misma clave que `API_CLAVE` en `db/api.php`.

Pendiente para la pulsera: tópicos por sensor, alerta de inactividad y SOS, y la orden de vibración al ESP32.
