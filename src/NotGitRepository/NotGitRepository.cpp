
#include "NotGitRepository/NotGitRepository.hpp"
#include <filesystem>
#include <fmt/color.h>
#include <fstream>

static void create_default_files(const std::filesystem::path& ngitPath); 
static void create_default_dirs(const std::filesystem::path& ngitPath);

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
    std::filesystem::path absolutePath = std::filesystem::absolute(path); // converts the path provided by user to absolute path

    std::filesystem::path curr = absolutePath;
    while (curr != curr.parent_path()) {
        if (std::filesystem::exists(curr / ".ngit")) {
            fmt::print(fmt::emphasis::bold | fg(fmt::color::yellow), "NotGit repository already exists at {} !\n", fmt::styled(curr.string(), fmt::emphasis::underline));
            return;
        }
        curr = curr.parent_path();
    }

    const std::filesystem::path ngitPath = absolutePath / ".ngit";

    // Create the directories
    create_default_dirs(ngitPath);
    // Create the files
    create_default_files(ngitPath);

    fmt::print(fg(fmt::color::green), "Initialised empty NotGit repository in {}\n", ngitPath.string());
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