#include "cc.h"

int label_no = 0;

void gen_immediate(int value) {
  printf("  sub $4, %%rsp\n");
  printf("  movl $%d, 0(%%rsp)\n", value);
}

void gen_push(char *reg) {
  printf("  sub $4, %%rsp\n");
  printf("  movl %%%s, 0(%%rsp)\n", reg);
}

void gen_pop(char *reg) {
  printf("  movl 0(%%rsp), %%%s\n", reg);
  printf("  add $4, %%rsp\n");
}

void gen_expr(Node *node);

void gen_operand(Node *node, char *reg) {
  gen_expr(node);
  gen_pop(reg);
}

void gen_operands(Node *left, Node *right, char *reg_left, char *reg_right) {
  gen_expr(left);
  gen_expr(right);
  gen_pop(reg_right);
  gen_pop(reg_left);
}

void gen_expr(Node *node) {
  if (node->type == CONST) {
    gen_immediate(node->int_value);
  }

  if (node->type == IDENTIFIER) {
    int pos = symbols_lookup(node->identifier)->position;

    printf("  movl %d(%%rbp), %%eax\n", pos);
    gen_push("eax");
  }

  if (node->type == UPLUS) {
    gen_operand(node->left, "eax");
    gen_push("eax");
  }

  if (node->type == UMINUS) {
    gen_operand(node->left, "eax");
    printf("  negl %%eax\n");
    gen_push("eax");
  }

  if (node->type == NOT) {
    gen_operand(node->left, "eax");
    printf("  notl %%eax\n");
    gen_push("eax");
  }

  if (node->type == LNOT) {
    gen_operand(node->left, "eax");
    printf("  cmpl $0, %%eax\n");
    printf("  sete %%al\n");
    printf("  movzbl %%al, %%eax\n");
    gen_push("eax");
  }

  if (node->type == MUL) {
    gen_operands(node->left, node->right, "eax", "ecx");
    printf("  imull %%ecx\n");
    gen_push("eax");
  }

  if (node->type == DIV) {
    gen_operands(node->left, node->right, "eax", "ecx");
    printf("  movl $0, %%edx\n");
    printf("  idivl %%ecx\n");
    gen_push("eax");
  }

  if (node->type == MOD) {
    gen_operands(node->left, node->right, "eax", "ecx");
    printf("  movl $0, %%edx\n");
    printf("  idivl %%ecx\n");
    gen_push("edx");
  }

  if (node->type == ADD) {
    gen_operands(node->left, node->right, "eax", "ecx");
    printf("  addl %%ecx, %%eax\n");
    gen_push("eax");
  }

  if (node->type == SUB) {
    gen_operands(node->left, node->right, "eax", "ecx");
    printf("  subl %%ecx, %%eax\n");
    gen_push("eax");
  }

  if (node->type == LSHIFT) {
    gen_operands(node->left, node->right, "eax", "ecx");
    printf("  sall %%cl, %%eax\n");
    gen_push("eax");
  }

  if (node->type == RSHIFT) {
    gen_operands(node->left, node->right, "eax", "ecx");
    printf("  sarl %%cl, %%eax\n");
    gen_push("eax");
  }

  if (node->type == LT) {
    gen_operands(node->left, node->right, "eax", "ecx");
    printf("  cmpl %%ecx, %%eax\n");
    printf("  setl %%al\n");
    printf("  movzbl %%al, %%eax\n");
    gen_push("eax");
  }

  if (node->type == GT) {
    gen_operands(node->left, node->right, "eax", "ecx");
    printf("  cmpl %%ecx, %%eax\n");
    printf("  setg %%al\n");
    printf("  movzbl %%al, %%eax\n");
    gen_push("eax");
  }

  if (node->type == LTE) {
    gen_operands(node->left, node->right, "eax", "ecx");
    printf("  cmpl %%ecx, %%eax\n");
    printf("  setle %%al\n");
    printf("  movzbl %%al, %%eax\n");
    gen_push("eax");
  }

  if (node->type == GTE) {
    gen_operands(node->left, node->right, "eax", "ecx");
    printf("  cmpl %%ecx, %%eax\n");
    printf("  setge %%al\n");
    printf("  movzbl %%al, %%eax\n");
    gen_push("eax");
  }

  if (node->type == EQ) {
    gen_operands(node->left, node->right, "eax", "ecx");
    printf("  cmpl %%ecx, %%eax\n");
    printf("  sete %%al\n");
    printf("  movzbl %%al, %%eax\n");
    gen_push("eax");
  }

  if (node->type == NEQ) {
    gen_operands(node->left, node->right, "eax", "ecx");
    printf("  cmpl %%ecx, %%eax\n");
    printf("  setne %%al\n");
    printf("  movzbl %%al, %%eax\n");
    gen_push("eax");
  }

  if (node->type == AND) {
    gen_operands(node->left, node->right, "eax", "ecx");
    printf("  andl %%ecx, %%eax\n");
    gen_push("eax");
  }

  if (node->type == XOR) {
    gen_operands(node->left, node->right, "eax", "ecx");
    printf("  xorl %%ecx, %%eax\n");
    gen_push("eax");
  }

  if (node->type == OR) {
    gen_operands(node->left, node->right, "eax", "ecx");
    printf("  orl %%ecx, %%eax\n");
    gen_push("eax");
  }

  if (node->type == LAND) {
    int label_false = label_no++;
    int label_end = label_no++;

    gen_operand(node->left, "eax");
    printf("  cmpl $0, %%eax\n");
    printf("  je .L%d\n", label_false);
    gen_operand(node->right, "eax");
    printf("  cmpl $0, %%eax\n");
    printf("  je .L%d\n", label_false);
    printf("  movl $1, %%eax\n");
    printf("  jmp .L%d\n", label_end);
    printf(".L%d:\n", label_false);
    printf("  movl $0, %%eax\n");
    printf(".L%d:\n", label_end);
    gen_push("eax");
  }

  if (node->type == LOR) {
    int label_true = label_no++;
    int label_end = label_no++;

    gen_operand(node->left, "eax");
    printf("  cmpl $0, %%eax\n");
    printf("  jne .L%d\n", label_true);
    gen_operand(node->right, "eax");
    printf("  cmpl $0, %%eax\n");
    printf("  jne .L%d\n", label_true);
    printf("  movl $0, %%eax\n");
    printf("  jmp .L%d\n", label_end);
    printf(".L%d:\n", label_true);
    printf("  movl $1, %%eax\n");
    printf(".L%d:\n", label_end);
    gen_push("eax");
  }

  if (node->type == CONDITION) {
    int label_false = label_no++;
    int label_end = label_no++;

    gen_operand(node->condition, "eax");
    printf("  cmpl $0, %%eax\n");
    printf("  je .L%d\n", label_false);
    gen_expr(node->left);
    printf("  jmp .L%d\n", label_end);
    printf(".L%d:\n", label_false);
    gen_expr(node->right);
    printf(".L%d:\n", label_end);
  }

  if (node->type == ASSIGN) {
    int pos = symbols_lookup(node->left->identifier)->position;

    gen_operand(node->right, "eax");
    printf("  movl %%eax, %d(%%rbp)\n", pos);
    gen_push("eax");
  }
}

void gen_block(Node *node) {
  if (node->type == BLOCK_ITEM) {
    gen_block(node->left);
    gen_pop("eax");
    gen_expr(node->right);
  } else {
    gen_expr(node);
  }
}

void gen(Node *node) {
  printf("  .global main\n");
  printf("main:\n");
  printf("  push %%rbp\n");
  printf("  mov %%rsp, %%rbp\n");

  printf("  sub $%d, %%rsp\n", 4 * symbols_count());

  gen_block(node);
  gen_pop("eax");

  printf("  add $%d, %%rsp\n", 4 * symbols_count());

  printf("  pop %%rbp\n");
  printf("  ret\n");
}
