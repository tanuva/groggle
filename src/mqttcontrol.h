#ifndef MQTTCONTROL_H
#define MQTTCONTROL_H

#include "color.h"

#include <mosquitto.h>

#include <functional>
#include <unordered_map>
#include <memory>
#include <string>

namespace groggle
{

void on_publish(struct mosquitto*, void*, int);
void on_message(struct mosquitto*, void*, const struct mosquitto_message*);

class MQTT
{
public:
    typedef std::function<void(const Color&)> ColorCb;
    typedef std::function<void(bool)> EnabledCb;

    MQTT();
    ~MQTT();
    bool init();
    void run();
    void publishInfo();
    void publishState(const bool enabled, const Color &color);
    void setColorCallback(ColorCb cb) { m_colorCallback = cb; }
    void setEnabledCallback(EnabledCb cb) { m_enabledCallback = cb; }

private:
    struct Message {
        int id = -1;
        std::string topic;
        char *payload = nullptr;

        ~Message() { if(payload) { delete[] payload; } }
        void setPayload(const std::string &s);
    };

    MQTT(const MQTT&) {}
    void publishMessage(const std::shared_ptr<Message> msg, const bool retain = false);

    ColorCb m_colorCallback;
    EnabledCb m_enabledCallback;

    const std::string TOPIC = "groggle";
    const std::string TOPIC_SET = TOPIC + "/set";

    struct mosquitto *m_client = nullptr;
    std::unordered_map<int, std::shared_ptr<Message>> m_messagesInFlight;

    // Mosquitto library callbacks
    friend void on_publish(struct mosquitto*, void*, int);
    friend void on_message(struct mosquitto*, void*, const struct mosquitto_message*);
};

}

#endif
