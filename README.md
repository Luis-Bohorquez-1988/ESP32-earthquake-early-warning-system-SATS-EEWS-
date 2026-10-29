ESPAÑOL / SPANISH

# 🌍 Sistema de Alarma Temprana Sísmica con ESP32

Un sistema IoT de monitoreo sísmico de bajo costo basado en **ESP32**, un acelerómetro **MPU6050**, una tarjeta Micro SD (para reproducción de audio directa sin errores de memoria), un amplificador **PAM8403** y un **Dashboard Web en tiempo real** con notificaciones automáticas a **Telegram**.

---

## 🛠️ Componentes Hardware Utilizados
* **Microcontrolador:** ESP32 NodeMCU.
* **Sensor de Movimiento:** Acelerómetro MPU6050 (I2C).
* **Almacenamiento:** Módulo lector de tarjeta Micro SD (SPI).
* **Audio:** Amplificador PAM8403 y altavoz.
* **Alimentación recomendada:** Cargador de pared USB (mínimo 1A o 2A para evitar caídas de voltaje).

---

## 📌 Conexión de Pines

### 1. MPU6050 (I2C)
* **SCL** ➡️ GPIO 22
* **SDA** ➡️ GPIO 21
* **VCC** ➡️ 3.3V / 5V
* **GND** ➡️ GND

### 2. Módulo Micro SD (SPI)
* **CS**  ➡️ GPIO 5
* **MOSI** ➡️ GPIO 23
* **MISO** ➡️ GPIO 19
* **SCK**  ➡️ GPIO 18
* **VCC**  ➡️ 3.3V *(Crucial conectarlo a 3.3V)*
* **GND**  ➡️ GND

### 3. Audio / DAC
* **Audio Out** ➡️ GPIO 25 (Conectado a la entrada del PAM8403)
* **GND Común** ➡️ Unir todas las tierras (ESP32, SD, Amplificador)

---

## ⚙️ Configuración del Software (Arduino IDE)
1. Selecciona la placa **ESP32 Dev Module**.
2. En el menú de particiones, selecciona: **"Huge APP (3MB No OTA/1MB SPIFFS)"**.
3. En SATS_EEWS.ino, en las lineas 11 y 12, modifica las entradas con el SSID y PASSWORD de tu red wifi (2.4Ghz solamente)
4. Asegúrate de colocar en tu tarjeta Micro SD un archivo de audio llamado exactamente `alerta.wav` o el nombre de tu preferencia siempre y cuando coincida EXACTAMENTE con el código y el archivo de audio este codificado en **PCM de 8 bits, Mono**.
5. Modifica las credenciales de tu red Wi-Fi y los datos de tu Bot de Telegram en el código fuente antes de subirlo.

---

## 🚀 Características Principales
* **Cero Errores OOM:** Reproducción de audio directa desde la SD bloque a bloque mediante los registros DAC del ESP32, evitando desbordamientos de memoria RAM.
* **Filtros Antirruido:** Algoritmo STA/LTA con umbrales de aceleración (`> 150 mg` y `Mw >= 4.5`) para filtrar vibraciones cotidianas y evitar falsos positivos.
* **Dashboard Web Local:** Servidor web integrado para visualizar gráficos en vivo (Ejes X, Y, Z), magnitud estimada e historial de eventos.
* **Notificaciones Telegram:** Envío automático de alertas instantáneas a tu teléfono celular ante sismos reales.

English/Inglés

# 🌍 ESP32 Seismic Early Warning System

A low-cost IoT seismic monitoring system powered by an **ESP32**, an **MPU6050** accelerometer, a Micro SD card module (for direct audio playback without out-of-memory errors), a **PAM8403** audio amplifier, and a **Real-Time Web Dashboard** featuring automatic **Telegram notifications**.

---

## 🛠️ Hardware Components
* **Microcontroller:** ESP32 NodeMCU.
* **Motion Sensor:** MPU6050 Accelerometer (I2C).
* **Storage:** Micro SD Card Reader Module (SPI).
* **Audio:** PAM8403 Amplifier and Speaker.
* **Recommended Power Supply:** Good quality USB wall charger (minimum 1A or 2A to prevent voltage drops).

---

## 📌 Pinout Connections

### 1. MPU6050 (I2C)
* **SCL** ➡️ GPIO 22
* **SDA** ➡️ GPIO 21
* **VCC** ➡️ 3.3V / 5V
* **GND** ➡️ GND

### 2. Micro SD Module (SPI)
* **CS**  ➡️ GPIO 5
* **MOSI** ➡️ GPIO 23
* **MISO** ➡️ GPIO 19
* **SCK**  ➡️ GPIO 18
* **VCC**  ➡️ 3.3V *(Crucial: connect to 3.3V)*
* **GND**  ➡️ GND

### 3. Audio / DAC
* **Audio Out** ➡️ GPIO 25 (Connected to the PAM8403 input)
* **Common GND** ➡️ Tie all grounds together (ESP32, SD card, and Amplifier)

---

## ⚙️ Software Setup (Arduino IDE)
1. Select board: **ESP32 Dev Module**.
2. Partition Scheme: Select **"Huge APP (3MB No OTA/1MB SPIFFS)"**.
3. In lines 11 and 12 on SATS_EEWS.ino, modify the SSID and PASSWORD entries according your WIFI network (2.4Ghz Only)
4. Place an audio file named exactly `alert.wav`  (or any name of your choice, provided it matches the code exactly), please be sure on your Micro SD card formatted and audfile encoded as **8-bit, Mono PCM**.
5. Update your Wi-Fi credentials and Telegram Bot Token/Chat ID in the source code before uploading.

---

## 🚀 Key Features
* **Zero OOM Errors:** Direct block-by-block audio streaming from the SD card using the ESP32 internal DAC registers, avoiding heavy libraries and RAM exhaustion.
* **Anti-Noise Filters:** STA/LTA algorithm combined with acceleration thresholds (`> 150 mg` and `Mw >= 4.5`) to filter out daily household vibrations and prevent false positives.
* **Local Web Dashboard:** Built-in web server displaying live charts (X, Y, Z axes), estimated magnitude, and event history.
* **Telegram Notifications:** Automatic instant alerts sent directly to your phone when a real seismic event occurs.
