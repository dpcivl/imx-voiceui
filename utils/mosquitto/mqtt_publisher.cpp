#include "mqtt_publisher.h"
#include <iostream>

DataPublisher::DataPublisher(MQTTClient &client) : mqttClient(client) {}

void DataPublisher::checkAndPublish(int comm_type, int id) {
    switch (comm_type) {
        case WW:        // WW for WakeWord
            switch (id) {
                case 1:
                    mqttClient.publish("test/topic", "Hi, user.");
                    break;
                case 2:
                case 3:
                    mqttClient.publish("test/emergency", "Emergency!!!!");
                    break;
                default:
                    std::cout << "Unknown ID for WW.\n";
                    break;
            }
            break;
        
        case VC:        // VC for VoiceCommand
            mqttClient.publish("test/topic", "Condition met, publishing data!");
            break;

        default:
            std::cout << "Condition not met, not publishing data.\n";
            break;
    }
}