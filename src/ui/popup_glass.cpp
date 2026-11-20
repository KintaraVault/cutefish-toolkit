// SPDX-FileCopyrightText: 2026 Reion Wong <reionwong@gmail.com>
// SPDX-License-Identifier: BSD-3-Clause

#include "ui/popup_glass.h"

#include "ui/theme.h"

namespace cutefish::ui {

GlassMaterial popupGlass(bool darkMode)
{
    GlassMaterial glass;
    glass.blurSigma = 10;
    glass.displacement = 10;
    glass.chroma = 0;
    glass.saturation = darkMode ? 1.2 : 1.4;
    // A thicker frost keeps symbols readable over bright or patterned content.
    if (darkMode) {
        glass.tintRed = 0.14;
        glass.tintGreen = 0.14;
        glass.tintBlue = 0.15;
    } else {
        glass.tintRed = 1;
        glass.tintGreen = 1;
        glass.tintBlue = 1;
    }
    glass.tintAlpha = darkMode ? design::menuTintDark : design::menuTintLight;
    return glass;
}

bool PopupGlass::update(Window& window)
{
    if (!window.backgroundGlassAvailable() || Theme::appearanceStyle().reduceTransparency) {
        if (m_darkMode) {
            m_darkMode.reset();
            window.setBackgroundGlass(std::nullopt);
        }
        return false;
    }
    const bool darkMode = Theme::current().darkMode;
    if (m_darkMode != darkMode) {
        m_darkMode = darkMode;
        window.setBackgroundGlass(popupGlass(darkMode));
    }
    return true;
}

void PopupGlass::prepare(Window& window)
{
    window.setBackgroundColor(Color::rgb(0x000000).withAlpha(0));
    window.setTranslucentSubpixelText(!window.backgroundGlassAvailable() || Theme::appearanceStyle().reduceTransparency);
}

} // namespace cutefish::ui
