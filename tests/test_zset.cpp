#include <iostream>
#include "../include/zset.h"

int main() {

    ZSet leaderboard;


    // Add members
    leaderboard.add(
        100,
        "Veerendra"
    );

    leaderboard.add(
        50,
        "Amit"
    );

    leaderboard.add(
        200,
        "Rahul"
    );


    std::cout
        << "Initial ZSET:\n";


    auto result =
        leaderboard.range();


    for (const auto& entry : result) {

        std::cout
            << entry.first
            << " "
            << entry.second
            << "\n";
    }


    // Update Veerendra
    leaderboard.add(
        150,
        "Veerendra"
    );


    std::cout
        << "\nAfter updating Veerendra:\n";


    result =
        leaderboard.range();


    for (const auto& entry : result) {

        std::cout
            << entry.first
            << " "
            << entry.second
            << "\n";
    }


    // Remove Amit
    leaderboard.remove(
        "Amit"
    );


    std::cout
        << "\nAfter removing Amit:\n";


    result =
        leaderboard.range();


    for (const auto& entry : result) {

        std::cout
            << entry.first
            << " "
            << entry.second
            << "\n";
    }


    return 0;
}
