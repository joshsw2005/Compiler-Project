
/* Joshua Wijesinghe | CSC-4510 | LL(1) recursive-descent parser for Lille.
 * FIRST sets choose alternatives; FOLLOW tokens terminate optional/repeated parts.
 * Extended to support static semantic analysis, scope management,
 * identifier checking, and type compatibility.
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
// eof is a predefined boolean operation in the Lille semantics; retain support.
bool parser::starts_expression() {
    return have(S::identifier) || have(S::integer) || have(S::real_num) ||
           have(S::strng) || have(S::true_sym) || have(S::false_sym) ||
           have(S::left_paren_sym) || have(S::not_sym) || have(S::odd_sym) ||
           have(S::plus_sym) || have(S::minus_sym) || have(S::eof_sym);
}

void parser::parse() {
    register_predefined_functions();
    scan.get_token(); // Prime the scanner exactly once.
    take(S::program_sym);
    auto program = current_identifier();
    program.type = T::type_prog;
    declare_identifier(program);
    blocks.push_back({program.name, program.type, T::type_unknown});
    identifiers.enter_scope();
    take(S::identifier);
    take(S::is_sym);
    block();
    identifiers.exit_scope();
    blocks.pop_back();
    take(S::semicolon_sym);
    take(S::end_of_program);
}
void parser::block() {
    // FIRST(declaration) = identifier, procedure, function.
    while (have(S::identifier) || have(S::procedure_sym) || have(S::function_sym))
        declaration();
    take(S::begin_sym);
    statement_list();
    take(S::end_sym);
    if (have(S::identifier)) {
        if (current_identifier().name != blocks.back().name)
            semantic_error("Closing block name mismatch");
        take(S::identifier);
    }
    if (blocks.back().type == T::type_func && !blocks.back().has_return)
        semantic_error("Function requires a return statement");
}

// Collect identifier names and source locations for the symbol table.
vector<identifier_info> parser::identifier_list() {
    vector<identifier_info> names;

    do {
        token* current = scan.this_token();

        identifier_info info;
        info.name = current->get_identifier_value();
        info.line = current->get_line_number();
        info.position = current->get_pos_on_line();

        names.push_back(info);
        take(S::identifier);

    } while (accept(S::comma_sym));

    return names;
}

// Return the declared Lille data type.
parser::T parser::type() {
    if (accept(S::integer_sym)) return T::type_integer;
    if (accept(S::real_sym)) return T::type_real;
    if (accept(S::string_sym)) return T::type_string;
    if (accept(S::boolean_sym)) return T::type_boolean;

    fail(78);
}
void parser::parameters() {
    parameter();
    while (accept(S::semicolon_sym)) parameter();
}
void parser::parameter() {
    auto names = identifier_list();
    take(S::colon_sym);
    lille_kind::lille_knd mode;
    if (accept(S::value_sym)) mode = lille_kind::value_param;
    else if (accept(S::ref_sym)) mode = lille_kind::ref_param;
    else fail(79);
    if (blocks.back().type == T::type_func && mode == lille_kind::ref_param)
        semantic_error("Function parameters must be passed by value");
    T t = type();
    for (auto info : names) {
        info.type = t; info.kind = mode;
        declare_identifier(info);
        pending_parameters.push_back({info.name, t, mode});
    }
}
void parser::declaration() {
    if (have(S::identifier)) {
        auto names = identifier_list();
        take(S::colon_sym);
        bool constant = accept(S::constant_sym);
        T t = type();
        string value;
        if (accept(S::becomes_sym)) {
            if (!constant) semantic_error("Variable initializer not allowed");
            token* tok = scan.this_token();
            T literal;
            if (have(S::integer)) { literal = T::type_integer; value = to_string(tok->get_integer_value()); }
            else if (have(S::real_num)) { literal = T::type_real; value = to_string(tok->get_real_value()); }
            else if (have(S::strng)) { literal = T::type_string; value = tok->get_string_value(); }
            else if (have(S::true_sym) || have(S::false_sym)) { literal = T::type_boolean; value = have(S::true_sym) ? "true" : "false"; }
            else fail(79);
            if (!compatible(t, literal)) semantic_error("Constant type mismatch");
            scan.get_token();
        } else if (constant) semantic_error("Constant requires initializer");
        for (auto info : names) {
            info.type = t;
            info.kind = constant ? lille_kind::constant : lille_kind::variable;
            info.constant_value = value;
            declare_identifier(info);
        }
        take(S::semicolon_sym);
    } else {
        bool function = have(S::function_sym);
        if (function) take(S::function_sym); else take(S::procedure_sym);
        auto routine = current_identifier();
        routine.type = function ? T::type_func : T::type_proc;
        declare_identifier(routine);
        auto* entry = require_identifier(routine.name);
        blocks.push_back({routine.name, routine.type, T::type_unknown});
        identifiers.enter_scope();
        take(S::identifier);
        pending_parameters.clear();
        if (accept(S::left_paren_sym)) { parameters(); take(S::right_paren_sym); }
        entry->parameters = pending_parameters;
        if (function) {
            take(S::return_sym);
            entry->return_type = type();
            blocks.back().return_type = entry->return_type;
        }
        take(S::is_sym);
        block();
        identifiers.exit_scope();
        blocks.pop_back();
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
        auto* info = require_identifier(current_identifier().name);
        take(S::identifier);
        if (accept(S::becomes_sym)) {
            if (!is_modifiable(*info)) semantic_error("Identifier is not assignable");
            if (!compatible(info->type, expression())) semantic_error("Assignment type mismatch");
        } else {
            if (info->type != T::type_proc) semantic_error("Procedure expected");
            check_call(*info);
        }
    } else if (accept(S::exit_sym)) {
        if (!loop_depth) semantic_error("EXIT outside loop");
        if (accept(S::when_sym)) require_boolean(expression());
    } else if (accept(S::return_sym)) {
        if (blocks.back().type == T::type_func) {
            if (!starts_expression() || !compatible(blocks.back().return_type, expression()))
                semantic_error("Invalid function return");
            blocks.back().has_return = true;
        } else if (starts_expression()) semantic_error("Unexpected return value");
    } else if (accept(S::read_sym)) {
        bool parens = accept(S::left_paren_sym);
        for (auto name : identifier_list()) {
            auto* info = require_identifier(name.name);
            if (!is_modifiable(*info) || !is_arithmetic(info->type)) semantic_error("READ requires numeric variable");
        }
        if (parens) take(S::right_paren_sym);
    } else if (have(S::write_sym) || have(S::writeln_sym)) {
        bool newline = have(S::writeln_sym);
        scan.get_token();
        bool parens = accept(S::left_paren_sym);
        if (!newline || starts_expression()) {
            check_output_type(expression());
            while (accept(S::comma_sym)) check_output_type(expression());
        }
        if (parens) take(S::right_paren_sym);
    } else if (accept(S::null_sym)) return;
    else if (have(S::if_sym)) if_statement();
    else if (have(S::loop_sym)) loop_statement();
    else if (accept(S::while_sym)) { require_boolean(expression()); loop_statement(); }
    else if (accept(S::for_sym)) {
        auto control = current_identifier();
        control.type = T::type_integer; control.kind = lille_kind::for_ident;
        take(S::identifier); take(S::in_sym); accept(S::reverse_sym);
        if (simple_expression() != T::type_integer) semantic_error("Integer bound required");
        take(S::range_sym);
        if (simple_expression() != T::type_integer) semantic_error("Integer bound required");
        identifiers.enter_scope(); declare_identifier(control);
        loop_statement(); identifiers.exit_scope();
    } else fail(80);
}
void parser::if_statement() {
    take(S::if_sym);
    require_boolean(expression());
    take(S::then_sym);
    statement_list();
    while (accept(S::elsif_sym)) {
        require_boolean(expression()); take(S::then_sym); statement_list();
    }
    if (accept(S::else_sym)) statement_list();
    take(S::end_sym);
    take(S::if_sym);
}
void parser::loop_statement() {
    ++loop_depth;
    take(S::loop_sym);
    statement_list();
    take(S::end_sym);
    take(S::loop_sym);
    --loop_depth;
}
// The grammar requires at least one argument if call parentheses are present.
void parser::arguments() {
    // Empty parentheses are not in the specified production.
    expression();
    while (accept(S::comma_sym)) expression();
}
parser::T parser::expression() {
    T left = simple_expression();
    if (have(S::equals_sym) || have(S::not_equals_sym) || have(S::less_than_sym) ||
        have(S::less_or_equal_sym) || have(S::greater_than_sym) || have(S::greater_or_equal_sym)) {
        scan.get_token(); T right = simple_expression();
        if (!(is_arithmetic(left) && is_arithmetic(right)) && !(left == T::type_boolean && right == T::type_boolean))
            semantic_error("Invalid comparison");
        return T::type_boolean;
    } else if (accept(S::in_sym)) {
        T lower = simple_expression(); take(S::range_sym); T upper = simple_expression();
        if (left != T::type_integer || lower != T::type_integer || upper != T::type_integer) semantic_error("Integer range required");
        return T::type_boolean;
    }
    return left;
}
parser::T parser::simple_expression() {
    T left = expression2();
    while (accept(S::ampersand_sym)) {
        T right = expression2();
        if (left != T::type_string || right != T::type_string) semantic_error("String operands required");
    }
    return left;
}
parser::T parser::expression2() {
    T left = term();
    while (have(S::plus_sym) || have(S::minus_sym) || have(S::or_sym)) {
        S op = scan.this_token()->get_sym(); scan.get_token();
        T right = term();
        if (op == S::or_sym) { require_boolean(left); require_boolean(right); }
        else left = arithmetic_result(left, right);
    }
    return left;
}
parser::T parser::term() {
    T left = factor();
    while (have(S::asterisk_sym) || have(S::slash_sym) || have(S::and_sym)) {
        S op = scan.this_token()->get_sym(); scan.get_token();
        T right = factor();
        if (op == S::and_sym) { require_boolean(left); require_boolean(right); }
        else left = arithmetic_result(left, right);
    }
    return left;
}
parser::T parser::factor() {
    if (have(S::plus_sym) || have(S::minus_sym)) {
        scan.get_token(); T t = primary();
        if (!is_arithmetic(t)) semantic_error("Numeric operand required");
        return t;
    }
    T t = primary();
    if (accept(S::power_sym)) {
        T exponent = primary();
        if (!is_arithmetic(t) || exponent != T::type_integer) semantic_error("Invalid exponentiation");
    }
    return t;
}
parser::T parser::primary() {
    if (accept(S::not_sym)) { require_boolean(expression()); return T::type_boolean; }
    if (accept(S::odd_sym)) {
        if (expression() != T::type_integer) semantic_error("ODD requires integer");
        return T::type_boolean;
    }
    if (accept(S::left_paren_sym)) { T t = simple_expression(); take(S::right_paren_sym); return t; }
    if (have(S::identifier)) {
        auto* info = require_identifier(current_identifier().name);
        take(S::identifier);
        if (info->type == T::type_func) { check_call(*info); return info->return_type; }
        if (info->type == T::type_proc || info->type == T::type_prog || have(S::left_paren_sym)) semantic_error("Invalid expression identifier");
        return info->type;
    }
    if (accept(S::integer)) return T::type_integer;
    if (accept(S::real_num)) return T::type_real;
    if (accept(S::strng)) return T::type_string;
    if (accept(S::true_sym) || accept(S::false_sym) || accept(S::eof_sym)) return T::type_boolean;
    fail(84);
}

// Semantic analysis helper functions.

// Determine whether a type is arithmetic.
bool parser::is_arithmetic(T type) {
    return type == T::type_integer ||
           type == T::type_real;
}

// Allow matching types and integer-to-real conversion.
bool parser::compatible(T expected, T actual) {
    if (expected == T::type_arith) return is_arithmetic(actual);
    return expected == actual ||
           (expected == T::type_real &&
            actual == T::type_integer);
}

// Constants and for-loop control variables cannot be modified.
bool parser::is_modifiable(const identifier_info& info) {
    if (info.kind == lille_kind::value_param && info.level > 0 &&
        static_cast<size_t>(info.level) <= blocks.size() && blocks[info.level-1].type == T::type_func) return false;
    return info.kind == lille_kind::variable ||
           info.kind == lille_kind::value_param ||
           info.kind == lille_kind::ref_param;
}

// Insert an identifier and check for duplicate declarations.
void parser::declare_identifier(const identifier_info& info) {
    identifier_info entry = info;
    entry.level = identifiers.get_current_scope();
    if (!identifiers.insert(entry)) {
        semantic_error("Duplicate declaration of " + info.name);
    }
}

// Search the active scopes for a declared identifier.
identifier_info* parser::require_identifier(const string& name) {
    identifier_info* result = identifiers.lookup(name);

    if (result == nullptr) {
        semantic_error("Undeclared identifier: " + name);
    }

    return result;
}

// Report the first semantic error and stop compilation.
[[noreturn]] void parser::semantic_error(const string& message) {
    token* current = scan.this_token();

    cerr << "Semantic error: " << message << endl;
    cerr << "Line: " << current->get_line_number()
         << ", Position: " << current->get_pos_on_line()
         << endl;

    throw std::runtime_error("Semantic analysis stopped");
}

identifier_info parser::current_identifier() {
    if (!have(S::identifier)) fail(79);
    token* tok = scan.this_token();
    identifier_info info;
    info.name = tok->get_identifier_value();
    info.line = tok->get_line_number(); info.position = tok->get_pos_on_line();
    return info;
}
void parser::require_boolean(T t) {
    if (t != T::type_boolean) semantic_error("Boolean expression required");
}
parser::T parser::arithmetic_result(T a, T b) {
    if (!is_arithmetic(a) || !is_arithmetic(b)) semantic_error("Numeric operands required");
    return a == T::type_real || b == T::type_real ? T::type_real : T::type_integer;
}
void parser::check_output_type(T t) {
    if (!is_arithmetic(t) && t != T::type_string) semantic_error("Invalid output type");
}
void parser::check_call(const identifier_info& info) {
    size_t i = 0;
    if (accept(S::left_paren_sym)) {
        do {
            if (i >= info.parameters.size()) semantic_error("Too many arguments");
            const auto& formal = info.parameters[i++];
            if (formal.mode == lille_kind::ref_param) {
                auto* actual = require_identifier(current_identifier().name);
                if (!is_modifiable(*actual) || actual->type != formal.type) semantic_error("Invalid reference argument");
                take(S::identifier);
            } else if (!compatible(formal.type, expression())) semantic_error("Argument type mismatch");
        } while (accept(S::comma_sym));
        take(S::right_paren_sym);
    }
    if (i != info.parameters.size()) semantic_error("Wrong argument count");
}
void parser::register_predefined_functions() {
    const string names[] = {"INT2REAL", "REAL2INT", "INT2STRING", "REAL2STRING"};
    const T types[] = {T::type_real, T::type_integer, T::type_string, T::type_string};
    for (int i = 0; i < 4; ++i) {
        identifier_info info;
        info.name = names[i]; info.type = T::type_func; info.return_type = types[i];
        info.parameters.push_back({"VALUE", T::type_arith, lille_kind::value_param});
        declare_identifier(info);
    }
}
