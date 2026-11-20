// SPDX-License-Identifier: BSD-3-Clause

#include "ui/toolbar_surface.h"

#include <algorithm>
#include <cstdio>
#include <utility>

#include "include/core/SkCanvas.h"
#include "include/effects/SkRuntimeEffect.h"

#include "ui/painter.h"
#include "ui/theme.h"
#include "ui/view_glass.h"
#include "ui/window.h"

namespace cutefish::ui::toolbar_surface {

// Distance and normal come from the same round-ended shape. The narrow
// specular layer follows the bevel, not the height of the bounding box.
static constexpr char kCapsuleShader[] = R"(
uniform float2 centre;
uniform float2 axis;
uniform float radius;
uniform float scale;
uniform float darkMode;
uniform float recessed;
uniform float frosting;
uniform float opaqueMaterial;
layout(color) uniform half4 tint;

half4 main(float2 xy)
{
    float2 p = xy - centre;
    float2 radial = p - clamp(p, -axis, axis);
    float len = length(radial);
    float d = len - radius;
    float coverage = clamp(0.5 - d, 0.0, 1.0);
    float depth = max(0.0, -d) / scale;
    float2 n = radial / max(len, 0.001);
    float upper = pow(max(dot(n, float2(-0.28, -0.96)), 0.0), 3.0);
    float lower = pow(max(dot(n, float2(0.28, 0.96)), 0.0), 4.0);
    float rim = 1.0 - smoothstep(0.42, 1.32, depth);
    float bevel = smoothstep(0.65, 1.30, depth) * (1.0 - smoothstep(1.30, 2.55, depth));
    float y = clamp(0.5 + p.y / (2.0 * radius + 2.0 * axis.y), 0.0, 1.0);

    float ink;
    float light;
    if (recessed > 0.5) {
        // The search wells catch light on the lower inside edge; the upper
        // shoulder darkens instead of catching the raised capsule's shine.
        float well = 1.0 - smoothstep(0.55, 5.0, depth);
        ink = darkMode > 0.5 ? 0.12 + well * (0.14 * upper) : 0.018 + well * (0.070 * upper);
        light = darkMode > 0.5 ? rim * (0.10 + 0.06 * lower) + bevel * (0.06 * lower)
                              : rim * (0.30 + 0.31 * upper) + bevel * (0.38 * lower);
    } else if (darkMode > 0.5) {
        // No broad white streaks in the dark material. Keep the interior
        // almost level, with a small highlight even on the dark backdrop.
        ink = rim * (0.10 - 0.08 * upper);
        light = mix(0.085, 0.068, y) + rim * (0.045 + 0.085 * upper + 0.070 * lower)
              + bevel * (0.028 * upper + 0.023 * lower);
    } else {
        // In light mode the shoulder is softly shaded above a near-white
        // lower face; the specular highlight follows the inner bevel.
        ink = mix(0.031, 0.007, smoothstep(0.18, 0.74, y))
              + rim * (0.033 + 0.012 * (1.0 - upper));
        light = mix(0.11, 0.16, y) + bevel * (0.28 * upper + 0.15 * lower);
    }
    // Black over white, with the result premultiplied and clipped only once.
    float rgb = light * (1.0 - ink);
    half4 surface = half4(half3(rgb), half(ink + rgb));
    half4 base = half4(tint.rgb * tint.a, tint.a);
    if (opaqueMaterial > 0.5)
        base = half4(half3(darkMode > 0.5 ? 0.20 : 0.93), 1.0);
    else {
        float frost = max(frosting, 0.0) * (darkMode > 0.5 ? 0.32 : 0.37);
        half4 ground = half4(half3((darkMode > 0.5 ? 0.17 : 1.0) * frost), frost);
        base = base + ground * (1.0 - base.a);
        surface *= half(1.0 + min(frosting, 0.0) * 0.49);
    }
    return (surface + base * (1.0 - surface.a)) * half(coverage);
}
)";

static void material(Painter& painter, const RectF& box, bool dark, bool recessed, Color tint)
{
    if (box.width <= 0 || box.height <= 0)
        return;
    static const sk_sp<SkRuntimeEffect> effect = [] {
        auto result = SkRuntimeEffect::MakeForShader(SkString(kCapsuleShader));
        if (!result.effect)
            std::fprintf(stderr, "cutefish-toolkit: toolbar capsule: %s\n", result.errorText.c_str());
        return result.effect;
    }();
    const Rect boxPx = painter.deviceRect(box);
    if (boxPx.width <= 0 || boxPx.height <= 0)
        return;
    const float radius = std::min(boxPx.width, boxPx.height) * 0.5f;
    sk_sp<SkShader> shader;
    if (effect) {
        SkRuntimeShaderBuilder builder(effect);
        builder.uniform("centre") = SkV2{boxPx.x + boxPx.width * 0.5f, boxPx.y + boxPx.height * 0.5f};
        builder.uniform("axis") = SkV2{boxPx.width * 0.5f - radius, boxPx.height * 0.5f - radius};
        builder.uniform("radius") = radius;
        builder.uniform("scale") = float(painter.scale());
        builder.uniform("darkMode") = dark ? 1.0f : 0.0f;
        builder.uniform("recessed") = recessed ? 1.0f : 0.0f;
        const auto& style = Theme::appearanceStyle();
        builder.uniform("frosting") = float(2 * style.glass - 1);
        builder.uniform("opaqueMaterial") = style.reduceTransparency ? 1.0f : 0.0f;
        builder.uniform("tint") = SkColor4f{tint.r / 255.f, tint.g / 255.f, tint.b / 255.f, tint.a / 255.f};
        shader = builder.makeShader();
    }
    if (!shader) {
        const Color fill = recessed ? Color::rgb(0).withAlpha(dark ? 0.12 : 0.025)
                                    : Color::rgb(0xffffff).withAlpha(dark ? 0.075 : 0.24);
        painter.fillDeviceRoundedRect(boxPx, radius, tint.a ? tint : fill);
        painter.strokeDeviceRoundedRect(boxPx, radius,
            Color::rgb(dark ? 0xffffff : 0x000000).withAlpha(0.10), 1);
        return;
    }
    SkPaint paint;
    paint.setShader(std::move(shader));
    paint.setAlphaf(float(painter.opacity()));
    painter.canvas()->drawIRect(SkIRect::MakeXYWH(boxPx.x, boxPx.y, boxPx.width, boxPx.height), paint);
}

ViewGlass previewGlass(const RectF& box, double strength, bool reduceTransparency)
{
    const auto t = appearance_style::glassValue(strength);
    ViewGlass pane;
    pane.rect = box;
    pane.radius = box.height / 2;
    pane.blur = std::lerp(0.7, 8.0, t);
    pane.bezel = std::min(7.0, box.height / 4);
    pane.bend = std::lerp(6.0, 1.5, t);
    pane.dispersion = .045;
    pane.saturation = std::lerp(1.12, .56, t);
    pane.tint[3] = reduceTransparency ? 1 : std::lerp(.08, .62, t);
    pane.edge[3] = .28;
    pane.edge[0] = pane.edge[1] = pane.edge[2] = 1;
    pane.sideEdge[3] = .09;
    return pane;
}

void applyWindowGlass(Window& window)
{
    const auto& style = Theme::appearanceStyle();
    if (style.reduceTransparency) {
        window.setBackgroundBlur({}, 0);
        window.setBackgroundGlass(std::nullopt);
        return;
    }
    const auto size = window.size();
    window.setBackgroundBlur({0, 0, size.width, size.height}, Theme::current().windowRadius);
    const auto t = appearance_style::glassValue(style.glass);
    GlassMaterial glass;
    glass.blurSigma = std::lerp(4.0, 14.0, t);
    glass.displacement = std::lerp(12.0, 3.0, t);
    glass.bevel = 8;
    glass.depth = 6;
    glass.chroma = .018;
    glass.tintAlpha = std::lerp(0.0, .18, t);
    glass.saturation = std::lerp(1.0, .72, t);
    if (Theme::current().darkMode)
        glass.tintRed = glass.tintGreen = glass.tintBlue = .12;
    window.setBackgroundGlass(glass);
}

void capsule(Painter& painter, const RectF& box, bool dark)
{
    material(painter, box, dark, false, {0, 0, 0, 0});
}

void searchField(Painter& painter, const RectF& box, bool dark, std::optional<Color> tint)
{
    material(painter, box, dark, true, tint.value_or(Color{0, 0, 0, 0}));
}

void searchFocus(Painter& painter, const RectF& box, Color accent)
{
    if (box.width <= 0 || box.height <= 0)
        return;
    painter.drawSmoothRectangle(box.adjusted(-1, -1, 1, 1), std::min(box.width, box.height) / 2 + 1,
                                {0, 0, 0, 0}, 2, accent.withAlpha(0.72));
}

void highlight(Painter& painter, const RectF& box, bool dark, bool pressed, bool selected)
{
    if (box.width <= 0 || box.height <= 0)
        return;
    const auto ink = dark ? Color::rgb(0xffffff) : Color::rgb(0x000000);
    const double opacity = pressed ? (dark ? 0.24 : 0.17) : selected ? (dark ? 0.165 : 0.115) : (dark ? 0.085 : 0.065);
    painter.fillRoundedRect(box, std::min(box.height, box.width) / 2, ink.withAlpha(opacity));
}

} // namespace cutefish::ui::toolbar_surface
