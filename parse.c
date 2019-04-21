#include "cc.h"

static Vector *literals; // Vector<String*>

// symbol table and scopes

static Vector *symbol_scopes; // Vector<Map<SymbolType*>*>

static void put_symbol(char *identifier, Symbol *symbol) {
  if (!identifier) return;

  Map *map = symbol_scopes->buffer[symbol_scopes->length - 1];

  symbol->prev = map_lookup(map, identifier);
  if (symbol->prev && symbol->prev->sy_type != symbol->sy_type) {
    ERROR(symbol->token, "invalid redeclaration: %s.", identifier);
  }

  map_put(map, identifier, symbol);
}

static Symbol *lookup_symbol(char *identifier) {
  if (!identifier) return NULL;

  for (int i = symbol_scopes->length - 1; i >= 0; i--) {
    Map *map = symbol_scopes->buffer[i];
    Symbol *symbol = map_lookup(map, identifier);
    if (symbol) return symbol;
  }

  return NULL;
}

// tokens

static Token **tokens;
static int pos;

static Token *peek(void) {
  return tokens[pos];
}

static Token *check(TokenType tk_type) {
  if (tokens[pos]->tk_type == tk_type) {
    return tokens[pos];
  }
  return NULL;
}

static Token *read(TokenType tk_type) {
  if (tokens[pos]->tk_type == tk_type) {
    return tokens[pos++];
  }
  return NULL;
}

static Token *expect(TokenType tk_type) {
  if (tokens[pos]->tk_type == tk_type) {
    return tokens[pos++];
  }

  if (isprint(tk_type)) {
    ERROR(tokens[pos], "'%c' is expected.", tk_type);
  }
  ERROR(tokens[pos], "unexpected token.");
}

// check declaration specifiers

static bool check_storage_class_specifier(void) {
  if (check(TK_TYPEDEF)) return true;
  if (check(TK_EXTERN)) return true;
  if (check(TK_STATIC)) return true;

  return false;
}

static bool check_typedef_name(void) {
  if (check(TK_IDENTIFIER)) {
    Symbol *symbol = lookup_symbol(peek()->identifier);
    return symbol && symbol->sy_type == SY_TYPE;
  }

  return false;
}

static bool check_type_specifier(void) {
  if (check(TK_VOID)) return true;
  if (check(TK_CHAR)) return true;
  if (check(TK_SHORT)) return true;
  if (check(TK_INT)) return true;
  if (check(TK_LONG)) return true;
  if (check(TK_SIGNED)) return true;
  if (check(TK_UNSIGNED)) return true;
  if (check(TK_BOOL)) return true;
  if (check(TK_STRUCT)) return true;
  if (check(TK_ENUM)) return true;
  if (check_typedef_name()) return true;

  return false;
}

static bool check_function_specifier(void) {
  if (check(TK_NORETURN)) return true;

  return false;
}

static bool check_declaration_specifier(void) {
  if (check_storage_class_specifier()) return true;
  if (check_type_specifier()) return true;
  if (check_function_specifier()) return true;

  return false;
}

// parse expression

static Expr *expr_new(NodeType nd_type, Token *token) {
  Expr *expr = calloc(1, sizeof(Expr));
  expr->nd_type = nd_type;
  expr->token = token;
  return expr;
}

static Expr *expr_unary(NodeType nd_type, Expr *_expr, Token *token) {
  Expr *expr = calloc(1, sizeof(Expr));
  expr->nd_type = nd_type;
  expr->expr = _expr;
  expr->token = token;
  return expr;
}

static Expr *expr_binary(NodeType nd_type, Expr *lhs, Expr *rhs, Token *token) {
  Expr *expr = calloc(1, sizeof(Expr));
  expr->nd_type = nd_type;
  expr->lhs = lhs;
  expr->rhs = rhs;
  expr->token = token;
  return expr;
}

static Expr *cast_expression(void);
static Expr *assignment_expression(void);
static Expr *expression(void);
static TypeName *type_name(void);

// primary-expression :
//   identifier
//   integer-constant
//   string-literal
//   '(' expression ')'
static Expr *primary_expression(void) {
  Token *token = peek();

  if (!check_typedef_name() && read(TK_IDENTIFIER)) {
    // check builtin macros
    if (strcmp(token->identifier, "__builtin_va_start") == 0 && read('(')) {
      Expr *macro_ap = assignment_expression();
      expect(',');
      char *macro_arg = expect(TK_IDENTIFIER)->identifier;
      expect(')');

      Expr *expr = expr_new(ND_VA_START, token);
      expr->macro_ap = macro_ap;
      expr->macro_arg = macro_arg;
      return expr;
    }
    if (strcmp(token->identifier, "__builtin_va_arg") == 0 && read('(')) {
      Expr *macro_ap = assignment_expression();
      expect(',');
      TypeName *macro_type = type_name();
      expect(')');

      Expr *expr = expr_new(ND_VA_ARG, token);
      expr->macro_ap = macro_ap;
      expr->macro_type = macro_type;
      return expr;
    }
    if (strcmp(token->identifier, "__builtin_va_end") == 0 && read('(')) {
      Expr *macro_ap = assignment_expression();
      expect(')');

      Expr *expr = expr_new(ND_VA_END, token);
      expr->macro_ap = macro_ap;
      return expr;
    }

    Symbol *symbol = lookup_symbol(token->identifier);
    if (symbol && symbol->sy_type == SY_CONST) {
      Expr *expr = expr_new(ND_ENUM_CONST, token);
      expr->identifier = token->identifier;
      expr->symbol = symbol;
      return expr;
    } else {
      Expr *expr = expr_new(ND_IDENTIFIER, token);
      expr->identifier = token->identifier;
      expr->symbol = symbol;
      return expr;
    }
  }

  if (read(TK_INTEGER_CONST)) {
    Expr *expr = expr_new(ND_INTEGER, token);
    expr->int_value = token->int_value;
    expr->int_decimal = token->int_decimal;
    expr->int_u = token->int_u;
    expr->int_l = token->int_l;
    expr->int_ll = token->int_ll;
    return expr;
  }

  if (read(TK_CHAR_CONST)) {
    Expr *expr = expr_new(ND_INTEGER, token);
    expr->int_value = token->char_value;
    return expr;
  }

  if (read(TK_STRING_LITERAL)) {
    int string_label = literals->length;
    vector_push(literals, token->string_literal);

    Expr *expr = expr_new(ND_STRING, token);
    expr->string_literal = token->string_literal;
    expr->string_label = string_label;
    return expr;
  }

  if (read('(')) {
    Expr *expr = expression();
    expect(')');
    return expr;
  }

  ERROR(token, "invalid primary expression.");
}

// postfix-expression :
//   primary-expression
//   postfix-expression '[' expression ']'
//   postfix-expression '(' (assignment-expression (',' assignment-expression)*)? ')'
//   postfix-expression '.' identifier
//   postfix-expression '->' identifier
//   postfix-expression '++'
//   postfix-expression '--'
static Expr *postfix_expression(Expr *expr) {
  if (!expr) {
    expr = primary_expression();
  }

  while (1) {
    Token *token = peek();
    if (read('[')) {
      Expr *index = expression();
      expect(']');

      expr = expr_unary(ND_SUBSCRIPTION, expr, token);
      expr->index = index;
    } else if (read('(')) {
      Vector *args = vector_new();
      if (!check(')')) {
        do {
          vector_push(args, assignment_expression());
        } while (read(','));
      }
      expect(')');

      expr = expr_unary(ND_CALL, expr, token);
      expr->args = args;
    } else if (read('.')) {
      char *member = expect(TK_IDENTIFIER)->identifier;

      expr = expr_unary(ND_DOT, expr, token);
      expr->member = member;
    } else if (read(TK_ARROW)) {
      char *member = expect(TK_IDENTIFIER)->identifier;

      expr = expr_unary(ND_ARROW, expr, token);
      expr->member = member;
    } else if (read(TK_INC)) {
      expr = expr_unary(ND_POST_INC, expr, token);
    } else if (read(TK_DEC)) {
      expr = expr_unary(ND_POST_DEC, expr, token);
    } else {
      break;
    }
  }

  return expr;
}

// unary-expression :
//   postfix-expression
//   '++' unary-expression
//   '--' unary-expression
//   unary-operator cast-expression
//   cast-expression
//   'sizeof' unary-expression
//   'sizeof' '(' type-name ')'
//   '_Alignof' '(' type-name ')'
// unary-operator :
//   '&' | '*' | '+' | '-' | '~' | '!'
static Expr *unary_expression(void) {
  Token *token = peek();

  if (read(TK_INC))
    return expr_unary(ND_PRE_INC, unary_expression(), token);
  if (read(TK_DEC))
    return expr_unary(ND_PRE_DEC, unary_expression(), token);

  if (read('&'))
    return expr_unary(ND_ADDRESS, unary_expression(), token);
  if (read('*'))
    return expr_unary(ND_INDIRECT, unary_expression(), token);
  if (read('+'))
    return expr_unary(ND_UPLUS, unary_expression(), token);
  if (read('-'))
    return expr_unary(ND_UMINUS, unary_expression(), token);
  if (read('~'))
    return expr_unary(ND_NOT, unary_expression(), token);
  if (read('!'))
    return expr_unary(ND_LNOT, unary_expression(), token);

  if (read(TK_SIZEOF)) {
    Expr *expr = expr_new(ND_SIZEOF, token);

    // If '(' follows 'sizeof', it is 'sizeof' type-name
    // or 'sizeof' '(' expression ')'.
    // Otherwise, it is 'sizeof' unary-expression.
    if (read('(')) {
      if (check_type_specifier()) {
        expr->type_name = type_name();
      } else {
        expr->expr = expression();
      }
      expect(')');
    } else {
      expr->expr = unary_expression();
    }

    return expr;
  }

  if (read(TK_ALIGNOF)) {
    Expr *expr = expr_new(ND_ALIGNOF, token);
    expect('(');
    expr->type_name = type_name();
    expect(')');
    return expr;
  }

  return postfix_expression(NULL);
}

// cast-expression :
//   unary-expression
//   '(' type-name ')' cast-expression
static Expr *cast_expression(void) {
  Token *token = peek();

  // If type-specifier follows '(', it is cast-expression.
  // Otherwise, it is postfix-expression like '(' expression ')' '++'.
  if (read('(')) {
    if (check_type_specifier()) {
      Expr *expr = expr_new(ND_CAST, token);
      expr->type_name = type_name();
      expect(')');
      expr->expr = cast_expression();
      return expr;
    }

    Expr *expr = expression();
    expect(')');
    return postfix_expression(expr);
  }

  return unary_expression();
}

// multiplicative-expression :
//   cast-expression (('*' | '/' | '%') cast-expression)*
static Expr *multiplicative_expression(Expr *expr) {
  if (!expr) {
    expr = cast_expression();
  }

  while (1) {
    Token *token = peek();
    if (read('*')) {
      expr = expr_binary(ND_MUL, expr, cast_expression(), token);
    } else if (read('/')) {
      expr = expr_binary(ND_DIV, expr, cast_expression(), token);
    } else if (read('%')) {
      expr = expr_binary(ND_MOD, expr, cast_expression(), token);
    } else {
      break;
    }
  }

  return expr;
}

// additive-expression :
//   multiplicative-expression (('+' | '-') multiplicative-expression)*
static Expr *additive_expression(Expr *expr) {
  expr = multiplicative_expression(expr);

  while (1) {
    Token *token = peek();
    if (read('+')) {
      expr = expr_binary(ND_ADD, expr, multiplicative_expression(NULL), token);
    } else if (read('-')) {
      expr = expr_binary(ND_SUB, expr, multiplicative_expression(NULL), token);
    } else {
      break;
    }
  }

  return expr;
}

// shift-expression :
//   additive-expression (('<<' | '>>') additive-expression)*
static Expr *shift_expression(Expr *expr) {
  expr = additive_expression(expr);

  while (1) {
    Token *token = peek();
    if (read(TK_LSHIFT)) {
      expr = expr_binary(ND_LSHIFT, expr, additive_expression(NULL), token);
    } else if (read(TK_RSHIFT)) {
      expr = expr_binary(ND_RSHIFT, expr, additive_expression(NULL), token);
    } else {
      break;
    }
  }

  return expr;
}

// relational-expression :
//   shift-expression (('<' | '>' | '<=' | '>=') shift-expression)*
static Expr *relational_expression(Expr *expr) {
  expr = shift_expression(expr);

  while (1) {
    Token *token = peek();
    if (read('<')) {
      expr = expr_binary(ND_LT, expr, shift_expression(NULL), token);
    } else if (read('>')) {
      expr = expr_binary(ND_GT, expr, shift_expression(NULL), token);
    } else if (read(TK_LTE)) {
      expr = expr_binary(ND_LTE, expr, shift_expression(NULL), token);
    } else if (read(TK_GTE)) {
      expr = expr_binary(ND_GTE, expr, shift_expression(NULL), token);
    } else {
      break;
    }
  }

  return expr;
}

// equality-expression :
//   relational-expression (('=' | '!=') equality-expression)*
static Expr *equality_expression(Expr *expr) {
  expr = relational_expression(expr);

  while (1) {
    Token *token = peek();
    if (read(TK_EQ)) {
      expr = expr_binary(ND_EQ, expr, relational_expression(NULL), token);
    } else if (read(TK_NEQ)) {
      expr = expr_binary(ND_NEQ, expr, relational_expression(NULL), token);
    } else {
      break;
    }
  }

  return expr;
}

// and-expression :
//   equality-expression ('&' equality-expression)*
static Expr *and_expression(Expr *expr) {
  expr = equality_expression(expr);

  while (1) {
    Token *token = peek();
    if (read('&')) {
      expr = expr_binary(ND_AND, expr, equality_expression(NULL), token);
    } else {
      break;
    }
  }

  return expr;
}

// xor-expression :
//   and-expression ('^' and-expression)*
static Expr *xor_expression(Expr *expr) {
  expr = and_expression(expr);

  while (1) {
    Token *token = peek();
    if (read('^')) {
      expr = expr_binary(ND_XOR, expr, and_expression(NULL), token);
    } else {
      break;
    }
  }

  return expr;
}

// or-expression :
//   xor-expression ('^' xor-expression)*
static Expr *or_expression(Expr *expr) {
  expr = xor_expression(expr);

  while (1) {
    Token *token = peek();
    if (read('|')) {
      expr = expr_binary(ND_OR, expr, xor_expression(NULL), token);
    } else {
      break;
    }
  }

  return expr;
}

// logical-and-expression :
//   or-expression ('&&' or-expression)*
static Expr *logical_and_expression(Expr *expr) {
  expr = or_expression(expr);

  while (1) {
    Token *token = peek();
    if (read(TK_AND)) {
      expr = expr_binary(ND_LAND, expr, or_expression(NULL), token);
    } else {
      break;
    }
  }

  return expr;
}

// logical-or-expression :
//   logical-and-expression ('||' logical-and-expression)*
static Expr *logical_or_expression(Expr *expr) {
  expr = logical_and_expression(expr);

  while (1) {
    Token *token = peek();
    if (read(TK_OR)) {
      expr = expr_binary(ND_LOR, expr, logical_and_expression(NULL), token);
    } else {
      break;
    }
  }

  return expr;
}

// conditional-expression :
//   logical-or-expression
//   logical-or-expression '?' expression ':' conditional-expression
static Expr *conditional_expression(Expr *cond) {
  cond = logical_or_expression(cond);

  Token *token = peek();
  if (read('?')) {
    Expr *lhs = expression();
    expect(':');
    Expr *rhs = conditional_expression(NULL);

    Expr *expr = expr_new(ND_CONDITION, token);
    expr->cond = cond;
    expr->lhs = lhs;
    expr->rhs = rhs;
    return expr;
  }

  return cond;
}

// assignment-expression :
//   conditional-expression
//   unary-expression assignment-operator assignment-expression
// assignment-operator :
//   '=' | '*=' | '/=' | '%=' | '+=' | '-='
static Expr *assignment_expression(void) {
  Expr *expr = cast_expression();

  Token *token = peek();
  if (read('='))
    return expr_binary(ND_ASSIGN, expr, assignment_expression(), token);
  if (read(TK_MUL_ASSIGN))
    return expr_binary(ND_MUL_ASSIGN, expr, assignment_expression(), token);
  if (read(TK_DIV_ASSIGN))
    return expr_binary(ND_DIV_ASSIGN, expr, assignment_expression(), token);
  if (read(TK_MOD_ASSIGN))
    return expr_binary(ND_MOD_ASSIGN, expr, assignment_expression(), token);
  if (read(TK_ADD_ASSIGN))
    return expr_binary(ND_ADD_ASSIGN, expr, assignment_expression(), token);
  if (read(TK_SUB_ASSIGN))
    return expr_binary(ND_SUB_ASSIGN, expr, assignment_expression(), token);

  return conditional_expression(expr);
}

// expression :
//   assignment-expression (',' assignment-expression)*
static Expr *expression(void) {
  Expr *expr = assignment_expression();

  while (1) {
    Token *token = peek();
    if (read(',')) {
      expr = expr_binary(ND_COMMA, expr, assignment_expression(), token);
    } else {
      break;
    }
  }

  return expr;
}

// constant-expression :
//   conditional-expression
static Expr *constant_expression(void) {
  return conditional_expression(NULL);
}

// parse declaration

static Decl *decl_new(Vector *specs, Vector *symbols, Token *token) {
  Decl *decl = calloc(1, sizeof(Decl));
  decl->nd_type = ND_DECL;
  decl->specs = specs;
  decl->symbols = symbols;
  decl->token = token;
  return decl;
}

static Decl *param_new(Vector *specs, Symbol *symbol, Token *token) {
  Decl *decl = calloc(1, sizeof(Decl));
  decl->nd_type = ND_DECL;
  decl->specs = specs;
  decl->symbol = symbol;
  decl->token = token;
  return decl;
}

static Specifier *specifier_new(SpecifierType sp_type, Token *token) {
  Specifier *spec = calloc(1, sizeof(Specifier));
  spec->sp_type = sp_type;
  spec->token = token;
  return spec;
}

static Declarator *declarator_new(DeclaratorType decl_type, Declarator *_decl, Token *token) {
  Declarator *decl = calloc(1, sizeof(Declarator));
  decl->decl_type = decl_type;
  decl->decl = _decl;
  decl->token = token;
  return decl;
}

static bool check_typedef(Vector *specs) {
  bool sp_typedef = false;
  for (int i = 0; i < specs->length; i++) {
    Specifier *spec = specs->buffer[i];
    if (spec->sp_type == SP_TYPEDEF) {
      if (sp_typedef) {
        ERROR(spec->token, "duplicated typedef.");
      }
      sp_typedef = true;
    }
  }

  return sp_typedef;
}

static Vector *declaration_specifiers(void);
static Vector *specifier_qualifier_list(void);
static Specifier *storage_class_specifier(void);
static Specifier *type_specifier(void);
static Specifier *struct_or_union_specifier(void);
static Decl *struct_declaration(void);
static Specifier *enum_specifier(void);
static Symbol *enumerator(void);
static Specifier *typedef_name(void);
static Specifier *function_specifier(void);
static Symbol *init_declarator(bool sp_typedef);
static Symbol *declarator(bool sp_typedef);
static Symbol *direct_declarator(bool sp_typedef);
static Decl *parameter_declaration(void);
static Symbol *param_declarator(void);
static Symbol *param_direct_declarator(void);
static TypeName *type_name(void);
static Symbol *abstract_declarator(void);
static Symbol *direct_abstract_declarator(void);
static Initializer *initializer(void);

// declaration :
//   declaration-specifiers (init-declarator)* ';'
static Decl *declaration(void) {
  Token *token = peek();

  Vector *specs = declaration_specifiers();
  bool sp_typedef = check_typedef(specs);

  Vector *symbols = vector_new();
  if (!check(';')) {
    do {
      Symbol *symbol = init_declarator(sp_typedef);
      vector_push(symbols, symbol);
      put_symbol(symbol->identifier, symbol);
    } while (read(','));
  }
  expect(';');

  return decl_new(specs, symbols, token);
}

// declaration-specifiers :
//   (storage-class-specifier | type-specifier | function-specifier)*
static Vector *declaration_specifiers(void) {
  Vector *specs = vector_new();
  while (1) {
    if (check_storage_class_specifier()) {
      vector_push(specs, storage_class_specifier());
    } else if (check_type_specifier()) {
      vector_push(specs, type_specifier());
    } else if (check_function_specifier()) {
      vector_push(specs, function_specifier());
    } else {
      break;
    }
  }

  return specs;
}

// specifier-qualifier-list :
//   (type-specifier | type-qualifier)*
static Vector *specifier_qualifier_list(void) {
  Vector *specs = vector_new();
  while (1) {
    if (check_storage_class_specifier()) {
      vector_push(specs, storage_class_specifier());
    } else if (check_type_specifier()) {
      vector_push(specs, type_specifier());
    } else {
      break;
    }
  }

  return specs;
}

// storage-class-specifier :
//   'typedef'
//   'extern'
static Specifier *storage_class_specifier(void) {
  Token *token = peek();

  if (read(TK_TYPEDEF))
    return specifier_new(SP_TYPEDEF, token);
  if (read(TK_EXTERN))
    return specifier_new(SP_EXTERN, token);
  if (read(TK_STATIC))
    return specifier_new(SP_STATIC, token);

  // unreachable
  internal_error("invalid storage-class-specifier.");
}

// type-specifier :
//   'void'
//   'char'
//   'short'
//   'int'
//   'unsigned'
//   '_Bool'
//   struct-or-union-specifier
//   enum-specifier
//   typedef-name
static Specifier *type_specifier(void) {
  Token *token = peek();

  if (read(TK_VOID))
    return specifier_new(SP_VOID, token);
  if (read(TK_CHAR))
    return specifier_new(SP_CHAR, token);
  if (read(TK_SHORT))
    return specifier_new(SP_SHORT, token);
  if (read(TK_INT))
    return specifier_new(SP_INT, token);
  if (read(TK_LONG))
    return specifier_new(SP_LONG, token);
  if (read(TK_SIGNED))
    return specifier_new(SP_SIGNED, token);
  if (read(TK_UNSIGNED))
    return specifier_new(SP_UNSIGNED, token);
  if (read(TK_BOOL))
    return specifier_new(SP_BOOL, token);

  if (check(TK_STRUCT))
    return struct_or_union_specifier();
  if (check(TK_ENUM))
    return enum_specifier();

  if (check_typedef_name())
    return typedef_name();

  // unreachable
  internal_error("invalid type-specifier.");
}

// struct-or-union-specifier :
//   'struct' identifier? '{' struct-declaration (',' struct-declaration)* '}'
//   'struct' identifier
static Specifier *struct_or_union_specifier(void) {
  Token *token = expect(TK_STRUCT);

  if (check(TK_IDENTIFIER)) {
    char *tag = expect(TK_IDENTIFIER)->identifier;

    if (read('{')) {
      Vector *decls = vector_new();
      do {
        vector_push(decls, struct_declaration());
      } while (!check('}') && !check(TK_EOF));
      expect('}');

      Specifier *spec = specifier_new(SP_STRUCT, token);
      spec->struct_tag = tag;
      spec->struct_decls = decls;
      return spec;
    }

    Specifier *spec = specifier_new(SP_STRUCT, token);
    spec->struct_tag = tag;
    spec->struct_decls = NULL;
    return spec;
  }

  Vector *decls = vector_new();
  expect('{');
  do {
    vector_push(decls, struct_declaration());
  } while (!check('}') && !check(TK_EOF));
  expect('}');

  Specifier *spec = specifier_new(SP_STRUCT, token);
  spec->struct_tag = NULL;
  spec->struct_decls = decls;
  return spec;
}

// struct-declaration :
//   specifier-qualifier-list (declarator (',' declarator)*)? ';'
static Decl *struct_declaration(void) {
  Token *token = peek();

  Vector *specs = specifier_qualifier_list();
  Vector *symbols = vector_new();
  if (!check(';')) {
    do {
      vector_push(symbols, declarator(false));
    } while (read(','));
  }
  expect(';');

  return decl_new(specs, symbols, token);
}

// enum-specifier :
//   'enum' identifier? '{' enumerator (',' enumerator)* '}'
//   'enum' identifier? '{' enumerator (',' enumerator)* ',' '}'
//   'enum' identifier
static Specifier *enum_specifier(void) {
  Token *token = expect(TK_ENUM);

  if (check(TK_IDENTIFIER)) {
    char *tag = expect(TK_IDENTIFIER)->identifier;

    if (read('{')) {
      Vector *enums = vector_new();
      do {
        Symbol *symbol = enumerator();
        vector_push(enums, symbol);
        put_symbol(symbol->identifier, symbol);
      } while (read(',') && !check('}'));
      expect('}');

      Specifier *spec = specifier_new(SP_ENUM, token);
      spec->enum_tag = tag;
      spec->enums = enums;
      return spec;
    }

    Specifier *spec = specifier_new(SP_ENUM, token);
    spec->enum_tag = tag;
    spec->enums = NULL;
    return spec;
  }

  Vector *enums = vector_new();
  expect('{');
  do {
    Symbol *symbol = enumerator();
    vector_push(enums, symbol);
    put_symbol(symbol->identifier, symbol);
  } while (read(',') && !check('}'));
  expect('}');

  Specifier *spec = specifier_new(SP_ENUM, token);
  spec->enum_tag = NULL;
  spec->enums = enums;
  return spec;
}

// enumerator :
//   enumeration-constant
//   enumeration-constant '=' constant-expression
static Symbol *enumerator(void) {
  Token *token = expect(TK_IDENTIFIER);
  Expr *const_expr = read('=') ? constant_expression() : NULL;

  Symbol *symbol = calloc(1, sizeof(Symbol));
  symbol->sy_type = SY_CONST;
  symbol->identifier = token->identifier;
  symbol->const_expr = const_expr;
  symbol->token = token;
  return symbol;
}

// typedef-name :
//   identifier
static Specifier *typedef_name(void) {
  Token *token = expect(TK_IDENTIFIER);
  Symbol *symbol = lookup_symbol(token->identifier);

  Specifier *spec = specifier_new(SP_TYPEDEF_NAME, token);
  spec->typedef_name = token->identifier;
  spec->typedef_symbol = symbol;
  return spec;
}

// function-specifier :
//   '_Noreturn'
static Specifier *function_specifier(void) {
  Token *token = peek();

  if (read(TK_NORETURN))
    return specifier_new(SP_NORETURN, token);

  // unreachable
  internal_error("unknown function-specifier.");
}

// init-declarator :
//   declarator
//   declarator '=' initializer
static Symbol *init_declarator(bool sp_typedef) {
  Symbol *symbol = declarator(sp_typedef);
  symbol->init = read('=') ? initializer() : NULL;

  return symbol;
}

// declarator :
//   '*'* direct-declarator
static Symbol *declarator(bool sp_typedef) {
  Token *token = peek();

  if (read('*')) {
    Symbol *symbol = declarator(sp_typedef);
    symbol->decl = declarator_new(DECL_POINTER, symbol->decl, token);

    return symbol;
  }

  return direct_declarator(sp_typedef);
}

// direct-declarator :
//   identifier
//   direct-declarator '[' assignment-expression? ']'
//   direct-declarator '(' (parameter-declaration (',' parameter-declaration)* (',' ...)?)? ')'
static Symbol *direct_declarator(bool sp_typedef) {
  Token *token = expect(TK_IDENTIFIER);

  Symbol *symbol = calloc(1, sizeof(Symbol));
  symbol->sy_type = sp_typedef ? SY_TYPE : SY_VARIABLE;
  symbol->identifier = token->identifier;
  symbol->decl = NULL;
  symbol->token = token;

  while (1) {
    Token *token = peek();

    if (read('[')) {
      Expr *size = !check(']') ? assignment_expression() : NULL;
      expect(']');

      symbol->decl = declarator_new(DECL_ARRAY, symbol->decl, token);
      symbol->decl->size = size;
    } else if (read('(')) {
      Map *proto_scope = map_new(); // begin prototype scope
      vector_push(symbol_scopes, proto_scope);

      Vector *params = vector_new();
      bool ellipsis = false;
      if (!check(')')) {
        vector_push(params, parameter_declaration());
        while (read(',')) {
          if (read(TK_ELLIPSIS)) {
            ellipsis = true;
            break;
          }
          vector_push(params, parameter_declaration());
        }
      }
      expect(')');

      vector_pop(symbol_scopes); // end prototype scope

      symbol->decl = declarator_new(DECL_FUNCTION, symbol->decl, token);
      symbol->decl->params = params;
      symbol->decl->ellipsis = ellipsis;
      symbol->decl->proto_scope = proto_scope;
    } else {
      break;
    }
  }

  return symbol;
}

// parameter-declaration :
//   specifier-qualifier-list declarator
//   specifier-qualifier abstract-declarator?
static Decl *parameter_declaration(void) {
  Token *token = peek();

  Vector *specs = specifier_qualifier_list();
  Symbol *symbol = param_declarator();

  put_symbol(symbol->identifier, symbol);

  return param_new(specs, symbol, token);
}

static Symbol *param_declarator(void) {
  Token *token = peek();

  if (read('*')) {
    Symbol *symbol = param_declarator();
    symbol->decl = declarator_new(DECL_POINTER, symbol->decl, token);

    return symbol;
  }

  return param_direct_declarator();
}

static Symbol *param_direct_declarator(void) {
  Token *token = peek();
  Token *ident = read(TK_IDENTIFIER);

  Symbol *symbol = calloc(1, sizeof(Symbol));
  symbol->sy_type = SY_VARIABLE;
  symbol->identifier = ident ? ident->identifier : NULL;
  symbol->decl = NULL;
  symbol->token = token;

  while (1) {
    Token *token = peek();

    if (read('[')) {
      Expr *size = !check(']') ? assignment_expression() : NULL;
      expect(']');

      symbol->decl = declarator_new(DECL_ARRAY, symbol->decl, token);
      symbol->decl->size = size;
    } else if (read('(')) {
      Map *proto_scope = map_new(); // begin prototype scope
      vector_push(symbol_scopes, proto_scope);

      Vector *params = vector_new();
      bool ellipsis = false;
      if (!check(')')) {
        vector_push(params, parameter_declaration());
        while (read(',')) {
          if (read(TK_ELLIPSIS)) {
            ellipsis = true;
            break;
          }
          vector_push(params, parameter_declaration());
        }
      }
      expect(')');

      vector_pop(symbol_scopes); // end prototype scope

      symbol->decl = declarator_new(DECL_FUNCTION, symbol->decl, token);
      symbol->decl->params = params;
      symbol->decl->ellipsis = ellipsis;
      symbol->decl->proto_scope = proto_scope;
    } else {
      break;
    }
  }

  return symbol;
}

// type-name :
//   specifier-qualifier-list abstract-declarator?
static TypeName *type_name(void) {
  Token *token = peek();

  Vector *specs = specifier_qualifier_list();
  Declarator *decl = NULL;
  if (check('*') || check('[') || check('(')) {
    decl = abstract_declarator()->decl;
  }

  TypeName *type_name = calloc(1, sizeof(TypeName));
  type_name->specs = specs;
  type_name->decl = decl;
  type_name->token = token;
  return type_name;
}

// abstract-declarator :
//   '*' '*'*
//   ('*' '*'*)? direct-abstract-declarator
static Symbol *abstract_declarator(void) {
  Token *token = peek();

  if (read('*')) {
    if (check('*') || check('[') || check('(')) {
      Symbol *symbol = abstract_declarator();
      symbol->decl = declarator_new(DECL_POINTER, symbol->decl, token);
      return symbol;
    }

    Symbol *symbol = calloc(1, sizeof(Symbol));
    symbol->sy_type = SY_VARIABLE;
    symbol->decl = declarator_new(DECL_POINTER, NULL, token);
    symbol->token = token;
    return symbol;
  }

  return direct_abstract_declarator();
}

// direct-abstract-declarator :
//   direct-abstract-declarator '[' assignment-expression ']'
static Symbol *direct_abstract_declarator(void) {
  Token *token = peek();

  Symbol *symbol = calloc(1, sizeof(Symbol));
  symbol->sy_type = SY_VARIABLE;
  symbol->decl = NULL;
  symbol->token = token;

  while (1) {
    Token *token = peek();

    if (read('[')) {
      Expr *size = !check(']') ? assignment_expression() : NULL;
      expect(']');

      symbol->decl = declarator_new(DECL_ARRAY, symbol->decl, token);
      symbol->decl->size = size;
    } else if (read('(')) {
      Map *proto_scope = map_new(); // begin prototype scope
      vector_push(symbol_scopes, proto_scope);

      Vector *params = vector_new();
      bool ellipsis = false;
      if (!check(')')) {
        vector_push(params, parameter_declaration());
        while (read(',')) {
          if (read(TK_ELLIPSIS)) {
            ellipsis = true;
            break;
          }
          vector_push(params, parameter_declaration());
        }
      }
      expect(')');

      vector_pop(symbol_scopes); // end prototype scope

      symbol->decl = declarator_new(DECL_FUNCTION, symbol->decl, token);
      symbol->decl->params = params;
      symbol->decl->ellipsis = ellipsis;
      symbol->decl->proto_scope = proto_scope;
    } else {
      break;
    }
  }

  return symbol;
}

// initializer :
//   assignment-expression
//   '{' initializer (',' initializer)* '}'
//   '{' initializer (',' initializer)* ',' '}'
static Initializer *initializer(void) {
  Token *token = peek();

  if (read('{')) {
    Vector *list = vector_new();
    if (!check('}')) {
      do {
        vector_push(list, initializer());
      } while (read(',') && !check('}'));
    }
    expect('}');

    Initializer *init = calloc(1, sizeof(Initializer));
    init->list = list;
    init->token = token;
    return init;
  }

  Initializer *init = calloc(1, sizeof(Initializer));
  init->expr = assignment_expression();
  init->token = token;
  return init;
}

// parse statement

static Stmt *stmt_new(NodeType nd_type, Token *token) {
  Stmt *stmt = calloc(1, sizeof(Stmt));
  stmt->nd_type = nd_type;
  stmt->token = token;
  return stmt;
}

static Stmt *statement(void);

// labbeled-statement :
//   identifier ':' statement
static Stmt *labeled_statement(void) {
  Token *token = expect(TK_IDENTIFIER);
  expect(':');
  Stmt *label_stmt = statement();

  Stmt *stmt = stmt_new(ND_LABEL, token);
  stmt->label_ident = token->identifier;
  stmt->label_stmt = label_stmt;
  return stmt;
}

// case-statement :
//   'case' constant-expression ':' statement
static Stmt *case_statement(void) {
  Token *token = expect(TK_CASE);
  Expr *case_const = constant_expression();
  expect(':');
  Stmt *case_stmt = statement();

  Stmt *stmt = stmt_new(ND_CASE, token);
  stmt->case_const = case_const;
  stmt->case_stmt = case_stmt;
  return stmt;
}

// default-statement :
//   'default' ':' statement
static Stmt *default_statement(void) {
  Token *token = expect(TK_DEFAULT);
  expect(':');
  Stmt *default_stmt = statement();

  Stmt *stmt = stmt_new(ND_DEFAULT, token);
  stmt->default_stmt = default_stmt;
  return stmt;
}

// compound-statement :
//   '{' (declaration | statement)* '}'
static Stmt *compound_statement(void) {
  Token *token = expect('{');
  Vector *block_items = vector_new();
  while (!check('}') && !check(TK_EOF)) {
    if (check_declaration_specifier()) {
      vector_push(block_items, declaration());
    } else {
      vector_push(block_items, statement());
    }
  }
  expect('}');

  Stmt *stmt = stmt_new(ND_COMP, token);
  stmt->block_items = block_items;
  return stmt;
}

// expression-statement :
//   expression? ';'
static Stmt *expression_statement(void) {
  Token *token = peek();

  Expr *expr = !check(';') ? expression() : NULL;
  expect(';');

  Stmt *stmt = stmt_new(ND_EXPR, token);
  stmt->expr = expr;
  return stmt;
}

// if-statement :
//   'if' '(' expression ')' statement
//   'if' '(' expression ')' statement 'else' statement
static Stmt *if_statement(void) {
  Token *token = expect(TK_IF);
  expect('(');
  Expr *if_cond = expression();
  expect(')');
  Stmt *then_body = statement();
  Stmt *else_body = read(TK_ELSE) ? statement() : NULL;

  Stmt *stmt = stmt_new(ND_IF, token);
  stmt->if_cond = if_cond;
  stmt->then_body = then_body;
  stmt->else_body = else_body;
  return stmt;
}

// switch-statement :
//   'switch' '(' expression ')' statement
static Stmt *switch_statement(void) {
  Token *token = expect(TK_SWITCH);
  expect('(');
  Expr *switch_cond = expression();
  expect(')');
  Stmt *switch_body = statement();

  Stmt *stmt = stmt_new(ND_SWITCH, token);
  stmt->switch_cond = switch_cond;
  stmt->switch_body = switch_body;
  return stmt;
}

// while-statement :
//   'while' '(' expression ')' statement
static Stmt *while_statement(void) {
  Token *token = expect(TK_WHILE);
  expect('(');
  Expr *while_cond = expression();
  expect(')');
  Stmt *while_body = statement();

  Stmt *stmt = stmt_new(ND_WHILE, token);
  stmt->while_cond = while_cond;
  stmt->while_body = while_body;
  return stmt;
}

// do-statement :
//   'do' statement 'while' '(' expression ')' ';'
static Stmt *do_statement(void) {
  Token *token = expect(TK_DO);
  Stmt *do_body = statement();
  expect(TK_WHILE);
  expect('(');
  Expr *do_cond = expression();
  expect(')');
  expect(';');

  Stmt *stmt = stmt_new(ND_DO, token);
  stmt->do_cond = do_cond;
  stmt->do_body = do_body;
  return stmt;
}

// for-statement :
//   'for' '(' expression? ';' expression? ';' expression? ')' statement
//   'for' '(' declaration expression? ';' expression? ')' statement
static Stmt *for_statement(void) {
  vector_push(symbol_scopes, map_new()); // begin for-statement scope

  Token *token = expect(TK_FOR);
  expect('(');
  Node *for_init;
  if (check_declaration_specifier()) {
    Decl *decl = declaration();
    if (check_typedef(decl->specs)) {
      ERROR(decl->token, "typedef is not allowed in for-statement.");
    }
    for_init = (Node *) decl;
  } else {
    for_init = (Node *) (!check(';') ? expression() : NULL);
    expect(';');
  }
  Expr *for_cond = !check(';') ? expression() : NULL;
  expect(';');
  Expr *for_after = !check(')') ? expression() : NULL;
  expect(')');
  Stmt *for_body = statement();

  vector_pop(symbol_scopes); // end for-statement scope

  Stmt *stmt = stmt_new(ND_FOR, token);
  stmt->for_init = for_init;
  stmt->for_cond = for_cond;
  stmt->for_after = for_after;
  stmt->for_body = for_body;
  return stmt;
}

// goto-statmemt :
//   'goto' identifier ';'
static Stmt *goto_statement(void) {
  Token *token = expect(TK_GOTO);
  char *goto_ident = expect(TK_IDENTIFIER)->identifier;
  expect(';');

  Stmt *stmt = stmt_new(ND_GOTO, token);
  stmt->goto_ident = goto_ident;
  return stmt;
}

// continue-statement :
//   'continue' ';'
static Stmt *continue_statement(void) {
  Token *token = expect(TK_CONTINUE);
  expect(';');

  return stmt_new(ND_CONTINUE, token);
}

// break-statement :
//   'break' ';'
static Stmt *break_statement(void) {
  Token *token = expect(TK_BREAK);
  expect(';');

  return stmt_new(ND_BREAK, token);
}

// return-statement :
//   'return' expression? ';'
static Stmt *return_statement(void) {
  Token *token = expect(TK_RETURN);
  Expr *ret_expr = !check(';') ? expression() : NULL;
  expect(';');

  Stmt *stmt = stmt_new(ND_RETURN, token);
  stmt->ret_expr = ret_expr;
  return stmt;
}

// statement :
//   labeled-statement
//   case-statement
//   default-statement
//   compound-statement
//   expression-statement
//   if-statement
//   switch-statement
//   while-statement
//   do-statement
//   goto-statement
//   continue-statement
//   break-statement
//   return-statement
static Stmt *statement(void) {
  if (check(TK_IDENTIFIER) && tokens[pos + 1]->tk_type == ':')
    return labeled_statement();
  if (check(TK_CASE))
    return case_statement();
  if (check(TK_DEFAULT))
    return default_statement();
  if (check(TK_IF))
    return if_statement();
  if (check(TK_SWITCH))
    return switch_statement();
  if (check(TK_WHILE))
    return while_statement();
  if (check(TK_DO))
    return do_statement();
  if (check(TK_FOR))
    return for_statement();
  if (check(TK_GOTO))
    return goto_statement();
  if (check(TK_CONTINUE))
    return continue_statement();
  if (check(TK_BREAK))
    return break_statement();
  if (check(TK_RETURN))
    return return_statement();

  if (check('{')) {
    vector_push(symbol_scopes, map_new()); // begin block scope
    Stmt *stmt = compound_statement();
    vector_pop(symbol_scopes); // end block scope
    return stmt;
  }

  return expression_statement();
}

// parse translation-unit

// external-declaration :
//   function-definition
//   declaration
// function-definition :
//   declaration-specifiers declarator compound-statement
static Node *external_declaration(void) {
  Token *token = peek();

  Vector *specs = declaration_specifiers();
  bool sp_typedef = check_typedef(specs);

  // If ';' follows declaration-specifiers,
  // this is declaration without declarators.
  if (read(';')) {
    return (Node *) decl_new(specs, vector_new(), token);
  }

  Symbol *symbol = declarator(sp_typedef);

  // If '{' does not follow the first declarator,
  // this is declaration with init-declarators.
  if (!check('{')) {
    Vector *symbols = vector_new();

    symbol->init = read('=') ? initializer() : NULL;
    vector_push(symbols, symbol);
    put_symbol(symbol->identifier, symbol);

    while (read(',')) {
      Symbol *symbol = init_declarator(sp_typedef);
      vector_push(symbols, symbol);
      put_symbol(symbol->identifier, symbol);
    }
    expect(';');

    return (Node *) decl_new(specs, symbols, token);
  }

  // Otherwise, this is function definition.
  Declarator *func_decl = symbol->decl;
  if (func_decl) {
    while (func_decl->decl) {
      func_decl = func_decl->decl;
    }
  }
  if (!func_decl || func_decl->decl_type != DECL_FUNCTION) {
    ERROR(symbol->token, "function definition should have function type.");
  }

  if (sp_typedef) {
    ERROR(token, "typedef is not allowed in function definition.");
  }

  put_symbol(symbol->identifier, symbol);

  vector_push(symbol_scopes, func_decl->proto_scope);
  Stmt *body = compound_statement();
  vector_pop(symbol_scopes);

  Func *func = calloc(1, sizeof(Func));
  func->nd_type = ND_FUNC;
  func->specs = specs;
  func->symbol = symbol;
  func->body = body;
  func->token = token;
  return (Node *) func;
}

// translation-unit :
//   external-declaration external-declaration*
static TransUnit *translation_unit(void) {
  literals = vector_new();

  symbol_scopes = vector_new();
  vector_push(symbol_scopes, map_new()); // begin file scope

  // __builtin_va_list
  Symbol *sym_va_list = calloc(1, sizeof(Symbol));
  sym_va_list->sy_type = SY_TYPE;
  sym_va_list->identifier = "__builtin_va_list";
  sym_va_list->token = NULL;
  put_symbol(sym_va_list->identifier, sym_va_list);

  Vector *decls = vector_new();
  do {
    vector_push(decls, external_declaration());
  } while (!check(TK_EOF));

  vector_pop(symbol_scopes); // end file scope

  TransUnit *unit = calloc(1, sizeof(TransUnit));
  unit->literals = literals;
  unit->decls = decls;
  return unit;
}

// parser

static Token *inspect_pp_number(Token *token) {
  char *pp_number = token->pp_number;
  int pos = 0;

  unsigned long long int_value = 0;
  bool int_decimal = false;
  if (pp_number[pos] == '0') {
    pos++;
    if (tolower(pp_number[pos]) == 'x') {
      pos++;
      while (isxdigit(pp_number[pos])) {
        char c = pp_number[pos++];
        int d = isdigit(c) ? c - '0' : tolower(c) - 'a' + 10;
        int_value = int_value * 16 + d;
      }
    } else {
      while ('0' <= pp_number[pos] && pp_number[pos] < '8') {
        int_value = int_value * 8 + (pp_number[pos++] - '0');
      }
    }
  } else {
    int_decimal = true;
    while (isdigit(pp_number[pos])) {
      int_value = int_value * 10 + (pp_number[pos++] - '0');
    }
  }

  bool int_u = false;
  bool int_l = false;
  bool int_ll = false;
  if (tolower(pp_number[pos]) == 'u') {
    int_u = true;
    pos++;
    if (tolower(pp_number[pos] == 'l')) {
      pos++;
      if (tolower(pp_number[pos] == 'l')) {
        int_ll = true;
        pos++;
      } else {
        int_l = true;
      }
    }
  } else if (tolower(pp_number[pos]) == 'l') {
    pos++;
    if (tolower(pp_number[pos]) == 'l') {
      int_ll = true;
      pos++;
    } else {
      int_l = true;
    }
    if (tolower(pp_number[pos]) == 'u') {
      int_u = true;
      pos++;
    }
  }

  if (pp_number[pos] != '\0') {
    ERROR(token, "invalid preprocessing number.");
  }

  Token *int_const = calloc(1, sizeof(Token));
  int_const->tk_type = TK_INTEGER_CONST;
  int_const->text = token->text;
  int_const->loc = token->loc;
  int_const->int_value = int_value;
  int_const->int_decimal = int_decimal;
  int_const->int_u = int_u;
  int_const->int_l = int_l;
  int_const->int_ll = int_ll;
  return int_const;
}

TransUnit *parse(Vector *_tokens) {
  // remove newlines and white-spaces
  // inspect pp-numbers
  Vector *parse_tokens = vector_new();
  for (int i = 0; i < _tokens->length; i++) {
    Token *token = _tokens->buffer[i];
    if (token->tk_type == TK_SPACE) continue;
    if (token->tk_type == TK_NEWLINE) continue;
    if (token->tk_type == TK_PP_NUMBER) {
      vector_push(parse_tokens, inspect_pp_number(token));
    } else {
      vector_push(parse_tokens, token);
    }
  }

  tokens = (Token **) parse_tokens->buffer;
  pos = 0;

  return translation_unit();
}
