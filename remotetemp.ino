#include <WiFi.h>
#include <OneWire.h>
#include <DallasTemperature.h>
#include <time.h>

// Replace with your network credentials
const char* ssid = "ADD SSID";
const char* password = "ADD PASSWORD";

// Set web server port number to 80
WiFiServer server(80);

// Data wire is plugged into port 17 on the ESP32
#define ONE_WIRE_BUS 17
OneWire oneWire(ONE_WIRE_BUS);
DallasTemperature sensors(&oneWire);
DeviceAddress insideThermometer;

// Temperature logging variables
float temperatureLog[576]; // Store temperature for 48 hours (every 5 minutes)
String timeLog[576]; // Store timestamps
unsigned long lastLogTime = 0;
String htmlResponse; // Global variable to store the HTML response

void setup() {
    Serial.begin(115200);
    
    // Set timezone to Eastern Time (EDT)
    setenv("TZ", "EST5EDT,M3.2.0/2,M11.1.0", 1);
    tzset();

    sensors.begin();
    if (!sensors.getAddress(insideThermometer, 0)) {
        Serial.println("Unable to find address for Device 0");
    }
    sensors.setResolution(insideThermometer, 9);

    // Initialize temperature log
    for (int i = 0; i < 576; i++) {
        temperatureLog[i] = -100.0;
        timeLog[i] = "";
    }

    // Connect to Wi-Fi
    Serial.print("Connecting to ");
    Serial.println(ssid);
    WiFi.begin(ssid, password);
    while (WiFi.status() != WL_CONNECTED) {
        delay(500);
        Serial.print(".");
    }
    Serial.println("");
    Serial.println("WiFi connected.");
    Serial.println("IP address: ");
    Serial.println(WiFi.localIP());

    // Initialize time
    configTime(0, 0, "pool.ntp.org", "time.nist.gov"); // NTP server
    server.begin();
}

void loop() {
    unsigned long currentMillis = millis();

    // Log temperature every 5 minutes
    if (currentMillis - lastLogTime >= 300000) { // 5 minutes in milliseconds
        lastLogTime = currentMillis;
        logTemperature();
    }

    handleClient();
}

void logTemperature() {
    sensors.requestTemperatures();
    float currentTemp = sensors.getTempC(insideThermometer);
    updateTemperatureLog(currentTemp);
}

void updateTemperatureLog(float temp) {
    // Shift all data down by one index
    for (int i = 0; i < 575; i++) {
        temperatureLog[i] = temperatureLog[i + 1];
        timeLog[i] = timeLog[i + 1];
    }

    // Store new value at index 575
    time_t now = time(nullptr);
    now -= 4 * 3600; // Adjust for UTC-4 (Eastern Daylight Time)
    struct tm* timeInfo = localtime(&now);
    timeLog[575] = String(timeInfo->tm_hour) + ":" + (timeInfo->tm_min < 10 ? "0" : "") + String(timeInfo->tm_min);
    temperatureLog[575] = temp;

    updateGraphHtml(); // Update HTML response with new data
}

void updateGraphHtml() {
    // Create the HTML response with temperature and graph
    htmlResponse = "<!DOCTYPE html><html>";
    htmlResponse += "<head><meta name=\"viewport\" content=\"width=device-width, initial-scale=1\">";
    htmlResponse += "<link rel=\"icon\" href=\"data:,\">";
    htmlResponse += "<style>html { font-family: Helvetica; display: inline-block; margin: 0px auto; text-align: center;}";
    htmlResponse += ".button { background-color: #4CAF50; border: none; color: white; padding: 16px 40px; text-decoration: none; font-size: 30px; margin: 2px; cursor: pointer;}";
    htmlResponse += ".button2 { background-color: #555555; }</style></head>";
    htmlResponse += "<body><h1>ESP32 Web Server</h1>";
    htmlResponse += "<p style=\"font-size: 48px;\">Temperature: <span id=\"temperature\">" + String(temperatureLog[575]) + " &deg;C</span></p>";
    htmlResponse += "<h2>Temperature Log (Last 48 hours)</h2><div style='width:100%; height:300px;'><canvas id='tempGraph' width='800' height='400'></canvas></div>";
    htmlResponse += "<script src='https://cdn.jsdelivr.net/npm/chart.js'></script><script>";
    htmlResponse += "const ctx = document.getElementById('tempGraph').getContext('2d');";
    htmlResponse += "let tempChart = null;";
    htmlResponse += "function initializeGraph() {";
    htmlResponse += "  tempChart = new Chart(ctx, {";
    htmlResponse += "    type: 'line',";
    htmlResponse += "    data: {";
    htmlResponse += "      labels: [" + getTimeLabels() + "],";
    htmlResponse += "      datasets: [{";
    htmlResponse += "        label: 'Temperature (°C)',";
    htmlResponse += "        data: [" + getTemperatureData() + "],";
    htmlResponse += "        borderColor: 'rgba(75, 192, 192, 1)',";
    htmlResponse += "        borderWidth: 1";
    htmlResponse += "      }]";
    htmlResponse += "    },";
    htmlResponse += "    options: {";
    htmlResponse += "      scales: {";
    htmlResponse += "        y: { beginAtZero: false }";
    htmlResponse += "      }";
    htmlResponse += "    }";
    htmlResponse += "  });";
    htmlResponse += "}";
    htmlResponse += "function updateTemperature() {";
    htmlResponse += "  fetch('/temperature-data')";
    htmlResponse += "    .then(response => response.json())";
    htmlResponse += "    .then(data => {";
    htmlResponse += "      document.getElementById('temperature').innerHTML = data.currentTemp + ' &deg;C';";
    htmlResponse += "      tempChart.data.labels.shift();";
    htmlResponse += "      tempChart.data.labels.push(data.time);";
    htmlResponse += "      tempChart.data.datasets[0].data.shift();";
    htmlResponse += "      tempChart.data.datasets[0].data.push(data.temp);";
    htmlResponse += "      tempChart.update();"; // Update the chart with new data
    htmlResponse += "    });";
    htmlResponse += "}";
    htmlResponse += "setInterval(updateTemperature, 300000);"; // Update every 5 minutes
    htmlResponse += "initializeGraph();"; // Initialize the graph on page load
    htmlResponse += "</script>";
    htmlResponse += "</body></html>";
    Serial.println(htmlResponse);
}

String getTimeLabels() {
    String labels = "";
    for (int i = 0; i < 576; i++) {
        labels += "\"" + timeLog[i] + "\"";
        if (i < 575) {
            labels += ",";
        }
    }
    return labels;
}

String getTemperatureData() {
    String data = "";
    for (int i = 0; i < 576; i++) {
        data += String(temperatureLog[i]);
        if (i < 575) {
            data += ",";
        }
    }
    return data;
}

String getTemperatureLogJson() {
    String json = "{\"currentTemp\": " + String(temperatureLog[575]) + ", \"time\": \"" + timeLog[575] + "\", \"temp\": " + String(temperatureLog[575]) + "}";
    return json;
}

void handleClient() {
    WiFiClient client = server.available();

    if (client) {
        Serial.println("New Client.");

        // Read the first line of the request
        String currentLine = client.readStringUntil('\r');
        Serial.println(currentLine);

        // Handle HTTP request
        if (currentLine.indexOf("GET /temperature-data") != -1) {
            // Send JSON response with latest temperature data
            client.println("HTTP/1.1 200 OK");
            client.println("Content-type: application/json");
            client.println("Connection: close");
            client.println();
            client.print(getTemperatureLogJson());
            client.stop();
            Serial.println("Client disconnected.");
            Serial.println("");
        } else {
            // Send the HTTP response headers
            client.println("HTTP/1.1 200 OK");
            client.println("Content-type:text/html");
            client.println("Connection: close");
            client.println();

            // Print the last updated HTML response with temperature and graph
            client.print(htmlResponse);
            client.stop();
            Serial.println("Client disconnected.");
            Serial.println("");
        }
    }
}
