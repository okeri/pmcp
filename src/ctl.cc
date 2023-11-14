#include <unordered_map>
#include <algorithm>
#include <iostream>
#include <filesystem>
#include <string>

#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>

#include "Action.hh"

class Client {
    int socket_;

  public:
    explicit Client(const std::string& path) {
        auto addr = sockaddr_un{};
        addr.sun_family = AF_UNIX;
        strncpy(addr.sun_path, path.c_str(), sizeof(addr.sun_path));
        socket_ = socket(AF_UNIX, SOCK_STREAM, 0);
        if (socket_ < 0) {
            throw std::runtime_error("cannot create socket");
        }

        if (connect(socket_, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) <
            0) {
            close(socket_);
            throw std::runtime_error("cannot connect to socket");
        }
    }

    Client(const Client&) = delete;
    Client(Client&&) = delete;
    Client& operator=(const Client&) = delete;
    Client& operator=(Client&&) = delete;

    ~Client() {
        close(socket_);
    }

    void send(Action action) {
        ::send(socket_, &action, sizeof(action), 0);
    }
};

std::string sockPath() {
    const auto* runtimePath =
        getenv("XDG_RUNTIME_DIR");  // NOLINT:concurrency-mt-unsafe
    if (runtimePath != nullptr) {
        return (std::filesystem::path(runtimePath) / "pmcp.sock").string();
    } else {
        return "/tmp/pmcp.sock";
    }
}

int main(int argc, char* argv[]) {
    std::unordered_map<std::string, Action> actionMap = {{"quit", Action::Quit},
        {"stop", Action::Stop}, {"pause", Action::Pause},
        {"prev", Action::Prev}, {"next", Action::Next}, {"play", Action::Play}};

    auto usage = [&actionMap](const char* name) {
        std::cout << "usage:" << name << " <";
        std::string sep = "";
        for (const auto& [k, _] : actionMap) {
            std::cout << sep << k;
            sep = "|";
        }
        std::cout << '>' << std::endl;
    };

    if (argc < 2) {
        usage(argv[0]);
        return -1;
    }
    auto cmd = std::string(argv[1]);
    std::transform(cmd.begin(), cmd.end(), cmd.begin(),
        [](unsigned char c) { return std::tolower(c); });

    auto found = actionMap.find(cmd);
    if (found == actionMap.end()) {
        usage(argv[0]);
        return -2;
    }

    auto client = Client(sockPath());
    client.send(found->second);
    return 0;
}
