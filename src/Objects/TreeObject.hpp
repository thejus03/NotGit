#pragma once

#include <vector>

#include "BaseObject.hpp"

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
    const std::vector<TreeEntry>& entries() const;

  private:
    std::vector<TreeEntry> entities_;
    std::string data_;

    std::string buildRawData() const;
};
