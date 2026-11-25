
/* Joshua Wijesinghe | CSC-4510 | Recursive-descent Lille syntax parser.
 * Extended to support static semantic analysis and type checking.
 */
#ifndef PARSER_H
#define PARSER_H
#include "scanner.h"
#include "error_handler.h"
#include "id_table.h"

class parser {
    scanner& scan;
    error_handler& errors;
    using S = symbol::symbol_type;
    using T = lille_type::lille_ty;
    bool have(S s) { return scan.have(s); }
    void take(S s) { scan.must_be(s); }
    bool accept(S s) { if (have(s)) { take(s); return true; } return false; }
    [[noreturn]] void fail(int code);
    bool starts_statement();
    bool starts_expression();
    void block();
    void declaration();
    vector<identifier_info> identifier_list();
    T type();
    void parameters();
    void parameter();
    void statement_list();
    void statement();
    void if_statement();
    void loop_statement();
    T expression();
    T simple_expression();
    T expression2();
    T term();
    T factor();
    T primary();
    void arguments();

    // Semantic analysis
    id_table& identifiers;
    int loop_depth = 0;

    // Tracks the enclosing program, procedure, or function.
    struct block_info {
        string name;
        T type;
        T return_type;
        bool has_return = false;
    };

    vector<block_info> blocks;

    // Semantic checking helper functions.
    bool is_arithmetic(T type);
    bool compatible(T expected, T actual);
    bool is_modifiable(const identifier_info& info);

    void declare_identifier(const identifier_info& info);
    identifier_info* require_identifier(const string& name);

    void register_predefined_functions();

    [[noreturn]] void semantic_error(const string& message);

public:
    parser(scanner& scanner_ref, error_handler& error_ref,
           id_table& table_ref)
        : scan(scanner_ref), errors(error_ref), identifiers(table_ref) {}
    void parse();
};
#endif
