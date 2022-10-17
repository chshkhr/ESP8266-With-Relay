//#define L_REVERSE
#define BLYNK

#ifdef BLYNK
#define BLYNK_TEMPLATE_ID "TMPLwvNcgN8Z"
#define BLYNK_DEVICE_NAME "switch"
bool blynk = false;
#endif

#define WEB_PASSWORD "your password"
#define FIRMWARE_FOLDER "http://192.168.0.43/espfw/"

int L0 = 0;
int L1 = 1;

#include <ESP8266WiFi.h>
#include <WiFiClient.h>
#include <ESP8266WebServer.h>
#include <ESP8266mDNS.h>
#include <ESP8266Ping.h>

#ifdef BLYNK
#include <BlynkSimpleEsp8266.h>
#endif

#include <ESP8266WiFiMulti.h>

#include <Arduino.h>
#include <ESP8266HTTPClient.h>
#include <ESP8266httpUpdate.h>

#include <ESP8266HTTPClient.h>
#include <Regexp.h>

ESP8266WiFiMulti wifiMulti;

const int numssisds = 5;
const char* ssids[numssisds] = { "WiFi-AP1", "WiFi-AP2", "WiFi-AP3" };
const char* pass = "your wifi password";
const int numpcs = 4;
const char* pcs[numpcs][5] = {
  { "192.168.0.193", "PC name 1", "BLYNK_AUTH_TOKEN_1", "d", "192.168.0.4" },
  { "192.168.0.194", "PC name 2", "BLYNK_AUTH_TOKEN_2", "d", "192.168.0.34" },
  { "192.168.0.195", "PC name 3 (local only)", "", "r", "192.168.0.50" },
  { "192.168.0.196", "PC name 4 (local only)", "", "r", "192.168.0.3" }
};

String ssid;
String device;
IPAddress serverip;

const int bldelay = 300;

ESP8266WebServer server(80);

const int led = LED_BUILTIN;
const int pin = 0;  // using GP0 esp8266

String top;

const String bot = "\
    </div>\
  </body>\
</html>";

String postForms;

#ifdef BLYNK
BLYNK_WRITE(V0) {
  //int value = param.asInt();
  digitalWrite(pin, L1);
  //Blynk.virtualWrite(V0, 1);
  delay(bldelay);
  digitalWrite(pin, L0);
  //Blynk.virtualWrite(V0, 0);
}
#endif

void handleRoot() {
  digitalWrite(led, 1);
  server.send(200, "text/html", top + postForms + bot);
  digitalWrite(led, 0);
}

void update_started() {
  Serial.println("CALLBACK:  HTTP update process started");
}

void update_finished() {
  Serial.println("CALLBACK:  HTTP update process finished");
}

void update_progress(int cur, int total) {
  Serial.printf("CALLBACK:  HTTP update process at %d of %d bytes...\n", cur, total);
}

void update_error(int err) {
  Serial.printf("CALLBACK:  HTTP update fatal error code %d\n", err);
}

void (*resetFunc)(void) = 0;  //declare reset function @ address 0

void handleUpdate() {
  if (server.method() != HTTP_POST) {
    digitalWrite(led, 1);
    server.send(405, "text/plain", "Method Not Allowed");
    digitalWrite(led, 0);
  } else {
    digitalWrite(led, 1);
    char buffer[40];
    String message = top;

    message += "<table><td>";
    if (server.arg(0).equals(WEB_PASSWORD)) {
      //message += "<p style=\"background-color:MediumSeaGreen;\">SUCCESS</p>";

      if (server.arg(0).equals("reset")) {
        message += "<p>RESETING</p>";
      } else {
        message += "<p>UPDATE STARTED</p>";
      }

      message += "</td></table>";
      message += bot;
      server.send(200, "text/html", message);
      digitalWrite(led, 0);

      if (server.arg(0).equals("reset")) {
        resetFunc();
      }

      WiFiClient client;

      ESPhttpUpdate.setLedPin(LED_BUILTIN, LOW);

      ESPhttpUpdate.onStart(update_started);
      ESPhttpUpdate.onEnd(update_finished);
      ESPhttpUpdate.onProgress(update_progress);
      ESPhttpUpdate.onError(update_error);


      //Serial.println(server.arg(1));

      t_httpUpdate_return ret = ESPhttpUpdate.update(client, "http://192.168.108.43:8080/win/winweb/espfw/" + server.arg(1));

      switch (ret) {
        case HTTP_UPDATE_FAILED:
          Serial.printf("HTTP_UPDATE_FAILD Error (%d): %s\n", ESPhttpUpdate.getLastError(), ESPhttpUpdate.getLastErrorString().c_str());
          resetFunc();
          break;

        case HTTP_UPDATE_NO_UPDATES:
          Serial.println("HTTP_UPDATE_NO_UPDATES");
          resetFunc();
          break;

        case HTTP_UPDATE_OK:
          Serial.println("HTTP_UPDATE_OK");
          break;
      }

    } else {
      message += "<p style=\"background-color:Tomato;\">FAIL</p>";
    }
  }
}

void handleSwitch() {
  if (server.method() != HTTP_POST) {
    digitalWrite(led, 1);
    server.send(405, "text/plain", "Method Not Allowed");
    digitalWrite(led, 0);
  } else {
    digitalWrite(led, 1);
    char buffer[40];
    String message = top;

    /*
    message += "POST form was:\n";
    for (uint8_t i = 0; i < server.args(); i++) {
      message += " " + server.argName(i) + ": " + server.arg(i) + "\n";
    }
    */
    message += "<table><td>";
    if (server.arg(0).equals(WEB_PASSWORD)) {
      //message += "<p style=\"background-color:MediumSeaGreen;\">SUCCESS</p>";
      message += "<p>SUCCESS</p>";
      digitalWrite(pin, L1);
      delay(server.arg(1).toInt());
      digitalWrite(pin, L0);
    } else {
      message += "<p style=\"background-color:Tomato;\">FAIL</p>";
    }

    message += "</td></table>";
    message += bot;
    server.send(200, "text/html", message);
    digitalWrite(led, 0);
  }
}

void handleNotFound() {
  digitalWrite(led, 1);
  String message = "File Not Found\n\n";
  message += "URI: ";
  message += server.uri();
  message += "\nMethod: ";
  message += (server.method() == HTTP_GET) ? "GET" : "POST";
  message += "\nArguments: ";
  message += server.args();
  message += "\n";
  for (uint8_t i = 0; i < server.args(); i++) {
    message += " " + server.argName(i) + ": " + server.arg(i) + "\n";
  }
  server.send(404, "text/plain", message);
  digitalWrite(led, 0);
}

// WiFi connect timeout per AP. Increase when connecting takes longer.
const uint32_t connectTimeoutMs = 5000;

// called for each match
void match_callback(const char* match,          // matching string (not null-terminated)
                    const unsigned int length,  // length of matching string
                    const MatchState& ms)       // MatchState in use (to get captures)
{
  char cap[100];  // must be large enough to hold captures

  for (byte i = 0; i < ms.level; i++) {
    ms.GetCapture(cap, i);
    if (strcmp(cap, "../")) {
      Serial.println(cap);
      String s(cap);
      if (s.endsWith(".bin")) {
        postForms += "<option value=\"" + s + "\">" + s + "</option>";
      }
    }

  }  // end of for each capture

}  // end of match_callback

void initpostForms(void) {
  String butLbl;

  if (Ping.ping(serverip)) {
    Serial.println("Ping succesful.");
    butLbl = "Turn Off";
  } else {
    Serial.println("Ping failed");
    butLbl = "Turn On";
  }

  postForms = "<form method=\"post\" enctype=\"application/x-www-form-urlencoded\" action=\"/switch/\">\
      <table>\
      <tr><td><label for=\"password\">Password: </label></td>\
      <td><input type=\"password\" id=\"password\" name=\"password\" value=\"\" required></td></tr>\
      <tr><td><label for=id=\"delay\">Delay: </label></td> \
      <td align=\"right\"><select id=\"delay\" name=\"delay\">\
        <option value=\"50\">50ms</option>\
        <option value=\"100\">100ms</option>\
        <option value=\"200\">200ms</option>\
        <option value=\"300\" selected>300ms</option>\
        <option value=\"500\">500ms</option>\
        <option value=\"1000\">1s</option>\
        <option value=\"1500\">1.5s</option>\
        <option value=\"2000\">2s</option>\
        <option value=\"3000\">3s</option>\
        <option value=\"5000\">5s</option>\
        <option value=\"10000\">10s</option>\
        <option value=\"60000\">1m</option>\
        <option value=\"3600000\">1h</option>\
      </select></td></tr>\
      <tr><td></td><td align=\"right\"><input type=\"submit\" value=\""
              + butLbl + "\"></td></tr>\
      </table>\
      </form>\
      <form method=\"post\" enctype=\"application/x-www-form-urlencoded\" action=\"/update/\">\
      <table>\
      <tr><td><label for=\"password\">Password: </label></td>\
      <td><input type=\"password\" id=\"password\" name=\"password\" value=\"\" required></td></tr>\
      <tr><td><label for=id=\"delay\">Firmware: </label></td> \
      <td align=\"right\"><select id=\"firmware\" name=\"firmware\">\
      <option value=\"reset\">Reset</option>";

  WiFiClient client;
  HTTPClient http;

  Serial.print("[HTTP] begin...\n");
  if (http.begin(client, FIRMWARE_FOLDER)) {  // HTTP


    Serial.print("[HTTP] GET...\n");
    // start connection and send HTTP header
    int httpCode = http.GET();

    // httpCode will be negative on error
    if (httpCode > 0) {
      // HTTP header has been send and Server response header has been handled
      Serial.printf("[HTTP] GET... code: %d\n", httpCode);

      // file found at server
      if (httpCode == HTTP_CODE_OK || httpCode == HTTP_CODE_MOVED_PERMANENTLY) {
        String payload = http.getString();
        Serial.println(payload);

        unsigned long count;

        int payload_len = payload.length() + 1;
        char c_payload[payload_len];
        payload.toCharArray(c_payload, payload_len);

        // match state object
        MatchState ms(c_payload);

        count = ms.GlobalMatch("href=['\"]?([^'\" >]+)", match_callback);
      }
    } else {
      Serial.printf("[HTTP] GET... failed, error: %s\n", http.errorToString(httpCode).c_str());
    }

    http.end();
  } else {
    Serial.printf("[HTTP} Unable to connect\n");
  }

  postForms += "</select></td></tr>\
    <tr><td></td><td align=\"right\"><input type=\"submit\" value=\"Reset/Update\"></td></tr>\
    </table>\
    </form>";
}

void setup(void) {
  WiFi.persistent(false);

  pinMode(led, OUTPUT);
  digitalWrite(led, 0);
  Serial.begin(115200);

  // Set WiFi to station mode
  WiFi.mode(WIFI_STA);

  // Register multi WiFi networks
  for (int i = 0; i < numssisds; i++) {
    Serial.println(ssids[i]);
    wifiMulti.addAP(ssids[i], pass);
  }

  Serial.println("Connecting...");
  if (wifiMulti.run(connectTimeoutMs) == WL_CONNECTED) {
    ssid = WiFi.SSID();
    device = WiFi.localIP().toString();
    Serial.println(String("Connected: ") + ssid + " - " + device);

    for (int i = 0; i < numpcs; i++) {
      if (device.equals(pcs[i][0])) {
        device = pcs[i][1];
        serverip.fromString(pcs[i][4]);
        if (pcs[i][3] == "r") {
          L0 = 1;
          L1 = 0;
        }
        pinMode(pin, OUTPUT);
        digitalWrite(pin, L1);
        digitalWrite(pin, L0);

#ifdef BLYNK
        String auth = pcs[i][2];
        int auth_len = auth.length() + 1;
        blynk = auth.length() > 1;
        if (blynk) {
          char c_auth[auth_len + 1];
          auth.toCharArray(c_auth, auth_len);

          int ssid_len = ssid.length() + 1;
          char c_ssid[ssid_len];
          ssid.toCharArray(c_ssid, ssid_len);

          Blynk.begin(c_auth, c_ssid, pass);
          Blynk.virtualWrite(V0, 0);
        }
#endif
        break;
      }
    }

    top = "<head>\
        <title>"
          + device + "</title>\
        <style>\
          body { background-color: #cccccc; font-family: Arial, Helvetica, Sans-Serif; Color: #000088; }\
          table {\
            border: 1px solid #4CAF50;\
            padding: 20px;\
            margin-top: 10px;\
            margin-bottom: 10px;\
            margin-right: 10px;\
            margin-left: 30px;\
            background-color: lightblue;\
          }\
        </style>\
      </head>\
      <body>\
        <h1>"
          + device + "</h1>" + ssid + "<br>\
        <div>";

    initpostForms();

    server.on("/", handleRoot);
    server.on("/update/", handleUpdate);
    server.on("/switch/", handleSwitch);
    server.onNotFound(handleNotFound);
    server.begin();
  } else {
    Serial.println("Can't connect to Network");
    delay(5000);
    Serial.println("Reset...");
    resetFunc();
  }
}

int i = 0;

void loop(void) {
  if (WiFi.status() == WL_CONNECTED) {
    i++;
    if (i >= 1000) {
      initpostForms();
      i == 0;
    }
    server.handleClient();
#ifdef BLYNK
    if (blynk) {
      Blynk.run();
    }
#endif
  } else {
    Serial.println("Connection lost. Reset...");
    resetFunc();
  }
}