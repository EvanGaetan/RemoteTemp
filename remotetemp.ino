// Include necessary libraries
#include <WiFi.h>
#include <OneWire.h>
#include <DallasTemperature.h>

// Replace with your network credentials
const char* ssid = "ADD SSID";
const char* password = "ADD PASSWORD";

// Set web server port number to 80
WiFiServer server(80);

// Data wire is plugged into port 17 on the esp32
#define ONE_WIRE_BUS 17
OneWire oneWire(ONE_WIRE_BUS);
DallasTemperature sensors(&oneWire);
DeviceAddress insideThermometer;

void setup() {
    Serial.begin(115200);

    sensors.begin();
    if (!sensors.getAddress(insideThermometer, 0)) {
        Serial.println("Unable to find address for Device 0");
    }
    sensors.setResolution(insideThermometer, 9);

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

    // Start web server
    server.begin();
}

void loop() {
    WiFiClient client = server.available();

    if (client) {
        Serial.println("New Client.");

        // Read the first line of the request
        String currentLine = client.readStringUntil('\r');
        Serial.println(currentLine);

        // Wait for the client to send the rest of the HTTP request
        while (client.connected() && !client.available()) {
            delay(1);
        }

        // Handle HTTP request
        if (client.available()) {
            String request = client.readString();
            Serial.println(request);

            // Send the HTTP response headers
            client.println("HTTP/1.1 200 OK");
            client.println("Content-type:text/html");
            client.println("Connection: close");
            client.println();

            // Request temperatures
            sensors.requestTemperatures();

            // Send HTML response with temperature
            String htmlResponse = "<!DOCTYPE html><html>";
            htmlResponse += "<head><meta name=\"viewport\" content=\"width=device-width, initial-scale=1\">";
            htmlResponse += "<link rel=\"icon\" href=\"data:,\">";
            htmlResponse += "<style>html { font-family: Helvetica; display: inline-block; margin: 0px auto; text-align: center;}";
            htmlResponse += ".button { background-color: #4CAF50; border: none; color: white; padding: 16px 40px; text-decoration: none; font-size: 30px; margin: 2px; cursor: pointer;}";
            htmlResponse += ".button2 { background-color: #555555; }</style></head>";
            htmlResponse += "<body><h1>ESP32 Web Server</h1>";
            htmlResponse += "<p style=\"font-size: 48px;\">Temperature: " + String(sensors.getTempC(insideThermometer)) + " &deg;C</p>";
            htmlResponse += "</body></html>";

            client.print(htmlResponse);

            // Close the connection
            client.stop();
            Serial.println("Client disconnected.");
            Serial.println("");
        }
    }
}