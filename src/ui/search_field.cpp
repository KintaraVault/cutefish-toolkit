// SPDX-FileCopyrightText: 2026 Reion Wong <reionwong@gmail.com>
// SPDX-License-Identifier: BSD-3-Clause

#include "ui/color_provider.h"
#include "ui/search_field.h"

#include <algorithm>

#include "ui/icon.h"
#include "ui/painter.h"
#include "ui/theme.h"
#include "ui/toolbar_surface.h"
#include "ui/window.h"

namespace cutefish::ui {

static constexpr double kGlyphSize = 14;

// A press area over the glyph: it takes the press, and the click clears.
class SearchField::ClearButton : public Icon
{
public:
    explicit ClearButton(SearchField* field)
        : Icon("clear", kGlyphSize)
        , m_field(field)
    {
    }

    bool acceptsPointer() const override { return true; }

protected:
    void pointerPress(const PointerEvent&) override { m_pressed = true; }

    void pointerRelease(const PointerEvent& event) override
    {
        const bool clicked = m_pressed && localRect().contains(event.position);
        m_pressed = false;
        if (clicked)
            m_field->setText({});
    }

private:
    SearchField* m_field;
    bool m_pressed = false;
};

SearchField::SearchField(std::string placeholder)
    : m_search(addChild<Icon>("search", kGlyphSize))
    , m_clear(addChild<ClearButton>(this))
{
    setPlaceholder(std::move(placeholder));
    setFrameVisible(false);
    m_search->setInkCentered(true);
    m_clear->setInkCentered(true);
    m_clear->setVisible(false);
    updateMargins();
}

void SearchField::setFill(std::optional<ColorRecipe> fill)
{
    m_fill = std::move(fill);
    requestPaint();
}

void SearchField::setGlyphColor(std::optional<ColorRecipe> color)
{
    m_search->setColor(color);
    m_clear->setColor(color);
}

void SearchField::setIconMetrics(double inset, double gap, double hitMargin)
{
    if (inset == m_inset && gap == m_gap && hitMargin == m_hitMargin)
        return;
    m_inset = inset;
    m_gap = gap;
    m_hitMargin = hitMargin;
    updateMargins();
    requestLayout();
}

void SearchField::paint(Painter& painter)
{
    const Theme& theme = Theme::current();
    const RectF bounds = localRect();
    const bool dark = window() ? window()->headerDarkMode() : theme.darkMode;
    toolbar_surface::searchField(painter, bounds, dark,
                                 m_fill ? std::optional(m_fill->resolve()) : std::nullopt);
    if (hasFocus())
        toolbar_surface::searchFocus(painter, bounds, theme.highlight);
    TextField::paint(painter);
}

void SearchField::layout()
{
    const RectF bounds = localRect();
    const double top = (bounds.height - kGlyphSize) / 2;
    const double hitX = std::min(m_inset, m_hitMargin);
    const double hitY = std::clamp(top, 0.0, m_hitMargin);
    m_search->setGeometry({m_inset, top, kGlyphSize, kGlyphSize});
    m_clear->setGeometry({bounds.width - m_inset - kGlyphSize - hitX, top - hitY,
                          kGlyphSize + 2 * hitX, kGlyphSize + 2 * hitY});
}

void SearchField::textChanged()
{
    m_clear->setVisible(!text().empty());
    updateMargins();
}

void SearchField::updateMargins()
{
    const double glyphEnd = m_inset + kGlyphSize + m_gap;
    setTextMargins(glyphEnd, text().empty() ? m_inset : glyphEnd);
}

} // namespace cutefish::ui
