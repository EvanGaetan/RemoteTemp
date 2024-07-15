#include <WiFi.h>
#include <OneWire.h>
#include <DallasTemperature.h>
#include <time.h>

// Replace with your network credentials
const char* ssid = "add ssid";
const char* password = "add password";

// Set web server port number to 80
WiFiServer server(80);

// Data wire is plugged into port 17 on the ESP32
#define ONE_WIRE_BUS 17
OneWire oneWire(ONE_WIRE_BUS);
DallasTemperature sensors(&oneWire);
DeviceAddress insideThermometer;

// Temperature logging variables
float temperatureLog[48]; // Store temperature for 24 hours (every 30 minutes)
String timeLog[48]; // Store timestamps
unsigned long lastLogTime = 0;
String htmlResponse; // Global variable to store the HTML response

void setup() {
    Serial.begin(115200);
    
    sensors.begin();
    if (!sensors.getAddress(insideThermometer, 0)) {
        Serial.println("Unable to find address for Device 0");
    }
    sensors.setResolution(insideThermometer, 9);

    // Initialize temperature log
    for (int i = 0; i < 48; i++) {
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

    // Log temperature every 30 minutes
    if (currentMillis - lastLogTime >= 1800000) { // 30 minutes
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
    // Move all index values down by one
    for (int i = 0; i < 47; i++) {
        temperatureLog[i] = temperatureLog[i + 1];
        timeLog[i] = timeLog[i + 1];
    }
    
    // Store new values at index 47
    time_t now = time(nullptr);
    struct tm* timeInfo = localtime(&now);
    timeLog[47] = String(timeInfo->tm_hour) + ":" + (timeInfo->tm_min < 10 ? "0" : "") + String(timeInfo->tm_min);
    temperatureLog[47] = temp;
    updateGraphHtml();
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
    htmlResponse += "<p style=\"font-size: 48px;\">Temperature: " + String(temperatureLog[47]) + " &deg;C</p>";
    htmlResponse += "<h2>Temperature Log (Last 24 hours)</h2><div style='width:100%; height:300px;'><canvas id='tempGraph' width='400' height='200'></canvas></div>";
    htmlResponse += "<script src='https://cdn.jsdelivr.net/npm/chart.js'></script><script>";
    htmlResponse += "const ctx = document.getElementById('tempGraph').getContext('2d');";
    htmlResponse += "const tempData = " + getTemperatureLogJson() + ";";
    htmlResponse += "const tempChart = new Chart(ctx, {type: 'line', data: {labels: tempData.labels, datasets: [{label: 'Temperature (°C)', data: tempData.temps, borderColor: 'rgba(75, 192, 192, 1)', borderWidth: 1}]}, options: {scales: {y: {beginAtZero: false}}}});";
    htmlResponse += "</script>";
    htmlResponse += "</body></html>";
    Serial.println(htmlResponse);
}

String getTemperatureLogJson() {
    String json = "{\"labels\": [";
    String temps = "[";
    for (int i = 0; i < 48; i++) {
        json += "\"" + timeLog[i] + "\"";
        temps += String(temperatureLog[i]);
        if (i < 47) {
            json += ",";
            temps += ",";
        }
    }
    json += "],\"temps\": " + temps + "]}";
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
        if (client.available()) {
            String request = client.readString();
            Serial.println(request);

            // Send the HTTP response headers
            client.println("HTTP/1.1 200 OK");
            client.println("Content-type:text/html");
            client.println("Connection: close");
            client.println();

            // Print the last updated HTML response with temperature and graph
            client.print(htmlResponse); // Using the global htmlResponse variable updated in updateGraphHtml()
            client.stop();
            Serial.println("Client disconnected.");
            Serial.println("");
        }
    }
}
