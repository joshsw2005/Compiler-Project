
/*
 * Joshua Wijesinghe
 * CSC-4510
 * Implements BST insertion, identifier lookup, scope
 * management, and inorder symbol-table printing.
 */

/*
 * idtable.cpp
 *
 * Written:            1 June 2020
 * Last modified:      1 June 2023
 *      Author: Michael Oudshoorn
 */

#include <iostream>
#include <string>
#include <stdexcept>

#include "token.h"
#include "error_handler.h"
#include "id_table.h"

using namespace std;

// Initialize the symbol table at scope level 0.
id_table::id_table(error_handler* err)
{
    error = err;
    current_scope = 0;
}

// Return the currently active scope level.
int id_table::get_current_scope() const
{
    return current_scope;
}

// Enter a new scope with an empty BST.
void id_table::enter_scope()
{
    if (current_scope + 1 >= MAX_SCOPES)
    {
        throw runtime_error("Maximum scope depth exceeded");
    }

    current_scope++;
    scopes[current_scope].reset();
}

// Print all active scopes before removing the current scope.
void id_table::exit_scope()
{
    if (current_scope < 0)
    {
        return;
    }

    dump_id_table(true);

    scopes[current_scope].reset();
    current_scope--;
}

// Recursively insert an identifier into a BST.
bool id_table::insert_node(
    unique_ptr<bst_node>& root,
    const identifier_info& info)
{
    if (root == nullptr)
    {
        root = make_unique<bst_node>(info);
        return true;
    }

    if (info.name < root->data.name)
    {
        return insert_node(root->left, info);
    }

    if (info.name > root->data.name)
    {
        return insert_node(root->right, info);
    }

    // Duplicate identifier in the same scope.
    return false;
}

// Insert an identifier into the current scope.
bool id_table::insert(const identifier_info& info)
{
    if (current_scope < 0)
    {
        return false;
    }

    identifier_info entry = info;
    entry.level = current_scope;

    return insert_node(scopes[current_scope], entry);
}

// Search a single BST for an identifier.
identifier_info* id_table::lookup_node(
    bst_node* root,
    const string& name)
{
    if (root == nullptr)
    {
        return nullptr;
    }

    if (name == root->data.name)
    {
        return &root->data;
    }

    if (name < root->data.name)
    {
        return lookup_node(root->left.get(), name);
    }

    return lookup_node(root->right.get(), name);
}

// Search only the current scope for duplicate declarations.
identifier_info* id_table::lookup_current_scope(
    const string& name)
{
    if (current_scope < 0)
    {
        return nullptr;
    }

    return lookup_node(scopes[current_scope].get(), name);
}

// Search from the current scope outward.
identifier_info* id_table::lookup(const string& name)
{
    for (int level = current_scope; level >= 0; level--)
    {
        identifier_info* result =
            lookup_node(scopes[level].get(), name);

        if (result != nullptr)
        {
            return result;
        }
    }

    return nullptr;
}

// Print identifiers alphabetically using inorder traversal.
void id_table::print_inorder(bst_node* root)
{
    if (root == nullptr)
    {
        return;
    }

    // Visit the left subtree first.
    print_inorder(root->left.get());

    const identifier_info& info = root->data;

    cout << "Name: " << info.name << endl;
    cout << "  Type: "
         << lille_type(info.type).to_string() << endl;
    cout << "  Kind: "
         << lille_kind(info.kind).to_string() << endl;
    cout << "  Level: " << info.level << endl;
    cout << "  Line: " << info.line << endl;
    cout << "  Position: " << info.position << endl;
    cout << "  Offset: " << info.offset << endl;
    cout << "  Trace: "
         << (info.trace ? "Yes" : "No") << endl;

    // Print constant information when applicable.
    if (info.kind == lille_kind::constant)
    {
        cout << "  Constant value: "
             << info.constant_value << endl;
    }

    // Print procedure and function parameter information.
    if (info.type == lille_type::type_proc ||
        info.type == lille_type::type_func)
    {
        cout << "  Number of parameters: "
             << info.parameters.size() << endl;

        for (size_t i = 0; i < info.parameters.size(); i++)
        {
            const parameter_info& param =
                info.parameters[i];

            cout << "    Parameter " << i + 1 << ": "
                 << param.name
                 << ", Type: "
                 << lille_type(param.type).to_string()
                 << ", Mode: "
                 << lille_kind(param.mode).to_string()
                 << endl;
        }
    }

    // Functions also have a return type.
    if (info.type == lille_type::type_func)
    {
        cout << "  Return type: "
             << lille_type(info.return_type).to_string()
             << endl;
    }

    cout << endl;

    // Visit the right subtree last.
    print_inorder(root->right.get());
}

// Print the current scope or all active scopes.
void id_table::dump_id_table(bool dump_all)
{
    if (current_scope < 0)
    {
        return;
    }

    cout << endl;

    if (dump_all)
    {
        cout << "Dump of the entire symbol table." << endl;
        cout << "~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~" << endl;
    }
    else
    {
        cout << "Dump of idtable for current scope only."
             << endl;
        cout << "~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~"
             << endl;
    }

    int first_scope = dump_all ? 0 : current_scope;

    for (int level = first_scope;
         level <= current_scope;
         level++)
    {
        cout << "Scope level " << level << endl;
        cout << "------------------------" << endl;

        if (scopes[level] == nullptr)
        {
            cout << "(Empty scope)" << endl;
        }
        else
        {
            print_inorder(scopes[level].get());
        }

        cout << endl;
    }
}
