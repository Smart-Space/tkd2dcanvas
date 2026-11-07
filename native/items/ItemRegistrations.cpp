#include "ItemRegistrations.h"

#include <memory>
#include <stdexcept>
#include <vector>

#include "tkd2dcanvas/ItemFactory.h"

#include "RectItem.h"
#include "OvalItem.h"
#include "LineItem.h"

namespace tkd2dcanvas {

namespace {

using Points = std::vector<Point>;

CanvasItemPtr MakeRect(CanvasItem::Id id, const std::vector<double>& coords) {
    if (coords.size() != 4) {
        throw std::invalid_argument("wrong # coordinates: expected 4, got " +
                                    std::to_string(coords.size()));
    }
    return std::make_unique<RectItem>(id, coords[0], coords[1], coords[2], coords[3]);
}

CanvasItemPtr MakeOval(CanvasItem::Id id, const std::vector<double>& coords) {
    if (coords.size() != 4) {
        throw std::invalid_argument("wrong # coordinates: expected 4, got " +
                                    std::to_string(coords.size()));
    }
    return std::make_unique<OvalItem>(id, coords[0], coords[1], coords[2], coords[3]);
}

CanvasItemPtr MakeLine(CanvasItem::Id id, const std::vector<double>& coords) {
    if (coords.size() < 4 || coords.size() % 2 != 0) {
        throw std::invalid_argument("wrong # coordinates: expected an even number >= 4, got " +
                                    std::to_string(coords.size()));
    }
    Points points;
    points.reserve(coords.size() / 2);
    for (std::size_t i = 0; i < coords.size(); i += 2) {
        points.push_back(Point{coords[i], coords[i + 1]});
    }
    return std::make_unique<LineItem>(id, std::move(points));
}

}  // namespace

void RegisterBuiltinItems() {
    ItemFactory& factory = ItemFactory::Instance();
    factory.Register("rectangle", &MakeRect);
    factory.Register("oval", &MakeOval);
    factory.Register("line", &MakeLine);
}

}  // namespace tkd2dcanvas
