#pragma once
#include <filesystem>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

#include "ObjectStore/ObjectStore.hpp"
#include "Objects/TreeObject.hpp"

std::optional<std::filesystem::path> getNgitRepoWorkingDirectory(
    const std::filesystem::path& path);

class NotGitRepository {
  public:
    explicit NotGitRepository(const std::filesystem::path& workingDirPath);

    void add(std::vector<std::filesystem::path> paths);

    void commit(const std::string& message);

    void log(bool oneline);

    void status();

    void restore(std::vector<std::filesystem::path> paths, bool staged);

    void rm(std::vector<std::filesystem::path> paths, bool cached);

    static void init(const std::filesystem::path& path);

  private:
    const std::filesystem::path workingDirPath_;
    const std::filesystem::path ngitDirPath_;
    ObjectStore store_;

    std::string currentBranch_;
    std::optional<std::string> latestCommitSHA_;
    bool isDetached_;

    using IndexMap = typename std::unordered_map<std::string, std::string>;

    IndexMap loadIndex() const;
    void writeIndex(const IndexMap& index) const;
    std::unordered_map<std::string, TreeEntry> flattenHeadTree() const;
};
