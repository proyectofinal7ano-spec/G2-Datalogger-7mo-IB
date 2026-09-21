#include <WiFi.h>
#include <WebServer.h>
#include <ESPmDNS.h>
#include <Preferences.h>

#include <Wire.h>
#include <SPI.h>
#include <SD.h>

#include <Adafruit_BME280.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <RTClib.h>

// =========================================================
// DATALOGGER ESP32
// BME280 + OLED SSD1306 + DS3231 + microSD + Web/API
// =========================================================

// -------------------------
// WIFI: CAMBIAR ESTOS DATOS
// -------------------------
const char* WIFI_SSID = "Data";
const char* WIFI_PASSWORD = "admin123";

// Si no encuentra tu WiFi, crea esta red propia.
const char* AP_SSID = "DataLogger-ESP32";
const char* AP_PASSWORD = "datalogger123";

// -------------------------
// I2C
// -------------------------
#define I2C_SDA 21
#define I2C_SCL 22

// -------------------------
// BME280
// -------------------------
#define BME_ADDRESS 0x76
Adafruit_BME280 bme;

// -------------------------
// OLED
// -------------------------
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET -1
#define OLED_ADDRESS 0x3C
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

// -------------------------
// RTC DS3231
// -------------------------
RTC_DS3231 rtc;

// -------------------------
// microSD
// -------------------------
#define SD_CS    13
#define SD_SCK   18
#define SD_MISO  19
#define SD_MOSI  23

// -------------------------
// SERVIDOR Y PREFERENCIAS
// -------------------------
WebServer server(80);
Preferences preferences;

// Subida del frontend a la microSD sin lector externo
File archivoUploadWeb;
bool uploadWebOK = false;

// -------------------------
// CONFIGURACION
// -------------------------
unsigned long samplingIntervalMs = 60000UL;  // 1 minuto
uint16_t retentionDays = 30;                 // 0 = sin limite

// -------------------------
// ESTADOS
// -------------------------
bool bmeOK = false;
bool oledOK = false;
bool rtcOK = false;
bool sdOK = false;
bool wifiOK = false;
bool modoAP = false;
bool medicionValida = false;

// -------------------------
// ULTIMA MEDICION
// -------------------------
float temperaturaActual = 0.0f;
float humedadActual = 0.0f;
float presionActual = 0.0f; // Pa
DateTime fechaActual((uint32_t)0);

unsigned long ultimaMedicionMs = 0;
unsigned long ultimoChequeoWiFiMs = 0;
uint32_t ultimoDiaLimpieza = 0;
uint32_t registrosHoy = 0;
uint32_t registrosDiaId = 0;

// =========================================================
// FECHA / HORA
// =========================================================
String dosDigitos(int n) {
  return n < 10 ? "0" + String(n) : String(n);
}

String fechaISO(const DateTime& dt) {
  return String(dt.year()) + "-" + dosDigitos(dt.month()) + "-" + dosDigitos(dt.day());
}

String horaISO(const DateTime& dt) {
  return dosDigitos(dt.hour()) + ":" + dosDigitos(dt.minute()) + ":" + dosDigitos(dt.second());
}

String fechaHoraISO(const DateTime& dt) {
  // Argentina usa UTC-03:00 durante todo el año actualmente.
  // El DS3231 guarda hora local, por eso agregamos el offset explicitamente.
  return fechaISO(dt) + "T" + horaISO(dt) + "-03:00";
}

String archivoDelDia(const DateTime& dt) {
  return "/data/" + fechaISO(dt) + ".csv";
}

uint32_t identificadorDia(const DateTime& dt) {
  return (uint32_t)dt.year() * 10000UL + (uint32_t)dt.month() * 100UL + dt.day();
}

String uint64String(uint64_t value) {
  char buffer[32];
  snprintf(buffer, sizeof(buffer), "%llu", (unsigned long long)value);
  return String(buffer);
}

// =========================================================
// OLED
// =========================================================
void actualizarOLED() {
  if (!oledOK) return;

  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);
  display.setTextSize(1);

  display.setCursor(0, 0);
  display.print(rtcOK ? horaISO(fechaActual) : "RTC ERROR");

  display.setCursor(0, 14);
  display.print("Temp: ");
  if (medicionValida) {
    display.print(temperaturaActual, 1);
    display.print(" C");
  } else {
    display.print("ERROR");
  }

  display.setCursor(0, 27);
  display.print("Hum:  ");
  if (medicionValida) {
    display.print(humedadActual, 1);
    display.print(" %");
  } else {
    display.print("ERROR");
  }

  display.setCursor(0, 40);
  display.print("Pres: ");
  if (medicionValida) {
    display.print(presionActual / 100.0f, 1);
    display.print(" hPa");
  } else {
    display.print("ERROR");
  }

  display.setCursor(0, 54);
  display.print("SD:");
  display.print(sdOK ? "OK" : "ERR");
  display.print(" WiFi:");
  if (modoAP) display.print("AP");
  else display.print(wifiOK ? "OK" : "ERR");

  display.display();
}

// =========================================================
// SD: GUARDAR MEDICION
// =========================================================
bool guardarMedicionSD() {
  if (!sdOK || !rtcOK || !medicionValida) return false;

  String ruta = archivoDelDia(fechaActual);
  bool nuevo = !SD.exists(ruta);

  File archivo = SD.open(ruta, FILE_APPEND);
  if (!archivo) {
    Serial.println("[SD] ERROR abriendo " + ruta);
    return false;
  }

  if (nuevo) {
    archivo.println("timestamp,temperatura,humedad,presion");
  }

  archivo.print(fechaHoraISO(fechaActual));
  archivo.print(',');
  archivo.print(temperaturaActual, 2);
  archivo.print(',');
  archivo.print(humedadActual, 2);
  archivo.print(',');
  archivo.println(presionActual, 2);
  archivo.close();

  uint32_t hoy = identificadorDia(fechaActual);
  if (registrosDiaId != hoy) {
    registrosDiaId = hoy;
    registrosHoy = 0;
  }
  registrosHoy++;

  Serial.println("[SD] Medicion guardada en " + ruta);
  return true;
}

// =========================================================
// SENSOR
// =========================================================
void tomarMedicion(bool guardar) {
  if (rtcOK) fechaActual = rtc.now();

  if (bmeOK) {
    temperaturaActual = bme.readTemperature();
    humedadActual = bme.readHumidity();
    presionActual = bme.readPressure();

    medicionValida =
      !isnan(temperaturaActual) &&
      !isnan(humedadActual) &&
      !isnan(presionActual) &&
      presionActual > 0;
  } else {
    medicionValida = false;
  }

  Serial.println();
  Serial.println("========= MEDICION =========");

  if (rtcOK) {
    Serial.print("Fecha: ");
    Serial.println(fechaISO(fechaActual));
    Serial.print("Hora: ");
    Serial.println(horaISO(fechaActual));
  }

  if (medicionValida) {
    Serial.print("Temperatura: ");
    Serial.print(temperaturaActual, 2);
    Serial.println(" C");

    Serial.print("Humedad: ");
    Serial.print(humedadActual, 2);
    Serial.println(" %");

    Serial.print("Presion: ");
    Serial.print(presionActual, 2);
    Serial.println(" Pa");
  } else {
    Serial.println("[ERROR] Medicion BME280 invalida");
  }

  if (guardar) guardarMedicionSD();
  actualizarOLED();
  Serial.println("============================");
}

// =========================================================
// SD: RETENCION
// =========================================================
void limpiarArchivosViejos() {
  if (!sdOK || !rtcOK || retentionDays == 0) return;

  DateTime ahora = rtc.now();
  uint32_t idDia = identificadorDia(ahora);
  if (ultimoDiaLimpieza == idDia) return;
  ultimoDiaLimpieza = idDia;

  uint32_t segundosRetencion = (uint32_t)retentionDays * 86400UL;
  uint32_t limite = ahora.unixtime() > segundosRetencion ? ahora.unixtime() - segundosRetencion : 0;

  File carpeta = SD.open("/data");
  if (!carpeta || !carpeta.isDirectory()) {
    if (carpeta) carpeta.close();
    return;
  }

  Serial.println("[SD] Revisando archivos vencidos...");
  File archivo = carpeta.openNextFile();

  while (archivo) {
    String ruta = archivo.name();
    bool esArchivo = !archivo.isDirectory();
    archivo.close();

    if (esArchivo) {
      String nombre = ruta;
      int slash = nombre.lastIndexOf('/');
      if (slash >= 0) nombre = nombre.substring(slash + 1);

      // YYYY-MM-DD.csv
      if (nombre.length() >= 14 && nombre.charAt(4) == '-' && nombre.charAt(7) == '-') {
        int y = nombre.substring(0, 4).toInt();
        int m = nombre.substring(5, 7).toInt();
        int d = nombre.substring(8, 10).toInt();

        if (y >= 2020 && m >= 1 && m <= 12 && d >= 1 && d <= 31) {
          DateTime finDelDia(y, m, d, 23, 59, 59);
          if (finDelDia.unixtime() < limite) {
            Serial.println("[SD] Eliminando: " + ruta);
            SD.remove(ruta);
          }
        }
      }
    }

    archivo = carpeta.openNextFile();
  }

  carpeta.close();
}

// =========================================================
// SD: CONTAR REGISTROS DEL DIA
// =========================================================
uint32_t contarRegistrosHoy() {
  if (!sdOK || !rtcOK) return 0;
  String ruta = archivoDelDia(rtc.now());
  if (!SD.exists(ruta)) return 0;

  File f = SD.open(ruta, FILE_READ);
  if (!f) return 0;

  uint32_t lineas = 0;
  while (f.available()) {
    String linea = f.readStringUntil('\n');
    linea.trim();
    if (linea.length() && !linea.startsWith("timestamp")) lineas++;
    yield();
  }
  f.close();
  return lineas;
}

// =========================================================
// CORS / HEADERS
// =========================================================
void agregarHeaders() {
  server.sendHeader("Access-Control-Allow-Origin", "*");
  server.sendHeader("Access-Control-Allow-Methods", "GET,POST,OPTIONS");
  server.sendHeader("Access-Control-Allow-Headers", "Content-Type");
  server.sendHeader("Cache-Control", "no-store");
}

// =========================================================
// API: ACTUAL
// =========================================================
void apiActual() {
  agregarHeaders();

  String json;
  json.reserve(300);
  json += "{";
  json += "\"created_at\":\"" + (rtcOK ? fechaHoraISO(fechaActual) : String("")) + "\",";
  json += "\"temperatura\":" + String(temperaturaActual, 2) + ",";
  json += "\"humedad\":" + String(humedadActual, 2) + ",";
  json += "\"presion\":" + String(presionActual, 2) + ",";
  json += "\"estado\":\"" + String(medicionValida ? "Normal" : "Error") + "\",";
  json += "\"bme280\":" + String(bmeOK ? "true" : "false") + ",";
  json += "\"rtc\":" + String(rtcOK ? "true" : "false") + ",";
  json += "\"sd\":" + String(sdOK ? "true" : "false") + ",";
  json += "\"wifi\":" + String(wifiOK ? "true" : "false");
  json += "}";

  server.send(200, "application/json", json);
}

// =========================================================
// API: STATUS
// =========================================================
void apiStatus() {
  agregarHeaders();
  String json;
  json.reserve(260);
  json += "{";
  json += "\"deviceId\":\"DATALOGGER-ESP32\",";
  json += "\"firmware\":\"1.0.0\",";
  json += "\"bme280\":" + String(bmeOK ? "true" : "false") + ",";
  json += "\"oled\":" + String(oledOK ? "true" : "false") + ",";
  json += "\"rtc\":" + String(rtcOK ? "true" : "false") + ",";
  json += "\"sd\":" + String(sdOK ? "true" : "false") + ",";
  json += "\"wifi\":" + String(wifiOK ? "true" : "false") + ",";
  json += "\"modo_ap\":" + String(modoAP ? "true" : "false") + ",";
  json += "\"uptime_ms\":" + String(millis()) + ",";
  json += "\"free_heap\":" + String(ESP.getFreeHeap());
  json += "}";
  server.send(200, "application/json", json);
}

// =========================================================
// API: STORAGE
// =========================================================
void apiStorage() {
  agregarHeaders();

  uint64_t card = sdOK ? SD.cardSize() : 0;
  uint64_t total = sdOK ? SD.totalBytes() : 0;
  uint64_t used = sdOK ? SD.usedBytes() : 0;
  if (rtcOK) {
    uint32_t hoy = identificadorDia(rtc.now());
    if (registrosDiaId != hoy) {
      registrosDiaId = hoy;
      registrosHoy = contarRegistrosHoy();
    }
  }
  uint32_t recordsToday = registrosHoy;

  String json;
  json.reserve(260);
  json += "{";
  json += "\"sd\":" + String(sdOK ? "true" : "false") + ",";
  json += "\"cardSizeBytes\":" + uint64String(card) + ",";
  json += "\"totalBytes\":" + uint64String(total) + ",";
  json += "\"usedBytes\":" + uint64String(used) + ",";
  json += "\"recordsToday\":" + String(recordsToday) + ",";
  json += "\"retentionDays\":" + String(retentionDays);
  json += "}";

  server.send(200, "application/json", json);
}

// =========================================================
// API: CONFIG GET/POST
// =========================================================
void apiConfigGET() {
  agregarHeaders();
  String json = "{\"samplingIntervalMs\":" + String(samplingIntervalMs) +
                ",\"retentionDays\":" + String(retentionDays) + "}";
  server.send(200, "application/json", json);
}

void apiConfigPOST() {
  agregarHeaders();
  bool cambio = false;

  if (server.hasArg("sampling_ms")) {
    unsigned long nuevo = strtoul(server.arg("sampling_ms").c_str(), nullptr, 10);
    if (nuevo >= 1000UL && nuevo <= 3600000UL) {
      samplingIntervalMs = nuevo;
      preferences.putULong("sampleMs", samplingIntervalMs);
      cambio = true;
    }
  }

  if (server.hasArg("retention_days")) {
    long nuevo = server.arg("retention_days").toInt();
    if (nuevo >= 0 && nuevo <= 3650) {
      retentionDays = (uint16_t)nuevo;
      preferences.putUShort("retDays", retentionDays);
      ultimoDiaLimpieza = 0;
      cambio = true;
    }
  }

  String json = "{\"ok\":" + String(cambio ? "true" : "false") +
                ",\"samplingIntervalMs\":" + String(samplingIntervalMs) +
                ",\"retentionDays\":" + String(retentionDays) + "}";
  server.send(200, "application/json", json);
}

// =========================================================
// CSV PARSER
// =========================================================
bool parsearLineaCSV(const String& linea, String& timestamp, String& temp, String& hum, String& pres) {
  int c1 = linea.indexOf(',');
  if (c1 < 0) return false;
  int c2 = linea.indexOf(',', c1 + 1);
  if (c2 < 0) return false;
  int c3 = linea.indexOf(',', c2 + 1);
  if (c3 < 0) return false;

  timestamp = linea.substring(0, c1);
  temp = linea.substring(c1 + 1, c2);
  hum = linea.substring(c2 + 1, c3);
  pres = linea.substring(c3 + 1);

  timestamp.trim();
  temp.trim();
  hum.trim();
  pres.trim();
  return true;
}

// =========================================================
// API: HISTORIAL
// /api/historial?days=1
// =========================================================
void apiHistorial() {
  agregarHeaders();

  if (!sdOK || !rtcOK) {
    server.send(503, "application/json", "{\"error\":\"SD o RTC no disponible\"}");
    return;
  }

  int dias = server.hasArg("days") ? server.arg("days").toInt() : 1;
  if (dias < 1) dias = 1;
  if (dias > 365) dias = 365;
  if (retentionDays > 0 && dias > retentionDays) dias = retentionDays;

  DateTime ahora = rtc.now();

  server.setContentLength(CONTENT_LENGTH_UNKNOWN);
  server.send(200, "application/json", "");
  server.sendContent("[");

  bool primero = true;

  for (int offset = dias - 1; offset >= 0; offset--) {
    DateTime fecha(ahora.unixtime() - ((uint32_t)offset * 86400UL));
    String ruta = archivoDelDia(fecha);
    if (!SD.exists(ruta)) continue;

    File archivo = SD.open(ruta, FILE_READ);
    if (!archivo) continue;

    bool primeraLinea = true;
    while (archivo.available()) {
      String linea = archivo.readStringUntil('\n');
      linea.trim();

      if (primeraLinea) {
        primeraLinea = false;
        if (linea.startsWith("timestamp")) continue;
      }

      if (!linea.length()) continue;

      String timestamp, temp, hum, pres;
      if (!parsearLineaCSV(linea, timestamp, temp, hum, pres)) continue;

      if (!primero) server.sendContent(",");
      primero = false;

      String item;
      item.reserve(160);
      item += "{\"created_at\":\"" + timestamp + "\",";
      item += "\"temperatura\":" + temp + ",";
      item += "\"humedad\":" + hum + ",";
      item += "\"presion\":" + pres + ",";
      item += "\"estado\":\"Normal\"}";
      server.sendContent(item);
      yield();
    }
    archivo.close();
  }

  server.sendContent("]");
}

// =========================================================
// API: EXPORT CSV
// /api/export?days=30
// =========================================================
void apiExportar() {
  agregarHeaders();

  if (!sdOK || !rtcOK) {
    server.send(503, "text/plain", "SD o RTC no disponible");
    return;
  }

  int dias = server.hasArg("days") ? server.arg("days").toInt() : (retentionDays == 0 ? 30 : retentionDays);
  if (dias < 1) dias = 1;
  if (dias > 365) dias = 365;
  if (retentionDays > 0 && dias > retentionDays) dias = retentionDays;

  server.sendHeader("Content-Disposition", "attachment; filename=\"datalogger.csv\"");
  server.setContentLength(CONTENT_LENGTH_UNKNOWN);
  server.send(200, "text/csv", "");
  server.sendContent("timestamp,temperatura,humedad,presion\r\n");

  DateTime ahora = rtc.now();

  for (int offset = dias - 1; offset >= 0; offset--) {
    DateTime fecha(ahora.unixtime() - ((uint32_t)offset * 86400UL));
    String ruta = archivoDelDia(fecha);
    if (!SD.exists(ruta)) continue;

    File archivo = SD.open(ruta, FILE_READ);
    if (!archivo) continue;

    bool primeraLinea = true;
    while (archivo.available()) {
      String linea = archivo.readStringUntil('\n');
      linea.trim();

      if (primeraLinea) {
        primeraLinea = false;
        if (linea.startsWith("timestamp")) continue;
      }

      if (!linea.length()) continue;
      server.sendContent(linea + "\r\n");
      yield();
    }
    archivo.close();
  }
}

// =========================================================
// API: REINICIO
// =========================================================
void apiReiniciar() {
  agregarHeaders();
  server.send(200, "application/json", "{\"ok\":true}");
  delay(400);
  ESP.restart();
}

// =========================================================
// STATIC FILES /www
// =========================================================
String tipoContenido(const String& path) {
  if (path.endsWith(".html")) return "text/html; charset=utf-8";
  if (path.endsWith(".css")) return "text/css; charset=utf-8";
  if (path.endsWith(".js")) return "application/javascript; charset=utf-8";
  if (path.endsWith(".json")) return "application/json";
  if (path.endsWith(".png")) return "image/png";
  if (path.endsWith(".jpg") || path.endsWith(".jpeg")) return "image/jpeg";
  if (path.endsWith(".svg")) return "image/svg+xml";
  if (path.endsWith(".ttf")) return "font/ttf";
  if (path.endsWith(".ico")) return "image/x-icon";
  return "application/octet-stream";
}

bool servirArchivoWeb(String path) {
  if (!sdOK) return false;
  if (path.indexOf("..") >= 0) return false;

  if (path == "/") path = "/index.html";
  String ruta = "/www" + path;

  if (!SD.exists(ruta)) return false;

  File archivo = SD.open(ruta, FILE_READ);
  if (!archivo || archivo.isDirectory()) {
    if (archivo) archivo.close();
    return false;
  }

  server.streamFile(archivo, tipoContenido(ruta));
  archivo.close();
  return true;
}

void paginaBasica() {
  String html = R"rawliteral(
<!doctype html><html lang="es"><head><meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1"><title>DataLogger</title></head>
<body style="font-family:Arial;max-width:760px;margin:40px auto;padding:20px">
<h1>DataLogger ESP32</h1><p>El ESP32 funciona, pero falta <code>/www/index.html</code> en la microSD.</p><p><a href="/admin">Cargar frontend sin lector de microSD</a></p>
<p><a href="/api/actual">/api/actual</a></p><p><a href="/api/status">/api/status</a></p><p><a href="/api/historial?days=1">/api/historial?days=1</a></p>
</body></html>)rawliteral";
  server.send(200, "text/html; charset=utf-8", html);
}

void manejarNoEncontrado() {
  if (server.method() == HTTP_OPTIONS) {
    agregarHeaders();
    server.send(204, "text/plain", "");
    return;
  }

  if (servirArchivoWeb(server.uri())) return;
  if (server.uri() == "/") {
    paginaBasica();
    return;
  }

  agregarHeaders();
  server.send(404, "application/json", "{\"error\":\"No encontrado\"}");
}

// =========================================================
// WIFI
// =========================================================
void iniciarWiFi() {
  Serial.println();
  Serial.println("Conectando WiFi...");

  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

  unsigned long inicio = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - inicio < 15000UL) {
    delay(500);
    Serial.print('.');
  }
  Serial.println();

  if (WiFi.status() == WL_CONNECTED) {
    wifiOK = true;
    modoAP = false;
    Serial.println("[OK] WiFi conectado");
    Serial.print("IP: ");
    Serial.println(WiFi.localIP());

    if (MDNS.begin("datalogger")) {
      MDNS.addService("http", "tcp", 80);
      Serial.println("URL: http://datalogger.local");
    }
  } else {
    Serial.println("[AVISO] No se pudo conectar al WiFi. Creando red propia...");
    WiFi.disconnect(true);
    delay(400);
    WiFi.mode(WIFI_AP);
    WiFi.softAP(AP_SSID, AP_PASSWORD);
    wifiOK = false;
    modoAP = true;
    Serial.print("Red: ");
    Serial.println(AP_SSID);
    Serial.print("IP: ");
    Serial.println(WiFi.softAPIP());
  }
}

void mantenerWiFi() {
  if (modoAP) return;
  if (millis() - ultimoChequeoWiFiMs < 30000UL) return;
  ultimoChequeoWiFiMs = millis();

  wifiOK = WiFi.status() == WL_CONNECTED;
  if (!wifiOK) {
    Serial.println("[WiFi] Reconectando...");
    WiFi.reconnect();
  }
}


// =========================================================
// ADMIN: SUBIR FRONTEND A /www SIN LECTOR DE microSD
// Abrir http://IP_DEL_ESP32/admin
// =========================================================
void paginaAdmin() {
  String html = R"rawliteral(
<!doctype html>
<html lang="es">
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>DataLogger - Cargar frontend</title>
<style>
body{font-family:Arial,sans-serif;max-width:760px;margin:35px auto;padding:20px;background:#f4f6f8;color:#162033}
.card{background:white;border:1px solid #dbe2ea;border-radius:14px;padding:20px;box-shadow:0 5px 20px rgba(0,0,0,.06)}
button{background:#003087;color:#fff;border:0;border-radius:9px;padding:11px 18px;font-weight:700;cursor:pointer}
input{margin:16px 0;width:100%}
#log{white-space:pre-wrap;background:#0d1726;color:#dce7f5;padding:12px;border-radius:9px;min-height:90px}
a{color:#003087}
</style>
</head>
<body>
<div class="card">
<h1>DataLogger ESP32</h1>
<h2>Cargar frontend a la microSD</h2>
<p>Selecciona todos los archivos que estan dentro de la carpeta <b>www</b> y presiona <b>Subir archivos</b>.</p>
<input id="files" type="file" multiple>
<button onclick="subir()">Subir archivos</button>
<p><a href="/">Abrir DataLogger</a></p>
<div id="log">Esperando archivos...</div>
</div>
<script>
async function subir(){
  const files=[...document.getElementById('files').files];
  const log=document.getElementById('log');
  if(!files.length){log.textContent='Selecciona los archivos de la carpeta www.';return;}
  log.textContent='';
  let ok=0;
  for(let i=0;i<files.length;i++){
    const f=files[i];
    log.textContent += `Subiendo ${i+1}/${files.length}: ${f.name} ... `;
    const fd=new FormData();
    fd.append('file',f,f.name);
    try{
      const r=await fetch('/api/upload',{method:'POST',body:fd});
      if(!r.ok) throw new Error(await r.text());
      log.textContent += 'OK\n';
      ok++;
    }catch(e){
      log.textContent += 'ERROR: '+e.message+'\n';
    }
  }
  log.textContent += `\nListo: ${ok}/${files.length} archivos subidos.\n`;
  if(ok===files.length) log.textContent += 'Ya podes abrir la pagina principal.';
}
</script>
</body>
</html>
)rawliteral";
  server.send(200, "text/html; charset=utf-8", html);
}

void manejarUploadWeb() {
  HTTPUpload& upload = server.upload();

  if (upload.status == UPLOAD_FILE_START) {
    uploadWebOK = false;

    if (!sdOK) {
      Serial.println("[UPLOAD] microSD no disponible");
      return;
    }

    String nombre = upload.filename;
    nombre.replace("\\", "/");
    int slash = nombre.lastIndexOf('/');
    if (slash >= 0) nombre = nombre.substring(slash + 1);

    // Evita rutas maliciosas y nombres vacios.
    nombre.replace("..", "");
    if (!nombre.length()) {
      Serial.println("[UPLOAD] Nombre de archivo invalido");
      return;
    }

    if (!SD.exists("/www")) SD.mkdir("/www");

    String ruta = "/www/" + nombre;
    if (SD.exists(ruta)) SD.remove(ruta);

    archivoUploadWeb = SD.open(ruta, FILE_WRITE);
    if (!archivoUploadWeb) {
      Serial.println("[UPLOAD] No se pudo abrir " + ruta);
      return;
    }

    uploadWebOK = true;
    Serial.println("[UPLOAD] Iniciando: " + ruta);
  }
  else if (upload.status == UPLOAD_FILE_WRITE) {
    if (uploadWebOK && archivoUploadWeb) {
      size_t escritos = archivoUploadWeb.write(upload.buf, upload.currentSize);
      if (escritos != upload.currentSize) uploadWebOK = false;
    }
  }
  else if (upload.status == UPLOAD_FILE_END) {
    if (archivoUploadWeb) archivoUploadWeb.close();
    Serial.print("[UPLOAD] Fin: ");
    Serial.print(upload.filename);
    Serial.print(" (");
    Serial.print(upload.totalSize);
    Serial.println(" bytes)");
  }
  else if (upload.status == UPLOAD_FILE_ABORTED) {
    if (archivoUploadWeb) archivoUploadWeb.close();
    uploadWebOK = false;
    Serial.println("[UPLOAD] Cancelado");
  }
}

void responderUploadWeb() {
  agregarHeaders();

  if (!sdOK) {
    server.send(503, "application/json", "{\"ok\":false,\"error\":\"microSD no disponible\"}");
    return;
  }

  if (uploadWebOK) {
    server.send(200, "application/json", "{\"ok\":true}");
  } else {
    server.send(500, "application/json", "{\"ok\":false,\"error\":\"No se pudo guardar el archivo\"}");
  }
}

// =========================================================
// SERVIDOR
// =========================================================
void iniciarServidor() {
  server.on("/admin", HTTP_GET, paginaAdmin);
  server.on("/api/upload", HTTP_POST, responderUploadWeb, manejarUploadWeb);
  server.on("/api/actual", HTTP_GET, apiActual);
  server.on("/api/status", HTTP_GET, apiStatus);
  server.on("/api/storage", HTTP_GET, apiStorage);
  server.on("/api/config", HTTP_GET, apiConfigGET);
  server.on("/api/config", HTTP_POST, apiConfigPOST);
  server.on("/api/historial", HTTP_GET, apiHistorial);
  server.on("/api/export", HTTP_GET, apiExportar);
  server.on("/api/reiniciar", HTTP_POST, apiReiniciar);
  server.onNotFound(manejarNoEncontrado);
  server.begin();
  Serial.println("[OK] Servidor HTTP iniciado");
}

// =========================================================
// SETUP
// =========================================================
void setup() {
  Serial.begin(115200);
  delay(1500);

  Serial.println();
  Serial.println("==============================");
  Serial.println(" DATALOGGER ESP32 - FINAL");
  Serial.println("==============================");

  preferences.begin("datalogger", false);
  samplingIntervalMs = preferences.getULong("sampleMs", 60000UL);
  retentionDays = preferences.getUShort("retDays", 30);

  Serial.print("Intervalo: ");
  Serial.print(samplingIntervalMs);
  Serial.println(" ms");
  Serial.print("Retencion: ");
  Serial.print(retentionDays);
  Serial.println(retentionDays == 0 ? " (sin limite)" : " dias");

  // I2C
  Wire.begin(I2C_SDA, I2C_SCL);
  Wire.setClock(100000);

  // BME280
  Serial.println("Probando BME280...");
  bmeOK = bme.begin(BME_ADDRESS);
  Serial.println(bmeOK ? "[OK] BME280" : "[ERROR] BME280");

  // OLED
  Serial.println("Probando OLED...");
  oledOK = display.begin(SSD1306_SWITCHCAPVCC, OLED_ADDRESS);
  Serial.println(oledOK ? "[OK] OLED" : "[ERROR] OLED");
  if (oledOK) {
    display.clearDisplay();
    display.setTextColor(SSD1306_WHITE);
    display.setTextSize(1);
    display.setCursor(0, 0);
    display.println("DATALOGGER");
    display.println("Iniciando...");
    display.display();
  }

  // RTC
  Serial.println("Probando DS3231...");
  rtcOK = rtc.begin();
  Serial.println(rtcOK ? "[OK] DS3231" : "[ERROR] DS3231");
  if (rtcOK && rtc.lostPower()) {
    Serial.println("RTC perdio la hora. Ajustando a hora de compilacion...");
    rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));
  }

  // SD
  Serial.println("Probando microSD...");
  SPI.begin(SD_SCK, SD_MISO, SD_MOSI, SD_CS);
  sdOK = SD.begin(SD_CS, SPI, 400000);
  Serial.println(sdOK ? "[OK] microSD" : "[ERROR] microSD");
  if (sdOK) {
    if (!SD.exists("/data")) SD.mkdir("/data");
    if (!SD.exists("/www")) SD.mkdir("/www");
    if (rtcOK) {
      registrosDiaId = identificadorDia(rtc.now());
      registrosHoy = contarRegistrosHoy();
    }
  }

  iniciarWiFi();
  iniciarServidor();

  tomarMedicion(true);
  limpiarArchivosViejos();
  ultimaMedicionMs = millis();

  Serial.println();
  Serial.println("==============================");
  Serial.print("BME280:  "); Serial.println(bmeOK ? "OK" : "ERROR");
  Serial.print("OLED:    "); Serial.println(oledOK ? "OK" : "ERROR");
  Serial.print("DS3231:  "); Serial.println(rtcOK ? "OK" : "ERROR");
  Serial.print("microSD: "); Serial.println(sdOK ? "OK" : "ERROR");
  Serial.println("==============================");
}

// =========================================================
// LOOP
// =========================================================
void loop() {
  server.handleClient();
  mantenerWiFi();

  if (millis() - ultimaMedicionMs >= samplingIntervalMs) {
    ultimaMedicionMs = millis();
    tomarMedicion(true);
    limpiarArchivosViejos();
  }

  delay(2);
}
