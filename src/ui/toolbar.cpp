// SPDX-License-Identifier: BSD-3-Clause

#include "ui/toolbar.h"

#include "base/event_loop.h"
#include "ui/drag.h"
#include "ui/icon.h"
#include "ui/keys.h"
#include "ui/label.h"
#include "ui/painter.h"
#include "ui/theme.h"
#include "ui/toolbar_surface.h"

#include <algorithm>
#include <cmath>

namespace cutefish::ui {

static constexpr const char* kItemMime = "application/x-cutefish-toolbar-item";
static constexpr double kGap = 5;
static constexpr double kPlateHeight = 30;
static constexpr double kCaptionHeight = 10;
static constexpr double kGlyphSize = 20;
static constexpr double kChevronSize = 8;
static constexpr double kChevronGap = 2;

static double plateTop(double height, bool caption)
{
    return std::max(0.0, (height - kPlateHeight - (caption ? kCaptionHeight : 0)) / 2);
}

class Toolbar::Tile : public View
{
public:
    Tile(Toolbar& owner, std::string id, std::string label, const std::string& glyph, int index, bool palette)
        : m_owner(owner), m_id(std::move(id)), m_index(index), m_palette(palette)
    {
        setName(label);
        setToolTip(label);
        const auto* item = owner.find(m_id);
        std::vector<std::string> glyphs;
        if (m_id == DefaultSet) {
            for (const auto& id : owner.m_defaults) {
                if (const auto* fallback = owner.find(id)) {
                    if (!fallback->previewGlyphs.empty())
                        glyphs.insert(glyphs.end(), fallback->previewGlyphs.begin(), fallback->previewGlyphs.end());
                    else if (!fallback->glyph.empty())
                        glyphs.push_back(fallback->glyph);
                }
            }
        } else if (item && !item->previewGlyphs.empty()) {
            glyphs = item->previewGlyphs;
        } else if (!glyph.empty()) {
            glyphs.push_back(glyph);
        }
        for (const auto& icon : glyphs) {
            auto* glyph = addChild<Icon>(icon, kGlyphSize);
            glyph->setInkCentered(true);
            m_icons.push_back(glyph);
        }
        if (item && item->dropdown) {
            m_chevron = addChild<Icon>("chevron-down-small", kChevronSize);
            m_chevron->setInkCentered(true);
        }
        m_label = addChild<Label>(std::move(label));
        m_label->setFontSize(10);
        m_label->setElide(Label::Elide::Right);
        m_label->setTextAlignment(Label::TextAlignment::Center);
        m_label->setVerticallyCentered(true);
    }

    bool acceptsPointer() const override { return true; }
    bool acceptsFocus() const override { return !spacer(); }
    const std::string& id() const { return m_id; }

protected:
    void layout() override
    {
        const auto display = m_palette ? Display::IconsAndText : m_owner.display();
        const bool icon = display != Display::Text && !m_icons.empty();
        m_label->setVisible(m_id != DefaultSet && (display != Display::Icons || spacer()) && (!spacer() || m_owner.m_editing));
        const bool stacked = (m_palette && spacer()) || (icon && m_label->isVisible());
        const RectF box = localRect();
        const auto plate = plateRect();
        const double accessory = m_chevron ? kChevronGap + kChevronSize : 0;
        const double step = m_icons.size() > 1
            ? std::clamp((plate.width - 4 - kGlyphSize - accessory) / (m_icons.size() - 1), 0.0, 32.0) : 0.0;
        const double glyphWidth = m_icons.empty() ? 0 : (m_icons.size() - 1) * step + kGlyphSize;
        double x = plate.x + (plate.width - glyphWidth - accessory) / 2;
        for (auto* glyph : m_icons) {
            glyph->setVisible(icon);
            glyph->setGeometry({x, plate.y + (plate.height - kGlyphSize) / 2, kGlyphSize, kGlyphSize});
            x += step;
        }
        if (m_chevron) {
            m_chevron->setVisible(icon);
            m_chevron->setGeometry({plate.x + (plate.width + glyphWidth - accessory) / 2 + kChevronGap,
                                    plate.y + (plate.height - kChevronSize) / 2, kChevronSize, kChevronSize});
        }
        m_label->setGeometry({1, stacked ? plate.bottom() : 0.0, std::max(0.0, box.width - 2), stacked ? box.height - plate.bottom() : box.height});
        m_label->setColor(stacked ? std::optional<ColorId>(ColorId::DisabledText) : std::nullopt);
    }

    void paint(Painter& painter) override
    {
        if (spacer() && !m_owner.m_editing)
            return;
        RectF bounds = localRect();
        const auto& theme = Theme::current();
        if (spacer() || m_id == DefaultSet) {
            if (m_palette && spacer())
                bounds = {(bounds.width - 28) / 2, 2, 28, 26};
            painter.drawRoundedRect(bounds.adjusted(1, 1, -1, -1), {3}, theme.text.withAlpha(m_hover ? 0.06 : 0),
                                     1, theme.text.withAlpha(0.23));
            if (m_palette && m_id == FlexibleSpace) {
                const double y = bounds.y + bounds.height / 2;
                const double x1 = bounds.x + 5;
                const double x2 = bounds.right() - 5;
                const auto ink = theme.text.withAlpha(0.40);
                painter.drawLine({x1, y}, {x2, y}, ink, 1);
                painter.drawLine({x1, y}, {x1 + 3, y - 3}, ink, 1);
                painter.drawLine({x1, y}, {x1 + 3, y + 3}, ink, 1);
                painter.drawLine({x2, y}, {x2 - 3, y - 3}, ink, 1);
                painter.drawLine({x2, y}, {x2 - 3, y + 3}, ink, 1);
            }
            return;
        }
        bounds = plateRect();
        const bool joined = !m_palette && ((m_index > 0 && m_owner.sharesPlate(m_index - 1, m_index))
            || m_owner.sharesPlate(m_index, m_index + 1));
        if (!joined)
            toolbar_surface::capsule(painter, bounds, theme.darkMode);
        if (isEnabled() && (m_hover || m_pressed || hasFocus()))
            toolbar_surface::highlight(painter, bounds.adjusted(2, 2, -2, -2), theme.darkMode, m_pressed);
        if (!m_palette && m_owner.m_editing && m_id == m_owner.m_previewItem && m_owner.m_previewIndex >= 0
            && size_t(m_owner.m_previewIndex) < m_icons.size() && m_icons[size_t(m_owner.m_previewIndex)]->isVisible()) {
            const auto glyph = m_icons[size_t(m_owner.m_previewIndex)]->geometry();
            toolbar_surface::highlight(painter, {glyph.x + glyph.width / 2 - 14, bounds.y + 3, 28, bounds.height - 6},
                                       theme.darkMode, false, true);
        }
    }

    void pointerEnter() override { m_hover = true; requestPaint(); }
    void pointerLeave() override { m_hover = false; requestPaint(); }
    void pointerPress(const PointerEvent& event) override
    {
        if (event.button != MouseButton::Left)
            return;
        m_pressed = true;
        m_start = event.position;
        requestPaint();
    }
    void pointerMove(const PointerEvent& event) override
    {
        if (!m_pressed || !m_owner.m_editing || std::hypot(event.position.x - m_start.x, event.position.y - m_start.y) < 7)
            return;
        m_pressed = false;
        requestPaint();
        DragSource source;
        source.mimeTypes = {kItemMime};
        source.context = &m_owner;
        source.data = [id = m_id, index = m_index](const std::string&) { return std::to_string(index) + '\n' + id; };
        source.actions = MoveAction;
        startDrag(std::move(source));
    }
    void pointerRelease(const PointerEvent& event) override
    {
        const bool activate = m_pressed && localRect().contains(event.position) && event.button == MouseButton::Left;
        m_pressed = false;
        requestPaint();
        if (activate)
            activated();
    }

    bool keyPress(const KeyEvent& event) override
    {
        if (event.modifiers != NoModifier || (event.key != key::Space && event.key != key::Return))
            return false;
        if (!m_pressed) {
            m_pressed = true;
            requestPaint();
        }
        return true;
    }

    bool keyRelease(const KeyEvent& event) override
    {
        if (!m_pressed || (event.key != key::Space && event.key != key::Return))
            return false;
        m_pressed = false;
        requestPaint();
        activated();
        return true;
    }

    void focusIn(FocusReason) override { requestPaint(); }
    void focusOut() override { m_pressed = false; requestPaint(); }

private:
    RectF plateRect() const
    {
        const auto box = localRect();
        if (m_id == DefaultSet)
            return box;
        const double height = std::min(kPlateHeight, box.height);
        const double width = m_palette ? std::min(box.width, m_icons.size() > 1 ? m_icons.size() * 32.0 + 4 : m_chevron ? 40.0 : kPlateHeight) : box.width;
        return {(box.width - width) / 2, plateTop(box.height, m_palette || m_owner.display() == Display::IconsAndText),
                width, height};
    }

    void activated()
    {
        if (!m_owner.m_editing && m_owner.onActivated)
            m_owner.onActivated(m_id, mapToWindow({0, plateRect().bottom() + 2}));
        else if (m_palette) {
            auto* owner = &m_owner;
            base::EventLoop::current()->post([owner, alive = owner->m_alive, id = m_id] {
                if (*alive && owner->m_editing)
                    owner->applyDrop(id, -1, int(owner->m_order.size()));
            });
        }
    }

    bool spacer() const { return m_id == Space || m_id == FlexibleSpace; }
    Toolbar& m_owner;
    std::string m_id;
    int m_index;
    bool m_palette;
    bool m_hover = false;
    bool m_pressed = false;
    PointF m_start;
    std::vector<Icon*> m_icons;
    Icon* m_chevron = nullptr;
    Label* m_label;
};

Toolbar::Toolbar() { setName("toolbar"); }
Toolbar::~Toolbar() { *m_alive = false; }

void Toolbar::setItems(std::vector<Item> items)
{
    m_items = std::move(items);
    rebuild();
}

const Toolbar::Item* Toolbar::find(const std::string& id) const
{
    const auto it = std::find_if(m_items.begin(), m_items.end(), [&](const Item& item) { return item.id == id; });
    return it == m_items.end() ? nullptr : &*it;
}

bool Toolbar::sharesPlate(size_t first, size_t second) const
{
    if (second >= m_order.size() || first >= m_order.size())
        return false;
    const auto* a = find(m_order[first]);
    const auto* b = find(m_order[second]);
    return a && b && !a->create && !b->create && !a->group.empty() && a->group == b->group;
}

View* Toolbar::itemView(const std::string& id) const
{
    for (const auto& child : children()) {
        if (auto* tile = dynamic_cast<Tile*>(child.get()); tile && tile->id() == id)
            return tile;
    }
    return nullptr;
}

void Toolbar::setOrder(const std::vector<std::string>& order)
{
    std::vector<std::string> valid;
    for (const auto& id : order) {
        if (id == Space || id == FlexibleSpace || (find(id) && std::find(valid.begin(), valid.end(), id) == valid.end()))
            valid.push_back(id);
    }
    if (m_order == valid)
        return;
    m_order = std::move(valid);
    rebuild();
}

void Toolbar::setDisplay(Display display)
{
    if (display == m_display)
        return;
    m_display = display;
    rebuild();
}

void Toolbar::setEditing(bool editing)
{
    if (editing == m_editing)
        return;
    m_editing = editing;
    m_insert = -1;
    rebuild();
}

void Toolbar::setPreviewSelection(std::string id, int index)
{
    if (m_previewItem == id && m_previewIndex == index)
        return;
    if (auto* tile = itemView(m_previewItem))
        tile->requestPaint();
    m_previewItem = std::move(id);
    m_previewIndex = index;
    if (auto* tile = itemView(m_previewItem))
        tile->requestPaint();
}

View* Toolbar::makePaletteItem(View& parent, const std::string& id, const std::string& label, const std::string& glyph)
{
    return parent.addChild<Tile>(*this, id, label, glyph, -1, true);
}

void Toolbar::rebuild()
{
    // A drop or an action may be handled by one of the previous children.
    while (!children().empty()) {
        std::shared_ptr<View> old = takeChild(children().back().get());
        if (auto* loop = base::EventLoop::current())
            loop->post([old] {});
    }
    for (size_t i = 0; i < m_order.size(); ++i) {
        const auto& id = m_order[i];
        const Item* item = find(id);
        if (!m_editing && item && item->create && m_display != Display::Text)
            item->create(*this);
        else
            addChild<Tile>(*this, id, item ? item->label : id == Space ? "↔" : "↤ ↦", item ? item->glyph : "", int(i), false);
    }
    requestLayout();
    requestPaint();
    if (parent())
        parent()->requestLayout();
}

SizeF Toolbar::sizeHint() const
{
    double width = 0;
    for (size_t i = 1; i < m_order.size(); ++i)
        if (!sharesPlate(i - 1, i))
            width += kGap;
    for (const auto& id : m_order) {
        if (const auto* item = find(id)) {
            Label label(item->label);
            width += m_display == Display::Icons ? item->width : std::max(item->width, label.sizeHint().width + 8);
        } else {
            width += id == Space ? 17 : 8;
        }
    }
    return {width, kPlateHeight + (m_display == Display::IconsAndText ? kCaptionHeight : 0)};
}

void Toolbar::layout()
{
    const int flexible = int(std::count(m_order.begin(), m_order.end(), FlexibleSpace));
    const double extra = flexible ? std::max(0.0, geometry().width - sizeHint().width) / flexible : 0;
    double x = 0;
    for (size_t i = 0; i < m_order.size(); ++i) {
        if (i && !sharesPlate(i - 1, i))
            x += kGap;
        const auto& id = m_order[i];
        const Item* item = find(id);
        double width = id == Space ? 17 : 8 + extra;
        if (item) {
            Label label(item->label);
            width = m_display == Display::Icons ? item->width : std::max(item->width, label.sizeHint().width + 8);
        }
        children()[i]->setGeometry({x, 0, width, geometry().height});
        x += width;
    }
}

int Toolbar::insertion(double x) const
{
    int index = 0;
    for (const auto& child : children()) {
        if (x < child->geometry().x + child->geometry().width / 2)
            break;
        ++index;
    }
    return index;
}

void Toolbar::paint(Painter& painter)
{
    for (size_t i = 0; i + 1 < children().size();) {
        if (!sharesPlate(i, i + 1)) {
            ++i;
            continue;
        }
        const size_t start = i;
        while (sharesPlate(i, i + 1))
            ++i;
        const auto a = children()[start]->geometry();
        const auto b = children()[i]->geometry();
        toolbar_surface::capsule(painter, {a.x, plateTop(a.height, m_display == Display::IconsAndText),
                                          b.right() - a.x, std::min(kPlateHeight, a.height)}, Theme::current().darkMode);
        ++i;
    }
    if (m_insert >= 0) {
        const auto bounds = children().empty() ? RectF {} : children().back()->geometry();
        const double x = m_insert < int(children().size()) ? children()[size_t(m_insert)]->geometry().x : bounds.x + bounds.width;
        painter.fillRect({std::max(0.0, x - 2), 1, 2, geometry().height - 2}, Theme::current().highlight);
    }
}

bool Toolbar::canDrop(const DropOffer& offer) const
{
    return m_editing && offer.hasMimeType(kItemMime) && offer.localSource() && offer.localSource()->context == this;
}

uint32_t Toolbar::dragUpdate(const DragEvent& event)
{
    m_insert = insertion(event.position.x);
    requestPaint();
    return MoveAction;
}

void Toolbar::dragLeave() { m_insert = -1; requestPaint(); }

uint32_t Toolbar::drop(const DragEvent& event)
{
    if (!canDrop(*event.offer))
        return NoAction;
    const auto data = event.offer->localSource()->data(kItemMime);
    const auto split = data.find('\n');
    if (split == std::string::npos)
        return NoAction;
    const int from = std::stoi(data.substr(0, split));
    const int to = insertion(event.position.x);
    dragLeave();
    base::EventLoop::current()->post([this, alive = m_alive, id = data.substr(split + 1), from, to] {
        if (*alive && m_editing)
            applyDrop(id, from, to);
    });
    return MoveAction;
}

void Toolbar::applyDrop(const std::string& id, int from, int to)
{
    auto order = m_order;
    if (id == DefaultSet) {
        order = m_defaults;
    } else {
        if (from < 0 && id != Space && id != FlexibleSpace) {
            const auto found = std::find(order.begin(), order.end(), id);
            if (found != order.end())
                from = int(found - order.begin());
        }
        if (from >= 0 && from < int(order.size())) {
            order.erase(order.begin() + from);
            if (to > from)
                --to;
        }
        order.insert(order.begin() + std::clamp(to, 0, int(order.size())), id);
    }
    setOrder(order);
    if (onOrderChanged)
        onOrderChanged(m_order);
}

} // namespace cutefish::ui
