
#include "NotGitRepository/NotGitRepository.hpp"

#include "constants.hpp"

#include <fmt/color.h>

#include <algorithm>
#include <cstdio>
#include <ctime>
#include <filesystem>
#include <format>
#include <fstream>
#include <optional>
#include <sstream>
#include <stdexcept>
#include <unordered_map>
#include <vector>

#include "Objects/BaseObject.hpp"
#include "Objects/BlobObject.hpp"
#include "Objects/CommitObject.hpp"
#include "Objects/TreeObject.hpp"

static void create_default_files(const std::filesystem::path& ngitPath);
static void create_default_dirs(const std::filesystem::path& ngitPath);
static bool isCorrupted(const std::filesystem::path& ngitPath);
static std::string readAuthor();
static bool customCmp(const std::string& a, const std::string& b);

NotGitRepository::NotGitRepository(const std::filesystem::path& workingDirPath)
    : workingDirPath_(workingDirPath),
      ngitDirPath_(workingDirPath / ".ngit"),
      store_(ngitDirPath_ / "objects") {
    // throw if NotGit repo is corrupted
    if (isCorrupted(ngitDirPath_))
        throw std::runtime_error(".ngit/ is missing or unreadable");

    // Read .ngit/HEAD and get current branch
    std::ifstream head(ngitDirPath_ / "HEAD");
    std::string line;
    std::getline(head, line);
    if (line.starts_with("ref: refs/heads/")) {
        currentBranch_ = line.substr(std::string("ref: refs/heads/").size());
        // Read .ngit/refs/heads/<currentBranch_> and get latest commit SHA-1
        std::filesystem::path refPath =
            ngitDirPath_ / "refs" / "heads" / currentBranch_;
        if (std::filesystem::exists(refPath)) {
            std::ifstream commit(refPath);
            std::getline(commit, line);
            latestCommitSHA_ = line;
        }
    } else {
        currentBranch_ = line;
        latestCommitSHA_ = line;
        isDetached_ = true;
    }
}

NotGitRepository::IndexMap NotGitRepository::loadIndex() const {
    IndexMap result;
    std::ifstream in(ngitDirPath_ / "index");
    std::string line;
    while (std::getline(in, line)) {
        if (line.size() < constants::kIndexPathStart) continue;
        result[line.substr(constants::kIndexPathStart)] =
            line.substr(0, constants::kIndexPathStart - 1);
    }
    return result;
}

void NotGitRepository::writeIndex(const IndexMap& index) const {
    std::vector<std::string> paths;
    paths.reserve(index.size());
    for (const auto& [p, _] : index) paths.push_back(p);
    std::sort(paths.begin(), paths.end(), customCmp);

    std::ofstream out(ngitDirPath_ / "index");
    for (const auto& p : paths)
        out << index.at(p) << " " << p << "\n";
}

std::unordered_map<std::string, TreeEntry> NotGitRepository::flattenHeadTree() const {
    std::unordered_map<std::string, TreeEntry> result;
    if (!latestCommitSHA_.has_value()) return result;

    auto commitObj = store_.read(*latestCommitSHA_);
    auto* commit = dynamic_cast<CommitObject*>(commitObj.get());

    std::vector<std::pair<std::string, std::string>> stack;
    stack.push_back({commit->treeSHA(), ""});
    while (!stack.empty()) {
        auto [sha, prefix] = stack.back();
        stack.pop_back();
        auto treeObj = store_.read(sha);
        auto* tree = dynamic_cast<TreeObject*>(treeObj.get());
        for (const auto& entry : tree->entries()) {
            std::string fullPath =
                prefix.empty() ? entry.name : prefix + "/" + entry.name;
            if (entry.mode == Mode::TREE)
                stack.push_back({entry.SHA1, fullPath});
            else
                result[fullPath] = entry;
        }
    }
    return result;
}

// Implements `ngit add [file]` command
void NotGitRepository::add(std::vector<std::filesystem::path> paths) {
    auto indexMap = loadIndex();

    while (!paths.empty()) {
        std::filesystem::path p = paths.back();
        paths.pop_back();

        if (std::filesystem::is_regular_file(p)) {
            std::ifstream file(p, std::ios::binary);
            std::string data((std::istreambuf_iterator<char>(file)),
                             std::istreambuf_iterator<char>());

            BlobObject obj(data);
            std::string sha1 = store_.write(obj);
            std::string relativePath =
                std::filesystem::relative(p, workingDirPath_).string();

            auto perms = std::filesystem::status(p).permissions();
            bool isExecutable = (perms & std::filesystem::perms::owner_exec) !=
                                std::filesystem::perms::none;
            std::string mode = isExecutable ? Mode::BLOB_E : Mode::BLOB_RW;

            indexMap[relativePath] = mode + " " + sha1;

        } else if (std::filesystem::is_directory(p)) {
            for (const auto& entry : std::filesystem::directory_iterator(p)) {
                if (entry.path().filename() == ".ngit") continue;
                paths.push_back(entry);
            }
        }
    }

    writeIndex(indexMap);
}

// Implements `ngit commit` command
void NotGitRepository::commit(const std::string& message) {
    std::unordered_map<std::string, std::vector<TreeEntry>> pathToTreeEntryMap;
    for (const auto& [path, modeAndSha] : loadIndex()) {
        std::string mode = modeAndSha.substr(0, 6);
        std::string SHA1 = modeAndSha.substr(constants::kIndexSHA1Start, constants::kIndexSHA1Len);
        std::filesystem::path filePath = path;
        pathToTreeEntryMap[filePath.parent_path().string()].emplace_back(
            filePath.filename().string(), mode, SHA1);
        std::filesystem::path anc = filePath.parent_path().parent_path();
        while (!anc.empty()) {
            pathToTreeEntryMap[anc.string()];
            anc = anc.parent_path();
        }
        pathToTreeEntryMap[""];
    }

    if (pathToTreeEntryMap.empty())
        throw std::runtime_error("nothing to commit (no files staged)");

    std::vector<std::string> dirNames;
    for (const auto& [path, treeEntry] : pathToTreeEntryMap)
        dirNames.push_back(path);
    std::sort(dirNames.begin(), dirNames.end(), customCmp);

    std::string rootSHA;

    for (const std::string& name : dirNames) {
        TreeObject tree(pathToTreeEntryMap[name]);
        std::string treeSha1 = store_.write(tree);
        std::filesystem::path path = name;
        std::filesystem::path parentPath = path.parent_path();
        if (name.empty()) {
            rootSHA = treeSha1;
        } else {
            pathToTreeEntryMap[parentPath.string()].emplace_back(
                path.filename().string(), Mode::TREE, treeSha1);
        }
    }

    // if no files have been changed then just show `ngit status` output
    if (latestCommitSHA_.has_value()) {
        auto objPtr = store_.read(*latestCommitSHA_);
        CommitObject* obj = dynamic_cast<CommitObject*>(objPtr.get());

        if (obj->treeSHA() == rootSHA) {
            status();
            return;
        }
    }

    Commit c;
    c.commitMsg = message;
    c.treeSHA = rootSHA;
    c.parentCommitSHA = latestCommitSHA_;
    std::string author = readAuthor();
    std::time_t now = std::time(nullptr);
    char tz[6];
    std::strftime(tz, sizeof(tz), "%z", std::localtime(&now));
    author += " " + std::to_string(now) + " " + tz;
    c.author = author;
    c.committer = author;

    CommitObject commit(c);
    std::string commitSHA = store_.write(commit);

    std::ofstream(ngitDirPath_ / "refs" / "heads" / currentBranch_)
        << commitSHA << "\n";
    latestCommitSHA_ = commitSHA;

    fmt::print(fg(fmt::color::green), "[{} {}] {}\n", currentBranch_,
               commitSHA.substr(0, 7), message);
}

void NotGitRepository::log(bool oneline) {
    if (!latestCommitSHA_.has_value())
        throw std::runtime_error(std::format(
            "fatal: your current branch '{}' does not have any commits yet",
            currentBranch_));

    std::string output;
    std::optional<std::string> sha = latestCommitSHA_;

    while (sha.has_value()) {
        auto obj = store_.read(*sha);
        auto* commit = dynamic_cast<CommitObject*>(obj.get());
        if (!commit) break;

        if (oneline) {
            output +=
                fmt::format(fg(fmt::color::yellow), "{}", sha->substr(0, 7));
            output += std::format(
                " {}\n",
                commit->commitMsg().substr(0, commit->commitMsg().find('\n')));
        } else {
            // Parse "Name <email> <unix_ts> <tz>" — last two tokens are
            // timestamp and timezone
            const std::string& authorStr = commit->author();
            size_t lastSpace = authorStr.rfind(' ');
            std::string timezone = authorStr.substr(lastSpace + 1);
            size_t secondLastSpace = authorStr.rfind(' ', lastSpace - 1);
            std::string identity = authorStr.substr(0, secondLastSpace);
            std::time_t ts = std::stol(authorStr.substr(
                secondLastSpace + 1, lastSpace - secondLastSpace - 1));

            char dateBuf[64];
            std::strftime(dateBuf, sizeof(dateBuf), "%a %b %e %H:%M:%S %Y",
                          std::localtime(&ts));
            output += fmt::format(fg(fmt::color::yellow), "commit {}\n", *sha);
            output += std::format("Author: {}\n", identity);
            output += std::format("Date:   {} {}\n\n", dateBuf, timezone);
            std::istringstream msgStream(commit->commitMsg());
            std::string line;
            while (std::getline(msgStream, line))
                output += "    " + line + "\n";
            output += "\n";
        }

        sha = commit->parentCommitSHA();
    }

    FILE* pager = popen("less -F -R -X", "w");
    if (!pager) {
        fmt::print("{}", output);
        return;
    }
    fputs(output.c_str(), pager);
    pclose(pager);
}

void NotGitRepository::status() {
    auto currentStateMap = loadIndex();
    auto lastCommitStateMap = flattenHeadTree();

    // {label, path} buckets
    std::vector<std::pair<std::string, std::string>> staged;
    std::vector<std::pair<std::string, std::string>> unstaged;
    std::vector<std::string> untracked;

    std::vector<std::filesystem::path> stack{workingDirPath_};
    while (!stack.empty()) {
        auto p = stack.back();
        stack.pop_back();
        if (std::filesystem::is_regular_file(p)) {
            std::string rel =
                std::filesystem::relative(p, workingDirPath_).string();
            if (!currentStateMap.contains(rel)) {
                untracked.push_back(rel);
            } else {
                std::ifstream file(p, std::ios::binary);
                std::string data((std::istreambuf_iterator<char>(file)),
                                 std::istreambuf_iterator<char>());
                if (BlobObject(data).sha1() != currentStateMap[rel].substr(constants::kIndexSHA1Start, constants::kIndexSHA1Len))
                    unstaged.push_back({"modified", rel});
            }
        } else if (std::filesystem::is_directory(p)) {
            for (const auto& entry : std::filesystem::directory_iterator(p)) {
                if (entry.path().filename() == ".ngit") continue;
                stack.push_back(entry);
            }
        }
    }

    // Unstaged deleted: in index but missing from disk
    for (const auto& [path, sha1] : currentStateMap) {
        if (!std::filesystem::exists(workingDirPath_ / path))
            unstaged.push_back({"deleted", path});
    }

    // Staged changes: index vs HEAD
    for (const auto& [path, sha] : currentStateMap) {
        if (!lastCommitStateMap.contains(path))
            staged.push_back({"new file", path});
        else if (lastCommitStateMap[path].SHA1 != sha.substr(constants::kIndexSHA1Start, constants::kIndexSHA1Len))
            staged.push_back({"modified", path});
    }
    // Staged deleted: in HEAD but removed from index
    for (const auto& [path, entry] : lastCommitStateMap) {
        if (!currentStateMap.contains(path))
            staged.push_back({"deleted", path});
    }

    fmt::print("On branch {}\n\n", currentBranch_);

    if (!staged.empty()) {
        fmt::print("Changes to be committed:\n");
        fmt::print(fg(fmt::color::gray),
                   "  (use \"ngit restore --staged <file>...\" to unstage)\n");
        for (const auto& [label, path] : staged)
            fmt::print(fg(fmt::color::green), "\t{:10}{}\n", label + ":", path);
        fmt::print("\n");
    }

    if (!unstaged.empty()) {
        fmt::print("Changes not staged for commit:\n");
        fmt::print(fg(fmt::color::gray),
                   "  (use \"ngit add <file>...\" to update what will be "
                   "committed)\n");
        for (const auto& [label, path] : unstaged)
            fmt::print(fg(fmt::color::red), "\t{:10}{}\n", label + ":", path);
        fmt::print("\n");
    }

    if (!untracked.empty()) {
        fmt::print("Untracked files:\n");
        fmt::print(fg(fmt::color::gray),
                   "  (use \"ngit add <file>...\" to include in what will be "
                   "committed)\n");
        for (const auto& path : untracked)
            fmt::print(fg(fmt::color::red), "\t{}\n", path);
        fmt::print("\n");
    }

    if (staged.empty() && unstaged.empty() && untracked.empty())
        fmt::print("nothing to commit, working tree clean\n");
}

void NotGitRepository::restore(std::vector<std::filesystem::path> paths,
                               bool staged) {
    auto indexMap = loadIndex();
    auto lastCommitStateMap = flattenHeadTree();

    while (!paths.empty()) {
        auto path = paths.back();
        paths.pop_back();
        if (std::filesystem::is_regular_file(path)) {
            std::string rel =
                std::filesystem::relative(path, workingDirPath_).string();
            if (staged) {
                if (!lastCommitStateMap.contains(rel))
                    indexMap.erase(rel);  // new file: remove from index
                else {
                    const auto& e = lastCommitStateMap[rel];
                    indexMap[rel] = e.mode + " " + e.SHA1;  // revert to HEAD
                }
            } else {
                if (!indexMap.contains(rel))
                    throw std::runtime_error(
                        std::format("error: pathspec '{}' did not match any "
                                    "file(s) known to ngit",
                                    rel));
                // restore working directory file from index
                std::string sha = indexMap[rel].substr(constants::kIndexSHA1Start, constants::kIndexSHA1Len);
                auto obj = store_.read(sha);
                auto* blob = dynamic_cast<BlobObject*>(obj.get());
                std::ofstream out(path, std::ios::binary);
                out << blob->rawData();
            }
        } else if (std::filesystem::is_directory(path)) {
            for (const auto& entry :
                 std::filesystem::directory_iterator(path)) {
                if (entry.path().filename() == ".ngit") continue;
                paths.push_back(entry);
            }
        }
    }

    if (staged) writeIndex(indexMap);
}

void NotGitRepository::rm(std::vector<std::filesystem::path> paths,
                          bool cached) {
    auto indexMap = loadIndex();

    while (!paths.empty()) {
        auto path = paths.back();
        paths.pop_back();
        if (std::filesystem::is_regular_file(path)) {
            std::string rel =
                std::filesystem::relative(path, workingDirPath_).string();
            if (!indexMap.contains(rel))
                throw std::runtime_error(
                    std::format("error: pathspec '{}' did not match any "
                                "file(s) known to ngit",
                                rel));
            indexMap.erase(rel);
            if (!cached) std::filesystem::remove(path);
        } else if (std::filesystem::is_directory(path)) {
            for (const auto& entry :
                 std::filesystem::directory_iterator(path)) {
                if (entry.path().filename() == ".ngit") continue;
                paths.push_back(entry);
            }
        } else {
            throw std::runtime_error(std::format(
                "error: pathspec '{}' did not match any file(s) known to ngit",
                path.string()));
        }
    }

    writeIndex(indexMap);
}

void NotGitRepository::init(const std::filesystem::path& path) {
    /*
        1. If .ngit/ folder does not exist, then init command should:
            - Create a .ngit folder with
                - objects/
                - refs/heads/
                - refs/tags/
                - HEAD
                - config
                - description
        2. If .ngit/ folder already exists, then do not touch anything and
       return a Valid message
    */
    std::filesystem::path absolutePath = std::filesystem::absolute(path);

    std::optional<std::filesystem::path> f =
        getNgitRepoWorkingDirectory(absolutePath);
    if (f.has_value()) {  // if ngit repo already exists then just exit
        const std::filesystem::path ngitPath = f.value() / ".ngit";
        fmt::print(fmt::emphasis::bold | fg(fmt::color::yellow),
                   "NotGit repository already exists at {} !\n",
                   fmt::styled(ngitPath.string(), fmt::emphasis::underline));
        return;
    }

    const std::filesystem::path ngitPath = absolutePath / ".ngit";

    // Create the directories
    create_default_dirs(ngitPath);
    // Create the files
    create_default_files(ngitPath);

    fmt::print(fmt::emphasis::bold | fg(fmt::color::green),
               "Initialised empty NotGit repository in {}\n",
               fmt::styled(ngitPath.string(), fmt::emphasis::underline));
}

static void create_default_dirs(const std::filesystem::path& ngitPath) {
    std::filesystem::create_directories(ngitPath / "objects");
    std::filesystem::create_directories(ngitPath / "refs" / "heads");
    std::filesystem::create_directories(ngitPath / "refs" / "tags");
}

static void create_default_files(const std::filesystem::path& ngitPath) {
    std::ofstream(ngitPath / "description")
        << "Unnamed repository; edit this file 'description' to name the "
           "repository.\n";
    std::ofstream(ngitPath / "HEAD") << "ref: refs/heads/main\n";
    std::ofstream(ngitPath / "config") << "[core]\n"
                                       << "\trepositoryformatversion = 0\n"
                                       << "\tfilemode = true\n"
                                       << "\tbare = false\n";
}

// Checks if .ngit folder has any missing files or subdirectories
static bool isCorrupted(const std::filesystem::path& ngitPath) {
    return !std::filesystem::exists(ngitPath / "HEAD") ||
           !std::filesystem::exists(ngitPath / "objects") ||
           !std::filesystem::exists(ngitPath / "refs" / "heads") ||
           !std::filesystem::exists(ngitPath / "refs" / "tags") ||
           !std::filesystem::exists(ngitPath / "config");
}

// Checks for .ngit folder recursively in parent directory to determine
// if ngit repo already exists and returns working directory
std::optional<std::filesystem::path> getNgitRepoWorkingDirectory(
    const std::filesystem::path& path) {
    std::filesystem::path curr = path;
    while (curr != curr.parent_path()) {
        if (std::filesystem::exists(curr / ".ngit")) {
            return curr;
        }
        curr = curr.parent_path();
    }
    return std::nullopt;
}

static std::string readAuthor() {
    const char* home = std::getenv("HOME");
    if (!home) throw std::runtime_error("HOME environment variable not set");

    std::ifstream f(std::filesystem::path(home) / ".gitconfig");
    if (!f) throw std::runtime_error("~/.gitconfig not found");

    std::string line, name, email;
    bool inUser = false;
    while (std::getline(f, line)) {
        if (line == "[user]") {
            inUser = true;
            continue;
        }
        if (line.starts_with("[")) {
            inUser = false;
            continue;
        }
        if (!inUser) continue;
        size_t eq = line.find('=');
        if (eq == std::string::npos) continue;
        std::string key = line.substr(0, eq);
        std::string val = line.substr(eq + 1);
        key.erase(0, key.find_first_not_of(" \t"));
        key.erase(key.find_last_not_of(" \t") + 1);
        val.erase(0, val.find_first_not_of(" \t"));
        val.erase(val.find_last_not_of(" \t") + 1);
        if (key == "name")
            name = val;
        else if (key == "email")
            email = val;
    }
    if (name.empty())
        throw std::runtime_error("user.name not set in ~/.gitconfig");
    return email.empty() ? name : name + " <" + email + ">";
}

static bool customCmp(const std::string& a, const std::string& b) {
    auto depth = [](const std::string& p) {
        return p.empty() ? 0 : (int)std::count(p.begin(), p.end(), '/') + 1;
    };
    return depth(a) > depth(b);
}