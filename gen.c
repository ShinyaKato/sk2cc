#include "cc.h"

char arg_reg[6][4] = { "rdi", "rsi", "rdx", "rcx", "r8", "r9" };

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
    int pos = node->symbol->position;

    printf("  movl %d(%%rbp), %%eax\n", pos);
    gen_push("eax");
  }

  if (node->type == FUNC_CALL) {
    for (int i = 0; i < node->args_count; i++) {
      gen_expr(node->args[i]);
    }

    for (int i = 5; i >= 0; i--) {
      if (i < node->args_count) {
        gen_pop("eax");
        printf("  mov %%rax, %%%s\n", arg_reg[i]);
      }
    }

    printf("  call %s\n", node->left->identifier);
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
    int pos = node->left->symbol->position;

    gen_operand(node->right, "eax");
    printf("  movl %%eax, %d(%%rbp)\n", pos);
    gen_push("eax");
  }
}

void gen_comp_stmt(Node *node) {
  for (int i = 0; i < node->statements->length; i++) {
    gen_expr((Node *) node->statements->array[i]);
    gen_pop("eax");
  }
}

void gen_func_def(Node *node) {
  if (strcmp(node->identifier, "main") == 0) {
    printf("  .global main\n");
  }

  printf("%s:\n", node->identifier);
  printf("  push %%rbp\n");
  printf("  mov %%rsp, %%rbp\n");
  printf("  sub $%d, %%rsp\n", 4 * node->vars_count);
  for (int i = 0; i < node->params_count; i++) {
    printf("  mov %%%s, %%rax\n", arg_reg[i]);
    printf("  movl %%eax, %d(%%rbp)\n", -(i * 4 + 4));
  }
  gen_comp_stmt(node->left);
  printf("  add $%d, %%rsp\n", 4 * node->vars_count);
  printf("  pop %%rbp\n");
  printf("  ret\n");
}

void gen(Node *node) {
  for (int i = 0; i < node->definitions->length; i++) {
    gen_func_def((Node *) node->definitions->array[i]);

    if (i + 1 < node->definitions->length) {
      printf("\n");
    }
  }
}