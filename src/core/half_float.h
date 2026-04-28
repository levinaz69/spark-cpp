#pragma once

#include <cstdint>
#include <cstring>
#include <cmath>
#include <algorithm>

namespace spark {

// Minimal half-float (IEEE 754 binary16) utilities
// For full half-float support, consider using the 'half' library

inline uint16_t float_to_half(float value) {
    uint32_t f;
    std::memcpy(&f, &value, 4);

    uint32_t sign = (f >> 16) & 0x8000;
    int32_t exponent = ((f >> 23) & 0xFF) - 127;
    uint32_t mantissa = f & 0x007FFFFF;

    if (exponent > 15) {
        // Overflow -> infinity
        return static_cast<uint16_t>(sign | 0x7C00);
    }
    if (exponent < -14) {
        // Underflow -> denormalized or zero
        if (exponent < -24) return static_cast<uint16_t>(sign);
        mantissa |= 0x00800000;
        uint32_t shift = static_cast<uint32_t>(-1 - exponent);
        mantissa >>= shift;
        return static_cast<uint16_t>(sign | (mantissa >> 13));
    }

    return static_cast<uint16_t>(sign | ((exponent + 15) << 10) | (mantissa >> 13));
}

inline float half_to_float(uint16_t h) {
    uint32_t sign = (static_cast<uint32_t>(h) & 0x8000) << 16;
    uint32_t exponent = (h >> 10) & 0x1F;
    uint32_t mantissa = h & 0x03FF;

    uint32_t f;
    if (exponent == 0) {
        if (mantissa == 0) {
            f = sign; // zero
        } else {
            // Denormalized
            exponent = 1;
            while (!(mantissa & 0x0400)) {
                mantissa <<= 1;
                exponent--;
            }
            mantissa &= 0x03FF;
            f = sign | ((exponent + 127 - 15) << 23) | (mantissa << 13);
        }
    } else if (exponent == 31) {
        f = sign | 0x7F800000 | (mantissa << 13); // inf or NaN
    } else {
        f = sign | ((exponent + 127 - 15) << 23) | (mantissa << 13);
    }

    float result;
    std::memcpy(&result, &f, 4);
    return result;
}

} // namespace spark
