#pragma once

#include "BaseObject.hpp"

class BlobObject : public BaseObject {
public:
    BlobObject(const std::string& data);
    std::string header() const override;
    std::string rawData() const override;

private:
    std::string name_;
    std::string data_;
};