#include <WiFi.h>
#include <HTTPClient.h>

// Configura las credenciales SOLO en tu copia local.
// Nunca subas una contrasena real a GitHub.
const char* ssid = "TU_RED_WIFI";
const char* password = "TU_CONTRASENA";

void probarHTTP() {
  HTTPClient http;
  http.begin("http://example.com");
  int codigo = http.GET();

  Serial.print("Respuesta HTTP: ");
  Serial.println(codigo);

  http.end();
}

void setup() {
  Serial.begin(115200);
  delay(1000);

  Serial.print("Conectando a ");
  Serial.println(ssid);

  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);

  unsigned long inicio = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - inicio < 20000) {
    delay(500);
    Serial.print(".");
  }
  Serial.println();

  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("ESP32 conectado a WiFi");
    Serial.print("Direccion IP: ");
    Serial.println(WiFi.localIP());
    Serial.print("Intensidad de senal (RSSI): ");
    Serial.print(WiFi.RSSI());
    Serial.println(" dBm");
    probarHTTP();
  } else {
    Serial.println("No se pudo conectar. Revisa nombre y contrasena de la red.");
  }
}

void loop() {
  if (WiFi.status() == WL_CONNECTED) {
    delay(10000);
    probarHTTP();
  }
}
