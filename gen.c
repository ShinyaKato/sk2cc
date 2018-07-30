#include "cc.h"

char arg_reg[6][4] = { "rdi", "rsi", "rdx", "rcx", "r8", "r9" };

int label_no = 0, return_label;
Vector *continue_labels, *break_labels;

void gen_push(char *reg) {
  printf("  pushq %%%s\n", reg);
}

void gen_pop(char *reg) {
  printf("  popq %%%s\n", reg);
}

void gen_jump(char *inst, int label) {
  printf("  %s .L%d\n", inst, label);
}

void gen_label(int label) {
  printf(".L%d:\n", label);
}

void gen_expr(Node *node);

void gen_lvalue(Node *node) {
  if (node->type == IDENTIFIER) {
    Symbol *symbol = node->symbol;
    printf("  leaq %d(%%rbp), %%rax\n", -symbol->offset);
    gen_push("rax");
  }

  if (node->type == INDIRECT) {
    gen_expr(node->left);
  }
}

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
    printf("  movl $%d, %%eax\n", node->int_value);
    gen_push("rax");
  }

  if (node->type == IDENTIFIER) {
    Symbol *symbol = node->symbol;
    if (!symbol) {
      error("undefined identifier.");
    }
    if (symbol->value_type->type == INT) {
      printf("  movl %d(%%rbp), %%eax\n", -symbol->offset);
    } else if (symbol->value_type->type == POINTER) {
      printf("  movq %d(%%rbp), %%rax\n", -symbol->offset);
    } else if (symbol->value_type->type == ARRAY) {
      printf("  leaq %d(%%rbp), %%rax\n", -symbol->offset);
    }
    gen_push("rax");
  }

  if (node->type == FUNC_CALL) {
    for (int i = 0; i < node->args->length; i++) {
      gen_expr((Node *) node->args->array[i]);
    }
    for (int i = 5; i >= 0; i--) {
      if (i < node->args->length) {
        gen_pop(arg_reg[i]);
      }
    }
    printf("  call %s\n", node->left->identifier);
    gen_push("rax");
  }

  if (node->type == ADDRESS) {
    int offset = node->left->symbol->offset;
    printf("  leaq %d(%%rbp), %%rax\n", -offset);
    gen_push("rax");
  }

  if (node->type == INDIRECT) {
    gen_expr(node->left);
    if (node->left->value_type->pointer_to->type != ARRAY) {
      gen_pop("rax");
      if (node->left->value_type->type == INT) {
        printf("  movl (%%rax), %%ecx\n");
      } else if (node->left->value_type->type == POINTER) {
        printf("  movq (%%rax), %%rcx\n");
      }
      gen_push("rcx");
    }
  }

  if (node->type == UPLUS) {
    gen_operand(node->left, "rax");
    gen_push("rax");
  }

  if (node->type == UMINUS) {
    gen_operand(node->left, "rax");
    printf("  negl %%eax\n");
    gen_push("rax");
  }

  if (node->type == NOT) {
    gen_operand(node->left, "rax");
    printf("  notl %%eax\n");
    gen_push("rax");
  }

  if (node->type == LNOT) {
    gen_operand(node->left, "rax");
    printf("  cmpl $0, %%eax\n");
    printf("  sete %%al\n");
    printf("  movzbl %%al, %%eax\n");
    gen_push("rax");
  }

  if (node->type == MUL) {
    gen_operands(node->left, node->right, "rax", "rcx");
    printf("  imull %%ecx\n");
    gen_push("rax");
  }

  if (node->type == DIV) {
    gen_operands(node->left, node->right, "rax", "rcx");
    printf("  movl $0, %%edx\n");
    printf("  idivl %%ecx\n");
    gen_push("rax");
  }

  if (node->type == MOD) {
    gen_operands(node->left, node->right, "rax", "rcx");
    printf("  movl $0, %%edx\n");
    printf("  idivl %%ecx\n");
    gen_push("rdx");
  }

  if (node->type == ADD) {
    Type *left_type = node->left->value_type;
    Type *right_type = node->right->value_type;

    if (left_type->type == INT && right_type->type == INT) {
      gen_operands(node->left, node->right, "rax", "rcx");
      printf("  addl %%ecx, %%eax\n");
      gen_push("rax");
    } else if (left_type->type == POINTER && right_type->type == INT) {
      int size = left_type->pointer_to->size;
      gen_expr(node->left);
      gen_operand(node->right, "rax");
      printf("  movq $%d, %%rcx\n", size);
      printf("  mulq %%rcx\n");
      gen_pop("rcx");
      printf("  addq %%rax, %%rcx\n");
      gen_push("rcx");
    }
  }

  if (node->type == SUB) {
    Type *left_type = node->left->value_type;
    Type *right_type = node->right->value_type;

    if (left_type->type == INT && right_type->type == INT) {
      gen_operands(node->left, node->right, "rax", "rcx");
      printf("  subl %%ecx, %%eax\n");
      gen_push("rax");
    } else if (left_type->type == POINTER && right_type->type == INT) {
      int size = left_type->pointer_to->size;
      gen_expr(node->left);
      gen_operand(node->right, "rax");
      printf("  movq $%d, %%rcx\n", size);
      printf("  mulq %%rcx\n");
      gen_pop("rcx");
      printf("  subq %%rax, %%rcx\n");
      gen_push("rcx");
    }
  }

  if (node->type == LSHIFT) {
    gen_operands(node->left, node->right, "rax", "rcx");
    printf("  sall %%cl, %%eax\n");
    gen_push("rax");
  }

  if (node->type == RSHIFT) {
    gen_operands(node->left, node->right, "rax", "rcx");
    printf("  sarl %%cl, %%eax\n");
    gen_push("rax");
  }

  if (node->type == LT) {
    gen_operands(node->left, node->right, "rax", "rcx");
    printf("  cmpl %%ecx, %%eax\n");
    printf("  setl %%al\n");
    printf("  movzbl %%al, %%eax\n");
    gen_push("rax");
  }

  if (node->type == GT) {
    gen_operands(node->left, node->right, "rax", "rcx");
    printf("  cmpl %%ecx, %%eax\n");
    printf("  setg %%al\n");
    printf("  movzbl %%al, %%eax\n");
    gen_push("rax");
  }

  if (node->type == LTE) {
    gen_operands(node->left, node->right, "rax", "rcx");
    printf("  cmpl %%ecx, %%eax\n");
    printf("  setle %%al\n");
    printf("  movzbl %%al, %%eax\n");
    gen_push("rax");
  }

  if (node->type == GTE) {
    gen_operands(node->left, node->right, "rax", "rcx");
    printf("  cmpl %%ecx, %%eax\n");
    printf("  setge %%al\n");
    printf("  movzbl %%al, %%eax\n");
    gen_push("rax");
  }

  if (node->type == EQ) {
    gen_operands(node->left, node->right, "rax", "rcx");
    printf("  cmpl %%ecx, %%eax\n");
    printf("  sete %%al\n");
    printf("  movzbl %%al, %%eax\n");
    gen_push("rax");
  }

  if (node->type == NEQ) {
    gen_operands(node->left, node->right, "rax", "rcx");
    printf("  cmpl %%ecx, %%eax\n");
    printf("  setne %%al\n");
    printf("  movzbl %%al, %%eax\n");
    gen_push("rax");
  }

  if (node->type == AND) {
    gen_operands(node->left, node->right, "rax", "rcx");
    printf("  andl %%ecx, %%eax\n");
    gen_push("rax");
  }

  if (node->type == XOR) {
    gen_operands(node->left, node->right, "rax", "rcx");
    printf("  xorl %%ecx, %%eax\n");
    gen_push("rax");
  }

  if (node->type == OR) {
    gen_operands(node->left, node->right, "rax", "rcx");
    printf("  orl %%ecx, %%eax\n");
    gen_push("rax");
  }

  if (node->type == LAND) {
    int label_false = label_no++;
    int label_end = label_no++;

    gen_operand(node->left, "rax");
    printf("  cmpl $0, %%eax\n");
    gen_jump("je", label_false);
    gen_operand(node->right, "rax");
    printf("  cmpl $0, %%eax\n");
    gen_jump("je", label_false);
    printf("  movl $1, %%eax\n");
    gen_jump("jmp", label_end);
    gen_label(label_false);
    printf("  movl $0, %%eax\n");
    gen_label(label_end);
    gen_push("rax");
  }

  if (node->type == LOR) {
    int label_true = label_no++;
    int label_end = label_no++;

    gen_operand(node->left, "rax");
    printf("  cmpl $0, %%eax\n");
    gen_jump("jne", label_true);
    gen_operand(node->right, "rax");
    printf("  cmpl $0, %%eax\n");
    gen_jump("jne", label_true);
    printf("  movl $0, %%eax\n");
    gen_jump("jmp", label_end);
    gen_label(label_true);
    printf("  movl $1, %%eax\n");
    gen_label(label_end);
    gen_push("rax");
  }

  if (node->type == CONDITION) {
    int label_false = label_no++;
    int label_end = label_no++;

    gen_operand(node->control, "rax");
    printf("  cmpl $0, %%eax\n");
    gen_jump("je", label_false);
    gen_expr(node->left);
    gen_jump("jmp", label_end);
    gen_label(label_false);
    gen_expr(node->right);
    gen_label(label_end);
  }

  if (node->type == ASSIGN) {
    gen_lvalue(node->left);
    gen_operand(node->right, "rcx");
    gen_pop("rax");
    if (node->left->value_type->type == INT) {
      printf("  movl %%ecx, (%%rax)\n");
    } else if (node->left->value_type->type == POINTER) {
      printf("  movq %%rcx, (%%rax)\n");
    } else {
      error("invalid assignment.");
    }
    gen_push("rcx");
  }
}

void gen_stmt(Node *node) {
  if (node->type == COMP_STMT) {
    for (int i = 0; i < node->statements->length; i++) {
      gen_stmt((Node *) node->statements->array[i]);
    }
  }

  if (node->type == EXPR_STMT) {
    gen_expr(node->expression);
    printf("  addq $8, %%rsp\n");
  }

  if (node->type == IF_STMT) {
    int label_end = label_no++;

    gen_operand(node->control, "rax");
    printf("  cmpl $0, %%eax\n");
    gen_jump("je", label_end);
    gen_stmt(node->if_body);
    gen_label(label_end);
  }

  if (node->type == IF_ELSE_STMT) {
    int label_else = label_no++;
    int label_end = label_no++;

    gen_operand(node->control, "rax");
    printf("  cmpl $0, %%eax\n");
    gen_jump("je", label_else);
    gen_stmt(node->if_body);
    gen_jump("jmp", label_end);
    gen_label(label_else);
    gen_stmt(node->else_body);
    gen_label(label_end);
  }

  if (node->type == WHILE_STMT) {
    int label_begin = label_no++;
    int label_end = label_no++;

    vector_push(continue_labels, (void *) &label_begin);
    vector_push(break_labels, (void *) &label_end);

    gen_label(label_begin);
    gen_operand(node->control, "rax");
    printf("  cmpl $0, %%eax\n");
    gen_jump("je", label_end);
    gen_stmt(node->loop_body);
    gen_jump("jmp", label_begin);
    gen_label(label_end);

    vector_pop(continue_labels);
    vector_pop(break_labels);
  }

  if (node->type == DO_WHILE_STMT) {
    int label_begin = label_no++;
    int label_control = label_no++;
    int label_end = label_no++;

    vector_push(continue_labels, (void *) &label_control);
    vector_push(break_labels, (void *) &label_end);

    gen_label(label_begin);
    gen_stmt(node->loop_body);
    gen_label(label_control);
    gen_operand(node->control, "rax");
    printf("  cmpl $0, %%eax\n");
    gen_jump("jne", label_begin);
    gen_label(label_end);

    vector_pop(continue_labels);
    vector_pop(break_labels);
  }

  if (node->type == FOR_STMT) {
    int label_begin = label_no++;
    int label_afterthrough = label_no++;
    int label_end = label_no++;

    vector_push(continue_labels, (void *) &label_afterthrough);
    vector_push(break_labels, (void *) &label_end);

    if (node->init) {
      gen_expr(node->init);
      printf("  addq $8, %%rsp\n");
    }
    gen_label(label_begin);
    if (node->control) {
      gen_operand(node->control, "rax");
      printf("  cmpl $0, %%eax\n");
      gen_jump("je", label_end);
    }
    gen_stmt(node->loop_body);
    gen_label(label_afterthrough);
    if (node->afterthrough) {
      gen_expr(node->afterthrough);
      printf("  addq $8, %%rsp\n");
    }
    gen_jump("jmp", label_begin);
    gen_label(label_end);

    vector_pop(continue_labels);
    vector_pop(break_labels);
  }

  if (node->type == CONTINUE_STMT) {
    if (continue_labels->length == 0) {
      error("invalid continue statement.");
    }
    gen_jump("jmp", *(int *) continue_labels->array[continue_labels->length - 1]);
  }

  if (node->type == BREAK_STMT) {
    if (break_labels->length == 0) {
      error("invalid break statement.");
    }
    gen_jump("jmp", *(int *) break_labels->array[break_labels->length - 1]);
  }

  if (node->type == RETURN_STMT) {
    if (node->expression) {
      gen_operand(node->expression, "rax");
    }
    gen_jump("jmp", return_label);
  }
}

void gen_func_def(Node *node) {
  if (strcmp(node->identifier, "main") == 0) {
    printf("  .global main\n");
  }

  return_label = label_no++;

  printf("%s:\n", node->identifier);
  gen_push("rbp");
  printf("  movq %%rsp, %%rbp\n");
  printf("  subq $%d, %%rsp\n", node->local_vars_size);

  for (int i = 0; i < node->params->length; i++) {
    Symbol *symbol = (Symbol *) node->params->array[i];
    if (symbol->value_type->type == INT) {
      printf("  movq %%%s, %%rax\n", arg_reg[i]);
      printf("  movl %%eax, %d(%%rbp)\n", -symbol->offset);
    } else if (symbol->value_type->type == POINTER) {
      printf("  movq %%%s, %d(%%rbp)\n", arg_reg[i], -symbol->offset);
    }
  }

  gen_stmt(node->function_body);

  gen_label(return_label);
  printf("  addq $%d, %%rsp\n", node->local_vars_size);
  gen_pop("rbp");
  printf("  ret\n");
}

void gen_trans_unit(Node *node) {
  for (int i = 0; i < node->definitions->length; i++) {
    gen_func_def((Node *) node->definitions->array[i]);
  }
}

void gen(Node *node) {
  continue_labels = vector_new();
  break_labels = vector_new();

  gen_trans_unit(node);
}
