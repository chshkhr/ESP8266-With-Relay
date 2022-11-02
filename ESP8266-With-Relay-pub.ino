#define TELEGRAM
#define BLYNK
#define VERSION "1.0.18"

#ifdef TELEGRAM
#define SKETCH_VERSION VERSION " Tg"
#else
#define SKETCH_VERSION VERSION
#endif

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
#include <Regexp.h>

#ifdef TELEGRAM
#include <AsyncTelegram2.h>

// Timezone definition
#include <time.h>
#define MYTZ "CET-1CEST,M3.5.0,M10.5.0/3"

BearSSL::WiFiClientSecure clientsec;
BearSSL::Session session;
BearSSL::X509List certificate(telegram_cert);

AsyncTelegram2 myBot(clientsec);

char token[50];
int64_t userid = 123456789;
bool tgavail;

// Name of public channel (your bot must be in admin group)
char* channel = "@YorChannel";
#endif

ESP8266WiFiMulti wifiMulti;

const int numssisds = 5;
const char* ssids[numssisds] = { "WiFi-AP1", "WiFi-AP2", "WiFi-AP3" };
const char* pass = "your wifi password";
const int numpcs = 4;
const char* pcs[numpcs][6] = {
  { "192.168.0.193", "PC name 1", "BLYNK_AUTH_TOKEN_1", "d", "192.168.0.4", "telegram Token 1" },
  { "192.168.0.194", "PC name 2", "BLYNK_AUTH_TOKEN_2", "d", "192.168.0.34", "telegram Token 2" },
  { "192.168.0.195", "PC name 3 (local only)", "", "r", "192.168.0.50", "telegram Token 3" },
  { "192.168.0.196", "PC name 4 (local only)", "", "r", "192.168.0.3", "telegram Token 4" }
};

String ssid;
String device, deviceip;
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

String postFormRoot;
String postFormUpdate;

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
  initpostFormRoot();
  server.send(200, "text/html", top + postFormRoot + bot);
  digitalWrite(led, 0);
}

void handleUpdForm() {
  digitalWrite(led, 1);
  initpostFormUpdate();
  server.send(200, "text/html", top + postFormUpdate + bot);
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

void (*restartFunc)(void) = 0;  //declare restart function @ address 0

#ifdef TELEGRAM
void tgChannelSend(String s) {
  if (tgavail) {
    String message;
    message += "@";
    message += myBot.getBotName();
    message += " (";
    message += device;
    message += "):\n";
    message += s;
    Serial.println(message);
    myBot.sendToChannel(channel, message, true);
  }
}
#endif


void doRestart() {
  do_restart = false;
#ifdef TELEGRAM
  tgChannelSend("Restarting...");
  // Wait until bot synced with telegram to prevent cyclic reboot
  while (!myBot.noNewMessage()) {
    Serial.print(".");
    delay(100);
  }
  Serial.println("Restart in 5 seconds...");
  delay(5000);
#endif
  ESP.restart();
}

void doUpdate() {
  do_update = false;
#ifdef TELEGRAM
  tgChannelSend("Updating...");
  // Wait until bot synced with telegram to prevent cyclic reboot
  while (!myBot.noNewMessage()) {
    Serial.print(".");
    delay(100);
  }
  Serial.println("Update in 5 seconds...");
  delay(5000);
#endif
  update(fwfn);
}


void updateOtherDevice(String devip, String firmware) {
  if (WiFi.status() == WL_CONNECTED) {

    WiFiClient client;
    HTTPClient http;

    String url = "http://" + devip + "/remote/" + "?pswupd=" + WEB_PASSWORD + "&firmware=" + firmware;
    http.begin(client, url);  //HTTP
    http.addHeader("Content-Type", "text/html");

    //Serial.println(url);

    int httpCode = http.GET();

    //Serial.println(httpCode);

    // httpCode will be negative on error
    if (httpCode > 0) {
      // HTTP header has been send and Server response header has been handled
      //Serial.printf("UPDATE %s: %d\n", devip, httpCode);

      // file found at server
      if (httpCode == HTTP_CODE_OK) {
        const String& payload = http.getString();
        //Serial.println("received payload:\n<<");
        Serial.println(payload);
        //Serial.println(">>");
      }
    } else {
      Serial.printf("UPDATE %s failed, error: %s\n",
                    devip,
                    http.errorToString(httpCode).c_str());
    }

    http.end();
  }
}

int iListIndex = -1;

void updateOthers(String firmware) {
  for (int i = 0; i < numpcs; i++) {
    if (i != iListIndex) {
      Serial.println(deviceip + " is sending update request to " + pcs[i][0]);
      updateOtherDevice(pcs[i][0], firmware);
    }
  }
};

void update(String firmware) {
  Serial.print("Update with ");
  Serial.print(firmware);
  Serial.println("...");

  WiFiClient client;

  ESPhttpUpdate.setLedPin(LED_BUILTIN, LOW);

  ESPhttpUpdate.onStart(update_started);
  ESPhttpUpdate.onEnd(update_finished);
  ESPhttpUpdate.onProgress(update_progress);
  ESPhttpUpdate.onError(update_error);

#ifdef TELEGRAM
  tgChannelSend(firmware);
#endif

  t_httpUpdate_return ret = ESPhttpUpdate.update(client, FIRMWARE_FOLDER + firmware);

  switch (ret) {
    case HTTP_UPDATE_FAILED:
#ifdef TELEGRAM
      tgChannelSend(ESPhttpUpdate.getLastErrorString().c_str());
#else
      Serial.printf("HTTP_UPDATE_FAILD Error (%d): %s\n", ESPhttpUpdate.getLastError(), ESPhttpUpdate.getLastErrorString().c_str());
#endif
      do_restart = true;
      break;

    case HTTP_UPDATE_NO_UPDATES:
      Serial.println("HTTP_UPDATE_NO_UPDATES");
      do_restart = true;
      break;

    case HTTP_UPDATE_OK:
#ifdef TELEGRAM
      tgChannelSend("HTTP_UPDATE_OK");
#else
      Serial.println("HTTP_UPDATE_OK");
#endif
      break;
  }
}

void handleRemote() {
  digitalWrite(led, 1);
  server.send(200, "text/plain", "- OK -");
  if (server.arg("pswupd").equals(WEB_PASSWORD)) {
    update(server.arg("firmware"));
  };
  digitalWrite(led, 0);
}

void handleUpdate() {
  if (server.method() != HTTP_POST) {
    digitalWrite(led, 1);
    server.send(405, "text/plain", "Method Not Allowed");
    digitalWrite(led, 0);
  } else {
    digitalWrite(led, 1);
    char buffer[40];
    String message = top;

    //message += server.arg("plain");

    message += "<table><td>";

    bool pswcorrect = server.arg("pswupd").equals(WEB_PASSWORD);
    if (pswcorrect) {
      //message += "<p style=\"background-color:MediumSeaGreen;\">SUCCESS</p>";

      if (server.arg("firmware").equals("restart")) {
        message += "<p>Restarting...</p>";
      } else {

        if (server.arg("alldev").equals("yes")) {
          message += "<p>UPDATE ALL DEVICES</p>";
          updateOthers(server.arg("firmware"));
        } else {
          message += "<p>UPDATE STARTED</p>";
        }

        message += server.arg("firmware");
        message += "</td></table>";
        message += bot;
        server.send(200, "text/html", message);
        digitalWrite(led, 0);
        update(server.arg("firmware"));
      }

    } else {
      message += "<p style=\"background-color:Tomato;\">FAIL</p>";
    }

    message += "</td></table>";
    message += bot;
    server.send(200, "text/html", message);
    digitalWrite(led, 0);

    if (pswcorrect && server.arg(1).equals("restart")) {
      do_restart = true;
    }
  }
}

void switcher(int waitmsec) {
#ifdef TELEGRAM
  tgChannelSend(String("Switch ") + waitmsec);
#endif
  digitalWrite(pin, L1);
  delay(waitmsec);
  digitalWrite(pin, L0);
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
      switcher(server.arg(1).toInt());
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
      //Serial.println(cap);
      String s(cap);
      if (s.endsWith(".bin")) {
        postFormUpdate += "<option value=\"" + s + "\">" + s + "</option>";
      }
    }

  }  // end of for each capture

}  // end of match_callback

void initpostFormRoot(void) {
  String butLbl, color;

  Serial.print("Ping ");
  Serial.print(serverip);
  if (Ping.ping(serverip)) {
    Serial.println(" succesful");
    butLbl = "Turn Off";
    color = "red";
  } else {
    Serial.println(" failed");
    butLbl = "Turn On";
    color = "gray";
  }

  postFormRoot = "<form method=\"post\" enctype=\"application/x-www-form-urlencoded\" action=\"/switch/\">\
      <table>\
      <tr><td><label for=\"password\">Password: </label></td>\
      <td><input type=\"password\" id=\"password\" name=\"password\" value=\"\" required></td></tr>\
      <tr><td><label for=\"delay\">Delay: </label></td> \
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
      <tr><td><span class=\"dot"
                 + color + "\"></span></td>\
      <td align=\"right\"><input type=\"submit\" value=\""
                 + butLbl + "\"></td></tr>\
      </table>\
      </form>\
      <a href=\"/updform/\">Update firmware</a>";
}

void initpostFormUpdate(void) {
  postFormUpdate = "<form method=\"post\" enctype=\"application/x-www-form-urlencoded\" action=\"/update/\">\
      <table>\
      <tr><td><label for=\"pswupd\">Password: </label></td>\
      <td><input type=\"password\" id=\"pswupd\" name=\"pswupd\" value=\"\" required></td></tr>\
      <tr><td><label for=\"firmware\">Firmware: </label></td> \
      <td align=\"right\"><select id=\"firmware\" name=\"firmware\">\
      <option value=\"restart\">Restart</option>";

  WiFiClient client;
  HTTPClient http;

  //Serial.print("[HTTP] begin...\n");
  if (http.begin(client, FIRMWARE_FOLDER)) {  // HTTP


    //Serial.print("[HTTP] GET...\n");
    // start connection and send HTTP header
    int httpCode = http.GET();

    // httpCode will be negative on error
    if (httpCode > 0) {
      // HTTP header has been send and Server response header has been handled
      //Serial.printf("[HTTP] GET... code: %d\n", httpCode);

      // file found at server
      if (httpCode == HTTP_CODE_OK || httpCode == HTTP_CODE_MOVED_PERMANENTLY) {
        String payload = http.getString();
        //Serial.println(payload);

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

  postFormUpdate += "</select></td></tr>\
    <tr><td><label for=\"alldev\">All devices: </label></td>\
    <td><input type=\"checkbox\" id=\"alldev\" name=\"alldev\" value=\"yes\"></td></tr> \
    <tr><td></td><td align=\"right\"><input type=\"submit\" value=\"Restart/Update\"></td></tr>\
    </table>\
    </form>\
    <a href=\"/\">Switcher</a>";
}

void setup(void) {
  WiFi.persistent(false);

  pinMode(led, OUTPUT);
  digitalWrite(led, 0);
  Serial.begin(115200);

  Serial.print("Version: "
               "\n");

  // Set WiFi to station mode
  WiFi.mode(WIFI_STA);

  // Register multi WiFi networks
  for (int i = 0; i < numssisds; i++) {
    //Serial.println(ssids[i]);
    wifiMulti.addAP(ssids[i], pass);
  }

  Serial.println("Connecting...");
  if (wifiMulti.run(connectTimeoutMs) == WL_CONNECTED) {
    ssid = WiFi.SSID();
    deviceip = WiFi.localIP().toString();
    Serial.println(String("Connected: ") + ssid + " - " + deviceip);

    for (int i = 0; i < numpcs; i++) {
      if (deviceip.equals(pcs[i][0])) {
        iListIndex = i;
        device = pcs[i][1];
        serverip.fromString(pcs[i][4]);
        if (pcs[i][3] == "r") {
          L0 = 1;
          L1 = 0;
        }
        pinMode(pin, OUTPUT);
        digitalWrite(pin, L1);
        digitalWrite(pin, L0);

#ifdef TELEGRAM
        tgavail = strlen(pcs[i][5]) > 0;
        if (tgavail) {
          String(pcs[i][5]).toCharArray(token, strlen(pcs[i][5]) + 1);
        }
#endif

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
            border-radius: 5px;\
            padding: 20px;\
            margin-top: 10px;\
            margin-bottom: 10px;\
            margin-right: 10px;\
            margin-left: 30px;\
            background-color: lightblue;\
          }\
          .dotgray {\
            height: 20px;\
            width: 20px;\
            background-color: #bbb;\
            border-radius: 50%;\
            border: 1px solid #4CAF50;\
            display: inline-block;\
          }\
          .dotred {\
            height: 20px;\
            width: 20px;\
            background-color: red;\
            border-radius: 50%;\
            border: 1px solid #4CAF50;\
            display: inline-block;\
          }\
        </style>\
      </head>\
      <body>\
        <h1>"
          + device + "</h1>" + ssid + "<br><small>" SKETCH_VERSION "</small><br>\
        <div>";

    server.on("/", handleRoot);
    server.on("/updform/", handleUpdForm);
    server.on("/update/", handleUpdate);
    server.on("/switch/", handleSwitch);
    server.on("/remote/", handleRemote);
    server.onNotFound(handleNotFound);
    server.begin();

#ifdef TELEGRAM
    if (tgavail) {
      // Sync time with NTP, to check properly Telegram certificate
      configTime(MYTZ, "time.google.com", "time.windows.com", "pool.ntp.org");
      //Set certficate, session and some other base client properies
      clientsec.setSession(&session);
      clientsec.setTrustAnchors(&certificate);
      clientsec.setBufferSizes(1024, 1024);
      // Set the Telegram bot properies
      myBot.setUpdateTime(2000);
      myBot.setTelegramToken(token);

      // Check if all things are ok
      Serial.print("\nTest Telegram connection... ");
      myBot.begin() ? Serial.println("OK") : Serial.println("NOK");

      //char welcome_msg[128];
      //snprintf(welcome_msg, 128, "%s\nBOT @%s online", device, myBot.getBotName());

      // Send a message to specific user who has started your bot
      //myBot.sendTo(userid, welcome_msg);
      tgChannelSend(SKETCH_VERSION " Online now");
    }
#endif

  } else {
    Serial.println("Can't connect to Network");
    delay(5000);
    Serial.println("Restart...");
    do_restart = true;
  }
}

void loop(void) {
  if (do_restart)
    doRestart();

  if (do_update)
    doUpdate();

  if (WiFi.status() == WL_CONNECTED) {
    server.handleClient();
#ifdef BLYNK
    if (blynk) {
      Blynk.run();
    }
#endif

#ifdef TELEGRAM
    if (tgavail) {
      // local variable to store telegram message data
      TBMessage msg;

      // if there is an incoming message...
      if (myBot.getNewMessage(msg)) {

        int l = msg.text.length() + 1;
        char buf[l];
        msg.text.toCharArray(buf, l);

        char* command = strtok(buf, " ");
        String s = "";
        if (strcmp(command, "switch") == 0) {
          s = strtok(NULL, " ");
          int dl = s.toInt();
          if (dl == 0) {
            dl = bldelay;
          }
          s = "Switch " + dl;
          switcher(dl);
        } else if ((strcmp(command, "ping") == 0)) {
          s = String("Ping ") + serverip.toString();
          myBot.sendMessage(msg, s);
          if (Ping.ping(serverip)) {
            s += " SUCCESS";
          } else {
            s += " FAIL";
          }
          tgChannelSend(s);      
        } else if ((strcmp(command, "pingall") == 0)) {
          myBot.sendMessage(msg, "Pinging all...");
          IPAddress ip;
          for (int i = 0; i < numpcs; i++) {
            ip.fromString(pcs[i][4]);
            s += pcs[i][1];
            if (Ping.ping(ip)) {
              s += " - SUCCESS";
            } else {
              s += " - FAIL";
            }
            ip.fromString(pcs[i][0]);
            if (i == iListIndex || Ping.ping(ip)) {
              s += " bot online\n";
            } else {
              s += " bot offline\n";
            }
          }
          tgChannelSend(s);
        } else if ((strcmp(command, "restart") == 0)) {
          s = "Restarting...";
          tgChannelSend(s);
          do_restart = true;
        } else if ((strcmp(command, "update") == 0)) {
          s = "Updating...";
          fwfn = strtok(NULL, " ");
          do_update = true;
        } else if ((strcmp(command, "updateall") == 0)) {
          s = "Updating all...";
          fwfn = strtok(NULL, " ");
          updateOthers(fwfn);
          do_update = true;
        } else {
          s = String("Unknown: ") + command;
        }
        myBot.sendMessage(msg, s);
      }
    }
#endif
  } else {
    Serial.println("Connection lost. Restart...");
    //restartFunc();
    do_restart = true;
  }
}