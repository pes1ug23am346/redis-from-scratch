#ifndef HASH_TABLE_H
#define HASH_TABLE_H

#include <string>
#include <vector>
#include <utility>

class HashTable {

private:

    struct Entry {

        std::string key;
        std::string value;

        enum class State {
            EMPTY,
            OCCUPIED,
            DELETED
        };

        State state = State::EMPTY;
    };

    std::vector<Entry> table;

    size_t size;


    // ----------------------------------
    // Hash function
    // ----------------------------------

    size_t hash(const std::string& key) const {

        size_t hash_value = 0;

        for (char c : key) {
            hash_value =
                hash_value * 31 + c;
        }

        return hash_value;
    }


    // ----------------------------------
    // Insert during resize
    // ----------------------------------

    void insert_entry(
        std::vector<Entry>& target,
        const std::string& key,
        const std::string& value
    ) {

        size_t index =
            hash(key) % target.size();

        while (
            target[index].state ==
            Entry::State::OCCUPIED
        ) {

            index =
                (index + 1) % target.size();
        }

        target[index].key = key;
        target[index].value = value;
        target[index].state =
            Entry::State::OCCUPIED;
    }


    // ----------------------------------
    // Resize
    // ----------------------------------

    void resize() {

        size_t new_capacity =
            table.size() * 2;

        std::vector<Entry> new_table(
            new_capacity
        );


        for (const auto& entry : table) {

            if (
                entry.state ==
                Entry::State::OCCUPIED
            ) {

                insert_entry(
                    new_table,
                    entry.key,
                    entry.value
                );
            }
        }

        table =
            std::move(new_table);
    }


public:

    // ----------------------------------
    // Constructor
    // ----------------------------------

    HashTable(size_t capacity = 8)
        : table(capacity),
          size(0) {
    }


    // ----------------------------------
    // SET
    // ----------------------------------

    void set(
        const std::string& key,
        const std::string& value
    ) {

        if (
            (size + 1) * 100 >=
            table.size() * 70
        ) {

            resize();
        }


        size_t index =
            hash(key) % table.size();


        while (
            table[index].state !=
            Entry::State::EMPTY
        ) {

            // Existing key
            if (
                table[index].state ==
                    Entry::State::OCCUPIED &&
                table[index].key == key
            ) {

                table[index].value =
                    value;

                return;
            }

            index =
                (index + 1) % table.size();
        }


        table[index].key = key;

        table[index].value = value;

        table[index].state =
            Entry::State::OCCUPIED;

        size++;
    }


    // ----------------------------------
    // GET
    // ----------------------------------

    bool get(
        const std::string& key,
        std::string& value
    ) const {

        size_t index =
            hash(key) % table.size();


        while (
            table[index].state !=
            Entry::State::EMPTY
        ) {

            if (
                table[index].state ==
                    Entry::State::OCCUPIED &&
                table[index].key == key
            ) {

                value =
                    table[index].value;

                return true;
            }


            // DELETED:
            // keep searching


            index =
                (index + 1) % table.size();
        }


        return false;
    }


    // ----------------------------------
    // DEL
    // ----------------------------------

    bool remove(
        const std::string& key
    ) {

        size_t index =
            hash(key) % table.size();


        while (
            table[index].state !=
            Entry::State::EMPTY
        ) {

            if (
                table[index].state ==
                    Entry::State::OCCUPIED &&
                table[index].key == key
            ) {

                table[index].state =
                    Entry::State::DELETED;

                table[index].key.clear();
                table[index].value.clear();

                size--;

                return true;
            }


            index =
                (index + 1) % table.size();
        }


        return false;
    }
    
    // ----------------------------------
    // Get all key-value pairs
    // ----------------------------------

    std::vector<std::pair<std::string, std::string>> get_all() const {

        std::vector<std::pair<std::string, std::string>> result;

        for (const auto& entry : table) {

            if (
                entry.state ==
                Entry::State::OCCUPIED
            ) {

                result.push_back({
                    entry.key,
                    entry.value
                });
            }
        }

        return result;
    }

};

#endif