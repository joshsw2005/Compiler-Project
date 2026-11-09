/* Joshua Wijesinghe | CSC-4510 | Recursive-descent Lille syntax parser.
 */
#ifndef PARSER_H
#define PARSER_H
#include "scanner.h"
#include "error_handler.h"

class parser {
    scanner& scan;
    error_handler& errors;
    using S = symbol::symbol_type;
    bool have(S s) { return scan.have(s); }
    void take(S s) { scan.must_be(s); }
    bool accept(S s) { if (have(s)) { take(s); return true; } return false; }
    [[noreturn]] void fail(int code);
    bool starts_statement();
    bool starts_expression();
    void block();
    void declaration();
    void identifier_list();
    void type();
    void parameters();
    void parameter();
    void statement_list();
    void statement();
    void if_statement();
    void loop_statement();
    void expression();
    void simple_expression();
    void expression2();
    void term();
    void factor();
    void primary();
    void arguments();
public:
    parser(scanner& scanner_ref, error_handler& error_ref) : scan(scanner_ref), errors(error_ref) {}
    void parse();
};
#endif
