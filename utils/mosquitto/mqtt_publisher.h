#pragma once

#include "mqtt_client.h"

class DataPublisher {
public:
    DataPublisher(MQTTClient &client);
    void checkAndPublish();

private:
    MQTTClient &mqttClient;
};