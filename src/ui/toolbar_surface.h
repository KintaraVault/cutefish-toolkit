// SPDX-License-Identifier: BSD-3-Clause
#pragma once

#include "ui/painter.h"
#include "ui/path.h"

#include <algorithm>

namespace cutefish::ui::toolbar_surface {

inline void capsule(Painter& painter, const RectF& box, bool dark, double emphasis = 0)
{
    if (box.width <= 0 || box.height <= 0)
        return;
    const double r = std::min(box.height, box.width) / 2;
    const auto white = Color::rgb(0xffffff);
    const auto black = Color::rgb(0x000000);
    painter.fillRoundedRect(box, r, LinearGradient::vertical(box,
        dark ? white.withAlpha(0.041 + emphasis) : black.withAlpha(0.012 + emphasis * 0.7),
        dark ? white.withAlpha(0.019 + emphasis * 0.8) : black.withAlpha(0.036 + emphasis * 0.8)));
    painter.strokeRoundedRect(box, r, dark ? white.withAlpha(0.095) : black.withAlpha(0.075));
}

} // namespace cutefish::ui::toolbar_surface
