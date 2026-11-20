// SPDX-License-Identifier: BSD-3-Clause
#pragma once

#include <algorithm>
#include <array>
#include <cmath>

#include "ui/geometry.h"

namespace cutefish::ui {

// Values are persisted by the Appearance service. Keep the indices stable.
enum class IconStyle { Default, Dark, Clear, Tinted };
enum class IconTone { Light, Dark, Auto };

struct AppearanceStyle
{
    double glass = 0.5;
    IconStyle icons = IconStyle::Default;
    IconTone tone = IconTone::Auto;
    int folderColor = 0;
    int tintColor = 0;
    bool reduceTransparency = false;
    int accentColor = -1; // Multicolor; unstyled controls retain the default blue.
    int textHighlight = 0; // Follow the accent.

    bool operator==(const AppearanceStyle&) const = default;
};

namespace appearance_style {

// Automatic folders keep their native blue; automatic tint follows the accent.
inline constexpr std::array colors{0x32a9da, 0x1688ef, 0x9b56dc, 0xec5d9d, 0xe64149,
                                   0xf38726, 0xe6b329, 0x35b952, 0x8c9098, 0x687cbe};

inline double glassValue(double value)
{
    return std::isfinite(value) ? std::clamp(value, 0.0, 1.0) : 0.5;
}

inline constexpr IconStyle iconStyle(int value)
{
    return value >= 0 && value <= int(IconStyle::Tinted) ? IconStyle(value) : IconStyle::Default;
}

inline constexpr IconTone iconTone(int value)
{
    return value >= 0 && value <= int(IconTone::Auto) ? IconTone(value) : IconTone::Auto;
}

inline constexpr int colorIndex(int value)
{
    return value >= 0 && value < int(colors.size()) ? value : 0;
}

inline constexpr Color color(int index, Color automatic)
{
    return colorIndex(index) ? Color::rgb(colors[index]) : automatic;
}

inline constexpr bool dark(const AppearanceStyle& style, bool darkMode)
{
    if (style.icons == IconStyle::Default)
        return darkMode;
    if (style.icons == IconStyle::Dark)
        return true;
    return style.tone == IconTone::Auto ? darkMode : style.tone == IconTone::Dark;
}

inline Color blend(Color a, Color b, double t)
{
    const auto mix = [t](int x, int y) { return uint8_t(std::lround(std::lerp(x, y, t))); };
    return {mix(a.r, b.r), mix(a.g, b.g), mix(a.b, b.b), mix(a.a, b.a)};
}

// Keep foreground artwork separate from its base: each style resolves its
// own ramp and opacity instead of tinting the finished tile a flat color.
struct IconLayers { Color top, bottom, ink, rim; };

inline IconLayers layers(const AppearanceStyle& style, bool darkMode, Color base, Color accent)
{
    const bool isDark = dark(style, darkMode);
    const auto white = Color::rgb(0xffffff);
    const auto black = Color::rgb(0);
    switch (style.icons) {
    case IconStyle::Dark:
        return {Color::rgb(0x45454c), Color::rgb(0x17181c),
                blend(base, white, .50), white.withAlpha(.26)};
    case IconStyle::Clear:
        return {white.withAlpha(isDark ? .28 : .76), Color::rgb(isDark ? 0x25252c : 0x969ca6).withAlpha(.64),
                Color::rgb(isDark ? 0xf4f5f7 : 0x373b44), white.withAlpha(isDark ? .24 : .74)};
    case IconStyle::Tinted: {
        const Color tint = color(style.tintColor, accent);
        return {blend(tint, isDark ? black : white, isDark ? .47 : .36),
                blend(tint, black, isDark ? .79 : .24),
                blend(tint, white, isDark ? .71 : .92), white.withAlpha(.29)};
    }
    case IconStyle::Default:
        return {blend(base, isDark ? black : white, isDark ? .13 : .19),
                blend(base, black, isDark ? .36 : .04), white, white.withAlpha(.25)};
    }
    return {};
}

} // namespace appearance_style
} // namespace cutefish::ui
