
#include "NotGitRepository/NotGitRepository.hpp"
#include <filesystem>
#include <fmt/color.h>
#include <fstream>
#include <optional>
#include <stdexcept>

static void create_default_files(const std::filesystem::path& ngitPath); 
static void create_default_dirs(const std::filesystem::path& ngitPath);
static bool isCorrupted(const std::filesystem::path& ngitPath);

NotGitRepository::NotGitRepository(const std::filesystem::path& workingDirPath) : workingDirPath_(workingDirPath), ngitDirPath_(workingDirPath / ".ngit") {

    // throw if NotGit repo is corrupted
    if (isCorrupted(ngitDirPath_)) 
        throw std::runtime_error(".ngit/ is missing or unreadable");
    
    // Read .ngit/HEAD and get current branch
    std::ifstream head(ngitDirPath_ / "HEAD");
    std::string line;
    std::getline(head, line);
    currentBranch_ = line.substr(std::string("ref: refs/heads/").size());

    // Read .ngit/heads/<currentBranch_> and get latest commit SHA-1
    std::filesystem::path refPath = ngitDirPath_ / "refs" / "heads" / currentBranch_;
    if (std::filesystem::exists(refPath)) {
        std::ifstream commit(refPath);
        std::getline(commit, line);
        latestCommitSHA_ = line;
    } 
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

// static bool isAlreadyNgitRepo(const std::filesystem::path& path) {
//     std::optional<std::filesystem::path> workingDirectory = getNgitRepoWorkingDirectory(path);
//     return workingDirectory.has_value();
// }

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