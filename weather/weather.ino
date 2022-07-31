#include <TFT_eSPI.h>
#include <Arduino_JSON.h>
#include <rpcWiFi.h>
#include <HTTPClient.h>
#include <PNGdec.h>
#include <DateTime.h>
#include "RTC_SAMD51.h"
#include "lcd_backlight.hpp"

// wifi variables
const char* ssid = "SSID";
const char* password =  "PASSWORD";

// weather API variables
float lon = 0;
float lat = 0;
String apiKey = "APIKEY";
int refreshEveryMinutes = 10;

int minutesPassedSinceRefresh = 0;

// base colors
#define whiteColor 0xFFFF
#define greyColor 0xC618
#define blackColor 0x0000

// weather colors
#define sunColor 0xFEA0
#define moonColor 0xFFE0
#define cloudColor 0xFFFF
#define cloudDarkColor 0xD69A

unsigned long unixtimestamp;
char timeOfDay = 'd';

bool debug = true;

TFT_eSPI tft;
HTTPClient http;
JSONVar data;
RTC_SAMD51 rtc;

static LCDBackLight backLight;

const char *months[] = {
    "Jan", "Feb", "Mar", "Apr", "May", "Jun",
    "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"
};

const char *days[] = {
    "Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"
};

// this substitutes the delay so the arduino can track time
void millisDelay(int duration) {
    long timestart = millis();
    while ((millis()-timestart) < duration) {};
}

// connect to the wifi
void connect() {
    Serial.begin(115200);
 
    // Set WiFi to station mode and disconnect from an AP if it was previously connected
    WiFi.mode(WIFI_STA);
    WiFi.disconnect();
 
    WiFi.begin(ssid, password);

    tft.println("Connecting to WiFi...");
    while (WiFi.status() != WL_CONNECTED) {
        millisDelay(500);
        WiFi.begin(ssid, password);
    }
    tft.println("Connected to the WiFi network");
    tft.print("IP Address: ");
    tft.println (WiFi.localIP());
}

void setup() {
  // clock
  rtc.begin();
  DateTime now = DateTime(F(__DATE__), F(__TIME__));
  rtc.adjust(now);

  // general settings
  tft.init();
  backLight.initialize();
  backLight.setBrightness(10);
  tft.setCursor(0, 0, 2);
  tft.setRotation(3);
  tft.fillScreen(blackColor);
  tft.setTextColor(whiteColor);
  tft.setTextSize(1);
  tft.setTextWrap(false); 

  connect();

  getWeather();
}

void loop() {
  draw();
  millisDelay(60000);

  if (minutesPassedSinceRefresh >= refreshEveryMinutes) {
    minutesPassedSinceRefresh = refreshEveryMinutes;
    getWeather();
  }
  else {
    minutesPassedSinceRefresh ++;
  }
}

void draw() {
  drawBackground();
  drawInfo();
  drawIcons();
}

void drawBackground() {
  tft.fillScreen(blackColor);
}

void drawIcons() {
  String weather = sanitizeJson(data["weather"][0]["icon"]).substring(0,2);
  float scale = 1;
  int x = 90;
  int y = 60;
  
  if (weather == "01") {
    draw01(x, y, scale);
  }
  else if (weather == "02") {
    draw02(x, y, scale);
  }
  else if (weather == "03") {
    draw03(x, y, scale);
  }
  else if (weather == "04") {
    draw04(x, y, scale);
  }
  else if (weather == "09") {
    draw09(x, y, scale);
  }
  else if (weather == "10") {
    draw10(x, y, scale);
  }
  else if (weather == "11") {
    draw11(x, y, scale);
  }
  else if (weather == "13") {
    draw13(x, y, scale);
  }
  else if (weather == "50") {
    draw50(x, y, scale);
  }
  else {
    tft.drawCentreString(weather, 90, 30, 6);
  }
}

// clear
void draw01(int x, int y, float scale) {
  if (timeOfDay == 'd') {
    tft.fillCircle(x, y, 20 * scale, sunColor);
  }
  else if (timeOfDay == 'n'){
    tft.fillCircle(x, y, 20 * scale, moonColor);
  }
  else {
    tft.drawCentreString("X", x, y, 8);
  }
}

// broken clouds
void draw02(int x, int y, float scale) {
  draw01(x + (12 * scale), y - (4 * scale), scale * 0.8);
  drawCloud(x - (5 * scale), y + (2 * scale), scale * 0.9, cloudColor);
}

// cloudy
void draw03(int x, int y, float scale) {
  drawCloud(x, y, scale, cloudColor);
}

// very cloudy
void draw04(int x, int y, float scale) {
  drawCloud(x + (8 * scale), y - (5 * scale), scale * 0.8, cloudDarkColor);
  drawCloud(x - (5 * scale), y + (2 * scale), scale * 0.9, cloudColor);
}

// rain
void draw09(int x, int y, float scale) {
  draw04(x + (3 * scale), y - (5 * scale), scale * 0.8);
  drawRain(x - (5 * scale), y - (1 * scale), scale);
}

// showers
void draw10(int x, int y, float scale) {
  draw02(x + (3 * scale), y - (5 * scale), scale * 0.8);
  drawRain(x - (5 * scale), y - (1 * scale), scale);
}

// storm
void draw11(int x, int y, float scale) {
  draw04(x + (3 * scale), y - (5 * scale), scale * 0.8);
  drawLightning(x - (5 * scale), y - (1 * scale), scale);
}

// snow
void draw13(int x, int y, float scale) {
  draw04(x + (3 * scale), y - (5 * scale), scale * 0.8);
  drawSnow(x, y + 25, scale);
}

// fog
void draw50(int x, int y, float scale) {
  int offset = 5;
  tft.drawLine(x - (40 * (scale / 2)), y + (0 * (scale / 2)), x + (30 * (scale / 2)), y + (0 * (scale / 2)), cloudDarkColor);
  tft.drawLine(x - (30 * (scale / 2)), y + (offset * 1), x + (40 * (scale / 2)), y + (offset * 1), cloudDarkColor);
  tft.drawLine(x - (35 * (scale / 2)), y + (offset * 2), x + (20 * (scale / 2)), y + (offset * 2), cloudDarkColor);
  tft.drawLine(x - (20 * (scale / 2)), y + (offset * 3), x + (15 * (scale / 2)), y + (offset * 3), cloudDarkColor);
  tft.drawLine(x - (40 * (scale / 2)), y - (offset * 1), x + (35 * (scale / 2)), y - (offset * 1), cloudDarkColor);
  tft.drawLine(x - (35 * (scale / 2)), y - (offset * 2), x + (40 * (scale / 2)), y - (offset * 2), cloudDarkColor);
  tft.drawLine(x - (20 * (scale / 2)), y - (offset * 3), x + (25 * (scale / 2)), y - (offset * 3), cloudDarkColor);
}

void drawSnow(int x, int y, float scale) {
  tft.drawLine(x - (10 * (scale / 2)), y, x + (10 * (scale / 2)), y, cloudDarkColor);
  tft.drawLine(x, y - (10 * (scale / 2)), x, y + (10 * (scale / 2)), cloudDarkColor);
  tft.drawLine(x - (8 * (scale / 2)), y - (8 * (scale / 2)), x + (8 * (scale / 2)), y + (8 * (scale / 2)), cloudDarkColor);
  tft.drawLine(x + (8 * (scale / 2)), y - (8 * (scale / 2)), x - (8 * (scale / 2)), y + (8 * (scale / 2)), cloudDarkColor);
}

void drawRain(int x, int y, float scale) {
  int offset = 5;
  tft.drawLine(x + (6 * (scale / 2)), y + (25 * (scale / 2)), x, y + (55 * (scale / 2)), cloudDarkColor);
  tft.drawLine(x + offset + (6 * (scale / 2)), y + (25 * (scale / 2)), x + offset, y + (55 * (scale / 2)), cloudDarkColor);
  tft.drawLine(x + (offset * 2) + (6 * (scale / 2)), y + (25 * (scale / 2)), x + (offset * 2), y + (55 * (scale / 2)), cloudDarkColor);
  tft.drawLine(x - offset + (6 * (scale / 2)), y + (25 * (scale / 2)), x - offset, y + (55 * (scale / 2)), cloudDarkColor);
  tft.drawLine(x - (offset * 2) + (6 * (scale / 2)), y + (25 * (scale / 2)), x - (offset * 2), y + (55 * (scale / 2)), cloudDarkColor);
}

void drawLightning(int x, int y, float scale) {
  tft.fillTriangle(x + (2 * (scale / 2)), y + (25 * (scale / 2)), x - (2 * (scale / 2)), y + (45 * (scale / 2)), x + (10 * (scale / 2)), y + (25 * (scale / 2)), sunColor);
  tft.fillTriangle(x - (2 * (scale / 2)), y + (45 * (scale / 2)), x + (1 * (scale / 2)), y + (40 * (scale / 2)), x + (10 * (scale / 2)), y + (45 * (scale / 2)), sunColor);
  tft.fillTriangle(x + (2 * (scale / 2)), y + (45 * (scale / 2)), x - (2 * (scale / 2)), y + (65 * (scale / 2)), x + (10 * (scale / 2)), y + (45 * (scale / 2)), sunColor);
}

void drawCloud(int x, int y, float scale, uint16_t color) {
  tft.fillCircle(x - (35 * (scale / 2)), y + (18 * (scale / 2)), 16 * scale, color);
  tft.fillCircle(x - (5 * (scale / 2)), y - (12 * (scale / 2)), 16 * scale, color);
  tft.fillCircle(x + (33 * (scale / 2)), y - (3 * (scale / 2)), 6 * scale, color);
  tft.fillCircle(x + (40 * (scale / 2)), y + (28 * (scale / 2)), 10 * scale, color);
  tft.fillRect(x - (35 * (scale / 2)), y - (4 * (scale / 2)), 38 * scale, 28 * scale, color);
}

String sanitizeJson(JSONVar jsonvar) {
  String string = JSON.stringify(jsonvar);
  string.replace("\"","");
  return string; 
}

uint8_t dow(unsigned long t)
{
    return ((t / 86400) + 4) % 7;
}

// draw the info at screen
void drawInfo() {

  tft.drawCentreString(sanitizeJson(data["weather"][0]["description"]), 90, 90, 2);
  tft.drawCentreString(sanitizeJson(data["name"]), 90, 110, 1);

  DateTime now = rtc.now();
  String year = String(now.year(), DEC);
  String month = months[String(now.month(), DEC).toInt()-1];
  String day = String(now.day(), DEC);
  String hour = String(now.hour(), DEC);
  String minute = String(now.minute(), DEC);
  String weekDay = days[String(dow(unixtimestamp), DEC).toInt()];

  if (hour.length() == 1) {
    hour = "0" + hour;
  }
  if (minute.length() == 1) {
    minute = "0" + minute;
  }
  
  String time = hour + ":" + minute;
  String date = weekDay + ", " + day + " " + month + " " + year;
  
  tft.drawCentreString(time, 90, 150, 6);
  tft.drawCentreString(date, 90, 190, 2);

  tft.drawString("T " + sanitizeJson(data["main"]["temp"]) + " " + "`" + "C", 200, 50, 2);
  tft.drawString("H " + sanitizeJson(data["main"]["humidity"]) + "%", 200, 70, 2);
  tft.drawString("W " + sanitizeJson(data["wind"]["speed"]) + " m/s", 200, 90, 2);
  tft.drawString("P " + sanitizeJson(data["main"]["pressure"]) + " hPa", 200, 110, 2);
}

String httpGETRequest(const char* serverName) {
  WiFiClient client;
  HTTPClient http;
    
  // Your Domain name with URL path or IP address with path
  http.begin(client, serverName);
  
  // Send HTTP POST request
  int httpResponseCode = http.GET();
  
  String payload = "{}"; 

  if (httpResponseCode>0) {
    if (debug) {
      tft.print("HTTP Response code: ");
      tft.println(httpResponseCode);
      debug = false;
    }
    payload = http.getString();
  }
  else {
    if (debug) {
      tft.print("Error code: ");
      tft.println(httpResponseCode);
    }
  }
  // Free resources
  http.end();

  return payload;
}

// this could sync if I find a timestamp api
void syncClock() {
//  unsigned long epoch = long(data["dt"]);
//  long tzOffset = long(data["timezone"]);
//  unixtimestamp = epoch + tzOffset;
//  rtc.adjust(DateTime(unixtimestamp));
}


// get the weather JSON
void getWeather() {

  // call the API
  String serverPath = "";
  serverPath = "http://api.openweathermap.org/data/2.5/weather?lat=" + String(lat) + "&lon=" + String(lon) + "&units=metric&exclude=alerts,daily&appid=" + apiKey;
  String jsonBuffer = httpGETRequest(serverPath.c_str());
  data = JSON.parse(jsonBuffer);
  if (JSON.typeof(data) == "undefined") {
    if (debug) {
      tft.println("Parsing input failed!");
    }
    return;
  } else {
    getWeatherCallback();
  }
}

// called on callback
void getWeatherCallback() {
  syncClock();
  timeOfDay = sanitizeJson(data["weather"][0]["icon"]).charAt(2);
}
