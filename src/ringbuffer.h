#ifndef RINGBUFFER_H
#define RINGBUFFER_H

#include <deque>

// TODO This should probably have a contract to only accept basic numeric types or so...

template <typename T>
class RingBuffer
{
public:
    RingBuffer(const size_t size)
        : m_maxSize(size)
    {}

    void append(T t) {
        m_values.push_back(t);
        while (m_values.size() > m_maxSize) {
            m_values.pop_front();
        }
    }

    T average() const {
        T avg = 0;
        for (const T v : m_values) {
            avg += v;
        }
        return avg / (m_values.size() > 0 ? m_values.size() : 1);
    }

private:
    size_t m_maxSize;
    std::deque<T> m_values;
};

#endif
