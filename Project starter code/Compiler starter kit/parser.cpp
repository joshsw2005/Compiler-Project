/* Joshua Wijesinghe | CSC-4510 | LL(1) recursive-descent parser for Lille.
 * FIRST sets choose alternatives; FOLLOW tokens terminate optional/repeated parts.
 */
#include "parser.h"
#include <stdexcept>

// Stop at the first syntax error (error recovery belongs to a later phase).
[[noreturn]] void parser::fail(int code) {
    errors.flag(scan.this_token(), code);
    throw std::runtime_error("Parsing stopped at first syntax error");
}

bool parser::starts_statement() {
    return have(S::identifier) || have(S::exit_sym) || have(S::return_sym) ||
           have(S::read_sym) || have(S::write_sym) || have(S::writeln_sym) ||
           have(S::null_sym) || have(S::if_sym) || have(S::loop_sym) ||
           have(S::while_sym) || have(S::for_sym);
}
bool parser::starts_expression() {
    return have(S::identifier) || have(S::integer) || have(S::real_num) ||
           have(S::strng) || have(S::true_sym) || have(S::false_sym) ||
           have(S::left_paren_sym) || have(S::not_sym) || have(S::odd_sym) ||
           have(S::plus_sym) || have(S::minus_sym) || have(S::eof_sym);
}

void parser::parse() {
    scan.get_token(); // Prime the scanner exactly once.
    take(S::program_sym);
    take(S::identifier);
    take(S::is_sym);
    block();
    take(S::semicolon_sym);
    take(S::end_of_program); // No extra tokens after the program.
}
void parser::block() {
    // FIRST(declaration) = identifier, procedure, function.
    while (have(S::identifier) || have(S::procedure_sym) || have(S::function_sym))
        declaration();
    take(S::begin_sym);
    statement_list();
    take(S::end_sym);
    accept(S::identifier); // Optional closing block name (semantic equality checked later).
}
void parser::identifier_list() {
    take(S::identifier);
    while (accept(S::comma_sym)) take(S::identifier);
}
void parser::type() {
    if (accept(S::integer_sym) || accept(S::real_sym) ||
        accept(S::string_sym) || accept(S::boolean_sym)) return;
    fail(78);
}
void parser::parameters() {
    parameter();
    while (accept(S::semicolon_sym)) parameter();
}
void parser::parameter() {
    identifier_list();
    take(S::colon_sym);
    if (!accept(S::value_sym) && !accept(S::ref_sym)) fail(79);
    type();
}
void parser::declaration() {
    if (have(S::identifier)) {
        identifier_list();
        take(S::colon_sym);
        accept(S::constant_sym);
        type();
        if (accept(S::becomes_sym)) {
            if (!(accept(S::integer) || accept(S::real_num) || accept(S::strng) ||
                  accept(S::true_sym) || accept(S::false_sym))) fail(79);
        }
        take(S::semicolon_sym);
    } else {
        bool function = have(S::function_sym);
        if (function) take(S::function_sym);
        else take(S::procedure_sym);
        take(S::identifier);
        if (accept(S::left_paren_sym)) {
            parameters();
            take(S::right_paren_sym);
        }
        if (function) { take(S::return_sym); type(); }
        take(S::is_sym);
        block();
        take(S::semicolon_sym);
    }
}
void parser::statement_list() {
    // A statement list is nonempty. The semicolon is mandatory after each statement.
    if (!starts_statement()) fail(80);
    do {
        statement();
        take(S::semicolon_sym);
    } while (starts_statement());
}
void parser::statement() {
    if (have(S::identifier)) {
        take(S::identifier);
        if (accept(S::becomes_sym)) expression();
        else if (accept(S::left_paren_sym)) {
            arguments();
            take(S::right_paren_sym);
        }
    } else if (accept(S::exit_sym)) {
        if (accept(S::when_sym)) expression();
    } else if (accept(S::return_sym)) {
        if (starts_expression()) expression();
    } else if (accept(S::read_sym)) {
        bool parens = accept(S::left_paren_sym);
        identifier_list();
        if (parens) take(S::right_paren_sym);
    } else if (accept(S::write_sym)) {
        bool parens = accept(S::left_paren_sym);
        expression();
        while (accept(S::comma_sym)) expression();
        if (parens) take(S::right_paren_sym);
    } else if (accept(S::writeln_sym)) {
        bool parens = accept(S::left_paren_sym);
        if (starts_expression()) {
            expression();
            while (accept(S::comma_sym)) expression();
        }
        if (parens) take(S::right_paren_sym);
    } else if (accept(S::null_sym)) {
        return;
    } else if (have(S::if_sym)) {
        if_statement();
    } else if (have(S::loop_sym)) {
        loop_statement();
    } else if (accept(S::while_sym)) {
        expression();
        loop_statement();
    } else if (accept(S::for_sym)) {
        take(S::identifier);
        take(S::in_sym);
        accept(S::reverse_sym);
        simple_expression();
        take(S::range_sym);
        simple_expression();
        loop_statement();
    } else fail(80);
}
void parser::if_statement() {
    take(S::if_sym);
    expression();
    take(S::then_sym);
    statement_list();
    while (accept(S::elsif_sym)) {
        expression(); take(S::then_sym); statement_list();
    }
    if (accept(S::else_sym)) statement_list();
    take(S::end_sym);
    take(S::if_sym);
}
void parser::loop_statement() {
    take(S::loop_sym);
    statement_list();
    take(S::end_sym);
    take(S::loop_sym);
}
void parser::arguments() {
    // Empty parentheses are not in the specified production.
    expression();
    while (accept(S::comma_sym)) expression();
}
void parser::expression() {
    simple_expression();
    // FIRST(expression suffix) = relational operators or IN; otherwise epsilon.
    if (have(S::equals_sym) || have(S::not_equals_sym) ||
        have(S::less_than_sym) || have(S::less_or_equal_sym) ||
        have(S::greater_than_sym) || have(S::greater_or_equal_sym)) {
        scan.get_token();
        simple_expression();
    } else if (accept(S::in_sym)) {
        simple_expression();
        take(S::range_sym);
        simple_expression();
    }
}
void parser::simple_expression() {
    expression2();
    while (accept(S::ampersand_sym)) expression2();
}
void parser::expression2() {
    term();
    while (have(S::plus_sym) || have(S::minus_sym) || have(S::or_sym)) {
        scan.get_token(); term();
    }
}
void parser::term() {
    factor();
    while (have(S::asterisk_sym) || have(S::slash_sym) || have(S::and_sym)) {
        scan.get_token(); factor();
    }
}
void parser::factor() {
    // Optional unary +/- or an optional exponentiation after primary.
    if (have(S::plus_sym) || have(S::minus_sym)) {
        scan.get_token(); primary();
    } else {
        primary();
        if (accept(S::power_sym)) primary();
    }
}
void parser::primary() {
    if (accept(S::not_sym) || accept(S::odd_sym)) {
        expression();
    } else if (accept(S::left_paren_sym)) {
        simple_expression();
        take(S::right_paren_sym);
    } else if (accept(S::identifier)) {
        if (accept(S::left_paren_sym)) {
            arguments();
            take(S::right_paren_sym);
        }
    } else if (accept(S::integer) || accept(S::real_num) || accept(S::strng) ||
               accept(S::true_sym) || accept(S::false_sym) || accept(S::eof_sym)) {
        return;
    } else fail(84);
}
