#include "mqtt_publisher.h"
#include <iostream>

DataPublisher::DataPublisher(MQTTClient &client) : mqttClient(client) {}

void DataPublisher::checkAndPublish() {
    // 특정 조건 확인
    bool conditionMet = true; // 예시를 위해 true로 설정

    if (conditionMet) {
        mqttClient.publish("test/topic", "Condition met, publishing data!");
    } else {
        std::cout << "Condition not met, not publishing data.\n";
    }
}