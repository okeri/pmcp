#pragma once

#include <optional>

#include "Action.hh"

class Server {
    int socket_;
    const char* sockPath_;

  public:
    explicit Server(const char* socketPath);
    Server(const Server&) = delete;
    Server(Server&&) = delete;
    Server& operator=(const Server&) = delete;
    Server& operator=(Server&&) = delete;
    [[nodiscard]] int socket() const noexcept;
    [[nodiscard]] std::optional<Action> read(int client) const noexcept;
    [[nodiscard]] int accept() const noexcept;
    ~Server();
};
