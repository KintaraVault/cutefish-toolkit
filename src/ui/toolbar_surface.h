// SPDX-License-Identifier: BSD-3-Clause
#pragma once

#include "ui/painter.h"
#include "ui/path.h"

#include <algorithm>

namespace cutefish::ui::toolbar_surface {

// A light-catching rim and a near-transparent interior, so the header's
// material (including any tint) is still visible through the toolbar.
inline void capsule(Painter& painter, const RectF& box, bool dark)
{
    if (box.width <= 0 || box.height <= 0)
        return;
    const double r = std::min(box.height, box.width) / 2;
    const auto white = Color::rgb(0xffffff);
    const auto black = Color::rgb(0x000000);
    painter.fillRoundedRect(box, r, LinearGradient::vertical(box,
        dark ? white.withAlpha(0.025) : white.withAlpha(0.37),
        dark ? white.withAlpha(0.012) : black.withAlpha(0.024)));
    painter.strokeRoundedRect(box.adjusted(0.5, 0.5, -0.5, -0.5), r - 0.5,
        dark ? white.withAlpha(0.095) : black.withAlpha(0.10));
}

inline void highlight(Painter& painter, const RectF& box, bool dark, bool pressed, bool selected = false)
{
    if (box.width <= 0 || box.height <= 0)
        return;
    const auto ink = dark ? Color::rgb(0xffffff) : Color::rgb(0x000000);
    const double opacity = pressed ? (dark ? 0.24 : 0.17) : selected ? (dark ? 0.165 : 0.115) : (dark ? 0.085 : 0.065);
    painter.fillRoundedRect(box, std::min(box.height, box.width) / 2, ink.withAlpha(opacity));
}

} // namespace cutefish::ui::toolbar_surface
