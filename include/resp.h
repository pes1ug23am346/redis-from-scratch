#ifndef RESP_H
#define RESP_H

#include <string>
#include <vector>
#include <sys/socket.h>

struct RESPResult {
    bool complete;
    bool error;
    size_t consumed;
    std::vector<std::string> parts;
};

inline RESPResult parseRESP(const std::string& buffer) {

    RESPResult result;
    result.complete = false;
    result.error = false;
    result.consumed = 0;

    if (buffer.empty()) {
        return result;
    }

    if (buffer[0] != '*') {
        result.error = true;
        return result;
    }

    size_t pos = buffer.find("\r\n");

    if (pos == std::string::npos) {
        return result;
    }

    int argument_count;

    try {
        argument_count =
            std::stoi(buffer.substr(1, pos - 1));
    }
    catch (...) {
        result.error = true;
        return result;
    }

    if (argument_count <= 0) {
        result.error = true;
        return result;
    }

    size_t current = pos + 2;

    for (int i = 0; i < argument_count; i++) {

        if (current >= buffer.size()) {
            return result;
        }

        if (buffer[current] != '$') {
            result.error = true;
            return result;
        }

        size_t length_end =
            buffer.find("\r\n", current);

        if (length_end == std::string::npos) {
            return result;
        }

        int length;

        try {
            length = std::stoi(
                buffer.substr(
                    current + 1,
                    length_end - current - 1
                )
            );
        }
        catch (...) {
            result.error = true;
            return result;
        }

        if (length < 0) {
            result.error = true;
            return result;
        }

        current = length_end + 2;

        if (buffer.size() < current + length + 2) {
            return result;
        }

        std::string argument =
            buffer.substr(current, length);

        result.parts.push_back(argument);

        current += length;

        if (buffer.substr(current, 2) != "\r\n") {
            result.error = true;
            return result;
        }

        current += 2;
    }

    result.complete = true;
    result.consumed = current;

    return result;
}

inline std::string respSimpleString(
    const std::string& value
) {
    return "+" + value + "\r\n";
}

inline std::string respError(
    const std::string& value
) {
    return "-" + value + "\r\n";
}

inline std::string respInteger(long long value) {
    return ":" + std::to_string(value) + "\r\n";
}

inline std::string respBulkString(
    const std::string& value
) {
    return "$" +
           std::to_string(value.size()) +
           "\r\n" +
           value +
           "\r\n";
}

inline std::string respNull() {
    return "$-1\r\n";
}


inline void sendRESP(
    int client_fd,
    const std::string& response
) {
    send(
        client_fd,
        response.c_str(),
        response.size(),
        0
    );
}


inline std::string respArray(
    const std::vector<std::string>& values
) {
    std::string result =
        "*" + std::to_string(values.size()) + "\r\n";

    for (const auto& value : values) {
        result += respBulkString(value);
    }

    return result;
}

#endif
