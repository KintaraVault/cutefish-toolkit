// SPDX-FileCopyrightText: 2026 Reion Wong <reionwong@gmail.com>
// SPDX-License-Identifier: BSD-3-Clause

#include "ui/color_provider.h"
#include "ui/slider.h"

#include <algorithm>
#include <cmath>

#include "ui/keys.h"
#include "ui/painter.h"
#include "ui/window.h"

namespace cutefish::ui {

static constexpr double kTrack = 4;
static constexpr double kHandle = kTrack + 2 * Slider::trackInset();
static constexpr double kHandleHeight = 16;
static constexpr double kShadowSigma = 2.5;
static constexpr double kShadowOffset = 2;

Slider::Slider() = default;

void Slider::setRange(double from, double to)
{
    m_from = from;
    m_to = to;
    setValue(m_value);
    // The handle moves for the same value in another range.
    requestPaint();
}

void Slider::setStepSize(double step)
{
    m_step = std::max(0.0, step);
    requestPaint();
}

void Slider::setStepped(bool stepped)
{
    if (stepped == m_stepped)
        return;
    m_stepped = stepped;
    requestLayout();
}

void Slider::setSnapMode(SnapMode mode)
{
    m_snapMode = mode;
}

Slider::SnapMode Slider::snapMode() const
{
    return m_snapMode.value_or(m_stepped ? SnapMode::SnapAlways : SnapMode::SnapOnRelease);
}

// A value set from outside is clamped, not snapped.
void Slider::setValue(double value)
{
    value = std::clamp(value, std::min(m_from, m_to), std::max(m_from, m_to));
    if (value == m_value)
        return;
    m_value = value;
    requestPaint();
    updateValueToolTip();
}

void Slider::setValueToolTip(std::function<std::string(double value)> text)
{
    m_valueToolTip = std::move(text);
    updateValueToolTip();
}

void Slider::updateValueToolTip()
{
    Window* window = this->window();
    if (!window || !m_valueToolTip)
        return;
    if (m_pressed)
        window->pinToolTip(this, handleRect(), m_valueToolTip(m_value));
    else
        window->unpinToolTip(this);
}

// On a step whenever there are steps.
double Slider::valueAt(double position) const
{
    const double value = (m_to - m_from) * position;
    if (m_step <= 0)
        return m_from + value;
    return m_from + std::round(value / m_step) * m_step;
}

double Slider::snapPosition(double position) const
{
    const double range = m_to - m_from;
    if (range == 0 || m_step <= 0)
        return position;
    const double step = m_step / range;
    return std::clamp(std::round(position / step) * step, 0.0, 1.0);
}

double Slider::position() const
{
    return m_to == m_from ? 0 : std::clamp((m_value - m_from) / (m_to - m_from), 0.0, 1.0);
}

double Slider::visualPosition() const
{
    return m_pressed ? m_dragPosition : position();
}

RectF Slider::visualRect() const
{
    // The handle's shadow, wherever the handle is.
    const double reach = Painter::shadowReach(kShadowSigma);
    return localRect().adjusted(-reach, std::min(0.0, kShadowOffset - reach), reach, kShadowOffset + reach);
}

SizeF Slider::sizeHint() const
{
    return {200, m_stepped ? 32.0 : 27.0};
}

RectF Slider::handleRect() const
{
    const RectF bounds = localRect();
    const double travel = std::max(0.0, bounds.width - kHandle);
    return {travel * visualPosition(), (bounds.height - kHandleHeight) / 2, kHandle, kHandleHeight};
}

void Slider::paint(Painter& painter)
{
    const RectF bounds = localRect();
    const RectF handle = handleRect();
    const double centreY = bounds.height / 2;
    // The track runs between the handle's centres at both ends, its round
    // caps reaching a little past them.
    const double start = trackInset();
    const double length = std::max(0.0, bounds.width - kHandle) + kTrack;

    painter.save();
    // Rounded rectangles, snapped as the rounded-rectangle shader draws them.
    painter.drawRoundedRect({start, centreY - kTrack / 2, length, kTrack}, {kTrack / 2}, color(ColorId::SliderTrack));
    if (visualPosition() > 0) {
        const double filled = std::max(0.0, bounds.width - kHandle) * visualPosition() + kTrack;
        painter.drawRoundedRect({start, centreY - kTrack / 2, filled, kTrack}, {kTrack / 2},
                                color(isEnabled() ? ColorId::SliderTrackFilled : ColorId::SliderTickMark));
    }
    if (m_stepped && m_step > 0 && m_to != m_from) {
        const int intervals = int(std::ceil(std::abs(m_to - m_from) / m_step));
        const double travel = std::max(0.0, bounds.width - kHandle);
        // Marks only while they stay apart.
        if (intervals > 0 && intervals <= int(travel / 6)) {
            const Color mark = color(ColorId::SliderTickMark);
            for (int i = 0; i <= intervals; ++i) {
                const double x = kHandle / 2 + travel * std::min(1.0, double(i) / intervals);
                painter.drawRoundedRect({x - 0.5, centreY + 6, 1, 2}, {0.5}, mark);
            }
        }
    }
    painter.restore();

    painter.save();
    if (isEnabled() && hasVisibleFocus())
        painter.drawRoundedRect(handle.adjusted(-2, -2, 2, 2), {kHandleHeight / 2 + 2},
                                Color{0, 0, 0, 0}, 2, color(ColorId::TextFieldFocusRing));
    if (!isEnabled())
        painter.multiplyOpacity(0.55);
    if (isEnabled())
        painter.drawShadow(handle, kHandleHeight / 2, kShadowSigma,
                           color(m_pressed ? ColorId::SliderHandleShadowPressed : ColorId::SliderHandleShadow),
                           {0, kShadowOffset});
    const Color fill = color(m_pressed ? ColorId::SliderHandlePressed : ColorId::SliderHandle);
    const Color border = color(ColorId::SliderHandleBorder);
    painter.drawSmoothRectangle(handle, kHandleHeight / 2, fill, 0.5, border);
    painter.restore();
}

void Slider::moveTo(double value)
{
    value = std::clamp(value, std::min(m_from, m_to), std::max(m_from, m_to));
    if (value == m_value)
        return;
    m_value = value;
    requestPaint();
    if (onMoved)
        onMoved(m_value);
}

void Slider::pointerPress(const PointerEvent& event)
{
    if (event.button != MouseButton::Left)
        return;
    const RectF handle = handleRect();
    m_pressed = true;
    if (onPressedChanged)
        onPressedChanged();
    m_dragPosition = position();
    // Grabbing the handle keeps it where it was taken; elsewhere it jumps there.
    m_grabOffset = handle.contains(event.position) ? event.position.x - (handle.x + kHandle / 2) : 0;
    pointerMove(event);
}

void Slider::pointerMove(const PointerEvent& event)
{
    if (!m_pressed)
        return;
    const double travel = geometry().width - kHandle;
    if (travel <= 0)
        return;
    double fraction = std::clamp((event.position.x - m_grabOffset - kHandle / 2) / travel, 0.0, 1.0);
    if (snapMode() == SnapMode::SnapAlways)
        fraction = snapPosition(fraction);
    m_dragPosition = fraction;
    requestPaint();
    moveTo(valueAt(fraction));
    updateValueToolTip();
}

void Slider::pointerRelease(const PointerEvent&)
{
    if (!m_pressed)
        return;
    m_pressed = false;
    if (onPressedChanged)
        onPressedChanged();
    // The handle lands where the value is: on its step unless nothing snaps.
    if (snapMode() == SnapMode::NoSnap)
        moveTo(valueAt(m_dragPosition));
    requestPaint();
    updateValueToolTip();
}

bool Slider::keyPress(const KeyEvent& event)
{
    // A step, or 0.1 without one.
    const double step = m_step > 0 ? m_step : 0.1;
    const double direction = m_to >= m_from ? 1 : -1;
    switch (event.key) {
    case key::Left:
    case key::Down:
        moveTo(m_value - step * direction);
        return true;
    case key::Right:
    case key::Up:
        moveTo(m_value + step * direction);
        return true;
    case key::Home:
        moveTo(m_from);
        return true;
    case key::End:
        moveTo(m_to);
        return true;
    default:
        return false;
    }
}

} // namespace cutefish::ui
