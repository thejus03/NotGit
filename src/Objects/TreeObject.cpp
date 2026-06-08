#include "TreeObject.hpp"
#include <vector>
#include <utility>
#include <algorithm>
#include <format>

static std::string hexToBinary(const std::string& hex);

TreeObject::TreeObject(std::vector<TreeEntry> entities) 
    : 
    entities_(std::move(entities)) {
    std::sort(entities_.begin(), entities_.end(), [](const TreeEntry& a, const TreeEntry& b) {
        return a.name < b.name;
    });
    data_ = buildRawData();
}

std::string TreeObject::header() const {
    return std::format("tree {}", data_.size()) + '\0';
}

std::string TreeObject::rawData() const { return data_; }


std::string TreeObject::buildRawData() const {

    std::string res;
    for (const auto& entityPtr : entities_) {
        res += std::format("{} {}", entityPtr.mode, entityPtr.name);
        res += '\0';
        res += hexToBinary(entityPtr.SHA1);
    }
    return res;
}


static std::string hexToBinary(const std::string& hex) {
    std::string binary;
    binary.reserve(hex.size() / 2);
    for (size_t i = 0; i < hex.size(); i += 2)
        binary += static_cast<char>(std::stoi(hex.substr(i, 2), nullptr, 16));
    return binary;
}