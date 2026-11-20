// SPDX-FileCopyrightText: 2026 Reion Wong <reionwong@gmail.com>
// SPDX-License-Identifier: BSD-3-Clause

#include "ui/header_button_group.h"

#include <cmath>

#include "ui/header_button.h"
#include "ui/painter.h"
#include "ui/theme.h"
#include "ui/toolbar_surface.h"
#include "ui/window.h"
#include "ui/window_controls.h"

namespace cutefish::ui {

// The divider stops short of the plate's top and bottom by this much.
static constexpr double kDividerInset = 6;

HeaderButtonGroup::HeaderButtonGroup()
{
    setMovesWindow(true);
}

HeaderButton* HeaderButtonGroup::addButton(std::string glyph, double glyphSize)
{
    auto* button = addChild<HeaderButton>(std::string(), kSegment);
    button->setGlyph(std::move(glyph), glyphSize);
    button->setSegment(true);
    m_buttons.push_back(button);
    return button;
}

SizeF HeaderButtonGroup::sizeHint() const
{
    int visible = 0;
    for (const HeaderButton* button : m_buttons) {
        if (button->isVisible())
            ++visible;
    }
    const double segment = m_toolbarAppearance ? 30 : kSegment;
    return {visible * segment, segment};
}

void HeaderButtonGroup::setToolbarAppearance(bool enabled)
{
    if (m_toolbarAppearance == enabled)
        return;
    m_toolbarAppearance = enabled;
    requestLayout();
    requestPaint();
}

RectF HeaderButtonGroup::visualRect() const
{
    return m_toolbarAppearance ? localRect() : window_controls::visualRect(localRect(), window_controls::State {});
}

void HeaderButtonGroup::layout()
{
    const RectF bounds = localRect();
    const SizeF hint = sizeHint();
    const double width = hint.width > 0 ? bounds.width * hint.height / hint.width : 0;
    double x = 0;
    for (HeaderButton* button : m_buttons) {
        if (!button->isVisible())
            continue;
        button->setGeometry({x, 0, width, bounds.height});
        x += width;
    }
}

void HeaderButtonGroup::paint(Painter& painter)
{
    const Window* window = this->window();
    const bool dark = window ? window->headerDarkMode() : Theme::current().darkMode;
    const RectF bounds = localRect();
    if (m_toolbarAppearance)
        toolbar_surface::capsule(painter, bounds, dark);
    else
        window_controls::paintGroupPlate(painter, bounds, dark);
    const Color ink = Color::rgb(dark ? 0xffffff : 0x000000);
    const Color edge = ink.withAlpha(dark ? 0.14 : 0.12);
    bool first = true;
    for (const HeaderButton* button : m_buttons) {
        if (!button->isVisible())
            continue;
        if (!first) {
            const double x = button->geometry().x;
            if (m_toolbarAppearance) {
                const double inset = bounds.height * 0.20;
                const double column = std::floor((x + painter.offset().x) * painter.scale());
                const RectF seam{column / painter.scale() - painter.offset().x, bounds.y + inset,
                                1 / painter.scale(), bounds.height - 2 * inset};
                Path path;
                path.addRect(seam);
                painter.fillPath(path, LinearGradient{{seam.x, seam.y}, {seam.x, seam.bottom()},
                    {{0, ink.withAlpha(0)}, {0.20, edge}, {0.80, edge}, {1, ink.withAlpha(0)}}});
            } else {
                painter.drawVerticalLine(x, bounds.y + kDividerInset, bounds.height - 2 * kDividerInset, edge);
            }
        }
        first = false;
    }
}

} // namespace cutefish::ui
