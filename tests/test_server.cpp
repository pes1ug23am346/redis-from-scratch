#include <cassert>
#include <cstring>
#include <fstream>
#include <iostream>
#include <string>

#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/stat.h>
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


std::string send_commands(
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

    assert(
        sent ==
        static_cast<ssize_t>(request.size())
    );


    // Don't allow the test to hang forever.
    timeval timeout{};
    timeout.tv_sec = 2;
    timeout.tv_usec = 0;

    setsockopt(
        fd,
        SOL_SOCKET,
        SO_RCVTIMEO,
        &timeout,
        sizeof(timeout)
    );


    std::string result;

    char buffer[4096];

    while (true) {

        ssize_t received =
            recv(
                fd,
                buffer,
                sizeof(buffer),
                0
            );

        if (received <= 0) {
            break;
        }

        result.append(
            buffer,
            received
        );
    }


    close(fd);

    return result;
}



int connect_to_server() {

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

    return fd;
}


std::string send_on_connection(
    int fd,
    const std::string& request
) {

    ssize_t sent =
        send(
            fd,
            request.c_str(),
            request.size(),
            0
        );

    assert(
        sent ==
        static_cast<ssize_t>(request.size())
    );


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
    // TRANSACTION: MULTI / SET / EXEC
    // ==============================

    response =
        send_commands(
            "*1\r\n"
            "$5\r\n"
            "MULTI\r\n"

            "*3\r\n"
            "$3\r\n"
            "SET\r\n"
            "$5\r\n"
            "txkey\r\n"
            "$7\r\n"
            "txvalue\r\n"

            "*1\r\n"
            "$4\r\n"
            "EXEC\r\n"
        );

    assert(
        response ==
        "+OK\r\n"
        "+QUEUED\r\n"
        "*1\r\n"
        "+OK\r\n"
    );


    // ==============================
    // TRANSACTION: SET / GET / EXEC
    // ==============================

    response =
        send_commands(
            "*1\r\n"
            "$5\r\n"
            "MULTI\r\n"

            "*3\r\n"
            "$3\r\n"
            "SET\r\n"
            "$6\r\n"
            "txkey2\r\n"
            "$7\r\n"
            "txvalue\r\n"

            "*2\r\n"
            "$3\r\n"
            "GET\r\n"
            "$6\r\n"
            "txkey2\r\n"

            "*1\r\n"
            "$4\r\n"
            "EXEC\r\n"
        );

    assert(
        response ==
        "+OK\r\n"
        "+QUEUED\r\n"
        "+QUEUED\r\n"
        "*2\r\n"
        "+OK\r\n"
        "$7\r\n"
        "txvalue\r\n"
    );


    // ==============================
    // TRANSACTION: MULTI / DISCARD
    // ==============================

    response =
        send_commands(
            "*1\r\n"
            "$5\r\n"
            "MULTI\r\n"

            "*3\r\n"
            "$3\r\n"
            "SET\r\n"
            "$11\r\n"
            "discard_key\r\n"
            "$5\r\n"
            "value\r\n"

            "*1\r\n"
            "$7\r\n"
            "DISCARD\r\n"
        );

    assert(
        response ==
        "+OK\r\n"
        "+QUEUED\r\n"
        "+OK\r\n"
    );


    // ==============================
    // EXEC WITHOUT MULTI
    // ==============================

    response =
        send_command(
            "*1\r\n"
            "$4\r\n"
            "EXEC\r\n"
        );

    assert(
        response ==
        "-ERR EXEC without MULTI\r\n"
    );


    // ==============================
    // NESTED MULTI
    // ==============================

    response =
        send_commands(
            "*1\r\n"
            "$5\r\n"
            "MULTI\r\n"

            "*1\r\n"
            "$5\r\n"
            "MULTI\r\n"

            "*1\r\n"
            "$7\r\n"
            "DISCARD\r\n"
        );

    assert(
        response ==
        "+OK\r\n"
        "-ERR MULTI calls can not be nested\r\n"
        "+OK\r\n"
    );


    // ==============================
    // TRANSACTION: ZADD / EXEC
    // ==============================

    response =
        send_commands(
            "*1\r\n"
            "$5\r\n"
            "MULTI\r\n"

            "*4\r\n"
            "$4\r\n"
            "ZADD\r\n"
            "$8\r\n"
            "txscores\r\n"
            "$3\r\n"
            "100\r\n"
            "$9\r\n"
            "Veerendra\r\n"

            "*4\r\n"
            "$4\r\n"
            "ZADD\r\n"
            "$8\r\n"
            "txscores\r\n"
            "$3\r\n"
            "200\r\n"
            "$5\r\n"
            "Rahul\r\n"

            "*1\r\n"
            "$4\r\n"
            "EXEC\r\n"
        );

    assert(
        response ==
        "+OK\r\n"
        "+QUEUED\r\n"
        "+QUEUED\r\n"
        "*2\r\n"
        ":1\r\n"
        ":1\r\n"
    );


    // ==============================
    // TRANSACTION: LPUSH / EXEC
    // ==============================

    response =
        send_commands(
            "*1\r\n"
            "$5\r\n"
            "MULTI\r\n"

            "*3\r\n"
            "$5\r\n"
            "LPUSH\r\n"
            "$6\r\n"
            "txlist\r\n"
            "$3\r\n"
            "one\r\n"

            "*3\r\n"
            "$5\r\n"
            "LPUSH\r\n"
            "$6\r\n"
            "txlist\r\n"
            "$3\r\n"
            "two\r\n"

            "*1\r\n"
            "$4\r\n"
            "EXEC\r\n"
        );

    assert(
        response ==
        "+OK\r\n"
        "+QUEUED\r\n"
        "+QUEUED\r\n"
        "*2\r\n"
        ":1\r\n"
        ":2\r\n"
    );


    // ==============================
    // PERSISTENCE: SET / SAVE
    // ==============================

    response =
        send_command(
            "*3\r\n"
            "$3\r\n"
            "SET\r\n"
            "$11\r\n"
            "persist_key\r\n"
            "$13\r\n"
            "persist_value\r\n"
        );

    assert(
        response ==
        "+OK\r\n"
    );


    response =
        send_command(
            "*3\r\n"
            "$3\r\n"
            "SET\r\n"
            "$12\r\n"
            "persist_key2\r\n"
            "$14\r\n"
            "persist_value2\r\n"
        );

    assert(
        response ==
        "+OK\r\n"
    );


    response =
        send_command(
            "*1\r\n"
            "$4\r\n"
            "SAVE\r\n"
        );

    assert(
        response ==
        "+OK\r\n"
    );


    struct stat file_info{};

    int file_exists =
        stat(
            "../data/dump.rdb",
            &file_info
        );

    assert(file_exists == 0);

    assert(file_info.st_size > 0);


    // ==============================
    // PERSISTENCE: EMPTY DATABASE
    // ==============================

    response =
        send_command(
            "*2\r\n"
            "$3\r\n"
            "DEL\r\n"
            "$11\r\n"
            "persist_key\r\n"
        );

    assert(
        response ==
        ":1\r\n"
    );


    response =
        send_command(
            "*2\r\n"
            "$3\r\n"
            "DEL\r\n"
            "$12\r\n"
            "persist_key2\r\n"
        );

    assert(
        response ==
        ":1\r\n"
    );


    response =
        send_command(
            "*1\r\n"
            "$4\r\n"
            "SAVE\r\n"
        );

    assert(
        response ==
        "+OK\r\n"
    );


    struct stat empty_file_info{};

    int empty_file_exists =
        stat(
            "../data/dump.rdb",
            &empty_file_info
        );

    assert(empty_file_exists == 0);

    assert(empty_file_info.st_size > 0);


    // ==============================
    // TTL: DEL clears expiration
    // ==============================

    response =
        send_command(
            "*3\r\n"
            "$3\r\n"
            "SET\r\n"
            "$12\r\n"
            "del_ttl_test\r\n"
            "$5\r\n"
            "hello\r\n"
        );

    assert(
        response ==
        "+OK\r\n"
    );


    response =
        send_command(
            "*3\r\n"
            "$7\r\n"
            "PEXPIRE\r\n"
            "$12\r\n"
            "del_ttl_test\r\n"
            "$4\r\n"
            "5000\r\n"
        );

    assert(
        response ==
        ":1\r\n"
    );


    response =
        send_command(
            "*2\r\n"
            "$3\r\n"
            "DEL\r\n"
            "$12\r\n"
            "del_ttl_test\r\n"
        );

    assert(
        response ==
        ":1\r\n"
    );


    response =
        send_command(
            "*3\r\n"
            "$3\r\n"
            "SET\r\n"
            "$12\r\n"
            "del_ttl_test\r\n"
            "$6\r\n"
            "value2\r\n"
        );

    assert(
        response ==
        "+OK\r\n"
    );


    response =
        send_command(
            "*2\r\n"
            "$4\r\n"
            "PTTL\r\n"
            "$12\r\n"
            "del_ttl_test\r\n"
        );

    assert(
        response ==
        ":-1\r\n"
    );


    // ==============================
    // TTL: SET clears expiration
    // ==============================

    response =
        send_command(
            "*3\r\n"
            "$3\r\n"
            "SET\r\n"
            "$12\r\n"
            "set_ttl_test\r\n"
            "$6\r\n"
            "value1\r\n"
        );

    assert(
        response ==
        "+OK\r\n"
    );


    response =
        send_command(
            "*3\r\n"
            "$7\r\n"
            "PEXPIRE\r\n"
            "$12\r\n"
            "set_ttl_test\r\n"
            "$4\r\n"
            "5000\r\n"
        );

    assert(
        response ==
        ":1\r\n"
    );


    response =
        send_command(
            "*3\r\n"
            "$3\r\n"
            "SET\r\n"
            "$12\r\n"
            "set_ttl_test\r\n"
            "$6\r\n"
            "value2\r\n"
        );

    assert(
        response ==
        "+OK\r\n"
    );


    response =
        send_command(
            "*2\r\n"
            "$4\r\n"
            "PTTL\r\n"
            "$12\r\n"
            "set_ttl_test\r\n"
        );

    assert(
        response ==
        ":-1\r\n"
    );


    // ==============================
    // TRANSACTIONS
    // ==============================

    // MULTI -> SET -> GET -> EXEC

    {
        int fd = connect_to_server();

        response =
            send_on_connection(
                fd,
                "*1\r\n"
                "$5\r\n"
                "MULTI\r\n"
            );

        assert(response == "+OK\r\n");


        response =
            send_on_connection(
                fd,
                "*3\r\n"
                "$3\r\n"
                "SET\r\n"
                "$7\r\n"
                "tx_test\r\n"
                "$5\r\n"
                "hello\r\n"
            );

        assert(response == "+QUEUED\r\n");


        response =
            send_on_connection(
                fd,
                "*2\r\n"
                "$3\r\n"
                "GET\r\n"
                "$7\r\n"
                "tx_test\r\n"
            );

        assert(response == "+QUEUED\r\n");


        response =
            send_on_connection(
                fd,
                "*1\r\n"
                "$4\r\n"
                "EXEC\r\n"
            );

        assert(
            response ==
            "*2\r\n"
            "+OK\r\n"
            "$5\r\n"
            "hello\r\n"
        );

        close(fd);
    }


    // MULTI -> SET -> PEXPIRE -> EXEC

    {
        int fd = connect_to_server();

        response =
            send_on_connection(
                fd,
                "*1\r\n"
                "$5\r\n"
                "MULTI\r\n"
            );

        assert(response == "+OK\r\n");


        response =
            send_on_connection(
                fd,
                "*3\r\n"
                "$3\r\n"
                "SET\r\n"
                "$11\r\n"
                "tx_ttl_test\r\n"
                "$5\r\n"
                "hello\r\n"
            );

        assert(response == "+QUEUED\r\n");


        response =
            send_on_connection(
                fd,
                "*3\r\n"
                "$7\r\n"
                "PEXPIRE\r\n"
                "$11\r\n"
                "tx_ttl_test\r\n"
                "$4\r\n"
                "1000\r\n"
            );

        assert(response == "+QUEUED\r\n");


        response =
            send_on_connection(
                fd,
                "*1\r\n"
                "$4\r\n"
                "EXEC\r\n"
            );

        assert(
            response ==
            "*2\r\n"
            "+OK\r\n"
            ":1\r\n"
        );

        close(fd);
    }


    // MULTI -> DEL -> EXEC

    {
        int fd = connect_to_server();

        response =
            send_on_connection(
                fd,
                "*1\r\n"
                "$5\r\n"
                "MULTI\r\n"
            );

        assert(response == "+OK\r\n");


        response =
            send_on_connection(
                fd,
                "*2\r\n"
                "$3\r\n"
                "DEL\r\n"
                "$11\r\n"
                "tx_ttl_test\r\n"
            );

        assert(response == "+QUEUED\r\n");


        response =
            send_on_connection(
                fd,
                "*1\r\n"
                "$4\r\n"
                "EXEC\r\n"
            );

        assert(
            response ==
            "*1\r\n"
            ":1\r\n"
        );

        close(fd);
    }


    response =
        send_command(
            "*2\r\n"
            "$3\r\n"
            "GET\r\n"
            "$11\r\n"
            "tx_ttl_test\r\n"
        );

    assert(response == "$-1\r\n");


    // MULTI -> SET -> DISCARD

    {
        int fd = connect_to_server();

        response =
            send_on_connection(
                fd,
                "*1\r\n"
                "$5\r\n"
                "MULTI\r\n"
            );

        assert(response == "+OK\r\n");


        response =
            send_on_connection(
                fd,
                "*3\r\n"
                "$3\r\n"
                "SET\r\n"
                "$12\r\n"
                "discard_test\r\n"
                "$5\r\n"
                "hello\r\n"
            );

        assert(response == "+QUEUED\r\n");


        response =
            send_on_connection(
                fd,
                "*1\r\n"
                "$7\r\n"
                "DISCARD\r\n"
            );

        assert(response == "+OK\r\n");

        close(fd);
    }


    response =
        send_command(
            "*2\r\n"
            "$3\r\n"
            "GET\r\n"
            "$12\r\n"
            "discard_test\r\n"
        );

    assert(response == "$-1\r\n");


    // Invalid command inside MULTI

    {
        int fd = connect_to_server();

        response =
            send_on_connection(
                fd,
                "*1\r\n"
                "$5\r\n"
                "MULTI\r\n"
            );

        assert(response == "+OK\r\n");


        response =
            send_on_connection(
                fd,
                "*1\r\n"
                "$5\r\n"
                "PINGX\r\n"
            );

        assert(response == "+QUEUED\r\n");


        response =
            send_on_connection(
                fd,
                "*1\r\n"
                "$4\r\n"
                "EXEC\r\n"
            );

        assert(
            response ==
            "*1\r\n"
            "-ERR unknown command\r\n"
        );

        close(fd);
    }


    // ==============================
    // TRANSACTION ISOLATION
    // ==============================

    {
        int client_a = connect_to_server();
        int client_b = connect_to_server();


        // Client A starts a transaction.

        response =
            send_on_connection(
                client_a,
                "*1\r\n"
                "$5\r\n"
                "MULTI\r\n"
            );

        assert(response == "+OK\r\n");


        // SET is queued on Client A.

        response =
            send_on_connection(
                client_a,
                "*3\r\n"
                "$3\r\n"
                "SET\r\n"
                "$17\r\n"
                "tx_isolation_2026\r\n"
                "$5\r\n"
                "hello\r\n"
            );

        assert(response == "+QUEUED\r\n");


        // Client B must not see the queued value.

        response =
            send_on_connection(
                client_b,
                "*2\r\n"
                "$3\r\n"
                "GET\r\n"
                "$17\r\n"
                "tx_isolation_2026\r\n"
            );

        assert(response == "$-1\r\n");


        // Client A executes its transaction.

        response =
            send_on_connection(
                client_a,
                "*1\r\n"
                "$4\r\n"
                "EXEC\r\n"
            );

        assert(
            response ==
            "*1\r\n"
            "+OK\r\n"
        );


        // Now Client B can see the committed value.

        response =
            send_on_connection(
                client_b,
                "*2\r\n"
                "$3\r\n"
                "GET\r\n"
                "$17\r\n"
                "tx_isolation_2026\r\n"
            );

        assert(
            response ==
            "$5\r\n"
            "hello\r\n"
        );


        close(client_a);
        close(client_b);
    }


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
