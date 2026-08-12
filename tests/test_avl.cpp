#include <iostream>
#include "../include/avl_tree.h"

void print_tree(
    const AVLTree& tree,
    const std::string& label
) {
    std::cout << "\n" << label << "\n";

    auto result = tree.get_all();

    for (const auto& entry : result) {

        std::cout
            << entry.first
            << " "
            << entry.second
            << "\n";
    }
}

int main() {

    AVLTree tree;

    // Build tree
    tree.insert(50, "Veerendra");
    tree.insert(30, "Amit");
    tree.insert(70, "Rahul");
    tree.insert(20, "Kiran");
    tree.insert(40, "Rohan");
    tree.insert(60, "Suresh");
    tree.insert(80, "Arjun");

    print_tree(
        tree,
        "Initial tree:"
    );


    // Delete leaf
    tree.remove(
        20,
        "Kiran"
    );

    print_tree(
        tree,
        "After deleting 20:"
    );


    // Delete node with two children
    tree.remove(
        70,
        "Rahul"
    );

    print_tree(
        tree,
        "After deleting 70:"
    );


    // Delete root
    tree.remove(
        50,
        "Veerendra"
    );

    print_tree(
        tree,
        "After deleting 50:"
    );


    return 0;
}
