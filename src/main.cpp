#include "NotGitRepository/NotGitRepository.hpp"
#include <iostream>
#include <CLI11.hpp>
#include <string>
#include <filesystem>

int main(int argc, char** argv) {
    CLI::App app {};
    argv = app.ensure_utf8(argv);

    std::string filename = "default";
    app.add_subcommand("init", "Create an empty NotGit repository or reinitialise an exisiting one");
    app.add_subcommand("add", "Add file contents to index");
    app.add_subcommand("commit", "Record changes to repository");
    app.add_subcommand("status", "Show working tree status");
    app.add_subcommand("checkout", "Switch branched or restore working tree files");
    app.add_subcommand("merge", "Switch branched or restore working tree files");
    app.add_subcommand("rm", "Switch branched or restore working tree files");
    app.add_subcommand("log", "Switch branched or restore working tree files");
    app.add_subcommand("diff", "Switch branched or restore working tree files");
    
    CLI11_PARSE(app, argc, argv); //  Parse cli commands here!

    if (app.got_subcommand("init")) {
        std::cout << "init command called!" << std::endl;
        NotGitRepository::init(std::filesystem::current_path());
    } else if (app.got_subcommand("add")) {
        std::cout << "add command called!" << std::endl;
    } else if (app.got_subcommand("commit")) {
        std::cout << "commit command called!" << std::endl;
    } else if (app.got_subcommand("status")) {
        std::cout << "status command called!" << std::endl;
    } else if (app.got_subcommand("checkout")) {
        std::cout << "checkout command called!" << std::endl;
    } else if (app.got_subcommand("merge")) {
        std::cout << "merge command called!" << std::endl;
    } else if (app.got_subcommand("rm")) {
        std::cout << "rm command called!" << std::endl;
    } else if (app.got_subcommand("log")) {
        std::cout << "log command called!" << std::endl;
    } else if (app.got_subcommand("diff")) {
        std::cout << "diff command called!" << std::endl;
    } else { // command does not exist
        return 1;
    }
}

