#ifndef EXPIRY_HEAP_H
#define EXPIRY_HEAP_H

#include <string>
#include <vector>
#include <chrono>
#include <algorithm>
#include <unordered_map>

class ExpiryHeap {

public:

    struct ExpiryEntry {
        std::string key;
        long long expire_at;
    };


private:

    std::vector<ExpiryEntry> heap;

    // Stores the latest expiration for every key
    std::unordered_map<std::string, long long> latest_expiration;


    long long now_ms() const {

        return std::chrono::duration_cast<
            std::chrono::milliseconds
        >(
            std::chrono::steady_clock::now()
                .time_since_epoch()
        ).count();
    }


    void heapify_up(size_t index) {

        while (index > 0) {

            size_t parent = (index - 1) / 2;

            if (heap[parent].expire_at <=
                heap[index].expire_at) {
                break;
            }

            std::swap(
                heap[parent],
                heap[index]
            );

            index = parent;
        }
    }


    void heapify_down(size_t index) {

        while (true) {

            size_t left = 2 * index + 1;
            size_t right = 2 * index + 2;
            size_t smallest = index;

            if (
                left < heap.size() &&
                heap[left].expire_at <
                heap[smallest].expire_at
            ) {
                smallest = left;
            }

            if (
                right < heap.size() &&
                heap[right].expire_at <
                heap[smallest].expire_at
            ) {
                smallest = right;
            }

            if (smallest == index) {
                break;
            }

            std::swap(
                heap[index],
                heap[smallest]
            );

            index = smallest;
        }
    }


public:

    // Add or update expiration
    void add(
        const std::string& key,
        long long milliseconds
    ) {

        long long expire_at =
            now_ms() + milliseconds;

        // Remember the newest expiration
        latest_expiration[key] = expire_at;

        heap.push_back({
            key,
            expire_at
        });

        heapify_up(
            heap.size() - 1
        );
    }


    // Return the latest expiration of a key
    long long get_expire_at(
        const std::string& key
    ) const {

        auto it =
            latest_expiration.find(key);

        if (it == latest_expiration.end()) {
            return -1;
        }

        return it->second;
    }


    // Return expired entries
    std::vector<ExpiryEntry>
    get_expired_entries() {

        std::vector<ExpiryEntry> expired;

        long long current_time =
            now_ms();


        while (
            !heap.empty() &&
            heap[0].expire_at <= current_time
        ) {

            ExpiryEntry entry =
                heap[0];

            heap[0] =
                heap.back();

            heap.pop_back();

            if (!heap.empty()) {
                heapify_down(0);
            }


            // Check whether this is still
            // the latest expiration.
            auto it =
                latest_expiration.find(
                    entry.key
                );

            if (
                it != latest_expiration.end() &&
                it->second == entry.expire_at
            ) {

                expired.push_back(entry);

                // Remove expiration metadata
                latest_expiration.erase(it);
            }
        }

        return expired;
    }


    // Remove expiration metadata for a key
    void remove(
        const std::string& key
    ) {
        latest_expiration.erase(key);
    }


    // Remaining TTL
    long long get_ttl(
        const std::string& key
    ) const {

        long long expire_at =
            get_expire_at(key);

        // No expiration
        if (expire_at == -1) {
            return -1;
        }

        long long remaining =
            expire_at - now_ms();

        if (remaining <= 0) {
            return -1;
        }

        return remaining;
    }
};

#endif
