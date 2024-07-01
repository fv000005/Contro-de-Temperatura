#include <Arduino.h>
#include <WiFi.h>
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <SPIFFS.h>
#include <Arduino_JSON.h>
#include <Adafruit_GFX.h>
#include <Adafruit_ST7735.h>
#include <OneWire.h>
#include <DallasTemperature.h>
#include <QuickPID.h>

// Credenciales WiFi uja
const char* ssid = "TP-LINK_7794";
const char* password = "00280549";
//casa
//const char* ssid     = "vodafone5D90_24G";
//const char* password = "UJYZGYCJZLN2NQ";

// Crear objeto AsyncWebServer en el puerto 80
AsyncWebServer server(80);

// Crear objeto WebSocket
AsyncWebSocket ws("/ws");

// Variables JSON para lecturas del sensor
JSONVar readings;

// Timer variables
unsigned long tiempoInicio_min = millis();
unsigned long tiempoInicio = millis();
unsigned long timerDelay = 30000;

// Pin y configuración del sensor de temperatura
const int pinDatos = 4;
OneWire oneWire(pinDatos);
DallasTemperature sensors(&oneWire);
// Definir las direcciones de los sensores
DeviceAddress sensor1 = {0x28, 0xBC, 0x9D, 0x43, 0xD4, 0x24, 0x1C, 0x14};
DeviceAddress sensor2 = {0x28, 0xA2, 0x51, 0x43, 0xD4, 0x7E, 0x2D, 0x2F};
DeviceAddress sensor3 = {0x28, 0xCE, 0x94, 0x43, 0xD4, 0x2B, 0x5F, 0xAD};
DeviceAddress sensor4 = {0x28, 0x97, 0x1C, 0x43, 0xD4, 0x78, 0x58, 0x41};
DeviceAddress sensor5 = {0x28, 0xE1, 0x47, 0x6A, 0x0A, 0x00, 0x00, 0x59};
DeviceAddress sensor6 = {0x28, 0x0D, 0xBE, 0x69, 0x0A, 0x00, 0x00, 0xD0};

// Variables para almacenar las temperaturas
float t1 = 0.0;
float t2 = 0.0;
float t3 = 0.0;
float t4 = 0.0;
float t5 = 0.0;
float t6 = 0.0;

// Interrupción y ciclo del LED
#define PIN_PWM 19 // Pin para generar PWM
#define PIN_venti 22 // pin ventilador
bool etadoventi = false;
#define PERIODO_PWM_MICROSECONDS 3600000000UL // 10 minutos en microsegundos (agregar 0 ceros más)
hw_timer_t *timer = NULL; // Puntero al temporizador
volatile bool cambiarEstado = false; // Bandera para indicar si se debe cambiar el estado del pin
float setpoint = 29;
float temperatura = 0;
float ciclo = 25;
int ciclomapeado;
float Kp = 0.05; // Ganancia proporcional
float Ki = 0.01; // Ganancia integral
float Kd = 1; // Ganancia derivativa
QuickPID myPID(&t5, &ciclo, &setpoint, Kp, Ki, Kd, QuickPID::pMode::pOnError, QuickPID::dMode::dOnMeas, QuickPID::iAwMode::iAwClamp, QuickPID::Action::direct);



// Definir pines para la pantalla TFT
#define TFT_CS    5  // CS conectado al pin 5 del ESP32
#define TFT_RST   15 // RESET conectado al pin 15 del ESP32
#define TFT_DC     2 // AO (DC/RS) conectado al pin 2 del ESP32
#define TFT_MOSI  23 // Data = SDA conectado al pin 23 del ESP32
#define TFT_SCLK  18 // Clock = SCK conectado al pin 18 del ESP32

// Inicializar la pantalla TFT
Adafruit_ST7735 tft = Adafruit_ST7735(TFT_CS, TFT_DC, TFT_MOSI, TFT_SCLK, TFT_RST);

void IRAM_ATTR onTimer() {
  cambiarEstado = true; // Establecer la bandera para indicar que se debe cambiar el estado
}

void notifyClients(String sensorReadings) {
  ws.textAll(sensorReadings);
}

String getSensorReadings() {
  JSONVar readings;
  readings["temperatura1"] = t1;
  readings["temperatura2"] = t2;
  readings["temperatura3"] = t3;
  readings["temperatura4"] = t4;
  readings["temperatura5"] = t5;
  readings["temperatura6"] = t6;
  readings["setpoint"] = setpoint;
  readings["ciclo"] = ciclo;
  readings["led"] = digitalRead(PIN_PWM) ? "OFF" : "ON";
  String jsonString = JSON.stringify(readings);
  return jsonString;
}

void handleWebSocketMessage(void *arg, uint8_t *data, size_t len) {
  AwsFrameInfo *info = (AwsFrameInfo*)arg;
  if (info->final && info->index == 0 && info->len == len && info->opcode == WS_TEXT) {
    String message = String((char*)data);

    if (message.startsWith("setpoint:")) {
      setpoint = message.substring(9).toFloat();
      Serial.println("Setpoint updated: " + String(setpoint));

    } else if (message.startsWith("pid:")) {
      String jsonString = message.substring(4);
      Serial.println("Received JSON: " + jsonString);

      JSONVar pidValues = JSON.parse(jsonString);

      if (JSON.typeof(pidValues) == "undefined") {
        Serial.println("Error parsing PID values!");
        return;
      }

      // Verificar y convertir los valores del JSON a double
      if (pidValues.hasOwnProperty("kp") && pidValues.hasOwnProperty("ki") && pidValues.hasOwnProperty("kd")) {
        Kp = atof(pidValues["kp"]);
        Ki = atof(pidValues["ki"]);
        Kd = atof(pidValues["kd"]);

        // Verificar si las conversiones fueron exitosas
        if (isnan(Kp) || isnan(Ki) || isnan(Kd)) {
          Serial.println("Error converting PID values to double!");
        } else {
          myPID.SetTunings(Kp, Ki, Kd);
          Serial.println("PID updated: Kp=" + String(Kp) + ", Ki=" + String(Ki) + ", Kd=" + String(Kd));
        }
      } else {
        Serial.println("Missing PID values in JSON!");
      }
      
    } else if (message == "getReadings") {
      String sensorReadings = getSensorReadings();
      notifyClients(sensorReadings);
    }
  }
}




void onEvent(AsyncWebSocket *server, AsyncWebSocketClient *client, AwsEventType type, void *arg, uint8_t *data, size_t len) {
  switch (type) {
    case WS_EVT_CONNECT:
      Serial.printf("WebSocket client #%u connected from %s\n", client->id(), client->remoteIP().toString().c_str());
      break;
    case WS_EVT_DISCONNECT:
      Serial.printf("WebSocket client #%u disconnected\n", client->id());
      break;
    case WS_EVT_DATA:
      handleWebSocketMessage(arg, data, len);
      break;
    case WS_EVT_PONG:
    case WS_EVT_ERROR:
      break;
  }
}

void initWebSocket() {
  ws.onEvent(onEvent);
  server.addHandler(&ws);
}

void initWiFi() {
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
  Serial.print("Connecting to WiFi ..");
  while (WiFi.status() != WL_CONNECTED) {
    Serial.print('.');
    delay(1000);
  }
  Serial.println(WiFi.localIP());
}

void initSPIFFS() {
  if (!SPIFFS.begin(true)) {
    Serial.println("An error has occurred while mounting SPIFFS");
  }
  Serial.println("SPIFFS mounted successfully");
}


//
bool compareAddress(const DeviceAddress addr1, const DeviceAddress addr2) {
    for (uint8_t i = 0; i < 8; i++) {
        if (addr1[i] != addr2[i]) return false;
    }
    return true;
}

void printAddress(DeviceAddress deviceAddress) {
    for (uint8_t i = 0; i < 8; i++) {
        if (deviceAddress[i] < 16) Serial.print("0");
        Serial.print(deviceAddress[i], HEX);
        if (i < 7) Serial.print(":");
    }
}

void printAddresses() {
    for (int i = 0; i < sensors.getDeviceCount(); ++i) {
        DeviceAddress sensorAddress;
        sensors.getAddress(sensorAddress, i);
        printAddress(sensorAddress);
        Serial.println();
    }
}






void setup() {
  Serial.begin(115200);
  sensors.begin(); // Inicializa la comunicación con el sensor de temperatura
  tft.initR(INITR_BLACKTAB); // Init ST7735S chip, black tab
  tft.setRotation(1); // Rotar la pantalla si es necesario
  tft.fillScreen(ST77XX_BLACK); // Limpiar la pantalla al inicio

  pinMode(PIN_PWM, OUTPUT); // Configurar el pin como salida
  pinMode(PIN_venti,OUTPUT);
  myPID.SetMode(QuickPID::Control::automatic);
  myPID.SetSampleTimeUs(600000000); // Tiempo de muestreo en microsegundos (10 min)
  myPID.SetOutputLimits(0, 100);  // Limitar la salida del PID

  // Configurar el temporizador para generar interrupciones cada 10 minutos
  timer = timerBegin(0, 80, true); // Iniciar el temporizador con un divisor de frecuencia de 80
  timerAttachInterrupt(timer, &onTimer, true); // Adjuntar la rutina de interrupción
  timerAlarmWrite(timer, PERIODO_PWM_MICROSECONDS, true); // Configurar el temporizador para generar una interrupción después de 10 minutos
  timerAlarmEnable(timer); // Habilitar el temporizador

  initWiFi();
  initSPIFFS();
  initWebSocket();

  // Configurar servidor Web
  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request) {
    request->send(SPIFFS, "/index.html", "text/html");
  });

  server.serveStatic("/", SPIFFS, "/").setDefaultFile("index.html");

  server.on("/style.css", HTTP_GET, [](AsyncWebServerRequest *request) {
    request->send(SPIFFS, "/style.css", "text/css");
  });

  server.on("/script.js", HTTP_GET, [](AsyncWebServerRequest *request) {
    request->send(SPIFFS, "/script.js", "application/javascript");
  });
  // Iniciar servidor
  server.begin();
  
  
  
  
}

void loop() {
  // Calcular el nuevo valor del ciclo del PID y actualizar el mapeo
  if (myPID.Compute()) {
    ciclomapeado = map(ciclo, 0, 100, 0, 20);
  }

  // Solicitar las temperaturas a los sensores
  sensors.requestTemperatures();
  int numSensores = sensors.getDeviceCount();

  for (int i = 0; i < numSensores; ++i) {
    DeviceAddress sensorAddress;
    if (sensors.getAddress(sensorAddress, i)) {
      float temp = sensors.getTempC(sensorAddress);
      if (compareAddress(sensorAddress, sensor1)) {
        t1 = temp;
      } else if (compareAddress(sensorAddress, sensor2)) {
        t2 = temp;
      } else if (compareAddress(sensorAddress, sensor3)) {
        t3 = temp - 2.2;
      } else if (compareAddress(sensorAddress, sensor4)) {
        t4 = temp;
      } else if (compareAddress(sensorAddress, sensor5)) {
        t5 = temp;
      } else if (compareAddress(sensorAddress, sensor6)) {
         t6 = temp;
      }
    }
  }

  if (cambiarEstado) {
    Serial.println("Pasaron 10 min.");
    if(etadoventi){
      digitalWrite(PIN_venti, LOW);
      etadoventi = false;
    }
    else{
      digitalWrite(PIN_venti, HIGH);
      etadoventi = true;
    }
    cambiarEstado = false; // Restablecer la bandera
  }
  
  if (millis() - tiempoInicio_min >= 30000) { // Verificar si ha pasado 1 minuto
    Serial.print("Pasó un minuto. Ciclo: ");
    Serial.println(ciclo);
    if (ciclomapeado > 0) {
      digitalWrite(PIN_PWM, LOW); // Cambiar el estado del pin
      Serial.println("LED ENCENDIDO");
      ciclomapeado--;
    } else {
      digitalWrite(PIN_PWM, HIGH); // Cambiar el estado del pin
      Serial.println("LED APAGADO");
    }
    tiempoInicio_min = millis(); // Reiniciar el tiempo de inicio
  }

  if (millis() - tiempoInicio >= 1000) { // Verificar si ha pasado 1 segundo
    tft.fillRect(0, 0, tft.width(), tft.height(), ST77XX_BLACK);
    tft.setTextColor(ST77XX_YELLOW);
    tft.setTextSize(2);
    tft.setCursor(60, 1);
    tft.print(t5);
    tft.setCursor(1, 1);
    tft.print("Temp:");
    tft.setCursor(60, 40);
    tft.print(setpoint);
    tft.setCursor(1, 40);
    tft.print("Sp : ");
    tft.setCursor(1, 60);
    tft.print("kp");
    tft.setCursor(30, 60);
    tft.print(Kp);
    tft.setCursor(1, 80);
    tft.print("ki");
    tft.setCursor(30, 80);
    tft.print(Ki);
    tft.setCursor(1, 100);
    tft.print("kd");
    tft.setCursor(30, 100);
    tft.print(Kd);
    

    if (digitalRead(PIN_PWM)) {
      tft.setTextColor(ST77XX_YELLOW);
      tft.setTextSize(2);
      tft.setCursor(1, 20);
      tft.print("SALIDA : OFF");
    } else {
      tft.setTextColor(ST77XX_YELLOW);
      tft.setTextSize(2);
      tft.setCursor(1, 20);
      tft.print("SALIDA : ON");
    }
    tft.setTextSize(1);
    tft.setCursor(85, 105);
    tft.print(WiFi.localIP());
    tft.setCursor(140, 90);
    tft.print(ciclomapeado);
    String sensorReadings = getSensorReadings();
    notifyClients(sensorReadings);
    tiempoInicio = millis(); // Reiniciar el tiempo de inicio
  }

  ws.cleanupClients();
}



