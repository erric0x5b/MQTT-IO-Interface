#define MQTT_VERSION MQTT_VERSION_3_1_1

#define CYCLE_TIME 20
#define SHORT_PRESS 1000
#define LONG_PRESS 3000

#define OUT_CHANNELS 4
#define IN_CHANNELS 8       // pulsanti fisici locali (A0-A7), BTN_USER e i canali di espansione sono gestiti a parte
#define EXP_INPUT_CHANNELS 16
#define RECONNECT_INTERVAL 5000  // ms tra un tentativo di riconnessione MQTT e il successivo (nessun limite al numero di tentativi)

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

typedef struct t_instatus {
  bool lastState;
  unsigned long startTime;
} t_instatus;

long lastReconnectAttempt = 0;
long lastStart = 0;
bool cycleState = false;
bool serverConnected = false;

// Mappa ogni ingresso locale (1-4) al relativo output fisico per il comando offline (MQTT non disponibile).
// Solo i primi 4 ingressi hanno un relè locale corrispondente: gli ingressi 5-8, il pulsante utente e i canali
// di espansione vengono comunque riportati via MQTT (quando disponibile) ma non hanno un'azione locale diretta.
const int channelMatrix[OUT_CHANNELS] = { 0, 1, 2, 3 };
const int channelLEDMatrix[OUT_CHANNELS] = { LED_D0, LED_D1, LED_D2, LED_D3 };
unsigned int channelState[OUT_CHANNELS] = { 0, 0, 0, 0 };  // stato locale (fallback) dei soli 4 relè fisici

// Stato di debounce/pressione per ogni ingresso locale (1-8), il pulsante utente (indice 8) e
// i canali del modulo di espansione (16 canali).
t_instatus inState[IN_CHANNELS];
t_instatus btnUserState;
t_instatus expState[EXP_INPUT_CHANNELS];

// Update these with values suitable for your hardware/network.
byte mac[] = { 0xDE, 0xED, 0xBA, 0xFE, 0xFE, 0xED };
IPAddress ip(10, 68, 2, 5);
IPAddress server(10, 68, 1, 10);
