// SPDX-FileCopyrightText: 2026 Reion Wong <reionwong@gmail.com>
// SPDX-License-Identifier: BSD-3-Clause

#include "ui/toolkit_colors.h"

#include "ui/theme.h"
#include "ui/wallpaper_tint.h"

namespace cutefish::ui {

static constexpr Color kWhite = Color::rgb(0xFFFFFF);
static constexpr Color kBlack = Color::rgb(0x000000);
static constexpr Color kTransparent = Color{0, 0, 0, 0};

void addToolkitColors(ColorMixer& mixer, const Theme& theme, Color documentBackground, uint32_t wallpaperTone)
{
    const bool dark = theme.darkMode;

    mixer[ColorId::Background] = theme.background;
    mixer[ColorId::DocumentBackground] = documentBackground;
    mixer[ColorId::SecondBackground] = theme.secondBackground;
    mixer[ColorId::PanelBackground] = WallpaperTint::apply(theme.secondBackground, wallpaperTone);
    mixer[ColorId::WallpaperTone] = Color::rgb(wallpaperTone);
    mixer[ColorId::AlternateBackground] = theme.alternateBackground;
    mixer[ColorId::Text] = theme.text;
    mixer[ColorId::DisabledText] = theme.disabledText;
    mixer[ColorId::Highlight] = theme.highlight;
    mixer[ColorId::HighlightedText] = theme.highlightedText;

    mixer[ColorId::DestructiveAction] = dark ? Color::rgb(0xFF696B) : Color::rgb(0xB51F25);
    mixer[ColorId::Separator] = withAlpha(ColorId::Text, dark ? 0.14 : 0.09);
    mixer[ColorId::HeaderSeparator] = withAlpha(ColorId::Text, dark ? 0.12 : 0.08);
    mixer[ColorId::ItemPressed] = withAlpha(ColorId::Text, dark ? 0.05 : 0.1);
    // Inactive windows keep the blur and halve the see-through instead of going opaque.
    mixer[ColorId::TranslucentWindowBackground] = withAlpha(ColorId::Background, dark ? 0.85 : 0.8);
    mixer[ColorId::TranslucentWindowBackgroundInactive] = withAlpha(ColorId::Background, dark ? 0.92 : 0.9);

    mixer[ColorId::ButtonBackground] = withAlpha(ColorId::Text, dark ? .10 : .065);
    mixer[ColorId::ButtonBackgroundHovered] = withAlpha(ColorId::Text, dark ? .15 : .10);
    mixer[ColorId::ButtonBackgroundPressed] = withAlpha(ColorId::Text, dark ? .21 : .15);
    mixer[ColorId::ButtonFocusRing] = withAlpha(ColorId::Highlight, 0.5);

    mixer[ColorId::ComboBoxBackground] = ColorId::AlternateBackground;
    mixer[ColorId::ComboBoxBackgroundPressed] = dark ? lighter(ColorId::ComboBoxBackground, 1.1)
                                                     : darker(ColorId::ComboBoxBackground, 1.2);
    mixer[ColorId::ComboBoxFocusBorder] = ColorId::Highlight;

    mixer[ColorId::ToggleBackground] = ColorId::SecondBackground;
    mixer[ColorId::ToggleBackgroundChecked] = ColorId::Highlight;
    mixer[ColorId::ToggleBorder] = withAlpha(ColorId::Text, dark ? 0.28 : 0.2);
    mixer[ColorId::ToggleBorderChecked] = darker(ColorId::Highlight, 1.12);
    mixer[ColorId::ToggleFocusRing] = withAlpha(ColorId::Highlight, 0.35);
    mixer[ColorId::CheckBoxInnerEdge] = kWhite.withAlpha(0.06);
    mixer[ColorId::CheckBoxInnerEdgeChecked] = kWhite.withAlpha(0.18);
    mixer[ColorId::ToggleMark] = ColorId::HighlightedText;

    mixer[ColorId::SwitchTrack] = withAlpha(ColorId::Text, 0.2);
    mixer[ColorId::SwitchTrackOn] = ColorId::Highlight;
    mixer[ColorId::SwitchHandle] = kWhite;

    mixer[ColorId::SliderTrack] = dark ? kWhite.withAlpha(0.3) : kBlack.withAlpha(0.1);
    mixer[ColorId::SliderTrackFilled] = ColorId::Highlight;
    mixer[ColorId::SliderTickMark] = dark ? kWhite.withAlpha(0.55) : kBlack.withAlpha(0.35);
    mixer[ColorId::SliderHandle] = kWhite;
    mixer[ColorId::SliderHandlePressed] = Color::rgb(0xFAFAFA);
    mixer[ColorId::SliderHandleBorder] = kBlack.withAlpha(0.04);
    mixer[ColorId::SliderHandleShadow] = kBlack.withAlpha(0.1);
    mixer[ColorId::SliderHandleShadowPressed] = kBlack.withAlpha(0.15);

    mixer[ColorId::SegmentedControlTrack] = dark ? kWhite.withAlpha(0.06) : kBlack.withAlpha(0.04);
    mixer[ColorId::SegmentedControlPlate] = dark ? lighter(ColorId::SecondBackground, 1.52)
                                                 : ColorRecipe(kWhite);
    mixer[ColorId::SegmentedControlPlateShadow] = kBlack.withAlpha(dark ? 0.18 : 0.12);
    mixer[ColorId::SegmentedControlTextPressed] = withAlpha(ColorId::Text, 0.5);
    mixer[ColorId::SegmentedTabBackground] = ColorId::AlternateBackground;
    mixer[ColorId::SegmentedTabPlateShadow] = dark ? kBlack.withAlpha(0.11) : kTransparent;

    mixer[ColorId::ScrollBarThumb] = dark ? kWhite.withAlpha(0.5) : kBlack.withAlpha(0.4);
    mixer[ColorId::ScrollBarThumbDragging] = dark ? kWhite.withAlpha(0.4) : kBlack.withAlpha(0.5);

    mixer[ColorId::TextFieldPlaceholder] = withAlpha(ColorId::Text, 0.4);
    mixer[ColorId::TextFieldFocusRing] = withAlpha(ColorId::Highlight, 0.25);

    mixer[ColorId::RoundButtonHovered] = withAlpha(ColorId::Text, dark ? 0.12 : 0.11);
    mixer[ColorId::RoundButtonPressed] = withAlpha(ColorId::Text, dark ? 0.06 : 0.21);

    mixer[ColorId::HeaderButtonTint] = dark ? kWhite : Color::rgb(0x323238);

    mixer[ColorId::AlertBackground] = dark ? ColorRecipe(WallpaperTint::apply(Color::rgb(0x252525), wallpaperTone))
                                          : ColorRecipe(ColorId::PanelBackground);
    mixer[ColorId::AlertText] = (dark ? kWhite : kBlack).withAlpha(0.847);
    // Flat fills with no edge: white in dark mode and black in light.
    mixer[ColorId::DialogButtonBackground] = (dark ? kWhite : kBlack).withAlpha(0.0756);
    mixer[ColorId::DialogButtonBackgroundPressed] = (dark ? kWhite : kBlack).withAlpha(0.165);
    mixer[ColorId::DialogButtonProminentText] = kWhite;
    mixer[ColorId::DialogButtonDestructiveText] = ColorId::DestructiveAction;
    mixer[ColorId::DialogButtonDestructiveBackground] =
        (dark ? Color::rgb(0xFF4245) : Color::rgb(0xFF383C)).withAlpha(dark ? .20 : .13);
    mixer[ColorId::DialogButtonFocusRing] = withAlpha(ColorId::Highlight, 0.3);

    // The kit's selected row uses a stronger blue than the system accent.
    mixer[ColorId::MenuHighlight] = theme.highlight == (dark ? accent::blueDark : accent::blue)
        ? ColorRecipe(Color::rgb(0x0069FA)) : darker(ColorId::Highlight, 1.22);
    mixer[ColorId::MenuSeparator] = dark ? kWhite.withAlpha(0.14) : kBlack.withAlpha(0.07);
    mixer[ColorId::MenuDisabledText] = withAlpha(ColorId::Text, 0.4);
    mixer[ColorId::MenuShortcutText] = withAlpha(ColorId::Text, 0.5);
    // Dark glass over a bright backdrop washes out a grey chevron, so in
    // dark mode it takes the label's colour.
    mixer[ColorId::MenuArrow] = dark ? ColorId::Text : ColorId::DisabledText;
    mixer[ColorId::MenuArrowDisabled] = dark ? ColorId::MenuDisabledText : ColorId::DisabledText;
    mixer[ColorId::MenuScrollIndicator] = dark ? kWhite.withAlpha(0.35) : kBlack.withAlpha(0.25);

    mixer[ColorId::WindowTabBarTrack] = dark ? kWhite.withAlpha(0.075) : kBlack.withAlpha(0.065);
    // Light schemes are not always white; a translucent pill lifts off any of them.
    mixer[ColorId::WindowTabBarSelected] = kWhite.withAlpha(dark ? 0.16 : 0.92);
    // Light mode raises a white pill on a pale track; a hairline keeps its edge.
    mixer[ColorId::WindowTabBarSelectedEdge] = dark ? kTransparent : Color::fromArgb(0x0F000000);

    mixer[ColorId::ToolTipBackground] = ColorId::SecondBackground;
    mixer[ColorId::ToolTipShadow] = kBlack.withAlpha(0.15);
    mixer[ColorId::ToastShadow] = kBlack.withAlpha(0.14);
}

} // namespace cutefish::ui
