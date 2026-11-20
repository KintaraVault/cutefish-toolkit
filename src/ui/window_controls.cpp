// SPDX-FileCopyrightText: 2026 Reion Wong <reionwong@gmail.com>
// SPDX-License-Identifier: BSD-3-Clause

#include "ui/window_controls.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <utility>
#include <vector>

#include "include/core/SkBitmap.h"
#include "include/core/SkCanvas.h"
#include "include/core/SkImage.h"
#include "include/effects/SkRuntimeEffect.h"

#include "ui/bitmap_private.h"
#include "ui/icon_loader.h"
#include "ui/painter.h"
#include "ui/path.h"

namespace cutefish::ui::window_controls {

static constexpr double kColoredPlate = 14;
static constexpr double kTrafficGlyph = 10;
static constexpr double kArtwork = 14;

// The shadow, the glyph and the coloured dot keep their proportions to a
// control's side.
static double shadowSigma(double side)
{
    return side * 0.1;
}

static double shadowOffset(double side)
{
    return side * 0.06;
}

static double scaled(double length, double side)
{
    return std::round(length * side / kButton);
}

static double sideOf(const RectF& bounds)
{
    return std::min(bounds.width, bounds.height);
}

// The plate under a control or a group of them: barely apart from the
// window, lit a little in its middle and along the inside of its top and
// bottom edges, a hairline rim darkest at the sides, and a faint shadow under
// it. `overlay` darkens it, or lightens it in dark mode, by that alpha.
static void paintPlateBody(Painter& painter, const RectF& plate, bool dark, double overlay)
{
    const double radius = plate.height / 2;
    const Color white = Color::rgb(0xFFFFFF);
    const Color black = Color::rgb(0x000000);
    const auto vertical = [&plate](std::vector<GradientStop> stops) {
        return LinearGradient {{plate.x, plate.y}, {plate.x, plate.bottom()}, std::move(stops)};
    };

    painter.drawOuterShadow(plate, radius, Corners::Circular, shadowSigma(plate.height),
                            black.withAlpha(dark ? 0.2 : 0.04), {0, shadowOffset(plate.height)});

    painter.save();
    painter.clipRoundedRect(plate, radius);
    // Lightest in the middle of its lower half.
    const double body = dark ? 0.25 : 1.0;
    painter.fillRoundedRect(plate, radius,
                            LinearGradient {{plate.x, plate.y},
                                            {plate.right(), plate.y},
                                            {{0, white.withAlpha(body * 0.04)},
                                             {0.5, white.withAlpha(body * 0.2)},
                                             {1, white.withAlpha(body * 0.04)}}});
    painter.fillRoundedRect(plate, radius,
                            vertical({{0.3, white.withAlpha(0)}, {0.75, white.withAlpha(body * 0.32)}}));
    const double shine = dark ? 0.3 : 1.0;
    painter.fillRoundedRect(plate, radius,
                            vertical({{0, white.withAlpha(shine)},
                                      {0.035, white.withAlpha(shine)},
                                      {0.07, white.withAlpha(shine * 0.5)},
                                      {0.12, white.withAlpha(shine * 0.25)},
                                      {0.25, white.withAlpha(0)},
                                      {0.85, white.withAlpha(0)},
                                      {0.93, white.withAlpha(shine * 0.3)},
                                      {0.955, white.withAlpha(shine * 0.6)},
                                      {0.97, white.withAlpha(shine)},
                                      {1, white.withAlpha(shine)}}));
    if (overlay > 0) {
        const Color tint = dark ? white : black;
        painter.fillRoundedRect(plate, radius, vertical({{0, tint.withAlpha(overlay)}, {1, tint.withAlpha(overlay)}}));
    }
    painter.restore();

    // One device pixel wide.
    const double hairline = 1 / painter.scale();
    Path rim;
    rim.setFillRule(Path::FillRule::EvenOdd);
    if (plate.width == plate.height) {
        rim.addEllipse(plate);
        rim.addEllipse(plate.adjusted(hairline, hairline, -hairline, -hairline));
    } else {
        rim.addRoundedRect(plate, radius, radius);
        rim.addRoundedRect(plate.adjusted(hairline, hairline, -hairline, -hairline), radius - hairline,
                           radius - hairline);
    }
    const Color edge = dark ? white : black;
    painter.fillPath(rim, vertical({{0, edge.withAlpha(dark ? 0.2 : 0.1)},
                                    {0.5, edge.withAlpha(dark ? 0.12 : 0.32)},
                                    {1, edge.withAlpha(dark ? 0.08 : 0.14)}}));
}

// `bounds` on whole device pixels, in logical coordinates.
static RectF snappedPlate(Painter& painter, const RectF& bounds)
{
    const double scale = painter.scale();
    const Rect device = painter.snappedRect(bounds);
    const PointF offset = painter.offset();
    return {device.x / scale - offset.x, device.y / scale - offset.y, device.width / scale, device.height / scale};
}

static void paintPlate(Painter& painter, const RectF& bounds, const State& state)
{
    // A square of whole device pixels, so the plate comes out round.
    const double side = sideOf(bounds);
    const RectF plate = snappedPlate(painter, {bounds.x + std::round((bounds.width - side) / 2),
                                               bounds.y + std::round((bounds.height - side) / 2), side, side});
    const Rect device = painter.deviceRect(plate);

    painter.save();
    painter.multiplyOpacity(state.enabled ? 1.0 : 0.35);
    const bool lit = state.enabled && (state.hovered || state.pressed);
    paintPlateBody(painter, plate, state.dark, lit ? (state.pressed ? 0.08 : 0.04) : 0);

    const Color ink = inkColor(state.dark, state.active || state.hovered);
    if (state.artwork == "dialog-close") {
        sk_sp<SkImage> image = IconLoader::instance().glyph("window-control-dialog-close", device.width, ink);
        painter.drawImage(image.get(), device.x, device.y);
    } else if (!state.artwork.empty()) {
        const double artwork = scaled(kArtwork, side);
        const Rect icon = painter.centeredSquare(bounds, artwork);
        sk_sp<SkImage> image = IconLoader::instance().centeredGlyph("window-control-" + state.artwork, icon.width, ink);
        painter.drawImage(image.get(), icon.x, icon.y);
    }
    painter.restore();
}

static constexpr char kTrafficLightShader[] = R"(
uniform float2 centre;
uniform float radius;
uniform float scale;
layout(color) uniform half4 topColor;
layout(color) uniform half4 bottomColor;
uniform float pressed;
uniform float darkMode;
uniform float colored;

half4 main(float2 xy)
{
    float2 p = xy - centre;
    float radial = length(p);
    float depth = (radius - radial) / scale;
    float coverage = clamp(0.5 + depth * scale, 0.0, 1.0);
    float2 normal = p / max(radial, 0.001);
    float up = max(-normal.y, 0.0);
    float down = max(normal.y, 0.0);
    float side = abs(normal.x);
    float rim = 1.0 - smoothstep(0.08, 1.00, depth);
    float bevel = smoothstep(0.28, 0.88, depth) * (1.0 - smoothstep(0.88, 1.70, depth));
    float y = clamp(0.5 + p.y / (2.0 * radius), 0.0, 1.0);

    // The dyed body scatters light toward the lower half. Its darker contour
    // pinches in at the sides; a short specular arc follows the upper rim.
    half3 body = mix(topColor.rgb, bottomColor.rgb, half(smoothstep(0.09, 0.91, y)));
    float absorption = rim * (0.11 + 0.26 * side * side + 0.05 * down);
    float neutralDark = darkMode * (1.0 - colored);
    absorption *= 1.0 - 0.62 * neutralDark;
    body = mix(body, half3(0.0), half(absorption + pressed * 0.15));
    float crown = up * up * up;
    float heel = down * down;
    float reflection = bevel * (0.035 * crown + 0.07 * heel)
        + rim * (0.075 * crown + 0.026 * heel)
        + neutralDark * (rim * 0.12 * crown + bevel * 0.11 * heel);
    body = mix(body, half3(1.0), half(reflection * (1.0 - pressed * 0.40)));

    // Small contact shadow under the lens, not the toolbar capsule's fill.
    float shadowDepth = (length(p - float2(0.0, scale * 0.48)) - radius) / scale;
    float shadow = (1.0 - smoothstep(-0.3, 1.55, shadowDepth)) * (0.12 + darkMode * 0.08);
    return half4(body * half(coverage), half(coverage + shadow * (1.0 - coverage)));
}
)";

static void paintTrafficLens(Painter& painter, const Rect& plate, Color top, Color bottom, const State& state,
                             bool colored)
{
    static const sk_sp<SkRuntimeEffect> effect = [] {
        auto result = SkRuntimeEffect::MakeForShader(SkString(kTrafficLightShader));
        if (!result.effect)
            std::fprintf(stderr, "cutefish-toolkit: traffic light: %s\n", result.errorText.c_str());
        return result.effect;
    }();
    const float radius = plate.width * 0.5f;
    sk_sp<SkShader> shader;
    if (effect) {
        const auto rgba = [](Color c) { return SkColor4f{c.r / 255.f, c.g / 255.f, c.b / 255.f, 1}; };
        SkRuntimeShaderBuilder builder(effect);
        builder.uniform("centre") = SkV2{plate.x + radius, plate.y + radius};
        builder.uniform("radius") = radius;
        builder.uniform("scale") = float(painter.scale());
        builder.uniform("topColor") = rgba(top);
        builder.uniform("bottomColor") = rgba(bottom);
        builder.uniform("pressed") = state.pressed ? 1.0f : 0.0f;
        builder.uniform("darkMode") = state.dark ? 1.0f : 0.0f;
        builder.uniform("colored") = colored ? 1.0f : 0.0f;
        shader = builder.makeShader();
    }
    if (!shader) {
        painter.fillDeviceRoundedRect(plate, radius, top);
        painter.strokeDeviceRoundedRect(plate, radius, Color::rgb(0).withAlpha(0.14), 1);
        return;
    }
    SkPaint paint;
    paint.setShader(std::move(shader));
    paint.setAlphaf(float(painter.opacity()));
    const int reach = int(std::ceil(2.1 * painter.scale()));
    auto area = SkIRect::MakeXYWH(plate.x, plate.y, plate.width, plate.height);
    area.outset(reach, reach);
    painter.canvas()->drawIRect(area, paint);
}

static void paintColored(Painter& painter, const RectF& bounds, const State& state)
{
    const double side = sideOf(bounds);
    const Rect plate = painter.centeredSquare(bounds, scaled(kColoredPlate, side));
    const bool colored = (state.active || state.hovered || state.pressed) && state.enabled;
    uint32_t top = state.dark ? 0x555455 : 0xcfcecd;
    uint32_t bottom = state.dark ? 0x494849 : 0xe1dfde;
    if (colored && state.artwork == "close") {
        top = 0xfd6c62;
        bottom = 0xf38a82;
    } else if (colored && state.artwork == "minimize") {
        top = 0xfbb825;
        bottom = 0xfed343;
    } else if (colored) {
        top = 0x5ace28;
        bottom = 0x98d975;
    }
    paintTrafficLens(painter, plate, Color::rgb(top), Color::rgb(bottom), state, colored);
    if (state.enabled && !state.artwork.empty() && (state.hovered || state.pressed)) {
        const Rect icon = painter.centeredSquare(bounds, scaled(kTrafficGlyph, side));
        sk_sp<SkImage> image = IconLoader::instance().centeredGlyph("window-control-" + state.artwork, icon.width,
                                                           Color::rgb(0).withAlpha(0.72));
        painter.drawImage(image.get(), icon.x, icon.y);
    }
}

void paint(Painter& painter, const RectF& bounds, const State& state)
{
    if (state.colored && state.artwork != "dialog-close")
        paintColored(painter, bounds, state);
    else
        paintPlate(painter, bounds, state);
}

Color inkColor(bool dark, bool strong)
{
    return (dark ? Color::rgb(0xFFFFFF) : Color::rgb(0x0C0C0C)).withAlpha(strong ? 1.0 : 0.5);
}

void paintGroupPlate(Painter& painter, const RectF& bounds, bool dark)
{
    paintPlateBody(painter, snappedPlate(painter, bounds), dark, 0);
}

RectF visualRect(const RectF& bounds, const State& state)
{
    if (state.colored && state.artwork != "dialog-close")
        return bounds;
    // A sigma and a half, past which the shadow is under a level; it stays
    // inside the gap to the next control.
    const double side = sideOf(bounds);
    const double reach = 1.5 * shadowSigma(side);
    return bounds.adjusted(-std::ceil(reach), -std::ceil(reach - shadowOffset(side)), std::ceil(reach),
                           std::ceil(reach + shadowOffset(side)));
}

Bitmap render(const State& state, double scale, Bitmap::Format format)
{
    const int side = int(std::lround((kButton + 2 * kShadowMargin) * scale));
    Bitmap bitmap(side, side, format);
    if (bitmap.isNull())
        return bitmap;
    SkBitmap target;
    const SkImageInfo info = SkImageInfo::Make(side, side, skColorType(format), kPremul_SkAlphaType);
    if (!target.installPixels(info, bitmap.bits(), bitmap.stride()))
        return {};
    SkCanvas canvas(target);
    Painter painter(&canvas, scale);
    paint(painter, {kShadowMargin, kShadowMargin, kButton, kButton}, state);
    return bitmap;
}

} // namespace cutefish::ui::window_controls
