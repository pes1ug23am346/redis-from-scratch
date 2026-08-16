#include <cassert>
#include <cstring>
#include <iostream>
#include <string>

#include <arpa/inet.h>
#include <sys/socket.h>
#include <unistd.h>

std::string send_command(
    const std::string& request
) {
    int fd =
        socket(
            AF_INET,
            SOCK_STREAM,
            0
        );

    assert(fd >= 0);

    sockaddr_in server{};

    server.sin_family =
        AF_INET;

    server.sin_port =
        htons(6379);

    inet_pton(
        AF_INET,
        "127.0.0.1",
        &server.sin_addr
    );

    int connected =
        connect(
            fd,
            reinterpret_cast<sockaddr*>(&server),
            sizeof(server)
        );

    assert(connected == 0);

    ssize_t sent =
        send(
            fd,
            request.c_str(),
            request.size(),
            0
        );

    assert(sent ==
           static_cast<ssize_t>(request.size()));

    char buffer[4096];

    ssize_t received =
        recv(
            fd,
            buffer,
            sizeof(buffer) - 1,
            0
        );

    assert(received > 0);

    buffer[received] =
        '\0';

    close(fd);

    return std::string(buffer);
}


int main() {

    // ==============================
    // PING
    // ==============================

    std::string response =
        send_command(
            "*1\r\n"
            "$4\r\n"
            "PING\r\n"
        );

    assert(response == "+PONG\r\n");


    // ==============================
    // lowercase ping
    // ==============================

    response =
        send_command(
            "*1\r\n"
            "$4\r\n"
            "ping\r\n"
        );

    assert(response == "+PONG\r\n");


    // ==============================
    // ECHO
    // ==============================

    response =
        send_command(
            "*2\r\n"
            "$4\r\n"
            "ECHO\r\n"
            "$5\r\n"
            "hello\r\n"
        );

    assert(
        response ==
        "$5\r\n"
        "hello\r\n"
    );


    // ==============================
    // SET
    // ==============================

    response =
        send_command(
            "*3\r\n"
            "$3\r\n"
            "SET\r\n"
            "$4\r\n"
            "name\r\n"
            "$9\r\n"
            "Veerendra\r\n"
        );

    assert(response == "+OK\r\n");


    // ==============================
    // GET
    // ==============================

    response =
        send_command(
            "*2\r\n"
            "$3\r\n"
            "GET\r\n"
            "$4\r\n"
            "name\r\n"
        );

    assert(
        response ==
        "$9\r\n"
        "Veerendra\r\n"
    );


    // ==============================
    // LPUSH
    // ==============================

    response =
        send_command(
            "*3\r\n"
            "$5\r\n"
            "LPUSH\r\n"
            "$4\r\n"
            "nums\r\n"
            "$2\r\n"
            "10\r\n"
        );

    assert(response == ":1\r\n");


    // ==============================
    // LPUSH second value
    // ==============================

    response =
        send_command(
            "*3\r\n"
            "$5\r\n"
            "LPUSH\r\n"
            "$4\r\n"
            "nums\r\n"
            "$2\r\n"
            "20\r\n"
        );

    assert(response == ":2\r\n");


    // ==============================
    // LPOP
    // ==============================

    response =
        send_command(
            "*2\r\n"
            "$4\r\n"
            "LPOP\r\n"
            "$4\r\n"
            "nums\r\n"
        );

    assert(
        response ==
        "$2\r\n"
        "20\r\n"
    );


    // ==============================
    // RPUSH
    // ==============================

    response =
        send_command(
            "*3\r\n"
            "$5\r\n"
            "RPUSH\r\n"
            "$4\r\n"
            "nums\r\n"
            "$2\r\n"
            "30\r\n"
        );

    assert(response == ":2\r\n");


    // ==============================
    // RPOP
    // ==============================

    response =
        send_command(
            "*2\r\n"
            "$4\r\n"
            "RPOP\r\n"
            "$4\r\n"
            "nums\r\n"
        );

    assert(
        response ==
        "$2\r\n"
        "30\r\n"
    );


    // ==============================
    // LPOP remaining value
    // ==============================

    response =
        send_command(
            "*2\r\n"
            "$4\r\n"
            "LPOP\r\n"
            "$4\r\n"
            "nums\r\n"
        );

    assert(
        response ==
        "$2\r\n"
        "10\r\n"
    );


    // ==============================
    // ZADD
    // ==============================

    response =
        send_command(
            "*4\r\n"
            "$4\r\n"
            "ZADD\r\n"
            "$6\r\n"
            "scores\r\n"
            "$2\r\n"
            "50\r\n"
            "$4\r\n"
            "Amit\r\n"
        );

    assert(response == ":1\r\n");


    // ==============================
    // ZADD second member
    // ==============================

    response =
        send_command(
            "*4\r\n"
            "$4\r\n"
            "ZADD\r\n"
            "$6\r\n"
            "scores\r\n"
            "$3\r\n"
            "100\r\n"
            "$9\r\n"
            "Veerendra\r\n"
        );

    assert(response == ":1\r\n");


    // ==============================
    // ZSCORE
    // ==============================

    response =
        send_command(
            "*3\r\n"
            "$6\r\n"
            "ZSCORE\r\n"
            "$6\r\n"
            "scores\r\n"
            "$9\r\n"
            "Veerendra\r\n"
        );

    assert(
        response ==
        "$3\r\n"
        "100\r\n"
    );


    // ==============================
    // ZCARD
    // ==============================

    response =
        send_command(
            "*2\r\n"
            "$5\r\n"
            "ZCARD\r\n"
            "$6\r\n"
            "scores\r\n"
        );

    assert(response == ":2\r\n");


    // ==============================
    // ZINCRBY
    // ==============================

    response =
        send_command(
            "*4\r\n"
            "$7\r\n"
            "ZINCRBY\r\n"
            "$6\r\n"
            "scores\r\n"
            "$2\r\n"
            "25\r\n"
            "$4\r\n"
            "Amit\r\n"
        );

    assert(
        response ==
        "$2\r\n"
        "75\r\n"
    );


    // ==============================
    // ZRANGE
    // ==============================

    response =
        send_command(
            "*4\r\n"
            "$6\r\n"
            "ZRANGE\r\n"
            "$6\r\n"
            "scores\r\n"
            "$1\r\n"
            "0\r\n"
            "$2\r\n"
            "-1\r\n"
        );

    assert(
        response ==
        "*2\r\n"
        "$4\r\n"
        "Amit\r\n"
        "$9\r\n"
        "Veerendra\r\n"
    );


    // ==============================
    // ZRANGE WITHSCORES
    // ==============================

    response =
        send_command(
            "*5\r\n"
            "$6\r\n"
            "ZRANGE\r\n"
            "$6\r\n"
            "scores\r\n"
            "$1\r\n"
            "0\r\n"
            "$2\r\n"
            "-1\r\n"
            "$10\r\n"
            "WITHSCORES\r\n"
        );

    assert(
        response ==
        "*4\r\n"
        "$4\r\n"
        "Amit\r\n"
        "$2\r\n"
        "75\r\n"
        "$9\r\n"
        "Veerendra\r\n"
        "$3\r\n"
        "100\r\n"
    );


    // ==============================
    // ZREM
    // ==============================

    response =
        send_command(
            "*3\r\n"
            "$4\r\n"
            "ZREM\r\n"
            "$6\r\n"
            "scores\r\n"
            "$4\r\n"
            "Amit\r\n"
        );

    assert(response == ":1\r\n");


    // ==============================
    // ZCARD after ZREM
    // ==============================

    response =
        send_command(
            "*2\r\n"
            "$5\r\n"
            "ZCARD\r\n"
            "$6\r\n"
            "scores\r\n"
        );

    assert(response == ":1\r\n");


    // ==============================
    // Add member for ZPOPMIN
    // ==============================

    response =
        send_command(
            "*4\r\n"
            "$4\r\n"
            "ZADD\r\n"
            "$6\r\n"
            "scores\r\n"
            "$2\r\n"
            "25\r\n"
            "$4\r\n"
            "Amit\r\n"
        );

    assert(response == ":1\r\n");


    // ==============================
    // Add member for ZPOPMAX
    // ==============================

    response =
        send_command(
            "*4\r\n"
            "$4\r\n"
            "ZADD\r\n"
            "$6\r\n"
            "scores\r\n"
            "$3\r\n"
            "200\r\n"
            "$5\r\n"
            "Rahul\r\n"
        );

    assert(response == ":1\r\n");


    // ==============================
    // ZPOPMIN
    // ==============================

    response =
        send_command(
            "*2\r\n"
            "$7\r\n"
            "ZPOPMIN\r\n"
            "$6\r\n"
            "scores\r\n"
        );

    assert(
        response ==
        "*2\r\n"
        "$4\r\n"
        "Amit\r\n"
        "$2\r\n"
        "25\r\n"
    );


    // ==============================
    // ZPOPMAX
    // ==============================

    response =
        send_command(
            "*2\r\n"
            "$7\r\n"
            "ZPOPMAX\r\n"
            "$6\r\n"
            "scores\r\n"
        );

    assert(
        response ==
        "*2\r\n"
        "$5\r\n"
        "Rahul\r\n"
        "$3\r\n"
        "200\r\n"
    );


    // ==============================
    // UNKNOWN COMMAND
    // ==============================

    response =
        send_command(
            "*1\r\n"
            "$5\r\n"
            "PINGX\r\n"
        );

    assert(
        response ==
        "-ERR unknown command\r\n"
    );


    // ==============================
    // WRONG ARGUMENTS
    // ==============================

    response =
        send_command(
            "*1\r\n"
            "$3\r\n"
            "GET\r\n"
        );

    assert(
        response ==
        "-ERR wrong number of arguments\r\n"
    );


    std::cout
        << "All server integration tests passed!\n";

    return 0;
}
