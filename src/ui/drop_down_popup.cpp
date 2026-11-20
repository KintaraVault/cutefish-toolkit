// SPDX-FileCopyrightText: 2026 Reion Wong <reionwong@gmail.com>
// SPDX-License-Identifier: BSD-3-Clause

#include "ui/drop_down_popup.h"

#include <algorithm>
#include <cmath>

#include "base/event_loop.h"
#include "ui/application.h"
#include "ui/bubble_shape.h"
#include "ui/embedded_window.h"
#include "ui/keys.h"
#include "ui/painter.h"
#include "ui/popup_glass.h"
#include "ui/theme.h"
#include "ui/view.h"
#include "ui/window.h"

namespace cutefish::ui {

// The bubble behind the content, as a menu's, and Escape.
class DropDownPopup::Surface : public View
{
public:
    explicit Surface(DropDownPopup* popup, bool bubble, const PopupPlacement& placement)
        : m_popup(popup), m_bubble(bubble), m_placement(placement)
    {
    }

    bool acceptsPointer() const override { return true; }
    bool acceptsFocus() const override { return true; }

    double arrowLength() const { return m_bubble ? 8 : 0; }

    void updatePlacement(Window& popup, SizeF size)
    {
        if (!m_bubble)
            return;
        const double centre = m_placement.anchorRect.x + std::floor(m_placement.anchorRect.width / 2);
        const double half = std::floor(size.width / 2);
        const double right = m_parentWidth - size.width;
        const double x = right >= 0 ? std::clamp(centre - half, 0.0, right) : centre - half;
        m_placement.offset.x = x + half - centre;
        m_arrowX = centre - x;
        popup.setPopupPlacement(m_placement);
    }

    BubbleShape shape(SizeF size) const
    {
        BubbleShape shape;
        shape.width = size.width;
        shape.height = size.height;
        shape.radius = m_bubble ? design::popoverRadius : design::menuRadius;
        shape.continuousCorners = Theme::current().continuousCorners;
        shape.arrowEdge = m_bubble ? BubbleShape::Edge::Top : BubbleShape::Edge::None;
        shape.arrowPosition = m_arrowX;
        shape.arrowWidth = 20;
        shape.arrowLength = arrowLength();
        return shape;
    }

    void setParentWidth(double width) { m_parentWidth = width; }

protected:
    void layout() override
    {
        for (const auto& child : children())
            child->setGeometry(localRect().adjusted(0, arrowLength(), 0, 0));
    }

    void paint(Painter& painter) override
    {
        // An embedded host draws the glass, tint included.
        if (Application::instance()->platform() == Application::Platform::Embedded && EmbeddedWindow::host())
            return;
        if (m_glass.update(*window()))
            return;
        // Without the compositor's blur: opaque, in the window radius.
        const Theme& theme = Theme::current();
        if (m_bubble) {
            const auto path = shape({localRect().width, localRect().height}).path();
            painter.fillPath(path, theme.secondBackground);
            painter.strokePath(path, theme.text.withAlpha(0.18), 1 / painter.scale());
        } else {
            painter.drawRoundedRect(localRect(), {design::menuRadius, 0, themeCorners()}, theme.secondBackground,
                                    1 / painter.scale(), theme.text.withAlpha(.14));
        }
    }

    bool keyPress(const KeyEvent& event) override
    {
        if (event.key != key::Escape || event.modifiers != NoModifier)
            return false;
        m_popup->close();
        return true;
    }

private:
    DropDownPopup* m_popup;
    PopupGlass m_glass;
    bool m_bubble;
    PopupPlacement m_placement;
    double m_parentWidth = 0;
    double m_arrowX = 0;
};

DropDownPopup::DropDownPopup() = default;

DropDownPopup::~DropDownPopup()
{
    // The content goes with the window; nothing is told.
    m_window.reset();
}

void DropDownPopup::setContentView(std::unique_ptr<View> view)
{
    m_content = view.get();
    if (m_surface) {
        while (!m_surface->children().empty())
            m_surface->takeChild(m_surface->children().front().get());
        m_surface->addChild(std::move(view));
        updateSize();
        return;
    }
    m_ownedContent = std::move(view);
}

SizeF DropDownPopup::contentSize() const
{
    if (!m_content)
        return {};
    // MenuPopupWindow's size: the content's implicit size, rounded up.
    const double width = std::ceil(m_content->sizeHint().width);
    return {width, std::ceil(m_content->heightForWidth(width)) + (m_surface ? m_surface->arrowLength() : 0)};
}

void DropDownPopup::applySize(SizeF size)
{
    m_window->resize(size);
    m_surface->updatePlacement(*m_window, size);
    if (m_surface->arrowLength()) {
        const auto shape = m_surface->shape(size);
        m_window->setBackgroundBlur(shape.bodyRect(), shape.radius);
        m_window->setBackgroundBlurShape(shape.path());
    } else {
        m_window->setBackgroundBlur({0, 0, size.width, size.height}, design::menuRadius);
    }
}

void DropDownPopup::popupFor(View* view, double gap, std::optional<RectF> rect)
{
    Window* parent = view ? view->window() : nullptr;
    if (!parent)
        return;
    const RectF local = rect.value_or(view->localRect());
    const PointF topLeft = view->mapToWindow({local.x, local.y});
    // MenuPopupWindow::showDropDown's anchor: the item's rect on whole
    // pixels, gap above and below it; the compositor flips it above when it
    // does not fit, and slides it along the screen.
    const double x = std::round(topLeft.x);
    const double y = std::round(topLeft.y);
    PopupPlacement placement;
    placement.anchorRect = {x, y - gap, std::ceil(local.width), std::ceil(local.height) + 2 * gap};
    placement.anchor = PopupAnchor::BottomLeft;
    placement.gravity = PopupAnchor::BottomRight;
    placement.flipX = false;
    open(parent, placement, false);
}

void DropDownPopup::popoverFor(View* view, double gap)
{
    Window* parent = view ? view->window() : nullptr;
    if (!parent)
        return;
    const auto topLeft = view->mapToWindow({0, 0});
    const auto box = view->localRect();
    PopupPlacement placement;
    placement.anchorRect = {std::round(topLeft.x), std::round(topLeft.y), std::ceil(box.width), std::ceil(box.height)};
    placement.anchor = PopupAnchor::Bottom;
    placement.gravity = PopupAnchor::Bottom;
    placement.offset.y = gap;
    placement.flipX = false;
    // Toolbar popovers open downward; don't flip the top arrow away from the anchor.
    placement.flipY = false;
    open(parent, placement, true);
}

void DropDownPopup::open(Window* parent, const PopupPlacement& placement, bool popover)
{
    close();
    m_window = Window::createPopup(parent, placement, true);
    auto surface = std::make_unique<Surface>(this, popover, placement);
    surface->setParentWidth(parent->size().width);
    m_surface = surface.get();
    if (m_ownedContent)
        m_surface->addChild(std::move(m_ownedContent));
    // Measured in the popup, at its scale.
    m_window->setContentView(std::move(surface));
    PopupGlass::prepare(*m_window);
    applySize(contentSize());
    m_window->onCloseRequested = [this] { close(); };
    // The content sees keys first; Escape reaches the surface unless it takes it.
    if (m_content && m_content->acceptsFocus())
        m_content->setFocus(FocusReason::Other);
    else
        m_surface->setFocus(FocusReason::Other);
    m_window->show();
}

void DropDownPopup::updateSize()
{
    if (m_window)
        applySize(contentSize());
}

void DropDownPopup::close()
{
    if (!m_window)
        return;
    // The content may be closing it from its own event handling: it stays
    // alive here, and the window goes once that is over, off screen now.
    if (m_content && m_content->parent() == m_surface)
        m_ownedContent = m_surface->takeChild(m_content);
    m_surface = nullptr;
    std::shared_ptr<Window> window(std::move(m_window));
    window->hide();
    Application::instance()->loop().post([window] {});
    if (onClosed)
        onClosed();
}

} // namespace cutefish::ui
