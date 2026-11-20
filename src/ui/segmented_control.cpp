// SPDX-FileCopyrightText: 2026 Reion Wong <reionwong@gmail.com>
// SPDX-License-Identifier: BSD-3-Clause

#include "ui/color_provider.h"
#include "ui/segmented_control.h"
#include "ui/toolbar_surface.h"

#include <algorithm>
#include <chrono>
#include <cmath>

#include "base/event_loop.h"
#include "ui/application.h"
#include "ui/icon.h"
#include "ui/keys.h"
#include "ui/label.h"
#include "ui/painter.h"
#include "ui/theme.h"

namespace cutefish::ui {

// 32 high inside a padding of 2, the plate 2 inside its button. A tab bar: a
// tab button high, a medium icon and a small spacing, without padding.
static constexpr double kSegmentedHeight = 32;
static constexpr double kTabHeight = 38;
static constexpr double kAccentHeight = 23;
static constexpr double kSegmentedPadding = 2;
static constexpr double kPlateInset = 2;
static constexpr std::chrono::milliseconds kPressDelay{50};

class SegmentedControl::Segment : public View
{
public:
    Segment(SegmentedControl* control, std::string text)
        : m_control(control)
        , m_label(addChild<Label>(std::move(text)))
    {
        // The label is plain text, coloured rather than faded.
        m_label->setFadesWhenDisabled(false);
        m_label->setTextAlignment(Label::TextAlignment::Center);
        m_label->setVerticallyCentered(true);
        setName(m_label->text());
        setToolTip(m_label->text());
        m_icon = addChild<Icon>(std::string(), 16);
        m_icon->setInkCentered(true);
        m_icon->setVisible(false);
    }

    Label* label() const { return m_label; }
    void updateGlyphSize() { m_icon->setSize(m_control->m_toolbarAppearance ? 20 : 16); }
    void setGlyph(const std::string& glyph, bool showLabel)
    {
        m_icon->setIconName(glyph);
        m_icon->setVisible(!glyph.empty());
        m_label->setVisible(glyph.empty() || showLabel);
    }
    double intrinsicWidth() const
    {
        return std::max(m_icon->isVisible() ? 28.0 : 0.0, m_label->isVisible() ? m_label->sizeHint().width : 0.0);
    }
    bool acceptsFocus() const override { return true; }

protected:
    void layout() override
    {
        const RectF box = localRect();
        const bool stacked = m_icon->isVisible() && m_label->isVisible();
        const double labelHeight = stacked ? std::min(m_label->sizeHint().height, box.height / 2) : 0;
        const double iconHeight = box.height - labelHeight;
        const double size = std::min(m_control->m_toolbarAppearance ? 20.0 : 16.0, iconHeight);
        m_icon->setSize(size);
        m_icon->setGeometry({(box.width - size) / 2, (iconHeight - size) / 2, size, size});
        m_label->setGeometry(stacked ? RectF {0, iconHeight, box.width, labelHeight} : box);
    }

    bool keyPress(const KeyEvent& event) override
    {
        if (event.key != key::Space || event.modifiers != NoModifier)
            return false;
        m_control->setPressed(index());
        return true;
    }

    bool keyRelease(const KeyEvent& event) override
    {
        if (event.key != key::Space || m_control->m_pressed != index())
            return false;
        m_control->setPressed(-1);
        m_control->pick(index());
        return true;
    }

    void focusOut() override
    {
        if (m_control->m_pressed == index())
            m_control->setPressed(-1);
        m_control->requestPaint();
    }
    void focusIn(FocusReason) override { m_control->requestPaint(); }

private:
    int index() const
    {
        const auto& segments = m_control->m_segments;
        return int(std::find(segments.begin(), segments.end(), this) - segments.begin());
    }

    SegmentedControl* m_control;
    Label* m_label;
    Icon* m_icon;
};

SegmentedControl::SegmentedControl(std::vector<std::string> items, Style style)
    : m_style(style)
    , m_contentHeight(style == Style::Accent ? kAccentHeight :
                      style == Style::Segmented ? kSegmentedHeight : kTabHeight)
{
    setItems(std::move(items));
}

SegmentedControl::~SegmentedControl()
{
    if (m_pressTimer)
        Application::instance()->loop().stopTimer(m_pressTimer);
}

void SegmentedControl::setItems(std::vector<std::string> items)
{
    for (Segment* segment : m_segments)
        takeChild(segment);
    m_segments.clear();
    for (std::string& item : items) {
        Segment* segment = addChild<Segment>(this, std::move(item));
        segment->updateGlyphSize();
        segment->label()->setFontSize(m_fontSize);
        m_segments.push_back(segment);
    }
    m_current = std::clamp(m_current, 0, std::max(0, int(m_segments.size()) - 1));
    m_pressed = -1;
    updateColors();
    requestLayout();
}

void SegmentedControl::setCurrentIndex(int index)
{
    if (index < 0 || index >= int(m_segments.size()) || index == m_current)
        return;
    m_current = index;
    updateColors();
    requestPaint();
}

void SegmentedControl::setGlyphs(const std::vector<std::string>& glyphs, bool showLabels)
{
    for (size_t i = 0; i < m_segments.size(); ++i)
        m_segments[i]->setGlyph(i < glyphs.size() ? glyphs[i] : std::string(), showLabels);
    requestLayout();
}

void SegmentedControl::setToolbarAppearance(bool enabled)
{
    m_toolbarAppearance = enabled;
    if (m_toolbarCaption)
        m_toolbarCaption->setVisible(enabled && !m_toolbarCaption->text().empty());
    for (auto* segment : m_segments)
        segment->updateGlyphSize();
    requestLayout();
    requestPaint();
}

void SegmentedControl::setToolbarCaption(std::string text)
{
    if (!m_toolbarCaption) {
        m_toolbarCaption = addChild<Label>();
        m_toolbarCaption->setFontSize(10);
        m_toolbarCaption->setColor(ColorId::DisabledText);
        m_toolbarCaption->setTextAlignment(Label::TextAlignment::Center);
        m_toolbarCaption->setVerticallyCentered(true);
    }
    m_toolbarCaption->setText(std::move(text));
    m_toolbarCaption->setVisible(m_toolbarAppearance && !m_toolbarCaption->text().empty());
    requestLayout();
}

void SegmentedControl::setContentHeight(double height)
{
    if (height == m_contentHeight)
        return;
    m_contentHeight = height;
    requestLayout();
}

void SegmentedControl::setFontSize(double size)
{
    if (size == m_fontSize)
        return;
    m_fontSize = size;
    for (Segment* segment : m_segments)
        segment->label()->setFontSize(size);
    requestLayout();
}

void SegmentedControl::setTrackColor(std::optional<ColorRecipe> color)
{
    m_trackColor = std::move(color);
    requestPaint();
}

double SegmentedControl::padding() const
{
    return m_style == Style::Segmented ? (m_toolbarAppearance ? 3 : kSegmentedPadding) : 0;
}

double SegmentedControl::implicitItemWidth(size_t index) const
{
    return index < m_segments.size() ? m_segments[index]->intrinsicWidth() : 0;
}

SizeF SegmentedControl::sizeHint() const
{
    double width = 0;
    for (size_t i = 0; i < m_segments.size(); ++i)
        width += implicitItemWidth(i);
    return {width + 2 * padding() + Theme::current().smallSpacing,
        m_contentHeight + 2 * padding() + (m_toolbarCaption && m_toolbarCaption->isVisible() ? 10 : 0)};
}

RectF SegmentedControl::trackRect() const
{
    auto rect = localRect();
    if (m_toolbarCaption && m_toolbarCaption->isVisible())
        rect.height = std::max(0.0, rect.height - 10);
    return rect;
}

// Buttons without a width of their own get equal shares.
RectF SegmentedControl::itemRect(size_t index) const
{
    const RectF inner = trackRect().adjusted(padding(), padding(), -padding(), -padding());
    const double width = m_segments.empty() ? 0 : inner.width / double(m_segments.size());
    return {inner.x + width * double(index), inner.y, width, inner.height};
}

int SegmentedControl::itemAt(PointF point) const
{
    for (size_t i = 0; i < m_segments.size(); ++i) {
        if (itemRect(i).contains(point))
            return int(i);
    }
    return -1;
}

void SegmentedControl::layout()
{
    for (size_t i = 0; i < m_segments.size(); ++i)
        m_segments[i]->setGeometry(itemRect(i));
    if (m_toolbarCaption && m_toolbarCaption->isVisible())
        m_toolbarCaption->setGeometry({0, trackRect().height, localRect().width, 10});
}

void SegmentedControl::paint(Painter& painter)
{
    const Theme& theme = Theme::current();
    const RectF bounds = localRect();
    if (m_style == Style::Accent) {
        painter.drawRoundedRect(bounds, {6}, m_trackColor ? m_trackColor->resolve()
                                                         : theme.text.withAlpha(theme.darkMode ? 0.14 : 0.08));
        if (m_current >= 0 && m_current < int(m_segments.size())) {
            const Color fill = !isEnabled() ? color(ColorId::SliderTickMark)
                : m_pressed == m_current && m_pressShown ? darker(ColorId::Highlight, 1.12).resolve()
                                                         : color(ColorId::Highlight);
            painter.drawRoundedRect(itemRect(size_t(m_current)), {6}, fill);
        }
        for (size_t i = 0; i < m_segments.size(); ++i) {
            if (m_segments[i]->hasVisibleFocus())
                painter.drawRoundedRect(itemRect(i).shrunk(Margins::all(1)), {5}, Color{0, 0, 0, 0}, 1.5,
                                        color(int(i) == m_current ? ColorId::HighlightedText : ColorId::Highlight));
        }
        return;
    }
    if (m_toolbarAppearance && m_style == Style::Segmented) {
        toolbar_surface::capsule(painter, trackRect(), theme.darkMode);
        for (size_t i = 0; i < m_segments.size(); ++i) {
            const int index = int(i);
            const auto item = itemRect(i);
            if (index == m_pressed || index == m_current || index == m_toolbarHovered || m_segments[i]->hasFocus()) {
                toolbar_surface::highlight(painter, item.adjusted(1, 0, -1, 0), theme.darkMode,
                    index == m_pressed, index == m_current);
            }
            if (index > 0 && index != m_current && index - 1 != m_current
                && index != m_toolbarHovered && index - 1 != m_toolbarHovered)
                painter.drawVerticalLine(item.x, item.y + 4, item.height - 8,
                    theme.text.withAlpha(theme.darkMode ? 0.095 : 0.12));
        }
        return;
    }
    const Color plate = color(ColorId::SegmentedControlPlate);
    const double tabRadius = theme.smallRadius + 2;
    if (m_style == Style::Segmented) {
        painter.drawRoundedRect(bounds, {bounds.height / 2},
                                m_trackColor ? m_trackColor->resolve() : color(ColorId::SegmentedControlTrack));
    } else {
        painter.drawRoundedRect(bounds, {tabRadius, 0, themeCorners()}, color(ColorId::SegmentedTabBackground));
    }
    if (m_current < 0 || m_current >= int(m_segments.size()))
        return;

    const RectF item = itemRect(size_t(m_current));
    if (m_style == Style::Segmented) {
        // A plain rectangle with a drop shadow: Gaussian deviation (radius + 1) / 3.33.
        const RectF highlight = item.adjusted(kPlateInset, kPlateInset, -kPlateInset, -kPlateInset);
        painter.drawShadow(highlight, highlight.height / 2, 1.2, color(ColorId::SegmentedControlPlateShadow),
                           {0, 1});
        painter.drawSmoothRectangle(highlight, highlight.height / 2, plate);
    } else {
        // Concentric with the bar through the inset.
        const Color shadow = color(ColorId::SegmentedTabPlateShadow);
        if (shadow.a > 0)
            painter.drawShadow(item.adjusted(kPlateInset, kPlateInset, -kPlateInset, -kPlateInset), theme.smallRadius,
                               0.9, shadow);
        painter.drawRoundedRect(item.adjusted(kPlateInset, kPlateInset, -kPlateInset, -kPlateInset),
                                {tabRadius, kPlateInset, themeCorners()}, plate);
    }
}

void SegmentedControl::pointerMove(const PointerEvent& event)
{
    if (!m_toolbarAppearance)
        return;
    const int index = itemAt(event.position);
    if (index != m_toolbarHovered) {
        m_toolbarHovered = index;
        requestPaint();
    }
}

void SegmentedControl::pointerLeave()
{
    if (m_toolbarHovered >= 0) {
        m_toolbarHovered = -1;
        requestPaint();
    }
}

void SegmentedControl::enabledChanged()
{
    updateColors();
}

void SegmentedControl::updateColors()
{
    for (size_t i = 0; i < m_segments.size(); ++i) {
        // The disabled text colour, half the text colour while pressed.
        Label* label = m_segments[i]->label();
        if (!isEnabled())
            label->setColor(ColorId::DisabledText);
        else if (m_style == Style::Accent && int(i) == m_current)
            label->setColor(ColorId::HighlightedText);
        else if (int(i) == m_pressed && m_pressShown)
            label->setColor(ColorId::SegmentedControlTextPressed);
        else
            label->setColor(std::nullopt);
    }
}

void SegmentedControl::setPressed(int index)
{
    if (index == m_pressed)
        return;
    m_pressed = index;
    if (m_toolbarAppearance)
        requestPaint();
    // A quick click shows no pressed label; a held press does.
    m_pressShown = false;
    if (m_pressTimer) {
        Application::instance()->loop().stopTimer(m_pressTimer);
        m_pressTimer = 0;
    }
    if (index >= 0) {
        m_pressTimer = Application::instance()->loop().startTimer(kPressDelay, false, [this] {
            m_pressTimer = 0;
            m_pressShown = true;
            updateColors();
            requestPaint();
        });
    }
    updateColors();
    if (m_style == Style::Accent)
        requestPaint();
}

void SegmentedControl::pick(int index)
{
    if (index < 0 || index >= int(m_segments.size()) || index == m_current)
        return;
    m_current = index;
    updateColors();
    requestPaint();
    if (onCurrentChanged)
        onCurrentChanged(index);
}

void SegmentedControl::pointerPress(const PointerEvent& event)
{
    if (event.button != MouseButton::Left)
        return;
    const int index = itemAt(event.position);
    if (index >= 0)
        m_segments[size_t(index)]->setFocus(FocusReason::Mouse);
    setPressed(index);
}

void SegmentedControl::pointerRelease(const PointerEvent& event)
{
    const int pressed = m_pressed;
    setPressed(-1);
    if (pressed >= 0 && itemAt(event.position) == pressed)
        pick(pressed);
}

} // namespace cutefish::ui
