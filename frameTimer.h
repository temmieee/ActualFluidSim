#pragma once

#include <chrono>
#include <vector>
#include <cstddef>

class FrameTimer
{
public:
    using clock_t = std::chrono::high_resolution_clock;
    using timepoint_t = clock_t::time_point;

    explicit FrameTimer(std::size_t smoothingWindow = 100)
        : m_smoothingWindow((smoothingWindow < 1) ? 1 : smoothingWindow),
          m_buffer(m_smoothingWindow, 0.0),
          m_index(0),
          m_count(0),
          m_sum(0.0),
          m_deltaSeconds(0.0),
          m_initialized(false)
    {
    }

    void Start()
    {
        m_last = clock_t::now();
        m_initialized = true;
    }

    double Tick()
    {
        const timepoint_t now = clock_t::now();
        if (!m_initialized) {
            // First call:
            m_last = now;
            m_initialized = true;
            m_deltaSeconds = 0.0;
            pushToBuffer(m_deltaSeconds);
            return m_deltaSeconds;
        }

        const std::chrono::duration<double> elapsed = now - m_last;
        m_deltaSeconds = elapsed.count();
        m_last = now;
        //prevent time travel lmao
        if (m_deltaSeconds < 0.0) {
            m_deltaSeconds = 0.0;
        }

        pushToBuffer(m_deltaSeconds);
        return m_deltaSeconds;
    }

    double GetDelta() const
    {
        return m_deltaSeconds;
    }

    double GetSmoothedDelta() const
    {
        if (m_count == 0) {
            return 0.0;
        }
        return m_sum / static_cast<double>(m_count);
    }

    double GetFPS() const
    {
        const double avg = GetSmoothedDelta();
        if (avg <= 1e-12) {
            return 0.0;
        }
        return 1.0 / avg;
    }

    void Reset()
    {
        std::fill(m_buffer.begin(), m_buffer.end(), 0.0);
        m_index = 0;
        m_count = 0;
        m_sum = 0.0;
        m_deltaSeconds = 0.0;
        m_initialized = false;
    }

    std::size_t SampleCount() const { return m_count; }

    std::size_t SmoothingWindow() const { return m_smoothingWindow; }

private:
    void pushToBuffer(double value)
    {
        if (m_count < m_smoothingWindow) {
            m_buffer[m_index] = value;
            m_sum += value;
            ++m_count;
            m_index = (m_index + 1) % m_smoothingWindow;
        } else {
            m_sum -= m_buffer[m_index];
            m_buffer[m_index] = value;
            m_sum += value;
            m_index = (m_index + 1) % m_smoothingWindow;
        }
    }

    const std::size_t m_smoothingWindow;
    std::vector<double> m_buffer;
    std::size_t m_index;
    std::size_t m_count;
    double m_sum;
    double m_deltaSeconds;
    timepoint_t m_last;
    bool m_initialized;
};