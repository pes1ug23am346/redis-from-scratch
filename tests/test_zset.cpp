#include <cassert>
#include <iostream>
#include "../include/zset.h"

int main() {

    ZSet leaderboard;

    assert(leaderboard.add(100, "Veerendra") == true);
    assert(leaderboard.add(50, "Amit") == true);
    assert(leaderboard.add(200, "Rahul") == true);

    auto result = leaderboard.range();

    assert(result.size() == 3);

    assert(result[0].first == 50);
    assert(result[0].second == "Amit");

    assert(result[1].first == 100);
    assert(result[1].second == "Veerendra");

    assert(result[2].first == 200);
    assert(result[2].second == "Rahul");

    assert(
        leaderboard.add(150, "Veerendra") == false
    );

    result = leaderboard.range();

    assert(result.size() == 3);
    assert(result[0].first == 50);
    assert(result[0].second == "Amit");
    assert(result[1].first == 150);
    assert(result[1].second == "Veerendra");
    assert(result[2].first == 200);
    assert(result[2].second == "Rahul");

    double score;

    assert(
        leaderboard.get_score(
            "Veerendra",
            score
        ) == true
    );

    assert(score == 150);

    assert(
        leaderboard.get_score(
            "Unknown",
            score
        ) == false
    );

    assert(leaderboard.size() == 3);

    assert(
        leaderboard.remove("Amit") == true
    );

    assert(leaderboard.size() == 2);

    result = leaderboard.range();

    assert(result.size() == 2);
    assert(result[0].first == 150);
    assert(result[0].second == "Veerendra");
    assert(result[1].first == 200);
    assert(result[1].second == "Rahul");

    assert(
        leaderboard.remove("Unknown") == false
    );

    std::cout
        << "All ZSET tests passed!\n";

    return 0;
}
