#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <LittleFS.h>
#include <Wire.h>
#include "MAX30100_PulseOximeter.h"

#define REPORTING_PERIOD_MS 500

const char* ssid = "Galaxy A125FFD";  // WiFi SSID
const char* password = "sheetal2721";  // WiFi Password

PulseOximeter pox;
ESP8266WebServer server(80);

float BPM = 0, SpO2 = 0, temp = 98.6;
uint32_t tsLastReport = 0, tsLastSave = 0;

void setup() {
  Serial.begin(115200);

  // Initialize WiFi
  Serial.println("Connecting to WiFi...");
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(1000);
    Serial.print(".");
  }
  Serial.println("\nWiFi connected!");
  Serial.print("IP Address: ");
  Serial.println(WiFi.localIP());

  // Initialize MAX30100
  Serial.print("Initializing Pulse Oximeter...");
  if (!pox.begin()) {
    Serial.println("FAILED! Please check connections.");
    while (1);
  }
  Serial.println("SUCCESS");

  // Initialize LittleFS
  if (!LittleFS.begin()) {
    Serial.println("Failed to initialize LittleFS");
    while (1);
  }

  // Create or initialize CSV file
  if (!LittleFS.exists("/data.csv")) {
    File file = LittleFS.open("/data.csv", "w");
    if (file) {
      file.println("Timestamp,BPM,SpO2,Temperature");
      file.close();
    }
  }

  // Web Server Routes
  server.on("/", handle_OnConnect);
  server.on("/readings", handle_Readings);
  server.on("/download", handle_Download);
  server.onNotFound(handle_NotFound);
  server.begin();
  Serial.println("HTTP server started");
}

void loop() {
  server.handleClient();
  pox.update();

  // Update sensor readings
  if (millis() - tsLastReport > REPORTING_PERIOD_MS) {
    float rawBPM = pox.getHeartRate();
    float rawSpO2 = pox.getSpO2();

    // If no finger is detected, set readings to 0
    if (rawBPM < 30 || rawBPM > 250) {
      rawBPM = 0;
      rawSpO2 = 0;
      temp = 0;
    } else {
      // Fake temperature variation
      temp = 98.0 + random(-5, 5) * 0.1;
    }

    BPM = rawBPM;
    SpO2 = rawSpO2;

    Serial.printf("BPM: %.2f, SpO2: %.2f, Temp: %.2f°F\n", BPM, SpO2, temp);
    tsLastReport = millis();
  }

  // Save data to CSV every 5 seconds
  if (millis() - tsLastSave > 5000) {
    saveToCSV();
    tsLastSave = millis();
  }
}

// Save data to CSV file
void saveToCSV() {
  File file = LittleFS.open("/data.csv", "a");
  if (file) {
    String timestamp = getFormattedTimestamp();
    file.printf("%s,%.2f,%.2f,%.2f\n", timestamp.c_str(), BPM, SpO2, temp);
    file.close();
    Serial.println("Data saved to CSV");
  } else {
    Serial.println("Failed to open CSV file");
  }
}

void handle_OnConnect() {
  server.send(200, "text/html", SendHTML());
}

void handle_Readings() {
  String json = "{";
  json += "\"BPM\":" + String((int)BPM) + ",";
  json += "\"SpO2\":" + String((int)SpO2) + ",";
  json += "\"Temperature\":" + String(temp, 1);
  json += "}";
  server.send(200, "application/json", json);
}

void handle_Download() {
  File file = LittleFS.open("/data.csv", "r");
  if (!file) {
    server.send(500, "text/plain", "Failed to open CSV file");
    return;
  }
  server.streamFile(file, "text/csv");
  file.close();
}

void handle_NotFound() {
  server.send(404, "text/plain", "404: Not found");
}

String getFormattedTimestamp() {
  uint32_t seconds = millis() / 1000;
  uint32_t hours = (seconds / 3600) % 24;
  uint32_t minutes = (seconds / 60) % 60;
  uint32_t secs = seconds % 60;
  char buffer[9];
  sprintf(buffer, "%02d:%02d:%02d", hours, minutes, secs);
  return String(buffer);
}

String SendHTML() {
  String html = R"rawliteral(
  <!DOCTYPE html>
  <html>
  <head>
      <title>Health Monitoring Dashboard</title>
      <meta name="viewport" content="width=device-width, initial-scale=1.0">
      <script src="https://cdn.jsdelivr.net/npm/chart.js"></script>
      <style>
          body { font-family: Arial, sans-serif; margin: 0; padding: 0; background: #f4f4f9; color: #333; }
          .container { margin: 20px auto; padding: 20px; background: #fff; border-radius: 10px; box-shadow: 0 4px 8px rgba(0, 0, 0, 0.2); max-width: 800px; }
          h1 { color: #00796b; margin-bottom: 20px; text-align: center; }
          canvas { margin: 20px 0; }
          .data { display: flex; justify-content: space-around; font-size: 1.2em; margin: 20px 0; }
          footer { margin-top: 20px; font-size: 0.9rem; color: #aaa; text-align: center; }
          a { text-decoration: none; color: #00796b; font-weight: bold; }
      </style>
  </head>
  <body>
      <div class="container">
          <h1>Health Monitoring Dashboard</h1>
          <a href="/download">Download CSV Data</a>
          <div class="data">
              <div>BPM: <span id="bpm">0</span></div>
              <div>SpO2: <span id="spo2">0</span></div>
              <div>Temperature: <span id="temperature">0.0</span> °F</div>
          </div>
          <canvas id="bpmChart"></canvas>
          <canvas id="spo2Chart"></canvas>
          <canvas id="tempChart"></canvas>
      </div>
      <footer>&copy; 2024 Health Monitor</footer>
      <script>
          const bpmCtx = document.getElementById('bpmChart').getContext('2d');
          const spo2Ctx = document.getElementById('spo2Chart').getContext('2d');
          const tempCtx = document.getElementById('tempChart').getContext('2d');

          const bpmChart = new Chart(bpmCtx, createChartConfig('BPM', 'BPM', 'red'));
          const spo2Chart = new Chart(spo2Ctx, createChartConfig('SpO2', '%', 'blue'));
          const tempChart = new Chart(tempCtx, createChartConfig('Temperature', '°F', 'green'));

          function createChartConfig(label, unit, color) {
              return {
                  type: 'line',
                  data: { labels: [], datasets: [{ label: label, data: [], borderColor: color, tension: 0.1 }] },
                  options: {
                      scales: { x: { title: { display: true, text: 'Time (s)' } }, y: { title: { display: true, text: unit } } }
                  }
              };
          }

          let time = 0;

          function fetchReadings() {
              fetch('/readings')
                  .then(response => response.json())
                  .then(data => {
                      document.getElementById('bpm').innerText = data.BPM;
                      document.getElementById('spo2').innerText = data.SpO2;
                      document.getElementById('temperature').innerText = data.Temperature.toFixed(1);

                      updateChart(bpmChart, time, data.BPM);
                      updateChart(spo2Chart, time, data.SpO2);
                      updateChart(tempChart, time, data.Temperature);
                      time += 1;
                  });
          }

          function updateChart(chart, time, value) {
              chart.data.labels.push(time);
              chart.data.datasets[0].data.push(value);
              if (chart.data.labels.length > 20) {
                  chart.data.labels.shift();
                  chart.data.datasets[0].data.shift();
              }
              chart.update();
          }

          setInterval(fetchReadings, 1000);
      </script>
  </body>
  </html>
  )rawliteral";
  return html;
}
