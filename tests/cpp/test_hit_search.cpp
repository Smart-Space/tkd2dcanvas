// test_hit_search.cpp — unit tests for the hit test / search helpers of the
// canvas model (the core of the "find" commands and of the tag bindings).
#include <cassert>
#include <cmath>
#include <cstdio>
#include <memory>

#include "tkd2dcanvas/CanvasModel.h"

namespace {

class FakeRectItem : public CanvasItem {
public:
    FakeRectItem(Id id, double x1, double y1, double x2, double y2)
        : CanvasItem(id, "fake-rect"), x1_(x1), y1_(y1), x2_(x2), y2_(y2) {}

    BBox GetBBox() const override { return BBox{x1_, y1_, x2_, y2_}; }
    void Translate(double dx, double dy) override {
        x1_ += dx; x2_ += dx; y1_ += dy; y2_ += dy;
    }
    void Draw(Renderer&) const override {}

private:
    double x1_, y1_, x2_, y2_;
};

}  // namespace

int main() {
    CanvasModel model;

    const int bottom = model.CreateItem(std::make_unique<FakeRectItem>(0, 0, 0, 100, 100));
    const int top = model.CreateItem(std::make_unique<FakeRectItem>(0, 10, 10, 110, 110));
    const int far = model.CreateItem(std::make_unique<FakeRectItem>(0, 500, 500, 600, 600));

    // --- hit test ------------------------------------------------------------
    // The topmost item wins when several items cover the point.
    assert(model.FindItemAt(50, 50, 0) == top);
    // A point that only the lower item covers.
    assert(model.FindItemAt(5, 5, 0) == bottom);
    // Nothing there.
    assert(model.FindItemAt(300, 300, 0) == CanvasModel::kInvalidId);

    // The tolerance widens the hit area (-closeenough).
    assert(model.FindItemAt(115, 50, 0) == CanvasModel::kInvalidId);
    assert(model.FindItemAt(115, 50, 5) == top);

    // --- closest --------------------------------------------------------------
    // The nearest item is reported even when the point is outside of it.
    assert(model.FindClosest(115, 50, 0) == top);
    assert(model.FindClosest(500, 500, 0) == far);
    // A halo shrinks every distance, so far away items become equally close
    // and the topmost one wins the tie.
    assert(model.FindClosest(115, 50, 500) == far);

    // --- z-order --------------------------------------------------------------
    std::vector<int> order = model.AllItemIds();
    assert(order.size() == 3 && order[0] == bottom && order[1] == top && order[2] == far);

    assert(model.MoveItemsAbove({bottom}, far));
    order = model.AllItemIds();
    assert(order[0] == top && order[1] == far && order[2] == bottom);

    assert(model.MoveItemsBelow({bottom}, top));
    order = model.AllItemIds();
    assert(order[0] == bottom && order[1] == top && order[2] == far);

    // Moving to the top / bottom of the list.
    assert(model.MoveItemsAbove({bottom}, CanvasModel::kInvalidId));
    order = model.AllItemIds();
    assert(order[2] == bottom);
    assert(model.MoveItemsBelow({bottom}, CanvasModel::kInvalidId));
    order = model.AllItemIds();
    assert(order[0] == bottom);

    // The relative order of the moved items is kept.
    CanvasModel grouped;
    const int g1 = grouped.CreateItem(std::make_unique<FakeRectItem>(0, 0, 0, 10, 10));
    const int g2 = grouped.CreateItem(std::make_unique<FakeRectItem>(0, 20, 20, 30, 30));
    const int g3 = grouped.CreateItem(std::make_unique<FakeRectItem>(0, 40, 40, 50, 50));
    assert(grouped.MoveItemsAbove({g1, g3}, g2));
    order = grouped.AllItemIds();
    assert(order.size() == 3 && order[0] == g2 && order[1] == g1 && order[2] == g3);

    // --- tags -----------------------------------------------------------------
    CanvasModel tagged;
    const int t1 = tagged.CreateItem(std::make_unique<FakeRectItem>(0, 0, 0, 10, 10));
    assert(tagged.AddTag(t1, "alpha"));
    assert(tagged.AddTag(t1, "beta"));
    const auto tags = tagged.TagsOf(t1);
    assert(tags.size() == 2 && tags[0] == "alpha" && tags[1] == "beta");
    assert(tagged.RemoveTag(t1, "alpha"));
    assert(tagged.TagsOf(t1).size() == 1 && tagged.TagsOf(t1)[0] == "beta");
    assert(tagged.FindWithTag("beta").size() == 1);
    assert(tagged.FindWithTag("alpha").empty());

    std::puts("test_hit_search: OK");
    return 0;
}
