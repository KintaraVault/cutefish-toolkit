// SPDX-FileCopyrightText: 2026 Reion Wong <reionwong@gmail.com>
// SPDX-License-Identifier: BSD-3-Clause

#include <gtest/gtest.h>

#include "ui/header_button.h"
#include "ui/icon.h"
#include "ui/layout.h"
#include "ui/navigation_buttons.h"
#include "ui/painter.h"
#include "ui/path.h"
#include "ui/scroll_bar.h"
#include "ui/search_field.h"
#include "ui/status_item.h"
#include "ui/subsurface.h"
#include "ui/test_input.h"
#include "ui/theme.h"
#include "ui/toolbar.h"
#include "ui/window.h"
#include "ui/window_controls.h"
#include "ui/window_header.h"

using namespace cutefish::ui;

TEST(SubsurfaceTest, HeadlessWindowsHaveNone)
{
    Window window;
    window.resize({200, 100});
    window.setContentView(std::make_unique<View>());
    window.show();
    Subsurface subsurface(&window);
    EXPECT_FALSE(subsurface.isValid());
    subsurface.setGeometry({0, 0, 200, 100});
    EXPECT_EQ(subsurface.geometry(), (RectF{0, 0, 200, 100}));
    EXPECT_TRUE(subsurface.beginFrame().isEmpty());
    EXPECT_FALSE(subsurface.endFrame());
    EXPECT_FALSE(subsurface.waitForFrame(0));
    EXPECT_FALSE(subsurface.makeCurrent());
    subsurface.setStacking(Subsurface::Stacking::Above);
    EXPECT_FALSE(subsurface.isValid());
    EXPECT_EQ(Subsurface::waylandDisplay(), nullptr);
    // A frame of the window still renders.
    EXPECT_GT(window.grab().width(), 0);
}

TEST(WindowHintsTest, GlassAndShapeAreAcceptedEverywhere)
{
    Window window;
    window.resize({300, 200});
    window.setContentView(std::make_unique<View>());
    window.show();
    window.setBackgroundBlur({0, 0, 300, 200}, 0);
    Path shape;
    shape.addRoundedRect({10, 120, 280, 70}, 12, 12);
    shape.addRoundedRect({200, 10, 90, 100}, 12, 12);
    window.setBackgroundBlurShape(shape);
    GlassMaterial material;
    material.blurSigma = 40;
    material.displacement = 0;
    material.chroma = 0;
    material.saturation = 0.65;
    material.tintRed = 0.07;
    material.tintGreen = 0.07;
    material.tintBlue = 0.08;
    material.tintAlpha = 0.62;
    window.setBackgroundGlass(material);
    window.setBackgroundGlass(std::nullopt);
    window.setResizeAroundCenter(true);
    window.setRestoreId("main");
    window.setPlacementAnchor(PlacementAnchor{{700, 12, 0, 0}, PopupAnchor::Bottom, PopupAnchor::Bottom});
    window.setPlacementAnchor(std::nullopt);
    EXPECT_GT(window.grab().width(), 0);
}

// The close button's plate, sampled at its middle.
static Color closeGlyphColor(Window& window, double headerHeight)
{
    const Image image = window.grab();
    const int x = int(7 + 13);
    const int y = int(headerHeight == 44 ? 9 + 13 : 7 + 13);
    return image.pixel(x, y);
}

TEST(WindowHintsTest, HeaderDarkModeTintsTheButtonsWhateverTheTheme)
{
    const bool wasDark = Theme::current().darkMode;
    Window window;
    auto column = std::make_unique<Column>();
    WindowHeader* header = column->addChild<WindowHeader>(44);
    header->setButtonsCentered(true);
    header->setLayoutHints({.fillWidth = true});
    window.resize({300, 200});
    window.setBackgroundColor(Color::rgb(0x808080));
    window.setContentView(std::move(column));
    window.show();
    // Unfocused traffic lights are neutral and match the header appearance.
    TestInput::configure(window, 300, 200, {});

    EXPECT_EQ(window.headerDarkMode(), wasDark);
    window.setHeaderDarkMode(true);
    EXPECT_TRUE(window.headerDarkMode());
    const Color dark = closeGlyphColor(window, 44);
    window.setHeaderDarkMode(false);
    EXPECT_FALSE(window.headerDarkMode());
    const Color light = closeGlyphColor(window, 44);
    EXPECT_LT(dark.r, 0x70);
    EXPECT_GT(light.r, 0xc0);
    EXPECT_LT(std::abs(int(dark.r) - int(dark.b)), 5);
    EXPECT_LT(std::abs(int(light.r) - int(light.b)), 5);
    window.setHeaderDarkMode(std::nullopt);
    EXPECT_EQ(window.headerDarkMode(), wasDark);
}

TEST(WindowHintsTest, ToolbarGlassSeparatesRaisedButtonsAndRecessedSearch)
{
    Window window;
    window.resize({180, 130});
    window.setScale(1.5);
    window.setHeaderDarkMode(true);
    window.setBackgroundColor(Color::rgb(0x222222));
    auto column = std::make_unique<Column>();
    auto* navigation = column->addChild<NavigationButtons>();
    navigation->setLayoutHints({.fillWidth = true});
    auto* search = column->addChild<SearchField>();
    search->setLayoutHints({.fillWidth = true, .minimumHeight = 30});
    window.setContentView(std::move(column));
    window.show();
    const Image image = window.grab();
    const auto sample = [&](const View& view, double x, double y) {
        return int(image.pixel(int((view.geometry().x + x) * 1.5), int((view.geometry().y + y) * 1.5)).r);
    };
    EXPECT_GT(sample(*navigation, 64, 10), 0x22);
    EXPECT_LT(sample(*search, 64, 10), 0x22);
    EXPECT_GT(sample(*search, 64, search->geometry().height - 0.8), sample(*search, 64, 10));
}

TEST(WindowHintsTest, CenteredHeaderButtons)
{
    Window window;
    auto column = std::make_unique<Column>();
    WindowHeader* header = column->addChild<WindowHeader>(44);
    header->setButtonsCentered(true);
    header->setLayoutHints({.fillWidth = true});
    window.resize({300, 200});
    window.setContentView(std::move(column));
    window.show();
    window.grab();
    // Close, minimize and maximize from the start, 9 px from it, 11 px down,
    // 1 apart for the default traffic lights.
    const std::string dump = window.layoutDump();
    EXPECT_NE(dump.find("logical 9 11 68 22"), std::string::npos) << dump;
    EXPECT_NE(dump.find("device 9 11 22 22"), std::string::npos) << dump;
    EXPECT_NE(dump.find("device 32 11 22 22"), std::string::npos) << dump;
    EXPECT_NE(dump.find("device 55 11 22 22"), std::string::npos) << dump;
}

TEST(WindowHintsTest, ToolbarGlyphsStayCentredAtFractionalScale)
{
    NavigationButtons navigation;
    navigation.setGeometry({0, 0, 74, 35});
    navigation.back()->setGlyph("chevron-left", 19);
    navigation.forward()->setGlyph("chevron-right", 19);
    for (const double scale : {1.0, 1.25, 1.5, 2.0}) {
        SCOPED_TRACE(scale);
        for (auto* button : {navigation.back(), navigation.forward()}) {
            button->setGeometry({0, 0, 37, 35});
            const Rect ink = TestInput::inkRect(*button, scale);
            ASSERT_GT(ink.width, 0);
            EXPECT_NEAR(ink.x + ink.width / 2.0, 37 * scale / 2, 1.0);
            EXPECT_NEAR(ink.y + ink.height / 2.0, 35 * scale / 2, 1.0);
        }
    }
}

TEST(WindowHintsTest, ToolbarCorrectsUnevenSvgPaddingBeforeRasterizing)
{
    Icon::registerGlyph("offset-toolbar-test", R"(<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 16 16">
        <rect x="8" y="10" width="5" height="3" rx="1" fill="currentColor"/></svg>)");
    HeaderButton button;
    button.setGlyph("offset-toolbar-test", 16);
    button.setGeometry({0, 0, 35, 37});
    for (const double scale : {1.0, 1.25, 1.5, 2.0}) {
        SCOPED_TRACE(scale);
        const Rect ink = TestInput::inkRect(button, scale);
        ASSERT_GT(ink.width, 0);
        EXPECT_NEAR(ink.x + ink.width / 2.0, 35 * scale / 2, 1.0);
        EXPECT_NEAR(ink.y + ink.height / 2.0, 37 * scale / 2, 1.0);
    }
}

TEST(WindowHintsTest, ToolbarDropdownCentresGlyphAndChevronTogether)
{
    Window window;
    auto column = std::make_unique<Column>();
    auto* toolbar = column->addChild<Toolbar>();
    toolbar->setName("toolbar");
    toolbar->setItems({{"group", "Group", "view-group", 43, {}, {}, true}});
    toolbar->setOrder({"group"});
    toolbar->setLayoutHints({.preferredHeight = 40});
    window.resize({180, 80});
    window.setContentView(std::move(column));
    window.show();
    window.grab();
    auto* item = toolbar->itemView("group");
    ASSERT_TRUE(item);
    ASSERT_GE(item->children().size(), 2u);
    const RectF glyph = item->children()[0]->geometry();
    const RectF chevron = item->children()[1]->geometry();
    EXPECT_EQ(glyph.x, item->geometry().width - chevron.right());
    EXPECT_EQ(chevron.x - glyph.right(), 2);
    EXPECT_EQ(glyph.y + glyph.height / 2, 20);
    EXPECT_EQ(chevron.y + chevron.height / 2, 20);
}

TEST(WindowHintsTest, TrafficLightHasDyedGlassAndASoftContactShadow)
{
    auto state = window_controls::State{.artwork = "close", .colored = true, .active = true};
    const Bitmap active = window_controls::render(state, 2);
    ASSERT_FALSE(active.isNull());
    const int mid = active.width() / 2;
    const auto green = [](uint32_t argb) { return (argb >> 8) & 0xff; };
    EXPECT_GT(green(active.pixel(mid, mid + 7)), green(active.pixel(mid, mid - 7)) + 12);
    EXPECT_GT(active.alpha(mid, mid + 15), active.alpha(mid, mid - 16));
    state.active = false;
    state.dark = true;
    const uint32_t neutral = window_controls::render(state, 2).pixel(mid, mid);
    EXPECT_NEAR(int(green(neutral)), int((neutral >> 16) & 0xff), 2);
}

TEST(WindowHintsTest, WindowControlPlatesHoverOneAtATime)
{
    Window window;
    auto column = std::make_unique<Column>();
    WindowHeader* header = column->addChild<WindowHeader>(44);
    Theme legacy = Theme::current();
    legacy.coloredWindowControls = false;
    const Theme saved = Theme::current();
    Theme::setCurrent(legacy);
    header->setButtonsCentered(true);
    header->setLayoutHints({.fillWidth = true});
    window.resize({300, 200});
    window.setContentView(std::move(column));
    window.show();
    // The close and minimize plates beside their glyphs.
    const auto plates = [&] {
        const Image image = window.grab();
        return std::pair{int(image.pixel(13, 22).r), int(image.pixel(41, 22).r)};
    };
    const auto [closeRest, minimizeRest] = plates();
    TestInput::movePointer(window, {48, 22});
    const auto [close, minimize] = plates();
    EXPECT_EQ(close, closeRest);
    EXPECT_LT(minimize, minimizeRest - 4);
    // The gap between them hovers neither.
    TestInput::movePointer(window, {34, 22});
    EXPECT_EQ(plates(), std::pair(closeRest, minimizeRest));
    Theme::setCurrent(saved);
}

TEST(WindowHintsTest, ColoredWindowControlGlyphsShowAcrossTheWholeRow)
{
    const Theme saved = Theme::current();
    Theme theme = saved;
    theme.coloredWindowControls = true;
    Theme::setCurrent(theme);
    Window window;
    auto column = std::make_unique<Column>();
    WindowHeader* header = column->addChild<WindowHeader>(44);
    header->setButtonsCentered(true);
    header->setLayoutHints({.fillWidth = true});
    window.resize({300, 200});
    window.setContentView(std::move(column));
    window.show();
    // Both glyphs sit in the middle of their plate.
    const auto centres = [&] {
        const Image image = window.grab();
        return std::pair{int(image.pixel(20, 22).r), int(image.pixel(43, 22).r)};
    };
    const auto [closeRest, minimizeRest] = centres();
    // The gap between close and minimize, over minimize, and over maximize.
    for (const double x : {31.5, 43.0, 66.0}) {
        SCOPED_TRACE(x);
        TestInput::movePointer(window, {x, 22});
        const auto [close, minimize] = centres();
        EXPECT_LT(close, closeRest - 40);
        EXPECT_LT(minimize, minimizeRest - 40);
    }
    TestInput::movePointer(window, {200, 22});
    EXPECT_EQ(centres(), std::pair(closeRest, minimizeRest));
    Theme::setCurrent(saved);
}

TEST(WindowHintsTest, ScrollBarDarkModeOverridesTheTheme)
{
    Window window;
    window.resize({40, 200});
    window.setBackgroundColor(Color::rgb(0x000000));
    auto bar = std::make_unique<ScrollBar>(ScrollBar::Orientation::Vertical);
    ScrollBar* scrollBar = bar.get();
    window.setContentView(std::move(bar));
    window.show();
    scrollBar->setRange(100, 200, 0);
    scrollBar->setAlwaysShown(true);
    // Let the bar fade in.
    for (int frame = 0; frame < 120; ++frame)
        window.grab(uint64_t(frame) * 16666667 + 1000000000);
    scrollBar->setDarkMode(true);
    const Color light = window.grab(5000000000).pixel(4, 40);
    scrollBar->setDarkMode(false);
    const Color dark = window.grab(5000000000).pixel(4, 40);
    // A white handle shows on the black background; a black one does not.
    EXPECT_GT(light.r, 0x60);
    EXPECT_LT(dark.r, 0x10);
}

TEST(WindowHintsTest, HeaderButtonGlyphColour)
{
    Window window;
    window.resize({32, 32});
    window.setBackgroundColor(Color::rgb(0xFFFFFF));
    auto button = std::make_unique<HeaderButton>("close");
    HeaderButton* close = button.get();
    window.setContentView(std::move(button));
    window.show();
    close->setGlyphColor(Color::rgb(0xFF0000));
    const Color red = window.grab().pixel(16, 16);
    EXPECT_GT(red.r, 0xC0);
    EXPECT_LT(red.g, 0x80);
    close->setGlyphColor(std::nullopt);
    const Color tint = window.grab().pixel(16, 16);
    EXPECT_LT(std::abs(int(tint.r) - int(tint.b)), 0x20);
}

namespace {

// Fills itself in a colour, where the item's artwork would be.
class Swatch final : public View
{
public:
    Color color {0, 0, 0, 255};

protected:
    void paint(Painter& painter) override { painter.fillRect(localRect(), color); }
};

} // namespace

// Headless, an item is a window of the size it asks for in a light bar; the
// highlight it asks for is the highlight it has.
TEST(StatusItemTest, HeadlessItemsDrawAtTheirSizeAndHighlightWhenAsked)
{
    EXPECT_FALSE(StatusItem::isSupported());
    StatusItem item;
    auto swatch = std::make_unique<Swatch>();
    Swatch* content = swatch.get();
    item.setContentView(std::move(swatch));
    item.setWidth(57.4);
    EXPECT_DOUBLE_EQ(57, item.width());
    EXPECT_GT(item.height(), 0);
    int highlights = 0;
    item.onHighlightChanged = [&] { ++highlights; };
    item.show();
    EXPECT_FALSE(item.darkAppearance());
    EXPECT_EQ((Color{0, 0, 0, 217}), item.foregroundColor());
    EXPECT_EQ(0, item.screenRect().width);

    content->color = item.foregroundColor();
    const Image image = item.window().grab();
    EXPECT_EQ(57, image.width());
    EXPECT_EQ(int(item.height()), image.height());
    EXPECT_EQ(217, image.pixel(28, image.height() / 2).a);

    item.setHighlighted(true);
    EXPECT_TRUE(item.isHighlighted());
    EXPECT_EQ(1, highlights);
    item.setHighlighted(false);
    EXPECT_FALSE(item.isHighlighted());

    // A popup hangs centred under the whole item and never flips above it.
    const PopupPlacement placement = item.popupPlacement(4);
    EXPECT_EQ((RectF{0, 0, 57, item.height()}), placement.anchorRect);
    EXPECT_EQ(PopupAnchor::Bottom, placement.anchor);
    EXPECT_EQ(PopupAnchor::Bottom, placement.gravity);
    EXPECT_EQ((PointF{0, 4}), placement.offset);
    EXPECT_FALSE(placement.flipY);
}
