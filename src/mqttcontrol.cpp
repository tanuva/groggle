#include "mqttcontrol.h"

#include <nlohmann/json.hpp>

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
    std::lock_guard<std::mutex> lock(mqtt->m_messagesMutex);
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

    if(!msg->payload) {
        SDL_Log("No payload for topic: %s", topic.c_str());
        return;
    }

    // Make sure to convert the payload to something safer first
    const std::string payload(reinterpret_cast<char*>(msg->payload), msg->payloadlen);
    SDL_Log("MQTT >> %s: %s", msg->topic, payload.c_str());

    const auto json = json::parse(payload);

    if(const auto state = json.find("state"); state != json.end()) {
        const auto stateValue = state.value();
        if(stateValue.is_string()) {
            if(stateValue == "ON") {
                mqtt->m_enabledCallback(true);
            } else if(stateValue == "OFF") {
                mqtt->m_enabledCallback(false);
            }
        }
    }

    Color newColor(0, 0, 1);

    if(const auto color = json.find("color"); color != json.end()) {
        const auto colorValue = color.value();
        if(const auto hue = colorValue.find("h"); hue != colorValue.end()) {
            if(hue.value().is_number()) {
                newColor.setH(hue.value());
            }
        }

        if(const auto sat = colorValue.find("s"); sat != colorValue.end()) {
            if(sat.value().is_number()) {
                newColor.setS(static_cast<float>(sat.value()) / 100.0f);
            }
        }

        mqtt->m_colorCallback(newColor);
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

    const std::string listenTopic = TOPIC_SET;
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

void MQTT::publishState(const bool enabled, const Color &color)
{
    std::shared_ptr<Message> msg = std::make_shared<Message>();
    msg->topic = TOPIC;
    // Creating the whole object in one statement interprets the outer object
    // as an array for some reason. Not sure how to do it in one step.
    json payload = {
        { "state", enabled ? "ON" : "OFF" },
        { "color", {
            { "h", color.h() },
            { "s", color.s() * 100 },
            { "r", Color::f2uint8(color.r()) },
            { "g", Color::f2uint8(color.g()) },
            { "b", Color::f2uint8(color.b()) }
        }}
    };
    msg->setPayload(payload.dump());
    publishMessage(msg, true);
}

void MQTT::publishInfo()
{
    // The "groggle" part of the topic should ideally be a UUID.
    // An empty payload deletes the device from HA.
    std::shared_ptr<Message> msg = std::make_shared<Message>();
    msg->topic = "homeassistant/light/groggle/config";
    json payload = {
        { "unique_id", "groggle" },
        { "name", "Groggle" },
        { "schema", "json" },
        { "state_topic", "groggle" },
        { "command_topic", "groggle/set" },
        { "rgb", true },
        { "hs", true },
        { "device", {
            { "identifiers", "groggle" },
            { "name", "Groggle" },
            { "sw_version", "1.0" } ,
            { "model", "Groggle" },
            { "manufacturer", "Marcel Kummer" }
        }}
    };
    msg->setPayload(payload.dump());
    publishMessage(msg, true);
}

void MQTT::publishMessage(const std::shared_ptr<Message> msg, const bool retain)
{
    std::lock_guard<std::mutex> lock(m_messagesMutex);
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
        SDL_Log("MQTT %i << %s: %s", msg->id, msg->topic.c_str(), msg->payload);
        break;
    default:
        SDL_Log("MQTT <x %i", res);
        break;
    }
}
