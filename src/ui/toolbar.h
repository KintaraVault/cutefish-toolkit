// SPDX-License-Identifier: BSD-3-Clause

#pragma once

#include "ui/view.h"

#include <functional>
#include <string>
#include <vector>

namespace cutefish::ui {

// A customizable action strip. Applications own identifiers, persistence and
// action semantics; the widget owns presentation, spacing and drag editing.
class Toolbar : public View
{
public:
    enum class Display { Icons, IconsAndText, Text };
    struct Item {
        std::string id;
        std::string label;
        std::string glyph;
        double width = 30;
        std::function<View*(Toolbar&)> create;
        std::vector<std::string> previewGlyphs;
        bool dropdown = false;
        // Consecutive items with the same nonempty group share one plate.
        std::string group;
    };
    static constexpr const char* Space = "space";
    static constexpr const char* FlexibleSpace = "flexible-space";
    static constexpr const char* DefaultSet = "default-set";

    Toolbar();
    void setItems(std::vector<Item> items);
    void setOrder(const std::vector<std::string>& order);
    const std::vector<std::string>& order() const { return m_order; }
    void setDefaults(std::vector<std::string> order) { m_defaults = std::move(order); }
    void setDisplay(Display display);
    Display display() const { return m_display; }
    void setEditing(bool editing);
    // Keep a segmented item's current glyph visible while its live widget
    // is replaced by the draggable tile.
    void setPreviewSelection(std::string id, int index);
    // The on-screen item, for anchoring popovers. Invalidated when rebuilt.
    View* itemView(const std::string& id) const;
    View* makePaletteItem(View& parent, const std::string& id, const std::string& label, const std::string& glyph);
    SizeF sizeHint() const override;

    std::function<void(const std::string&, PointF)> onActivated;
    std::function<void(const std::vector<std::string>&)> onOrderChanged;

protected:
    void layout() override;
    void paint(Painter& painter) override;
    bool canDrop(const DropOffer& offer) const override;
    uint32_t dragUpdate(const DragEvent& event) override;
    void dragLeave() override;
    uint32_t drop(const DragEvent& event) override;

private:
    class Tile;
    void rebuild();
    const Item* find(const std::string& id) const;
    bool sharesPlate(size_t first, size_t second) const;
    void applyDrop(const std::string& id, int from, int to);
    int insertion(double x) const;
    std::vector<Item> m_items;
    std::vector<std::string> m_order;
    std::vector<std::string> m_defaults;
    Display m_display = Display::Icons;
    bool m_editing = false;
    int m_insert = -1;
    std::string m_previewItem;
    int m_previewIndex = -1;
    std::shared_ptr<bool> m_alive = std::make_shared<bool>(true);

public:
    ~Toolbar() override;
};

} // namespace cutefish::ui
