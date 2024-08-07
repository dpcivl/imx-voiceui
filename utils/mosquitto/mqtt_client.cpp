#include "mqtt_client.h"
#include <iostream>
#include <string.h>

MQTTClient::MQTTClient(const char *host, int port) : connected(false) {
    mosquitto_lib_init();
    mosq = mosquitto_new("publisher-client", true, NULL);
    if (!mosq) {
        std::cerr << "Failed to create mosquitto client.\n";
        return;
    }

    if (mosquitto_connect(mosq, host, port, 60) != MOSQ_ERR_SUCCESS) {
        std::cerr << "Failed to connect to Mosquitto server.\n";
        mosquitto_destroy(mosq);
        return;
    }
    connected = true;
    std::cout << "Connected to Mosquitto server.\n";
}

MQTTClient::~MQTTClient() {
    if (connected) {
        mosquitto_disconnect(mosq);
    }
    mosquitto_destroy(mosq);
    mosquitto_lib_cleanup();
}

bool MQTTClient::publish(const char *topic, const char *message) {
    if (!connected) {
        std::cerr << "Not connected to Mosquitto server.\n";
        return false;
    }
    if (mosquitto_publish(mosq, NULL, topic, strlen(message), message, 0, false) != MOSQ_ERR_SUCCESS) {
        std::cerr << "Failed to publish message.\n";
        return false;
    }
    std::cout << "Message published successfully.\n";
    return true;
}