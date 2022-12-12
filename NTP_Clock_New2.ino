#include <heltec.h>

//////////////////////////////////////////////////////////////////////////////////////////////////////////////
//                                                                                                          //
//                                     Heltec WiFi Kit 32 NTP Clock                                         //
//                                                                                                          //
//////////////////////////////////////////////////////////////////////////////////////////////////////////////

//
// Notes:
//
//  "Heltec WiFi Kit 32 NTP Clock" is a ntp initialized date and time clock.  The device connects to the
// an ntp time server via wifi and a udp port, obtains the ntp time from the server, adjusts then writes
// the time to the ESP32 rtc (real time clock), and displays the date and time on the built in OLED display.
//
//  Upon startup, the code initializes the serial port, wifi, graphics and udp port.  The serial port is
// used only during initialization to display when the wifi is connected and when the ntp time has been
// received from the ntp server.  Wifi is used to communicate with the ntp server.  The graphics is used
// to display the initialization and operational displays on the built in OLED.  Finally, the udp port
// receives the ntp time from the ntp server.
//
//  The main loop performs two major functions; obtains the time from the ntp server and to update the oled.
// In this example, the time is obtained from the ntp server only once, and upon receipt, is adjusted for
// time zone then written into the ESP32 rtc (real time clock).  The OLED is updated once per pass, and there
// is a 200ms delay in the main loop so the OLED is updated 5 times a second.
//
//  Before compiling and downloading the code, adjust the following settings:
//
//  1) TIME_ZONE  - currently set to -6 for Oklahoma (my home state), adjust to your timezone.
//  2) chPassword - currently set to "YourWifiPassword", adjust to your wifi password.
//  3) chSSID     - currently set to "YourWifiSsid", adjust to your wifi ssid.
//

//////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// Includes.
//
//////////////////////////////////////////////////////////////////////////////////////////////////////////////

#include                              "time.h"                              // for time calculations
#include                              <WiFi.h>                              // for wifi
#include                              <WiFiUdp.h>                           // for udp via wifi
#include                              <U8g2lib.h>                           // see https://github.com/olikraus/u8g2/wiki/u8g2reference
#include                              <Timezone_Generic.h>                  // see https://github.com/khoih-prog/Timezone_Generic

//////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// Constants.
//
//////////////////////////////////////////////////////////////////////////////////////////////////////////////

#define   FONT_ONE_HEIGHT               8                                   // font one height in pixels
#define   FONT_TWO_HEIGHT               20                                  // font two height in pixels
#define   NTP_DELAY_COUNT               20                                  // delay count for ntp update
#define   NTP_PACKET_LENGTH             48                                  // ntp packet length
#define   TIME_ZONE                     (-5)                                // offset from utc DT=-4 ST=-5
#define   UDP_PORT                      4000                                // UDP listen port
#define   ADJ_FACTOR                    4.5

//////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// Variables.
//
//////////////////////////////////////////////////////////////////////////////////////////////////////////////

char      chBuffer[128];                                                  // general purpose character buffer
char      chPassword[] =                  "wifi_password";               // your network password
char      chSSID[] =                      "SSID";         		// your network SSID
bool      bTimeReceived =                 false;                          // time has not been received
U8G2_SSD1306_128X64_NONAME_F_HW_I2C       u8g2(U8G2_R0, 16, 15, 4);       // OLED graphics
int       nWifiStatus =                   WL_IDLE_STATUS;                 // wifi status
WiFiUDP   Udp;
int       DST = 0;                                                        // 1 for DST
int       DSTtime = 0;
int       start = 0;
String    tz = {"EST5EDT,M3.2.0,M11.1.0"};

//const long  gmtOffset_sec = 3600;
//const int   daylightOffset_sec = 3600;
//const char* ntpServer = "pool.ntp.org";

void setTimezone(String);
void setTimezone(String timezone){
  Serial.printf("  Setting Timezone to %s\n",timezone.c_str());
  setenv("TZ",timezone.c_str(),1);  //  Now adjust the TZ.  Clock settings are adjusted to show the new local time
  tzset();
}

void printLocalTime(void);
void printLocalTime(){
  struct tm timeinfo;
  if(!getLocalTime(&timeinfo,5000)){
    Serial.println("Failed to obtain time with getLocalTime.");
    return;
  }
  Serial.println(&timeinfo, "\n%A, %B %d %Y %H:%M:%S zone %Z %z ");
}

//initTime called from Setup
void initTime(String timezone){
  struct tm timeinfo;

  Serial.println("initTime: initial setup of time");
  //configTime(((DST + TIME_ZONE) * 3600UL), (DST * 3600UL), "time-c.nist.gov");    // First connect to NTP server, with 0 TZ offset
  configTime(0,0,"time-c.nist.gov");    // First connect to NTP server, with 0 TZ offset
  delay(200);
  if(!getLocalTime(&timeinfo)){
    Serial.println("  Failed to obtain time");
    bTimeReceived = false;
    return;
  }
  Serial.println("  Got the time from NTP");
  // Now we can set the real timezone
  setTimezone(timezone);
  start = 1;
  bTimeReceived = true;
  Serial.println("  Initial time set.");
}

// update time called from the loop when needed
void updateNtpTime(String);
void updateNtpTime(String timezone){
  struct tm timeinfo;

  Serial.println("updateNtpTime: updating time");
  //configTime(((DST + TIME_ZONE) * 3600UL), (DST * 3600UL), "time-c.nist.gov");    // First connect to NTP server, with 0 TZ offset
  configTime(0,0,"time-c.nist.gov");    // First connect to NTP server, with 0 TZ offset
  delay(200);
  if(!getLocalTime(&timeinfo)){
    Serial.println("  Failed to obtain updated time");
    bTimeReceived = false;
    return;
  }
  Serial.println("  Got the updated time from NTP");
  // Now we can set the real timezone
  setTimezone(timezone);
  start = 1;
  bTimeReceived = true;
}


//////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// Setup
//
//////////////////////////////////////////////////////////////////////////////////////////////////////////////

void setup()
{
  // Serial.
  Serial.begin(115200);
  while (!Serial)
  {
    Serial.print('.');
  }

  // US Eastern Time Zone (New York, Detroit)
  //TimeChangeRule usEDT = {"EDT", Second, Sun, Mar, 2, -240};  // Eastern Daylight Time = UTC - 4 hours
  //TimeChangeRule usEST = {"EST", First, Sun, Nov, 2, -300};   // Eastern Standard Time = UTC - 5 hours  
  //Timezone usET(usEDT, usEST);
  //DSTtime = usET.toLocal(utc);

  pinMode(25, OUTPUT);
  digitalWrite(25, HIGH);
  delay(100);
  digitalWrite(25, LOW);
  delay(100);
  digitalWrite(25, HIGH);
  delay(100);
  digitalWrite(25, LOW);

  // OLED graphics.
  u8g2.begin();
  u8g2.setFont(u8g2_font_6x10_tr);
  u8g2.setFontRefHeightExtendedText();
  u8g2.setDrawColor(1);
  u8g2.setFontPosTop();
  u8g2.setFontDirection(0);

  // Wifi.

  // Display title.
  u8g2.clearBuffer();
  sprintf(chBuffer, "%s", "Connecting to:");
  u8g2.drawStr(64 - (u8g2.getStrWidth(chBuffer) / 2), 0, chBuffer);
  sprintf(chBuffer, "%s", chSSID);
  u8g2.drawStr(64 - (u8g2.getStrWidth(chBuffer) / 2), 31 - (FONT_ONE_HEIGHT / 2), chBuffer);
  u8g2.sendBuffer();

  // Connect to wifi.
  Serial.print("NTP clock: connecting to wifi");
  WiFi.begin(chSSID, chPassword);
  while (WiFi.status() != WL_CONNECTED)
  {
    Serial.print(".");
    delay(500);
  }

  Serial.println();
  sprintf(chBuffer, "NTP clock: WiFi connected to %s.", chSSID);
  Serial.println(chBuffer);

  digitalWrite(25, HIGH);
  delay(100);
  digitalWrite(25, LOW);

  // Display connection stats.
  // Clean the display buffer.
  u8g2.clearBuffer();

  // Display the title.
  sprintf(chBuffer, "%s", "WiFi Stats:");
  u8g2.drawStr(64 - (u8g2.getStrWidth(chBuffer) / 2), 0, chBuffer);

  // Display the ip address assigned by the wifi router.
  char  chIp[81];
  WiFi.localIP().toString().toCharArray(chIp, sizeof(chIp) - 1);
  sprintf(chBuffer, "IP  : %s", chIp);
  u8g2.drawStr(0, FONT_ONE_HEIGHT * 2, chBuffer);

  // Display the ssid of the wifi router.
  sprintf(chBuffer, "SSID: %s", chSSID);
  u8g2.drawStr(0, FONT_ONE_HEIGHT * 3, chBuffer);

  // Display the rssi.
  sprintf(chBuffer, "RSSI: %d", WiFi.RSSI());
  u8g2.drawStr(0, FONT_ONE_HEIGHT * 4, chBuffer);

  // Display waiting for ntp message.
  u8g2.drawStr(0, FONT_ONE_HEIGHT * 6, "Awaiting NTP time...");

  // Now send the display buffer to the OLED.
  u8g2.sendBuffer();
  delay(100);
  digitalWrite(25, HIGH);

  // Udp.
  Udp.begin(UDP_PORT);

  //initialize time functions using TZ
  // start=1; timereceived=true
  //initTime("EST5EDT,M3.2.0,M11.1.0");
  initTime(tz);
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// Main loop.
//
//////////////////////////////////////////////////////////////////////////////////////////////////////////////
void  loop()
{
  // Local variables.
  static  int   refresh = 0;
  static  int   nNtpDelay = NTP_DELAY_COUNT;
  static  byte  chNtpPacket[NTP_PACKET_LENGTH];
  static  int   err;

  // Check for time to send ntp request.
  if (bTimeReceived == false)
  {
    updateNtpTime(tz);  //sets TimeReceived=true if received
  }

  if (refresh > 16200 || start==0) {  //16200 one hour
    start = 1;
    WiFi.begin(chSSID, chPassword);
    while (WiFi.status() != WL_CONNECTED)
    {
      Serial.print(".");
      delay(500);
    }
    bTimeReceived = false;
    refresh = 0;
  }

  // Update OLED.
  if (bTimeReceived)
  {
    refresh += 1;
    // Save some effort...don't bother printing the refresh number
    //Serial.println(String(16200 - refresh));
    digitalWrite(25, LOW);

    // Ntp time has been received, ajusted and written to the ESP32 rtc, so obtain the time from the ESP32 rtc.
    struct  timeval tvTimeValue;
    gettimeofday(&tvTimeValue, NULL);

    // Erase the display buffer.
    u8g2.clearBuffer();

    // Obtain a pointer to local time.
    struct tm * tmPointer = localtime(&tvTimeValue.tv_sec);

    // Display the date.
    strftime(chBuffer, sizeof(chBuffer), "%a, %d %b %Y",  tmPointer);
    u8g2.setFont(u8g2_font_6x10_tr);
    u8g2.drawStr(60 - (u8g2.getStrWidth(chBuffer) / 2), 7, chBuffer); //0

    // Display the time.
    strftime(chBuffer, sizeof(chBuffer), "%H:%M:%S",  tmPointer); //%H=24h; %I=12h
    u8g2.setFont(u8g2_font_fur20_tn);
    u8g2.drawStr(10, 52 - FONT_TWO_HEIGHT, chBuffer);  //63 - FONT_TWO_HEIGHT

    // Send the display buffer to the OLED
    u8g2.sendBuffer();
  }

  // Give up some time.
  delay(200);
}
