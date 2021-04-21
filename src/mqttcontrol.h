#ifndef MQTTCONTROL_H
#define MQTTCONTROL_H

#include "color.h"

#include <mosquitto.h>

#include <functional>
#include <unordered_map>
#include <memory>
#include <mutex>
#include <string>

namespace groggle
{

void on_publish(struct mosquitto*, void*, int);
void on_message(struct mosquitto*, void*, const struct mosquitto_message*);

struct State {
    bool enabled = true;
    Color color;
};

class MQTT
{
public:
    typedef std::function<void(const State&)> StateCb;

    MQTT();
    ~MQTT();
    bool init();
    void run();
    void publishInfo();
    void publish(const State &s);
    void setStateCallback(StateCb cb) { m_stateCallback = cb; }

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

    StateCb m_stateCallback;
    State curState;

    const std::string TOPIC = "groggle";
    const std::string TOPIC_SET = TOPIC + "/set";

    struct mosquitto *m_client = nullptr;
    std::mutex m_messagesMutex;
    std::unordered_map<int, std::shared_ptr<Message>> m_messagesInFlight;

    // Mosquitto library callbacks
    friend void on_publish(struct mosquitto*, void*, int);
    friend void on_message(struct mosquitto*, void*, const struct mosquitto_message*);
};

}

#endif
