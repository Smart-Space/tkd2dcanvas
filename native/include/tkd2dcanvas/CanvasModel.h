// CanvasModel.h — the soul of the project: item store, z-order, tags,
// view/scroll state. Knows nothing about Tcl/Tk or Direct2D.
#pragma once

#include <memory>
#include <string>
#include <vector>
#include <unordered_map>

#include "tkd2dcanvas/CanvasItem.h"
#include "tkd2dcanvas/TagsIndex.h"
#include "tkd2dcanvas/CanvasView.h"

class CanvasModel {
public:
    static constexpr int kInvalidId = -1;

    CanvasModel();
    ~CanvasModel() = default;

    // --- Items ---------------------------------------------------------------
    int CreateItem(CanvasItemPtr item);              // takes ownership, returns id
    bool DeleteItem(int id);
    void DeleteAll();
    bool Exists(int id) const;
    CanvasItem* FindItem(int id);
    const CanvasItem* FindItem(int id) const;

    std::size_t Size() const { return items_.size(); }
    std::vector<int> AllItemIds() const { return zOrder_; }  // bottom-to-top

    // --- z-order ---------------------------------------------------------------
    bool RaiseToTop(int id);
    bool LowerToBottom(int id);
    bool RaiseBy(int id, int delta);                // +delta = towards top
    std::vector<CanvasItem*> ItemsInZOrder() const;  // bottom-to-top

    // --- Operations -------------------------------------------------------------
    bool Translate(const std::vector<int>& ids, double dx, double dy);
    BBox ItemBBox(int id) const;
    BBox AllBBox() const;

    // --- Tags ---------------------------------------------------------------------
    bool AddTag(int id, const std::string& tag);
    bool AddTagToItems(const std::vector<int>& ids, const std::string& tag);
    bool RemoveTag(int id, const std::string& tag);
    bool RemoveTagFromItems(const std::vector<int>& ids, const std::string& tag);
    std::vector<int> FindWithTag(const std::string& tag) const;
    std::vector<std::string> TagsOf(int id) const;
    TagsIndex& Tags() { return tags_; }

    // --- View ---------------------------------------------------------------------
    CanvasView& View() { return view_; }
    const CanvasView& View() const { return view_; }

private:
    std::unordered_map<int, CanvasItemPtr> items_;
    std::vector<int> zOrder_;                        // bottom-to-top
    TagsIndex tags_;
    CanvasView view_;
    int nextId_ = 1;

    void DetachFromZOrder(int id);
};
