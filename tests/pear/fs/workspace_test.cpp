#include <gtest/gtest.h>

#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

#include "workspace.hpp"

namespace fs = std::filesystem;

// Утилита: создаём уникальную временную директорию и удаляем в деструкторе
struct TempDir {
    fs::path dir;

    TempDir() {
        auto base = fs::temp_directory_path();
        dir = base / fs::path("pear_ws_test_" + std::to_string(::getpid()) + "_" + std::to_string(std::rand()));
        fs::create_directories(dir);
    }

    ~TempDir() {
        std::error_code ec;
        fs::remove_all(dir, ec);
    }
};

TEST(Workspace, InitCreatesPeerStructure) {
    TempDir td;

    auto ws = p2p::filesystem::Workspace::init(td.dir);

    EXPECT_TRUE(fs::exists(ws.get_peer_dir()));
    EXPECT_TRUE(fs::is_directory(ws.get_peer_dir()));

    EXPECT_TRUE(fs::exists(ws.get_obj_dir()));
    EXPECT_TRUE(fs::is_directory(ws.get_obj_dir()));

    EXPECT_TRUE(fs::exists(ws.get_meta_dir()));
    EXPECT_TRUE(fs::is_directory(ws.get_meta_dir()));
}

TEST(Workspace, InitFailsInsideExistingWorkspace) {
    TempDir td;

    (void)p2p::filesystem::Workspace::init(td.dir);

    // пытаемся init в подпапке — должно упасть (запрещаем nested)
    fs::path sub = td.dir / "sub";
    fs::create_directories(sub);

    EXPECT_THROW(
        (void)p2p::filesystem::Workspace::init(sub),
        std::runtime_error
    );
}

TEST(Workspace, DiscoverFindsWorkspaceFromNestedDir) {
    TempDir td;

    (void)p2p::filesystem::Workspace::init(td.dir);

    fs::path nested = td.dir / "a" / "b" / "c";
    fs::create_directories(nested);

    auto ws = p2p::filesystem::Workspace::discover(nested);

    EXPECT_EQ(ws.get_root(), td.dir);
    EXPECT_EQ(ws.get_peer_dir(), td.dir / ".peer");
    EXPECT_EQ(ws.get_obj_dir(), td.dir / ".peer" / "obj");
    EXPECT_EQ(ws.get_meta_dir(), td.dir / ".peer" / "meta");
}

TEST(Workspace, DiscoverFailsWhenNoWorkspace) {
    TempDir td;

    // workspace не создавали
    EXPECT_THROW(
        (void)p2p::filesystem::Workspace::discover(td.dir),
        std::runtime_error
    );
}

TEST(Workspace, CreateEmptyFilesCreatesReadOnlyEmptyPlaceholders) {
    TempDir td;

    auto ws = p2p::filesystem::Workspace::init(td.dir);

    std::vector<std::string> names = {"alpha.txt", "beta.bin", "gamma"};
    ws.create_all_empty_files(names);

    for (const auto& n : names) {
        fs::path p = td.dir / (n + ".empty");
        ASSERT_TRUE(fs::exists(p));
        ASSERT_TRUE(fs::is_regular_file(p));
        ASSERT_EQ(fs::file_size(p), 0u);

        // Проверяем, что нет owner_write (в WSL/Linux должно проходить)
        auto perm = fs::status(p).permissions();
        EXPECT_FALSE((perm & fs::perms::owner_write) != fs::perms::none);
    }
}

TEST(Workspace, CreateObjectFileCopiesToObjDir) {
    TempDir td;

    auto ws = p2p::filesystem::Workspace::init(td.dir);

    // создаём локальный файл в root
    fs::path local = td.dir / "file1.txt";
    {
        std::ofstream out(local);
        out << "hello\n";
    }

    fs::path obj = ws.create_objectfile(local);

    EXPECT_TRUE(fs::exists(obj));
    EXPECT_TRUE(fs::is_regular_file(obj));
    EXPECT_EQ(obj.parent_path(), ws.get_obj_dir());
    EXPECT_EQ(obj.filename().string(), "file1.txt");

    // содержимое должно совпасть
    std::string content_local, content_obj;
    {
        std::ifstream in1(local);
        std::getline(in1, content_local, '\0');
    }
    {
        std::ifstream in2(obj);
        std::getline(in2, content_obj, '\0');
    }
    EXPECT_EQ(content_obj, content_local);
}

TEST(Workspace, CreateObjectFileThrowsOnDuplicateId) {
    TempDir td;

    auto ws = p2p::filesystem::Workspace::init(td.dir);

    fs::path local = td.dir / "dup.txt";
    {
        std::ofstream out(local);
        out << "one";
    }

    (void)ws.create_objectfile(local);

    // Второй раз попытаемся создать тот же object_id (filename) -> copy_file должен упасть
    // Обычно бросается std::filesystem::filesystem_error
    EXPECT_THROW(
        (void)ws.create_objectfile(local),
        fs::filesystem_error
    );
}

TEST(Workspace, DeleteObjectFileRemovesObject) {
    TempDir td;

    auto ws = p2p::filesystem::Workspace::init(td.dir);

    fs::path local = td.dir / "to_delete.txt";
    {
        std::ofstream out(local);
        out << "data";
    }

    fs::path obj = ws.create_objectfile(local);
    ASSERT_TRUE(fs::exists(obj));

    ws.delete_objectfile("to_delete.txt");
    EXPECT_FALSE(fs::exists(obj));
}