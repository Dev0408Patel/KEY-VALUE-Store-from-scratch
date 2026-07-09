#include <iostream>
#include <string>
#include <vector>
#include <thread>
#include <sstream>
#include <cstring>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include "fastkv/kv_store.h"
#include "fastkv/mutex_kv_store.h"
#include "fastkv/lock_free_kv_store.h"

/**
 * @brief Thread routine to process client queries concurrently.
 */
void handle_client(int client_fd, fastkv::IKVStore* store) {
    char buffer[1024];
    while (true) {
        std::memset(buffer, 0, sizeof(buffer));
        ssize_t bytes_read = read(client_fd, buffer, sizeof(buffer) - 1);
        if (bytes_read <= 0) {
            break; // Socket closed or error
        }

        std::string command(buffer);
        // Strip out carriage return/newlines
        while (!command.empty() && (command.back() == '\n' || command.back() == '\r')) {
            command.pop_back();
        }

        if (command.empty()) {
            continue;
        }

        std::stringstream ss(command);
        std::string op;
        ss >> op;

        std::string response;
        if (op == "SET") {
            std::string key, val;
            ss >> key;
            std::string temp;
            if (ss >> temp) {
                val = temp;
                while (ss >> temp) {
                    val += " " + temp;
                }
            }
            if (key.empty()) {
                response = "ERR Missing key\n";
            } else {
                store->Set(key, val);
                response = "OK\n";
            }
        } else if (op == "GET") {
            std::string key, val;
            ss >> key;
            if (key.empty()) {
                response = "ERR Missing key\n";
            } else if (store->Get(key, val)) {
                response = "VALUE " + val + "\n";
            } else {
                response = "ERR Key not found\n";
            }
        } else if (op == "DEL") {
            std::string key;
            ss >> key;
            if (key.empty()) {
                response = "ERR Missing key\n";
            } else if (store->Delete(key)) {
                response = "OK\n";
            } else {
                response = "ERR Key not found\n";
            }
        } else if (op == "SIZE") {
            response = "SIZE " + std::to_string(store->Size()) + "\n";
        } else if (op == "CLEAR") {
            store->Clear();
            response = "OK\n";
        } else if (op == "EXIT" || op == "QUIT") {
            response = "BYE\n";
            ssize_t written = write(client_fd, response.c_str(), response.size());
            (void)written;
            break;
        } else {
            response = "ERR Unknown command\n";
        }

        ssize_t written = write(client_fd, response.c_str(), response.size());
        (void)written;
    }
    close(client_fd);
}

int main(int argc, char* argv[]) {
    int port = 6380;
    std::string backend_type = "lockfree";

    // CLI argument parsing
    for (int i = 1; i < argc; ++i) {
        if (std::string(argv[i]) == "--port" && i + 1 < argc) {
            port = std::stoi(argv[++i]);
        } else if (std::string(argv[i]) == "--backend" && i + 1 < argc) {
            backend_type = argv[++i];
        }
    }

    fastkv::IKVStore* store = nullptr;
    if (backend_type == "mutex") {
        std::cout << "Starting FastKV Server (Mutex-Striped Backend) on port " << port << "...\n";
        store = new fastkv::MutexKVStore();
    } else {
        std::cout << "Starting FastKV Server (Lock-Free Backend) on port " << port << "...\n";
        store = new fastkv::LockFreeKVStore();
    }

    int server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd == -1) {
        std::cerr << "Failed to create socket\n";
        delete store;
        return 1;
    }

    int opt = 1;
    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    sockaddr_in address{};
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(static_cast<uint16_t>(port));

    if (bind(server_fd, (struct sockaddr*)&address, sizeof(address)) < 0) {
        std::cerr << "Bind failed\n";
        close(server_fd);
        delete store;
        return 1;
    }

    if (listen(server_fd, 128) < 0) {
        std::cerr << "Listen failed\n";
        close(server_fd);
        delete store;
        return 1;
    }

    std::cout << "FastKV Key-Value Server is ready for network connections.\n";

    while (true) {
        sockaddr_in client_address{};
        socklen_t client_len = sizeof(client_address);
        int client_fd = accept(server_fd, (struct sockaddr*)&client_address, &client_len);
        if (client_fd < 0) {
            std::cerr << "Failed to accept client connection\n";
            continue;
        }

        // Spawn client routine concurrently
        std::thread(handle_client, client_fd, store).detach();
    }

    close(server_fd);
    delete store;
    return 0;
}
