
#include "NotGitRepository/NotGitRepository.hpp"
#include <filesystem>
#include <fmt/color.h>

void NotGitRepository::init (const std::filesystem::path& path) {
    /*
        1. If .ngit/ folder does not exist, then init command should:
            - Create a .ngit folder with 
                - objects/
                - refs/
                - HEAD
                - config
                - description
        2. If .ngit/ folder already exists, then do not touch anything and return a Valid message
    */

    const std::filesystem::path ngitPath = path / ".ngit";
    if (std::filesystem::exists(ngitPath)) {
        fmt::print(fmt::emphasis::bold | fg(fmt::color::yellow), "NotGit repository already exists at {} !\n", path.string());
        return;
    }

    std::filesystem::create_directories(ngitPath / "objects");
    std::filesystem::create_directories(ngitPath / "refs" / "heads");
    std::filesystem::create_directories(ngitPath / "refs" / "tags");
     
    
}