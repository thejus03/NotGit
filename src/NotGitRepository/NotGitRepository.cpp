
#include "NotGitRepository/NotGitRepository.hpp"
#include "Objects/BaseObject.hpp"
#include "Objects/BlobObject.hpp"
#include "Objects/TreeObject.hpp"
#include <filesystem>
#include <fmt/color.h>
#include <fstream>
#include <optional>
#include <stdexcept>
#include <unordered_map>
#include <vector>
#include <algorithm>
#include <ctime>
#include "Objects/CommitObject.hpp"

static void create_default_files(const std::filesystem::path& ngitPath);
static void create_default_dirs(const std::filesystem::path& ngitPath);
static bool isCorrupted(const std::filesystem::path& ngitPath);
static std::string readAuthor(const std::filesystem::path& ngitDirPath);
static bool customCmp(const std::string& a, const std::string& b);

NotGitRepository::NotGitRepository(const std::filesystem::path& workingDirPath) : workingDirPath_(workingDirPath), ngitDirPath_(workingDirPath / ".ngit"), store_(ngitDirPath_ / "objects") {

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
        std::filesystem::path refPath = ngitDirPath_ / "refs" / "heads" / currentBranch_;
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

// Implements `ngit add [file]` command
void NotGitRepository::add(const std::filesystem::path& path) {
    // load existing index into map (path -> "mode sha1")
    std::unordered_map<std::string, std::string> indexMap;
    std::ifstream indexIn(ngitDirPath_ / "index");
    std::string line;
    while (std::getline(indexIn, line)) {
        if (line.size() < 48) continue;
        indexMap[line.substr(48)] = line.substr(0, 47); // "mode sha1"
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
    std::vector<std::string> filePaths;
    for (const auto& [filePath, modeAndSha] : indexMap) filePaths.push_back(filePath);
    std::sort(filePaths.begin(), filePaths.end(), customCmp);

    std::ofstream indexOut(ngitDirPath_ / "index");
    for (const std::string& filePath : filePaths) {
        const std::string& modeAndSha = indexMap[filePath];
        indexOut << modeAndSha << " " << filePath << "\n";
    }
}


// Implements `ngit commit` command
void NotGitRepository::commit(const std::string& message) {
    
    std::unordered_map<std::string, std::vector<TreeEntry>> pathToTreeEntryMap;
    std::ifstream indexIn(ngitDirPath_ / "index");
    std::string line;
    while (std::getline(indexIn, line)) {
        if (line.size() < 48) continue;
        std::string mode = line.substr(0, 6);
        std::string SHA1 = line.substr(7, 40);
        std::string path = line.substr(48);
        std::filesystem::path filePath = path;
        pathToTreeEntryMap[filePath.parent_path().string()].emplace_back(filePath.filename().string(), mode, SHA1);
        std::filesystem::path anc = filePath.parent_path().parent_path();
        while (!anc.empty()) {
            pathToTreeEntryMap[anc.string()];
            anc = anc.parent_path();
        }
        pathToTreeEntryMap[""];
    }
    indexIn.close();

    if (pathToTreeEntryMap.empty())
        throw std::runtime_error("nothing to commit (no files staged)");

    std::vector<std::string> dirNames;
    for (const auto& [path, treeEntry]: pathToTreeEntryMap) dirNames.push_back(path);
    std::sort(dirNames.begin(), dirNames.end(), customCmp);

    std::string rootSHA;    

    for (const std::string& name : dirNames) {
        TreeObject tree (pathToTreeEntryMap[name]);
        std::string treeSha1 = store_.write(tree);
        std::filesystem::path path = name;
        std::filesystem::path parentPath = path.parent_path();
        if (name.empty()) {
            rootSHA = treeSha1;
        } else{
            pathToTreeEntryMap[parentPath.string()].emplace_back(path.filename().string(), Mode::TREE, treeSha1);
        }
    }
    Commit c;
    c.commitMsg = message;
    c.treeSHA = rootSHA;
    c.parentCommitSHA = latestCommitSHA_;
    std::string author = readAuthor(ngitDirPath_);
    std::time_t now = std::time(nullptr);
    char tz[6];
    std::strftime(tz, sizeof(tz), "%z", std::localtime(&now));
    author += " " + std::to_string(now) + " " + tz;
    c.author = author;
    c.committer = author;

    CommitObject commit(c);
    std::string commitSHA = store_.write(commit);

    std::ofstream(ngitDirPath_ / "refs" / "heads" / currentBranch_) << commitSHA << "\n";
    latestCommitSHA_ = commitSHA;

    fmt::print(fg(fmt::color::green), "[{} {}] {}\n", currentBranch_, commitSHA.substr(0, 7), message);
}

void NotGitRepository::init (const std::filesystem::path& path) {
    /*
        1. If .ngit/ folder does not exist, then init command should:
            - Create a .ngit folder with 
                - objects/
                - refs/heads/
                - refs/tags/
                - HEAD
                - config
                - description
        2. If .ngit/ folder already exists, then do not touch anything and return a Valid message
    */
    std::filesystem::path absolutePath = std::filesystem::absolute(path);

    std::optional<std::filesystem::path> f = getNgitRepoWorkingDirectory(absolutePath);
    if (f.has_value()) {// if ngit repo already exists then just exit
        const std::filesystem::path ngitPath = f.value() / ".ngit";
        fmt::print(fmt::emphasis::bold | fg(fmt::color::yellow), "NotGit repository already exists at {} !\n", fmt::styled(ngitPath.string(), fmt::emphasis::underline));
        return;
    }

    const std::filesystem::path ngitPath = absolutePath / ".ngit";

    // Create the directories
    create_default_dirs(ngitPath);
    // Create the files
    create_default_files(ngitPath);

    fmt::print(fmt::emphasis::bold | fg(fmt::color::green), "Initialised empty NotGit repository in {}\n", fmt::styled(ngitPath.string(), fmt::emphasis::underline));
}

static void create_default_dirs(const std::filesystem::path& ngitPath) {
    std::filesystem::create_directories(ngitPath / "objects");
    std::filesystem::create_directories(ngitPath / "refs" / "heads");
    std::filesystem::create_directories(ngitPath / "refs" / "tags");
}

static void create_default_files(const std::filesystem::path& ngitPath) {
    std::ofstream(ngitPath / "description") << "Unnamed repository; edit this file 'description' to name the repository.\n";
    std::ofstream(ngitPath / "HEAD") << "ref: refs/heads/main\n";
    std::ofstream(ngitPath / "config")
        << "[core]\n"
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
std::optional<std::filesystem::path> getNgitRepoWorkingDirectory(const std::filesystem::path& path) {
    std::filesystem::path curr = path;
    while (curr != curr.parent_path()) {
        if (std::filesystem::exists(curr / ".ngit")) {
            return curr;
        }
        curr = curr.parent_path();
    }
    return std::nullopt;
}

static std::string readAuthor(const std::filesystem::path&) {
    const char* home = std::getenv("HOME");
    if (!home)
        throw std::runtime_error("HOME environment variable not set");

    std::ifstream f(std::filesystem::path(home) / ".gitconfig");
    if (!f)
        throw std::runtime_error("~/.gitconfig not found");

    std::string line, name, email;
    bool inUser = false;
    while (std::getline(f, line)) {
        if (line == "[user]") { inUser = true; continue; }
        if (line.starts_with("[")) { inUser = false; continue; }
        if (!inUser) continue;
        size_t eq = line.find('=');
        if (eq == std::string::npos) continue;
        std::string key = line.substr(0, eq);
        std::string val = line.substr(eq + 1);
        key.erase(0, key.find_first_not_of(" \t"));
        key.erase(key.find_last_not_of(" \t") + 1);
        val.erase(0, val.find_first_not_of(" \t"));
        val.erase(val.find_last_not_of(" \t") + 1);
        if (key == "name") name = val;
        else if (key == "email") email = val;
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