// test_tags.cpp — unit tests for TagsIndex.
#include <cassert>
#include <cstdio>
#include <algorithm>

#include "tkd2dcanvas/TagsIndex.h"

int main() {
    TagsIndex tags;

    tags.Add(1, "node");
    tags.Add(2, "node");
    tags.Add(1, "selected");

    const auto node = tags.Find("node");
    assert(node.size() == 2);
    assert((node[0] == 1 && node[1] == 2));

    assert(tags.Contains(1, "node"));
    assert(tags.Contains(1, "selected"));
    assert(!tags.Contains(2, "selected"));

    const auto t1 = tags.TagsOf(1);
    assert(t1.size() == 2);

    tags.Remove(1, "node");
    assert(!tags.Contains(1, "node"));
    assert(tags.Find("node").size() == 1);

    tags.RemoveItem(2);
    assert(tags.Find("node").empty());

    // Duplicate adds are idempotent.
    tags.Add(3, "dup");
    tags.Add(3, "dup");
    assert(tags.TagsOf(3).size() == 1);

    std::puts("test_tags: OK");
    return 0;
}
