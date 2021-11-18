//#include <FastLED.h>

#include <FS.h>
#include <ESP8266WiFi.h>
#include <Updater.h>
#include <ArduinoJson.h>
#include <ezTime.h>


//---------------------------------------------------------------------
// configuration parameters

uint16_t dayStart = 8 * 60;
uint8_t brightnessDay[3] = {10,10,10}; //[0...10]
uint16_t dayEnd = 22 * 60;
uint8_t brightnessNight[3] = {1,1,1}; //[0...10]

char ntpUrl[128] = "pool.ntp.org";
char timezoneName[128] = "UTC";

char ssid_sta[128]     = "yourSSID";
char password_sta[128] = "Your WiFi password";


//---------------------------------------------------------------------
// WiFi

byte my_WiFi_Mode = WIFI_OFF;  // WIFI_STA = 1 = Workstation  WIFI_AP = 2  = Accesspoint

const char * ssid_ap = "colock-ap";
const char * password_ap = "colock-ap";

WiFiServer server(80);
WiFiClient client;

#define MAX_PACKAGE_SIZE 2048
char HTML_String[5000];
char HTTP_Header[150];


//---------------------------------------------------------------------
// time variables

Timezone myTZ;


//---------------------------------------------------------------------
// general variables

enum action_t
{
  ACTION_RESET = 0,
  ACTION_WRITE_CONFIG = 1,
  ACTION_WIFI_SET = 2,
  ACTION_COLOR_SET = 3,
  ACTION_TIME_SET = 4,
  ACTION_NTP_SET = 5
};

enum colorIndex_t
{
  BRIGHT_RED = 0,
  BRIGHT_GREEN = 1,
  BRIGHT_BLUE = 2
};



// forward declarations
void initDisplay();

void initConfig();
void loadConfig();
void saveConfig();

void initTime();
void handleTime();
bool isDayTime();

void showEmpty();
void showTime();

void WiFi_Start_STA();
void WiFi_Start_AP();
void WiFi_Traffic();

void make_HTML01();
void make_HTML_update();
void make_HTML_update_success();
void make_HTML_redirectHome(uint8_t delay);

void send_not_found();
void make_header();
void send_HTML();

void strcati(char* tx, int i);
void strcati2(char* tx, int i);
int Pick_Parameter_Zahl(const char * par, char * str);
int Find_End(const char * such, const char * str);
int Find_Start(const char * such, const char * str);
int Pick_Dec(const char * tx, int idx );
int Pick_N_Zahl(const char * tx, char separator, byte n);
int Pick_Hex(const char * tx, int idx );
void Pick_Text(char * tx_ziel, char  * tx_quelle, int max_ziel);
char HexChar_to_NumChar( char c);

void exhibit(const char * tx, int v);
void exhibit(const char * tx, unsigned int v);
void exhibit(const char * tx, unsigned long v);
void exhibit(const char * tx, const char * v);

String cstr2String(const char* cstr);



void setup()
{
  Serial.begin(74880);

  Serial.println("colock startup...");
  
  initDisplay();

  initConfig();

  showEmpty();

  WiFi_Start_STA();
  if (my_WiFi_Mode == WIFI_OFF)
  {
    WiFi_Start_AP();
  }

  initTime();
}

void loop()
{
  WiFi_Traffic();
  handleTime();
  delay(10);
}




void initDisplay()
{
  Serial1.begin(150000);
  showEmpty();
}



void initConfig()
{
  if (SPIFFS.begin())
  {
    Serial.println("mounted file system");
    loadConfig();
  }
  else
  {
    Serial.println("failed to mount filesystem - using default configuration.");
  }
}

void loadConfig()
{
  if (SPIFFS.exists("/config.json"))
  {
    Serial.println("reading config file");
    File configFile = SPIFFS.open("/config.json", "r");
    if (configFile)
    {
      Serial.println("opened config file");
      DynamicJsonDocument jsonDoc(2048);
      auto error = deserializeJson(jsonDoc, configFile);
      serializeJsonPretty(jsonDoc, Serial);
      if (!error)
      {
        Serial.println("\nparsed json");
        dayStart = jsonDoc.getMember("dayStart").as<uint16_t>();
        brightnessDay[0] = jsonDoc.getMember("dRed").as<uint8_t>();
        brightnessDay[1] = jsonDoc.getMember("dGreen").as<uint8_t>();
        brightnessDay[2] = jsonDoc.getMember("dBlue").as<uint8_t>();
        dayEnd = jsonDoc.getMember("dayEnd").as<uint16_t>();
        brightnessNight[0] = jsonDoc.getMember("nRed").as<uint8_t>();
        brightnessNight[1] = jsonDoc.getMember("nGreen").as<uint8_t>();
        brightnessNight[2] = jsonDoc.getMember("nBlue").as<uint8_t>();
        strcpy(timezoneName, jsonDoc.getMember("timezone").as<String>().c_str());
        strcpy(ntpUrl, jsonDoc.getMember("ntpUrl").as<String>().c_str());
        strcpy(ssid_sta, jsonDoc.getMember("ssid").as<String>().c_str());
        strcpy(password_sta, jsonDoc.getMember("password").as<String>().c_str());
      }
      else
      {
        Serial.print("failed to load json config with code ");
        Serial.print(error.c_str());
        Serial.println(" - using default configuration.");
      }
    }
    else
    {
      Serial.println("failed to open /config.json - using default configuration.");
    }
    configFile.close();
  }
  else
  {
    Serial.println("/config.json not present - using default configuration.");
  }
}

void saveConfig()
{
  Serial.println("writing config file");
  File configFile = SPIFFS.open("/config.json", "w");
  if (configFile)
  {
    Serial.println("opened config file");
    DynamicJsonDocument jsonDoc(2048);
    jsonDoc["dayStart"] = dayStart;
    jsonDoc["dRed"] = brightnessDay[0];
    jsonDoc["dGreen"] = brightnessDay[1];
    jsonDoc["dBlue"] = brightnessDay[2];
    jsonDoc["dayEnd"] = dayEnd;
    jsonDoc["nRed"] = brightnessNight[0];
    jsonDoc["nGreen"] = brightnessNight[1];
    jsonDoc["nBlue"] = brightnessNight[2];
    jsonDoc["timezone"] = timezoneName;
    jsonDoc["ntpUrl"] = ntpUrl;
    jsonDoc["ssid"] = ssid_sta;
    jsonDoc["password"] = password_sta;
    serializeJsonPretty(jsonDoc, Serial);
    Serial.println("\nfilled json");
    serializeJson(jsonDoc, configFile);
    Serial.println("wrote config file");
  }
  else
  {
    Serial.println("failed to open /config.json.");
  }
  configFile.close();
}





void initTime()
{
  if (my_WiFi_Mode == WIFI_STA)
  {
    setServer(cstr2String(ntpUrl));
    waitForSync();
    setInterval(60 * 60);
    myTZ.setLocation(cstr2String(timezoneName));
  }
  showTime();
}

void handleTime()
{
  events();
  if (minuteChanged())
  {
    showTime();
  }
}

bool isDayTime()
{
  uint16_t minuteOfDay = myTZ.minute() + myTZ.hour() * 60;
  return dayStart <= minuteOfDay && minuteOfDay < dayEnd;
}

void showEmpty()
{
  //TODO: clear LEDs and schow
  Serial.println("TODO: clear LEDs and show");
}

void showTime()
{
  char ihMSD = (myTZ.hour()/10)%10;
  char ihLSD = myTZ.hour()%10;
  char imMSD = (myTZ.minute()/10)%10;
  char imLSD = myTZ.minute()%10;
  uint8_t brightness[3];
  if (isDayTime())
  {
    brightness[0] = brightnessDay[0];
    brightness[1] = brightnessDay[1];
    brightness[2] = brightnessDay[2];
  }
  else
  {
    brightness[0] = brightnessNight[0];
    brightness[1] = brightnessNight[1];
    brightness[2] = brightnessNight[2];
  }

  //TODO: set LEDs and show
  Serial.print("TODO: set LEDs and show: ");
  Serial.println(myTZ.dateTime());
}




//---------------------------------------------------------------------
void WiFi_Start_STA()
{
  unsigned long timeout;

  WiFi.mode(WIFI_STA);
  WiFi.hostname("colock");

  WiFi.begin(ssid_sta, password_sta);
  timeout = millis() + 12000L;
  while (WiFi.status() != WL_CONNECTED
      && millis() < timeout)
  {
    delay(10);
  }

  if (WiFi.status() == WL_CONNECTED)
  {
    server.begin();
    my_WiFi_Mode = WIFI_STA;

    Serial.print("Connected IP - Address : ");
    for (int i = 0; i < 3; i++)
    {
      Serial.print( WiFi.localIP()[i]);
      Serial.print(".");
    }
    Serial.println(WiFi.localIP()[3]);
  }
  else
  {
    WiFi.mode(WIFI_OFF);
    my_WiFi_Mode = WIFI_OFF;
    Serial.println("WiFi connection failed");
  }
}

//---------------------------------------------------------------------
void WiFi_Start_AP()
{
  WiFi.mode(WIFI_AP);
  WiFi.hostname("colock");
  WiFi.softAP(ssid_ap, password_ap);
  server.begin();
  my_WiFi_Mode = WIFI_AP;

  Serial.print("Accesspoint started - Name: \"");
  Serial.print(WiFi.softAPSSID());
  Serial.print("\" password: \"");
  Serial.print(WiFi.softAPPSK());
  Serial.print("\" IP address: ");
  Serial.println(WiFi.softAPIP());
}

//---------------------------------------------------------------------
void WiFi_Traffic()
{
  char my_char;
  int htmlPtr = 0;
  unsigned long my_timeout;

  // Check if a client has connected
  client = server.available();
  if (!client)
  {
    return;
  }

  my_timeout = millis() + 250L;
  while (!client.available() && (millis() < my_timeout) ) delay(10);
  delay(10);
  if (millis() > my_timeout)
  {
    Serial.println("Client connection timeout!");
    return;
  }
  //---------------------------------------------------------------------
  htmlPtr = 0;
  my_char = 0;
  while (client.available() && my_char != '\r')
  {
    my_char = client.read();
    HTML_String[htmlPtr++] = my_char;
  }
  client.flush();
  HTML_String[htmlPtr] = 0;
  Serial.println ("--------------------------------------------------------");
  Serial.print("Remote IP - Address : ");
  for (int i = 0; i < 3; i++)
  {
    Serial.print( client.remoteIP()[i]);
    Serial.print(".");
  }
  Serial.println(client.remoteIP()[3]);

  exhibit("Remote Port ", client.remotePort());
  exhibit("Request : ", HTML_String);

  bool doRestart = false;
  bool reconnect = false;
  
  if (Find_Start("GET / HTTP", HTML_String) >= 0)
  {
    make_HTML01();
  }
  else if (Find_Start("GET /update", HTML_String) >= 0 && Find_Start("HTTP", HTML_String) >= 0)
  {
    make_HTML_update();
  }
  else if (Find_Start("POST /update", HTML_String) >= 0 && Find_Start("HTTP", HTML_String) >= 0)
  {
    send_not_found();//receive and update firmware
    return;
  }
  else if (Find_Start("GET /?", HTML_String) >= 0)
  {
    int myIndex;
    //---------------------------------------------------------------------
    // Benutzereingaben einlesen und verarbeiten
    //---------------------------------------------------------------------
    uint8_t action = Pick_Parameter_Zahl("ACTION=", HTML_String);

    if (action == ACTION_RESET)
    {
      Serial.println("Restarting WiFi chip...");
      doRestart = true;
    }
    else if (action == ACTION_WRITE_CONFIG)
    {
      Serial.println("writing config file...");
      saveConfig();
    }
    else if (action == ACTION_TIME_SET)
    {
      myIndex = Find_End("TIME=", HTML_String);
      if (myIndex >= 0)
      {
        char tmp_time[10]; // TIME=hh:mm:ss
        Pick_Text(tmp_time, &HTML_String[myIndex], 8);
        uint8_t hour = Pick_N_Zahl(tmp_time, ':', 1);
        uint8_t minute = Pick_N_Zahl(tmp_time, ':', 2);

        myIndex = Find_End("DATE=", HTML_String);
        if (myIndex >= 0)
        {
          char tmp_date[12]; // DATE=YYYY-MM-DD
          Pick_Text(tmp_date, &HTML_String[myIndex], 10);
          uint16_t year = Pick_N_Zahl(tmp_date, '-', 1);
          uint8_t month = Pick_N_Zahl(tmp_date, '-', 2);
          uint8_t day = Pick_N_Zahl(tmp_date, '-', 3);
          myTZ.setTime(hour, minute, 0, day, month, year);
          Serial.print("New time: ");
          Serial.print(myTZ.year());
          Serial.print("-");
          Serial.print(myTZ.month());
          Serial.print("-");
          Serial.print(myTZ.day());
          Serial.print(" ");
          Serial.print(myTZ.hour());
          Serial.print(":");
          Serial.print(myTZ.minute());
          Serial.print("\n");
        }
      }
    }
    else if (action == ACTION_NTP_SET)
    {
      myIndex = Find_End("NTPURL=", HTML_String);
      if (myIndex >= 0)
      {
        Pick_Text(ntpUrl, &HTML_String[myIndex], 128);
        exhibit("NTPURL: ", ntpUrl);
        setServer(cstr2String(ntpUrl));
      }
      myIndex = Find_End("TIMEZONE=", HTML_String);
      if (myIndex >= 0)
      {
        Pick_Text(timezoneName, &HTML_String[myIndex], 128);
        myTZ.setLocation(cstr2String(timezoneName));
        exhibit("TIMEZONE: ", myTZ.getTimezoneName().c_str());
      }
      updateNTP();
    }
    else if (action == ACTION_WIFI_SET)
    {
      myIndex = Find_End("SSID=", HTML_String);
      if (myIndex >= 0)
      {
        Pick_Text(ssid_sta, &HTML_String[myIndex], 128);
        exhibit("SSID: ", ssid_sta);
      }
      myIndex = Find_End("PSWD=", HTML_String);
      if (myIndex >= 0)
      {
        Pick_Text(password_sta, &HTML_String[myIndex], 128);
      }
      reconnect = true;
    }
    else if (action == ACTION_COLOR_SET)
    {
      myIndex = Find_End("DAY_START=", HTML_String);
      if (myIndex >= 0)
      {
        char tmp_string[10];
        Pick_Text(tmp_string, &HTML_String[myIndex], 8);
        dayStart = Pick_N_Zahl(tmp_string, ':', 1) * 60 + Pick_N_Zahl(tmp_string, ':', 2);
      }
      brightnessDay[BRIGHT_RED] = Pick_Parameter_Zahl("D_RED=", HTML_String);
      brightnessDay[BRIGHT_GREEN] = Pick_Parameter_Zahl("D_GREEN=", HTML_String);
      brightnessDay[BRIGHT_BLUE] = Pick_Parameter_Zahl("D_BLUE=", HTML_String);
      myIndex = Find_End("DAY_END=", HTML_String);
      if (myIndex >= 0)
      {
        char tmp_string[10];
        Pick_Text(tmp_string, &HTML_String[myIndex], 8);
        dayEnd = Pick_N_Zahl(tmp_string, ':', 1) * 60 + Pick_N_Zahl(tmp_string, ':', 2);
      }
      brightnessNight[BRIGHT_RED] = Pick_Parameter_Zahl("N_RED=", HTML_String);
      brightnessNight[BRIGHT_GREEN] = Pick_Parameter_Zahl("N_GREEN=", HTML_String);
      brightnessNight[BRIGHT_BLUE] = Pick_Parameter_Zahl("N_BLUE=", HTML_String);
    }
    
    showTime();

    //---------------------------------------------------------------------
    //Antwortseite aufbauen
    make_HTML_redirectHome(doRestart ? 3 : 0);
  }
  else
  {
    send_not_found();
    return;
  }

  //---------------------------------------------------------------------
  // Seite senden
  send_HTML();

  if (doRestart)
  {
    Serial.println("Restarting now.");
    ESP.restart();
  }

  if (reconnect)
  {
    WiFi.mode(WIFI_OFF);
    Serial.println("WiFi connection closed - try new parameters");
    WiFi_Start_STA();
    if (my_WiFi_Mode == WIFI_OFF)
    {
      WiFi_Start_AP();
    }
  }
}

//---------------------------------------------------------------------
// HTML Seite 01 aufbauen
//---------------------------------------------------------------------
void make_HTML01()
{
  strcpy( HTML_String, "<!DOCTYPE html>");
  strcat( HTML_String, "<html>");
  strcat( HTML_String, "<head>");
  strcat( HTML_String, "<title>colock setup</title>");
  strcat( HTML_String, "</head>");
  strcat( HTML_String, "<body bgcolor=\"#adcede\">");
  strcat( HTML_String, "<font size=\"6\" color=\"#000000\" face=\"VERDANA,ARIAL,HELVETICA\">");

  strcat( HTML_String, "<h1>colock setup&nbsp;&nbsp;&nbsp;</h1>");
  strcat( HTML_String, "<table style=\"width:900px\">");
  strcat( HTML_String, "<tr align=center>");
  
  strcat( HTML_String, "<td>");
  strcat( HTML_String, "<form>");
  strcat( HTML_String, "<button style=\"width:400px;height:75px;font-size:100%\" name=\"ACTION\" value=\"");
  strcati(HTML_String, ACTION_RESET);
  strcat( HTML_String, "\">reset WiFi chip</button>");
  strcat( HTML_String, "</form>");
  strcat( HTML_String, "</td>");

  strcat( HTML_String, "<td>");
  strcat( HTML_String, "<form>");
  strcat( HTML_String, "<button style=\"width:400px;height:75px;font-size:100%\" name=\"ACTION\" value=\"");
  strcati(HTML_String, ACTION_WRITE_CONFIG);
  strcat( HTML_String, "\">write config to SPIFFS</button>");
  strcat( HTML_String, "</form>");
  strcat( HTML_String, "</td>");

  strcat( HTML_String, "</tr>");
  strcat( HTML_String, "</table>");


  strcat( HTML_String, "<h2>Color</h2>");
  strcat( HTML_String, "<form>");
  strcat( HTML_String, "<table style=\"width:900px\">");

  strcat( HTML_String, "<tr align=center>");
  strcat( HTML_String, "<td>");
  strcat( HTML_String, "day starts at:");
  strcat( HTML_String, "<br><input type=\"time\" style=\"width:400px;height:75px;font-size:100%\" name=\"DAY_START\" value=\"");
  strcati2(HTML_String, dayStart / 60);
  strcat( HTML_String, ":");
  strcati2(HTML_String, dayStart % 60);
  strcat( HTML_String, "\">");
  strcat( HTML_String, "</td>");
  strcat( HTML_String, "<td>");
  strcat( HTML_String, "night starts at:");
  strcat( HTML_String, "<br><input type=\"time\" style=\"width:400px;height:75px;font-size:100%\" name=\"DAY_END\" value=\"");
  strcati2(HTML_String, dayEnd / 60);
  strcat( HTML_String, ":");
  strcati2(HTML_String, dayEnd % 60);
  strcat( HTML_String, "\">");
  strcat( HTML_String, "</td>");
  strcat( HTML_String, "</tr>");

  strcat( HTML_String, "<tr align=center>");
  strcat( HTML_String, "<td>");
  strcat( HTML_String, "day red:");
  strcat( HTML_String, "<br><input type=\"range\" style=\"width:350px;height:75px;font-size:100%\" name=\"D_RED\" min=\"0\" max=\"10\" value = \"");
  strcati(HTML_String, brightnessDay[BRIGHT_RED]);
  strcat( HTML_String, "\">");
  strcat( HTML_String, "</td>");
  strcat( HTML_String, "<td>");
  strcat( HTML_String, "night red:");
  strcat( HTML_String, "<br><input type=\"range\" style=\"width:350px;height:75px;font-size:100%\" name=\"N_RED\" min=\"0\" max=\"10\" value = \"");
  strcati(HTML_String, brightnessNight[BRIGHT_RED]);
  strcat( HTML_String, "\">");
  strcat( HTML_String, "</td>");
  strcat( HTML_String, "</tr>");

  strcat( HTML_String, "<tr align=center>");
  strcat( HTML_String, "<td>");
  strcat( HTML_String, "day green:");
  strcat( HTML_String, "<br><input type=\"range\" style=\"width:350px;height:75px;font-size:100%\" name=\"D_GREEN\" min=\"0\" max=\"10\" value = \"");
  strcati(HTML_String, brightnessDay[BRIGHT_GREEN]);
  strcat( HTML_String, "\">");
  strcat( HTML_String, "</td>");
  strcat( HTML_String, "<td>");
  strcat( HTML_String, "night green:");
  strcat( HTML_String, "<br><input type=\"range\" style=\"width:350px;height:75px;font-size:100%\" name=\"N_GREEN\" min=\"0\" max=\"10\" value = \"");
  strcati(HTML_String, brightnessNight[BRIGHT_GREEN]);
  strcat( HTML_String, "\">");
  strcat( HTML_String, "</td>");
  strcat( HTML_String, "</tr>");

  strcat( HTML_String, "<tr align=center>");
  strcat( HTML_String, "<td>");
  strcat( HTML_String, "day blue:");
  strcat( HTML_String, "<br><input type=\"range\" style=\"width:350px;height:75px;font-size:100%\" name=\"D_BLUE\" min=\"0\" max=\"10\" value = \"");
  strcati(HTML_String, brightnessDay[BRIGHT_BLUE]);
  strcat( HTML_String, "\">");
  strcat( HTML_String, "</td>");
  strcat( HTML_String, "<td>");
  strcat( HTML_String, "night blue:");
  strcat( HTML_String, "<br><input type=\"range\" style=\"width:350px;height:75px;font-size:100%\" name=\"N_BLUE\" min=\"0\" max=\"10\" value = \"");
  strcati(HTML_String, brightnessNight[BRIGHT_BLUE]);
  strcat( HTML_String, "\">");
  strcat( HTML_String, "</td>");
  strcat( HTML_String, "</tr>");

  strcat( HTML_String, "<tr align=center>");
  strcat( HTML_String, "<td colspan=2>");
  strcat( HTML_String, "<button style=\"width:500px;height:75px;font-size:100%\" name=\"ACTION\" value=\"");
  strcati(HTML_String, ACTION_COLOR_SET);
  strcat( HTML_String, "\">set color</button>");
  strcat( HTML_String, "</td>");
  strcat( HTML_String, "</tr>");

  strcat( HTML_String, "</table>");
  strcat( HTML_String, "</form>");


  strcat( HTML_String, "<h2>Time</h2>");
  strcat( HTML_String, "for country code or TZ database name, see<br><br><a href=\"https://en.wikipedia.org/wiki/List_of_tz_database_time_zones\">https://en.wikipedia.org/wiki/List_of_tz_database_time_zones</a>");
  strcat( HTML_String, "<form>");
  strcat( HTML_String, "<table style=\"width:900px\">");

  strcat( HTML_String, "<tr align=center>");
  strcat( HTML_String, "<td>");
  strcat( HTML_String, "Timezone:");
  strcat( HTML_String, "<br><input type=\"text\" style=\"width:400px;height:75px;font-size:100%\" name=\"TIMEZONE\" value=\"");
  strcat( HTML_String, timezoneName);
  strcat( HTML_String, "\">");
  strcat( HTML_String, "</td>");
  strcat( HTML_String, "<td>");
  strcat( HTML_String, "NTP server URL:");
  strcat( HTML_String, "<br><input type=\"text\" style=\"width:400px;height:75px;font-size:100%\" name=\"NTPURL\" value=\"");
  strcat( HTML_String, ntpUrl);
  strcat( HTML_String, "\">");
  strcat( HTML_String, "</td>");
  strcat( HTML_String, "</tr>");

  strcat( HTML_String, "<tr align=center>");
  strcat( HTML_String, "<td colspan=2>");
  strcat( HTML_String, "<button style=\"width:500px;height:75px;font-size:100%\" name=\"ACTION\" value=\"");
  strcati(HTML_String, ACTION_NTP_SET);
  strcat( HTML_String, "\">set NTP server URL and time zone</button>");
  strcat( HTML_String, "</td>");
  strcat( HTML_String, "</tr>");

  strcat( HTML_String, "</table>");
  strcat( HTML_String, "</form>");

  strcat( HTML_String, "<form>");
  strcat( HTML_String, "<br>");
  strcat( HTML_String, "<table style=\"width:900px\">");
  strcat( HTML_String, "<tr align=center>");
  strcat( HTML_String, "<td colspan=2>");
  strcat( HTML_String, "set current time:");
  strcat( HTML_String, "</td>");
  strcat( HTML_String, "</tr>");

  strcat( HTML_String, "<tr align=center>");
  strcat( HTML_String, "<td>");
  strcat( HTML_String, "<input type=\"date\" style=\"width:400px;height:75px;font-size:100%\" name=\"DATE\" value=\"");
  strcati2(HTML_String, myTZ.year());
  strcat( HTML_String, "-");
  strcati2(HTML_String, myTZ.month());
  strcat( HTML_String, "-");
  strcati2(HTML_String, myTZ.day());
  strcat( HTML_String, "\">");
  strcat( HTML_String, "</td>");
  strcat( HTML_String, "<td>");
  strcat( HTML_String, "<input type=\"time\" style=\"width:400px;height:75px;font-size:100%\" name=\"TIME\" value=\"");
  strcati2(HTML_String, myTZ.hour());
  strcat( HTML_String, ":");
  strcati2(HTML_String, myTZ.minute());
  strcat( HTML_String, "\">");
  strcat( HTML_String, "</td>");
  strcat( HTML_String, "</tr>");

  strcat( HTML_String, "<tr align=center>");
  strcat( HTML_String, "<td colspan=2>");
  strcat( HTML_String, "<button style=\"width:500px;height:75px;font-size:100%\" name=\"ACTION\" value=\"");
  strcati(HTML_String, ACTION_TIME_SET);
  strcat( HTML_String, "\">set time</button>");
  strcat( HTML_String, "</td>");
  strcat( HTML_String, "</tr>");

  strcat( HTML_String, "</table>");
  strcat( HTML_String, "</form>");


  strcat( HTML_String, "<h2>WiFi</h2>");
  strcat( HTML_String, "<form>");
  strcat( HTML_String, "<table style=\"width:900px\">");

  strcat( HTML_String, "<tr align=center>");
  strcat( HTML_String, "<td>");
  strcat( HTML_String, "WiFi SSID:");
  strcat( HTML_String, "<br><input type=\"text\" style=\"width:400px;height:75px;font-size:100%\" name=\"SSID\" value=\"");
  strcat( HTML_String, ssid_sta);
  strcat( HTML_String, "\">");
  strcat( HTML_String, "</td>");
  strcat( HTML_String, "<td>");
  strcat( HTML_String, "WiFi password:");
  strcat( HTML_String, "<br><input type=\"password\" style=\"width:400px;height:75px;font-size:100%\" name=\"PSWD\" value=\"\">");
  strcat( HTML_String, "</td>");
  strcat( HTML_String, "</tr>");

  strcat( HTML_String, "<tr align=center>");
  strcat( HTML_String, "<td colspan=2>");
  strcat( HTML_String, "<button style=\"width:500px;height:75px;font-size:100%\" name=\"ACTION\" value=\"");
  strcati(HTML_String, ACTION_WIFI_SET);
  strcat( HTML_String, "\">set WiFi parameters</button>");
  strcat( HTML_String, "</td>");
  strcat( HTML_String, "</tr>");

  strcat( HTML_String, "</table>");
  strcat( HTML_String, "</form>");

  strcat( HTML_String, "</font>");
  strcat( HTML_String, "</body>");
  strcat( HTML_String, "</html>");
}

void make_HTML_update()
{
  strcpy( HTML_String, "<!DOCTYPE html>");
  strcat( HTML_String, "<html lang='en'>");
  strcat( HTML_String, "<head>");
  strcat( HTML_String, "<meta charset='utf-8'>");
  strcat( HTML_String, "<meta name='viewport' content='width=device-width,initial-scale=1'/>");
  strcat( HTML_String, "</head>");
  strcat( HTML_String, "<body>");
  
  strcat( HTML_String, "<form method='POST' action='' enctype='multipart/form-data'>");
  strcat( HTML_String, "Firmware:<br>");
  strcat( HTML_String, "<input type='file' accept='.bin' name='firmware'>");
  strcat( HTML_String, "<input type='submit' value='Update Firmware'>");
  strcat( HTML_String, "</form>");
  
  strcat( HTML_String, "<form method='POST' action='' enctype='multipart/form-data'>");
  strcat( HTML_String, "FileSystem:<br>");
  strcat( HTML_String, "<input type='file' accept='.bin' name='filesystem'>");
  strcat( HTML_String, "<input type='submit' value='Update FileSystem'>");
  strcat( HTML_String, "</form>");
  
  strcat( HTML_String, "</body>");
  strcat( HTML_String, "</html>");
}

void make_HTML_update_success()
{
  strcpy( HTML_String, "<META http-equiv=\"refresh\" content=\"15;URL=/\">Update Success! Rebooting...");
}

void make_HTML_redirectHome(uint8_t delay)
{
  strcpy( HTML_String, "<!DOCTYPE html>");
  strcat( HTML_String, "<html>");
  strcat( HTML_String, "<head>");
  strcat( HTML_String, "<meta http-equiv=\"refresh\" content=\"");
  strcati(HTML_String, delay);
  strcat( HTML_String, "; url=/\" />");
  strcat( HTML_String, "<title>colock setup</title>");
  strcat( HTML_String, "</head>");
  strcat( HTML_String, "<body bgcolor=\"#adcede\">");
  strcat( HTML_String, "<font color=\"#000000\" face=\"VERDANA,ARIAL,HELVETICA\">");
  strcat( HTML_String, "<p>Please follow <a href=\"/\">this link</a>.</p>");
  strcat( HTML_String, "</font>");
  strcat( HTML_String, "</body>");
  strcat( HTML_String, "</html>");
}


//--------------------------------------------------------------------------
void send_not_found()
{
  Serial.println("Send Not Found");
  client.print("HTTP/1.1 404 Not Found\r\n\r\n");
  delay(20);
  client.stop();
}

//--------------------------------------------------------------------------
void make_header()
{
  strcpy(HTTP_Header , "HTTP/1.1 200 OK\r\n");
  strcat(HTTP_Header, "Content-Length: ");
  strcati(HTTP_Header, strlen(HTML_String));
  strcat(HTTP_Header, "\r\n");
  strcat(HTTP_Header, "Content-Type: text/html\r\n");
  strcat(HTTP_Header, "Connection: close\r\n");
  strcat(HTTP_Header, "\r\n");

  exhibit("Header : ", HTTP_Header);
  exhibit("Laenge Header : ", static_cast<unsigned int>(strlen(HTTP_Header)));
  exhibit("Laenge HTML   : ", static_cast<unsigned int>(strlen(HTML_String)));
}

//--------------------------------------------------------------------------
void send_HTML()
{
  char my_char;
  int  my_len = strlen(HTML_String);
  int  my_ptr = 0;
  int  my_send = 0;

  //--------------------------------------------------------------------------
  // Header erzeugen und vorausschicken
  make_header();
  client.print(HTTP_Header);
  delay(20);

  //--------------------------------------------------------------------------
  // in Portionen senden
  while ((my_len - my_send) > 0)
  {
    my_send = my_ptr + MAX_PACKAGE_SIZE;
    if (my_send > my_len)
    {
      client.print(&HTML_String[my_ptr]);
      delay(20);
      Serial.println(&HTML_String[my_ptr]);
      my_send = my_len;
    }
    else
    {
      my_char = HTML_String[my_send];
      // Auf Anfang eines Tags positionieren
      while ( my_char != '<') my_char = HTML_String[--my_send];
      HTML_String[my_send] = 0;
      client.print(&HTML_String[my_ptr]);
      delay(20);
      Serial.println(&HTML_String[my_ptr]);
      HTML_String[my_send] =  my_char;
      my_ptr = my_send;
    }
  }
  client.stop();
}


//---------------------------------------------------------------------
void strcati(char* tx, int i)
{
  char tmp[8];

  itoa(i, tmp, 10);
  strcat (tx, tmp);
}

//---------------------------------------------------------------------
void strcati2(char* tx, int i)
{
  char tmp[8];

  itoa(i, tmp, 10);
  if (strlen(tmp) < 2) strcat (tx, "0");
  strcat (tx, tmp);
}

//---------------------------------------------------------------------
int Pick_Parameter_Zahl(const char * par, char * str)
{
  int myIdx = Find_End(par, str);
  
  if (myIdx >= 0) return  Pick_Dec(str, myIdx);
  else return -1;
}
//---------------------------------------------------------------------
int Find_End(const char * such, const char * str)
{
  int tmp = Find_Start(such, str);
  if (tmp >= 0)tmp += strlen(such);
  return tmp;
}

//---------------------------------------------------------------------
int Find_Start(const char * such, const char * str)
{
  int tmp = -1;
  int ww = strlen(str) - strlen(such);
  int ll = strlen(such);

  for (int i = 0; i <= ww && tmp == -1; i++)
  {
    if (strncmp(such, &str[i], ll) == 0) tmp = i;
  }
  return tmp;
}
//---------------------------------------------------------------------
int Pick_Dec(const char * tx, int idx )
{
  int tmp = 0;
  bool negate = false;

  if (tx[idx] == '-')
  {
    negate = true;
    ++idx;
  }
  
  for (int p = idx; p < idx + 5 && (tx[p] >= '0' && tx[p] <= '9') ; p++)
  {
    tmp = 10 * tmp + tx[p] - '0';
  }
  return negate ? -tmp : tmp;
}
//----------------------------------------------------------------------------
int Pick_N_Zahl(const char * tx, char separator, byte n)
{

  int ll = strlen(tx);
  int tmp = -1;
  byte anz = 1;
  byte i = 0;
  while (i < ll && anz < n)
  {
    if (tx[i] == separator)anz++;
    i++;
  }
  if (i < ll) return Pick_Dec(tx, i);
  else return -1;
}

//---------------------------------------------------------------------
int Pick_Hex(const char * tx, int idx )
{
  int tmp = 0;

  for (int p = idx; p < idx + 5 && ( (tx[p] >= '0' && tx[p] <= '9') || (tx[p] >= 'A' && tx[p] <= 'F')) ; p++)
  {
    if (tx[p] <= '9')tmp = 16 * tmp + tx[p] - '0';
    else tmp = 16 * tmp + tx[p] - 55;
  }

  return tmp;
}

//---------------------------------------------------------------------
void Pick_Text(char * tx_ziel, char  * tx_quelle, int max_ziel)
{

  int p_ziel = 0;
  int p_quelle = 0;
  int len_quelle = strlen(tx_quelle);

  while (p_ziel < max_ziel && p_quelle < len_quelle && tx_quelle[p_quelle] && tx_quelle[p_quelle] != ' ' && tx_quelle[p_quelle] !=  '&')
  {
    if (tx_quelle[p_quelle] == '%')
    {
      tx_ziel[p_ziel] = (HexChar_to_NumChar( tx_quelle[p_quelle + 1]) << 4) + HexChar_to_NumChar(tx_quelle[p_quelle + 2]);
      p_quelle += 2;
    }
    else if (tx_quelle[p_quelle] == '+')
    {
      tx_ziel[p_ziel] = ' ';
    }
    else
    {
      tx_ziel[p_ziel] = tx_quelle[p_quelle];
    }
    p_ziel++;
    p_quelle++;
  }

  tx_ziel[p_ziel] = 0;
}
//---------------------------------------------------------------------
char HexChar_to_NumChar( char c)
{
  if (c >= '0' && c <= '9') return c - '0';
  if (c >= 'A' && c <= 'F') return c - 55;
  return 0;
}

//---------------------------------------------------------------------
void exhibit(const char * tx, int v)
{
  Serial.print(tx);
  Serial.println(v);
}
//---------------------------------------------------------------------
void exhibit(const char * tx, unsigned int v)
{
  Serial.print(tx);
  Serial.println(v);
}
//---------------------------------------------------------------------
void exhibit(const char * tx, unsigned long v)
{
  Serial.print(tx);
  Serial.println(v);
}
//---------------------------------------------------------------------
void exhibit(const char * tx, const char * v)
{
  Serial.print(tx);
  Serial.println(v);
}

String cstr2String(const char* cstr) {
  return String() += cstr;
}
