/*
MQTT I/O interface, STM32 and ethernet based.
Rev. Beta - 09/2026
E.T. Design

Changelog rispetto alla Rev. Alpha:
- Riconnessione MQTT senza limite di tentativi (prima si fermava dopo 50 tentativi/~4 minuti
  e richiedeva un riavvio manuale del dispositivo)
- Stato dei relè pubblicato come "retained" e ripubblicato ad ogni riconnessione, cosi Home
  Assistant si risincronizza subito dopo un proprio riavvio o un'interruzione di rete
- Tutti gli ingressi locali (A0-A7, pulsante utente, modulo di espansione) vengono ora letti
  ad OGNI ciclo da 20ms invece che uno alla volta a rotazione: prima ogni pulsante veniva
  controllato solo una volta ogni ~200ms, e una pressione rapida poteva cadere tra due
  controlli e non venire mai rilevata
- Corretta la segnalazione SHORT/LONG press degli ingressi 1 e 2, che finiva sul topic
  sbagliato (input/state invece di input/action) rispetto agli altri ingressi
- Corretto un accesso fuori dai limiti degli array channelMatrix/channelLEDMatrix quando il
  comando offline veniva invocato per canali del modulo di espansione (indici fino a 24 su
  array di 4 elementi): ora il comando locale si applica solo ai 4 ingressi con un relè
  fisico corrispondente, gli altri canali vengono comunque riportati via MQTT ma senza
  azione locale
- Corretto un errore che escludeva il primo canale (indice 0) del modulo di espansione dalla
  lettura
- "IDLE" ora viene pubblicato una sola volta al rilascio del pulsante invece che ad ogni
  ciclo (~20ms) mentre il pulsante non e' premuto: riduce di molto il traffico MQTT
*/

#include <SPI.h>
#include <Ethernet.h>
#include <PubSubClient.h>
#include "OptaBlue.h"
#include "globals.h"

using namespace Opta;

EthernetClient ethClient;
PubSubClient client(ethClient);

t_opta opta;
t_optaExp optaExp;

bool publishState(int n, const char* payload, int type);
void offlineCommand(int channel, const char* action);

void callback(char* p_topic, byte* p_payload, unsigned int p_length) {
  // Concat the payload into a string
  String payload;
  bool state = false;

  for (uint8_t i = 0; i < p_length; i++) {
    payload += (char)p_payload[i];
  }

  if (payload.equals("ON")) {
    state = true;
  } else if (payload.equals("OFF")) {
    state = false;
  }

  for (uint8_t u = 1; u <= OUT_CHANNELS; u++) {
    String topic = String(MQTT_COMMAND_TOPIC) + String(u);

    if (topic.equals(p_topic)) {
      switch (u) {
        case 1:
          opta.out_1 = state;
          publishState(1, payload.c_str(), 2);
          digitalWrite(D0, opta.out_1);
          digitalWrite(LED_D0, opta.out_1);
          break;
        case 2:
          opta.out_2 = state;
          publishState(2, payload.c_str(), 2);
          digitalWrite(D1, opta.out_2);
          digitalWrite(LED_D1, opta.out_2);
          break;
        case 3:
          opta.out_3 = state;
          publishState(3, payload.c_str(), 2);
          digitalWrite(D2, opta.out_3);
          digitalWrite(LED_D2, opta.out_3);
          break;
        case 4:
          opta.out_4 = state;
          publishState(4, payload.c_str(), 2);
          digitalWrite(D3, opta.out_4);
          digitalWrite(LED_D3, opta.out_4);
          break;
        default:
          break;
      }
    }
  }
}

boolean reconnect() {
  if (client.connect("Opta1", "mqtt_2", "arduinomqtt")) {
    Serial.println("Connection successful!");
    // Once connected, publish an announcement...
    client.publish(MQTT_AVAILABLE_TOPIC, "online", true);
    Serial.println("Available topic sent");

    // ... and resubscribe
    client.subscribe("Opta1/relayOut/set/1");
    client.subscribe("Opta1/relayOut/set/2");
    client.subscribe("Opta1/relayOut/set/3");
    client.subscribe("Opta1/relayOut/set/4");

    // Risincronizzazione: ripubblica lo stato attuale dei 4 rele' (retained) cosi che
    // Home Assistant si aggiorni subito dopo un proprio riavvio, senza dover aspettare
    // il prossimo cambio di stato reale.
    publishState(1, opta.out_1 ? "ON" : "OFF", 2);
    publishState(2, opta.out_2 ? "ON" : "OFF", 2);
    publishState(3, opta.out_3 ? "ON" : "OFF", 2);
    publishState(4, opta.out_4 ? "ON" : "OFF", 2);

    digitalWrite(LEDR, LOW);
    digitalWrite(LED_BUILTIN, HIGH);

    serverConnected = true;
  }
  return client.connected();
}

bool publishState(int n, const char* payload, int type) {
  char channel[3];
  char topic[50];

  itoa(n, channel, 10);

  switch (type) {
    case 1:
      strcpy(topic, MQTT_INPUT_STATE_TOPIC);
      strcat(topic, channel);
      client.publish(topic, payload);
      break;
    case 2:
      // Stato dei rele': retained, cosi chi si connette dopo (es. HA dopo un riavvio)
      // vede subito l'ultimo stato noto senza dover aspettare un cambiamento.
      strcpy(topic, MQTT_STATE_TOPIC);
      strcat(topic, channel);
      client.publish(topic, payload, true);
      break;
    case 3:
      strcpy(topic, MQTT_INPUT_ACTION_TOPIC);
      strcat(topic, channel);
      client.publish(topic, payload);
      break;
    default:
      return false;
  }
  return true;
}

// Comando locale di fallback quando MQTT non e' disponibile. Si applica solo ai canali
// 1-4, gli unici con un rele' fisico corrispondente (vedi channelMatrix in globals.h).
// Per tutti gli altri ingressi la funzione non ha nulla da commutare localmente e ritorna
// subito: prima, invece, veniva chiamata anche per i 16 canali del modulo di espansione
// (indici fino a 24) e accedeva fuori dai limiti di array dimensionati per 4 elementi.
void offlineCommand(int channel, const char* action) {
  channel--;  // da numero canale (1-based) a indice array (0-based)

  if (channel < 0 || channel >= OUT_CHANNELS) {
    return;
  }

  if (strcmp(action, "PRESS") == 0 && !serverConnected) {
    channelState[channel] = channelState[channel] ? 0 : 1;
  }

  publishState(channel + 1, channelState[channel] ? "ON" : "OFF", 2);

  if (!serverConnected) {
    digitalWrite(channelMatrix[channel], channelState[channel]);
    digitalWrite(channelLEDMatrix[channel], channelState[channel]);
  }
}

// Gestisce debounce, rilevamento pressione e classificazione SHORT/LONG per un singolo
// ingresso. Chiamata per tutti gli ingressi locali, il pulsante utente e i canali di
// espansione: prima questa logica era duplicata (e leggermente incoerente tra un canale
// e l'altro, vedi changelog) in un grande switch/case, ora e' un'unica funzione condivisa
// usata per tutti.
void processInput(int channel, bool pressed, t_instatus &st) {
  if (pressed) {
    if (st.lastState) {
      unsigned long held = millis() - st.startTime;
      if (held > SHORT_PRESS && held < LONG_PRESS) {
        publishState(channel, "SHORT", 3);
      } else if (held > LONG_PRESS) {
        publishState(channel, "LONG", 3);
      }
    } else {
      publishState(channel, "PRESS", 1);
      st.lastState = true;
      st.startTime = millis();
      offlineCommand(channel, "PRESS");
    }
  } else {
    // Pubblica "IDLE" una sola volta al rilascio, non ad ogni ciclo: prima veniva
    // pubblicato incondizionatamente ogni 20ms per ogni ingresso non premuto, il che
    // significava un flusso costante di messaggi MQTT ridondanti (e probabilmente
    // contribuiva a rendere meno affidabile la consegna dei messaggi "PRESS" veri).
    if (st.lastState) {
      publishState(channel, "IDLE", 1);
    }
    st.lastState = false;
    st.startTime = 0;
  }
}

void getDigitalExpansion(void) {

  DigitalMechExpansion mechExp = OptaController.getExpansion(0);
  mechExp.updateDigitalInputs();

  for (int k = 0; k < OPTA_DIGITAL_IN_NUM; k++) {
    /* this will return the pin status of the pin k */
    PinStatus v = mechExp.digitalRead(k);
    optaExp.input[k] = v;
  }
}


void setup() {
  client.setServer(server, 1883);
  client.setCallback(callback);

  Ethernet.begin(mac, ip);
  delay(1500);
  lastReconnectAttempt = 0;

  Serial.begin(9600);

  pinMode(D0, OUTPUT);
  pinMode(D1, OUTPUT);
  pinMode(D2, OUTPUT);
  pinMode(D3, OUTPUT);

  pinMode(LED_D0, OUTPUT);
  pinMode(LED_D1, OUTPUT);
  pinMode(LED_D2, OUTPUT);
  pinMode(LED_D3, OUTPUT);

  pinMode(LEDR, OUTPUT);
  pinMode(LED_BUILTIN, OUTPUT);

  pinMode(BTN_USER, INPUT);

  pinMode(A0, INPUT);
  pinMode(A1, INPUT);
  pinMode(A2, INPUT);
  pinMode(A3, INPUT);
  pinMode(A4, INPUT);
  pinMode(A5, INPUT);
  pinMode(A6, INPUT);
  pinMode(A7, INPUT);

  OptaController.begin();

  digitalWrite(LED_BUILTIN, HIGH);
  digitalWrite(LEDR, LOW);
  Serial.println("Initialization complete");
}

void loop() {
  unsigned long taskStart = millis();

  OptaController.update();

  // Connessione MQTT: riprova ogni RECONNECT_INTERVAL ms, SENZA limite al numero di
  // tentativi. In precedenza il firmware si arrendeva dopo 50 tentativi (~4 minuti) e
  // restava scollegato finche' non veniva riavviato manualmente.
  if (!client.connected()) {
    unsigned long now = millis();
    if (now - lastReconnectAttempt > RECONNECT_INTERVAL) {
      lastReconnectAttempt = now;
      serverConnected = false;

      // Attempt to reconnect
      if (reconnect()) {
        lastReconnectAttempt = 0;
      } else {
        Serial.println("Connection failed. Retrying in 5 sec");
        digitalWrite(LEDR, HIGH);
        digitalWrite(LED_BUILTIN, LOW);
      }
    }
  } else {
    // Client connected
    client.loop();
  }

  // Task da 50Hz: legge TUTTI gli ingressi ad ogni ciclo. Prima venivano letti uno alla
  // volta a rotazione (un canale diverso per ciclo, tramite un contatore), quindi ogni
  // singolo pulsante veniva effettivamente controllato solo una volta ogni ~200ms e una
  // pressione rapida poteva cadere tra due controlli e non venire mai rilevata.
  if (taskStart - lastStart > CYCLE_TIME) {
    lastStart = taskStart;
    getDigitalExpansion();

    static const int localPins[IN_CHANNELS] = { A0, A1, A2, A3, A4, A5, A6, A7 };
    for (int i = 0; i < IN_CHANNELS; i++) {
      processInput(i + 1, digitalRead(localPins[i]), inState[i]);
    }

    // Pulsante utente a bordo Opta: canale IN_CHANNELS+1 (=9), stessa numerazione di prima.
    processInput(IN_CHANNELS + 1, digitalRead(BTN_USER), btnUserState);

    // Canali del modulo di espansione: numerati a partire da IN_CHANNELS+2 (=10), quindi
    // 10-25, esattamente come nella numerazione originale. Prima il ciclo partiva da
    // x=1 invece di x=0, escludendo sempre il primo canale di espansione dalla lettura.
    for (int x = 0; x < EXP_INPUT_CHANNELS; x++) {
      processInput(IN_CHANNELS + 2 + x, optaExp.input[x], expState[x]);
    }
  }
}
