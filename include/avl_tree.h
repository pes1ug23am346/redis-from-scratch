#ifndef AVL_TREE_H
#define AVL_TREE_H

#include <string>
#include <algorithm>
#include <vector>
#include <utility>

class AVLTree {

private:

    struct Node {

        double score;
        std::string member;

        Node* left;
        Node* right;

        int height;

        Node(
            double s,
            const std::string& m
        )
            : score(s),
              member(m),
              left(nullptr),
              right(nullptr),
              height(1) {
        }
    };


    Node* root = nullptr;


    // ----------------------------------
    // Height
    // ----------------------------------

    int get_height(Node* node) const {

        if (node == nullptr) {
            return 0;
        }

        return node->height;
    }


    // ----------------------------------
    // Balance factor
    // ----------------------------------

    int get_balance(Node* node) const {

        if (node == nullptr) {
            return 0;
        }

        return get_height(node->left)
             - get_height(node->right);
    }


    // ----------------------------------
    // Update height
    // ----------------------------------

    void update_height(Node* node) {

        node->height =
            1 + std::max(
                get_height(node->left),
                get_height(node->right)
            );
    }


    // ----------------------------------
    // Compare two nodes
    //
    // Score first.
    // Member breaks ties.
    // ----------------------------------

    bool less_than(
        double score,
        const std::string& member,
        Node* node
    ) const {

        if (score < node->score) {
            return true;
        }

        if (score > node->score) {
            return false;
        }

        return member < node->member;
    }


    // ----------------------------------
    // RIGHT ROTATION
    // ----------------------------------

    Node* right_rotate(Node* y) {

        Node* x =
            y->left;

        Node* B =
            x->right;

        x->right = y;

        y->left = B;

        update_height(y);
        update_height(x);

        return x;
    }


    // ----------------------------------
    // LEFT ROTATION
    // ----------------------------------

    Node* left_rotate(Node* x) {

        Node* y =
            x->right;

        Node* B =
            y->left;

        y->left = x;

        x->right = B;

        update_height(x);
        update_height(y);

        return y;
    }


    // ----------------------------------
    // Find smallest node
    // ----------------------------------

    Node* get_min_node(Node* node) const {

        Node* current = node;

        while (
            current != nullptr &&
            current->left != nullptr
        ) {

            current =
                current->left;
        }

        return current;
    }


    // ----------------------------------
    // Insert
    // ----------------------------------

    Node* insert(
        Node* node,
        double score,
        const std::string& member
    ) {

        if (node == nullptr) {

            return new Node(
                score,
                member
            );
        }


        if (
            less_than(
                score,
                member,
                node
            )
        ) {

            node->left =
                insert(
                    node->left,
                    score,
                    member
                );

        } else {

            // If exactly same score + member,
            // don't insert duplicate.

            if (
                score == node->score &&
                member == node->member
            ) {

                return node;
            }

            node->right =
                insert(
                    node->right,
                    score,
                    member
                );
        }


        update_height(node);


        int balance =
            get_balance(node);


        // LL
        if (
            balance > 1 &&
            less_than(
                score,
                member,
                node->left
            )
        ) {

            return right_rotate(node);
        }


        // RR
        if (
            balance < -1 &&
            !less_than(
                score,
                member,
                node->right
            )
        ) {

            return left_rotate(node);
        }


        // LR
        if (
            balance > 1 &&
            !less_than(
                score,
                member,
                node->left
            )
        ) {

            node->left =
                left_rotate(
                    node->left
                );

            return right_rotate(node);
        }


        // RL
        if (
            balance < -1 &&
            less_than(
                score,
                member,
                node->right
            )
        ) {

            node->right =
                right_rotate(
                    node->right
                );

            return left_rotate(node);
        }


        return node;
    }


    // ----------------------------------
    // Delete
    // ----------------------------------

    Node* remove(
        Node* node,
        double score,
        const std::string& member
    ) {

        if (node == nullptr) {
            return nullptr;
        }


        // Search left
        if (
            less_than(
                score,
                member,
                node
            )
        ) {

            node->left =
                remove(
                    node->left,
                    score,
                    member
                );
        }


        // Search right
        else if (
            node->score != score ||
            node->member != member
        ) {

            node->right =
                remove(
                    node->right,
                    score,
                    member
                );
        }


        // Found node
        else {

            // Case 1:
            // No children

            if (
                node->left == nullptr &&
                node->right == nullptr
            ) {

                delete node;

                return nullptr;
            }


            // Case 2:
            // Only right child

            if (node->left == nullptr) {

                Node* temp =
                    node->right;

                delete node;

                return temp;
            }


            // Case 3:
            // Only left child

            if (node->right == nullptr) {

                Node* temp =
                    node->left;

                delete node;

                return temp;
            }


            // Case 4:
            // Two children
            //
            // Replace with inorder successor

            Node* successor =
                get_min_node(
                    node->right
                );


            node->score =
                successor->score;

            node->member =
                successor->member;


            node->right =
                remove(
                    node->right,
                    successor->score,
                    successor->member
                );
        }


        update_height(node);


        int balance =
            get_balance(node);


        // ----------------------------------
        // LL
        // ----------------------------------

        if (
            balance > 1 &&
            get_balance(node->left) >= 0
        ) {

            return right_rotate(node);
        }


        // ----------------------------------
        // LR
        // ----------------------------------

        if (
            balance > 1 &&
            get_balance(node->left) < 0
        ) {

            node->left =
                left_rotate(
                    node->left
                );

            return right_rotate(node);
        }


        // ----------------------------------
        // RR
        // ----------------------------------

        if (
            balance < -1 &&
            get_balance(node->right) <= 0
        ) {

            return left_rotate(node);
        }


        // ----------------------------------
        // RL
        // ----------------------------------

        if (
            balance < -1 &&
            get_balance(node->right) > 0
        ) {

            node->right =
                right_rotate(
                    node->right
                );

            return left_rotate(node);
        }


        return node;
    }


    // ----------------------------------
    // In-order traversal
    // ----------------------------------

    void inorder(
        Node* node,
        std::vector<
            std::pair<double, std::string>
        >& result
    ) const {

        if (node == nullptr) {
            return;
        }


        inorder(
            node->left,
            result
        );


        result.push_back({
            node->score,
            node->member
        });


        inorder(
            node->right,
            result
        );
    }


public:

    AVLTree() = default;


    // ----------------------------------
    // Insert
    // ----------------------------------

    void insert(
        double score,
        const std::string& member
    ) {

        root =
            insert(
                root,
                score,
                member
            );
    }


    // ----------------------------------
    // Remove
    // ----------------------------------

    void remove(
        double score,
        const std::string& member
    ) {

        root =
            remove(
                root,
                score,
                member
            );
    }


    // ----------------------------------
    // Get sorted elements
    // ----------------------------------

    std::vector<
        std::pair<double, std::string>
    > get_all() const {

        std::vector<
            std::pair<double, std::string>
        > result;


        inorder(
            root,
            result
        );


        return result;
    }
};

#endif
