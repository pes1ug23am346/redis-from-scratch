#include <iostream>
#include <unordered_map>
#include <cstring>
#include <string>
#include <vector>
#include <sstream>
#include <cerrno>
#include <fcntl.h>

#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <poll.h>

#include "../include/hash_table.h"
#include "../include/expiry_heap.h"
#include "../include/zset.h"
#include "../include/list.h"


// --------------------------------------
// Connection state for each client
// --------------------------------------

struct Connection {
    int fd;
    std::string input_buffer;
};


// --------------------------------------
// Make socket non-blocking
// --------------------------------------

bool set_nonblocking(int fd) {

    int flags = fcntl(fd, F_GETFL, 0);

    if (flags == -1) {
        return false;
    }

    if (fcntl(fd, F_SETFL, flags | O_NONBLOCK) == -1) {
        return false;
    }

    return true;
}


// --------------------------------------
// Parse command
// --------------------------------------

std::string format_score(double score) {

    if (score == static_cast<long long>(score)) {

        return std::to_string(
            static_cast<long long>(score)
        );
    }

    std::string result =
        std::to_string(score);

    // Remove trailing zeros
    while (
        !result.empty() &&
        result.back() == '0'
    ) {
        result.pop_back();
    }

    // Remove trailing decimal point
    if (
        !result.empty() &&
        result.back() == '.'
    ) {
        result.pop_back();
    }

    return result;
}


std::vector<std::string> parse_command(
    const std::string& request
) {
    std::vector<std::string> parts;

    std::stringstream ss(request);

    std::string word;

    while (ss >> word) {
        parts.push_back(word);
    }

    return parts;
}


int main() {

    // ----------------------------------
    // 1. Create TCP socket
    // ----------------------------------

    int server_fd = socket(
        AF_INET,
        SOCK_STREAM,
        0
    );

    if (server_fd == -1) {

        std::cerr
            << "Failed to create socket\n";

        return 1;
    }

    std::cout
        << "Socket created successfully!\n";


    // ----------------------------------
    // 2. Make server socket non-blocking
    // ----------------------------------

    if (!set_nonblocking(server_fd)) {

        std::cerr
            << "Failed to make server socket non-blocking\n";

        close(server_fd);

        return 1;
    }


    // ----------------------------------
    // 3. Create server address
    // ----------------------------------

    sockaddr_in server_address{};

    server_address.sin_family =
        AF_INET;

    server_address.sin_addr.s_addr =
        INADDR_ANY;

    server_address.sin_port =
        htons(6379);


    // ----------------------------------
    // 4. Bind
    // ----------------------------------

    if (bind(
            server_fd,
            (struct sockaddr*)&server_address,
            sizeof(server_address)
        ) == -1) {

        std::cerr
            << "Bind failed\n";

        close(server_fd);

        return 1;
    }

    std::cout
        << "Server bound to port 6379!\n";


    // ----------------------------------
    // 5. Listen
    // ----------------------------------

    if (listen(server_fd, 10) == -1) {

        std::cerr
            << "Listen failed\n";

        close(server_fd);

        return 1;
    }

    std::cout
        << "Server is listening on port 6379...\n";


    // ----------------------------------
    // 6. Poll list
    // ----------------------------------

    std::vector<pollfd> fds;

    pollfd server_pollfd{};

    server_pollfd.fd =
        server_fd;

    server_pollfd.events =
        POLLIN;

    server_pollfd.revents =
        0;

    fds.push_back(
        server_pollfd
    );


    // ----------------------------------
    // 7. Client connections
    // ----------------------------------

    std::vector<Connection> connections;


    // ----------------------------------
    // 8. Our custom key-value store
    // ----------------------------------

    HashTable store;
    ExpiryHeap expiry_heap;
    std::unordered_map<std::string, ZSet> zsets;
    std::unordered_map<std::string, RedisList> lists;


    // ----------------------------------
    // 9. Main event loop
    // ----------------------------------

    while (true) {
        // ----------------------------------
        // Remove expired keys
        // ----------------------------------

        std::vector<ExpiryHeap::ExpiryEntry>
            expired_entries =
                expiry_heap.get_expired_entries();

        for (const auto& entry : expired_entries) {

            store.remove(entry.key);

            std::cout
                << "Expired key: "
                << entry.key
                << "\n";
        }
        int ready = poll(
            fds.data(),
            fds.size(),
            100
        );


        if (ready == -1) {

            if (errno == EINTR) {
                continue;
            }

            std::cerr
                << "Poll failed\n";

            break;
        }


        // ----------------------------------
        // 10. Check every file descriptor
        // ----------------------------------

        for (size_t i = 0;
             i < fds.size();
             i++) {

            if (!(fds[i].revents & POLLIN)) {
                continue;
            }


            // ==================================
            // SERVER SOCKET
            // New client connection
            // ==================================

            if (fds[i].fd == server_fd) {

                int client_fd =
                    accept(
                        server_fd,
                        nullptr,
                        nullptr
                    );


                if (client_fd == -1) {

                    if (errno == EAGAIN ||
                        errno == EWOULDBLOCK) {

                        continue;
                    }

                    std::cerr
                        << "Accept failed\n";

                    continue;
                }


                // Make client non-blocking
                if (!set_nonblocking(client_fd)) {

                    std::cerr
                        << "Failed to make client "
                           "socket non-blocking\n";

                    close(client_fd);

                    continue;
                }


                std::cout
                    << "New client connected!\n";


                // Add client to poll list
                pollfd client_pollfd{};

                client_pollfd.fd =
                    client_fd;

                client_pollfd.events =
                    POLLIN;

                client_pollfd.revents =
                    0;

                fds.push_back(
                    client_pollfd
                );


                // Create connection state
                Connection connection;

                connection.fd =
                    client_fd;

                connection.input_buffer =
                    "";

                connections.push_back(
                    connection
                );
            }


            // ==================================
            // CLIENT SOCKET
            // ==================================

            else {

                int client_fd =
                    fds[i].fd;


                // ----------------------------------
                // Find Connection object
                // ----------------------------------

                Connection* connection =
                    nullptr;

                for (auto& conn :
                     connections) {

                    if (conn.fd ==
                        client_fd) {

                        connection =
                            &conn;

                        break;
                    }
                }


                if (connection ==
                    nullptr) {

                    continue;
                }


                // ----------------------------------
                // Receive data
                // ----------------------------------

                char buffer[1024];


                int bytes_received =
                    recv(
                        client_fd,
                        buffer,
                        sizeof(buffer),
                        0
                    );


                // ----------------------------------
                // Client disconnected
                // ----------------------------------

                if (bytes_received == 0) {

                    std::cout
                        << "Client disconnected\n";

                    close(client_fd);


                    // Remove from poll list
                    fds.erase(
                        fds.begin() + i
                    );


                    // Remove connection
                    for (size_t j = 0;
                         j < connections.size();
                         j++) {

                        if (
                            connections[j].fd ==
                            client_fd
                        ) {

                            connections.erase(
                                connections.begin() + j
                            );

                            break;
                        }
                    }


                    i--;

                    continue;
                }


                // ----------------------------------
                // Receive error
                // ----------------------------------

                if (bytes_received == -1) {

                    if (errno == EAGAIN ||
                        errno == EWOULDBLOCK) {

                        continue;
                    }

                    std::cerr
                        << "Receive failed\n";

                    close(client_fd);


                    fds.erase(
                        fds.begin() + i
                    );


                    for (size_t j = 0;
                         j < connections.size();
                         j++) {

                        if (
                            connections[j].fd ==
                            client_fd
                        ) {

                            connections.erase(
                                connections.begin() + j
                            );

                            break;
                        }
                    }


                    i--;

                    continue;
                }


                // ----------------------------------
                // Append received data
                // ----------------------------------

                connection->input_buffer.append(
                    buffer,
                    bytes_received
                );


                // ----------------------------------
                // Process complete requests
                // ----------------------------------

                while (true) {

                    size_t newline_pos =
                        connection->input_buffer.find(
                            '\n'
                        );


                    // No complete request
                    if (
                        newline_pos ==
                        std::string::npos
                    ) {

                        break;
                    }


                    // ----------------------------------
                    // Extract request
                    // ----------------------------------

                    std::string request =
                        connection->input_buffer.substr(
                            0,
                            newline_pos
                        );


                    // Remove request + newline
                    connection->input_buffer.erase(
                        0,
                        newline_pos + 1
                    );


                    std::cout
                        << "Complete request: "
                        << request
                        << "\n";


                    // ----------------------------------
                    // Parse command
                    // ----------------------------------

                    std::vector<std::string> parts =
                        parse_command(
                            request
                        );


                    // Empty command
                    if (parts.empty()) {
                        continue;
                    }


                    std::cout
                        << "Command: "
                        << parts[0]
                        << "\n";


                    // ==================================
                    // SET
                    // ==================================

                    if (parts[0] == "SET") {

                        // SET key value
                        // SET key value PX milliseconds

                        if (parts.size() != 3 &&
                            parts.size() != 5) {

                            const char* response =
                                "-ERR wrong number of arguments\n";

                            send(
                                client_fd,
                                response,
                                strlen(response),
                                0
                            );

                            continue;
                        }


                        std::string key =
                            parts[1];

                        std::string value =
                            parts[2];


                        // Store value
                        store.set(
                            key,
                            value
                        );


                        // Optional PX expiration
                        if (parts.size() == 5) {

                            if (parts[3] != "PX") {

                                const char* response =
                                    "-ERR syntax error\n";

                                send(
                                    client_fd,
                                    response,
                                    strlen(response),
                                    0
                                );

                                continue;
                            }


                            long long milliseconds;

                            try {

                                milliseconds =
                                    std::stoll(parts[4]);

                            } catch (...) {

                                const char* response =
                                    "-ERR invalid milliseconds\n";

                                send(
                                    client_fd,
                                    response,
                                    strlen(response),
                                    0
                                );

                                continue;
                            }


                            if (milliseconds <= 0) {

                                const char* response =
                                    "-ERR invalid milliseconds\n";

                                send(
                                    client_fd,
                                    response,
                                    strlen(response),
                                    0
                                );

                                continue;
                            }


                            expiry_heap.add(
                                key,
                                milliseconds
                            );
                        }


                        std::cout
                            << "SET "
                            << key
                            << " = "
                            << value
                            << "\n";


                        const char* response =
                            "+OK\n";


                        send(
                            client_fd,
                            response,
                            strlen(response),
                            0
                        );
                    }


                    // ==================================
                    // GET
                    // ==================================

                    else if (
                        parts[0] == "GET"
                    ) {

                        if (parts.size() != 2) {

                            const char* response =
                                "-ERR wrong number of arguments\n";

                            send(
                                client_fd,
                                response,
                                strlen(response),
                                0
                            );

                            continue;
                        }


                        std::string key =
                            parts[1];

                        std::string value;


                        // Search hash table
                        if (
                            store.get(
                                key,
                                value
                            )
                        ) {

                            std::string response =
                                value + "\n";


                            send(
                                client_fd,
                                response.c_str(),
                                response.size(),
                                0
                            );

                        } else {

                            const char* response =
                                "(nil)\n";


                            send(
                                client_fd,
                                response,
                                strlen(response),
                                0
                            );
                        }
                    }


                    // ==================================
                    // DEL
                    // ==================================

                    else if (
                        parts[0] == "DEL"
                    ) {

                        if (parts.size() != 2) {

                            const char* response =
                                "-ERR wrong number of arguments\n";

                            send(
                                client_fd,
                                response,
                                strlen(response),
                                0
                            );

                            continue;
                        }


                        std::string key =
                            parts[1];


                        bool deleted =
                            store.remove(
                                key
                            );


                        if (deleted) {

                            const char* response =
                                ":1\n";


                            send(
                                client_fd,
                                response,
                                strlen(response),
                                0
                            );

                        } else {

                            const char* response =
                                ":0\n";


                            send(
                                client_fd,
                                response,
                                strlen(response),
                                0
                            );
                        }
                    }

                    // ==================================
                    // LLEN
                    // ==================================

                    else if (parts[0] == "LLEN") {

                        // LLEN key

                        if (parts.size() != 2) {

                            const char* response =
                                "-ERR wrong number of arguments\n";

                            send(
                                client_fd,
                                response,
                                strlen(response),
                                0
                            );

                            continue;
                        }


                        std::string key =
                            parts[1];


                        auto it =
                            lists.find(key);


                        int count = 0;


                        if (it != lists.end()) {

                            count =
                                it->second.size();
                        }


                        std::string response =
                            std::to_string(count);

                        response += "\n";


                        send(
                            client_fd,
                            response.c_str(),
                            response.size(),
                            0
                        );
                    }


                    // ==================================
                    // LRANGE
                    // ==================================

                    else if (parts[0] == "LRANGE") {

                        // LRANGE key start stop

                        if (parts.size() != 4) {

                            const char* response =
                                "-ERR wrong number of arguments\n";

                            send(
                                client_fd,
                                response,
                                strlen(response),
                                0
                            );

                            continue;
                        }


                        std::string key =
                            parts[1];

                        int start;
                        int stop;


                        try {

                            start =
                                std::stoi(parts[2]);

                            stop =
                                std::stoi(parts[3]);

                        }
                        catch (...) {

                            const char* response =
                                "-ERR invalid range\n";

                            send(
                                client_fd,
                                response,
                                strlen(response),
                                0
                            );

                            continue;
                        }


                        auto it =
                            lists.find(key);


                        // List doesn't exist
                        if (it == lists.end()) {

                            const char* response =
                                "(empty)\n";

                            send(
                                client_fd,
                                response,
                                strlen(response),
                                0
                            );

                            continue;
                        }


                        auto elements =
                            it->second.get_range(
                                start,
                                stop
                            );


                        if (elements.empty()) {

                            const char* response =
                                "(empty)\n";

                            send(
                                client_fd,
                                response,
                                strlen(response),
                                0
                            );

                            continue;
                        }


                        std::string response;


                        for (
                            const auto& value :
                            elements
                        ) {

                            response += value;
                            response += "\n";
                        }


                        send(
                            client_fd,
                            response.c_str(),
                            response.size(),
                            0
                        );
                    }


                    // ==================================
                    // RPOP
                    // ==================================

                    else if (parts[0] == "RPOP") {

                        // RPOP key

                        if (parts.size() != 2) {

                            const char* response =
                                "-ERR wrong number of arguments\n";

                            send(
                                client_fd,
                                response,
                                strlen(response),
                                0
                            );

                            continue;
                        }


                        std::string key =
                            parts[1];


                        auto it =
                            lists.find(key);


                        // List doesn't exist
                        if (it == lists.end()) {

                            const char* response =
                                "(nil)\n";

                            send(
                                client_fd,
                                response,
                                strlen(response),
                                0
                            );

                            continue;
                        }


                        std::string value;


                        // Remove from back
                        if (
                            !it->second.pop_back(
                                value
                            )
                        ) {

                            const char* response =
                                "(nil)\n";

                            send(
                                client_fd,
                                response,
                                strlen(response),
                                0
                            );

                            continue;
                        }


                        value += "\n";


                        send(
                            client_fd,
                            value.c_str(),
                            value.size(),
                            0
                        );
                    }


                    // ==================================
                    // LPOP
                    // ==================================

                    else if (parts[0] == "LPOP") {

                        // LPOP key

                        if (parts.size() != 2) {

                            const char* response =
                                "-ERR wrong number of arguments\n";

                            send(
                                client_fd,
                                response,
                                strlen(response),
                                0
                            );

                            continue;
                        }


                        std::string key =
                            parts[1];


                        auto it =
                            lists.find(key);


                        // List doesn't exist
                        if (it == lists.end()) {

                            const char* response =
                                "(nil)\n";

                            send(
                                client_fd,
                                response,
                                strlen(response),
                                0
                            );

                            continue;
                        }


                        std::string value;


                        // Remove from front
                        if (
                            !it->second.pop_front(
                                value
                            )
                        ) {

                            const char* response =
                                "(nil)\n";

                            send(
                                client_fd,
                                response,
                                strlen(response),
                                0
                            );

                            continue;
                        }


                        value += "\n";


                        send(
                            client_fd,
                            value.c_str(),
                            value.size(),
                            0
                        );
                    }


                    // ==================================
                    // RPUSH
                    // ==================================

                    else if (parts[0] == "RPUSH") {

                        // RPUSH key value

                        if (parts.size() != 3) {

                            const char* response =
                                "-ERR wrong number of arguments\n";

                            send(
                                client_fd,
                                response,
                                strlen(response),
                                0
                            );

                            continue;
                        }


                        std::string key =
                            parts[1];

                        std::string value =
                            parts[2];


                        // Add to back
                        lists[key].push_back(
                            value
                        );


                        // Return new list length
                        std::string response =
                            std::to_string(
                                lists[key].size()
                            );

                        response += "\n";


                        send(
                            client_fd,
                            response.c_str(),
                            response.size(),
                            0
                        );
                    }


                    // ==================================
                    // LPUSH
                    // ==================================

                    else if (parts[0] == "LPUSH") {

                        // LPUSH key value

                        if (parts.size() != 3) {

                            const char* response =
                                "-ERR wrong number of arguments\n";

                            send(
                                client_fd,
                                response,
                                strlen(response),
                                0
                            );

                            continue;
                        }


                        std::string key =
                            parts[1];

                        std::string value =
                            parts[2];


                        // Add to front
                        lists[key].push_front(
                            value
                        );


                        // Return new list length
                        std::string response =
                            std::to_string(
                                lists[key].size()
                            );

                        response += "\n";


                        send(
                            client_fd,
                            response.c_str(),
                            response.size(),
                            0
                        );
                    }


                    // ==================================
                    // ZADD
                    // ==================================

                    else if (parts[0] == "ZADD") {

                        // ZADD key score member

                        if (parts.size() != 4) {

                            const char* response =
                                "-ERR wrong number of arguments\n";

                            send(
                                client_fd,
                                response,
                                strlen(response),
                                0
                            );

                            continue;
                        }


                        std::string key =
                            parts[1];

                        double score;


                        try {

                            score =
                                std::stod(parts[2]);

                        }
                        catch (...) {

                            const char* response =
                                "-ERR invalid score\n";

                            send(
                                client_fd,
                                response,
                                strlen(response),
                                0
                            );

                            continue;
                        }


                        std::string member =
                            parts[3];


                        bool added =
                            zsets[key].add(
                                score,
                                member
                            );


                        const char* response;

                        if (added) {

                            response = ":1\n";

                        } else {

                            response = ":0\n";
                        }


                        send(
                            client_fd,
                            response,
                            strlen(response),
                            0
                        );
                    }


                    // ==================================
                    // ZPOPMIN
                    // ==================================

                    else if (parts[0] == "ZPOPMIN") {

                        // ZPOPMIN key

                        if (parts.size() != 2) {

                            const char* response =
                                "-ERR wrong number of arguments\n";

                            send(
                                client_fd,
                                response,
                                strlen(response),
                                0
                            );

                            continue;
                        }

                        std::string key =
                            parts[1];

                        auto it =
                            zsets.find(key);

                        if (it == zsets.end()) {

                            const char* response =
                                "(empty)\n";

                            send(
                                client_fd,
                                response,
                                strlen(response),
                                0
                            );

                            continue;
                        }

                        double score;
                        std::string member;

                        if (
                            !it->second.pop_min(
                                score,
                                member
                            )
                        ) {

                            const char* response =
                                "(empty)\n";

                            send(
                                client_fd,
                                response,
                                strlen(response),
                                0
                            );

                            continue;
                        }

                        std::string response =
                            member;

                        response += "\n";
                        response += format_score(score);
                        response += "\n";

                        send(
                            client_fd,
                            response.c_str(),
                            response.size(),
                            0
                        );
                    }


                    // ==================================
                    // ZPOPMAX
                    // ==================================

                    else if (parts[0] == "ZPOPMAX") {

                        // ZPOPMAX key

                        if (parts.size() != 2) {

                            const char* response =
                                "-ERR wrong number of arguments\n";

                            send(
                                client_fd,
                                response,
                                strlen(response),
                                0
                            );

                            continue;
                        }

                        std::string key =
                            parts[1];

                        auto it =
                            zsets.find(key);

                        if (it == zsets.end()) {

                            const char* response =
                                "(empty)\n";

                            send(
                                client_fd,
                                response,
                                strlen(response),
                                0
                            );

                            continue;
                        }

                        double score;
                        std::string member;

                        if (
                            !it->second.pop_max(
                                score,
                                member
                            )
                        ) {

                            const char* response =
                                "(empty)\n";

                            send(
                                client_fd,
                                response,
                                strlen(response),
                                0
                            );

                            continue;
                        }

                        std::string response =
                            member;

                        response += "\n";
                        response += format_score(score);
                        response += "\n";

                        send(
                            client_fd,
                            response.c_str(),
                            response.size(),
                            0
                        );
                    }


                    // ==================================
                    // ZINCRBY
                    // ==================================

                    else if (parts[0] == "ZINCRBY") {

                        // ZINCRBY key increment member

                        if (parts.size() != 4) {

                            const char* response =
                                "-ERR wrong number of arguments\n";

                            send(
                                client_fd,
                                response,
                                strlen(response),
                                0
                            );

                            continue;
                        }


                        std::string key =
                            parts[1];

                        double amount;


                        try {

                            amount =
                                std::stod(parts[2]);

                        }
                        catch (...) {

                            const char* response =
                                "-ERR invalid increment\n";

                            send(
                                client_fd,
                                response,
                                strlen(response),
                                0
                            );

                            continue;
                        }


                        std::string member =
                            parts[3];


                        double new_score =
                            zsets[key].increment(
                                amount,
                                member
                            );


                        std::string response =
                            format_score(new_score);

                        response += "\n";


                        send(
                            client_fd,
                            response.c_str(),
                            response.size(),
                            0
                        );
                    }


                    // ==================================
                    // ZSCORE
                    // ==================================

                    else if (parts[0] == "ZSCORE") {

                        // ZSCORE key member

                        if (parts.size() != 3) {

                            const char* response =
                                "-ERR wrong number of arguments\n";

                            send(
                                client_fd,
                                response,
                                strlen(response),
                                0
                            );

                            continue;
                        }


                        std::string key =
                            parts[1];

                        std::string member =
                            parts[2];

                        double score;


                        // Check whether the ZSET exists
                        auto it =
                            zsets.find(key);


                        if (
                            it == zsets.end() ||
                            !it->second.get_score(
                                member,
                                score
                            )
                        ) {

                            const char* response =
                                "(nil)\n";

                            send(
                                client_fd,
                                response,
                                strlen(response),
                                0
                            );

                            continue;
                        }


                        std::string response =
                            format_score(score);

                        response += "\n";


                        send(
                            client_fd,
                            response.c_str(),
                            response.size(),
                            0
                        );
                    }


                    // ==================================
                    // ZCARD
                    // ==================================

                    else if (parts[0] == "ZCARD") {

                        // ZCARD key

                        if (parts.size() != 2) {

                            const char* response =
                                "-ERR wrong number of arguments\n";

                            send(
                                client_fd,
                                response,
                                strlen(response),
                                0
                            );

                            continue;
                        }


                        std::string key =
                            parts[1];


                        auto it =
                            zsets.find(key);


                        int count = 0;


                        if (it != zsets.end()) {

                            count =
                                it->second.size();
                        }


                        std::string response =
                            std::to_string(count);

                        response += "\n";


                        send(
                            client_fd,
                            response.c_str(),
                            response.size(),
                            0
                        );
                    }


                    // ==================================
                    // ZREM
                    // ==================================

                    else if (parts[0] == "ZREM") {

                        // ZREM key member

                        if (parts.size() != 3) {

                            const char* response =
                                "-ERR wrong number of arguments\n";

                            send(
                                client_fd,
                                response,
                                strlen(response),
                                0
                            );

                            continue;
                        }


                        std::string key =
                            parts[1];

                        std::string member =
                            parts[2];


                        bool removed =
                            zsets[key].remove(
                                member
                            );


                        const char* response;

                        if (removed) {

                            response = ":1\n";

                        } else {

                            response = ":0\n";
                        }


                        send(
                            client_fd,
                            response,
                            strlen(response),
                            0
                        );
                    }


                    // ==================================
                    // ZREM
                    // ==================================

                    // ==================================
                    // ZRANGE
                    // ==================================

                    else if (parts[0] == "ZRANGE") {

                        std::string key =
                            parts[1];


                        // ZRANGE key start stop
                        // ZRANGE key start stop WITHSCORES

                        if (
                            parts.size() != 4 &&
                            parts.size() != 5
                        ) {

                            const char* response =
                                "-ERR wrong number of arguments\n";

                            send(
                                client_fd,
                                response,
                                strlen(response),
                                0
                            );

                            continue;
                        }


                        // Check WITHSCORES
                        if (
                            parts.size() == 5 &&
                            parts[4] != "WITHSCORES"
                        ) {

                            const char* response =
                                "-ERR syntax error\n";

                            send(
                                client_fd,
                                response,
                                strlen(response),
                                0
                            );

                            continue;
                        }


                        int start_index;
                        int stop_index;


                        try {

                            start_index =
                                std::stoi(parts[2]);

                            stop_index =
                                std::stoi(parts[3]);

                        }
                        catch (...) {

                            const char* response =
                                "-ERR invalid range\n";

                            send(
                                client_fd,
                                response,
                                strlen(response),
                                0
                            );

                            continue;
                        }


                        // Get sorted elements
                        auto elements =
                            zsets[key].range();


                        int size =
                            static_cast<int>(
                                elements.size()
                            );


                        // Convert negative indexes
                        if (start_index < 0) {

                            start_index =
                                size + start_index;
                        }


                        if (stop_index < 0) {

                            stop_index =
                                size + stop_index;
                        }


                        // Clamp start
                        if (start_index < 0) {

                            start_index = 0;
                        }


                        // Clamp stop
                        if (stop_index >= size) {

                            stop_index =
                                size - 1;
                        }


                        // Empty range
                        if (
                            size == 0 ||
                            start_index > stop_index ||
                            start_index >= size
                        ) {

                            const char* response =
                                "(empty)\n";

                            send(
                                client_fd,
                                response,
                                strlen(response),
                                0
                            );

                            continue;
                        }


                        std::string response;


                        for (
                            int i = start_index;
                            i <= stop_index;
                            i++
                        ) {

                            // Member
                            response +=
                                elements[i].second;

                            response += "\n";


                            // Score
                            if (parts.size() == 5) {

                                response +=
                                    format_score(
                                        elements[i].first
                                    );

                                response += "\n";
                            }
                        }


                        send(
                            client_fd,
                            response.c_str(),
                            response.size(),
                            0
                        );
                    }


                    else if (parts[0] == "PEXPIRE") {

                        if (parts.size() != 3) {

                            const char* response =
                                "-ERR wrong number of arguments\n";

                            send(
                                client_fd,
                                response,
                                strlen(response),
                                0
                            );

                            continue;
                        }

                        std::string key = parts[1];

                        long long milliseconds;

                        try {
                            milliseconds =
                                std::stoll(parts[2]);
                        }
                        catch (...) {

                            const char* response =
                                "-ERR invalid milliseconds\n";

                            send(
                                client_fd,
                                response,
                                strlen(response),
                                0
                            );

                            continue;
                        }


                        // Check whether key exists
                        std::string value;

                        if (!store.get(key, value)) {

                            const char* response =
                                ":0\n";

                            send(
                                client_fd,
                                response,
                                strlen(response),
                                0
                            );

                            continue;
                        }


                        // Add expiration
                        expiry_heap.add(
                            key,
                            milliseconds
                        );


                        const char* response =
                            ":1\n";

                        send(
                            client_fd,
                            response,
                            strlen(response),
                            0
                        );
                    }


                    else if (parts[0] == "PTTL") {

                        if (parts.size() != 2) {

                            const char* response =
                                "-ERR wrong number of arguments\n";

                            send(
                                client_fd,
                                response,
                                strlen(response),
                                0
                            );

                            continue;
                        }

                        std::string key = parts[1];


                        // First check if key exists
                        std::string value;

                        if (!store.get(key, value)) {

                            const char* response =
                                "-2\n";

                            send(
                                client_fd,
                                response,
                                strlen(response),
                                0
                            );

                            continue;
                        }


                        long long ttl =
                            expiry_heap.get_ttl(key);


                        if (ttl == -2) {

                            // Key exists but has no expiration
                            const char* response =
                                "-1\n";

                            send(
                                client_fd,
                                response,
                                strlen(response),
                                0
                            );

                        } else {

                            std::string response =
                                std::to_string(ttl) + "\n";

                            send(
                                client_fd,
                                response.c_str(),
                                response.size(),
                                0
                            );
                        }
                    }


                    // ==================================
                    // UNKNOWN COMMAND
                    // ==================================

                    else {

                        const char* response =
                            "-ERR unknown command\n";


                        send(
                            client_fd,
                            response,
                            strlen(response),
                            0
                        );
                    }
                }
            }
        }
    }


    // ----------------------------------
    // Cleanup
    // ----------------------------------

    for (auto& pfd : fds) {
        close(pfd.fd);
    }


    return 0;
}