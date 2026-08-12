#ifndef LIST_H
#define LIST_H

#include <string>
#include <vector>

class RedisList {

private:

    struct Node {

        std::string value;

        Node* prev;
        Node* next;

        Node(const std::string& v)
            : value(v),
              prev(nullptr),
              next(nullptr) {
        }
    };


    Node* head = nullptr;
    Node* tail = nullptr;

    int length = 0;


public:

    RedisList() = default;


    // ----------------------------------
    // Destructor
    // ----------------------------------

    ~RedisList() {

        Node* current = head;

        while (current != nullptr) {

            Node* next =
                current->next;

            delete current;

            current = next;
        }
    }


    // ----------------------------------
    // LPUSH
    // ----------------------------------

    void push_front(
        const std::string& value
    ) {

        Node* new_node =
            new Node(value);

        if (head == nullptr) {

            head = new_node;
            tail = new_node;

        } else {

            new_node->next = head;

            head->prev =
                new_node;

            head = new_node;
        }

        length++;
    }


    // ----------------------------------
    // RPUSH
    // ----------------------------------

    void push_back(
        const std::string& value
    ) {

        Node* new_node =
            new Node(value);

        if (tail == nullptr) {

            head = new_node;
            tail = new_node;

        } else {

            new_node->prev = tail;

            tail->next =
                new_node;

            tail = new_node;
        }

        length++;
    }


    // ----------------------------------
    // LPOP
    // ----------------------------------

    bool pop_front(
        std::string& value
    ) {

        if (head == nullptr) {
            return false;
        }

        Node* old_head =
            head;

        value =
            old_head->value;

        if (head == tail) {

            head = nullptr;
            tail = nullptr;

        } else {

            head =
                old_head->next;

            head->prev =
                nullptr;
        }

        delete old_head;

        length--;

        return true;
    }


    // ----------------------------------
    // RPOP
    // ----------------------------------

    bool pop_back(
        std::string& value
    ) {

        if (tail == nullptr) {
            return false;
        }

        Node* old_tail =
            tail;

        value =
            old_tail->value;

        if (head == tail) {

            head = nullptr;
            tail = nullptr;

        } else {

            tail =
                old_tail->prev;

            tail->next =
                nullptr;
        }

        delete old_tail;

        length--;

        return true;
    }


    // ----------------------------------
    // LRANGE
    // ----------------------------------

    std::vector<std::string> get_range(
        int start,
        int stop
    ) const {

        std::vector<std::string> result;


        // Empty list
        if (length == 0) {
            return result;
        }


        // Redis-style negative indexes
        if (start < 0) {
            start = length + start;
        }

        if (stop < 0) {
            stop = length + stop;
        }


        // Clamp indexes
        if (start < 0) {
            start = 0;
        }

        if (stop >= length) {
            stop = length - 1;
        }


        // Invalid range
        if (
            start > stop ||
            start >= length
        ) {
            return result;
        }


        Node* current =
            head;


        int index = 0;


        while (
            current != nullptr &&
            index <= stop
        ) {

            if (index >= start) {

                result.push_back(
                    current->value
                );
            }

            current =
                current->next;

            index++;
        }


        return result;
    }


    // ----------------------------------
    // LLEN
    // ----------------------------------

    int size() const {

        return length;
    }
};

#endif
