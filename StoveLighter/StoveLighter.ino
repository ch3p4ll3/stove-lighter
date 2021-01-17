#include <WiFi.h>
#include "time.h"
#include <WiFiClient.h>
#include <WebServer.h>
#include <ESPmDNS.h>
#include <time.h>
#include <WiFiUdp.h>
#include <ArduinoOTA.h>
#include <EEPROM.h>

#ifdef ESP8266
#include <sys/time.h>
#endif

#include "CronAlarms.h"

const char* ssid       = "WiFi_SSID";
const char* password   = "WiFi_PSK";
const char* ntpServer = "pool.ntp.org";
const long  gmtOffset_sec = 3600;
const int   daylightOffset_sec = 3600;
const String days[7] = {"Monday", "Tuesday", "Wednesday", "Thursday", "Friday", "Saturday", "Sunday"};

bool accendi = false;
int period = 2000;
unsigned long time_now = 0;
CronID_t ids[7];
struct hour {
  String On[7];
};

unsigned long currentTime = millis();
// Previous time
unsigned long previousTime = 0; 
// Define timeout time in milliseconds (example: 2000ms = 2s)
const long timeoutTime = 2000;
String header;

WebServer server(80);

void setup() {
  pinMode(2, OUTPUT);
  EEPROM.begin(sizeof(hour));
  Serial.begin(115200);
  Serial.printf("Connecting to %s ", ssid);
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
      delay(500);
      Serial.print(".");
  }
  Serial.println(" CONNECTED");
  Serial.println(WiFi.localIP());
  initServer();
  initOta();

  struct tm tm_newtime = getLocalTime();
#ifdef ESP8266
  timeval tv = { mktime(&tm_newtime), 0 };
  timezone tz = { 0, 0};
  settimeofday(&tv, &tz);
#elif defined(__AVR__)
  set_zone(0);
  set_dst(0);
  set_system_time( mktime(&tm_newtime) );
#endif
  updateCron();
  // create the alarms, to trigger at specific times
}

void loop() {
#ifdef __AVR__
  system_tick(); // must be implemented at 1Hz
#endif
  Cron.delay();// if the loop has nothing else to do, delay in ms
                // should be provided as argument to ensure immediate
                // trigger when the time is reached
  if((millis() - time_now >= period) && accendi){
    digitalWrite(2, LOW);
    accendi = false;
    Serial.println("OH YES BOY");
  }
  server.handleClient();
  ArduinoOTA.handle();
}

void AccendiStufa() {
  accendi = true;
  time_now = millis();
  digitalWrite(2, HIGH);
  Serial.println("STOVE ON");
}

tm getLocalTime(){
  struct tm timeinfo;
  configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);
  while(!getLocalTime(&timeinfo)){
    Serial.println("Failed to obtain time");
    configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);
    delay(1000);
  }
  Serial.println(&timeinfo, "%A, %B %d %Y %H:%M:%S");
  return timeinfo;
}

void initOta(){
  ArduinoOTA.setHostname("esp32-stove");
  ArduinoOTA
    .onStart([]() {
      String type;
      if (ArduinoOTA.getCommand() == U_FLASH)
        type = "sketch";
      else // U_SPIFFS
        type = "filesystem";

      // NOTE: if updating SPIFFS this would be the place to unmount SPIFFS using SPIFFS.end()
      Serial.println("Start updating " + type);
    })
    .onEnd([]() {
      Serial.println("\nEnd");
    })
    .onProgress([](unsigned int progress, unsigned int total) {
      Serial.printf("Progress: %u%%\r", (progress / (total / 100)));
    })
    .onError([](ota_error_t error) {
      Serial.printf("Error[%u]: ", error);
      if (error == OTA_AUTH_ERROR) Serial.println("Auth Failed");
      else if (error == OTA_BEGIN_ERROR) Serial.println("Begin Failed");
      else if (error == OTA_CONNECT_ERROR) Serial.println("Connect Failed");
      else if (error == OTA_RECEIVE_ERROR) Serial.println("Receive Failed");
      else if (error == OTA_END_ERROR) Serial.println("End Failed");
    });

  ArduinoOTA.begin();
  Serial.println("OTA started");
}

void split(String data, char separator, int out[2]){
    int count = 0;
    String ar[2];

    for (int i=0; i<data.length(); i++){
        if (data[i] == separator){
            count++;
        }

        else{
            ar[count] += data[i];
        }
    }

    out[0] = ar[0].toInt();
    out[1] = ar[1].toInt();
}

void updateCron(){
  for (int i = 0; i<7; i++){
    Cron.free(ids[i]);
    ids[i] = dtINVALID_ALARM_ID;
  }

  hour hours;
  EEPROM.get(0, hours);

  for (int i = 0; i<7; i++){
    if (hours.On[i] != ""){
      int Onri[2];
      String temp;
      split(hours.On[i], ':', Onri);
      temp = "0 " + String(Onri[1]) + " " + String(Onri[0]) + " * * " + String(i+1);
      char cron[sizeof(temp)+10];
      temp.toCharArray(cron, sizeof(temp)+10);
      Serial.println(cron);
      ids[i] = Cron.create(cron, AccendiStufa, false);
    }
  }
}

void handleRoot() {
  String temp;

  if (server.args()){
    if(server.argName(0) == "accendi" && server.arg(0) == "true" || server.argName(0) == "spegni" && server.arg(0) == "true"){
      AccendiStufa();
    }

    else {
      hour hours;
      for (int i = 0; i < server.args(); i++) {
        Serial.println(server.arg(i));
        hours.On[i] = server.arg(i);
      }
      EEPROM.put(0, hours);
      EEPROM.commit();
      updateCron();
    }
  }
 
  temp = "<script>"
    "function yesnoCheck(that) {"
      "if (that.checked) {"
          "document.getElementById('h'+that.getAttribute('value')).style.display = 'block';"
      "} else {"
          "document.getElementById('h'+that.getAttribute('value')).style.display = 'none';"
      "}"
    "}"
  "</script>"
  "<form action='./' method='GET'>"
    "<input type='checkbox' value='1' onchange='yesnoCheck(this);'>"
    "Monday <input type='time' name='1' id='h1' style='display: none;'><br>"
    "<input type='checkbox' value='2' onchange='yesnoCheck(this);'>"
    "Tuesday <input type='time' name='2' id='h2' style='display: none;'><br>"
    "<input type='checkbox' value='3' onchange='yesnoCheck(this);'>"
    "Wednesday <input type='time' name='3' id='h3' style='display: none;'><br>"
    "<input type='checkbox' value='4' onchange='yesnoCheck(this);'>"
    "Thursday <input type='time' name='4' id='h4' style='display: none;'><br>"
    "<input type='checkbox' value='5' onchange='yesnoCheck(this);'>"
    "Friday <input type='time' name='5' id='h5' style='display: none;'><br>"
    "<input type='checkbox' value='6' onchange='yesnoCheck(this);'>"
    "Saturday <input type='time' name='6' id='h6' style='display: none;'><br>"
    "<input type='checkbox' value='7' onchange='yesnoCheck(this);'>"
    "Sunday <input type='time' name='7' id='h7' style='display: none;'><br>"
    "<input type='submit'>"
    "<input type='reset'>"
  "</form>"
  "<a href='./?accendi=true'>Accendi</a> | "
  "<a href='./?spegni=true'>Spegni</a>";
  server.send(200, "text/html", temp);
}

void handleNotFound() {
  String message = "File Not Found\n\n";
  message += "URI: ";
  message += server.uri();
  message += "\nMethod: ";
  message += (server.method() == HTTP_GET) ? "GET" : "POST";
  message += "\nArguments: ";
  message += server.args();
  message += "\n";

  for (int i = 0; i < server.args(); i++) {
    message += " " + server.argName(i) + ": " + server.arg(i) + "\n";
  }

  server.send(404, "text/plain", message);
}

void ontime(){
  hour hours;
  String msg;
  EEPROM.get(0, hours);
  for(int i=0; i<7; i++){
    if (hours.On[i] != ""){
      msg += days[i] + ": " + hours.On[i] + "\n";
    }
  }
  server.send(200, "text/plain", msg); 
}

void initServer(){
  if (MDNS.begin("esp32")) {
    Serial.println("MDNS responder started");
  }

  server.on("/", handleRoot);
  server.on("/ontime", ontime);
  server.onNotFound(handleNotFound);
  server.begin();
  Serial.println("HTTP server started");
}
