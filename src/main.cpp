#include <iostream>
#include <string>

#include "workspace.hpp"  // сейчас workspace.hpp лежит в src/pear/fs и добавлен include-dir в CMake

int main(int argc, char** argv) {
    using pear::storage::Workspace;

    if (argc < 2) {
        std::cout
            << "Usage:\n"
            << "  pear init\n"
            << "  pear status\n";
        return 1;
    }

    const std::string cmd = argv[1];

    try {
        if (cmd == "init") {
            // init в текущей директории
            auto ws = Workspace::init();
            std::cout << "Initialized workspace at: " << ws.get_root().string() << "\n";
            return 0;
        }

        if (cmd == "status") {
            // ищем .peer вверх от текущей директории
            auto ws = Workspace::discover();
            std::cout << "Workspace found:\n";
            std::cout << "  root: " << ws.get_root().string() << "\n";
            std::cout << "  peer: " << ws.get_peer_dir().string() << "\n";
            std::cout << "  obj : " << ws.get_obj_dir().string() << "\n";
            std::cout << "  meta: " << ws.get_meta_dir().string() << "\n";
            return 0;
        }

        std::cerr << "Unknown command: " << cmd << "\n";
        return 1;
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << "\n";
        return 1;
    }
}