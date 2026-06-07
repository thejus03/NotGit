#pragma once
#include <string>
#include <filesystem>

class NotGitRepository {
public: 
    static void init(const std::filesystem::path& path);
    
private:
    std::filesystem::path workingDirPath_;
    std::filesystem::path ngitDirPath;
};