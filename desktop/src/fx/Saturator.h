#pragma once

// The soft clipper the whole instrument saturates with: x(27 + x^2)/(27 + 9x^2),
// unity slope at zero, reaching exactly 1 at an input of 3 and flat beyond it.
//
// The curve is Emilie Gillet's, from stmlib's SoftClip, re-typed so the effects need no Mutable Instruments include.
inline float softClip(float x)
{
    if (x < -3.0f) return -1.0f;
    if (x > 3.0f) return 1.0f;

    return x * (27.0f + x * x) / (27.0f + 9.0f * x * x);
}

// The same curve as a master clipper: transparent below `threshold`, soft
// above it, and bounded at one. The slope matches either side of the threshold, so the knee is a bend, not a step.
inline float softClipAbove(float x, float threshold)
{
    const float magnitude = x < 0.0f ? -x : x;
    if (magnitude <= threshold)
        return x;

    const float sign = x < 0.0f ? -1.0f : 1.0f;
    const float headroom = 1.0f - threshold;
    return sign * (threshold + headroom * softClip((magnitude - threshold) / headroom));
}
