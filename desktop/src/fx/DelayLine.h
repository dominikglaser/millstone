#pragma once

#include <algorithm>
#include <cmath>
#include <vector>

// A short delay line read at a fractional position, which is what a modulated
// delay needs: the read point moves continuously, so it almost never lands on
// a whole sample. Linear interpolation is enough here because the delays are a
// few milliseconds and the modulation is slow.
class DelayLine
{
public:
    void prepare(double sampleRate, float maximumDelaySeconds)
    {
        const auto length = static_cast<std::size_t>(sampleRate * maximumDelaySeconds) + 4;
        buffer.assign(length, 0.0f);
        writeIndex = 0;
    }

    void reset()
    {
        std::fill(buffer.begin(), buffer.end(), 0.0f);
        writeIndex = 0;
    }

    void write(float sample)
    {
        buffer[writeIndex] = sample;
        if (++writeIndex >= buffer.size())
            writeIndex = 0;
    }

    // delaySamples may be fractional, and is clamped to what the line holds.
    float read(float delaySamples) const
    {
        const auto length = static_cast<float>(buffer.size());
        delaySamples = std::clamp(delaySamples, 1.0f, length - 2.0f);

        float position = static_cast<float>(writeIndex) - delaySamples;
        if (position < 0.0f)
            position += length;

        const auto index = static_cast<std::size_t>(position);
        const float fraction = position - static_cast<float>(index);
        const std::size_t next = index + 1 < buffer.size() ? index + 1 : 0;

        return buffer[index] + fraction * (buffer[next] - buffer[index]);
    }

private:
    std::vector<float> buffer;
    std::size_t writeIndex = 0;
};
