#include "sk2cc.h"

Location *location_new(char *filename, char *line_ptr, int lineno, int column) {
  Location *loc = calloc(1, sizeof(Location));
  loc->filename = filename;
  loc->line_ptr = line_ptr;
  loc->lineno = lineno;
  loc->column = column;
  return loc;
}

Token *token_new(TokenType tk_type, char *text, Location *loc) {
  Token *token = calloc(1, sizeof(Token));
  token->tk_type = tk_type;
  token->text = text;
  token->loc = loc;
  return token;
}

Token *inspect_pp_number(Token *token) {
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

  Token *new_token = token_new(TK_INTEGER_CONST, token->text, token->loc);
  new_token->int_value = int_value;
  new_token->int_decimal = int_decimal;
  new_token->int_u = int_u;
  new_token->int_l = int_l;
  new_token->int_ll = int_ll;
  return new_token;
}
