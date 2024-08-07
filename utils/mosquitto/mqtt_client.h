#pragma once

#include <mosquitto.h>

class MQTTClient {
public:
    MQTTClient(const char *host, int port);
    ~MQTTClient();
    bool publish(const char *topic, const char *message);

private:
    mosquitto *mosq;
    bool connected;
};