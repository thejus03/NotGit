#pragma once

#include "BaseObject.hpp"
#include <vector>

struct TreeEntry {
    std::string name;
    std::string mode;
    std::string SHA1;
};

class TreeObject : public BaseObject {
public:
    TreeObject(std::vector<TreeEntry> entities);
    std::string header() const override;
    std::string rawData() const override;

private:
    std::vector<TreeEntry> entities_;
    std::string data_;

    std::string buildRawData() const;

};
