#include "cc.h"

void generate_immediate(int value) {
  printf("  sub $4, %%rsp\n");
  printf("  movl $%d, 0(%%rsp)\n", value);
}

void generate_push(char *reg) {
  printf("  sub $4, %%rsp\n");
  printf("  movl %%%s, 0(%%rsp)\n", reg);
}

void generate_pop(char *reg) {
  printf("  movl 0(%%rsp), %%%s\n", reg);
  printf("  add $4, %%rsp\n");
}

int label_no = 0;

void generate_expression(Node *node) {
  if (node->type == CONST) {
    generate_immediate(node->int_value);
  } else if (node->type == IDENTIFIER) {
    int pos = symbols_lookup(node->identifier)->position;
    printf("  movl %d(%%rbp), %%eax\n", pos);
    generate_push("eax");
  } else if (node->type == LAND) {
    int label_false = label_no++;
    int label_end = label_no++;
    generate_expression(node->left);
    generate_pop("eax");
    printf("  cmpl $0, %%eax\n");
    printf("  je .L%d\n", label_false);
    generate_expression(node->right);
    generate_pop("eax");
    printf("  cmpl $0, %%eax\n");
    printf("  je .L%d\n", label_false);
    printf("  movl $1, %%eax\n");
    printf("  jmp .L%d\n", label_end);
    printf(".L%d:\n", label_false);
    printf("  movl $0, %%eax\n");
    printf(".L%d:\n", label_end);
    generate_push("eax");
  } else if (node->type == LOR) {
    int label_true = label_no++;
    int label_end = label_no++;
    generate_expression(node->left);
    generate_pop("eax");
    printf("  cmpl $0, %%eax\n");
    printf("  jne .L%d\n", label_true);
    generate_expression(node->right);
    generate_pop("eax");
    printf("  cmpl $0, %%eax\n");
    printf("  jne .L%d\n", label_true);
    printf("  movl $0, %%eax\n");
    printf("  jmp .L%d\n", label_end);
    printf(".L%d:\n", label_true);
    printf("  movl $1, %%eax\n");
    printf(".L%d:\n", label_end);
    generate_push("eax");
  } else if (node->type == CONDITION) {
    int label_false = label_no++;
    int label_end = label_no++;
    generate_expression(node->condition);
    generate_pop("eax");
    printf("  cmpl $0, %%eax\n");
    printf("  je .L%d\n", label_false);
    generate_expression(node->left);
    printf("  jmp .L%d\n", label_end);
    printf(".L%d:\n", label_false);
    generate_expression(node->right);
    printf(".L%d:\n", label_end);
  } else if (node->type == UPLUS) {
    generate_expression(node->left);
  } else if (node->type == UMINUS) {
    generate_expression(node->left);
    generate_pop("eax");
    printf("  negl %%eax\n");
    generate_push("eax");
  } else if (node->type == NOT) {
    generate_expression(node->left);
    generate_pop("eax");
    printf("  notl %%eax\n");
    generate_push("eax");
  } else if (node->type == LNOT) {
    generate_expression(node->left);
    generate_pop("eax");
    printf("  cmpl $0, %%eax\n");
    printf("  sete %%al\n");
    printf("  movzbl %%al, %%eax\n");
    generate_push("eax");
  } else if (node->type == ASSIGN) {
    int pos = symbols_lookup(node->left->identifier)->position;
    generate_expression(node->right);
    generate_pop("eax");
    printf("  movl %%eax, %d(%%rbp)\n", pos);
    generate_push("eax");
  } else {
    generate_expression(node->left);
    generate_expression(node->right);
    generate_pop("ecx");
    generate_pop("eax");
    if (node->type == ADD) {
      printf("  addl %%ecx, %%eax\n");
      generate_push("eax");
    } else if (node->type == SUB) {
      printf("  subl %%ecx, %%eax\n");
      generate_push("eax");
    } else if (node->type == MUL) {
      printf("  imull %%ecx\n");
      generate_push("eax");
    } else if (node->type == DIV) {
      printf("  movl $0, %%edx\n");
      printf("  idivl %%ecx\n");
      generate_push("eax");
    } else if (node->type == MOD) {
      printf("  movl $0, %%edx\n");
      printf("  idivl %%ecx\n");
      generate_push("edx");
    } else if (node->type == LSHIFT) {
      printf("  sall %%cl, %%eax\n");
      generate_push("eax");
    } else if (node->type == RSHIFT) {
      printf("  sarl %%cl, %%eax\n");
      generate_push("eax");
    } else if (node->type == LT) {
      printf("  cmpl %%ecx, %%eax\n");
      printf("  setl %%al\n");
      printf("  movzbl %%al, %%eax\n");
      generate_push("eax");
    } else if (node->type == GT) {
      printf("  cmpl %%ecx, %%eax\n");
      printf("  setg %%al\n");
      printf("  movzbl %%al, %%eax\n");
      generate_push("eax");
    } else if (node->type == LTE) {
      printf("  cmpl %%ecx, %%eax\n");
      printf("  setle %%al\n");
      printf("  movzbl %%al, %%eax\n");
      generate_push("eax");
    } else if (node->type == GTE) {
      printf("  cmpl %%ecx, %%eax\n");
      printf("  setge %%al\n");
      printf("  movzbl %%al, %%eax\n");
      generate_push("eax");
    } else if (node->type == EQ) {
      printf("  cmpl %%ecx, %%eax\n");
      printf("  sete %%al\n");
      printf("  movzbl %%al, %%eax\n");
      generate_push("eax");
    } else if (node->type == NEQ) {
      printf("  cmpl %%ecx, %%eax\n");
      printf("  setne %%al\n");
      printf("  movzbl %%al, %%eax\n");
      generate_push("eax");
    } else if (node->type == AND) {
      printf("  andl %%ecx, %%eax\n");
      generate_push("eax");
    } else if (node->type == OR) {
      printf("  orl %%ecx, %%eax\n");
      generate_push("eax");
    } else if (node->type == XOR) {
      printf("  xorl %%ecx, %%eax\n");
      generate_push("eax");
    }
  }
}

void generate_block_items(Node *node) {
  if (node->type == BLOCK_ITEM) {
    generate_block_items(node->left);
    generate_pop("eax");
    generate_expression(node->right);
  } else {
    generate_expression(node);
  }
}

void generate(Node *node) {
  printf("  .global main\n");
  printf("main:\n");
  printf("  push %%rbp\n");
  printf("  mov %%rsp, %%rbp\n");

  printf("  sub $%d, %%rsp\n", 4 * symbols_count());

  generate_block_items(node);

  generate_pop("eax");

  printf("  add $%d, %%rsp\n", 4 * symbols_count());

  printf("  pop %%rbp\n");
  printf("  ret\n");
}

int main(void) {
  lex_init();
  parse_init();

  Node *node = parse();
  generate(node);

  return 0;
}
