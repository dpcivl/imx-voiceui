#pragma once

#include "mqtt_client.h"

// enum 정의
enum Status {
    WW = 1,
    VC = 2
};


class DataPublisher {
public:
    DataPublisher(MQTTClient &client);
    void checkAndPublish(int comm_type, int id);

private:
    MQTTClient &mqttClient;
};