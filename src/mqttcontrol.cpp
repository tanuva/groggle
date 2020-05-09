#include "mqttcontrol.h"

#include <json.hpp>

#include <SDL_log.h>

#include <errno.h>

#include <cassert>
#include <sstream>

using namespace groggle;
using json = nlohmann::json;

void groggle::on_publish(struct mosquitto */*client*/, void *userdata, int mid)
{
    SDL_Log("Published: %i", mid);
    MQTT *mqtt = reinterpret_cast<MQTT*>(userdata);
    assert(mqtt);
    mqtt->m_messagesInFlight.erase(mid);
}

void groggle::on_message(struct mosquitto */*client*/,
                         void *userdata,
                         const struct mosquitto_message *msg)
{
    /*
    struct mosquitto_message{
        int mid;
        char *topic;
        void *payload;
        int payloadlen;
        int qos;
        bool retain;
    };
    */

    MQTT *mqtt = reinterpret_cast<MQTT*>(userdata);
    assert(mqtt);

    const std::string topic = msg->topic;
    if(topic.find(mqtt->TOPIC_SET) != 0) {
        SDL_Log("Unexpected topic: %s", topic.c_str());
        return;
    }

    if(!msg->payload) {
        SDL_Log("No payload for topic: %s", topic.c_str());
        return;
    }

    // Aaaah. Clean this up!
    // Make sure to convert the payload to something safer first
    const std::string payload(reinterpret_cast<char*>(msg->payload), msg->payloadlen);
    SDL_Log("MQTT >> %s: %s", msg->topic, payload.c_str());

    const std::string property = topic.substr(mqtt->TOPIC_SET.length(), std::string::npos);
    std::stringstream ss(payload, std::ios_base::in);
    if(property == "enabled") {
        bool enabled = false;
        ss >> enabled;
        mqtt->m_enabledCallback(enabled);
    } else {
        SDL_Log("Unexpected property: %s", property.c_str());
    }
}

void MQTT::Message::setPayload(const std::string &s)
{
    payload = new char[strlen(s.c_str())];
    strcpy(payload, s.c_str());
}

MQTT::MQTT()
{
    mosquitto_lib_init();
}

MQTT::~MQTT()
{
    if(m_client) {
        mosquitto_disconnect(m_client);
        mosquitto_destroy(m_client);
    }

    mosquitto_lib_cleanup();
}

bool MQTT::init()
{
    m_client = mosquitto_new("groggle", true, this);
    if(!m_client) {
        SDL_Log("MQTT init failed: %s", strerror(errno));
        return false;
    }

    mosquitto_threaded_set(m_client, true);

    switch(mosquitto_connect(m_client, "localhost", 1883, 10)) {
    case MOSQ_ERR_INVAL:
        SDL_Log("MQTT client cannot connect, invalid input parameters");
        return false;
    case MOSQ_ERR_ERRNO:
        SDL_Log("MQTT connection failed: %s", strerror(errno));
        return false;
    }

    mosquitto_publish_callback_set(m_client, &on_publish);
    mosquitto_message_callback_set(m_client, &on_message);

    const std::string listenTopic = TOPIC_SET + '#';
    int res = mosquitto_subscribe(m_client, nullptr, listenTopic.c_str(), 0);
    if(res != MOSQ_ERR_SUCCESS) {
        SDL_Log("Subscription to \"%s\" failed: %s",
            listenTopic.c_str(),
            mosquitto_strerror(res));
        return false;
    }

    return true;
}

void MQTT::run()
{
    mosquitto_loop_forever(m_client, -1, 1);
}

void MQTT::publishEnabled(const bool enabled)
{
    std::shared_ptr<Message> msg = std::make_shared<Message>();
    msg->topic = TOPIC + "enabled";

    std::stringstream payload;
    payload << enabled;
    msg->setPayload(payload.str());

    publishMessage(msg);
}

void MQTT::publishMessage(const std::shared_ptr<Message> msg, const bool retain)
{
    int res = mosquitto_publish(m_client,
        &msg->id,
        msg->topic.c_str(),
        strlen(msg->payload),
        msg->payload,
        0,
        retain);

    switch(res) {
    case MOSQ_ERR_SUCCESS:
        m_messagesInFlight.insert({msg->id, msg});
        SDL_Log("MQTT << %s: %s", msg->topic.c_str(), msg->payload);
        break;
    default:
        SDL_Log("MQTT <x %i", res);
        break;
    }
}
