//Librerias
#include <WiFi.h>
#include <LiquidCrystal.h>
#include <IOXhop_FirebaseESP32.h>
#include <ESPAsyncWebServer.h>
#include <AsyncTCP.h>

// Configuración de Firebase
#define FIREBASE_Host ""
#define FIREBASE_Key ""

// Definición de la ruta en la base de datos de Firebase
String ruta = "Estacionamiento";  // Cambiado de "Estacionamiento" a "Espacios"

// Configuración de la red WiFi
const char* ssid = "";
const char* pass = "";

// Configuración de los pines para los sensores ultrasónicos 
const int EchoEntrada = 13;
const int TriggerEntrada = 12;
const int EchoSalida = 14;
const int TriggerSalida = 27;

// Inicialización del objeto LiquidCrystal para el LCD
LiquidCrystal lcd(23, 22, 21, 19, 18, 5);

// Definición de constantes y variables para el control del estacionamiento
const int totalEstacionamientos = 5;
int Estacionamientos = totalEstacionamientos;
int autosEntrada = 0;
int autosSalida = 0;
int ocupados = 0;

// Configuración de tiempos de espera para los sensores
unsigned long tiempoEsperaSensor = 0;
const unsigned long tiempoEsperaSensorEntrada = 3000;
const unsigned long tiempoEsperaSensorSalida = 3000;

// Configuración del servidor web asíncrono
AsyncWebServer server(80);

void setup() {

  Serial.begin(115200);

  // Conexión a la red WiFi
  WiFi.begin(ssid, pass);
  delay(2000);

  Serial.println("Conectando a la red WiFi");
  Serial.println(ssid);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.println("");
  Serial.println("WiFi Conectado");
  Serial.println("Dirección IP: ");
  Serial.println(WiFi.localIP());

  // Inicialización de la conexión a Firebase
  Firebase.begin(FIREBASE_Host, FIREBASE_Key);

  // Configuración de pines
  pinMode(TriggerEntrada, OUTPUT);
  pinMode(TriggerSalida, OUTPUT);
  pinMode(EchoEntrada, INPUT);
  pinMode(EchoSalida, INPUT);

  // Inicialización del objeto LCD
  lcd.begin(16, 2);
  lcd.print("Libres: ");
  lcd.setCursor(8, 0);
  lcd.print(Estacionamientos);

  lcd.setCursor(0, 1);
  lcd.print("Ocupados: ");

  // Configuración de las rutas para el servidor web
  server.on("/", HTTP_GET, [](AsyncWebServerRequest* request) {
    //código HTML para la página principal
    String html = "<html lang='es'><head><meta charset='UTF-8'><meta name='viewport' content='width=device-width, initial-scale=1.0'><title>Parking System</title>";
    html += "<link rel='stylesheet' href='https://stackpath.bootstrapcdn.com/bootstrap/4.3.1/css/bootstrap.min.css'>";
    html += "<link rel='stylesheet' href='https://cdnjs.cloudflare.com/ajax/libs/font-awesome/5.15.1/css/all.min.css'>";
    html += "<style>body{font-family:'Arial',sans-serif;background-color:#f8f9fa;color:#343a40;}.container{max-width:800px;margin:auto;padding:20px;background-color:#ffffff;box-shadow:0 0 10px rgba(0,0,0,0.1);border-radius:8px;margin-top:50px;text-align:center;}h1{text-align:center;color:#007bff;}";
    html += ".highlight-title{color: #007bff; font-weight: bold;}</style></head><body>";
    html += "<div class='container'><h1><span class='highlight-title'>Parking System</span></h1><img src='https://uploads-ssl.webflow.com/63750836cfc6505b31be8ba3/63750836cfc65060dfbe8cd1_art-24-dynakrom.jpg' alt='Estacionamientos' style='max-width: 100%; height: auto;'>";
    html += "<div class='card p-4'>";
    html += "<p><i class='icon fas fa-car libres' style='color: #28a745; font-size: 1.5em;'></i> <span class='libres highlight-libres' style='color: #28a745; font-weight: bold; font-size: 1.2em;'>Libres:</span> <span id='libres' class='font-weight-bold libres highlight-libres' style='color: #28a745; font-size: 1.2em;'>" + String(Estacionamientos) + "</span></p>";
    html += "<p><i class='icon fas fa-car ocupados' style='color: #dc3545; font-size: 1.5em;'></i> <span class='ocupados highlight-ocupados' style='color: #dc3545; font-weight: bold; font-size: 1.2em;'>Ocupados:</span> <span id='ocupados' class='font-weight-bold ocupados highlight-ocupados' style='color: #dc3545; font-size: 1.2em;'>" + String(autosEntrada - autosSalida) + "</span></p>";
    html += "<p><i class='icon fas fa-chart-pie aforo' style='color: #007bff; font-size: 1.5em;'></i> <span class='aforo highlight-aforo' style='color: #007bff; font-weight: bold; font-size: 1.2em;'>Aforo:</span> <span id='porcentajeAforo' class='font-weight-bold aforo highlight-aforo' style='color: #007bff; font-size: 1.2em;'>" + String(((autosEntrada - autosSalida) * 100) / totalEstacionamientos) + "%</span></p>";
    html += "<canvas id='myChart' width='400' height='200'></canvas></div></div>";
    html += "<script src='https://code.jquery.com/jquery-3.2.1.slim.min.js'></script>";
    html += "<script src='https://cdnjs.cloudflare.com/ajax/libs/popper.js/1.12.9/umd/popper.min.js'></script>";
    html += "<script src='https://maxcdn.bootstrapcdn.com/bootstrap/4.0.0/js/bootstrap.min.js'></script>";
    html += "<script>setInterval(function() { location.reload(true); }, 5000);</script>";
    html += "</body></html>";
    request->send(200, "text/html", html);
  });

  server.on("/ajax", HTTP_GET, [](AsyncWebServerRequest* request) {
    String json = "{";
    json += "\"libres\":" + String(Estacionamientos) + ",";
    json += "\"ocupados\":" + String(autosEntrada - autosSalida) + ",";
    json += "\"porcentajeAforo\":" + String(((autosEntrada - autosSalida) * 100) / totalEstacionamientos);
    json += "}";
    request->send(200, "application/json", json);
  });
  // Inicio del servidor web
  server.begin();
}

void loop() {
  // Lógica para el sensor de entrada
  if (Estacionamientos > 0 && millis() - tiempoEsperaSensor >= tiempoEsperaSensorEntrada) {
    // Activa el sensor de entrada
    digitalWrite(TriggerEntrada, HIGH);
    delayMicroseconds(10);
    digitalWrite(TriggerEntrada, LOW);

    // Calcula la distancia para el sensor de entrada
    long tEntrada = pulseIn(EchoEntrada, HIGH);
    long dEntrada = tEntrada / 58;
    
    // Verifica si hay un automóvil en la entrada
    if (dEntrada <= 8) {
      autosEntrada++;
      Estacionamientos--;
      actualizarLCD();
      tiempoEsperaSensor = millis();
      delay(500);
    }

    ocupados = autosEntrada - autosSalida;
  }
  // Lógica para el sensor de salida
  if (millis() - tiempoEsperaSensor >= tiempoEsperaSensorSalida) {
    // Activa el sensor de salida
    digitalWrite(TriggerSalida, HIGH);
    delayMicroseconds(10);
    digitalWrite(TriggerSalida, LOW);

    // Calcula la distancia para el sensor de salida
    long tSalida = pulseIn(EchoSalida, HIGH);
    long dSalida = tSalida / 58;

    // Verifica si hay un automóvil en la salida
    if (dSalida <= 6 && autosSalida < autosEntrada) {
      autosSalida++;
      Estacionamientos++;
      actualizarLCD();
      tiempoEsperaSensor = millis();
    }

    ocupados = autosEntrada - autosSalida;
  }

  delay(500);
}
// Función para actualizar la pantalla LCD y enviar datos a Firebase
void actualizarLCD() {
  // Actualiza la pantalla LCD
  lcd.clear();
  lcd.print("Libres: ");
  lcd.setCursor(8, 0);
  lcd.print(Estacionamientos);

  lcd.setCursor(0, 1);
  lcd.print("Ocupados: ");
  lcd.setCursor(9, 1);
  lcd.print(autosEntrada - autosSalida);

  // Verifica si el estacionamiento está lleno y actualiza el LCD
  if (Estacionamientos == 0) {
    lcd.clear();
    lcd.print("Estacionamiento");
    lcd.setCursor(0, 1);
    lcd.print("Lleno");
  }
  // Actualiza los valores en la base de datos de Firebase
  Firebase.setInt(ruta + "/Libres", Estacionamientos);
  Firebase.setInt(ruta + "/Ocupados", autosEntrada - autosSalida);
}