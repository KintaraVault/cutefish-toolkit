// SPDX-License-Identifier: BSD-3-Clause
#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>

#include "ui/geometry.h"

namespace cutefish::ui {

// Small signed sRGB adjustments to opaque material colours. We send these
// instead of wallpaper pixels (or even the actual sampled colour) to clients.
struct WallpaperTint
{
    static constexpr uint32_t neutral = 0x808080;
    uint32_t light = neutral;
    uint32_t dark = neutral;

    bool operator==(const WallpaperTint&) const = default;

    static WallpaperTint fromSample(double r, double g, double b)
    {
        const double luma = 0.2126 * r + 0.7152 * g + 0.0722 * b;
        // Light material picks up a little shade, dark material stays close
        // to its base. Bound the chroma independently even for neon wallpapers.
        const auto pack = [&](double shade, double gain, double limit) {
            const auto channel = [&](double value) {
                return uint32_t(128 + std::lround(shade + std::clamp((value - luma) * gain, -limit, limit)));
            };
            return channel(r) << 16 | channel(g) << 8 | channel(b);
        };
        return {pack(-2.0 - 9.0 * (1.0 - luma / 255.0), 0.05, 3.0),
                pack(7.0 * (luma / 255.0 - 0.4), 0.08, 5.0)};
    }

    uint32_t forMode(bool darkMode) const { return darkMode ? dark : light; }

    static Color apply(Color base, uint32_t tone)
    {
        const auto channel = [&](uint8_t value, int shift) {
            // Also bound unexpected values received from an older/custom
            // compositor. Preserve the material's original opacity.
            const int offset = std::clamp(int((tone >> shift) & 0xff) - 128, -14, 10);
            return uint8_t(std::clamp(int(value) + offset, 0, 255));
        };
        return {channel(base.r, 16), channel(base.g, 8), channel(base.b, 0), base.a};
    }
};

} // namespace cutefish::ui
