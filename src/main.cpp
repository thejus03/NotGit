#include "NotGitRepository/NotGitRepository.hpp"
#include <exception>
#include <filesystem>
#include <iostream>
#include <CLI11.hpp>
#include <optional>
#include <string>
#include <fmt/color.h>

int main(int argc, char** argv) {
    CLI::App app {};
    argv = app.ensure_utf8(argv);

    std::filesystem::path initPath; 
    auto init = app.add_subcommand("init", "Create an empty NotGit repository or reinitialise an exisiting one");
    init->add_option("path", initPath, "Path to initialise repository")->expected(0, 1); // adds optional path to `ngit init` command

    std::vector<std::filesystem::path> addPaths;
    auto add = app.add_subcommand("add", "Add file contents to index");
    add->add_option("paths", addPaths, "Files or directories to add")->required()->expected(1, -1);

    std::string commitMessage;
    auto commit = app.add_subcommand("commit", "Record changes to repository");
    commit->add_option("-m,--message", commitMessage, "Commit message")->required();

    app.add_subcommand("status", "Show working tree status");
    app.add_subcommand("checkout", "Switch branched or restore working tree files");
    app.add_subcommand("merge", "Switch branched or restore working tree files");
    bool cached = false;
    std::vector<std::filesystem::path> rmPaths;
    auto rm = app.add_subcommand("rm", "Remove files from the index");
    rm->add_flag("--cached", cached, "Remove from index only, keep file on disk");
    rm->add_option("paths", rmPaths, "Files to remove")->required()->expected(1, -1);
    
    bool staged = false;
    std::vector<std::filesystem::path> restorePaths;
    auto restore = app.add_subcommand("restore", "Restore working tree files");
    restore->add_flag("-S,--staged", staged, "Restore staged files" );
    restore->add_option("paths", restorePaths, "Restore file paths");

    bool oneline = false;
    auto log = app.add_subcommand("log", "Show commit logs");
    log->add_flag("--oneline", oneline, "Show each commit as a single line");

    app.add_subcommand("diff", "Switch branched or restore working tree files");

    // Allow random commands so that error not thrown by CLI11.
    // Will be caught in else branch for cleaner error message for user.
    app.allow_extras(); 

    CLI11_PARSE(app, argc, argv); //  Parse cli commands here!

    if (app.got_subcommand("init")) {
        NotGitRepository::init(initPath);
    } else {
        std::optional<std::filesystem::path> workingDir = getNgitRepoWorkingDirectory(std::filesystem::current_path());
        if (!workingDir.has_value()) {
            fmt::print(fmt::emphasis::bold | fg(fmt::color::red), "fatal: not a NotGit repository (or any parent up to root)\n");
            return 1;
        }

        try {

            NotGitRepository ngitRepo (workingDir.value());

            if (app.got_subcommand("add")) {
                ngitRepo.add(addPaths);
            } else if (app.got_subcommand("commit")) {
                ngitRepo.commit(commitMessage);
            } else if (app.got_subcommand("status")) {
                ngitRepo.status();
            } else if (app.got_subcommand("checkout")) {
                std::cout << "checkout command called!" << std::endl;
            } else if (app.got_subcommand("merge")) {
                std::cout << "merge command called!" << std::endl;
            } else if (app.got_subcommand("rm")) {
                std::cout << "rm command called!" << std::endl;
            } else if (app.got_subcommand("log")) {
                ngitRepo.log(oneline);
            } else if (app.got_subcommand("diff")) {
                std::cout << "diff command called!" << std::endl;
            } else if (app.got_subcommand("restore")) {
                ngitRepo.restore(restorePaths, staged);
            } else if (app.got_subcommand("rm")) {
                ngitRepo.rm(rmPaths, cached);
            } else { // command does not exist
                if (!app.remaining().empty())
                    fmt::print(fg(fmt::color::red), "ngit: '{}' is not a ngit command. Run 'ngit --help'.\n", app.remaining().at(0));
                else
                    fmt::print(fg(fmt::color::red), "ngit: no command provided. Run 'ngit --help'.\n");
                return 1;
            }
        } catch (std::exception& e) {
            fmt::print(fmt::emphasis::bold | fg(fmt::color::red), "{}\n", e.what());
            return 1;
        }
    }
}

