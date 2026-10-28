#include <WiFi.h>
#include <WebServer.h>
#include <Wire.h>
#include <time.h>
#include "SD.h"
#include "SPI.h"
#include <driver/dac.h>
#include "soc/soc.h"
#include "soc/rtc_cntl_reg.h"

const char* ssid = "Familia Bohórquez";
const char* password = "1015408630Aa"; 

const char* ntpServer = "pool.ntp.org";
const long gmtOffset_sec = -5 * 3600;      
const int daylightOffset_sec = 0;

WebServer server(80);

const int MPU_addr = 0x68;
int16_t AcX, AcY, AcZ;

float baselineX = 0, baselineY = 0, baselineZ = 0;

// Variables globales
float mgX = 0, mgY = 0, mgZ = 0;
float aceleracionResultante = 0;
float magnitudMw = 0.0; 
String estadoIntensidad = "Nivel Bajo (Estable)";
bool alertaActiva = false;

// --- CONFIGURACIÓN DE SD Y AUDIO DIRECTO ---
const int SD_CS = 5;
const char* archivoAudio = "/Alerta.wav";
unsigned long ultimaReproduccionAudio = 0;

// --- ALGORITMO STA/LTA Y FILTRO DE PERSISTENCIA ---
const int TAM_BUFFER = 50; 
float historialAceleracion[TAM_BUFFER];
int indiceBuffer = 0;
int contadorPersistenciaP = 0; 
unsigned long ultimaCalibracionMs = 0;
const unsigned long intervaloCalibracion = 300000; // 5 minutos

// --- CONTROL DE REINICIO PREVENTIVO (3 HORAS) ---
unsigned long ultimoReinicioMs = 0;
const unsigned long intervaloReinicio = 10800000UL; 

unsigned long ultimaLecturaMpuMs = 0;

struct EventoSeismico {
  String hora;
  String descripcion;
};

const int MAX_HISTORIAL = 5;
EventoSeismico historial[MAX_HISTORIAL];
int totalEventos = 0;
unsigned long ultimoRegistroMs = 0;

String obtenerHoraActual() {
  struct tm timeinfo;
  if (!getLocalTime(&timeinfo)) return "00:00:00";
  char buffer[9];
  strftime(buffer, sizeof(buffer), "%H:%M:%S", &timeinfo);
  return String(buffer);
}

String obtenerFechaActual() {
  struct tm timeinfo;
  if (!getLocalTime(&timeinfo)) return "00/00/0000";
  char buffer[11];
  strftime(buffer, sizeof(buffer), "%d/%m/%Y", &timeinfo);
  return String(buffer);
}

void registrarEvento(String desc, float mag) {
  unsigned long ahora = millis();
  if (ahora - ultimoRegistroMs > 20000) { 
    for (int i = MAX_HISTORIAL - 1; i > 0; i--) {
      historial[i] = historial[i - 1];
    }
    historial[0].hora = obtenerHoraActual();
    historial[0].descripcion = desc + " (" + String(mag, 1) + " Mw)";
    if (totalEventos < MAX_HISTORIAL) totalEventos++;
    ultimoRegistroMs = ahora;
  }
}

void leerMPUSeguro() {
  Wire.beginTransmission(MPU_addr);
  Wire.write(0x3B); 
  Wire.endTransmission(false);
  Wire.requestFrom(MPU_addr, 6, true);
  
  if (Wire.available() >= 6) {
    uint8_t xh = Wire.read();
    uint8_t xl = Wire.read();
    AcX = (int16_t)((xh << 8) | xl);

    uint8_t yh = Wire.read();
    uint8_t yl = Wire.read();
    AcY = (int16_t)((yh << 8) | yl);

    uint8_t zh = Wire.read();
    uint8_t zl = Wire.read();
    AcZ = (int16_t)((zh << 8) | zl);
  }
}

void calibrarBaseline() {
  long sumX = 0, sumY = 0, sumZ = 0;
  int muestras = 100;
  for (int i = 0; i < muestras; i++) {
    leerMPUSeguro();
    sumX += AcX;
    sumY += AcY;
    sumZ += AcZ;
    delay(5);
  }
  baselineX = (float)(sumX / muestras) / 16.384;
  baselineY = (float)(sumY / muestras) / 16.384;
  baselineZ = (float)(sumZ / muestras) / 16.384;
}

// Reproducción directa bloque a bloque con velocidad ajustada a 70 us
void reproducirAlertaAudio() {
  unsigned long ahora = millis();
  if (ahora - ultimaReproduccionAudio > 10000) {
    if (SD.exists(archivoAudio)) {
      File audioFile = SD.open(archivoAudio, FILE_READ);
      if (audioFile) {
        Serial.println("Reproduciendo Alerta.wav directamente desde la SD...");
        
        dac_output_enable(DAC_CHANNEL_1);
        audioFile.seek(44); // Omitir cabecera WAV

        while (audioFile.available()) {
          uint8_t sample = audioFile.read();
          dac_output_voltage(DAC_CHANNEL_1, sample);
          delayMicroseconds(70); // Velocidad óptima configurada
        }

        audioFile.close();
        dac_output_voltage(DAC_CHANNEL_1, 0); 
        ultimaReproduccionAudio = ahora;
        Serial.println("Reproducción de audio finalizada.");
      } else {
        Serial.println("Error al abrir el archivo Alerta.wav.");
      }
    } else {
      Serial.println("El archivo Alerta.wav no existe en la tarjeta SD.");
    }
  }
}

void procesarSismografo() {
  unsigned long ahora = millis();

  if (ahora - ultimoReinicioMs >= intervaloReinicio) {
    Serial.println("Ejecutando reinicio preventivo de 3 horas...");
    delay(100);
    ESP.restart();
  }

  if (ahora - ultimaLecturaMpuMs >= 20) {
    ultimaLecturaMpuMs = ahora;

    leerMPUSeguro();
    mgX = (float)AcX / 16.384;
    mgY = (float)AcY / 16.384;
    mgZ = (float)AcZ / 16.384;

    float deltaX = abs(mgX - baselineX);
    float deltaY = abs(mgY - baselineY);
    float deltaZ = abs(mgZ - baselineZ);
    aceleracionResultante = sqrt((deltaX * deltaX) + (deltaY * deltaY) + (deltaZ * deltaZ));

    if (ahora - ultimaCalibracionMs >= intervaloCalibracion) {
      if (aceleracionResultante < 10.0) {
        calibrarBaseline();
        ultimaCalibracionMs = ahora;
      } else {
        ultimaCalibracionMs = ahora - intervaloCalibracion + 30000; 
      }
    }

    historialAceleracion[indiceBuffer] = aceleracionResultante;
    indiceBuffer = (indiceBuffer + 1) % TAM_BUFFER;

    float sumaCorto = 0;
    float sumaLargo = 0;
    int muestrasCorto = 5;  
    int muestrasLargo = 50; 

    for (int i = 0; i < muestrasLargo; i++) {
      int idx = (indiceBuffer - 1 - i + TAM_BUFFER) % TAM_BUFFER;
      sumaLargo += historialAceleracion[idx];
      if (i < muestrasCorto) {
        sumaCorto += historialAceleracion[idx];
      }
    }

    float sta = sumaCorto / muestrasCorto;
    float lta = sumaLargo / muestrasLargo;
    if (lta < 1.0) lta = 1.0; 

    float relacionStaLta = sta / lta;
    
    // Escala ajustada para requerir mayor aceleración real antes de pasar de 4.5 Mw
    magnitudMw = (aceleracionResultante / 150.0) * 4.0;
    if (magnitudMw < 0.1) {
      magnitudMw = 0.0; 
    }

    // Filtros elevados para evitar falsos positivos con movimientos cotidianos menores
    if (relacionStaLta > 2.5 && aceleracionResultante > 150.0) {
      contadorPersistenciaP++; 
      
      if (contadorPersistenciaP >= 8) {
        if (magnitudMw >= 4.5) {
          estadoIntensidad = "¡ALERTA SÍSMICA MAYOR! (Mw " + String(magnitudMw, 1) + ")";
          alertaActiva = true;
          registrarEvento("Sismo Mayor Detectado", magnitudMw);
          reproducirAlertaAudio(); 
        } else {
          estadoIntensidad = "Sismo Menor / Percusivo (Mw " + String(magnitudMw, 1) + ")";
          alertaActiva = false;
          registrarEvento("Ondas P (Menor)", magnitudMw);
        }
        contadorPersistenciaP = 0; 
      }
      
    } else if (aceleracionResultante > 300.0) {
      if (magnitudMw >= 4.5) {
        estadoIntensidad = "¡ALERTA CRÍTICA! Impacto S Fuerte";
        alertaActiva = true;
        registrarEvento("Impacto Sismo Fuerte", magnitudMw);
        reproducirAlertaAudio(); 
      } else {
        estadoIntensidad = "Nivel Moderado (Menor a 4.5 Mw)";
        alertaActiva = false;
      }
      contadorPersistenciaP = 0;
    } else {
      estadoIntensidad = "Nivel Bajo (Estable)";
      alertaActiva = false;
      if (contadorPersistenciaP > 0) contadorPersistenciaP--; 
    }
  }
}

void handleData() {
  String json = "{";
  json += "\"x\":" + String(mgX, 1) + ",";
  json += "\"y\":" + String(mgY, 1) + ",";
  json += "\"z\":" + String(mgZ, 1) + ",";
  json += "\"magnitud\":" + String(magnitudMw, 1) + ",";
  json += "\"intensity\":\"" + estadoIntensidad + "\",";
  json += "\"alert\":" + String(alertaActiva ? "true" : "false") + ",";
  json += "\"fecha\":\"" + obtenerFechaActual() + "\",";
  json += "\"hora\":\"" + obtenerHoraActual() + "\",";
  
  json += "\"historial\":[";
  for (int i = 0; i < totalEventos; i++) {
    json += "{\"hora\":\"" + historial[i].hora + "\",\"desc\":\"" + historial[i].descripcion + "\"}";
    if (i < totalEventos - 1) json += ",";
  }
  json += "]";
  json += "}";

  server.send(200, "application/json", json);
}

void handleRoot() {
  String html = "<!DOCTYPE html>";
  html += "<html lang=\"es\"><head><meta charset=\"UTF-8\">";
  html += "<meta name=\"viewport\" content=\"width=device-width, initial-scale=1.0\">";
  html += "<title>Sistema de Alarma Temprana Sísmica</title>";
  html += "<script src=\"https://cdn.jsdelivr.net/npm/chart.js\"></script>";
  html += "<style>";
  html += "body { font-family: 'Segoe UI', Arial, sans-serif; background-color: #0d1117; color: #c9d1d9; margin: 0; padding: 20px; text-align: center; }";
  html += ".container { max-width: 600px; margin: auto; background: #161b22; padding: 20px; border-radius: 16px; box-shadow: 0 8px 24px rgba(0,0,0,0.6); border: 1px solid #30363d; }";
  html += "h2 { color: #58a6ff; margin-bottom: 15px; font-size: 22px; }";
  html += ".status-bar { display: flex; justify-content: space-between; background: #21262d; padding: 10px 15px; border-radius: 8px; font-size: 13px; margin-bottom: 15px; border: 1px solid #30363d; }";
  html += ".datetime-bar { background: #21262d; padding: 8px; border-radius: 8px; font-size: 14px; font-family: monospace; color: #58a6ff; margin-bottom: 15px; border: 1px solid #30363d; }";
  html += ".axis-grid { display: grid; grid-template-columns: repeat(3, 1fr); gap: 10px; margin-bottom: 15px; }";
  html += ".axis-box { background: #21262d; padding: 10px; border-radius: 8px; border: 1px solid #30363d; }";
  html += ".main-metric { background: #21262d; padding: 15px; border-radius: 8px; border: 1px solid #30363d; margin-bottom: 15px; }";
  html += ".label { font-size: 13px; color: #8b949e; font-weight: bold; }";
  html += ".value { font-size: 15px; font-family: monospace; color: #3fb950; margin-top: 4px; }";
  html += ".metric-value { font-size: 24px; font-family: monospace; color: #58a6ff; margin-top: 5px; font-weight: bold; }";
  html += ".chart-container { background: #21262d; padding: 15px; border-radius: 10px; border: 1px solid #30363d; margin-bottom: 15px; }";
  html += ".alert-box { background: #21262d; padding: 12px; border-radius: 8px; border: 1px solid #30363d; display: flex; justify-content: space-between; align-items: center; margin-bottom: 10px; font-size: 13px; }";
  html += ".history-box { background: #21262d; padding: 12px; border-radius: 8px; border: 1px solid #30363d; text-align: left; margin-top: 15px; }";
  html += ".history-title { font-size: 13px; color: #8b949e; font-weight: bold; margin-bottom: 8px; text-align: center; }";
  html += "ul { margin: 0; padding-left: 20px; font-size: 13px; font-family: monospace; color: #c9d1d9; }";
  html += "li { margin-bottom: 4px; }";
  html += ".indicator { width: 12px; height: 12px; border-radius: 50%; background: #3fb950; box-shadow: 0 0 8px #3fb950; transition: background 0.3s; }";
  html += "</style></head><body>";
  
  html += "<div class=\"container\">";
  html += "<h2>Sistema de Alarma Temprana Sísmica</h2>";
  html += "<div class=\"status-bar\"><span>CONECTADO: Familia Bohórquez</span><span>Filtro Magnitud Mw >= 4.5</span></div>";
  html += "<div class=\"datetime-bar\"><span id=\"txtFecha\">--/--/----</span> &nbsp;|&nbsp; <span id=\"txtHora\">--:--:--</span></div>";
  html += "<div class=\"main-metric\"><div class=\"label\">MAGNITUD ESTIMADA DEL SISMO</div><div id=\"valMagnitud\" class=\"metric-value\">Mw 0.0</div></div>";

  html += "<div class=\"axis-grid\">";
  html += "<div class=\"axis-box\"><div class=\"label\">Eje X</div><div id=\"valX\" class=\"value\">0.0 mg</div></div>";
  html += "<div class=\"axis-box\"><div class=\"label\">Eje Y</div><div id=\"valY\" class=\"value\">0.0 mg</div></div>";
  html += "<div class=\"axis-box\"><div class=\"label\">Eje Z</div><div id=\"valZ\" class=\"value\">0.0 mg</div></div>";
  html += "</div>";

  html += "<div class=\"chart-container\"><canvas id=\"seismicChart\"></canvas></div>";
  html += "<div class=\"alert-box\"><span id=\"txtIntensity\">ESTADO: Nivel Bajo (Estable)</span><div id=\"indIntensity\" class=\"indicator\"></div></div>";
  html += "<div class=\"alert-box\"><span>ALARMA DE AUDIO: En Espera</span><div id=\"indAudio\" class=\"indicator\"></div></div>";

  html += "<div class=\"history-box\"><div class=\"history-title\">HISTORIAL DE MOVIMIENTOS NOTABLES</div><ul id=\"listaHistorial\"><li>Sin eventos recientes</li></ul></div>";
  html += "</div>";

  html += "<script>";
  html += "const ctx = document.getElementById('seismicChart').getContext('2d');";
  html += "const maxDataPoints = 40;";
  html += "const chart = new Chart(ctx, {";
  html += "  type: 'line',";
  html += "  data: { labels: Array(maxDataPoints).fill(''), datasets: [";
  html += "    { label: 'X', data: Array(maxDataPoints).fill(0), borderColor: '#3fb950', borderWidth: 2, tension: 0.2, pointRadius: 0 },";
  html += "    { label: 'Y', data: Array(maxDataPoints).fill(0), borderColor: '#d29922', borderWidth: 2, tension: 0.2, pointRadius: 0 },";
  html += "    { label: 'Z', data: Array(maxDataPoints).fill(0), borderColor: '#58a6ff', borderWidth: 2, tension: 0.2, pointRadius: 0 }";
  html += "  ]},";
  html += "  options: { responsive: true, animation: false, scales: { y: { grid: { color: '#30363d' }, ticks: { color: '#8b949e' } }, x: { grid: { display: false }, ticks: { display: false } } }, plugins: { legend: { labels: { color: '#c9d1d9', boxWidth: 12 } } } }";
  html += "});";

  html += "setInterval(async () => {";
  html += "  try {";
  html += "    let response = await fetch('/data');";
  html += "    let data = await response.json();";
  html += "    document.getElementById('txtFecha').innerText = data.fecha;";
  html += "    document.getElementById('txtHora').innerText = data.hora;";
  html += "    document.getElementById('valX').innerText = data.x.toFixed(1) + ' mg';";
  html += "    document.getElementById('valY').innerText = data.y.toFixed(1) + ' mg';";
  html += "    document.getElementById('valZ').innerText = data.z.toFixed(1) + ' mg';";
  html += "    document.getElementById('valMagnitud').innerText = 'Mw ' + data.magnitud.toFixed(1);";
  html += "    document.getElementById('txtIntensity').innerText = 'ESTADO: ' + data.intensity;";
  html += "    let ind = document.getElementById('indIntensity');";
  html += "    let indAudio = document.getElementById('indAudio');";
  html += "    if(data.alert) {";
  html += "      ind.style.backgroundColor = '#f85149'; ind.style.boxShadow = '0 0 12px #f85149';";
  html += "      indAudio.style.backgroundColor = '#f85149'; indAudio.style.boxShadow = '0 0 12px #f85149';";
  html += "    } else {";
  html += "      ind.style.backgroundColor = '#3fb950'; ind.style.boxShadow = '0 0 8px #3fb950';";
  html += "      indAudio.style.backgroundColor = '#3fb950'; indAudio.style.boxShadow = '0 0 8px #3fb950';";
  html += "    }";
  html += "    let lista = document.getElementById('listaHistorial');";
  html += "    if(data.historial && data.historial.length > 0) {";
  html += "      let htmlList = '';";
  html += "      data.historial.forEach(ev => { htmlList += `<li>[${ev.hora}] ${ev.desc}</li>`; });";
  html += "      lista.innerHTML = htmlList;";
  html += "    }";
  html += "    chart.data.datasets[0].data.shift(); chart.data.datasets[0].data.push(data.x);";
  html += "    chart.data.datasets[1].data.shift(); chart.data.datasets[1].data.push(data.y);";
  html += "    chart.data.datasets[2].data.shift(); chart.data.datasets[2].data.push(data.z);";
  html += "    chart.update('none');";
  html += "  } catch (err) { console.error(err); }";
  html += "}, 150);";
  html += "</script></body></html>";

  server.send(200, "text/html", html);
}

void setup() {
  WRITE_PERI_REG(RTC_CNTL_BROWN_OUT_REG, 0); // Desactiva el detector de bajones de voltaje
  Serial.begin(115200);
  Wire.begin(21, 22);

  // Inicializar MPU6050
  Wire.beginTransmission(MPU_addr);
  Wire.write(0x6B);
  Wire.write(0);
  Wire.endTransmission(true);
  delay(500);

  // Inicializar Tarjeta SD (SPI)
  if (!SD.begin(SD_CS)) {
    Serial.println("Error: No se pudo inicializar la tarjeta Micro SD.");
  } else {
    Serial.println("Tarjeta Micro SD inicializada correctamente.");
  }

  calibrarBaseline();
  
  for(int i=0; i<TAM_BUFFER; i++) historialAceleracion[i] = 0.0;
  ultimaCalibracionMs = millis();
  ultimoReinicioMs = millis();
  ultimaLecturaMpuMs = millis();

  WiFi.begin(ssid, password);
  int intentos = 0;
  while (WiFi.status() != WL_CONNECTED && intentos < 20) {
    delay(500);
    intentos++;
  }
  
  if (WiFi.status() == WL_CONNECTED) {
    configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);
  }

  server.on("/", handleRoot);
  server.on("/data", handleData);
  server.begin();
}

void loop() {
  server.handleClient();
  procesarSismografo();
}