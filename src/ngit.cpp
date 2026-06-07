#include <iostream>

#include <CLI11.hpp>

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

    auto s = app.get_subcommand("init");
    std::cout << (s->parsed()) << std::endl;
}

