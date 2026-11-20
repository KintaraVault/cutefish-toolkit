// SPDX-License-Identifier: BSD-3-Clause
#pragma once

#include <optional>

#include "ui/geometry.h"

namespace cutefish::ui {
class Painter;
class Window;
struct ViewGlass;
}

namespace cutefish::ui::toolbar_surface {

// Translucent body and directional bevel; the header tint shows through.
void capsule(Painter& painter, const RectF& box, bool dark);
void searchField(Painter& painter, const RectF& box, bool dark, std::optional<Color> tint = {});
void searchFocus(Painter& painter, const RectF& box, Color accent);
void highlight(Painter& painter, const RectF& box, bool dark, bool pressed, bool selected = false);
// A real backdrop lens on content within a view, using the same material
// strength as the toolbar and its window. No settings are read implicitly.
ViewGlass previewGlass(const RectF& box, double strength, bool reduceTransparency);
void applyWindowGlass(Window& window);

} // namespace cutefish::ui::toolbar_surface
