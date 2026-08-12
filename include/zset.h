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
    // ZRANGE
    // ----------------------------------

    std::vector<
        std::pair<double, std::string>
    > range() const {

        return tree.get_all();
    }
};

#endif
