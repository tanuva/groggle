#ifndef TIMER
#define TIMER

#include <functional>

namespace groggel
{

class Timer
{
public:
    typedef std::function<void(long long)> Callback;

    Timer(const float duration, const float frequency);
    void setCallback(Callback tick) {
        m_tick = tick;
    }
    void run();
    void stop() {
        m_running = false;
    }

private:
    const long long S_TO_NS = 1000 * 1000 * 1000;
    const long long m_duration;
    const long long m_pulseInterval;
    bool m_running = false;
    Callback m_tick = [](const long long) {};
};

}

#endif
