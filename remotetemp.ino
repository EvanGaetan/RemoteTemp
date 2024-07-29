#include <WiFi.h>
#include <OneWire.h>
#include <DallasTemperature.h>
#include <ESP_Mail_Client.h>
#include <time.h>

// Replace with your network credentials
const char* ssid = "SSID";
const char* password = "PASSWORD";

// Set web server port number to 80
WiFiServer server(80);

// Data wire is plugged into port 17 on the ESP32
#define ONE_WIRE_BUS 17
OneWire oneWire(ONE_WIRE_BUS);
DallasTemperature sensors(&oneWire);
DeviceAddress insideThermometer;

// Central time-related variables
unsigned long logIntervalSeconds = 600; // Interval between temperature logs in seconds
const int logArraySize = 72; // Number of entries for 12 hours with 10-minute intervals

// Temperature logging variables
float temperatureLog[logArraySize]; // Store temperature for 12 hours (every logIntervalSeconds seconds)
String timeLog[logArraySize]; // Store timestamps
unsigned long lastLogTime = 0;

// Email settings
#define SMTP_HOST "smtp.gmail.com"
#define SMTP_PORT 465
#define AUTHOR_EMAIL "SENDER EMAIL"
#define AUTHOR_PASSWORD "APP PASSWORD"
#define RECIPIENT_EMAIL "EMAIL"

// Maximum and minimum temperature thresholds
const float MAX_TEMP = 21.0;
const float MIN_TEMP = 2.0;

// Global variables for HTML response
String htmlResponse; // Global variable to store the HTML response
SMTPSession smtp;


void setup() {
    Serial.begin(115200);
    
    // Set timezone to Eastern Time (EDT)
    setenv("TZ", "EST5EDT,M3.2.0/2,M11.1.0", 1);
    tzset();

    // Initialize Dallas temperature sensor
    sensors.begin();
    if (!sensors.getAddress(insideThermometer, 0)) {
        Serial.println("Unable to find address for Device 0");
    }
    sensors.setResolution(insideThermometer, 9);

    // Initialize temperature log
    for (int i = 0; i < logArraySize; i++) {
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
    Serial.print("IP address: ");
    Serial.println(WiFi.localIP());

    // Initialize time
    configTime(0, 0, "pool.ntp.org", "time.nist.gov"); // NTP server
    server.begin();

    // Initialize SMTP session

}

void loop() {
    unsigned long currentMillis = millis();

    // Log temperature every logIntervalSeconds seconds
    if (currentMillis - lastLogTime >= logIntervalSeconds * 1000) {
        lastLogTime = currentMillis;
        logTemperature();
    }

    if(currentMillis >= 172800000){
      ESP.restart();
    }

    // Check temperature thresholds
    float currentTemp = sensors.getTempC(insideThermometer);
    handleClient();
}

void logTemperature() {
    sensors.requestTemperatures();
    float currentTemp = sensors.getTempC(insideThermometer);
      if (currentTemp > MAX_TEMP || currentTemp < MIN_TEMP) {
        sendTemperatureAlert(currentTemp);
    }
    updateTemperatureLog(currentTemp);
}

void updateTemperatureLog(float temp) {
    // Shift all data down by one index
    for (int i = 0; i < logArraySize - 1; i++) {
        temperatureLog[i] = temperatureLog[i + 1];
        timeLog[i] = timeLog[i + 1];
    }

    // Store new value at index logArraySize - 1
    time_t now = time(nullptr);
    now -= 4 * 3600; // Adjust for UTC-4 (Eastern Daylight Time)
    struct tm* timeInfo = localtime(&now);
    timeLog[logArraySize - 1] = String(timeInfo->tm_hour) + ":" + (timeInfo->tm_min < 10 ? "0" : "") + String(timeInfo->tm_min);
    temperatureLog[logArraySize - 1] = temp;

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
    htmlResponse += "<body><h1>Flower Storage</h1>";
    htmlResponse += "<p style=\"font-size: 48px;\">Temperature: <span id=\"temperature\">" + String(temperatureLog[logArraySize - 1]) + " &deg;C</span></p>";
    htmlResponse += "<h2>Temperature Log (Last 12 hours)</h2><div style='width:100%; height:300px;'><canvas id='tempGraph' width='800' height='400'></canvas></div>";
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
    htmlResponse += "setInterval(updateTemperature, " + String(logIntervalSeconds * 1000) + ");"; // Update every logIntervalSeconds seconds
    htmlResponse += "initializeGraph();"; // Initialize the graph on page load
    htmlResponse += "</script>";
    htmlResponse += "</body></html>";
    Serial.println(htmlResponse);
}

String getTimeLabels() {
    String labels = "";
    for (int i = 0; i < logArraySize; i++) {
        labels += "\"" + timeLog[i] + "\"";
        if (i < logArraySize - 1) {
            labels += ",";
        }
    }
    return labels;
}

String getTemperatureData() {
    String data = "";
    for (int i = 0; i < logArraySize; i++) {
        data += String(temperatureLog[i]);
        if (i < logArraySize - 1) {
            data += ",";
        }
    }
    return data;
}

void sendTemperatureAlert(float currentTemp) {
    SMTP_Message message;
    message.sender.name = "ESP32";
    message.sender.email = AUTHOR_EMAIL;
    message.subject = F("Temperature Alert!");
    message.addRecipient(F("NAME"), RECIPIENT_EMAIL);

    String messageBody = "Current temperature is ";
    messageBody += String(currentTemp);
    messageBody += " °C. This exceeds acceptable limits.";
    message.text.content = messageBody.c_str();
    message.text.charSet = "us-ascii";
    message.text.transfer_encoding = Content_Transfer_Encoding::enc_7bit;
   
       smtp.debug(1); // Enable debug output for SMTP
    Session_Config config;
    config.server.host_name = SMTP_HOST;
    config.server.port = SMTP_PORT;
    config.login.email = AUTHOR_EMAIL;
    config.login.password = AUTHOR_PASSWORD;
    config.login.user_domain = "";

      config.time.ntp_server = F("pool.ntp.org,time.nist.gov");
      config.time.gmt_offset = 4;
      config.time.day_light_offset = 0;

    if (!smtp.connect(&config)) {
        Serial.printf("SMTP connection failed, Error code: %d\n", smtp.errorCode());
        while (1);
    }

    Serial.println("SMTP Connected.");
    if (!smtp.connect(&config)){
    ESP_MAIL_PRINTF("Connection error, Status Code: %d, Error Code: %d, Reason: %s", smtp.statusCode(), smtp.errorCode(), smtp.errorReason().c_str());
    return;
    }
    if (!smtp.isLoggedIn()){
    Serial.println("\nNot yet logged in.");
    }
    else{
    if (smtp.isAuthenticated())
      Serial.println("\nSuccessfully logged in.");
    else
      Serial.println("\nConnected with no Auth.");
    }
    if (!MailClient.sendMail(&smtp, &message)) {
        Serial.printf("Failed to send email, Error code: %d\n", smtp.errorCode());
    }
}

void handleClient() {
    WiFiClient client = server.available();   // Listen for incoming clients

    if (client) {                             // If a new client connects,
        Serial.println("New Client.");         // print a message out in the serial port
        String currentLine = "";               // make a String to hold incoming data from the client
        while (client.connected()) {           // loop while the client's connected
            if (client.available()) {          // if there's bytes to read from the client,
                char c = client.read();        // read a byte, then
                Serial.write(c);               // print it out the serial monitor
                if (c == '\n') {               // if the byte is a newline character

                    if (currentLine.length() == 0) {
                        // send a standard HTTP response header
                        client.println("HTTP/1.1 200 OK");
                        client.println("Content-type:text/html");
                        client.println("Connection: close");
                        client.println();

                        // Send the HTML webpage
                        client.print(htmlResponse);

                        break;
                    } else {
                        // Reset the currentLine variable,
                        currentLine = "";
                    }
                } else if (c != '\r') {          // if you got anything else but a carriage return character,
                    currentLine += c;            // add it to the end of the currentLine
                }
            }
        }
        delay(10);                             // give the web browser time to receive the data
        client.stop();                         // close the connection
        Serial.println("Client disconnected.");
        Serial.println("");
    }
}
