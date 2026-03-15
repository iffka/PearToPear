#include <pear/demon/demon.hpp>

#include <chrono>
#include <csignal>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <optional>
#include <stdexcept>
#include <string>
#include <thread>

#include <sys/types.h>
#include <unistd.h>

namespace {

std::filesystem::path get_pid_file(const std::filesystem::path& workspace_root) {
    return workspace_root / ".peer" / "meta" / "demon.pid";
}

void send_status(int fd, const std::string& msg) {
    std::string line = msg + '\n';
    write(fd, line.c_str(), line.size());
}

std::optional<pid_t> find_pid(const std::filesystem::path& workspace_root) {
    const std::filesystem::path pid_file = get_pid_file(workspace_root);

    if (!std::filesystem::exists(pid_file)) {
        return std::nullopt;
    }

    std::ifstream in(pid_file);
    if (!in) {
        return std::nullopt;
    }

    pid_t pid;
    in >> pid;

    if (!in || pid <= 0) {
        return std::nullopt;
    }

    if (::kill(pid, 0) == 0) {
        return pid;
    }

    std::filesystem::remove(pid_file);
    return std::nullopt;
}

void write_pid_file(const std::filesystem::path& workspace_root, pid_t pid) {
    const std::filesystem::path pid_file = get_pid_file(workspace_root);

    std::ofstream out(pid_file, std::ios::trunc);
    if (!out) {
        throw std::runtime_error("failed to open demon.pid");
    }

    out << pid;
    if (!out) {
        throw std::runtime_error("failed to write demon.pid");
    }
}

void remove_pid_file(const std::filesystem::path& workspace_root) {
    std::filesystem::remove(get_pid_file(workspace_root));
}

void run(const std::filesystem::path& workspace_root,
         const std::string& repo_id,
         bool is_main,
         int write_fd) {
    send_status(write_fd, "child: spawned");
    send_status(write_fd, "child: workspace_root = " + workspace_root.string());
    send_status(write_fd, "child: repo_id = " + repo_id);
    send_status(write_fd, std::string("child: is_main = ") + (is_main ? "true" : "false"));

    if (is_main) {
        send_status(write_fd, "child: would start main server");
    }

    send_status(write_fd, "child: would start file server");
    send_status(write_fd, "child: servers are considered ready");

    if (setsid() < 0) {
        send_status(write_fd, "fail: setsid failed");
        close(write_fd);
        _exit(1);
    }

    send_status(write_fd, "child: setsid ok");

    try {
        write_pid_file(workspace_root, getpid());
    } catch (const std::exception& e) {
        send_status(write_fd, std::string("fail: ") + e.what());
        close(write_fd);
        _exit(1);
    }

    send_status(write_fd, "child: pid file written");
    send_status(write_fd, "ok");

    close(write_fd);

    // #todo тут потом будет реальная жизнь демона с поднятыми серверами
    while (true) {
        sleep(1);
    }
}

} // namespace

namespace pear::demon {

void spawn(const std::filesystem::path& workspace_root,
           const std::string& repo_id,
           bool is_main) {
    if (is_alive(workspace_root)) {
        throw std::runtime_error("Already connected");
    }

    int pipefd[2];
    if (pipe(pipefd) < 0) {
        throw std::runtime_error("pipe failed");
    }

    pid_t pid = fork();
    if (pid < 0) {
        close(pipefd[0]);
        close(pipefd[1]);
        throw std::runtime_error("fork failed");
    }

    if (pid == 0) {
        close(pipefd[0]);
        run(workspace_root, repo_id, is_main, pipefd[1]);
        _exit(0);
    }

    close(pipefd[1]);

    std::string buffer;

    while (true) {
        char temp[256];
        ssize_t n = read(pipefd[0], temp, sizeof(temp));

        if (n < 0) {
            close(pipefd[0]);
            throw std::runtime_error("read failed");
        }

        if (n == 0) {
            break;
        }

        buffer.append(temp, n);

        std::size_t pos = 0;
        while (true) {
            std::size_t newline = buffer.find('\n', pos);
            if (newline == std::string::npos) {
                buffer.erase(0, pos);
                break;
            }

            std::string line = buffer.substr(pos, newline - pos);
            pos = newline + 1;

            std::cout << line << '\n';

            if (line == "ok") {
                close(pipefd[0]);
                return;
            }

            if (line.rfind("fail:", 0) == 0) {
                close(pipefd[0]);
                throw std::runtime_error(line.substr(6));
            }
        }
    }

    close(pipefd[0]);
    throw std::runtime_error("demon start failed");
}

void kill(const std::filesystem::path& workspace_root) {
    std::optional<pid_t> pid = find_pid(workspace_root);

    if (!pid.has_value()) {
        throw std::runtime_error("Not connected");
    }

    if (::kill(*pid, SIGTERM) < 0) {
        throw std::runtime_error("failed to send SIGTERM");
    }

    for (int i = 0; i < 20; ++i) {
        if (::kill(*pid, 0) != 0) {
            remove_pid_file(workspace_root);
            return;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    throw std::runtime_error("failed to stop demon");
}

bool is_alive(const std::filesystem::path& workspace_root) {
    return find_pid(workspace_root).has_value();
}

} // namespace pear::demon