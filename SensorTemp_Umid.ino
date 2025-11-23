#include <WiFi.h>
#include <HTTPClient.h>
#include <WebServer.h>
#include <DHT.h>
#include <SPIFFS.h>
#include <ESP32Servo.h>

const char* ssid = "SANDRO";
const char* password = "13111968";

String apiKey = "231YOGNRJRRG6AS7";  
const char* serverTS = "http://api.thingspeak.com/update";

unsigned long lastTS = 0;
const unsigned long intervaloTS = 15000; 

WebServer server(80);
DHT dht(26, DHT11);

Servo servoMotor;
int portaServo = 25;   // GPIO recomendado

void enviarThingSpeak(float temp, float umid) {
  if (WiFi.status() == WL_CONNECTED) {
    HTTPClient http;

    String url = serverTS;
    url += "?api_key=" + apiKey;
    url += "&field1=" + String(temp);
    url += "&field2=" + String(umid);

    http.begin(url);
    int httpCode = http.GET();

    Serial.print("ThingSpeak resposta: ");
    Serial.println(httpCode);

    http.end();
  }
}


void controlaServo(float temp) {

  if (temp < 25) {
    servoMotor.write(0);         
    Serial.println("Servo = 0° (frio)");

  } else if (temp >= 25 && temp < 30) {
    servoMotor.write(90);       
    Serial.println("Servo = 90° (morno)");

  } else {
    servoMotor.write(180);        
    Serial.println("Servo = 180° (quente)");
  }
}


const char INDEX_HTML[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html lang="pt-BR">
<head>
<meta charset="UTF-8">
<title>Termohigrógrafo IoT</title>
<meta name="viewport" content="width=device-width, initial-scale=1">

<style>
body { background:#020617; color:white; font-family:sans-serif; padding:20px; }
h1 { margin-bottom:20px; }
canvas { max-width:600px; width:100%; height:300px; margin-top:40px; }
</style>
</head>
<body>

<h1>Dashboard IoT</h1>

<p>Temperatura: <span id="tempValue">--</span> °C</p>
<p>Umidade: <span id="humValue">--</span> %</p>

<canvas id="tempChart"></canvas>
<canvas id="humChart"></canvas>

<script src="/chart.js"></script>

<script>
const labels = [];
const tempData = [];
const humData = [];
const maxPoints = 30;

const tempChart = new Chart(document.getElementById("tempChart"), {
  type: "line",
  data: { labels: labels, datasets: [{
    label: "Temperatura (°C)",
    data: tempData,
    borderColor: "rgb(255,80,80)",
    borderWidth: 2
  }]}
});

const humChart = new Chart(document.getElementById("humChart"), {
  type: "line",
  data: { labels: labels, datasets: [{
    label: "Umidade (%)",
    data: humData,
    borderColor: "rgb(80,160,255)",
    borderWidth: 2
  }]}
});

function addPoint(label, temp, hum) {
  labels.push(label);
  tempData.push(temp);
  humData.push(hum);

  if (labels.length > maxPoints) {
    labels.shift(); tempData.shift(); humData.shift();
  }

  tempChart.update();
  humChart.update();
}

async function fetchData() {
  const r = await fetch("/data");
  const json = await r.json();
  const now = new Date().toLocaleTimeString("pt-BR",{ hour12:false });

  addPoint(now, json.temperature, json.humidity);

  document.getElementById("tempValue").textContent = json.temperature.toFixed(2);
  document.getElementById("humValue").textContent = json.humidity.toFixed(2);
}

setInterval(fetchData, 2000);
fetchData();
</script>

</body>
</html>
)rawliteral";

void handleRoot() { server.send_P(200, "text/html", INDEX_HTML); }

void handleData() {
  float t = dht.readTemperature();
  float h = dht.readHumidity();

  controlaServo(t);

  char json[80];
  snprintf(json, sizeof(json),
          "{\"temperature\":%.2f,\"humidity\":%.2f}", t, h);

  server.send(200, "application/json", json);
}

void handleChartJS() {
  File file = SPIFFS.open("/chart.min.js", "r");
  if (!file) {
    server.send(404, "text/plain", "chart.min.js NÃO encontrado!");
    return;
  }
  server.streamFile(file, "application/javascript");
  file.close();
}

void setup() {
  Serial.begin(115200);
  dht.begin();

  servoMotor.attach(portaServo);

  if (!SPIFFS.begin(true)) {
    Serial.println("Erro ao montar SPIFFS!");
    return;
  }

  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500); Serial.print(".");
  }

  Serial.println("\nWiFi conectado!");
  Serial.println(WiFi.localIP());

  server.on("/", handleRoot);
  server.on("/data", handleData);
  server.on("/chart.js", handleChartJS);
  server.begin();
}

void loop() {
  server.handleClient();

  unsigned long agora = millis();
  if (agora - lastTS >= intervaloTS) {
    float t = dht.readTemperature();
    float h = dht.readHumidity();

    if (!isnan(t) && !isnan(h)) {
      enviarThingSpeak(t, h);
    }
    lastTS = agora;
  }
}
