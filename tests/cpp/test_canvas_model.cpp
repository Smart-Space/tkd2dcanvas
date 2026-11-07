// test_canvas_model.cpp — unit tests for CanvasModel using a fake item.
#include <cassert>
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

    const int a = model.CreateItem(std::make_unique<FakeRectItem>(0, 0, 0, 50, 50));
    const int b = model.CreateItem(std::make_unique<FakeRectItem>(0, 100, 100, 150, 150));
    assert(a != CanvasModel::kInvalidId && b != CanvasModel::kInvalidId);
    assert(a != b);
    assert(model.Size() == 2);

    // z-order: a below b initially.
    auto ids = model.AllItemIds();
    assert(ids.size() == 2 && ids[0] == a && ids[1] == b);

    assert(model.RaiseToTop(a));
    ids = model.AllItemIds();
    assert(ids[0] == b && ids[1] == a);

    assert(model.AddTag(a, "group"));
    assert(model.AddTag(b, "group"));
    const auto group = model.FindWithTag("group");
    assert(group.size() == 2);

    model.Translate({a}, 10.0, 10.0);
    const BBox box = model.ItemBBox(a);
    assert(box.left == 10.0 && box.top == 10.0 && box.right == 60.0 && box.bottom == 60.0);

    assert(model.DeleteItem(a));
    assert(!model.Exists(a));
    assert(model.Size() == 1);

    // "all" shortcut works without any explicit tags.
    assert(model.FindWithTag("all").size() == 1);

    std::puts("test_canvas_model: OK");
    return 0;
}
