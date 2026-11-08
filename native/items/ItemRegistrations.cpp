#include "ItemRegistrations.h"

#include <memory>
#include <stdexcept>
#include <vector>

#include "tkd2dcanvas/ItemFactory.h"

#include "RectItem.h"
#include "OvalItem.h"
#include "LineItem.h"
#include "PolygonItem.h"
#include "ArcItem.h"
#include "TextItem.h"
#include "ImageItem.h"
#include "BitmapItem.h"

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

CanvasItemPtr MakePolygon(CanvasItem::Id id, const std::vector<double>& coords) {
    if (coords.size() < 6 || coords.size() % 2 != 0) {
        throw std::invalid_argument("wrong # coordinates: expected at least 6 (an even number), got " +
                                    std::to_string(coords.size()));
    }
    Points points;
    points.reserve(coords.size() / 2);
    for (std::size_t i = 0; i < coords.size(); i += 2) {
        points.push_back(Point{coords[i], coords[i + 1]});
    }
    return std::make_unique<PolygonItem>(id, std::move(points));
}

CanvasItemPtr MakeArc(CanvasItem::Id id, const std::vector<double>& coords) {
    if (coords.size() != 4) {
        throw std::invalid_argument("wrong # coordinates: expected 4, got " +
                                    std::to_string(coords.size()));
    }
    // Tk defaults: -start 0, -extent 90, -style pieslice.
    return std::make_unique<ArcItem>(id, coords[0], coords[1], coords[2], coords[3]);
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

CanvasItemPtr MakeText(CanvasItem::Id id, const std::vector<double>& coords) {
    if (coords.size() != 2) {
        throw std::invalid_argument("wrong # coordinates: expected 2 (x y), got " +
                                    std::to_string(coords.size()));
    }
    // Tk canvas text defaults to black fill; the text itself arrives via -text.
    return std::make_unique<TextItem>(id, coords[0], coords[1], "");
}

CanvasItemPtr MakeImage(CanvasItem::Id id, const std::vector<double>& coords) {
    if (coords.size() != 2) {
        throw std::invalid_argument("wrong # coordinates: expected 2 (x y), got " +
                                    std::to_string(coords.size()));
    }
    // The photo name arrives via -image.
    return std::make_unique<ImageItem>(id, coords[0], coords[1], "");
}

CanvasItemPtr MakeBitmap(CanvasItem::Id id, const std::vector<double>& coords) {
    if (coords.size() != 2) {
        throw std::invalid_argument("wrong # coordinates: expected 2 (x y), got " +
                                    std::to_string(coords.size()));
    }
    // The bitmap name arrives via -bitmap.
    return std::make_unique<BitmapItem>(id, coords[0], coords[1], "");
}

}  // namespace

void RegisterBuiltinItems() {
    ItemFactory& factory = ItemFactory::Instance();
    factory.Register("rectangle", &MakeRect);
    factory.Register("oval", &MakeOval);
    factory.Register("line", &MakeLine);
    factory.Register("polygon", &MakePolygon);
    factory.Register("arc", &MakeArc);
    factory.Register("text", &MakeText);
    factory.Register("image", &MakeImage);
    factory.Register("bitmap", &MakeBitmap);
}

}  // namespace tkd2dcanvas
