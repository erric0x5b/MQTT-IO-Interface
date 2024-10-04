/*
MQTT I/O interface, STM32 and ethernet based.
Rev. Alpha - 10/2024
E.T. Design
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
t_instatus in1, in2, in3, in4, in5, in6, in7, in8;
t_inputStatus plc, exp1;

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

  for (uint8_t u = 1; u <= OUT_CHANNELS; u++) {  // Correzione: partire da 1
    String topic = String(MQTT_COMMAND_TOPIC) + String(u);

    if (topic.equals(p_topic)) {
      switch (u) {
        case 1:
          opta.out_1 = state;
          publishState(1, payload, 2);
          digitalWrite(D0, opta.out_1);
          digitalWrite(LED_D0, opta.out_1);
          break;
        case 2:
          opta.out_2 = state;
          publishState(2, payload, 2);
          digitalWrite(D1, opta.out_2);
          digitalWrite(LED_D1, opta.out_2);
          break;
        case 3:
          opta.out_3 = state;
          publishState(3, payload, 2);
          digitalWrite(D2, opta.out_3);
          digitalWrite(LED_D2, opta.out_3);
          break;
        case 4:
          opta.out_4 = state;
          publishState(4, payload, 2);
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

    digitalWrite(LEDR, LOW);
    digitalWrite(LED_BUILTIN, HIGH);

    serverConnected = true;
  }
  return client.connected();
}

bool publishState(int n, String payload, int type) {
  char channel[2];  // Buffer per il numero come stringa
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
      strcpy(topic, MQTT_STATE_TOPIC);
      strcat(topic, channel);
      client.publish(topic, payload.c_str());
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
  bool state = false;
  channel--;

  if (strcmp(payload, "PRESS") == 0 && !serverConnected) {
    state = true;
    if (channelState[channel] == 1) {
      channelState[channel] = 0;
    } else {
      channelState[channel] = 1;
    }
  } else {
    state = false;
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
  static int counter = 1;
  static int reconnectionCounter = 0;

  OptaController.update();

  // MQTT connection
  if (!client.connected() && reconnectionCounter < MAX_RECONNECTIONS) {
    unsigned long now = millis();
    if (now - lastReconnectAttempt > 5000) {
      reconnectionCounter++;
      lastReconnectAttempt = now;
      serverConnected = false;

      // Attempt to reconnect
      if (reconnect()) {
        lastReconnectAttempt = 0;
      } else {
        Serial.println("Connection failed. Attempting reconnection in 5 sec");
        digitalWrite(LEDR, HIGH);
        digitalWrite(LED_BUILTIN, LOW);
      }
    }
  } else {
    reconnectionCounter = 0;
    // Client connected
    client.loop();
  }

  // 50Hz task
  if (taskStart - lastStart > CYCLE_TIME) {
    lastStart = taskStart;
    getDigitalExpansion();

    switch (counter) {
      case 1:

        if (digitalRead(A0)) {
          if (in1.lastState) {

            if (millis() - in1.startTime > SHORT_PRESS && millis() - in1.startTime < LONG_PRESS) {
              publishState(counter, "SHORT", 1);
            } else if (millis() - in1.startTime > LONG_PRESS) {
              publishState(counter, "LONG", 1);
            }
          } else if (millis() - in1.startTime > SHORT_PRESS) {
            publishState(counter, "PRESS", 1);
            in1.lastState = true;
            in1.startTime = millis();
            offlineCommand(counter, "PRESS");
          }
        } else {
          in1.lastState = false;
          in1.startTime = 0;
          publishState(counter, "IDLE", 1);
        }
        break;
      case 2:
        if (digitalRead(A1)) {
          if (in2.lastState) {
            if (millis() - in2.startTime > SHORT_PRESS && millis() - in2.startTime < LONG_PRESS) {
              publishState(counter, "SHORT", 1);
            } else if (millis() - in2.startTime > LONG_PRESS) {
              publishState(counter, "LONG", 1);
            }
          } else {
            publishState(counter, "PRESS", 1);
            in2.lastState = true;
            in2.startTime = millis();
            offlineCommand(counter, "PRESS");
          }
        } else {
          in2.lastState = false;
          in2.startTime = 0;
          publishState(counter, "IDLE", 1);
        }
        break;
      case 3:
        if (digitalRead(A2)) {
          if (in3.lastState) {
            if (millis() - in3.startTime > SHORT_PRESS && millis() - in3.startTime < LONG_PRESS) {
              publishState(counter, "SHORT", 3);
            } else if (millis() - in3.startTime > LONG_PRESS) {
              publishState(counter, "LONG", 3);
            }
          } else {
            publishState(counter, "PRESS", 1);
            in3.lastState = true;
            in3.startTime = millis();
            offlineCommand(counter, "PRESS");
          }
        } else {
          in3.lastState = false;
          in3.startTime = 0;
          publishState(counter, "IDLE", 1);
        }
        break;
      case 4:
        if (digitalRead(A3)) {
          if (in4.lastState) {
            if (millis() - in4.startTime > SHORT_PRESS && millis() - in4.startTime < LONG_PRESS) {
              publishState(counter, "SHORT", 3);
            } else if (millis() - in4.startTime > LONG_PRESS) {
              publishState(counter, "LONG", 3);
            }
          } else {
            publishState(counter, "PRESS", 1);
            in4.lastState = true;
            in4.startTime = millis();
            offlineCommand(counter, "PRESS");
          }
        } else {
          in4.lastState = false;
          in4.startTime = 0;
          publishState(counter, "IDLE", 1);
        }
        break;
      case 5:
        if (digitalRead(A4)) {
          if (in5.lastState) {
            if (millis() - in5.startTime > SHORT_PRESS && millis() - in5.startTime < LONG_PRESS) {
              publishState(counter, "SHORT", 3);
            } else if (millis() - in5.startTime > LONG_PRESS) {
              publishState(counter, "LONG", 3);
            }
          } else {
            publishState(counter, "PRESS", 1);
            in5.lastState = true;
            in5.startTime = millis();
            offlineCommand(counter, "PRESS");
          }
        } else {
          in5.lastState = false;
          in5.startTime = 0;
          publishState(counter, "IDLE", 1);
        }
        break;
      case 6:
        if (digitalRead(A5)) {
          if (in6.lastState) {
            if (millis() - in6.startTime > SHORT_PRESS && millis() - in6.startTime < LONG_PRESS) {
              publishState(counter, "SHORT", 3);
            } else if (millis() - in6.startTime > LONG_PRESS) {
              publishState(counter, "LONG", 3);
            }
          } else {
            publishState(counter, "PRESS", 1);
            in6.lastState = true;
            in6.startTime = millis();
            offlineCommand(counter, "PRESS");
          }
        } else {
          in6.lastState = false;
          in6.startTime = 0;
          publishState(counter, "IDLE", 1);
        }
        break;
      case 7:
        if (digitalRead(A6)) {
          if (in7.lastState) {
            if (millis() - in7.startTime > SHORT_PRESS && millis() - in7.startTime < LONG_PRESS) {
              publishState(counter, "SHORT", 3);
            } else if (millis() - in7.startTime > LONG_PRESS) {
              publishState(counter, "LONG", 3);
            }
          } else {
            publishState(counter, "PRESS", 1);
            in7.lastState = true;
            in7.startTime = millis();
            offlineCommand(counter, "PRESS");
          }
        } else {
          in7.lastState = false;
          in7.startTime = 0;
          publishState(counter, "IDLE", 1);
        }
        break;
      case 8:
        if (digitalRead(A7)) {
          if (in8.lastState) {
            if (millis() - in8.startTime > SHORT_PRESS && millis() - in8.startTime < LONG_PRESS) {
              publishState(counter, "SHORT", 3);
            } else if (millis() - in8.startTime > LONG_PRESS) {
              publishState(counter, "LONG", 3);
            }
          } else {
            publishState(counter, "PRESS", 1);
            in8.lastState = true;
            in8.startTime = millis();
            offlineCommand(counter, "PRESS");
          }
        } else {
          in8.lastState = false;
          in8.startTime = 0;
          publishState(counter, "IDLE", 1);
        }
        break;
      case 9:
        if (digitalRead(BTN_USER)) {
          publishState(counter, "PRESS", 1);
        }
        break;
      case 10:
        for (int x = 1; x < EXP_INPUT_CHANNELS; x++) {
          unsigned int expChannel = counter + x;
          if (optaExp.input[x]) {
            if (exp1.channelLast[x]) {
              if (millis() - exp1.channelStart[x] > SHORT_PRESS && millis() - exp1.channelStart[x] < LONG_PRESS) {
                publishState(expChannel, "SHORT", 3);
              } else if (millis() - exp1.channelStart[x] > LONG_PRESS) {
                publishState(expChannel, "LONG", 3);
              }
            } else {
              publishState(expChannel, "PRESS", 1);
              exp1.channelLast[x] = true;
              exp1.channelStart[x] = millis();
              offlineCommand(expChannel, "PRESS");
            }
          } else {
            exp1.channelLast[x] = false;
            exp1.channelStart[x] = 0;
            publishState(expChannel, "IDLE", 1);
          }
        }
        break;
    }

    if (counter++ > IN_CHANNELS) {
      counter = 1;
    }
  }
}
