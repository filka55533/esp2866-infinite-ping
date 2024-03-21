#include "ESP8266WiFi.h"
#include "Pinger.h"

#ifdef STASSID
  #undef STASSID
  #undef STAPSK
#endif

#define STASSID "Mywlan"
#define STAPSK "12345678"

typedef enum _STATE {
  START = 0,
  WAIT_CONNECTION,
  WAIT_IP,
  ECHO_SENDING
} STATE;

static STATE state = START;

class InfinitePinger : private Pinger {
private:
  bool _is_sending = false;

public: 
  bool start_infinite_ping(IPAddress ip, std::function<void(void)> on_receive) {
    
    this->OnReceive([ip, on_receive](const PingerResponse& resp){
      if (ip != resp.DestIPAddress)
        return false;

      on_receive();
      return true;
    });

    this->OnEnd([ip, this](const PingerResponse& resp) {
      return 
        ip == resp.DestIPAddress && this->_is_sending ? 
          this->Ping(ip, 1, 1000) :
          false;    
    });

    return this->_is_sending = this->Ping(ip, 1, 1000);
  }

  void stop_infinite_ping() {
    this->_is_sending = false;
  }

  bool is_sending() {
    return this->_is_sending;
  }
};

InfinitePinger pinger;

IPAddress gateway;

WiFiEventHandler on_connect_h, on_disconnect_h, on_get_ip_h;

void blink(const int ms) {
  digitalWrite(LED_BUILTIN, LOW);
  delay(ms / 2);                     
  digitalWrite(LED_BUILTIN, HIGH);  
  delay(ms / 2);
}

void on_connect_to_ap(const WiFiEventStationModeConnected& ev) {
  Serial.printf("[%s]: Success connection on AP with SSID %s\r\n", __func__, ev.ssid.c_str());
  state = WAIT_IP;
}

void on_disconnect_from_ap(const WiFiEventStationModeDisconnected& ev) {
  Serial.printf("[%s]: Disconnect from AP with SSID %s: reason code: %d\r\n", __func__, ev.ssid, ev.reason);
  pinger.stop_infinite_ping();
  state = START;
}

void on_get_ip_from_ap(const WiFiEventStationModeGotIP& ev) {
  Serial.printf("[%s]: Success on getting ip\r\n", __func__);
  Serial.printf("[%s]: Ip is: %d.%d.%d.%d\r\n", __func__, ev.ip[0], ev.ip[1], ev.ip[2], ev.ip[3]);
  gateway = std::move(ev.gw);
  Serial.printf("[%s]: Gateway is: %d.%d.%d.%d\r\n", __func__, gateway[0], gateway[1], gateway[2], gateway[3]);
  state = ECHO_SENDING;
}

void start_connection() {
  WiFi.begin(STASSID, STAPSK);
  on_connect_h = WiFi.onStationModeConnected(on_connect_to_ap);
  on_disconnect_h = WiFi.onStationModeDisconnected(on_disconnect_from_ap);
  on_get_ip_h = WiFi.onStationModeGotIP(on_get_ip_from_ap);
  state = WAIT_CONNECTION;
}

void init_wifi() {
  Serial.printf("[%s]:Initialize connection to ap with ssid \"%s\"\r\n", __func__, STASSID);
  WiFi.mode(WIFI_STA);
}

void setup() {
  pinMode(LED_BUILTIN, OUTPUT); 
  Serial.begin(115200);
  Serial.println(ESP.getFullVersion());
  Serial.printf("[%s]: Start program\r\n", __func__); 
  init_wifi();
}

void on_receive_ping() {
  Serial.printf("[%s]: Get echo from %d.%d.%d.%d\r\n", __func__, gateway[0], gateway[1], gateway[2], gateway[3]);
  blink(1000);
}

void loop() {
  switch (state) {
    case START:
      start_connection();
      break;
    case WAIT_CONNECTION:
      Serial.printf("[%s]: Waiting for connecting on AP\r\n", __func__);
      break;
    case WAIT_IP:
      Serial.printf("[%s]: Wait for getting IP\r\n", __func__);
      break;
    case ECHO_SENDING:
      if (!pinger.is_sending() && !pinger.start_infinite_ping(gateway, on_receive_ping)) {
        Serial.printf("[%s]: Error on start pinging\r\n", __func__);
        break;
      }
      return;
  }
  delay(1000);
}
