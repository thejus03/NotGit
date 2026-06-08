
#include "NotGitRepository/NotGitRepository.hpp"
#include "Objects/BlobObject.hpp"
#include <filesystem>
#include <fmt/color.h>
#include <fstream>
#include <optional>
#include <stdexcept>
#include <unordered_map>
#include <vector>

static void create_default_files(const std::filesystem::path& ngitPath); 
static void create_default_dirs(const std::filesystem::path& ngitPath);
static bool isCorrupted(const std::filesystem::path& ngitPath);

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
        // Read .ngit/heads/<currentBranch_> and get latest commit SHA-1
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