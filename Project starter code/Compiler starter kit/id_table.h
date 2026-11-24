
/*
 * Joshua Wijesinghe
 * CSC-4510
 * Defines the symbol table using an array of binary search trees.
 *
 * Original Author: Michael Oudshoorn
 * Written: 1 June 2020
 * Last modified: 1 June 2023
 */

#ifndef ID_TABLE_H_
#define ID_TABLE_H_

#include <iostream>
#include <string>
#include <array>
#include <memory>
#include <vector>

#include "token.h"
#include "error_handler.h"
#include "lille_type.h"
#include "lille_kind.h"

using namespace std;

// Stores information about a formal parameter.
struct parameter_info {
    string name;
    lille_type::lille_ty type;
    lille_kind::lille_knd mode;
};

// Stores the attributes of an identifier.
struct identifier_info {
    string name;

    lille_type::lille_ty type = lille_type::type_unknown;
    lille_kind::lille_knd kind = lille_kind::unknown;

    int level = 0;
    int line = 0;
    int position = 0;
    int offset = 0;

    bool trace = false;

    string constant_value;

    vector<parameter_info> parameters;

    lille_type::lille_ty return_type =
        lille_type::type_unknown;
};

class id_table {
private:
    error_handler* error;

    static constexpr int MAX_SCOPES = 100;

    // Each node contains one identifier and two children.
    struct bst_node {
        identifier_info data;

        unique_ptr<bst_node> left;
        unique_ptr<bst_node> right;

        explicit bst_node(const identifier_info& info)
            : data(info) {}
    };

    // Each array entry is the root of a separate BST.
    array<unique_ptr<bst_node>, MAX_SCOPES> scopes;

    int current_scope;

    bool insert_node(unique_ptr<bst_node>& root,
                     const identifier_info& info);

    identifier_info* lookup_node(bst_node* root,
                                 const string& name);

    void print_inorder(bst_node* root);

public:
    explicit id_table(error_handler* err);

    void enter_scope();
    void exit_scope();

    int get_current_scope() const;

    bool insert(const identifier_info& info);

    identifier_info* lookup(const string& name);

    identifier_info* lookup_current_scope(
        const string& name);

    void dump_id_table(bool dump_all = true);
};

#endif /* ID_TABLE_H_ */
