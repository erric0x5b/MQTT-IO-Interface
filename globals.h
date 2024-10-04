#define MQTT_VERSION MQTT_VERSION_3_1_1

#define CYCLE_TIME 20
#define SHORT_PRESS 1000
#define LONG_PRESS 3000

#define OUT_CHANNELS 4
#define IN_CHANNELS 9
#define EXP_INPUT_CHANNELS 16
#define MAX_RECONNECTIONS 50

#define MQTT_COMMAND_TOPIC "Opta1/relayOut/set/"
#define MQTT_STATE_TOPIC "Opta1/relayOut/state/"
#define MQTT_INPUT_STATE_TOPIC "Opta1/input/state/"
#define MQTT_INPUT_ACTION_TOPIC "Opta1/input/action/"
#define MQTT_AVAILABLE_TOPIC "Opta1/available"

#define LUCE_CUCINA_STRISCE A0
#define LUCE_ESTERNO_FRONTE A1
#define LUCE_INGRESSO A2
#define LUCE_BAGNO A3
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

const int channelMatrix[8] = { 0, 1, 2, 3 };  //assegnazione output a ogni ingresso. ES: IN_1 -> channelMatrix[0]
const int channelLEDMatrix[8] = { LED_D0, LED_D1, LED_D2, LED_D3 };
unsigned int channelState[8] = { 0, 0, 0, 0, 0, 0, 0, 0 };  //variabile per memorizzare gli stati degli ingressi

// Update these with values suitable for your hardware/network.
byte mac[] = { 0xDE, 0xED, 0xBA, 0xFE, 0xFE, 0xED };
IPAddress ip(10, 68, 2, 5);
IPAddress server(10, 68, 1, 10);
