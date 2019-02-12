#include "sk2cc.h"

Vector *src;
int src_pos;

Token *token_new() {
  Token *token = (Token *) calloc(1, sizeof(Token));
  token->schar = (SourceChar **) &(src->buffer[src_pos]);
  return token;
}

char peek_char() {
  SourceChar *schar = src->buffer[src_pos];
  return *(schar->char_ptr);
}

char get_char() {
  char c = peek_char();
  src_pos++;
  return c;
}

void expect_char(char c) {
  if (peek_char() != c) {
    error(token_new(), "%c is expected.", c);
  }
  get_char();
}

bool read_char(char c) {
  if (peek_char() == c) {
    get_char();
    return true;
  }
  return false;
}

char get_escape_sequence() {
  if (read_char('\'')) return '\'';
  if (read_char('"')) return '"';
  if (read_char('?')) return '?';
  if (read_char('\\')) return '\\';
  if (read_char('a')) return '\a';
  if (read_char('b')) return '\b';
  if (read_char('f')) return '\f';
  if (read_char('n')) return '\n';
  if (read_char('r')) return '\r';
  if (read_char('v')) return '\v';
  if (read_char('t')) return '\t';
  if (read_char('0')) return '\0';
  error(token_new(), "invalid escape sequence.");
}

Token *lex() {
  Token *token = token_new();
  if (isdigit(peek_char())) {
    String *s = string_new();
    while (isdigit(peek_char())) {
      string_push(s, get_char());
    }
    int int_value = 0;
    for (int i = 0; i < s->length; i++) {
      int_value = int_value * 10 + (s->buffer[i] - '0');
    }
    token->tk_type = TK_INTEGER_CONST;
    token->int_value = int_value;
  } else if (read_char('\'')) {
    int int_value;
    if (read_char('\\')) {
      int_value = get_escape_sequence();
    } else {
      int_value = get_char();
    }
    expect_char('\'');
    token->tk_type = TK_INTEGER_CONST;
    token->int_value = int_value;
  } else if (read_char('"')) {
    String *string_value = string_new();
    while (peek_char() != '"') {
      char c = get_char();
      if (c == '\\') c = get_escape_sequence();
      string_push(string_value, c);
    }
    get_char();
    string_push(string_value, '\0');
    token->tk_type = TK_STRING_LITERAL;
    token->string_value = string_value;
  } else if (isalpha(peek_char()) || peek_char() == '_') {
    String *identifier = string_new();
    string_push(identifier, get_char());
    while (isalnum(peek_char()) || peek_char() == '_') {
      string_push(identifier, get_char());
    }
    if (strcmp(identifier->buffer, "void") == 0) {
      token->tk_type = TK_VOID;
    } else if (strcmp(identifier->buffer, "_Bool") == 0) {
      token->tk_type = TK_BOOL;
    } else if (strcmp(identifier->buffer, "char") == 0) {
      token->tk_type = TK_CHAR;
    } else if (strcmp(identifier->buffer, "short") == 0) {
      token->tk_type = TK_SHORT;
    } else if (strcmp(identifier->buffer, "int") == 0) {
      token->tk_type = TK_INT;
    } else if (strcmp(identifier->buffer, "unsigned") == 0) {
      token->tk_type = TK_UNSIGNED;
    } else if (strcmp(identifier->buffer, "struct") == 0) {
      token->tk_type = TK_STRUCT;
    } else if (strcmp(identifier->buffer, "enum") == 0) {
      token->tk_type = TK_ENUM;
    } else if (strcmp(identifier->buffer, "typedef") == 0) {
      token->tk_type = TK_TYPEDEF;
    } else if (strcmp(identifier->buffer, "extern") == 0) {
      token->tk_type = TK_EXTERN;
    } else if (strcmp(identifier->buffer, "_Noreturn") == 0) {
      token->tk_type = TK_NORETURN;
    } else if (strcmp(identifier->buffer, "sizeof") == 0) {
      token->tk_type = TK_SIZEOF;
    } else if (strcmp(identifier->buffer, "_Alignof") == 0) {
      token->tk_type = TK_ALIGNOF;
    } else if (strcmp(identifier->buffer, "if") == 0) {
      token->tk_type = TK_IF;
    } else if (strcmp(identifier->buffer, "else") == 0) {
      token->tk_type = TK_ELSE;
    } else if (strcmp(identifier->buffer, "while") == 0) {
      token->tk_type = TK_WHILE;
    } else if (strcmp(identifier->buffer, "do") == 0) {
      token->tk_type = TK_DO;
    } else if (strcmp(identifier->buffer, "for") == 0) {
      token->tk_type = TK_FOR;
    } else if (strcmp(identifier->buffer, "continue") == 0) {
      token->tk_type = TK_CONTINUE;
    } else if (strcmp(identifier->buffer, "break") == 0) {
      token->tk_type = TK_BREAK;
    } else if (strcmp(identifier->buffer, "return") == 0) {
      token->tk_type = TK_RETURN;
    } else {
      token->tk_type = TK_IDENTIFIER;
      token->identifier = identifier->buffer;
    }
  } else if (read_char('~')) {
    token->tk_type = '~';
  } else if (read_char('+')) {
    if (read_char('+')) {
      token->tk_type = TK_INC;
    } else if (read_char('=')) {
      token->tk_type = TK_ADD_ASSIGN;
    } else {
      token->tk_type = '+';
    }
  } else if (read_char('-')) {
    if (read_char('>')) {
      token->tk_type = TK_ARROW;
    } else if (read_char('-')) {
      token->tk_type = TK_DEC;
    } else if (read_char('=')) {
      token->tk_type = TK_SUB_ASSIGN;
    } else {
      token->tk_type = '-';
    }
  } else if (read_char('*')) {
    if (read_char('=')) {
      token->tk_type = TK_MUL_ASSIGN;
    } else {
      token->tk_type = '*';
    }
  } else if (read_char('/')) {
    if (read_char('=')) {
      token->tk_type = TK_DIV_ASSIGN;
    } else if (read_char('/')) {
      while (1) {
        char c = get_char();
        if (c == '\n') break;
      }
      token->tk_type = TK_SPACE;
    } else if (read_char('*')) {
      while (1) {
        char c = get_char();
        if (c == '*' && read_char('/')) break;
      }
      token->tk_type = TK_SPACE;
    } else {
      token->tk_type = '/';
    }
  } else if (read_char('%')) {
    if (read_char('=')) {
      token->tk_type = TK_MOD_ASSIGN;
    } else {
      token->tk_type = '%';
    }
  } else if (read_char('<')) {
    if (read_char('=')) {
      token->tk_type = TK_LTE;
    } else if (read_char('<')) {
      token->tk_type = TK_LSHIFT;
    } else {
      token->tk_type = '<';
    }
  } else if (read_char('>')) {
    if (read_char('=')) {
      token->tk_type = TK_GTE;
    } else if (read_char('>')) {
      token->tk_type = TK_RSHIFT;
    } else {
      token->tk_type = '>';
    }
  } else if (read_char('=')) {
    if (read_char('=')) {
      token->tk_type = TK_EQ;
    } else {
      token->tk_type = '=';
    }
  } else if (read_char('!')) {
    if (read_char('=')) {
      token->tk_type = TK_NEQ;
    } else {
      token->tk_type = '!';
    }
  } else if (read_char('&')) {
    if (read_char('&')) {
      token->tk_type = TK_AND;
    } else {
      token->tk_type = '&';
    }
  } else if (read_char('|')) {
    if (read_char('|')) {
      token->tk_type = TK_OR;
    } else {
      token->tk_type = '|';
    }
  } else if (read_char('^')) {
    token->tk_type = '^';
  } else if (read_char('?')) {
    token->tk_type = '?';
  } else if (read_char(':')) {
    token->tk_type = ':';
  } else if (read_char(';')) {
    token->tk_type = ';';
  } else if (read_char('[')) {
    token->tk_type = '[';
  } else if (read_char(']')) {
    token->tk_type = ']';
  } else if (read_char('(')) {
    token->tk_type = '(';
  } else if (read_char(')')) {
    token->tk_type = ')';
  } else if (read_char('{')) {
    token->tk_type = '{';
  } else if (read_char('}')) {
    token->tk_type = '}';
  } else if (read_char(',')) {
    token->tk_type = ',';
  } else if (read_char('.')) {
    if (read_char('.')) {
      if (!read_char('.')) {
        error(token, "invalid token '..'");
      }
      token->tk_type = TK_ELLIPSIS;
    } else {
      token->tk_type = '.';
    }
  } else if (read_char('#')) {
    token->tk_type = '#';
  } else if (read_char(' ')) {
    token->tk_type = TK_SPACE;
  } else if (read_char('\n')) {
    token->tk_type = TK_NEWLINE;
  } else if (peek_char() == EOF) {
    token->tk_type = TK_EOF;
  } else {
    error(token, "unexpected character: %c.", peek_char());
  }

  token->schar_end = (SourceChar **) &(src->buffer[src_pos]);

  return token;
}

Vector *tokenize(Vector *input_src) {
  src = input_src;
  src_pos = 0;

  Vector *pp_tokens = vector_new();
  while (1) {
    Token *pp_token = lex();
    vector_push(pp_tokens, pp_token);
    if (pp_token->tk_type == TK_EOF) break;
  }

  return pp_tokens;
}
