#pragma once

#include <string>

class BaseObject {
public:
    virtual std::string header() const = 0;
    virtual std::string rawData() const = 0;
    std::string sha1() const;
    virtual ~BaseObject() = default;
};

namespace Mode {
    const std::string BLOB_RW = "100644";
    const std::string BLOB_E = "100755";
    const std::string TREE = "040000";
    const std::string COMMIT = "160000";
}