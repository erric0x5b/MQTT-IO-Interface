#ifndef DEVICE_PROFILE_H
#define DEVICE_PROFILE_H

// Build profile selection:
// - `env:opta`        => current installation with expansion module
// - `env:opta_piano2` => second installation without expansion module
#if defined(OPTA_PROFILE_PIANO2)

#define HA_DEVICE_ID "opta2"
#define HA_DEVICE_NAME "Opta2"
#define MQTT_TOPIC_PREFIX "Opta2"
#define MQTT_CLIENT_ID "Opta2"

#define HAS_EXPANDER 0

#define DEVICE_MAC_0 0xDE
#define DEVICE_MAC_1 0xED
#define DEVICE_MAC_2 0xBA
#define DEVICE_MAC_3 0xFE
#define DEVICE_MAC_4 0xFE
#define DEVICE_MAC_5 0xEC

#define DEVICE_IP_0 10
#define DEVICE_IP_1 68
#define DEVICE_IP_2 2
#define DEVICE_IP_3 6

#define DEVICE_DNS_0 10
#define DEVICE_DNS_1 68
#define DEVICE_DNS_2 2
#define DEVICE_DNS_3 1

#define DEVICE_GW_0 10
#define DEVICE_GW_1 68
#define DEVICE_GW_2 2
#define DEVICE_GW_3 1

#else

#define HA_DEVICE_ID "opta1"
#define HA_DEVICE_NAME "Opta1"
#define MQTT_TOPIC_PREFIX "Opta1"
#define MQTT_CLIENT_ID "Opta1"

#define HAS_EXPANDER 1

#define DEVICE_MAC_0 0xDE
#define DEVICE_MAC_1 0xED
#define DEVICE_MAC_2 0xBA
#define DEVICE_MAC_3 0xFE
#define DEVICE_MAC_4 0xFE
#define DEVICE_MAC_5 0xED

#define DEVICE_IP_0 10
#define DEVICE_IP_1 68
#define DEVICE_IP_2 2
#define DEVICE_IP_3 5

#define DEVICE_DNS_0 10
#define DEVICE_DNS_1 68
#define DEVICE_DNS_2 2
#define DEVICE_DNS_3 1

#define DEVICE_GW_0 10
#define DEVICE_GW_1 68
#define DEVICE_GW_2 2
#define DEVICE_GW_3 1

#endif

#define DEVICE_SUBNET_0 255
#define DEVICE_SUBNET_1 255
#define DEVICE_SUBNET_2 255
#define DEVICE_SUBNET_3 0

#define MQTT_SERVER_IP_0 10
#define MQTT_SERVER_IP_1 68
#define MQTT_SERVER_IP_2 1
#define MQTT_SERVER_IP_3 10

#if HAS_EXPANDER
#define OUT_CHANNELS 12
#define IN_CHANNELS 25
#define EXP_INPUT_CHANNELS 16
#define EXP_RELAY_BASE 5
#define EXP_RELAY_COUNT 8
#else
#define OUT_CHANNELS 4
#define IN_CHANNELS 9
#define EXP_INPUT_CHANNELS 0
#define EXP_RELAY_BASE 5
#define EXP_RELAY_COUNT 0
#endif

#define MQTT_COMMAND_TOPIC MQTT_TOPIC_PREFIX "/relayOut/set/"
#define MQTT_STATE_TOPIC MQTT_TOPIC_PREFIX "/relayOut/state/"
#define MQTT_INPUT_STATE_TOPIC MQTT_TOPIC_PREFIX "/input/state/"
#define MQTT_INPUT_ACTION_TOPIC MQTT_TOPIC_PREFIX "/input/action/"
#define MQTT_AVAILABLE_TOPIC MQTT_TOPIC_PREFIX "/available"

#endif
