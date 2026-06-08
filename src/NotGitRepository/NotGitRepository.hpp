#pragma once
#include "ObjectStore/ObjectStore.hpp"
#include <filesystem>
#include <optional>
#include <string>

std::optional<std::filesystem::path> getNgitRepoWorkingDirectory(const std::filesystem::path& path);

class NotGitRepository {
public: 
    explicit NotGitRepository(const std::filesystem::path& workingDirPath);

    static void init(const std::filesystem::path& path);
     
private:
    const std::filesystem::path workingDirPath_;
    const std::filesystem::path ngitDirPath_;
    const ObjectStore store_;
    std::string currentBranch_;
    std::optional<std::string> latestCommitSHA_;
    bool isDetached_;
};
