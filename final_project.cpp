#include <iostream>
#include <vector>
#include <limits>
#include <optional>
#include <algorithm>
#include <cassert>
#include <cmath>

constexpr size_t MAX_ENTRIES = 4;  
constexpr size_t MIN_ENTRIES = 2; 

class Rectangle {
public:
    float x_min, y_min, x_max, y_max;

    Rectangle(float x_min, float y_min, float x_max, float y_max)
        : x_min(x_min), y_min(y_min), x_max(x_max), y_max(y_max) {}

    float area() const {
        return (x_max - x_min) * (y_max - y_min);
    }

    bool overlaps(const Rectangle& other) const {
        return !(x_min >= other.x_max || x_max <= other.x_min ||
                 y_min >= other.y_max || y_max <= other.y_min);
    }

    void expand(const Rectangle& other) {
        x_min = std::min(x_min, other.x_min);
        y_min = std::min(y_min, other.y_min);
        x_max = std::max(x_max, other.x_max);
        y_max = std::max(y_max, other.y_max);
    }
};

template <typename DataT>
struct Entry {
    Rectangle bounding_box;
    std::optional<DataT> data;
    size_t child_index;

    Entry(const Rectangle& rect, const std::optional<DataT>& data = std::nullopt)
        : bounding_box(rect), data(data), child_index(std::numeric_limits<size_t>::max()) {}
};

template <typename DataT>
struct Node {
    bool is_leaf;
    std::vector<Entry<DataT>> entries;

    Node(bool is_leaf) : is_leaf(is_leaf) {}
};

template <typename DataT>
class RTree {
public:
    std::vector<Node<DataT>> nodes;
    size_t root_index;

    RTree() {
        root_index = createNode(true);
    }

    void insert(const Rectangle& rect, const DataT& data) {
        size_t leaf_index = chooseLeaf(root_index, rect);
        Node<DataT>& leaf = nodes[leaf_index];
        leaf.entries.emplace_back(rect, data);

        if (leaf.entries.size() > MAX_ENTRIES) {
            splitNode(leaf_index);
        }
    }

    std::vector<DataT> rangeQuery(const Rectangle& rect) const {
        std::vector<DataT> results;
        rangeQueryHelper(root_index, rect, results);
        return results;
    }

private:
    size_t createNode(bool is_leaf) {
        nodes.emplace_back(is_leaf);
        return nodes.size() - 1;
    }

    size_t chooseLeaf(size_t node_index, const Rectangle& rect) {
        Node<DataT>& node = nodes[node_index];

        if (node.is_leaf) {
            return node_index;
        }

        size_t best_index = 0;
        float min_area_increase = std::numeric_limits<float>::max();
        for (size_t i = 0; i < node.entries.size(); ++i) {
            Rectangle& entry_rect = node.entries[i].bounding_box;
            float area_before = entry_rect.area();
            Rectangle expanded_rect = entry_rect;
            expanded_rect.expand(rect);
            float area_increase = expanded_rect.area() - area_before;
            if (area_increase < min_area_increase) {
                min_area_increase = area_increase;
                best_index = i;
            }
        }

        node.entries[best_index].bounding_box.expand(rect);
        size_t child_index = node.entries[best_index].child_index;
        return chooseLeaf(child_index, rect);
    }


    void splitNode(size_t node_index) {
        Node<DataT>& node = nodes[node_index];
        Node<DataT> new_node(node.is_leaf);

        size_t seed1 = 0, seed2 = 1;
        float max_area_diff = -1.0f;

        for (size_t i = 0; i < node.entries.size(); ++i) {
            for (size_t j = i + 1; j < node.entries.size(); ++j) {
                Rectangle combined = node.entries[i].bounding_box;
                combined.expand(node.entries[j].bounding_box);

                float area_diff = combined.area() - node.entries[i].bounding_box.area() -
                                node.entries[j].bounding_box.area();

                if (area_diff > max_area_diff) {
                    max_area_diff = area_diff;
                    seed1 = i;
                    seed2 = j;
                }
            }
        }

        Entry<DataT> seed1_entry = std::move(node.entries[seed1]);
        Entry<DataT> seed2_entry = std::move(node.entries[seed2]);

        std::vector<Entry<DataT>> remaining_entries;
        for (size_t i = 0; i < node.entries.size(); ++i) {
            if (i != seed1 && i != seed2) {
                remaining_entries.push_back(std::move(node.entries[i]));
            }
        }

        node.entries.clear();
        node.entries.push_back(std::move(seed1_entry));
        new_node.entries.push_back(std::move(seed2_entry));

        for (auto& entry : remaining_entries) {
            Rectangle rect1 = node.entries[0].bounding_box;
            Rectangle rect2 = new_node.entries[0].bounding_box;
            Rectangle expanded1 = rect1;
            Rectangle expanded2 = rect2;
            expanded1.expand(entry.bounding_box);
            expanded2.expand(entry.bounding_box);
            float area_increase1 = expanded1.area() - rect1.area();
            float area_increase2 = expanded2.area() - rect2.area();
            if (area_increase1 < area_increase2) {
                node.entries.push_back(std::move(entry));
            } else {
                new_node.entries.push_back(std::move(entry));
            }
        }

        if (node_index == root_index) {
            root_index = createNode(false);
            Node<DataT>& root = nodes[root_index];
            size_t new_node_index = nodes.size();
            nodes.push_back(std::move(new_node));
            Rectangle mbr1 = node.entries[0].bounding_box;
            for (size_t i = 1; i < node.entries.size(); ++i) {
                mbr1.expand(node.entries[i].bounding_box);
            }

            Rectangle mbr2 = nodes[new_node_index].entries[0].bounding_box;
            for (size_t i = 1; i < nodes[new_node_index].entries.size(); ++i) {
                mbr2.expand(nodes[new_node_index].entries[i].bounding_box);
            }
            root.entries.emplace_back(mbr1);
            root.entries.back().child_index = node_index;

            root.entries.emplace_back(mbr2);
            root.entries.back().child_index = new_node_index;
        } else {
            size_t parent_index = findParent(node_index);
            Node<DataT>& parent = nodes[parent_index];
            for (auto& entry : parent.entries) {
                if (entry.child_index == node_index) {
                    Rectangle mbr = node.entries[0].bounding_box;
                    for (size_t i = 1; i < node.entries.size(); ++i) {
                        mbr.expand(node.entries[i].bounding_box);
                    }
                    entry.bounding_box = mbr;
                    break;
                }
            }

            size_t new_node_index = nodes.size();
            nodes.push_back(std::move(new_node));
            Rectangle mbr_new_node = nodes[new_node_index].entries[0].bounding_box;
            for (size_t i = 1; i < nodes[new_node_index].entries.size(); ++i) {
                mbr_new_node.expand(nodes[new_node_index].entries[i].bounding_box);
            }
            parent.entries.emplace_back(mbr_new_node);
            parent.entries.back().child_index = new_node_index;
            if (parent.entries.size() > MAX_ENTRIES) {
                splitNode(parent_index);
            }
        }
    }


    size_t findParent(size_t child_index) {
        for (size_t i = 0; i < nodes.size(); ++i) {
            Node<DataT>& node = nodes[i];
            if (!node.is_leaf) {
                for (const auto& entry : node.entries) {
                    if (entry.child_index == child_index) {
                        return i;
                    }
                }
            }
        }
        throw std::runtime_error("Parent node not found");
    }


    void distributeEntries(std::vector<Entry<DataT>>& group1,
                           std::vector<Entry<DataT>>& group2,
                           Entry<DataT>& seed1, Entry<DataT>& seed2) {
        group1.push_back(std::move(seed1));
        group2.push_back(std::move(seed2));
    }

    void rangeQueryHelper(size_t node_index, const Rectangle& rect, std::vector<DataT>& results) const {
        const Node<DataT>& node = nodes[node_index];

        for (const auto& entry : node.entries) {
            if (entry.bounding_box.overlaps(rect)) {
                if (node.is_leaf) {
                    results.push_back(*entry.data);
                } else {
                    rangeQueryHelper(entry.child_index, rect, results);
                }
            }
        }
    }
};
void runTests() {
    RTree<int> rtree;

    // Test 1: Basic insertion
    Rectangle rect1(0, 0, 5, 5);
    rtree.insert(rect1, 1);
    auto results = rtree.rangeQuery(Rectangle(0, 0, 10, 10));
    assert(results.size() == 1 && results[0] == 1);
    std::cout << "Test 1 passed!" << std::endl;

    // Test 2: Overlapping rectangles
    Rectangle rect2(6, 6, 10, 10);
    rtree.insert(rect2, 2);
    results = rtree.rangeQuery(Rectangle(0, 0, 10, 10));
    assert(results.size() == 2);
    std::cout << "Test 2 passed!" << std::endl;

    // Test 3: Non-overlapping query
    results = rtree.rangeQuery(Rectangle(15, 15, 20, 20));
    assert(results.empty());
    std::cout << "Test 3 passed!" << std::endl;

    // Test 4: Splitting nodes
    Rectangle rect3(11, 11, 15, 15);
    Rectangle rect4(16, 16, 20, 20);
    rtree.insert(rect3, 3);
    rtree.insert(rect4, 4);
    results = rtree.rangeQuery(Rectangle(10, 10, 20, 20));    
    assert(results.size() == 2 && std::find(results.begin(), results.end(), 3) != results.end());
    std::cout << "Test 4 passed!" << std::endl;
    std::cout << "All tests passed!" << std::endl;
}

int main() {
    runTests();
    return 0;
}
