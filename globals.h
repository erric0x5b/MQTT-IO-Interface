#define MQTT_VERSION MQTT_VERSION_3_1_1

#include "device_profile.h"

#define CYCLE_TIME 20
#define SHORT_PRESS 500
#define LONG_PRESS 3000

#define OFFLINE_OUT_CHANNELS 4

#define LUCE_CUCINA_STRISCE A0
#define LUCE_ESTERNO_FRONTE A1
#define LUCE_INGRESSO A2
#define LUCE_FARETTO A3
#define LUCE_PRANZO_FARETTI A4
#define LUCE_PRANZO_STRISCE A5
#define LUCE_CUCINA_PENSILI A6
#define LUCE_SCALA A7


typedef struct t_opta {
  bool out_1;
  bool out_2;
  bool out_3;
  bool out_4;
  bool in_1;
  bool in_2;
  bool in_3;
  bool in_4;
  bool in_5;
  bool in_6;
  bool in_7;
  bool in_8;
} t_opta;

typedef struct t_optaExp {
  bool output[8];
  int input[16];
} t_optaExp;

typedef struct t_inputStatus {
  unsigned int channelState[16] = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };
  unsigned int channelLast[16] = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };
  unsigned long channelStart[16] = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };
} t_inputStatus;

typedef struct t_instatus {
  bool lastState;
  unsigned long startTime;
} t_instatus;

long lastReconnectAttempt = 0;
long lastStart = 0;
bool cycleState = false;
bool serverConnected = false;
bool expansionAvailable = false;

const int channelMatrix[OFFLINE_OUT_CHANNELS] = { D0, D1, D2, D3 };  // mapping offline: IN_1..IN_4 -> OUT D0..D3  //assegnazione output a ogni ingresso. ES: IN_1 -> channelMatrix[0]
const int channelLEDMatrix[OFFLINE_OUT_CHANNELS] = { LED_D0, LED_D1, LED_D2, LED_D3 };
unsigned int channelState[OFFLINE_OUT_CHANNELS] = { 0, 0, 0, 0 };  // stati uscite (solo onboard) per offline mode  //variabile per memorizzare gli stati degli ingressi

// Update these with values suitable for your hardware/network.
byte mac[] = { DEVICE_MAC_0, DEVICE_MAC_1, DEVICE_MAC_2, DEVICE_MAC_3, DEVICE_MAC_4, DEVICE_MAC_5 };
IPAddress ip(DEVICE_IP_0, DEVICE_IP_1, DEVICE_IP_2, DEVICE_IP_3);
IPAddress dns(DEVICE_DNS_0, DEVICE_DNS_1, DEVICE_DNS_2, DEVICE_DNS_3);
IPAddress gateway(DEVICE_GW_0, DEVICE_GW_1, DEVICE_GW_2, DEVICE_GW_3);
IPAddress subnet(DEVICE_SUBNET_0, DEVICE_SUBNET_1, DEVICE_SUBNET_2, DEVICE_SUBNET_3);
IPAddress server(MQTT_SERVER_IP_0, MQTT_SERVER_IP_1, MQTT_SERVER_IP_2, MQTT_SERVER_IP_3);
