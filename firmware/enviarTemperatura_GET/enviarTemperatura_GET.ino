#include <Wire.h>
#include "MAX30105.h"
#include "spo2_algorithm.h"
#include <OneWire.h>
#include <DallasTemperature.h>
#include <WiFi.h>
#include <MQTT.h>            // arduino-mqtt: permite publicar con QoS 1
#include <ArduinoJson.h>

// ---------- Red y broker MQTT (Mosquitto en la PC) ----------
const char* WIFI_SSID = "TU_RED_WIFI";
const char* WIFI_PASS = "TU_CONTRASENA_WIFI";
// IP de la PC con Mosquitto (en la PC: ipconfig -> Direccion IPv4). NO uses localhost.
#define MQTT_HOST   "IP_DE_LA_PC"
#define MQTT_PORT   1883
#define DEVICE_ID   "postuvital"                       // id del equipo en el esquema del reto
#define MQTT_TOPIC  "reto2026/postuvital/data"          // reto2026/<equipo>/data
#define MQTT_TOPIC_STATUS "reto2026/postuvital/status"  // online / offline (Last Will)
#define CLIENT_ID   "ESP32-postuvital"
#define MQTT_QOS    1
#define INTERVALO_ENVIO 5000                     // ms entre publicaciones normales

// ---------- Pines ----------
#define TEMP_PIN 4
#define SDA_BUS 19      // MAX30102 y MPU6050 comparten este bus
#define SCL_BUS 21

MAX30105 sensor;
OneWire oneWire(TEMP_PIN);
DallasTemperature ds18b20(&oneWire);

bool maxListo = false;
byte BRILLO = 20;
unsigned long ultimoIntento = 0;

uint32_t irBuffer[100];
uint32_t redBuffer[100];
int32_t spo2;
int8_t spo2Valido;
int32_t ritmoAlg;
int8_t ritmoAlgValido;

// ---------- Rescate del bus I2C ----------
void sclBajo()  { pinMode(SCL_BUS, OUTPUT); digitalWrite(SCL_BUS, LOW); }
void sclLibre() { pinMode(SCL_BUS, INPUT); }
void sdaBajo()  { pinMode(SDA_BUS, OUTPUT); digitalWrite(SDA_BUS, LOW); }
void sdaLibre() { pinMode(SDA_BUS, INPUT); }

// Libera un modulo que dejo SDA pegada a tierra: 9 pulsos de reloj y un STOP
void i2cLiberar() {
  sdaLibre();
  sclLibre();
  delayMicroseconds(10);
  for (int i = 0; i < 9; i++) {
    sclBajo();  delayMicroseconds(10);
    sclLibre(); delayMicroseconds(10);
  }
  sdaBajo();  delayMicroseconds(10);
  sclLibre(); delayMicroseconds(10);
  sdaLibre(); delayMicroseconds(10);
  if (digitalRead(SCL_BUS) == 0) {
    Serial.println("  (SCL sigue pegado a tierra: desconecta el USB 30 segundos)");
  }
}

void iniciarBus() {
  Wire.begin(SDA_BUS, SCL_BUS);
  Wire.setClock(100000);        // 100 kHz: aguanta mejor los cables largos
  Wire.setTimeOut(100);
}

void recuperarBus() {
  Wire.end();
  i2cLiberar();
  iniciarBus();
}

// ---------- Pulso (detector propio, 25 muestras/s = 40 ms) ----------
const float MS_POR_MUESTRA = 40.0;
const float AMP_MIN = 15.0;
float dc = 0, acSuav = 0, pico = 0;
bool arriba = false;
bool iniciado = false;
unsigned long numMuestra = 0;
unsigned long ultimoLatidoMuestra = 0;
const byte TAM = 5;
float bpms[TAM];
byte nBpm = 0, idxBpm = 0;

void guardarBpm(float b) {
  if (b < 40 || b > 180) return;
  bpms[idxBpm] = b;
  idxBpm = (idxBpm + 1) % TAM;
  if (nBpm < TAM) nBpm++;
}

float medianaBpm() {
  float t[TAM];
  for (byte i = 0; i < nBpm; i++) t[i] = bpms[i];
  for (byte i = 0; i < nBpm; i++)
    for (byte j = i + 1; j < nBpm; j++)
      if (t[j] < t[i]) { float x = t[i]; t[i] = t[j]; t[j] = x; }
  return t[nBpm / 2];
}

void reiniciarPulso() {
  iniciado = false;
  dc = 0; acSuav = 0; pico = 0;
  arriba = false;
  nBpm = 0; idxBpm = 0;
  ultimoLatidoMuestra = 0;
}

void procesarMuestra(uint32_t ir) {
  numMuestra++;
  if (!iniciado) { dc = ir; iniciado = true; }

  dc += ((float)ir - dc) * 0.08;
  float ac = (float)ir - dc;
  acSuav = acSuav * 0.5 + ac * 0.5;

  float umbral = pico * 0.5;
  if (acSuav > pico) pico = acSuav; else pico *= 0.97;

  if (!arriba && pico > AMP_MIN && acSuav > umbral) {
    arriba = true;
    if (ultimoLatidoMuestra == 0) {
      ultimoLatidoMuestra = numMuestra;
    } else {
      float ms = (numMuestra - ultimoLatidoMuestra) * MS_POR_MUESTRA;
      if (ms >= 330 && ms <= 1500) {
        guardarBpm(60000.0 / ms);
        ultimoLatidoMuestra = numMuestra;
      } else if (ms > 1500) {
        ultimoLatidoMuestra = numMuestra;
      }
    }
  }
  if (arriba && acSuav < 0) arriba = false;
}

void ajustarBrillo() {
  sensor.setPulseAmplitudeIR(BRILLO);
  sensor.setPulseAmplitudeRed(BRILLO);
}

bool iniciarMax() {
  if (!sensor.begin(Wire, I2C_SPEED_STANDARD)) return false;   // 100 kHz
  // brillo, promedio, modo (2 = rojo + IR), muestras/s, ancho de pulso, rango
  sensor.setup(BRILLO, 4, 2, 100, 411, 4096);
  return true;
}

// ---------- Temperatura ----------
float temperatura = NAN;
unsigned long ultimaTemp = 0;

void leerTemperatura() {
  if (millis() - ultimaTemp >= 1000) {
    float t = ds18b20.getTempCByIndex(0);
    temperatura = (t == DEVICE_DISCONNECTED_C) ? NAN : t;
    ds18b20.requestTemperatures();
    ultimaTemp = millis();
  }
}

// ---------- MPU6050 (mismo bus: Wire) ----------
bool mpuListo = false;
int mpuAddr = 0x68;
int mpuFallos = 0;
float ax, ay, az, roll, pitch, accMag;
float movMax = 0;        // pico de aceleracion desde el ultimo print
float accMaxEnvio = 0;   // pico de aceleracion desde el ultimo envio al servidor

bool iniciarMPU() {
  int dirs[2] = {0x68, 0x69};
  for (int i = 0; i < 2; i++) {
    Wire.beginTransmission(dirs[i]);
    if (Wire.endTransmission() == 0) {
      mpuAddr = dirs[i];
      Wire.beginTransmission(mpuAddr);
      Wire.write(0x6B);                  // PWR_MGMT_1
      Wire.write(0x00);                  // despierta el sensor
      Wire.endTransmission();
      Wire.beginTransmission(mpuAddr);
      Wire.write(0x1C);                  // ACCEL_CONFIG
      Wire.write(0x10);                  // rango +-8 g (con +-2 g el golpe de una caida se satura)
      Wire.endTransmission();
      return true;
    }
  }
  return false;
}

int16_t leer16() {
  int16_t hi = Wire.read();
  int16_t lo = Wire.read();
  return (hi << 8) | lo;
}

bool leerMPU() {
  Wire.beginTransmission(mpuAddr);
  Wire.write(0x3B);                      // primer registro de aceleracion
  if (Wire.endTransmission(false) != 0) return false;
  Wire.requestFrom(mpuAddr, 14, true);
  if (Wire.available() < 14) return false;

  int16_t rax = leer16();
  int16_t ray = leer16();
  int16_t raz = leer16();
  leer16();                              // temperatura interna (no se usa)
  leer16(); leer16(); leer16();          // giroscopio (no se usa aqui)

  ax = rax / 4096.0;                     // rango +-8 g
  ay = ray / 4096.0;
  az = raz / 4096.0;
  accMag = sqrt(ax * ax + ay * ay + az * az);
  roll = atan2(ay, az) * 57.2958;
  pitch = atan2(-ax, sqrt(ay * ay + az * az)) * 57.2958;
  return true;
}

// Deteccion de caida en 3 fases: caida libre -> golpe -> quietud
byte faseCaida = 0;
unsigned long tFase = 0;
unsigned long tQuieto = 0;
volatile bool caidaPendiente = false;     // true hasta que el servidor confirme (HTTP 200)

void detectarCaida() {
  unsigned long ahora = millis();
  switch (faseCaida) {
    case 0:                                   // espera caida libre (casi 0 g)
      if (accMag < 0.6) { faseCaida = 1; tFase = ahora; }
      break;
    case 1:                                   // espera un golpe en menos de 1 s
      if (accMag > 2.5) { faseCaida = 2; tFase = ahora; tQuieto = 0; }
      else if (ahora - tFase > 1000) faseCaida = 0;
      break;
    case 2:                                   // confirma 1.5 s de quietud (cerca de 1 g)
      if (accMag > 0.8 && accMag < 1.25) {
        if (tQuieto == 0) tQuieto = ahora;
        if (ahora - tQuieto >= 1500) {
          Serial.println(">>> CAIDA CONFIRMADA <<<");
          caidaPendiente = true;              // se manda en el proximo envio
          faseCaida = 0;
        }
      } else {
        tQuieto = 0;
        if (ahora - tFase > 4000) faseCaida = 0;   // no se quedo quieto: no fue caida
      }
      break;
  }
}

void tareasMPU() {
  if (!mpuListo) return;
  if (leerMPU()) {
    mpuFallos = 0;
    detectarCaida();
    if (accMag > movMax) movMax = accMag;
    if (accMag > accMaxEnvio) accMaxEnvio = accMag;
  } else if (++mpuFallos > 20) {         // 20 lecturas seguidas fallidas: se da por perdido
    mpuListo = false;
    Serial.println("MPU6050 sin datos. Reintento solo...");
  }
}

void imprimirSensores() {
  Serial.print("  |  Temp: ");
  if (isnan(temperatura)) Serial.print("--"); else Serial.print(temperatura, 1);
  Serial.print(" C");
  if (mpuListo) {
    Serial.print("  |  Roll: ");  Serial.print((int)roll);
    Serial.print("  Pitch: ");    Serial.print((int)pitch);
    Serial.print("  Mov: ");      Serial.print(movMax, 2);
    Serial.print(" g");
  } else {
    Serial.print("  |  MPU: --");
  }
  Serial.println();
  movMax = 0;
}

// ---------- Publicacion por MQTT (QoS 1) ----------
// Todo el trabajo de red va en otra tarea (nucleo 0) para que nunca
// detenga la lectura de sensores ni la deteccion de caidas.
int bpmEnvio = 0;                  // 0 = sin dato (no se envia)
int spo2Envio = 0;
bool primerEnvio = true;           // avisa que empezo una sesion nueva
unsigned long ultimoEnvio = 0;
unsigned long ultimoWifi = 0;
unsigned long ultimoMqtt = 0;
volatile bool enviando = false;    // true mientras la tarea de red tiene un mensaje en curso
String jsonEnvio;
bool jsonLlevaCaida = false;
bool jsonLlevaInicio = false;
TaskHandle_t tareaRedHandle = NULL;
WiFiClient wifiCliente;
MQTTClient mqtt(512);

void eventoWifi(WiFiEvent_t evento, WiFiEventInfo_t info) {
  if (evento == ARDUINO_EVENT_WIFI_STA_START) {
    Serial.print("[WiFi] MAC del ESP32: ");
    Serial.println(WiFi.macAddress());
  } else if (evento == ARDUINO_EVENT_WIFI_STA_GOT_IP) {
    Serial.print("[WiFi] Conectado. IP del ESP32: ");
    Serial.println(WiFi.localIP());
    configTime(0, 0, "pool.ntp.org", "time.google.com");   // hora real para el campo ts
  } else if (evento == ARDUINO_EVENT_WIFI_STA_DISCONNECTED) {
    // 2 = clave incorrecta/autenticacion, 201 = red no encontrada, 203 = asociacion rechazada
    Serial.print("[WiFi] Desconectado, razon ");
    Serial.println(info.wifi_sta_disconnected.reason);
  }
}

void tareaRed(void* p) {
  mqtt.begin(MQTT_HOST, MQTT_PORT, wifiCliente);
  mqtt.setTimeout(2000);
  mqtt.setKeepAlive(30);
  mqtt.setWill(MQTT_TOPIC_STATUS, "offline", true, MQTT_QOS);   // Last Will: el broker avisa si el ESP32 se cae
  for (;;) {
    if (ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(100))) {         // hay un mensaje listo
      bool ok = false;
      if (WiFi.status() == WL_CONNECTED) {
        if (!mqtt.connected() && millis() - ultimoMqtt > 3000) {
          ultimoMqtt = millis();
          if (mqtt.connect(CLIENT_ID)) {
            Serial.println("[MQTT] Conectado al broker");
            mqtt.publish(MQTT_TOPIC_STATUS, "online", true, MQTT_QOS);
          } else {
            Serial.print("[MQTT] No conecta al broker (error ");
            Serial.print((int)mqtt.lastError());
            Serial.println(")");
          }
        }
        if (mqtt.connected()) {
          ok = mqtt.publish(MQTT_TOPIC, jsonEnvio, false, MQTT_QOS);   // QoS 1: espera la confirmacion del broker
          Serial.println(ok ? "[MQTT] Publicado (QoS 1, confirmado por el broker)" : "[MQTT] Fallo al publicar");
        }
      } else {
        Serial.println("[MQTT] Sin WiFi, se reintenta");
      }
      if (ok) {
        if (jsonLlevaInicio) primerEnvio = false;
      } else if (jsonLlevaCaida) {
        caidaPendiente = true;                    // no se perdio: se manda otra vez
      }
      enviando = false;
    }
    mqtt.loop();
  }
}

// Se llama muy seguido desde el loop: mantiene el WiFi y deja listo el siguiente mensaje
void servicioRed() {
  unsigned long ahora = millis();

  if (WiFi.status() != WL_CONNECTED) {
    // No se reinicia el WiFi a cada rato: se reintenta con pausa.
    if (ahora - ultimoWifi > 10000) {
      ultimoWifi = ahora;
      Serial.println("[WiFi] Sin conexion, reintentando...");
      if (strlen(WIFI_PASS)) WiFi.begin(WIFI_SSID, WIFI_PASS); else WiFi.begin(WIFI_SSID);
      WiFi.setTxPower(WIFI_POWER_19_5dBm);
    }
    return;
  }

  if (enviando) return;
  bool urgente = caidaPendiente && (ahora - ultimoEnvio >= 1000);   // una caida no espera 5 s
  if (!urgente && ahora - ultimoEnvio < INTERVALO_ENVIO) return;

  bool llevaCaida = caidaPendiente;
  bool alarma = llevaCaida;                       // rangos normales del reto
  if (spo2Envio > 0 && spo2Envio < 95) alarma = true;
  if (bpmEnvio > 0 && (bpmEnvio < 60 || bpmEnvio > 100)) alarma = true;
  if (!isnan(temperatura) && (temperatura < 36.1 || temperatura > 37.2)) alarma = true;

  time_t t = time(nullptr);
  unsigned long ts = (t > 1700000000) ? (unsigned long)t : 0;   // 0 = sin hora (NTP aun no responde)
  if (ts == 0 && !llevaCaida && ahora < 30000) return;           // espera la hora NTP (hasta 30 s) para que ts nunca sea 0

  // Payload JSON con ArduinoJson (esquema plano del reto + variables propias del equipo)
  StaticJsonDocument<384> doc;
  doc["device_id"] = DEVICE_ID;
  doc["mac"]       = WiFi.macAddress();
  doc["ts"]        = ts;
  bool hayDatos = false;
  if (spo2Envio > 0) { doc["spo2"] = serialized(String((float)spo2Envio, 1)); hayDatos = true; }   // decimal
  if (bpmEnvio > 0)  { doc["hr"]   = bpmEnvio;                                hayDatos = true; }   // entero
  if (!isnan(temperatura)) { doc["temp_c"] = serialized(String(temperatura, 1)); hayDatos = true; }
  if (mpuListo) {
    float acc = constrain(accMaxEnvio, 0.0, 20.0);
    doc["acc_g"] = serialized(String(acc, 2));
    doc["roll"]  = serialized(String(roll, 1));
    doc["pitch"] = serialized(String(pitch, 1));
    doc["fall"]  = llevaCaida;
    hayDatos = true;
  }
  ultimoEnvio = ahora;
  if (!hayDatos) return;
  doc["alarm"] = alarma;
  if (primerEnvio) doc["inicio"] = true;

  String json;
  serializeJson(doc, json);
  Serial.print("[JSON] ");
  Serial.println(json);                          // evidencia del payload en el monitor serial

  if (mpuListo) accMaxEnvio = 0;
  if (llevaCaida) caidaPendiente = false;        // si falla, la tarea la vuelve a poner
  jsonEnvio = json;
  jsonLlevaCaida = llevaCaida;
  jsonLlevaInicio = primerEnvio;
  enviando = true;
  xTaskNotifyGive(tareaRedHandle);
}

// ---------- Diagnostico ----------
void diagnosticoLineas() {
  pinMode(SDA_BUS, INPUT_PULLDOWN);
  pinMode(SCL_BUS, INPUT_PULLDOWN);
  delay(50);
  int sda = digitalRead(SDA_BUS);
  int scl = digitalRead(SCL_BUS);
  Serial.print("Lineas: SDA (D");
  Serial.print(SDA_BUS);
  Serial.print(")=");
  Serial.print(sda);
  Serial.print("  SCL (D");
  Serial.print(SCL_BUS);
  Serial.print(")=");
  Serial.println(scl);
  if (!sda) Serial.println("  -> SDA no llega a ningun modulo o esta pegada a tierra.");
  if (!scl) Serial.println("  -> SCL no llega a ningun modulo o esta pegada a tierra.");
}

void escanearI2C() {
  Serial.print("Dispositivos I2C: ");
  int n = 0;
  for (byte d = 8; d < 120; d++) {
    Wire.beginTransmission(d);
    if (Wire.endTransmission() == 0) {
      Serial.print("0x");
      Serial.print(d, HEX);
      Serial.print(" ");
      n++;
    }
  }
  if (n == 0) Serial.print("ninguno");
  Serial.println();
}

// Si falta algun sensor, reinicia el bus y lo busca de nuevo cada 3 segundos
void reintentarSensores() {
  if (maxListo && mpuListo) return;
  if (millis() - ultimoIntento < 3000) return;
  ultimoIntento = millis();
  recuperarBus();
  if (!maxListo && iniciarMax()) {
    maxListo = true;
    reiniciarPulso();
    Serial.println("MAX30102 conectado");
  }
  if (!mpuListo && iniciarMPU()) {
    mpuListo = true;
    mpuFallos = 0;
    Serial.println("MPU6050 conectado");
  }
}

// ---------- setup ----------
void setup() {
  Serial.begin(115200);
  delay(1000);                            // deja que los modulos se estabilicen

  ds18b20.begin();
  ds18b20.setWaitForConversion(false);
  ds18b20.requestTemperatures();
  Serial.print("DS18B20 encontrados: ");
  Serial.println(ds18b20.getDeviceCount());

  i2cLiberar();
  diagnosticoLineas();
  iniciarBus();
  escanearI2C();                          // debe salir 0x57 (MAX30102) y 0x68 (MPU6050)

  maxListo = iniciarMax();
  Serial.println(maxListo ? "MAX30102 encontrado"
                          : "MAX30102 no responde. Reintento solo cada 3 segundos.");

  mpuListo = iniciarMPU();
  if (mpuListo) {
    Serial.print("MPU6050 encontrado en 0x");
    Serial.println(mpuAddr, HEX);
  } else {
    Serial.println("MPU6050 no responde. Reintento solo cada 3 segundos.");
  }

  WiFi.onEvent(eventoWifi);
  WiFi.persistent(false);
  WiFi.mode(WIFI_STA);
  WiFi.setAutoReconnect(false);   // se reintenta a mano, con pausa
  WiFi.setSleep(false);
  Serial.print("Buscando red ");
  Serial.print(WIFI_SSID);
  Serial.print(": ");
  int nRedes = WiFi.scanNetworks();
  bool vista = false;
  for (int i = 0; i < nRedes; i++) {
    if (WiFi.SSID(i) == WIFI_SSID) {
      vista = true;
      Serial.print("visible (senal ");
      Serial.print(WiFi.RSSI(i));
      Serial.print(" dBm, canal ");
      Serial.print(WiFi.channel(i));
      Serial.print(") ");
    }
  }
  Serial.println(vista ? "" : "NO VISIBLE en 2.4 GHz (el ESP32 no ve redes de 5 GHz)");
  WiFi.scanDelete();
  if (strlen(WIFI_PASS)) WiFi.begin(WIFI_SSID, WIFI_PASS); else WiFi.begin(WIFI_SSID);
  WiFi.setTxPower(WIFI_POWER_19_5dBm);   // maxima potencia: la senal de Tec-IoT es debil
  ultimoWifi = millis();
  xTaskCreatePinnedToCore(tareaRed, "red", 8192, NULL, 1, &tareaRedHandle, 0);

  Serial.println("Listo. Apoya la yema del dedo SIN apretar y quedate quieto 15 segundos.");
}

// ---------- loop ----------
void loop() {
  reintentarSensores();

  // Si el MAX30102 no responde, se sigue con temperatura y MPU
  if (!maxListo) {
    bpmEnvio = 0; spo2Envio = 0;
    leerTemperatura();
    tareasMPU();
    servicioRed();
    static unsigned long t = 0;
    if (millis() - t >= 1000) {
      t = millis();
      Serial.print("MAX30102: --");
      imprimirSensores();
    }
    delay(40);
    return;
  }

  uint64_t suma = 0;
  bool ok = true;

  for (byte i = 0; i < 100 && ok; i++) {
    unsigned long t0 = millis();
    while (!sensor.available()) {
      sensor.check();
      if (millis() - t0 > 300) { ok = false; break; }   // el sensor dejo de dar datos
    }
    if (!ok) break;

    redBuffer[i] = sensor.getFIFORed();
    irBuffer[i] = sensor.getFIFOIR();
    sensor.nextSample();
    suma += irBuffer[i];
    procesarMuestra(irBuffer[i]);
    leerTemperatura();
    tareasMPU();
    servicioRed();
  }

  if (!ok) {
    maxListo = false;
    bpmEnvio = 0; spo2Envio = 0;
    reiniciarPulso();
    ultimoIntento = millis();
    Serial.println("MAX30102 sin datos. Reinicio el bus y reintento solo...");
    return;
  }

  uint32_t irProm = suma / 100;

  if (irProm > 150000 && BRILLO > 5) { BRILLO -= 5; ajustarBrillo(); }
  else if (irProm > 10000 && irProm < 30000 && BRILLO < 100) { BRILLO += 5; ajustarBrillo(); }

  if (irProm < 10000) {
    reiniciarPulso();
    bpmEnvio = 0; spo2Envio = 0;
    Serial.print("No se detecta dedo");
    imprimirSensores();
    return;
  }

  maxim_heart_rate_and_oxygen_saturation(irBuffer, 100, redBuffer,
                                         &spo2, &spo2Valido, &ritmoAlg, &ritmoAlgValido);

  bpmEnvio  = (nBpm >= 3) ? (int)medianaBpm() : 0;
  spo2Envio = (spo2Valido && spo2 >= 80 && spo2 <= 100) ? (int)spo2 : 0;

  Serial.print("Pulso: ");
  if (nBpm >= 3) Serial.print((int)medianaBpm()); else Serial.print("--");
  Serial.print(" bpm  |  SpO2: ");
  if (spo2Valido && spo2 >= 80 && spo2 <= 100) Serial.print(spo2); else Serial.print("--");
  Serial.print(" %");
  imprimirSensores();
}