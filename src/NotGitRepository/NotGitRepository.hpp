#pragma once
#include "ObjectStore/ObjectStore.hpp"
#include "Objects/BaseObject.hpp"
#include "Objects/BlobObject.hpp"
#include <filesystem>
#include <optional>
#include <string>
#include <fstream>
#include <vector>
#include <unordered_map>

std::optional<std::filesystem::path> getNgitRepoWorkingDirectory(const std::filesystem::path& path);

class NotGitRepository {
public: 
    explicit NotGitRepository(const std::filesystem::path& workingDirPath);

    void add(const std::filesystem::path& path) {
        // load existing index into map (path -> "mode sha1")
        std::unordered_map<std::string, std::string> indexMap;
        std::ifstream indexIn(ngitDirPath_ / "index");
        std::string line;
        while (std::getline(indexIn, line)) {
            size_t lastSpace = line.rfind(' ');
            if (lastSpace != std::string::npos)
                indexMap[line.substr(lastSpace + 1)] = line.substr(0, lastSpace);
        }
        indexIn.close();

        std::vector<std::filesystem::path> stack {path};
        while (!stack.empty()) {
            std::filesystem::path p = stack.back();
            stack.pop_back();

            if (std::filesystem::is_regular_file(p)) {
                std::ifstream file(p, std::ios::binary);
                std::string data((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());

                BlobObject obj(data);
                std::string sha1 = store_.write(obj);
                std::string relativePath = std::filesystem::relative(p, workingDirPath_).string();

                auto perms = std::filesystem::status(p).permissions();
                bool isExecutable = (perms & std::filesystem::perms::owner_exec) != std::filesystem::perms::none;
                std::string mode = isExecutable ? Mode::BLOB_E : Mode::BLOB_RW;

                indexMap[relativePath] = mode + " " + sha1;

            } else if (std::filesystem::is_directory(p)) {
                for (const auto& entry : std::filesystem::directory_iterator(p)) {
                    if (entry.path().filename() == ".ngit") continue;
                    stack.push_back(entry);
                }
            }
        }

        // write index back
        std::ofstream indexOut(ngitDirPath_ / "index");
        for (const auto& [filePath, modeAndSha] : indexMap)
            indexOut << modeAndSha << " " << filePath << "\n";
    }

    static void init(const std::filesystem::path& path);
     
private:
    const std::filesystem::path workingDirPath_;
    const std::filesystem::path ngitDirPath_;
    ObjectStore store_;

    std::string currentBranch_;
    std::optional<std::string> latestCommitSHA_;
    bool isDetached_;
};
