/*
 * Time Switch Relay V3
 * Created by Somsak Elect, on 5/5/2019
 * 
 * WiFi Module: ESP8266-12F
 * Set up via: Web browser, http://192.168.4.1
 * 
 * Web content use WebSetting-optimal-v3.html that optimal from WebSetting-full-v3.html
 * Must set last Time ON < next Time OFF. EX. ON: Mon. 12:30, OFF: Tue. 15:30
 * 
 * Note: WiFi Access Point is visible only 10minute after power ON 
*/

//======================= Library ========================//
#include <ESP8266WiFi.h>
#include <WiFiClient.h>
#include <ESP8266mDNS.h>
#include <ESP8266WebServer.h>
#include <EEPROM.h>
#include <Wire.h>
#include <RtcDS3231.h>

//==================== Port ========================//
#define LED_PIN       2
#define SetLED(x)     digitalWrite(LED_PIN, x>0? LOW:HIGH) //ON: x=1, OFF: x=0

//===================== WiFi Access Point ===================//
String DeviceName = "Elect-";
const char *AP_PASS = "12345678"; //รหัสผ่าน WiFi

//==================== EEPROM =================//
#define EEP_SIZE  150   //EEPROM size
#define EEP_HEADER 111  //Save to address 0

//=================== RTC ===================//
//Wiring: GPIO4 - SDA, GPIO5 - SCL
RtcDS3231<TwoWire> rtc(Wire); 
char t[40];

//==================== Setup Data ===============//
#define DT_CODE         99        //Not change state at Time ON or Time OFF of day, hold state from previously value
#define DT_MM           6039      //Minute that equal DT_CODE*60 + DT_CODE
#define TIMER_LEN_MAX   29      
int timer_buffer[TIMER_LEN_MAX];  //Address: 0=End address of data (4=Type1, 28=Type2), 1=H_on, 2=M_on, 3=H_off,...


typedef struct {
  int hour;
  int minute;
}TIME;

typedef struct{
  TIME on;
  TIME off;
}TIMER;

typedef struct{
  int type;        //1=Active at same time in everyday (type 1), 2=Vary active at time in each day (type 2)
  bool onoff;      //True=Active ON, False=Active OFF, Default state is Active OFF due to use single switch relay 
  TIMER day[7];    //Active time in each day, Addr: 0=Sun, 1=Mon, ..., 6=Sat
}TIMER_DATA;

TIMER_DATA TSR;

const String day_name[] = { "Sun.", "Mon.", "Tue.", "Wed.", "Thu.", "Fri.", "Sat." };

//================= HTML and Server============//
// Set web server port number to 80
WiFiServer server(80);
//HTML Request
const String start_message = "GET /timeset-";
const String end_message = " HTTP/1.1";
String header; // Variable to store the HTTP request
String message; //Cut only the message for time setting
//Web page
const char *content1 = "<!DOCTYPE html>\
<html>\
<head>\
<meta name=\"viewport\" content=\"width=device-width, initial-scale=1\">\
<style>\
body {font-family: Arial;}\
.tab {\
  overflow: hidden;\
  border: 1px solid #ccc;\
  background-color: #f1f1f1;\
}\
.tab button {\
  background-color: inherit;\
  float: left;\
  border: none;\
  outline: none;\
  cursor: pointer;\
  padding: 14px 16px;\
  transition: 0.3s;\
  font-size: 17px;\
}\
.tab button:hover {\
  background-color: #ddd;\
}\
.tab button.active {\
  background-color: #ccc;\
}\
.tabcontent {\
  display: none;\
  padding: 6px 12px;\
  border: 1px solid #ccc;\
  border-top: none;\
}\
.btn {\
  border: none;\
  color: white;\
  padding: 14px 16px;\
  font-size: 16px;\
  cursor: pointer;\
}\
.fit-width {\
 display: block;\
  width: 100%;\
}\
.dark_btn {background-color: #e7e7e7; color: black;} \
.dark_btn:hover {background: #ddd;}\
.bright_btn {background-color: #4CAF50;} \
.bright_btn:hover {background-color: #46a049;}\
* {\
  box-sizing: border-box;\
}\
.container {\
  display: none;\
  padding: 8px;\
  background-color: white;\
}\
input[type=time], input[type=text]{\
  width: 100%;\
  padding: 16px;\
  margin: 5px 0 16px 0;\
  display: inline-block;\
  border: none;\
  background: #f1f1f1;\
  font-size: 16px;\
}\
input[type=time]:focus, input[type=text]:focus {\
  background-color: #ddd;\
  outline: none;\
}\
hr {\
  border: 1px solid #f1f1f1;\
  margin-bottom: 8px;\
}\
table {\
  font-family: arial, sans-serif;\
  border-collapse: collapse;\
  width: 100%;\
}\
td, th {\
  border: 1px solid #dddddd;\
  text-align: left;\
  padding: 8px;\
}\
tr:nth-child(even) {\
  background-color: #ffffff;\
}\
.loader {\
  display: none;\
  margin-left: auto;\
  margin-right: auto;\
  border: 16px solid #f3f3f3;\
  border-radius: 50%;\
  border-top: 16px solid #3498db;\
  width: 120px;\
  height: 120px;\
  -webkit-animation: spin 2s linear infinite;\
  animation: spin 2s linear infinite;\
}\
@-webkit-keyframes spin {\
  0% { -webkit-transform: rotate(0deg); }\
  100% { -webkit-transform: rotate(360deg); }\
}\
@keyframes spin {\
  0% { transform: rotate(0deg); }\
  100% { transform: rotate(360deg); }\
}\
.close {\
  cursor: pointer;\
  position: absolute;\
  top: 155px;\
  right: 20px;\
  padding: 12px 16px;\
  transform: translate(0%, -50%);\
  font-size: 24px;\
}\
.close:hover {background: #bbb;}\
</style>\
</head>\
<body>\
<h2>Time Switch Relay</h2>\
<div id=\"MainContent\">\
<div class=\"tab\">\
  <button id=\"defaultOpen\" class=\"tablinks\" onclick=\"openTab(event, \'Details\')\">Details</button>\
  <button class=\"tablinks\" onclick=\"openTab(event, \'Setting\')\">Setting</button>\
</div>\
<div id=\"Details\" class=\"tabcontent\">\
  <h3>Model</h3>\
  <p>TSR-01</p>\
  <h3>Relay contact</h3>\
  <p>16A AC220-240V</p>\
  <h3>Power supply</h3>\
  <p>AC110-250V</p>\
</div>\
<div id=\"Setting\" class=\"tabcontent\">\
  <div id=\"Current\" class=\"container\">\n";

String content2 = "";      //Set to current setup, Don't forget add '\n' at end char

const char *content3 = "<button class=\"btn fit-width bright_btn\" onclick=\"displaySetting(\'SettingType\')\">New</button>\
</div>\
    <div id=\"SettingType\" class=\"container\">\
    <h3>Select the setting type</h3>\
      <input type=\"radio\" name=\"type\" checked value=\"SetEveryday\">Same time in everyday<br>\
      <input type=\"radio\" name=\"type\" value=\"SetSpecific\">Custom time in each day<br><br>\
        <button class=\"btn dark_btn\" onclick=\"displaySetting(\'Current\')\">Back</button>\
        <button class=\"btn bright_btn\" onclick=\"checkRadio()\">Next</button>\
  </div>\
    <div id=\"SetEveryday\" class=\"container\">\
      <h3>Set time in everyday</h3>\
      <hr>\
      <p>Note: Not entering data. The relay will work according to the last value.</p>\
      <label for=\"timeon\"><b>Time ON</b></label>\
      <input type=\"time\" name=\"timeon\" required>\
      <label for=\"timeoff\"><b>Time OFF</b></label>\
      <input type=\"time\" name=\"timeoff\" required>\
      <button class=\"btn dark_btn\" onclick=\"displaySetting(\'SettingType\')\">Back</button>\
      <button class=\"btn bright_btn\" onclick=\"setTimeSwitch(\'SetEveryday\')\">Save</button>\
    </div>  \
    <div id=\"SetSpecific\" class=\"container\">\
      <h3>Set time in each day</h3>\
      <hr>\
      <p>Note: Not entering data. The relay will work according to the last value.</p>\
<table id=\"TimeTable\">\
  <tr>\
    <th>Day</th>\
    <th>Time ON</th>\
    <th>Time OFF</th>\
  </tr>\
  <tr>\
    <td>Sun.</td>\
    <td><input type=\"time\" name=\"time-on\" required></td>\
    <td><input type=\"time\" name=\"time-off\" required></td>\
  </tr>\
  <tr>\
    <td>Mon.</td>\
    <td><input type=\"time\" name=\"time-on\" required></td>\
    <td><input type=\"time\" name=\"time-off\" required></td>\
  </tr>\
  <tr>\
    <td>Tue.</td>\
    <td><input type=\"time\" name=\"time-on\" required></td>\
    <td><input type=\"time\" name=\"time-off\" required></td>\
  </tr>\
  <tr>\
    <td>Wed.</td>\
    <td><input type=\"time\" name=\"time-on\" required></td>\
    <td><input type=\"time\" name=\"time-off\" required></td>\
  </tr>\
  <tr>\
    <td>Thu.</td>\
    <td><input type=\"time\" name=\"time-on\" required></td>\
    <td><input type=\"time\" name=\"time-off\" required></td>\
  </tr>\
  <tr>\
    <td>Fir.</td>\
    <td><input type=\"time\" name=\"time-on\" required></td>\
    <td><input type=\"time\" name=\"time-off\" required></td>\
  </tr>\
  <tr>\
    <td>Sat.</td>\
    <td><input type=\"time\" name=\"time-on\" required></td>\
    <td><input type=\"time\" name=\"time-off\" required></td>\
  </tr>\
</table><br>\
      <button class=\"btn dark_btn\" onclick=\"displaySetting(\'SettingType\')\">Back</button>\
      <button class=\"btn bright_btn\" onclick=\"setTimeSwitch(\'TimeTable\')\">Save</button>\
    </div>\
</div>\
</div>\
<div id=\"Waiting\" class=\"loader\"></div>\
<script>\
document.getElementById(\"defaultOpen\").click();\
function openTab(evt, tabName) {\
  var i, tabcontent, tablinks;\
  tabcontent = document.getElementsByClassName(\"tabcontent\");\
  for (i = 0; i < tabcontent.length; i++) {\
    tabcontent[i].style.display = \"none\";\
  }\
  tablinks = document.getElementsByClassName(\"tablinks\");\
  for (i = 0; i < tablinks.length; i++) {\
    tablinks[i].className = tablinks[i].className.replace(\" active\", \"\");\
  }\
  document.getElementById(tabName).style.display = \"block\";\
  evt.currentTarget.className += \" active\";\
  if(tabName === \'Setting\'){ displaySetting(\'Current\'); }\
}\
function displaySetting(stepName){\
    var stepcontent = document.getElementsByClassName(\"container\");\
    for (var i=0; i<stepcontent.length; i++){\
      stepcontent[i].style.display = \"none\";\
    }\
    document.getElementById(stepName).style.display = \"block\";\
}\
function checkRadio(){\
    var r = SettingType.querySelectorAll(\"input[type=radio]\");\
    for(var a=0; a<r.length; a++){\
      if(r[a].checked){\
            displaySetting(r[a].value); \
            break;\
        }\
    }\
}\
function setTimeSwitch(nameType){\
    var syn = \"-\";    \
    var error = false;\
    var message=\"\"; \
    var d = new Date(); \
    var dt = d.getFullYear().toString()+syn+d.getMonth().toString()+syn+d.getDate().toString()+syn+d.getHours().toString()+syn+d.getMinutes().toString()+syn+d.getSeconds().toString();\
    var tonoff; var max; \
    if(nameType==\'Disable\'){\
      max = 0;\
    }else{\
      tonoff = document.getElementById(nameType).querySelectorAll(\"input[type=time]\");\
      max = tonoff.length;\
    }\
    message += \"timeset-\"+dt+syn+max.toString()+syn;\
    var c_on = 0;\
    for(var i=0; i<max; i++){\
      var buff;\
        if((i==0 || i%2==0) && tonoff[i].value){\
      c_on++;\
        }\
        if(!tonoff[i].value){\
          buff = \"99:99\"; \
        }else{\
          buff = tonoff[i].value.toString();\
        }\
        message += buff.replace(/:/g, \'-\');\
        if(i!=max-1){ message += syn; }\
    }\
    if(c_on>0 || max==0){\
      var request = new XMLHttpRequest();\
      request.open(\"GET\", message, true);\
      request.send(null);\
      displayLoader();\
    }else{\
      alert(\"Please enter time ON at least one value.\");\
    }\
}\
function displayLoader(){\
    document.getElementById(\"MainContent\").style.display = \"none\";\
    document.getElementById(\"Waiting\").style.display = \"block\";\
    setTimeout(refreshWeb, 2000);\
}\
function refreshWeb(){\
  location.reload(true); \
}\
function disableTimer(){\
  if(confirm(\"Are you sure you want to disable the timer?\")){\
      setTimeSwitch(\'Disable\');\
    }\
}\
</script>\
</body>\
</html>";

//==================== Other =================//
//Count time to turn off WiFi
unsigned long startTime; //Save start time
#define HIDE_WIFI_TIME  10*60*1000    //ms
//LED Blink after turn off WiFi
unsigned long last_blink_time = 0;
bool blink_state;
#define INTERVAL_BLINK_TIME 1000

//=================================== Start Function ====================//
void createWiFiAccessPoint(){
  Serial.println();
  Serial.print("Create WiFi Access Point");
  Serial.println();
  //Clear previously config
  WiFi.persistent(false);
  WiFi.disconnect();
  WiFi.mode(WIFI_AP);
  //Get MAC address
  byte mac[6]; //6 byte array to hold the MAC address
  WiFi.softAPmacAddress(mac); //ex. mac array in form Hex value = {B4,E6,2D,B2,48,6E} but actual MAC ID = {6E,48,B2,2D,E6,B4} <= reverse
  String macID = String(mac[4], HEX) + String(mac[5], HEX); //Convert byte to HEX
  macID.toUpperCase();

  DeviceName += macID; //Update device name 
  const char* AP_SSID = DeviceName.c_str(); //convert String to Char*

  //Create WiFi Access Point without set IP address, default IP: 192.168.4.1
  if(!WiFi.softAP(AP_SSID, AP_PASS)){
    Serial.println("failed to create AP, we should reset as see if it connects");
    softwareReset();
  }
  
  Serial.print("WiFi Access Point started: "); Serial.println(WiFi.softAPIP());

}

//After reset, this method good work without hanging! with changing WiFi mode
void softwareReset(){
   //DB.println("Software reset!");
   delayMillis(1000);
   ESP.reset();
   delayMillis(2000);
}

//Avoid WDT reset!: use for delay >= 1000
void delayMillis(int d){
  int n = d/10;
  for(int i=0; i<n; i++){
    delay(10);
  }
}

void clearEEPROM(int start, int f){
  //Serial.println("Clear EEPROM");
  for(int i=start; i<f; i++){
    EEPROM.write(i, 0);
  }
  EEPROM.commit(); //Must alway call this function when you write to EEPROM finish. if you don't use, writing EEPROM will unsuccess.
}

void clearTSR(){
  int i;
  TSR.type = 0;
  TSR.onoff = false;
  for(i=0; i<7; i++){
      TSR.day[i].on.hour = 0;
      TSR.day[i].on.minute = 0;
      TSR.day[i].off.hour = 0;
      TSR.day[i].off.minute = 0;
  }
}

void setToTSR(){
  int i, y=0;
  TSR.onoff = false;
  if(timer_buffer[y++]==28){
    TSR.type = 2;
    for(i=0; i<7; i++){
      TSR.day[i].on.hour = timer_buffer[y++];
      TSR.day[i].on.minute = timer_buffer[y++];
      TSR.day[i].off.hour = timer_buffer[y++];
      TSR.day[i].off.minute = timer_buffer[y++];
    }
  }else{
    TSR.type = 1;
    for(i=0; i<7; i++){
      TSR.day[i].on.hour = timer_buffer[1];
      TSR.day[i].on.minute = timer_buffer[2];
      TSR.day[i].off.hour = timer_buffer[3];
      TSR.day[i].off.minute = timer_buffer[4];
    }
  }
}

bool setTimerBufferToEPPROM(){
  int sum=0, i, len = timer_buffer[0]+1;
  Serial.print("Start save timer setup: "); Serial.println(len);
  //Clear EEP
  clearEEPROM(0, len+2);   
  //Call sum and write EEPROM
  EEPROM.write(0, EEP_HEADER); //Set header to first address
  Serial.print("Timer data: ");
  for(i=0; i<len; i++){
    EEPROM.write(i+1, timer_buffer[i]);
    sum += timer_buffer[i];
    Serial.print(timer_buffer[i]); Serial.print(" ");
  }
  Serial.println();
  Serial.print("SUM address: "); Serial.println(i+1);
  Serial.print("SUM value: "); Serial.println(sum);
  Serial.print("SUM H: "); Serial.println((sum>>8)&0xFF);
  Serial.print("SUM L: "); Serial.println(sum&0xFF);
  EEPROM.write(i+1, (sum>>8)&0xFF); //Set sum to end address
  EEPROM.write(i+2, sum&0xFF);
  //Confirm save
  if(EEPROM.commit()){
    setToTSR();   //Set TSR
    return true;
  }else{
    clearTSR();  //Clear
    return false;
  }
}

void clearTimerData(){
  int i;
  for(i=0; i<TIMER_LEN_MAX; i++){
    timer_buffer[i] = 0;
  }
}

void setTimerBufferFromEEPROM(){
  int i, eep_sum[2], sum2, sum1;
  Serial.println(); Serial.println("Start set Timer Data from EEPROM...");
  //Clear
  clearTimerData();
  clearTSR();
  //Check header
  if(EEPROM.read(0)!=EEP_HEADER){
    Serial.println("Header not match!");
    return;
  }
  //Read data
  timer_buffer[0] = EEPROM.read(1); //End Address, type
  sum1 += timer_buffer[0];
  Serial.print("Timer data: "); Serial.print(timer_buffer[0]); Serial.print(" ");
  for(i=1; i<timer_buffer[0]+1; i++){
    timer_buffer[i] = EEPROM.read(i+1);
    sum1 += timer_buffer[i];
    Serial.print(timer_buffer[i]); Serial.print(" ");
  }
  Serial.println();
  Serial.print("SUM1: "); Serial.println(sum1);
  //Check sum
  Serial.print("SUM2 address: "); Serial.println(i+1);
  eep_sum[0] = EEPROM.read(i+1);
  eep_sum[1] = EEPROM.read(i+2);
  sum2 = eep_sum[0]<<8|(eep_sum[1]&0xFF);
  Serial.print("SUM2 H: "); Serial.println(eep_sum[0]); 
  Serial.print("SUM2 L: "); Serial.println(eep_sum[1]);
  Serial.print("SUM2 value: "); Serial.println(sum2);
  if(sum1!=sum2){
    clearTimerData();
    Serial.println("Check sum not match, so clear data");
  }else{
    setToTSR();
    Serial.println("Set Timer Data for EEPROM success!");
  }
  Serial.println();
 
}

void setContent2(){
  int i; String synx = " - "; int u;
  content2 = "<h3>Current Setting<span class=\"close\" onclick=\"disableTimer()\">&times;</span></h3><hr>";
  switch(TSR.type){
    case 1: 
      content2 += "<p>Work same time in everyday</p>\n";
      content2 += "<h3>Time ON"+synx+"Time OFF</h3>\n";
      //u = TSR.day[0].on.hour==DT_CODE? 1:0;
      //content2 += "<p>"+getTwoDigit(TSR.day[0].on.hour)+synx[u]+getTwoDigit(TSR.day[0].on.minute)+synx[2]+getTwoDigit(TSR.day[0].off.hour)+synx[u]+getTwoDigit(TSR.day[0].off.minute)+"</p>";
      content2 += "<p>"+getTimeString(TSR.day[0].on.hour, TSR.day[0].on.minute)+synx+getTimeString(TSR.day[0].off.hour, TSR.day[0].off.minute)+"</p>";
      break;
    case 2: 
      content2 += "<p>Work custom time</p>\n"; 
      content2 += "<h3>Day Time ON"+synx+"Time OFF</h3>\n";
      for(i=0; i<7; i++){
       // u = TSR.day[i].on.hour==DT_CODE? 1:0;
        //String buff = day_name[i]+" "+getTwoDigit(TSR.day[i].on.hour)+synx[u]+getTwoDigit(TSR.day[i].on.minute)+synx[2]+getTwoDigit(TSR.day[i].off.hour)+synx[u]+getTwoDigit(TSR.day[i].off.minute);
        String buff = day_name[i]+" "+getTimeString(TSR.day[i].on.hour, TSR.day[i].on.minute)+synx+getTimeString(TSR.day[i].off.hour, TSR.day[i].off.minute);
        content2 += "<p>"+buff+"</p>";
        Serial.println(buff);
      }
      break;
    default: content2 = "";
  }
}

String getTwoDigit(int data){
  if(data>=0 & data<10){
    return "0"+String(data);
  }
  //if(data==DT_CODE){ return ""; } //99 mean this time is disable
  return String(data);
}

String getTimeString(int hour, int minute){
  if(hour==DT_CODE || minute==DT_CODE){
    return "none";
  }
  return getTwoDigit(hour)+":"+getTwoDigit(minute);
}

//Thank: Sakamoto's methods, https://en.wikipedia.org/wiki/Determination_of_the_day_of_the_week
byte DayOfWeek(int y, byte m, byte d) {   // y > 1752, 1 <= m <= 12
  static int t[] = {0, 3, 2, 5, 0, 3, 5, 1, 4, 6, 2, 4};
  y -= m < 3; //if m<3 is ture, it will return 1, otherwise will return 0
  return ((y + y/4 - y/100 + y/400 + t[m-1] + d) % 7) + 1; // 01 - 07, 01 = Sunday
}

void checkTimeForActiveSW(){
  RtcDateTime now = rtc.GetDateTime();    //get the time from the RTC
  byte dw = DayOfWeek(now.Year(), now.Month(), now.Day());
  sprintf(t, "%02d:%02d:%02d %02d/%02d/%02d",  now.Hour(), now.Minute(), now.Second(), now.Day(), now.Month(), now.Year());  
  Serial.print(F("Time/Date: ")); Serial.print(t); Serial.print(" "); Serial.println(day_name[dw-1]);
  //Check disable flag before check time matching
  if(TSR.type!=0){
    //Convert to minute
    int cm = now.Hour()*60 + now.Minute();
    int sm_on = TSR.day[dw-1].on.hour*60 + TSR.day[dw-1].on.minute;
    int sm_off = TSR.day[dw-1].off.hour*60 + TSR.day[dw-1].off.minute;

    if(sm_on!=DT_MM && cm>=sm_on && cm<sm_off){
      //Active ON
      TSR.onoff = true;
    }else if(sm_off!=DT_MM && cm>=sm_off){
      //Active OFF
      TSR.onoff = false;
    }
    
    /*
    //Check case
    if(sm_on<sm_off){
      if(cm>=sm_on && cm<sm_off){
        //Active ON
        TSR.onoff = true;
      }else{
        //Active OFF
        TSR.onoff = false;
      }
    }else{
      if(cm>=sm_off && cm<sm_on){
        //Active OFF
        TSR.onoff = false;
      }else{
        //Active ON
        TSR.onoff = true;
      }
    }*/
  }
  Serial.print("Relay is active "); Serial.println(TSR.onoff? "ON":"OFF");
  //Set port to control relay
  
}

void setup() {
  int i; bool result;
  //Debug
  Serial.begin(115200);
  Serial.println();
  //EEPROM
  EEPROM.begin(EEP_SIZE);
  //RTC
  rtc.Begin();
  //Set Pin
  pinMode(LED_PIN, OUTPUT);
  SetLED(1);    //ON LED
  //Create AP
  createWiFiAccessPoint();
  //Save Time
  startTime = millis();
  //Start server
  server.begin();        
  server.setNoDelay(true);               
  Serial.println("HTTP server started");
  Serial.println();
  //HTML and TSR
  setTimerBufferFromEEPROM();
  setContent2();


/*
  Serial.print("Timer type: "); Serial.println(TSR.type);
  for(i=0; i<7; i++){
    Serial.println("Day/ ON/ OFF"); 
    Serial.println(String(i)+"/ "+String(TSR.day[i].on.hour)+":"+String(TSR.day[i].on.minute)+"/ "+String(TSR.day[i].off.hour)+":"+String(TSR.day[i].off.minute));
  }
  Serial.println(); */
  
}

void loop(){

  WiFiClient client = server.available();              // Listen for incoming clients

  if (client) {                             // If a new client connects,
    Serial.println("New Client.");          // print a message out in the serial port
    String currentLine = "";                // make a String to hold incoming data from the client
    while (client.connected()) {            // loop while the client's connected
      if (client.available()) {             // if there's bytes to read from the client,
        char c = client.read();             // read a byte, then
        Serial.write(c);                    // print it out the serial monitor
        header += c;

        //Set setting message
        if(header.startsWith(start_message) && header.endsWith(end_message)){
            int y = header.indexOf(end_message);
            message = header.substring(start_message.length(), y); 
            /*
            Serial.println();
            Serial.print("Start Setting Data: ");
            Serial.println(message);
            Serial.print("End Setting Data\n");   
            Serial.println();       */      
        }
        if (c == '\n') {                    // if the byte is a newline character
          // if the current line is blank, you got two newline characters in a row.
          // that's the end of the client HTTP request, so send a response:
          if (currentLine.length() == 0) {
            String reload = "";
            //OFF LED
            SetLED(0);
            // HTTP headers always start with a response code (e.g. HTTP/1.1 200 OK)
            // and a content-type so the client knows what's coming, then a blank line:
            client.println("HTTP/1.1 200 OK");
            //Check Setting Message
            if(message!=""){
              client.println("Content-type:text/xml");
              client.println("Connection: keep-alive");
              client.println();
              
              //============== Set message to request array ==============//
              int i, len, request[40];
              Serial.println();
              Serial.print("Setting message: "); Serial.println(message);
              //Check message
              if(!message.endsWith("-")){ message += "-"; }
              //Serial.println("Convert to Char Array...");
              // Length (with one extra character for the null terminator)
              int str_len = message.length() + 1; 
              // Prepare the character array (the buffer) 
              char char_array[str_len];
              //Convert
              message.toCharArray(char_array, str_len);
              //Set to request buffer
              Serial.print("Request array: ");
              String a = ""; int y = 0;
              for(i=0; i<str_len-1; i++){
                //Serial.print(i); Serial.print(": "); Serial.println(char_array[i]);
                if(char_array[i]=='-'){
                  request[y] = a.toInt();
                  Serial.print(request[y]); Serial.print(" ");
                  a = ""; y++;
                }else{
                  a.concat(char_array[i]);
                }
              } 
              Serial.println();
              
              //================== Set RTC =========================//
              Serial.println("Get current time from request array to set to RTC...");
              //DS3231 note: Leap-Year Compensation Valid Up to 2100, https://datasheets.maximintegrated.com/en/ds/DS3231.pdf
              //Current Date time from request array by Addr: 0=Year(UK), 1=Month(0-11), 2=Day(1-31), 3=Hour(0-23), 4=Minute(0-59), 5=Second(0-59)
              //RtcDateTime: RtcDateTime(YY, MM, DD, HH, mm, ss) by MM: 1-12, DD: 1-31, HH: 0-23, mm: 0-59
              RtcDateTime currentTime = RtcDateTime(request[0]<2100? request[0]%2000:request[0]%2100, request[1]+1, request[2], request[3], request[4], request[5]); 
              rtc.SetDateTime(currentTime); //Configure the RTC 

              //================= Set Timer Buffer ================//
              clearTimerData(); //Clear
              Serial.println("Set to timer_buffer..."); 
              timer_buffer[0] = request[6]*2;
              //Check type
              if(timer_buffer[0]>0){
                //Normal
                Serial.print("Data: "+String(timer_buffer[0])+" ");
                for(i=0; i<timer_buffer[0]; i++){
                timer_buffer[i+1] = request[7+i];
                    Serial.print(String(timer_buffer[i+1])+" ");
                }
                Serial.println();
                //Copy to EEPROM
                setTimerBufferToEPPROM();
              }else{
                //Disable
                Serial.println("Disable timer");
                clearEEPROM(0, TIMER_LEN_MAX);
                clearTSR();
              }
              //Set content2
              setContent2();
              Serial.print("Set success");
              Serial.println();
              
            }else{
              
              //================ Web content ====================//
              Serial.println("Print web content to browser...");
              Serial.println();
              //Send web page
              client.println("Content-type:text/html");
              client.println("Connection: close");
              client.println();
              client.print(content1);
              client.print(content2+"\n");
              client.print(content3);
            }
            // The HTTP response ends with another blank line
            client.println();
            // Break out of the while loop
            break;
          } else { // if you got a newline, then clear currentLine
            currentLine = "";
          }
        } else if (c != '\r') {  // if you got anything else but a carriage return character,
          currentLine += c;      // add it to the end of the currentLine
        }
      }
    }
    // Clear the header variable
    header = "";
    message = "";
    // Close the connection
    client.stop();
    Serial.println("Client disconnected.");
    Serial.println("");
    //ON LED
    SetLED(1);
    //Save start time (Start time couter of turn-off WiFi after set timer finish)
    startTime = millis();
  }else{
    //Check time for active switch relay
    checkTimeForActiveSW();
    delay(1000);
  }

  //Check time for turn OFF WiFi
  if((millis()-startTime)>HIDE_WIFI_TIME){
     Serial.println();
     Serial.println("Turn OFF WiFi...");
     Serial.println();
     //Thank: https://circuits4you.com/2019/01/08/esp8266-turn-off-wifi-save-power/
     WiFi.mode(WIFI_OFF);
     //Blink LED
     if(millis() > (last_blink_time+INTERVAL_BLINK_TIME)){
        last_blink_time = millis();   //Save last time
        blink_state = !blink_state;   //Toggle state
        SetLED(blink_state);          //Set LED
     }
  }



  
}



