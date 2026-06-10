#include "BlobObject.hpp"

#include <format>

BlobObject::BlobObject(const std::string& data) : data_(data) {}

std::string BlobObject::header() const {
    return std::format("blob {}", data_.size()) + '\0';
}

std::string BlobObject::rawData() const { return data_; }
