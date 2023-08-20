#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include "FS.h"
#include <ArduinoJson.h>
ESP8266WebServer server(80);
#define ICS_NO 2
String ESPSESSIONID("");
String login_id("");
String login_pwd("");
String apssid("");
String appwd("");
String dns_name("");
String email_id("");
void(*resetFunc)(void)=0;
static const uint8_t dataPin=13;
static const uint8_t clockPin=14;
static const uint8_t latchPin=4;
String getFileContent(const char* filename){
  File file = SPIFFS.open(filename,"r");
  String config_json = file?file.readStringUntil('\0'):"\0";
  file.close();
  return config_json;
}
bool shouldRestartWithChanges(String data){
  StaticJsonBuffer<512> jsonBuffer;
  JsonObject &root = jsonBuffer.parseObject(data);
  JsonObject &newConfig = jsonBuffer.createObject();
  
  newConfig["login_id"]=root["login_id"]!=""?root["login_id"]:login_id;
  newConfig["login_pwd"]=root["login_pwd"]!=""?root["login_pwd"]:login_pwd;
  newConfig["apssid"]=root["apssid"]!=""?root["apssid"]:apssid;
  newConfig["appwd"]=root["appwd"]!=""?root["appwd"]:appwd;
  newConfig["dns_name"]=root["dns_name"]!=""?root["dns_name"]:dns_name;
  newConfig["email_id"]=root["email_id"]!=""?root["email_id"]:email_id;

  File file =SPIFFS.open("/config.json","w");
  newConfig.printTo(file);
  file.close();
  return root["devicestatus"]=="restart";
}

bool isAuthentified(){
  if (server.hasHeader("Cookie")){   
    String cookie = server.header("Cookie");
    if (cookie.indexOf("ESPSESSIONID="+ESPSESSIONID) != -1) {
      return true;
    }
  }
  return false;
}
void handleLogin(){
  if(server.hasArg("logout")){
    String header = "HTTP/1.1 301 OK\r\nSet-Cookie: ESPSESSIONID=0\r\nLocation: /login\r\nCache-Control: no-cache\r\n\r\n";
    server.sendContent(header);
    return;
  }else if(isAuthentified()){
    String header = "HTTP/1.1 301 OK\r\nSet-Cookie: ESPSESSIONID="+ESPSESSIONID+"\r\nLocation: /\r\nCache-Control: no-cache\r\n\r\n";
    server.sendContent(header);
    return;
  }
  if (server.hasArg("uname") && server.hasArg("pwd")){
    if (server.arg("uname") == login_id &&  server.arg("pwd") == login_pwd){
      String header = "HTTP/1.1 301 OK\r\nSet-Cookie: ESPSESSIONID="+ESPSESSIONID+"\r\nLocation: /\r\nCache-Control: no-cache\r\n\r\n";
      server.sendContent(header);
      return; 
    }else{
      String header = "HTTP/1.1 301 OK\r\nSet-Cookie: ESPSESSIONID=0\r\nLocation: /login?authentified=no\r\nCache-Control: no-cache\r\n\r\n";
      server.sendContent(header);
      return;
    }
  }
  String login_html("/login.html");
  if(server.hasArg("authentified"))
  {
    if(server.arg("authentified")=="no")
      login_html="/loginerror.html";
  }
  login_html=getFileContent(login_html.c_str());
  server.send(200, "text/html", login_html);
}

void manipulateAppliances(String data,bool initiate=false){
  StaticJsonBuffer<512> jsonBuffer;
  JsonObject &root = jsonBuffer.parseObject(data);
  if(root.success()){
   JsonArray& ids = root["ids"]; 
   int index = ids.size()-1;
   digitalWrite(latchPin,LOW);
   shiftOut(dataPin,clockPin,LSBFIRST,0);
   if(index<ICS_NO-1)
    shiftOut(dataPin,clockPin,LSBFIRST,255);
   while(index>=0){
    String room_id = ids.get<String>(index);
    byte value_of_room = 255-root[room_id].as<byte>();
    shiftOut(dataPin,clockPin,LSBFIRST,value_of_room);
    index--;
   }
   digitalWrite(latchPin,HIGH);
   if(initiate==true){
    File states_json = SPIFFS.open("/states.json","w");
    root.printTo(states_json);
    states_json.close();
   }
 }
}

void handlePages(){
  if (!isAuthentified()){
    String header = "HTTP/1.1 301 OK\r\nSet-Cookie: ESPSESSIONID=0\r\nLocation: /login\r\nCache-Control: no-cache\r\n\r\n";
    server.sendContent(header);
  }else{
    String html_file = (server.uri()=="/"?"/home":server.uri())+".html";
    if(server.uri()=="/settings/change"){
      if(server.hasArg("plain")){
        String data = server.arg("plain");
        if(shouldRestartWithChanges(data))
        {
          server.send(204,"");
          resetFunc();
        }
        else server.send(200,"");
      }else server.send(403,"");
    }else if(server.uri()=="/home/stateappliances"){
      if(server.hasArg("plain")){
        String data = server.arg("plain");
        manipulateAppliances(data);
      }
    }else{
      String html_page = getFileContent(html_file.c_str());
      if(html_page!="\0"){
        server.send(200,"text/html",html_page);
      }else{
        server.send(204,"text/html","<h1>HTTP Error 204: "+html_file+" as content is not found.");
      }
    }
  }
  return;
}

void handlePageNotFound(){
  server.send(404,"text/html","<h1>HTTP Error 404: "+server.uri()+" as page is not found.");
}

void handleChanges(){
  if (!isAuthentified()){
    String header = "HTTP/1.1 301 OK\r\nSet-Cookie: ESPSESSIONID=0\r\nLocation: /login\r\nCache-Control: no-cache\r\nAllow-Control-Allow-Origin:* \r\n\r\n";
    server.sendContent(header);
  }
  if(server.hasArg("plain")){
    if(server.arg("plain") == login_pwd){
      server.send(200,"");
    }else server.send(204,"");
  }else server.send(204,"");
}
void formatPrevious(){
  server.send(200,"text/html","<br><h4><progress id=\'progress\' value=\'0\' max=\'100\' width=\'200px\'></progress></h4>");
  File states_json=SPIFFS.open("/states.json","w");
  if(!states_json) Serial.println("No file exist");
  states_json.print("no states available");
  states_json.close();
  resetFunc();
}
void handleAbout(){
  String about_html = getFileContent("/about.html");
  if(about_html!="\0")
    server.send(200,"text/html",about_html);
  else server.send(204,"text/html","<h1>HTTP Error 204: "+about_html+" as content is not found.");
}

void createAP(const char* ssid, const char* password){
  WiFi.mode(WIFI_OFF);
  delay(2000);
  WiFi.mode(WIFI_AP);
  WiFi.softAP(ssid,password);
}

void setup(){
  Serial.begin(115200);
  Serial.println("Device started");
  pinMode(latchPin,OUTPUT);
  pinMode(dataPin,OUTPUT);
  pinMode(clockPin,OUTPUT);
  digitalWrite(latchPin,LOW);
  shiftOut(dataPin,clockPin,MSBFIRST,255);
  shiftOut(dataPin,clockPin,MSBFIRST,255);
  digitalWrite(latchPin,HIGH);
  delay(2000);
  SPIFFS.begin();
  String states_json = getFileContent("/states.json");
  if(states_json!="no states available")
    manipulateAppliances(states_json,true);
  String config_json = getFileContent("/config.json");
  if(config_json!="\0"){
    StaticJsonBuffer<512> jsonBuffer;
    JsonObject &root = jsonBuffer.parseObject(config_json);
    if(root.success()){
      login_id = root["login_id"].as<String>();
      login_pwd = root["login_pwd"].as<String>();
      apssid = root["apssid"].as<String>();
      appwd = root["appwd"].as<String>();
      dns_name = root["dns_name"].as<String>();
      email_id = root["email_id"].as<String>();
      createAP(apssid.c_str(), appwd.c_str());

      ESPSESSIONID=String(random(100,10000),HEX);
      
      server.on("/", handlePages);
      server.on("/home",handlePages);
      server.on("/home/stateappliances",handlePages);
      server.on("/home/formatstates",formatPrevious);
      server.on("/login", handleLogin);
      server.on("/about", handleAbout);
      server.on("/settings", handlePages);
      server.on("/settings/change", handlePages);
      server.on("/settings/isauthentified", handleChanges);
      server.on("/switches", handlePages);
      server.onNotFound(handlePageNotFound);

      const char * headerkeys[] = {"User-Agent","Cookie"} ;
      size_t headerkeyssize = sizeof(headerkeys)/sizeof(char*);

      server.collectHeaders(headerkeys, headerkeyssize );
      server.begin();
    }
  }
}

void loop(){
  server.handleClient();
}


