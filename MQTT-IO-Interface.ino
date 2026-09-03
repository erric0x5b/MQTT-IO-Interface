/*
MQTT I/O interface, STM32 and ethernet based.
Rev. Alpha - 10/2024
E.T. Design

Changelog 09/2026:
- Rimosso il limite di 50 tentativi di riconnessione MQTT (MAX_RECONNECTIONS):
  dopo un'interruzione prolungata (es. update di HA/Mosquitto) il firmware
  smetteva per sempre di riprovare e serviva un riavvio manuale
- Stato dei rele' (locali + espansione) pubblicato come "retained" e
  ripubblicato ad ogni riconnessione riuscita, cosi' Home Assistant si
  risincronizza subito dopo un proprio riavvio senza aspettare un cambio
  di stato reale
- Tutti gli ingressi onboard (A0-A7) e il pulsante utente vengono ora letti
  ad OGNI ciclo da 20ms invece che uno alla volta a rotazione: su Opta1
  (con espansione, IN_CHANNELS=25) ogni singolo pulsante veniva controllato
  solo una volta ogni ~500ms, e una pressione rapida poteva non venire mai
  rilevata. Gli ingressi del modulo di espansione erano gia' letti tutti
  insieme ad ogni ciclo, nessuna modifica necessaria li'.
*/

#include <SPI.h>
#include <Ethernet.h>
#include <PubSubClient.h>
#include <Arduino_Portenta_OTA.h>
#include "OptaBlue.h"
#include "globals.h"

// --- Forward declarations ---
bool publishState(int n, String payload, int type);
static void otaPublishStatus(const String& s);
static void performOtaFromUrl(const String& url);


// ---- Home Assistant MQTT Discovery (prefix: homeassistant) ----
#define HA_DISCOVERY_PREFIX "homeassistant"
#define HA_MANUFACTURER    "Arduino"
#define HA_MODEL           "Arduino Opta"
#define HA_SW_VERSION      "MQTT-IO-Interface"

// ---- OTA (local HTTP .ota file) ----
#define OTA_CMD_TOPIC     "opta/" HA_DEVICE_ID "/ota"
#define OTA_STATUS_TOPIC  "opta/" HA_DEVICE_ID "/ota/status"
#define OTA_DEFAULT_URL   "http://10.68.1.10:8123/local/update.ota"
#define OTA_DEFAULT_IS_HTTPS false


using namespace Opta;

EthernetClient ethClient;
PubSubClient client(ethClient);

Arduino_Portenta_OTA_QSPI ota(QSPI_FLASH_FATFS_MBR, 2);

t_opta opta;
t_optaExp optaExp;
t_instatus in1, in2, in3, in4, in5, in6, in7, in8, in9;
t_inputStatus plc, exp1;


static bool expanderEnabled() {
#if HAS_EXPANDER
  return expansionAvailable;
#else
  return false;
#endif
}

static void refreshExpanderAvailability() {
#if HAS_EXPANDER
  OptaController.checkForExpansions();
  expansionAvailable = OptaController.getExpansionNum() > 0;
#else
  expansionAvailable = false;
#endif
}


static void otaPublishStatus(const String& msg) {
  client.publish(OTA_STATUS_TOPIC, msg.c_str(), true);
}

static void printIpAddress(const char* label, const IPAddress& addr) {
  Serial.print(label);
  Serial.print(": ");
  Serial.println(addr);
}

static void performOtaFromUrl(const String& url) {
  otaPublishStatus("START");
  Serial.print("OTA URL: "); Serial.println(url);

  if (!ota.isOtaCapable()) {
    otaPublishStatus("ERROR:BOOTLOADER");
    Serial.println("OTA not capable: update bootloader (STM32H747_System examples).");
    return;
  }

  Arduino_Portenta_OTA::Error ota_err = Arduino_Portenta_OTA::Error::None;

  otaPublishStatus("INIT");
  if ((ota_err = ota.begin()) != Arduino_Portenta_OTA::Error::None) {
    otaPublishStatus(String("ERROR:BEGIN:") + String((int)ota_err));
    return;
  }

  otaPublishStatus("DOWNLOAD");
  int const ota_download = ota.download(url.c_str(), OTA_DEFAULT_IS_HTTPS);
  if (ota_download <= 0) {
    otaPublishStatus(String("ERROR:DOWNLOAD:") + String(ota_download));
    return;
  }

  otaPublishStatus("DECOMPRESS");
  int const ota_decompress = ota.decompress();
  if (ota_decompress < 0) {
    otaPublishStatus(String("ERROR:DECOMPRESS:") + String(ota_decompress));
    return;
  }

  otaPublishStatus("UPDATE");
  if ((ota_err = ota.update()) != Arduino_Portenta_OTA::Error::None) {
    otaPublishStatus(String("ERROR:UPDATE:") + String((int)ota_err));
    return;
  }

  otaPublishStatus("RESETTING");
  delay(300);
  ota.reset();
}

void callback(char* p_topic, byte* p_payload, unsigned int p_length) {
  // Payload -> String
  String payload;
  payload.reserve(p_length + 1);
  for (unsigned int i = 0; i < p_length; i++) {
    payload += (char)p_payload[i];
  }
  payload.trim();

  bool state = payload.equals("ON");

  // OTA trigger:
  // - publish an URL (http://.../UPDATE.OTA) to OTA_CMD_TOPIC
  // - or publish "CHECK" to use OTA_DEFAULT_URL
  if (String(p_topic).equals(OTA_CMD_TOPIC)) {
    if (payload.equalsIgnoreCase("CHECK")) {
      performOtaFromUrl(String(OTA_DEFAULT_URL));
    } else if (payload.startsWith("http://") || payload.startsWith("https://")) {
      performOtaFromUrl(payload);
    } else {
      otaPublishStatus("ERROR:BAD_PAYLOAD");
    }
    return;
  }

  // Relay command: Opta1/relayOut/set/<ch>
  const String base = String(MQTT_COMMAND_TOPIC);
  String topicStr = String(p_topic);
  if (!topicStr.startsWith(base)) return;

  int ch = topicStr.substring(base.length()).toInt(); // 0 if not a number
  if (ch < 1 || ch > OUT_CHANNELS) return;

  // Onboard relays 1..4
  if (ch >= 1 && ch <= 4) {
    switch (ch) {
      case 1:
        opta.out_1 = state;
        digitalWrite(D0, opta.out_1);
        digitalWrite(LED_D0, opta.out_1);
        break;
      case 2:
        opta.out_2 = state;
        digitalWrite(D1, opta.out_2);
        digitalWrite(LED_D1, opta.out_2);
        break;
      case 3:
        opta.out_3 = state;
        digitalWrite(D2, opta.out_3);
        digitalWrite(LED_D2, opta.out_3);
        break;
      case 4:
        opta.out_4 = state;
        digitalWrite(D3, opta.out_4);
        digitalWrite(LED_D3, opta.out_4);
        break;
    }
    publishState(ch, state ? "ON" : "OFF", 2);
    return;
  }

  // Expansion relays AFX00005: channels 5..12 -> idx 0..7
#if HAS_EXPANDER
  uint8_t expIdx = (uint8_t)(ch - EXP_RELAY_BASE);
  if (expanderEnabled() && expIdx < EXP_RELAY_COUNT) {
    DigitalMechExpansion mechExp = OptaController.getExpansion(0);
    mechExp.digitalWrite(expIdx, state ? HIGH : LOW);
    mechExp.updateDigitalOutputs();
    optaExp.output[expIdx] = state;
    publishState(ch, state ? "ON" : "OFF", 2);
  }
#endif
}

// Publish a retained discovery message (Home Assistant will create/update entities)
static bool haPublishDiscovery(const char* topic, const String& payload) {
  // HA discovery messages should be retained so HA can see them after restart.
  // NOTE: PubSubClient needs a larger buffer (setBufferSize) for these JSON payloads.
  bool ok = client.publish(topic, payload.c_str(), true /*retain*/);
  if (!ok) {
    Serial.print("[HA DISCOVERY] publish FAILED topic=");
    Serial.println(topic);
  } else {
    Serial.print("[HA DISCOVERY] published topic=");
    Serial.println(topic);
  }
  return ok;
}

// Overload helper: allow passing a String topic (keeps call sites simple).
static bool haPublishDiscovery(const String& topic, const String& payload) {
  return haPublishDiscovery(topic.c_str(), payload);
}

static bool haDiscoveryPublished = false;
static unsigned long lastDiscoveryMs = 0;
#define HA_DISCOVERY_REANNOUNCE_MS (6UL * 60UL * 60UL * 1000UL)



static void publishHADiscovery() {
  // NOTE: discovery payloads can be large; ensure client.setBufferSize(1024) in setup().

  // Device block used by all entities
  const String dev =
    String("\"device\":{") +
    "\"identifiers\":[\"" + HA_DEVICE_ID + "\"]," +
    "\"name\":\"" + HA_DEVICE_NAME + "\"," +
    "\"manufacturer\":\"" + HA_MANUFACTURER + "\"," +
    "\"model\":\"" + HA_MODEL + "\"," +
    "\"sw_version\":\"" + HA_SW_VERSION + "\"" +
    "}";

  // ---- Relay outputs as MQTT switches ----
  for (int i = 1; i <= OUT_CHANNELS; i++) {
    char dt[128];
    snprintf(dt, sizeof(dt), HA_DISCOVERY_PREFIX "/switch/%s_relay%d/config", HA_DEVICE_ID, i);

    String payload =
      String("{") +
      "\"name\":\"Relay " + String(i) + "\"," +
      "\"uniq_id\":\"" + String(HA_DEVICE_ID) + "_relay" + String(i) + "\"," +
      "\"cmd_t\":\"" + String(MQTT_COMMAND_TOPIC) + String(i) + "\"," +
      "\"stat_t\":\"" + String(MQTT_STATE_TOPIC) + String(i) + "\"," +
      "\"pl_on\":\"ON\"," +
      "\"pl_off\":\"OFF\"," +
      "\"avty_t\":\"" + String(MQTT_AVAILABLE_TOPIC) + "\"," +
      "\"pl_avail\":\"online\"," +
      "\"pl_not_avail\":\"offline\"," +
      dev +
      "}";

    haPublishDiscovery(dt, payload);
  }

  // ---- Input state as MQTT binary_sensors (PRESS/IDLE) ----
  // Names for the first 8 onboard inputs (optional – the rest will be generic)
  const char* inNames[] = {
    "",
    "Cucina Strisce",
    "Esterno Fronte",
    "Ingresso",
    "Faretto_L8",
    "Pranzo Faretti",
    "Pranzo Strisce",
    "Cucina Pensili",
    "Scala",
    "Input 9",
    "Input 10",
    "Input 11",
    "Input 12",
    "Input 13",
    "Input 14",
    "Input 15",
    "Input 16",
    "Input 17",
    "Input 18",
    "Input 19",
    "Input 20",
    "Input 21",
    "Input 22",
    "Input 23",
    "Input 24",
    "Input 25"
  };
  const int inNameCount = sizeof(inNames) / sizeof(inNames[0]);

  for (int i = 1; i <= IN_CHANNELS; i++) {
    char dt[128];
    snprintf(dt, sizeof(dt), HA_DISCOVERY_PREFIX "/binary_sensor/%s_in%d/config", HA_DEVICE_ID, i);

    String inputName = (i < inNameCount) ? String(inNames[i]) : String("Input ") + String(i);

    String payload =
      String("{") +
      "\"name\":\"" + inputName + "\"," +
      "\"uniq_id\":\"" + String(HA_DEVICE_ID) + "_in" + String(i) + "\"," +
      "\"stat_t\":\"" + String(MQTT_INPUT_STATE_TOPIC) + String(i) + "\"," +
      "\"pl_on\":\"PRESS\"," +
      "\"pl_off\":\"IDLE\"," +
      "\"avty_t\":\"" + String(MQTT_AVAILABLE_TOPIC) + "\"," +
      "\"pl_avail\":\"online\"," +
      "\"pl_not_avail\":\"offline\"," +
      dev +
      "}";

    haPublishDiscovery(dt, payload);
  }

  // ---- Input actions as MQTT Device Triggers (SINGLE/LONG/VERY_LONG) ----
  // This makes them appear under the device’s Automations as selectable triggers.
  const struct { const char* suf; const char* payload; } trig[3] = {
    {"single", "SINGLE"},
    {"long", "LONG"},
    {"very_long", "VERY_LONG"}
  };

  for (int i = 1; i <= IN_CHANNELS; i++) {
    for (int t = 0; t < 3; t++) {
      char dt[160];
      snprintf(dt, sizeof(dt), HA_DISCOVERY_PREFIX "/device_automation/%s/in%d_%s/config", HA_DEVICE_ID, i, trig[t].suf);

      String payload =
        String("{") +
        "\"automation_type\":\"trigger\"," +
        "\"platform\":\"device_automation\"," +
        "\"type\":\"action\"," +
        "\"subtype\":\"in" + String(i) + "_" + String(trig[t].suf) + "\"," +
        "\"topic\":\"" + String(MQTT_INPUT_ACTION_TOPIC) + String(i) + "\"," +
        "\"payload\":\"" + String(trig[t].payload) + "\"," +
        dev +
        "}";

      haPublishDiscovery(dt, payload);
    }
  }

  // ---- OTA status sensor ----
  {
    String dt = String(HA_DISCOVERY_PREFIX) + "/sensor/" + String(HA_DEVICE_ID) + "/ota_status/config";
    String payload =
      String("{") +
      "\"name\":\"OTA Status\"," +
      "\"uniq_id\":\"" + String(HA_DEVICE_ID) + "_ota_status\"," +
      "\"stat_t\":\"" + String(OTA_STATUS_TOPIC) + "\"," +
      "\"avty_t\":\"" + String(MQTT_AVAILABLE_TOPIC) + "\"," +
      "\"pl_avail\":\"online\"," +
      "\"pl_not_avail\":\"offline\"," +
      dev +
      "}";
    haPublishDiscovery(dt, payload);
  }

  // ---- OTA trigger button (payload: CHECK) ----
  {
    String dt = String(HA_DISCOVERY_PREFIX) + "/button/" + String(HA_DEVICE_ID) + "/ota_check/config";
    String payload =
      String("{") +
      "\"name\":\"OTA Check/Update\"," +
      "\"uniq_id\":\"" + String(HA_DEVICE_ID) + "_ota_check\"," +
      "\"cmd_t\":\"" + String(OTA_CMD_TOPIC) + "\"," +
      "\"pl_prs\":\"CHECK\"," +
      "\"avty_t\":\"" + String(MQTT_AVAILABLE_TOPIC) + "\"," +
      "\"pl_avail\":\"online\"," +
      "\"pl_not_avail\":\"offline\"," +
      dev +
      "}";
    haPublishDiscovery(dt, payload);
  }


  lastDiscoveryMs = millis();
}


bool reconnect() {
  if (client.connect(HA_DEVICE_ID, HA_DEVICE_NAME, "arduinomqtt", MQTT_AVAILABLE_TOPIC, 0, true, "offline")) {
    
      // Publish (retained) Home Assistant discovery config on every (re)connect.
      publishHADiscovery();
      haDiscoveryPublished = true;
      lastDiscoveryMs = millis();
    Serial.println("Connection successful!");
    // Once connected, publish an announcement...
    client.publish(MQTT_AVAILABLE_TOPIC, "online", true);
    Serial.println("Available topic sent");

    // Home Assistant discovery (retain = true)
    publishHADiscovery();


    // ... and resubscribe
    for (int ch = 1; ch <= OUT_CHANNELS; ch++) {
      String t = String(MQTT_COMMAND_TOPIC) + String(ch);
      client.subscribe(t.c_str());
    }
    client.subscribe(OTA_CMD_TOPIC);

    // Risincronizzazione: ripubblica lo stato attuale di tutti i rele' (retained)
    // cosi' che Home Assistant si aggiorni subito dopo un proprio riavvio, senza
    // dover aspettare il prossimo cambio di stato reale.
    publishState(1, opta.out_1 ? "ON" : "OFF", 2);
    publishState(2, opta.out_2 ? "ON" : "OFF", 2);
    publishState(3, opta.out_3 ? "ON" : "OFF", 2);
    publishState(4, opta.out_4 ? "ON" : "OFF", 2);
#if HAS_EXPANDER
    if (expanderEnabled()) {
      for (int i = 0; i < EXP_RELAY_COUNT; i++) {
        publishState(EXP_RELAY_BASE + i, optaExp.output[i] ? "ON" : "OFF", 2);
      }
    }
#endif

    digitalWrite(LEDR, LOW);
    digitalWrite(LED_BUILTIN, HIGH);

    serverConnected = true;
  } else {
    Serial.print("MQTT connect failed, state=");
    Serial.println(client.state());
  }
  return client.connected();
}

bool publishState(int n, String payload, int type) {
  char channel[4];  // Buffer per il numero come stringa
  char topic[50];   // Buffer per il topic finale

  itoa(n, channel, 10);

  // Costruisci il topic
  switch (type) {
    case 1:
      strcpy(topic, MQTT_INPUT_STATE_TOPIC);
      strcat(topic, channel);
      client.publish(topic, payload.c_str());
      break;
    case 2:
      // Stato dei rele': retained, cosi' chi si connette dopo (es. HA dopo un
      // riavvio) vede subito l'ultimo stato noto senza dover aspettare un
      // cambiamento reale.
      strcpy(topic, MQTT_STATE_TOPIC);
      strcat(topic, channel);
      client.publish(topic, payload.c_str(), true);
      break;
    case 3:
      strcpy(topic, MQTT_INPUT_ACTION_TOPIC);
      strcat(topic, channel);
      client.publish(topic, payload.c_str());
      break;
    default:
      return false;
  }
  return true;
}

void offlineCommand(int channel, char* payload) {
  channel--;

  // Offline mode controls ONLY Opta onboard outputs.
  if (channel < 0 || channel >= OFFLINE_OUT_CHANNELS) {
    return;
  }

  if (strcmp(payload, "SINGLE") == 0 && !serverConnected) {
    if (channelState[channel] == 1) {
      channelState[channel] = 0;
    } else {
      channelState[channel] = 1;
    }
  }

  if (channelState[channel] == 1) {
    strcpy(payload, "ON");
    publishState(channel + 1, "ON", 2);
  } else {
    strcpy(payload, "OFF");
    publishState(channel + 1, "OFF", 2);
  }

  if (serverConnected == false) {
    digitalWrite(channelMatrix[channel], channelState[channel]);
    digitalWrite(channelLEDMatrix[channel], channelState[channel]);
  }
}

void getDigitalExpansion(void) {
  if (!expanderEnabled()) {
    return;
  }

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
  client.setBufferSize(1024);
  client.setCallback(callback);

  Serial.begin(9600);
  delay(500);

  int ethOk = Ethernet.begin(mac, ip, dns, gateway, subnet);
  delay(1500);
  lastReconnectAttempt = 0;
  Serial.print("Ethernet begin: ");
  Serial.println(ethOk ? "OK" : "FAIL");
  Serial.print("Ethernet link: ");
  Serial.println(Ethernet.linkStatus() == LinkON ? "ON" : "OFF");
  printIpAddress("Local IP", ip);
  printIpAddress("Gateway", gateway);
  printIpAddress("DNS", dns);
  printIpAddress("MQTT server", server);

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
  refreshExpanderAvailability();

  digitalWrite(LED_BUILTIN, HIGH);
  digitalWrite(LEDR, LOW);
  Serial.print("Expander enabled: ");
  Serial.println(expanderEnabled() ? "YES" : "NO");
  Serial.println("Initialization complete");
}

void loop() {

  // Periodically re-announce discovery (in case retained configs were cleared).
  if (client.connected()) {
    unsigned long now = millis();
    if (!haDiscoveryPublished || (now - lastDiscoveryMs) > HA_DISCOVERY_REANNOUNCE_MS) {
      publishHADiscovery();
      haDiscoveryPublished = true;
      lastDiscoveryMs = now;
    }
  }


  unsigned long taskStart = millis();

  OptaController.update();

  // MQTT connection: riprova ogni 5s SENZA limite al numero di tentativi.
  // Prima, dopo MAX_RECONNECTIONS (50) tentativi falliti (~4 minuti), il firmware
  // smetteva per sempre di riprovare e restava scollegato finche' non veniva
  // riavviato manualmente (es. dopo un aggiornamento di Home Assistant/Mosquitto
  // che richiede piu' di qualche minuto).
  if (!client.connected()) {
    unsigned long now = millis();
    if (now - lastReconnectAttempt > 5000) {
      lastReconnectAttempt = now;
      serverConnected = false;

      // Attempt to reconnect
      if (reconnect()) {
        lastReconnectAttempt = 0;
      } else {
        Serial.println("Connection failed. Attempting reconnection in 5 sec");
        printIpAddress("MQTT server", server);
        digitalWrite(LEDR, HIGH);
        digitalWrite(LED_BUILTIN, LOW);
      }
    }
  } else {
    // Client connected
    client.loop();
  }

  // 50Hz task
  if (taskStart - lastStart > CYCLE_TIME) {
    lastStart = taskStart;
    getDigitalExpansion();

    // --- Input handling: publish state (PRESS/IDLE) + action (SINGLE/LONG/VERY_LONG) ---
    auto publishButtonAction = [&](int ch, unsigned long durMs) {
      if (durMs < SHORT_PRESS) {
        publishState(ch, "SINGLE", 3);
        offlineCommand(ch, (char*)"SINGLE");   // offline toggle on SINGLE only
      } else if (durMs < LONG_PRESS) {
        publishState(ch, "LONG", 3);
      } else {
        publishState(ch, "VERY_LONG", 3);
      }
    };

    auto handleLocalInput = [&](int ch, bool pressed, t_instatus &st) {
      if (pressed) {
        if (!st.lastState) {
          st.lastState = true;
          st.startTime = millis();
          publishState(ch, "PRESS", 1);
        }
      } else {
        if (st.lastState) {
          unsigned long durMs = millis() - st.startTime;
          st.lastState = false;
          st.startTime = 0;
          publishState(ch, "IDLE", 1);
          publishButtonAction(ch, durMs);
        }
      }
    };

    // Onboard inputs 1..8 -> A0..A7. Letti TUTTI ad ogni ciclo da 20ms invece che
    // uno alla volta a rotazione: prima, con un contatore che passava per tutti i
    // canali (fino a 25 su Opta1 con espansione), ogni singolo pulsante veniva
    // effettivamente controllato solo una volta ogni ~500ms, e una pressione
    // rapida poteva cadere tra due controlli e non venire mai rilevata.
    static const int localPins[8] = { A0, A1, A2, A3, A4, A5, A6, A7 };
    static t_instatus* localStates[8] = { &in1, &in2, &in3, &in4, &in5, &in6, &in7, &in8 };

    for (int i = 0; i < 8; i++) {
      handleLocalInput(i + 1, digitalRead(localPins[i]), *localStates[i]);
    }

    // User button (canale 9)
    handleLocalInput(9, digitalRead(BTN_USER), in9);

    // Expansion inputs 10..25: gia' processati tutti insieme ad ogni ciclo prima
    // (non erano soggetti alla rotazione), nessuna modifica di logica qui.
    if (expanderEnabled()) {
      for (int x = 0; x < EXP_INPUT_CHANNELS; x++) {
        int ch = 10 + x;
        bool pressed = optaExp.input[x];

        if (pressed) {
          if (!exp1.channelLast[x]) {
            exp1.channelLast[x] = true;
            exp1.channelStart[x] = millis();
            publishState(ch, "PRESS", 1);
          }
        } else {
          if (exp1.channelLast[x]) {
            unsigned long durMs = millis() - exp1.channelStart[x];
            exp1.channelLast[x] = false;
            exp1.channelStart[x] = 0;
            publishState(ch, "IDLE", 1);
            publishButtonAction(ch, durMs);
          }
        }
      }
    }
  }
}

bool setOutput(uint8_t ch, bool on) {
  if (ch < 1 || ch > OUT_CHANNELS) return false;

  // ===== Onboard relays (1..4) =====
  if (ch <= 4) {
    int pin = (ch == 1) ? D0 :
              (ch == 2) ? D1 :
              (ch == 3) ? D2 : D3;

    int led = (ch == 1) ? LED_D0 :
              (ch == 2) ? LED_D1 :
              (ch == 3) ? LED_D2 : LED_D3;

    digitalWrite(pin, on);
    digitalWrite(led, on);
    return true;
  }

  // ===== Expander relays (5..12 → idx 0..7) =====
#if HAS_EXPANDER
  if (!expanderEnabled()) return false;
  uint8_t idx = ch - EXP_RELAY_BASE;  // 0..7
  if (idx >= EXP_RELAY_COUNT) return false;

  DigitalMechExpansion mechExp = OptaController.getExpansion(0);
  mechExp.digitalWrite(idx, on ? HIGH : LOW);
#else
  (void)on;
  return false;
#endif

  return true;
}

bool getOutput(uint8_t ch) {
  if (ch < 1 || ch > OUT_CHANNELS) return false;

  // ===== Onboard relays =====
  if (ch <= 4) {
    int pin = (ch == 1) ? D0 :
              (ch == 2) ? D1 :
              (ch == 3) ? D2 : D3;
    return digitalRead(pin);
  }

  // ===== Expander relays =====
#if HAS_EXPANDER
  if (!expanderEnabled()) return false;
  uint8_t idx = ch - EXP_RELAY_BASE;  // 0..7
  if (idx >= EXP_RELAY_COUNT) return false;

  DigitalMechExpansion mechExp = OptaController.getExpansion(0);
  return mechExp.digitalRead(idx);
#else
  return false;
#endif
}
