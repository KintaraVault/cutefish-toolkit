// SPDX-License-Identifier: BSD-3-Clause

#include "ui/toolbar.h"

#include "base/event_loop.h"
#include "ui/drag.h"
#include "ui/icon.h"
#include "ui/label.h"
#include "ui/painter.h"
#include "ui/theme.h"
#include "ui/window_controls.h"

#include <algorithm>
#include <cmath>

namespace cutefish::ui {

static constexpr const char* kItemMime = "application/x-cutefish-toolbar-item";
static constexpr double kGap = 5;

class Toolbar::Tile : public View
{
public:
    Tile(Toolbar& owner, std::string id, std::string label, const std::string& glyph, int index, bool palette)
        : m_owner(owner), m_id(std::move(id)), m_index(index), m_palette(palette)
    {
        setName(label);
        m_icon = addChild<Icon>(glyph, 16);
        m_icon->setVisible(!glyph.empty());
        m_label = addChild<Label>(std::move(label));
        m_label->setFontSize(10);
        m_label->setElide(Label::Elide::Right);
        m_label->setTextAlignment(Label::TextAlignment::Center);
        m_label->setVerticallyCentered(true);
    }

    bool acceptsPointer() const override { return true; }

protected:
    void layout() override
    {
        const auto display = m_palette ? Display::IconsAndText : m_owner.display();
        const bool icon = display != Display::Text && !spacer() && m_id != DefaultSet;
        m_icon->setVisible(icon);
        m_label->setVisible((display != Display::Icons || spacer()) && (!spacer() || m_owner.m_editing));
        const bool stacked = icon && m_label->isVisible();
        const RectF box = localRect();
        m_icon->setGeometry({(box.width - 16) / 2, stacked ? 2 : (box.height - 16) / 2, 16, 16});
        m_label->setGeometry({1, stacked ? 18.0 : 0.0, std::max(0.0, box.width - 2), stacked ? box.height - 18 : box.height});
    }

    void paint(Painter& painter) override
    {
        if (spacer() && !m_owner.m_editing)
            return;
        RectF bounds = localRect();
        if (m_owner.display() == Display::IconsAndText && !m_palette)
            bounds.height = std::min(21.0, bounds.height);
        window_controls::paintGroupPlate(painter, bounds, Theme::current().darkMode);
        if (m_hover || m_pressed)
            painter.drawSmoothRectangle(bounds, bounds.height / 2, Theme::current().text.withAlpha(m_pressed ? 0.13 : 0.06));
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
        if (!activate)
            return;
        if (!m_owner.m_editing && m_owner.onActivated)
            m_owner.onActivated(m_id, mapToWindow({0, geometry().height + 2}));
        else if (m_palette) {
            auto* owner = &m_owner;
            base::EventLoop::current()->post([owner, alive = owner->m_alive, id = m_id] {
                if (*alive && owner->m_editing)
                    owner->applyDrop(id, -1, int(owner->m_order.size()));
            });
        }
    }

private:
    bool spacer() const { return m_id == Space || m_id == FlexibleSpace; }
    Toolbar& m_owner;
    std::string m_id;
    int m_index;
    bool m_palette;
    bool m_hover = false;
    bool m_pressed = false;
    PointF m_start;
    Icon* m_icon;
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
    double width = m_order.empty() ? 0 : (m_order.size() - 1) * kGap;
    for (const auto& id : m_order) {
        if (const auto* item = find(id)) {
            Label label(item->label);
            width += m_display == Display::Icons ? item->width : std::max(item->width, label.sizeHint().width + 8);
        } else {
            width += id == Space ? 17 : 8;
        }
    }
    return {width, m_display == Display::IconsAndText ? 36.0 : 30.0};
}

void Toolbar::layout()
{
    const int flexible = int(std::count(m_order.begin(), m_order.end(), FlexibleSpace));
    const double extra = flexible ? std::max(0.0, geometry().width - sizeHint().width) / flexible : 0;
    double x = 0;
    for (size_t i = 0; i < m_order.size(); ++i) {
        const auto& id = m_order[i];
        const Item* item = find(id);
        double width = id == Space ? 17 : 8 + extra;
        if (item) {
            Label label(item->label);
            width = m_display == Display::Icons ? item->width : std::max(item->width, label.sizeHint().width + 8);
        }
        children()[i]->setGeometry({x, 0, width, geometry().height});
        x += width + kGap;
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
    if (m_editing)
        painter.fillRect(localRect(), Theme::current().text.withAlpha(0.05));
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
