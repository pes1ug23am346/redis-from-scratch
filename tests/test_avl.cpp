#include <cassert>
#include <iostream>
#include "../include/avl_tree.h"

int main() {

    AVLTree tree;

    // ==============================
    // INSERT
    // ==============================

    tree.insert(50, "Veerendra");
    tree.insert(30, "Amit");
    tree.insert(70, "Rahul");
    tree.insert(20, "Kiran");
    tree.insert(40, "Rohan");
    tree.insert(60, "Suresh");
    tree.insert(80, "Arjun");


    // ==============================
    // INITIAL TREE
    // ==============================

    auto result =
        tree.get_all();

    assert(result.size() == 7);

    assert(result[0].first == 20);
    assert(result[0].second == "Kiran");

    assert(result[1].first == 30);
    assert(result[1].second == "Amit");

    assert(result[2].first == 40);
    assert(result[2].second == "Rohan");

    assert(result[3].first == 50);
    assert(result[3].second == "Veerendra");

    assert(result[4].first == 60);
    assert(result[4].second == "Suresh");

    assert(result[5].first == 70);
    assert(result[5].second == "Rahul");

    assert(result[6].first == 80);
    assert(result[6].second == "Arjun");


    // ==============================
    // DELETE LEAF
    // ==============================

    tree.remove(
        20,
        "Kiran"
    );

    result =
        tree.get_all();

    assert(result.size() == 6);

    assert(result[0].first == 30);
    assert(result[1].first == 40);
    assert(result[2].first == 50);
    assert(result[3].first == 60);
    assert(result[4].first == 70);
    assert(result[5].first == 80);


    // ==============================
    // DELETE NODE WITH TWO CHILDREN
    // ==============================

    tree.remove(
        70,
        "Rahul"
    );

    result =
        tree.get_all();

    assert(result.size() == 5);

    assert(result[0].first == 30);
    assert(result[1].first == 40);
    assert(result[2].first == 50);
    assert(result[3].first == 60);
    assert(result[4].first == 80);


    // ==============================
    // DELETE ROOT
    // ==============================

    tree.remove(
        50,
        "Veerendra"
    );

    result =
        tree.get_all();

    assert(result.size() == 4);

    assert(result[0].first == 30);
    assert(result[0].second == "Amit");

    assert(result[1].first == 40);
    assert(result[1].second == "Rohan");

    assert(result[2].first == 60);
    assert(result[2].second == "Suresh");

    assert(result[3].first == 80);
    assert(result[3].second == "Arjun");


    // ==============================
    // REMOVE NON-EXISTING NODE
    // ==============================

    tree.remove(
        999,
        "Unknown"
    );

    result =
        tree.get_all();

    assert(result.size() == 4);


    std::cout
        << "All AVL tests passed!\n";

    return 0;
}
