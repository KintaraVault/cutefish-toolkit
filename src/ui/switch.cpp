// SPDX-FileCopyrightText: 2026 Reion Wong <reionwong@gmail.com>
// SPDX-License-Identifier: BSD-3-Clause

#include "ui/color_provider.h"
#include "ui/switch.h"

#include <algorithm>
#include <chrono>

#include "ui/animation.h"
#include "ui/painter.h"

namespace cutefish::ui {

static constexpr SizeF kIndicator = {44, 20};
static constexpr SizeF kHandle = {26, 16};
static constexpr double kHandleMargin = 2;

static Color mix(Color from, Color to, double t)
{
    const auto channel = [t](uint8_t a, uint8_t b) { return uint8_t(a + (b - a) * t + 0.5); };
    return {channel(from.r, to.r), channel(from.g, to.g), channel(from.b, to.b), channel(from.a, to.a)};
}

Switch::Switch(std::string text)
    : ToggleControl(std::move(text), kIndicator, 8, Kind::Switch)
{
}

Switch::~Switch() = default;

void Switch::checkedChanged(bool animated)
{
    const double target = isChecked() ? 1 : 0;
    if (!animated || !window()) {
        // A state set from code, such as a page loading its values, snaps.
        m_handle = m_color = target;
        return;
    }
    if (!m_handleAnimation) {
        m_handleAnimation = std::make_unique<Animation>(window());
        m_handleAnimation->setDuration(std::chrono::milliseconds(250));
        m_handleAnimation->setEasing(Animation::Easing::OutSine);
        m_handleAnimation->onUpdate = [this](double value) {
            m_handle = m_handleFrom + ((isChecked() ? 1 : 0) - m_handleFrom) * value;
            requestPaint();
        };
        m_colorAnimation = std::make_unique<Animation>(window());
        m_colorAnimation->setDuration(std::chrono::milliseconds(125));
        m_colorAnimation->setEasing(Animation::Easing::InOutCubic);
        m_colorAnimation->onUpdate = [this](double value) {
            m_color = m_colorFrom + ((isChecked() ? 1 : 0) - m_colorFrom) * value;
            requestPaint();
        };
    }
    m_handleFrom = m_handle;
    m_colorFrom = m_color;
    m_handleAnimation->start();
    m_colorAnimation->start();
}

void Switch::paintIndicator(Painter& painter, const RectF& rect)
{
    painter.save();
    const Color off = color(ColorId::SwitchTrack);
    const Color on = isEnabled() ? color(ColorId::SwitchTrackOn) : color(ColorId::SliderTickMark);
    if (!isEnabled())
        painter.multiplyOpacity(0.55);
    painter.drawSmoothRectangle(rect, rect.height / 2, mix(off, on, m_color));
    const double travel = rect.width - kHandle.width - 2 * kHandleMargin;
    const RectF handle = {rect.x + kHandleMargin + travel * m_handle,
                          rect.y + (rect.height - kHandle.height) / 2, kHandle.width, kHandle.height};
    if (isEnabled())
        painter.drawShadow(handle, kHandle.height / 2, 0.9, Color::rgb(0).withAlpha(0.19), {0, 1});
    painter.drawSmoothRectangle(handle, kHandle.height / 2, color(ColorId::SwitchHandle),
                                0.5, Color::rgb(0).withAlpha(0.08));
    painter.restore();
}

} // namespace cutefish::ui
