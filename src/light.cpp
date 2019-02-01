#include "light.h"

#include <algorithm> // min, max
#include <cmath>

void hslToRgb(const float hsl[3], float *rgb)
{
    // Source: https://en.wikipedia.org/wiki/HSL_and_HSV#Alternative_HSL_conversion
    const float a = hsl[1] * std::min(hsl[2], 1.0f - hsl[2]);
    static const auto k = [hsl](const int n) {
        // (n + H / 30) mod 12
        // The modulus shall preserve the fractional component.
        const float tmp = n + hsl[0] / 30.0f;
        const float frac = tmp - std::floor(tmp);
        return (static_cast<int>(floor(tmp)) % 12) + frac;
    };
    static const auto f = [hsl, a](const int n) {
        // f(n) = L - a * max(k - 3, 9 - k, 1), -1)
        return hsl[2] - a * std::max(
                                -1.0f,
                                std::min(
                                    1.0f,
                                    std::min(k(n) - 3.0f,
                                             9.0f - k(n))));
    };

    rgb[0] = f(0);
    rgb[1] = f(8);
    rgb[2] = f(4);
}
