#include <assert.h>
#include "../sk2cc.h"

void test_token(TokenType tk_type, char *expected) {
  char *actual = token_name(tk_type);
  assert(strcmp(actual, expected) == 0);
}

void test_char_token(char c) {
  char expected[] = { c, '\0' };
  test_token(c, expected);
}

int main(void) {
  test_char_token('[');
  test_char_token(']');
  test_char_token('(');
  test_char_token(')');
  test_char_token('{');
  test_char_token('}');
  test_char_token('.');
  test_char_token('&');
  test_char_token('*');
  test_char_token('+');
  test_char_token('-');
  test_char_token('~');
  test_char_token('!');
  test_char_token('/');
  test_char_token('%');
  test_char_token('<');
  test_char_token('>');
  test_char_token('^');
  test_char_token('|');
  test_char_token('?');
  test_char_token(':');
  test_char_token(';');
  test_char_token('=');
  test_char_token(',');
  test_char_token('#');

  test_token(TK_NEWLINE, "newline");
  test_token(TK_SPACE, "white space");

  test_token(TK_SIZEOF, "sizeof");
  test_token(TK_ALIGNOF, "_Alignof");

  test_token(TK_TYPEDEF, "typedef");
  test_token(TK_EXTERN, "extern");
  test_token(TK_VOID, "void");
  test_token(TK_CHAR, "char");
  test_token(TK_SHORT, "short");
  test_token(TK_INT, "int");
  test_token(TK_LONG, "long");
  test_token(TK_SIGNED, "signed");
  test_token(TK_UNSIGNED, "unsigned");
  test_token(TK_BOOL, "_Bool");
  test_token(TK_STRUCT, "struct");
  test_token(TK_ENUM, "enum");
  test_token(TK_NORETURN, "_Noreturn");

  test_token(TK_IF, "if");
  test_token(TK_ELSE, "else");
  test_token(TK_WHILE, "while");
  test_token(TK_DO, "do");
  test_token(TK_FOR, "for");
  test_token(TK_CONTINUE, "continue");
  test_token(TK_BREAK, "break");
  test_token(TK_RETURN, "return");

  test_token(TK_IDENTIFIER, "identifier");
  test_token(TK_INTEGER_CONST, "integer constant");
  test_token(TK_STRING_LITERAL, "string literal");

  test_token(TK_ARROW, "->");
  test_token(TK_INC, "++");
  test_token(TK_DEC, "--");
  test_token(TK_LSHIFT, "<<");
  test_token(TK_RSHIFT, ">>");
  test_token(TK_LTE, "<=");
  test_token(TK_GTE, ">=");
  test_token(TK_EQ, "==");
  test_token(TK_NEQ, "!=");
  test_token(TK_AND, "&&");
  test_token(TK_OR, "||");
  test_token(TK_MUL_ASSIGN, "*=");
  test_token(TK_DIV_ASSIGN, "/=");
  test_token(TK_MOD_ASSIGN, "%=");
  test_token(TK_ADD_ASSIGN, "+=");
  test_token(TK_SUB_ASSIGN, "-=");
  test_token(TK_ELLIPSIS, "...");

  test_token(TK_EOF, "end of file");

  return 0;
}
