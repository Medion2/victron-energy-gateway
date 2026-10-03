/*
  Iskra MT681 ESP32 Victron Gateway - Public Release 1.0.0
  WT32-ETH01 / ESP32-ETH01
  Optical SML reader: ESP32 RX GPIO4 <- reader TX
                      ESP32 TX GPIO2 -> reader RX
  9600 baud, 8N1

  First start:
    AP: Iskra-MT681-Setup
    Open http://192.168.4.1
    Select/enter Wi-Fi and save.

  Web:
    /          meter dashboard
    /setup     Wi-Fi setup
    /update    firmware OTA upload
    /raw       last SML telegram as HEX
    /reboot    restart ESP32

  No meter PIN/password is used.
*/

#include <WiFi.h>
#include <ETH.h>
#include <WebServer.h>
#include <Preferences.h>
#include <Update.h>
#include <PubSubClient.h>

static const char *FW_VERSION = "1.0.0-PUBLIC";
static const char *AP_SSID = "Iskra-MT681-Setup";
static const uint8_t SML_RX_PIN = 4;
static const uint8_t SML_TX_PIN = 2;
static const uint32_t SML_BAUD = 9600;

// WT32-ETH01 / ESP32-ETH01 LAN8720
#define ETH_PHY_TYPE  ETH_PHY_LAN8720
#define ETH_PHY_ADDR  1
#define ETH_PHY_MDC   23
#define ETH_PHY_MDIO  18
#define ETH_PHY_POWER 16
#define ETH_CLK_MODE  ETH_CLOCK_GPIO0_IN
static bool ethConnected = false;

// Public release: DHCP is enabled by default.
// Set USE_STATIC_ETH_IP to true only if you want to use the example static settings below.
static const bool USE_STATIC_ETH_IP = false;
IPAddress ETH_IP(192,168,1,50);
IPAddress ETH_GATEWAY(192,168,1,1);
IPAddress ETH_SUBNET(255,255,255,0);
IPAddress ETH_DNS1(192,168,1,1);
IPAddress ETH_DNS2(1,1,1,1);

HardwareSerial SmlSerial(2);
WebServer server(80);
Preferences prefs;

String lastFrameHex;
uint8_t frameBuf[2048];
size_t frameLen = 0;
bool inFrame = false;
uint32_t lastByteMs = 0;
uint32_t totalBytes = 0;
uint32_t frameCount = 0;
uint32_t lastFrameMs = 0;

struct MeterValue {
  bool valid = false;
  double value = 0;
  int8_t scaler = 0;
  uint8_t unit = 0;
  uint64_t rawUnsigned = 0;
  int64_t rawSigned = 0;
};
MeterValue e180, e280, p1670, pL1, pL2, pL3;

WiFiClient mqttNet;
PubSubClient mqtt(mqttNet);
String cerboHost = "";
const char* MQTT_CLIENT_ID = "iskramt681-gateway";
String victronPortalId = "";
int victronPvInstance = -1;
uint32_t lastMqttPublish = 0;
uint32_t lastRegistrationPublish = 0;
const double FIXED_AC_VOLTAGE = 230.0;

static String esc(const String &s) {
  String r;
  for (size_t i=0;i<s.length();i++) {
    char c=s[i];
    if(c=='&') r+="&amp;";
    else if(c=='<') r+="&lt;";
    else if(c=='>') r+="&gt;";
    else if(c=='"') r+="&quot;";
    else r+=c;
  }
  return r;
}

static double pow10i(int8_t s) {
  double p=1.0;
  if(s>0) while(s--) p*=10.0;
  else while(s++) p/=10.0;
  return p;
}

// Decode a simple, one-byte SML TL field.
// type 5 = signed integer, type 6 = unsigned integer.
// len nibble includes TL byte itself.
static bool decodeNumber(const uint8_t *d, size_t n, size_t pos,
                         bool &isSigned, int64_t &sv, uint64_t &uv, size_t &used) {
  if(pos>=n) return false;
  uint8_t tl=d[pos];
  if(tl & 0x80) return false;
  uint8_t type=(tl>>4)&0x07;
  uint8_t total=tl&0x0F;
  if(total<2 || pos+total>n || (type!=5 && type!=6)) return false;
  size_t bytes=total-1;
  uv=0;
  for(size_t i=0;i<bytes;i++) uv=(uv<<8)|d[pos+1+i];
  isSigned=(type==5);
  if(isSigned) {
    if(bytes<8 && (uv & (1ULL << (bytes*8-1))))
      sv=(int64_t)(uv | (~0ULL << (bytes*8)));
    else
      sv=(int64_t)uv;
  } else sv=(int64_t)uv;
  used=total;
  return true;
}

static bool parseObis(const uint8_t *d, size_t n, uint8_t c, uint8_t dcode, MeterValue &out) {
  const uint8_t pat[7]={0x07,0x01,0x00,c,dcode,0x00,0xFF};
  for(size_t i=0;i+7<n;i++) {
    if(memcmp(d+i,pat,7)!=0) continue;
    size_t p=i+7;

    auto skipField = [&](size_t &q)->bool {
      if(q>=n) return false;
      uint8_t tl=d[q];
      if(tl==0x01){ q++; return true; }
      if(tl & 0x80) return false;
      uint8_t total=tl&0x0F;
      if(total==0 || q+total>n) return false;
      q+=total;
      return true;
    };

    if(!skipField(p)) continue;
    if(!skipField(p)) continue;

    uint8_t unit=0;
    if(p>=n) continue;
    if(d[p]==0x01) p++;
    else {
      bool sg; int64_t sv; uint64_t uv; size_t u;
      if(!decodeNumber(d,n,p,sg,sv,uv,u)) continue;
      unit=(uint8_t)uv; p+=u;
    }

    int8_t scaler=0;
    if(p>=n) continue;
    if(d[p]==0x01) p++;
    else {
      bool sg; int64_t sv; uint64_t uv; size_t u;
      if(!decodeNumber(d,n,p,sg,sv,uv,u)) continue;
      scaler=(int8_t)(sg?sv:(int64_t)uv); p+=u;
    }

    bool sg; int64_t sv; uint64_t uv; size_t u;
    if(!decodeNumber(d,n,p,sg,sv,uv,u)) continue;

    out.valid=true;
    out.unit=unit;
    out.scaler=scaler;
    out.rawUnsigned=uv;
    out.rawSigned=sv;
    double raw=sg ? (double)sv : (double)uv;
    out.value=raw*pow10i(scaler);
    return true;
  }
  return false;
}

static void parseFrame() {
  e180=MeterValue(); e280=MeterValue(); p1670=MeterValue();
  pL1=MeterValue(); pL2=MeterValue(); pL3=MeterValue();
  parseObis(frameBuf,frameLen,0x01,0x08,e180);
  parseObis(frameBuf,frameLen,0x02,0x08,e280);
  parseObis(frameBuf,frameLen,0x10,0x07,p1670);
  parseObis(frameBuf,frameLen,0x24,0x07,pL1);
  parseObis(frameBuf,frameLen,0x38,0x07,pL2);
  parseObis(frameBuf,frameLen,0x4C,0x07,pL3);

  lastFrameHex="";
  lastFrameHex.reserve(frameLen*3);
  char b[4];
  for(size_t i=0;i<frameLen;i++) {
    snprintf(b,sizeof(b),"%02X",frameBuf[i]);
    lastFrameHex+=b;
    if(i+1<frameLen) lastFrameHex+=' ';
  }
  frameCount++;
  lastFrameMs=millis();
}

static void smlLoop() {
  while(SmlSerial.available()) {
    uint8_t b=(uint8_t)SmlSerial.read();
    totalBytes++;
    lastByteMs=millis();

    if(!inFrame) {
      static uint8_t pre[8]; static uint8_t pn=0;
      if(pn<8) pre[pn++]=b;
      else { memmove(pre,pre+1,7); pre[7]=b; }
      if(pn>=8) {
        const uint8_t start[8]={0x1B,0x1B,0x1B,0x1B,0x01,0x01,0x01,0x01};
        if(memcmp(pre,start,8)==0) {
          memcpy(frameBuf,start,8); frameLen=8; inFrame=true; pn=0;
        }
      }
    } else {
      if(frameLen<sizeof(frameBuf)) frameBuf[frameLen++]=b;
      else { inFrame=false; frameLen=0; }
    }
  }

  if(inFrame && frameLen>16 && millis()-lastByteMs>120) {
    parseFrame();
    inFrame=false;
    frameLen=0;
  }
}

static String valueHtml(const char *obis, const char *name, const MeterValue &v, bool energy) {
  String s="<div class='card'><div class='label'>"+String(obis)+" · "+name+"</div>";
  if(!v.valid) s+="<div class='missing'>nicht empfangen</div>";
  else {
    double display=v.value;
    String unitText="Einheit "+String(v.unit);
    if(energy && v.unit==30) { display/=1000.0; unitText="kWh"; }
    else if(!energy && v.unit==27) unitText="W";
    s+="<div class='value'>"+String(display, energy?3:1)+" <span>"+unitText+"</span></div>";
    s+="<div class='small'>Scaler: "+String(v.scaler)+" · Rohwert: "+String((long long)v.rawSigned)+"</div>";
  }
  s+="</div>";
  return s;
}

static String pageHead(const String &title) {
  return "<!doctype html><html lang='de'><head><meta charset='utf-8'>"
         "<meta name='viewport' content='width=device-width,initial-scale=1'>"
         "<title>"+title+"</title><style>"
         "body{font-family:Arial,sans-serif;background:#101214;color:#eee;margin:0;padding:18px;max-width:900px;margin:auto}"
         "h1{font-size:26px}.card{background:#1c2024;border-radius:14px;padding:16px;margin:12px 0}"
         ".label,.small{color:#aeb7c0}.value{font-size:32px;font-weight:700;margin:8px 0}.value span{font-size:16px}"
         ".missing{font-size:22px;color:#ffbd66;margin-top:8px}.ok{color:#74e59a}.bad{color:#ff7b7b}"
         "a,button,input,select{font-size:16px}a{color:#7ec8ff}.btn{display:inline-block;padding:10px 13px;background:#303840;"
         "border-radius:9px;text-decoration:none;color:white;margin:4px 5px 4px 0}input,select{box-sizing:border-box;width:100%;"
         "padding:10px;margin:6px 0 12px;border-radius:8px;border:1px solid #555;background:#181b1e;color:white}"
         "button{padding:11px 18px;border:0;border-radius:8px}pre{overflow:auto;white-space:pre-wrap;background:#050505;padding:12px;border-radius:8px}"
         "</style></head><body>";
}

static double pvFromMeter(const MeterValue &v) {
  if(!v.valid) return 0.0;
  double p = -v.value;
  return p > 0 ? p : 0;
}

static double currentFromPower(double powerW) {
  if (FIXED_AC_VOLTAGE <= 0.0) return 0.0;
  return powerW / FIXED_AC_VOLTAGE;
}

static void mqttPublishValue(const String &path, double value) {
  if(!mqtt.connected() || victronPortalId.length()==0 || victronPvInstance<0) return;
  String topic="W/"+victronPortalId+"/pvinverter/"+String(victronPvInstance)+"/"+path;
  String payload="{\"value\":"+String(value,3)+"}";
  mqtt.publish(topic.c_str(),payload.c_str());
}

static String jsonStringValue(const String &j,const String &key) {
  String tag="\""+key+"\"";
  int p=j.indexOf(tag); if(p<0) return "";
  p=j.indexOf(':',p+tag.length()); if(p<0) return "";
  p=j.indexOf('"',p+1); if(p<0) return "";
  int e=j.indexOf('"',p+1); if(e<0) return "";
  return j.substring(p+1,e);
}

static int jsonPvInstance(const String &j) {
  int p=j.indexOf("\"pv1\""); if(p<0) return -1;
  p=j.indexOf(':',p); if(p<0) return -1;
  p++;
  while(p<(int)j.length() && (j[p]==' ' || j[p]=='\"')) p++;
  return j.substring(p).toInt();
}

static void mqttCallback(char* topic, byte* payload, unsigned int length) {
  String t(topic), j;
  for(unsigned int i=0;i<length;i++) j+=(char)payload[i];
  if(t=="device/"+String(MQTT_CLIENT_ID)+"/DBus") {
    String portal=jsonStringValue(j,"portalId");
    int inst=jsonPvInstance(j);
    if(portal.length() && inst>=0) {
      victronPortalId=portal;
      victronPvInstance=inst;
    }
  }
}

static void mqttLoadConfig() {
  prefs.begin("cerbo",true);
  cerboHost=prefs.getString("host","");
  prefs.end();
}

static void mqttPublishRegistration() {
  if(!mqtt.connected()) return;
  String statusTopic="device/"+String(MQTT_CLIENT_ID)+"/Status";
  String online="{\"clientId\":\""+String(MQTT_CLIENT_ID)+"\",\"connected\":1,\"version\":\"v1.0.0\",\"services\":{\"pv1\":\"pvinverter\"}}";
  mqtt.publish(statusTopic.c_str(),online.c_str());
  lastRegistrationPublish=millis();
}

static void mqttConnect() {
  if(!cerboHost.length() || mqtt.connected()) return;
  mqtt.setServer(cerboHost.c_str(),1883);
  mqtt.setCallback(mqttCallback);
  String statusTopic="device/"+String(MQTT_CLIENT_ID)+"/Status";
  String offline="{\"clientId\":\""+String(MQTT_CLIENT_ID)+"\",\"connected\":0,\"version\":\"v1.0.0\",\"services\":{\"pv1\":\"pvinverter\"}}";
  if(mqtt.connect(MQTT_CLIENT_ID,nullptr,nullptr,statusTopic.c_str(),0,false,offline.c_str())) {
    String dbusTopic="device/"+String(MQTT_CLIENT_ID)+"/DBus";
    mqtt.subscribe(dbusTopic.c_str());
    mqttPublishRegistration();
  }
}

static void mqttLoop() {
  if(!cerboHost.length()) return;
  if(!mqtt.connected()) {
    static uint32_t retry=0;
    if(millis()-retry>5000){ retry=millis(); mqttConnect(); }
  }
  mqtt.loop();

  if(mqtt.connected() && victronPvInstance<0 && millis()-lastRegistrationPublish>5000) {
    mqttPublishRegistration();
  }

  if(mqtt.connected() && victronPvInstance>=0 && millis()-lastMqttPublish>2000) {
    lastMqttPublish=millis();
    double pvTotal = pvFromMeter(p1670);
    double pvL1 = pvFromMeter(pL1);
    double pvL2 = pvFromMeter(pL2);
    double pvL3 = pvFromMeter(pL3);

    mqttPublishValue("Ac/Power",pvTotal);
    mqttPublishValue("Ac/L1/Power",pvL1);
    mqttPublishValue("Ac/L2/Power",pvL2);
    mqttPublishValue("Ac/L3/Power",pvL3);

    mqttPublishValue("Ac/L1/Voltage",FIXED_AC_VOLTAGE);
    mqttPublishValue("Ac/L2/Voltage",FIXED_AC_VOLTAGE);
    mqttPublishValue("Ac/L3/Voltage",FIXED_AC_VOLTAGE);

    mqttPublishValue("Ac/L1/Current",currentFromPower(pvL1));
    mqttPublishValue("Ac/L2/Current",currentFromPower(pvL2));
    mqttPublishValue("Ac/L3/Current",currentFromPower(pvL3));
    mqttPublishValue("Ac/Current",pvTotal / (3.0 * FIXED_AC_VOLTAGE));

    if(e280.valid && e280.unit==30) mqttPublishValue("Ac/Energy/Forward",e280.value/1000.0);
    mqttPublishValue("Ac/MaxPower",10000);
    mqttPublishValue("ErrorCode",0);
    mqttPublishValue("StatusCode",7);
    mqttPublishValue("Position",0);
  }
}

static void handleRoot() {
  String h=pageHead("Iskra MT681 Reader");
  h+="<h1>Iskra MT681 Reader <small>V"+String(FW_VERSION)+"</small></h1>";
  h+=valueHtml("1.8.0","Netzbezug",e180,true);
  h+=valueHtml("2.8.0","Einspeisung",e280,true);
  h+=valueHtml("16.7.0","Aktuelle Netzleistung",p1670,false);
  h+=valueHtml("36.7.0","L1 Leistung",pL1,false);
  h+=valueHtml("56.7.0","L2 Leistung",pL2,false);
  h+=valueHtml("76.7.0","L3 Leistung",pL3,false);
  h+="<div class='card'><div class='label'>PV → Cerbo GX</div>";
  h+="<div class='value'>"+String(pvFromMeter(p1670),0)+" <span>W</span></div>";
  h+="<div class='small'>MQTT: "+String(mqtt.connected()?"verbunden":"nicht verbunden");
  h+=" · Cerbo: "+esc(cerboHost.length()?cerboHost:"nicht eingerichtet");
  h+=" · PV Instance: "+String(victronPvInstance);
  h+=" · Spannung fest: 230 V · Phasenstrom berechnet · Gesamtstrom = P/(3×230 V) · Auto-Registrierung aktiv</div></div>";
  h+="<div class='card'><b>SML:</b> ";
  h+=(frameCount?"<span class='ok'>Daten empfangen</span>":"<span class='bad'>warte auf Telegramm</span>");
  h+="<br>Telegramme: "+String(frameCount)+"<br>UART Bytes: "+String(totalBytes);
  h+="<br>Letztes Telegramm: ";
  h+=(frameCount?String((millis()-lastFrameMs)/1000)+" s":"--");
  if(ethConnected) h+="<br>Netzwerk: Ethernet · "+ETH.localIP().toString();
  else h+="<br>Netzwerk: WLAN · "+esc(WiFi.SSID())+" · "+WiFi.localIP().toString()+" · "+String(WiFi.RSSI())+" dBm";
  h+="<br>Laufzeit: "+String(millis()/1000)+" s</div>";
  h+="<a class='btn' href='/raw'>SML Rohdaten</a>"
     "<a class='btn' href='/cerbo'>Cerbo GX</a>"
     "<a class='btn' href='/setup'>WLAN</a>"
     "<a class='btn' href='/update'>Firmware Update</a>"
     "<a class='btn' href='/reboot'>Neustart</a>";
  h+="<script>setTimeout(()=>location.reload(),5000)</script></body></html>";
  server.send(200,"text/html",h);
}

static String wifiOptions() {
  String o;
  int n=WiFi.scanNetworks();
  for(int i=0;i<n;i++) {
    o+="<option value='"+esc(WiFi.SSID(i))+"'>"+esc(WiFi.SSID(i))+" ("+String(WiFi.RSSI(i))+" dBm)</option>";
  }
  return o;
}

static void handleSetup() {
  String h=pageHead("WLAN einrichten");
  h+="<h1>WLAN einrichten</h1><div class='card'><form method='POST' action='/savewifi'>"
     "<label>WLAN auswählen</label><select name='ssid'><option value=''>-- auswählen --</option>";
  h+=wifiOptions();
  h+="</select><label>oder WLAN-Name manuell</label><input name='manual' placeholder='SSID'>"
     "<label>WLAN-Passwort</label><input type='password' name='pass' placeholder='Passwort'>"
     "<button type='submit'>Speichern & verbinden</button></form></div>"
     "<p><a href='/'>Zurück</a></p></body></html>";
  server.send(200,"text/html",h);
}

static void handleSaveWifi() {
  String ssid=server.arg("manual");
  if(!ssid.length()) ssid=server.arg("ssid");
  String pass=server.arg("pass");
  if(!ssid.length()) { server.send(400,"text/plain","WLAN-Name fehlt."); return; }
  prefs.begin("wifi",false);
  prefs.putString("ssid",ssid);
  prefs.putString("pass",pass);
  prefs.end();
  server.send(200,"text/html",pageHead("Gespeichert")+"<h1>WLAN gespeichert</h1><p>ESP32 startet neu...</p></body></html>");
  delay(1200); ESP.restart();
}

static void handleUpdatePage() {
  String h=pageHead("Firmware Update");
  h+="<h1>Firmware Update</h1><div class='card'><p>Neue <b>firmware.bin</b> auswählen.</p>"
     "<form method='POST' action='/update' enctype='multipart/form-data'>"
     "<input type='file' name='firmware' accept='.bin' required><button type='submit'>Firmware installieren</button></form>"
     "<p>Während des Updates Stromversorgung nicht trennen.</p></div><p><a href='/'>Zurück</a></p></body></html>";
  server.send(200,"text/html",h);
}

static void handleCerbo() {
  String h=pageHead("Cerbo GX");
  h+="<h1>Cerbo GX / PV</h1><div class='card'>";
  h+="<p>IP-Adresse oder Hostname des Cerbo GX eintragen. MQTT Port 1883.</p>";
  h+="<form method='POST' action='/savecerbo'><label>Cerbo GX</label>";
  h+="<input name='host' value='"+esc(cerboHost)+"' placeholder='z.B. 192.168.1.100'>";
  h+="<button type='submit'>Speichern</button></form>";
  h+="<p>Status: "+String(mqtt.connected()?"verbunden":"nicht verbunden")+"</p>";
  h+="<p>Portal ID: "+esc(victronPortalId)+"<br>PV Device Instance: "+String(victronPvInstance)+"</p>";
  h+="</div><p><a href='/'>Zurück</a></p></body></html>";
  server.send(200,"text/html",h);
}

static void handleSaveCerbo() {
  String host=server.arg("host");
  host.trim();
  prefs.begin("cerbo",false);
  prefs.putString("host",host);
  prefs.end();
  cerboHost=host;
  mqtt.disconnect();
  victronPortalId="";
  victronPvInstance=-1;
  server.send(200,"text/html",pageHead("Cerbo gespeichert")+"<h1>Gespeichert</h1><p><a href='/'>Zurück</a></p></body></html>");
}

static void startWeb() {
  server.on("/",HTTP_GET,handleRoot);
  server.on("/setup",HTTP_GET,handleSetup);
  server.on("/cerbo",HTTP_GET,handleCerbo);
  server.on("/savecerbo",HTTP_POST,handleSaveCerbo);
  server.on("/savewifi",HTTP_POST,handleSaveWifi);
  server.on("/raw",HTTP_GET,[](){
    server.send(200,"text/plain",lastFrameHex.length()?lastFrameHex:"Noch kein SML-Telegramm empfangen.");
  });
  server.on("/reboot",HTTP_GET,[](){
    server.send(200,"text/plain","ESP32 startet neu...");
    delay(500); ESP.restart();
  });
  server.on("/update",HTTP_GET,handleUpdatePage);
  server.on("/update",HTTP_POST,
    [](){
      bool ok=!Update.hasError();
      server.send(ok?200:500,"text/html",
        pageHead("Firmware Update")+(ok?"<h1>Update erfolgreich</h1><p>ESP32 startet neu...</p>":
        "<h1>Update fehlgeschlagen</h1><p>Die bisherige Firmware bleibt erhalten.</p>")+"</body></html>");
      if(ok){ delay(1000); ESP.restart(); }
    },
    [](){
      HTTPUpload &up=server.upload();
      if(up.status==UPLOAD_FILE_START) {
        if(!Update.begin(UPDATE_SIZE_UNKNOWN)) Update.printError(Serial);
      } else if(up.status==UPLOAD_FILE_WRITE) {
        if(Update.write(up.buf,up.currentSize)!=up.currentSize) Update.printError(Serial);
      } else if(up.status==UPLOAD_FILE_END) {
        if(!Update.end(true)) Update.printError(Serial);
      }
    });
  server.onNotFound([](){
    server.sendHeader("Location","http://"+WiFi.softAPIP().toString()+"/setup",true);
    server.send(302,"text/plain","");
  });
  server.begin();
}

static bool connectSavedWifi() {
  prefs.begin("wifi",true);
  String ssid=prefs.getString("ssid","");
  String pass=prefs.getString("pass","");
  prefs.end();
  if(!ssid.length()) return false;

  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid.c_str(),pass.c_str());
  uint32_t start=millis();
  while(WiFi.status()!=WL_CONNECTED && millis()-start<15000) delay(250);
  return WiFi.status()==WL_CONNECTED;
}

static void startSetupAP(bool keepStation=false) {
  if(keepStation) WiFi.mode(WIFI_AP_STA);
  else WiFi.mode(WIFI_AP);
  WiFi.softAP(AP_SSID);
  Serial.print("Setup WLAN: "); Serial.println(AP_SSID);
  Serial.print("Setup IP: "); Serial.println(WiFi.softAPIP());
}

static void onNetworkEvent(WiFiEvent_t event) {
  switch(event) {
    case ARDUINO_EVENT_ETH_START:
      ETH.setHostname("iskra-mt681-gateway");
      break;
    case ARDUINO_EVENT_ETH_GOT_IP:
      ethConnected = true;
      Serial.print("Ethernet IP: ");
      Serial.println(ETH.localIP());
      break;
    case ARDUINO_EVENT_ETH_DISCONNECTED:
    case ARDUINO_EVENT_ETH_STOP:
      ethConnected = false;
      break;
    default:
      break;
  }
}

void setup() {
  Serial.begin(115200);
  delay(300);
  Serial.println("\nIskra MT681 ESP32 Victron Gateway - Public Release 1.0.0");

  SmlSerial.begin(SML_BAUD,SERIAL_8N1,SML_RX_PIN,SML_TX_PIN);

  WiFi.onEvent(onNetworkEvent);
  ETH.begin(ETH_PHY_TYPE, ETH_PHY_ADDR, ETH_PHY_MDC, ETH_PHY_MDIO, ETH_PHY_POWER, ETH_CLK_MODE);
  if (USE_STATIC_ETH_IP) {
    ETH.config(ETH_IP, ETH_GATEWAY, ETH_SUBNET, ETH_DNS1, ETH_DNS2);
  }

  uint32_t ethStart=millis();
  while(!ethConnected && millis()-ethStart<7000) delay(100);

  bool connected = ethConnected ? true : connectSavedWifi();
  if(connected) {
    Serial.print("Netzwerk verbunden, IP: ");
    Serial.println(ethConnected ? ETH.localIP() : WiFi.localIP());
  } else {
    startSetupAP(false);
  }

  mqttLoadConfig();
  startWeb();
}

void loop() {
  server.handleClient();
  smlLoop();
  mqttLoop();

  static uint32_t lostSince=0;
  static bool recoveryAP=false;
  if(WiFi.getMode()!=WIFI_AP) {
    if(WiFi.status()!=WL_CONNECTED) {
      if(!lostSince) lostSince=millis();
      if(!recoveryAP && millis()-lostSince>30000) {
        startSetupAP(true);
        recoveryAP=true;
      }
    } else {
      lostSince=0;
    }
  }
  delay(1);
}
