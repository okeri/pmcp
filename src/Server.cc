#include <stdexcept>

#include <sys/socket.h>
#include <sys/un.h>
#include <netinet/in.h>
#include <unistd.h>

#include "Server.hh"

Server::Server(const char* socketPath) : sockPath_(socketPath) {
    auto addr = sockaddr_un();
    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path, socketPath, sizeof(addr.sun_path));
    socket_ = ::socket(AF_UNIX, SOCK_STREAM, 0);
    unlink(socketPath);
    if (bind(socket_, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) < 0) {
        throw std::runtime_error("cannot bind socket");
    }
    if (listen(socket_, 1) < 0) {
        throw std::runtime_error("cannot listen socket");
    }
}

int Server::socket() const noexcept {
    return socket_;
}

std::optional<Action> Server::read(int client) const noexcept {
    auto result = Action{};
    auto ret = recv(client, &result, sizeof(result), 0);
    if (ret > 0) {
        return result;
    }
    close(client);
    return std::nullopt;
}

int Server::accept() const noexcept {
    auto client = sockaddr_in{};
    auto size = static_cast<socklen_t>(sizeof(client));
    return accept4(
        socket_, reinterpret_cast<sockaddr*>(&client), &size, SOCK_CLOEXEC);
}

Server::~Server() {
    unlink(sockPath_);
    close(socket_);
}
