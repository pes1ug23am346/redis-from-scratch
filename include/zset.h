#ifndef ZSET_H
#define ZSET_H

#include <string>
#include <vector>
#include <utility>
#include "../include/avl_tree.h"
#include "../include/hash_table.h"

class ZSet {

private:

    // Fast lookup:
    // member -> score
    HashTable scores;

    // Sorted structure:
    // score -> member
    AVLTree tree;


public:

    // ----------------------------------
    // ZADD
    // ----------------------------------

    bool add(
        double score,
        const std::string& member
    ) {

        std::string old_score_string;


        // Check whether member already exists
        if (
            scores.get(
                member,
                old_score_string
            )
        ) {

            double old_score =
                std::stod(old_score_string);


            // Same score:
            // member already exists,
            // nothing changes.
            if (old_score == score) {
                return false;
            }


            // Remove old score/member
            // from AVL tree.
            tree.remove(
                old_score,
                member
            );


            // Update score in HashTable.
            scores.set(
                member,
                std::to_string(score)
            );


            // Insert new score/member.
            tree.insert(
                score,
                member
            );


            // Existing member was updated.
            return false;
        }


        // New member.
        scores.set(
            member,
            std::to_string(score)
        );


        tree.insert(
            score,
            member
        );


        // New member was added.
        return true;
    }


    // ----------------------------------
    // ZSCORE
    // ----------------------------------

    bool get_score(
        const std::string& member,
        double& score
    ) const {

        std::string score_string;

        if (!scores.get(
                member,
                score_string
            )) {

            return false;
        }

        score =
            std::stod(score_string);

        return true;
    }


    // ----------------------------------
    // ZREM
    // ----------------------------------

    bool remove(
        const std::string& member
    ) {

        std::string score_string;


        // Member doesn't exist
        if (
            !scores.get(
                member,
                score_string
            )
        ) {

            return false;
        }


        double score =
            std::stod(score_string);


        // Remove from AVL
        tree.remove(
            score,
            member
        );


        // Remove from HashTable
        return scores.remove(
            member
        );
    }


    // ----------------------------------
    // ZPOPMIN
    // ----------------------------------

    bool pop_min(
        double& score,
        std::string& member
    ) {

        auto elements =
            tree.get_all();

        if (elements.empty()) {
            return false;
        }

        score =
            elements.front().first;

        member =
            elements.front().second;

        return remove(member);
    }


    // ----------------------------------
    // ZPOPMAX
    // ----------------------------------

    bool pop_max(
        double& score,
        std::string& member
    ) {

        auto elements =
            tree.get_all();

        if (elements.empty()) {
            return false;
        }

        score =
            elements.back().first;

        member =
            elements.back().second;

        return remove(member);
    }


    // ----------------------------------
    // ZINCRBY
    // ----------------------------------

    double increment(
        double amount,
        const std::string& member
    ) {

        std::string old_score_string;

        double old_score = 0;


        // Check whether member exists
        if (
            scores.get(
                member,
                old_score_string
            )
        ) {

            old_score =
                std::stod(old_score_string);


            // Remove old score from AVL
            tree.remove(
                old_score,
                member
            );
        }


        // Calculate new score
        double new_score =
            old_score + amount;


        // Update HashTable
        scores.set(
            member,
            std::to_string(new_score)
        );


        // Insert new score into AVL
        tree.insert(
            new_score,
            member
        );


        return new_score;
    }


    // ----------------------------------
    // ZCARD
    // ----------------------------------

    int size() const {

        return static_cast<int>(
            tree.get_all().size()
        );
    }


    // ----------------------------------
    // ZRANGE
    // ----------------------------------

    std::vector<
        std::pair<double, std::string>
    > range() const {

        return tree.get_all();
    }
};

#endif
