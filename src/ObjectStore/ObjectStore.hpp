#pragma once

#include <string>
#include <filesystem>
#include "Objects/BaseObject.hpp"

class ObjectStore {
public:
    ObjectStore(const std::filesystem::path& objectDirPath);
    std::unique_ptr<BaseObject> read(const std::string& sha1) const;
    std::string write(const BaseObject& obj) const;
    bool exists(const std::string& sha1) const;
private:
    std::filesystem::path objectDirPath_;

    std::string decompress_(const std::string& compressed) const;
    std::string compress_(const std::string& data) const;
    std::filesystem::path objectFilePathFromSHA_(const std::string& sha1) const;
};