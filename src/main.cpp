#include <iostream>
#include <unordered_map>
#include <cstring>
#include <string>
#include <vector>
#include <sstream>
#include <fstream>
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
#include "../include/resp.h"

// --------------------------------------
// Connection state for each client
// --------------------------------------

struct Connection {
    int fd;
    std::string input_buffer;

    // Transaction state
    bool in_transaction = false;

    std::vector<std::vector<std::string>> queued_commands;

    // Responses produced while EXEC is running
    std::vector<std::string> transaction_responses;

    // True while EXEC is executing queued commands
    bool executing_transaction = false;

    // Number of queued commands that EXEC must execute
    size_t expected_transaction_responses = 0;
};


// --------------------------------------
// Send response normally or collect it
// during EXEC
// --------------------------------------

void send_or_queue_response(
    Connection* connection,
    int client_fd,
    const std::string& response
) {

    if (connection->executing_transaction) {

        connection->transaction_responses.push_back(
            response
        );


        // EXEC has finished all queued commands
        if (
            connection->transaction_responses.size() ==
            connection->expected_transaction_responses
        ) {

            std::string transaction_response =
                respRawArray(
                    connection->transaction_responses
                );

            sendRESP(
                client_fd,
                transaction_response
            );


            connection->transaction_responses.clear();

            connection->expected_transaction_responses = 0;

            connection->executing_transaction = false;
        }

        return;
    }


    sendRESP(
        client_fd,
        response
    );
}


// --------------------------------------
// Encode command as RESP
// --------------------------------------

std::string encode_command(
    const std::vector<std::string>& parts
) {

    std::string result =
        "*" + std::to_string(parts.size()) + "\r\n";

    for (const auto& part : parts) {

        result +=
            "$" +
            std::to_string(part.size()) +
            "\r\n" +
            part +
            "\r\n";
    }

    return result;
}


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



// --------------------------------------
// Save key-value database to disk
// --------------------------------------

bool save_database(
    const HashTable& store,
    const ExpiryHeap& expiry_heap
) {

    std::ofstream file(
        "../data/dump.rdb",
        std::ios::binary
    );

    if (!file) {
        return false;
    }


    // RDB format identifier
    const uint64_t magic =
        0x5245444953444231ULL; // "REDISDB1"

    const uint64_t version = 1;


    file.write(
        reinterpret_cast<const char*>(&magic),
        sizeof(magic)
    );

    file.write(
        reinterpret_cast<const char*>(&version),
        sizeof(version)
    );


    auto entries =
        store.get_all();


    // Count only entries that should actually
    // be persisted.
    uint64_t count = 0;

    for (const auto& entry : entries) {

        long long expire_at =
            expiry_heap.get_expire_at(
                entry.first
            );

        // Key has an expiration.
        if (expire_at != -1) {

            long long ttl =
                expiry_heap.get_ttl(
                    entry.first
                );

            // Already expired.
            if (ttl <= 0) {
                continue;
            }
        }

        count++;
    }


    file.write(
        reinterpret_cast<const char*>(&count),
        sizeof(count)
    );


    for (const auto& entry : entries) {

        const std::string& key =
            entry.first;

        const std::string& value =
            entry.second;


        long long expire_at =
            expiry_heap.get_expire_at(key);

        long long ttl = -1;


        if (expire_at != -1) {

            ttl =
                expiry_heap.get_ttl(key);

            // Don't persist an already-expired key.
            if (ttl <= 0) {
                continue;
            }
        }


        uint64_t key_size =
            key.size();

        uint64_t value_size =
            value.size();


        file.write(
            reinterpret_cast<const char*>(&key_size),
            sizeof(key_size)
        );

        file.write(
            key.data(),
            key.size()
        );


        file.write(
            reinterpret_cast<const char*>(&value_size),
            sizeof(value_size)
        );

        file.write(
            value.data(),
            value.size()
        );


        // Remaining TTL.
        file.write(
            reinterpret_cast<const char*>(&ttl),
            sizeof(ttl)
        );
    }


    return file.good();
}


// --------------------------------------
// Load key-value database from disk
// --------------------------------------

bool load_database(
    HashTable& store,
    ExpiryHeap& expiry_heap
) {

    std::ifstream file(
        "../data/dump.rdb",
        std::ios::binary
    );

    if (!file) {
        return false;
    }


    const uint64_t expected_magic =
        0x5245444953444231ULL;

    const uint64_t expected_version = 1;


    uint64_t magic;

    file.read(
        reinterpret_cast<char*>(&magic),
        sizeof(magic)
    );

    if (!file || magic != expected_magic) {
        return false;
    }


    uint64_t version;

    file.read(
        reinterpret_cast<char*>(&version),
        sizeof(version)
    );

    if (!file || version != expected_version) {
        return false;
    }


    uint64_t count;

    file.read(
        reinterpret_cast<char*>(&count),
        sizeof(count)
    );

    if (!file) {
        return false;
    }


    for (
        uint64_t i = 0;
        i < count;
        i++
    ) {

        uint64_t key_size;

        file.read(
            reinterpret_cast<char*>(&key_size),
            sizeof(key_size)
        );

        if (!file) {
            return false;
        }


        std::string key(
            key_size,
            '\0'
        );

        file.read(
            key.data(),
            key_size
        );

        if (!file) {
            return false;
        }


        uint64_t value_size;

        file.read(
            reinterpret_cast<char*>(&value_size),
            sizeof(value_size)
        );

        if (!file) {
            return false;
        }


        std::string value(
            value_size,
            '\0'
        );

        file.read(
            value.data(),
            value_size
        );

        if (!file) {
            return false;
        }


        long long ttl;

        file.read(
            reinterpret_cast<char*>(&ttl),
            sizeof(ttl)
        );

        if (!file) {
            return false;
        }


        // Restore key/value.
        store.set(
            key,
            value
        );


        // Restore expiration.
        if (ttl > 0) {

            expiry_heap.add(
                key,
                ttl
            );
        }
    }


    return true;
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

    // Load saved database from disk
    load_database(
        store,
        expiry_heap
    );
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

                    RESPResult parsed =
                        parseRESP(
                            connection->input_buffer
                        );


                    // ----------------------------------
                    // Invalid RESP request
                    // ----------------------------------

                    if (parsed.error) {

                        const char* response =
                            "-ERR protocol error\r\n";

                        send_or_queue_response(
                            connection,
                            client_fd,
                            response
                        );

                        connection->input_buffer.clear();

                        break;
                    }


                    // ----------------------------------
                    // Incomplete request
                    // ----------------------------------

                    if (!parsed.complete) {
                        break;
                    }


                    // ----------------------------------
                    // Remove consumed RESP bytes
                    // ----------------------------------

                    connection->input_buffer.erase(
                        0,
                        parsed.consumed
                    );


                    // ----------------------------------
                    // Parsed command
                    // ----------------------------------

                    std::vector<std::string> parts =
                        parsed.parts;


                    if (parts.empty()) {
                        continue;
                    }


                    // Redis commands are case-insensitive
                    std::transform(
                        parts[0].begin(),
                        parts[0].end(),
                        parts[0].begin(),
                        [](unsigned char c) {
                            return std::toupper(c);
                        }
                    );


                    std::cout
                        << "Command: "
                        << parts[0]
                        << "\n";


                    // ==================================
                    // MULTI
                    // ==================================

                    if (parts[0] == "MULTI") {

                        if (parts.size() != 1) {

                            std::string response =
                                respError("ERR wrong number of arguments");

                            send_or_queue_response(
                                connection,
                                client_fd,
                                response
                            );

                            continue;
                        }


                        if (connection->in_transaction) {

                            std::string response =
                                respError("ERR MULTI calls can not be nested");

                            send_or_queue_response(
                                connection,
                                client_fd,
                                response
                            );

                            continue;
                        }


                        connection->in_transaction = true;

                        connection->queued_commands.clear();


                        std::string response =
                            respSimpleString("OK");

                        send_or_queue_response(
                            connection,
                            client_fd,
                            response
                        );

                        continue;
                    }


                    // ==================================
                    // EXEC
                    // ==================================

                    if (parts[0] == "EXEC") {

                        if (parts.size() != 1) {

                            std::string response =
                                respError("ERR wrong number of arguments");

                            send_or_queue_response(
                                connection,
                                client_fd,
                                response
                            );

                            continue;
                        }


                        if (!connection->in_transaction) {

                            std::string response =
                                respError("ERR EXEC without MULTI");

                            send_or_queue_response(
                                connection,
                                client_fd,
                                response
                            );

                            continue;
                        }


                        // Copy queued commands
                        std::vector<std::vector<std::string>> commands =
                            connection->queued_commands;


                        // Leave transaction mode
                        connection->in_transaction = false;

                        connection->queued_commands.clear();


                        // Prepare EXEC state
                        connection->transaction_responses.clear();

                        connection->executing_transaction = true;

                        connection->expected_transaction_responses =
                            commands.size();


                        // Empty transaction
                        if (commands.empty()) {

                            connection->executing_transaction = false;

                            connection->expected_transaction_responses = 0;

                            std::string response =
                                respRawArray({});

                            sendRESP(
                                client_fd,
                                response
                            );

                            continue;
                        }


                        // Reinsert queued commands into
                        // the existing RESP processing pipeline.
                        std::string encoded_commands;

                        for (const auto& command : commands) {

                            encoded_commands +=
                                encode_command(command);
                        }


                        connection->input_buffer =
                            encoded_commands +
                            connection->input_buffer;


                        // Continue into the existing
                        // command-processing loop.
                        continue;
                    }


                    // ==================================
                    // DISCARD
                    // ==================================

                    if (parts[0] == "DISCARD") {

                        if (parts.size() != 1) {

                            std::string response =
                                respError("ERR wrong number of arguments");

                            send_or_queue_response(
                                connection,
                                client_fd,
                                response
                            );

                            continue;
                        }


                        if (!connection->in_transaction) {

                            std::string response =
                                respError("ERR DISCARD without MULTI");

                            send_or_queue_response(
                                connection,
                                client_fd,
                                response
                            );

                            continue;
                        }


                        connection->queued_commands.clear();

                        connection->in_transaction = false;


                        std::string response =
                            respSimpleString("OK");

                        send_or_queue_response(
                            connection,
                            client_fd,
                            response
                        );

                        continue;
                    }


                    // ==================================
                    // TRANSACTION QUEUE
                    // ==================================

                    if (
                        connection->in_transaction &&
                        parts[0] != "EXEC" &&
                        parts[0] != "DISCARD"
                    ) {

                        connection->queued_commands.push_back(
                            parts
                        );

                        std::string response =
                            respSimpleString("QUEUED");

                        send_or_queue_response(
                            connection,
                            client_fd,
                            response
                        );

                        continue;
                    }


                    // ==================================
                    // PING
                    // ==================================

                    if (parts[0] == "PING") {

                        // PING takes no arguments

                        if (parts.size() != 1) {

                            std::string response =
                                respError("ERR wrong number of arguments");

                            send_or_queue_response(
                                connection,
                                client_fd,
                                response
                            );

                            continue;
                        }


                        std::string response =
                            respSimpleString("PONG");

                        send_or_queue_response(
                            connection,
                            client_fd,
                            response
                        );
                    }


                    // ==================================
                    // ECHO
                    // ==================================

                    else if (parts[0] == "ECHO") {

                        // ECHO message

                        if (parts.size() != 2) {

                            std::string response =
                                respError("ERR wrong number of arguments");

                            send_or_queue_response(
                                connection,
                                client_fd,
                                response
                            );

                            continue;
                        }


                        std::string response =
                            respBulkString(parts[1]);

                        send_or_queue_response(
                            connection,
                            client_fd,
                            response
                        );
                    }


                    // ==================================
                    // SET
                    // ==================================

                    else if (parts[0] == "SET") {

                        // SET key value
                        // SET key value PX milliseconds

                        if (parts.size() != 3 &&
                            parts.size() != 5) {

                            std::string response =
                                respError("ERR wrong number of arguments");

                            send_or_queue_response(
                                connection,
                                client_fd,
                                response
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

                                std::string response =
                                    respError("ERR syntax error");

                                sendRESP(
                                    client_fd,
                                    response
                                );

                                continue;
                            }


                            long long milliseconds;

                            try {

                                milliseconds =
                                    std::stoll(parts[4]);

                            } catch (...) {

                                std::string response =
                                    respError("ERR invalid milliseconds");

                                sendRESP(
                                    client_fd,
                                    response
                                );

                                continue;
                            }


                            if (milliseconds <= 0) {

                                std::string response =
                                    respError("ERR invalid milliseconds");

                                sendRESP(
                                    client_fd,
                                    response
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


                        std::string response =
                            respSimpleString("OK");


                        send_or_queue_response(
                            connection,
                            client_fd,
                            response
                        );
                    }


                    // ==================================
                    // GET
                    // ==================================

                    else if (
                        parts[0] == "GET"
                    ) {

                        if (parts.size() != 2) {

                            std::string response =
                                respError("ERR wrong number of arguments");

                            send_or_queue_response(
                                connection,
                                client_fd,
                                response
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
                                respBulkString(value);


                            send_or_queue_response(
                                connection,
                                client_fd,
                                response
                            );

                        } else {

                            std::string response =
                                respNull();


                            send_or_queue_response(
                                connection,
                                client_fd,
                                response
                            );
                        }
                    }


                    // ==================================
                    // SAVE
                    // ==================================

                    else if (
                        parts[0] == "SAVE"
                    ) {

                        // SAVE takes no arguments

                        if (parts.size() != 1) {

                            std::string response =
                                respError(
                                    "ERR wrong number of arguments"
                                );

                            send_or_queue_response(
                                connection,
                                client_fd,
                                response
                            );

                            continue;
                        }


                        bool saved =
                            save_database(
                                store,
                                expiry_heap
                            );


                        if (!saved) {

                            std::string response =
                                respError(
                                    "ERR failed to save database"
                                );

                            send_or_queue_response(
                                connection,
                                client_fd,
                                response
                            );

                            continue;
                        }


                        std::string response =
                            respSimpleString("OK");


                        send_or_queue_response(
                            connection,
                            client_fd,
                            response
                        );
                    }


                    // DEL
                    // ==================================

                    else if (
                        parts[0] == "DEL"
                    ) {

                        if (parts.size() != 2) {

                            std::string response =
                                respError("ERR wrong number of arguments");

                            send_or_queue_response(
                                connection,
                                client_fd,
                                response
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

                            std::string response =
                                respInteger(1);


                            send_or_queue_response(
                                connection,
                                client_fd,
                                response
                            );

                        } else {

                            std::string response =
                                respInteger(0);


                            send_or_queue_response(
                                connection,
                                client_fd,
                                response
                            );
                        }
                    }

                    // ==================================
                    // LLEN
                    // ==================================

                    else if (parts[0] == "LLEN") {

                        // LLEN key

                        if (parts.size() != 2) {

                            std::string response =
                                respError("ERR wrong number of arguments");

                            send_or_queue_response(
                                connection,
                                client_fd,
                                response
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
                            respInteger(count);

                        send_or_queue_response(
                            connection,
                            client_fd,
                            response
                        );
                    }


                    // ==================================
                    // LRANGE
                    // ==================================

                    else if (parts[0] == "LRANGE") {

                        // LRANGE key start stop

                        if (parts.size() != 4) {

                            std::string response =
                                respError("ERR wrong number of arguments");

                            send_or_queue_response(
                                connection,
                                client_fd,
                                response
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

                            std::string response =
                                respError("ERR invalid range");

                            send_or_queue_response(
                                connection,
                                client_fd,
                                response
                            );

                            continue;
                        }


                        auto it =
                            lists.find(key);


                        // List doesn't exist
                        if (it == lists.end()) {

                            std::string response =
                                respArray({});

                            send_or_queue_response(
                                connection,
                                client_fd,
                                response
                            );

                            continue;
                        }


                        auto elements =
                            it->second.get_range(
                                start,
                                stop
                            );


                        std::string response =
                            respArray(elements);

                        send_or_queue_response(
                            connection,
                            client_fd,
                            response
                        );
                    }


                    // RPOP
                    // ==================================

                    else if (parts[0] == "RPOP") {

                        // RPOP key

                        if (parts.size() != 2) {

                            std::string response =
                                respError("ERR wrong number of arguments");

                            send_or_queue_response(
                                connection,
                                client_fd,
                                response
                            );

                            continue;
                        }


                        std::string key =
                            parts[1];


                        auto it =
                            lists.find(key);


                        // List doesn't exist
                        if (it == lists.end()) {

                            std::string response =
                                respNull();

                            send_or_queue_response(
                                connection,
                                client_fd,
                                response
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

                            std::string response =
                                respNull();

                            send_or_queue_response(
                                connection,
                                client_fd,
                                response
                            );

                            continue;
                        }


                        std::string response =
                            respBulkString(value);

                        send_or_queue_response(
                            connection,
                            client_fd,
                            response
                        );
                    }


                    // ==================================
                    // LPOP
                    // ==================================

                    else if (parts[0] == "LPOP") {

                        // LPOP key

                        if (parts.size() != 2) {

                            std::string response =
                                respError("ERR wrong number of arguments");

                            send_or_queue_response(
                                connection,
                                client_fd,
                                response
                            );

                            continue;
                        }


                        std::string key =
                            parts[1];


                        auto it =
                            lists.find(key);


                        // List doesn't exist
                        if (it == lists.end()) {

                            std::string response =
                                respNull();

                            send_or_queue_response(
                                connection,
                                client_fd,
                                response
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

                            std::string response =
                                respNull();

                            send_or_queue_response(
                                connection,
                                client_fd,
                                response
                            );

                            continue;
                        }


                        std::string response =
                            respBulkString(value);

                        send_or_queue_response(
                            connection,
                            client_fd,
                            response
                        );
                    }


                    // RPUSH
                    // ==================================

                    else if (parts[0] == "RPUSH") {

                        // RPUSH key value

                        if (parts.size() != 3) {

                            std::string response =
                                respError("ERR wrong number of arguments");

                            send_or_queue_response(
                                connection,
                                client_fd,
                                response
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
                            respInteger(
                                lists[key].size()
                            );

                        send_or_queue_response(
                            connection,
                            client_fd,
                            response
                        );
                    }


                    // ==================================
                    // LPUSH
                    // ==================================

                    else if (parts[0] == "LPUSH") {

                        // LPUSH key value

                        if (parts.size() != 3) {

                            std::string response =
                                respError("ERR wrong number of arguments");

                            send_or_queue_response(
                                connection,
                                client_fd,
                                response
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
                            respInteger(
                                lists[key].size()
                            );

                        send_or_queue_response(
                            connection,
                            client_fd,
                            response
                        );
                    }


                    // ==================================
                    // ZADD
                    // ==================================

                    else if (parts[0] == "ZADD") {

                        // ZADD key score member

                        if (parts.size() != 4) {

                            std::string response =
                                respError("ERR wrong number of arguments");

                            send_or_queue_response(
                                connection,
                                client_fd,
                                response
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

                            send_or_queue_response(
                                connection,
                                client_fd,
                                response
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


                        std::string response;

                        if (added) {

                            response =
                                respInteger(1);

                        } else {

                            response =
                                respInteger(0);
                        }


                        send_or_queue_response(
                            connection,
                            client_fd,
                            response
                        );
                    }


                    // ==================================
                    // ZPOPMIN
                    // ==================================

                    else if (parts[0] == "ZPOPMIN") {

                        // ZPOPMIN key

                        if (parts.size() != 2) {

                            std::string response =
                                respError("ERR wrong number of arguments");

                            send_or_queue_response(
                                connection,
                                client_fd,
                                response
                            );

                            continue;
                        }

                        std::string key =
                            parts[1];

                        auto it =
                            zsets.find(key);

                        if (it == zsets.end()) {

                            std::string response =
                                respArray({});

                            send_or_queue_response(
                                connection,
                                client_fd,
                                response
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

                            std::string response =
                                respArray({});

                            send_or_queue_response(
                                connection,
                                client_fd,
                                response
                            );

                            continue;
                        }

                        std::vector<std::string> values;

                        values.push_back(
                            member
                        );

                        values.push_back(
                            format_score(score)
                        );

                        std::string response =
                            respArray(values);

                        send_or_queue_response(
                            connection,
                            client_fd,
                            response
                        );
                    }


                    // ==================================
                    // ZPOPMAX
                    // ==================================

                    else if (parts[0] == "ZPOPMAX") {

                        // ZPOPMAX key

                        if (parts.size() != 2) {

                            std::string response =
                                respError("ERR wrong number of arguments");

                            send_or_queue_response(
                                connection,
                                client_fd,
                                response
                            );

                            continue;
                        }

                        std::string key =
                            parts[1];

                        auto it =
                            zsets.find(key);

                        if (it == zsets.end()) {

                            std::string response =
                                respArray({});

                            send_or_queue_response(
                                connection,
                                client_fd,
                                response
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

                            std::string response =
                                respArray({});

                            send_or_queue_response(
                                connection,
                                client_fd,
                                response
                            );

                            continue;
                        }

                        std::vector<std::string> values;

                        values.push_back(
                            member
                        );

                        values.push_back(
                            format_score(score)
                        );

                        std::string response =
                            respArray(values);

                        send_or_queue_response(
                            connection,
                            client_fd,
                            response
                        );
                    }


                    // ==================================
                    // ZINCRBY
                    // ==================================

                    else if (parts[0] == "ZINCRBY") {

                        // ZINCRBY key increment member

                        if (parts.size() != 4) {

                            std::string response =
                                respError("ERR wrong number of arguments");

                            send_or_queue_response(
                                connection,
                                client_fd,
                                response
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

                            send_or_queue_response(
                                connection,
                                client_fd,
                                response
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
                            respBulkString(
                                format_score(new_score)
                            );

                        send_or_queue_response(
                            connection,
                            client_fd,
                            response
                        );
                    }


                    // ==================================
                    // ZSCORE
                    // ==================================

                    else if (parts[0] == "ZSCORE") {

                        // ZSCORE key member

                        if (parts.size() != 3) {

                            std::string response =
                                respError("ERR wrong number of arguments");

                            send_or_queue_response(
                                connection,
                                client_fd,
                                response
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

                            std::string response =
                                respNull();

                            send_or_queue_response(
                                connection,
                                client_fd,
                                response
                            );

                            continue;
                        }


                        std::string response =
                            respBulkString(
                                format_score(score)
                            );

                        send_or_queue_response(
                            connection,
                            client_fd,
                            response
                        );
                    }


                    // ==================================
                    // ZCARD
                    // ==================================

                    else if (parts[0] == "ZCARD") {

                        // ZCARD key

                        if (parts.size() != 2) {

                            std::string response =
                                respError("ERR wrong number of arguments");

                            send_or_queue_response(
                                connection,
                                client_fd,
                                response
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
                            respInteger(count);

                        send_or_queue_response(
                            connection,
                            client_fd,
                            response
                        );
                    }


                    // ==================================
                    // ZREM
                    // ==================================

                    else if (parts[0] == "ZREM") {

                        // ZREM key member

                        if (parts.size() != 3) {

                            std::string response =
                                respError("ERR wrong number of arguments");

                            send_or_queue_response(
                                connection,
                                client_fd,
                                response
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


                        std::string response;

                        if (removed) {

                            response =
                                respInteger(1);

                        } else {

                            response =
                                respInteger(0);
                        }


                        send_or_queue_response(
                            connection,
                            client_fd,
                            response
                        );
                    }


                    // ==================================
                    // ZREM
                    // ==================================

                    // ==================================
                    // ==================================
                    // ZRANGE
                    // ==================================

                    else if (parts[0] == "ZRANGE") {

                        // ZRANGE key start stop
                        // ZRANGE key start stop WITHSCORES

                        if (
                            parts.size() != 4 &&
                            parts.size() != 5
                        ) {

                            std::string response =
                                respError("ERR wrong number of arguments");

                            send_or_queue_response(
                                connection,
                                client_fd,
                                response
                            );

                            continue;
                        }


                        std::string key =
                            parts[1];


                        // Check WITHSCORES
                        if (
                            parts.size() == 5 &&
                            parts[4] != "WITHSCORES"
                        ) {

                            std::string response =
                                respError("ERR syntax error");

                            send_or_queue_response(
                                connection,
                                client_fd,
                                response
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

                            std::string response =
                                respError("ERR invalid range");

                            send_or_queue_response(
                                connection,
                                client_fd,
                                response
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

                            std::string response =
                                respArray({});

                            send_or_queue_response(
                                connection,
                                client_fd,
                                response
                            );

                            continue;
                        }


                        std::vector<std::string> values;


                        for (
                            int i = start_index;
                            i <= stop_index;
                            i++
                        ) {

                            // Member
                            values.push_back(
                                elements[i].second
                            );


                            // Score
                            if (parts.size() == 5) {

                                values.push_back(
                                    format_score(
                                        elements[i].first
                                    )
                                );
                            }
                        }


                        std::string response =
                            respArray(values);

                        send_or_queue_response(
                            connection,
                            client_fd,
                            response
                        );
                    }


                    else if (parts[0] == "PEXPIRE") {

                        if (parts.size() != 3) {

                            std::string response =
                                respError("ERR wrong number of arguments");

                            send_or_queue_response(
                                connection,
                                client_fd,
                                response
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

                            send_or_queue_response(
                                connection,
                                client_fd,
                                response
                            );

                            continue;
                        }


                        // Check whether key exists
                        std::string value;

                        if (!store.get(key, value)) {

                            std::string response =
                                respInteger(0);

                            send_or_queue_response(
                                connection,
                                client_fd,
                                response
                            );

                            continue;
                        }


                        // Add expiration
                        expiry_heap.add(
                            key,
                            milliseconds
                        );


                        std::string response =
                            respInteger(1);

                        send_or_queue_response(
                            connection,
                            client_fd,
                            response
                        );
                    }


                    else if (parts[0] == "PTTL") {

                        if (parts.size() != 2) {

                            std::string response =
                                respError("ERR wrong number of arguments");

                            send_or_queue_response(
                                connection,
                                client_fd,
                                response
                            );

                            continue;
                        }

                        std::string key = parts[1];


                        // First check if key exists
                        std::string value;

                        if (!store.get(key, value)) {

                            std::string response =
                                respInteger(-2);

                            send_or_queue_response(
                                connection,
                                client_fd,
                                response
                            );

                            continue;
                        }


                        long long ttl =
                            expiry_heap.get_ttl(key);


                        if (ttl == -2) {

                            // Key exists but has no expiration
                            std::string response =
                                respInteger(-1);

                            send_or_queue_response(
                                connection,
                                client_fd,
                                response
                            );

                        } else {

                            std::string response =
                                respInteger(ttl);

                            send_or_queue_response(
                                connection,
                                client_fd,
                                response
                            );
                        }
                    }


                    // ==================================
                    // UNKNOWN COMMAND
                    // ==================================

                    else {

                        const char* response =
                            "-ERR unknown command\r\n";


                        send_or_queue_response(
                            connection,
                            client_fd,
                            response
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
