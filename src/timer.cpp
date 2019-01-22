#include "timer.h"

#include <chrono>
#include <cmath>
#include <iostream>
#include <unistd.h> // usleep

using std::chrono::steady_clock;

using namespace groggel;

Timer::Timer(const float duration, const float frequency)
    : m_duration(duration * S_TO_NS)
    , m_pulseInterval(round(1 / frequency * S_TO_NS))
{}

void Timer::run()
{
    const auto start = steady_clock::now();
    long long elapsed = 0;
    long long elapsedAtNextPulse = m_pulseInterval;

    do {
        const auto elapsedSpan = steady_clock::now() - start;
        elapsed = elapsedSpan.count();

        if (elapsed >= elapsedAtNextPulse) {
            m_tick(elapsed);

            const auto elapsedSpanAfterTick = steady_clock::now() - start;
            int steps = 0;
            while (elapsedAtNextPulse < elapsedSpanAfterTick.count()) {
                elapsedAtNextPulse += m_pulseInterval;
                steps++;
            }

            if (steps > 1) {
                std::cerr << "Timer: Skipped " << steps - 1 << " pulses!\n";
            }
            assert(elapsedAtNextPulse > elapsedSpanAfterTick.count());
        }

        usleep( 1 /*ms*/ * 1000 );
        //usleep( 1 * 100 );
    } while ( elapsed < m_duration );
}
