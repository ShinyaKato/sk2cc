#include "cc.h"

char arg_reg[6][4] = { "rdi", "rsi", "rdx", "rcx", "r8", "r9" };

int label_no = 0;
Vector *continue_labels, *break_labels;
int return_label;

void gen_immediate(int value) {
  printf("  sub $8, %%rsp\n");
  printf("  movl $%d, 0(%%rsp)\n", value);
}

void gen_push(char *reg) {
  printf("  sub $8, %%rsp\n");
  printf("  movl %%%s, 0(%%rsp)\n", reg);
}

void gen_pop(char *reg) {
  if(reg) printf("  movl 0(%%rsp), %%%s\n", reg);
  printf("  add $8, %%rsp\n");
}

void gen_jump(char *inst, int label) {
  printf("  %s .L%d\n", inst, label);
}

void gen_label(int label) {
  printf(".L%d:\n", label);
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
    Symbol *symbol = node->symbol;
    if (!symbol) {
      error("undefined identifier.");
    }

    if (node->value_type->type == INT) {
      printf("  movl %d(%%rbp), %%eax\n", -symbol->offset);
      gen_push("eax");
    } else if (node->value_type->type == POINTER) {
      printf("  mov %d(%%rbp), %%rax\n", -symbol->offset);
      printf("  push %%rax\n");
    }
  }

  if (node->type == FUNC_CALL) {
    for (int i = 0; i < node->args->length; i++) {
      gen_expr((Node *) node->args->array[i]);
    }

    for (int i = 5; i >= 0; i--) {
      if (i < node->args->length) {
        Node *arg = (Node *) node->args->array[i];
        if (arg->value_type->type == INT) {
          gen_pop("eax");
          printf("  mov %%rax, %%%s\n", arg_reg[i]);
        } else if (arg->value_type->type == POINTER) {
          printf("  pop %%%s\n", arg_reg[i]);
        }
      }
    }

    printf("  call %s\n", node->left->identifier);

    if (node->value_type->type == INT) {
      gen_push("eax");
    } else if (node->value_type->type == POINTER) {
      printf("  push %%rax\n");
    }
  }

  if (node->type == ADDRESS) {
    int offset = node->left->symbol->offset;

    printf("  lea %d(%%rbp), %%rax\n", -offset);
    printf("  push %%rax\n");
  }

  if (node->type == INDIRECT) {
    gen_expr(node->left);
    printf("  pop %%rax\n");
    printf("  movl (%%rax), %%ecx\n");
    gen_push("ecx");
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
    Node *left = node->left;
    Node *right = node->right;
    Type *left_type = left->value_type;
    Type *right_type = right->value_type;

    if (left_type->type == INT && right_type->type == INT) {
      gen_operands(node->left, node->right, "eax", "ecx");
      printf("  addl %%ecx, %%eax\n");
      gen_push("eax");
    }

    if (left_type->type == POINTER && right_type->type == INT) {
      int size;
      if (left_type->pointer_of->type == INT) size = 4;
      else if (left_type->pointer_of->type == POINTER) size = 8;
      else error("cannot identify size of pointer_of.");

      gen_expr(node->left);
      gen_operand(node->right, "eax");
      printf("  mov $%d, %%rdx\n", size);
      printf("  mul %%rdx\n");
      printf("  pop %%rcx\n");
      printf("  add %%rax, %%rcx\n");
      printf("  push %%rcx\n");
    }
  }

  if (node->type == SUB) {
    Node *left = node->left;
    Node *right = node->right;
    Type *left_type = left->value_type;
    Type *right_type = right->value_type;

    if (left_type->type == INT && right_type->type == INT) {
      gen_operands(node->left, node->right, "eax", "ecx");
      printf("  subl %%ecx, %%eax\n");
      gen_push("eax");
    }

    if (left_type->type == POINTER && right_type->type == INT) {
      int size;
      if (left_type->pointer_of->type == INT) size = 4;
      else if (left_type->pointer_of->type == POINTER) size = 8;
      else error("cannot identify size of pointer_of.");

      gen_expr(node->left);
      gen_operand(node->right, "eax");
      printf("  mov $%d, %%rdx\n", size);
      printf("  mul %%rdx\n");
      printf("  pop %%rcx\n");
      printf("  sub %%rax, %%rcx\n");
      printf("  push %%rcx\n");
    }
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
    gen_jump("je", label_false);
    gen_operand(node->right, "eax");
    printf("  cmpl $0, %%eax\n");
    gen_jump("je", label_false);
    printf("  movl $1, %%eax\n");
    gen_jump("jmp", label_end);
    gen_label(label_false);
    printf("  movl $0, %%eax\n");
    gen_label(label_end);
    gen_push("eax");
  }

  if (node->type == LOR) {
    int label_true = label_no++;
    int label_end = label_no++;

    gen_operand(node->left, "eax");
    printf("  cmpl $0, %%eax\n");
    gen_jump("jne", label_true);
    gen_operand(node->right, "eax");
    printf("  cmpl $0, %%eax\n");
    gen_jump("jne", label_true);
    printf("  movl $0, %%eax\n");
    gen_jump("jmp", label_end);
    gen_label(label_true);
    printf("  movl $1, %%eax\n");
    gen_label(label_end);
    gen_push("eax");
  }

  if (node->type == CONDITION) {
    int label_false = label_no++;
    int label_end = label_no++;

    gen_operand(node->control, "eax");
    printf("  cmpl $0, %%eax\n");
    gen_jump("je", label_false);
    gen_expr(node->left);
    gen_jump("jmp", label_end);
    gen_label(label_false);
    gen_expr(node->right);
    gen_label(label_end);
  }

  if (node->type == ASSIGN) {
    if (node->left->type == IDENTIFIER) {
      Symbol *symbol = node->left->symbol;

      if (node->left->value_type->type == INT) {
        gen_operand(node->right, "eax");
        printf("  movl %%eax, %d(%%rbp)\n", -symbol->offset);
        gen_push("eax");
      }

      if (node->left->value_type->type == POINTER) {
        gen_expr(node->right);
        printf("  pop %%rax\n");
        printf("  mov %%rax, %d(%%rbp)\n", -symbol->offset);
        printf("  push %%rax\n");
      }
    }

    if (node->left->type == INDIRECT) {
      if (node->left->left->value_type->type == INT) {
        gen_expr(node->left->left);
        gen_operand(node->right, "eax");
        printf("  pop %%rcx\n");
        printf("  movl %%eax, (%%rcx)\n");
        gen_push("eax");
      }

      if (node->left->left->value_type->type == POINTER) {
        gen_expr(node->left->left);
        gen_expr(node->right);
        printf("  pop %%rax\n");
        printf("  pop %%rcx\n");
        printf("  mov %%rax, (%%rcx)\n");
        printf("  push %%rax\n");
      }
    }
  }
}

void gen_stmt(Node *node) {
  if (node->type == COMP_STMT) {
    for (int i = 0; i < node->statements->length; i++) {
      gen_stmt((Node *) node->statements->array[i]);
    }
  }

  if (node->type == EXPR_STMT) {
    gen_operand(node->expression, NULL);
  }

  if (node->type == IF_STMT) {
    int label_end = label_no++;

    gen_operand(node->control, "eax");
    printf("  cmpl $0, %%eax\n");
    gen_jump("je", label_end);
    gen_stmt(node->if_body);
    gen_label(label_end);
  }

  if (node->type == IF_ELSE_STMT) {
    int label_else = label_no++;
    int label_end = label_no++;

    gen_operand(node->control, "eax");
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
    gen_operand(node->control, "eax");
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
    gen_operand(node->control, "eax");
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
      gen_operand(node->init, NULL);
    }
    gen_label(label_begin);
    if (node->control) {
      gen_operand(node->control, "eax");
      printf("  cmpl $0, %%eax\n");
      gen_jump("je", label_end);
    }
    gen_stmt(node->loop_body);
    gen_label(label_afterthrough);
    if (node->afterthrough) {
      gen_operand(node->afterthrough, NULL);
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
    Node *expr = node->expression;
    if (expr) {
      if (expr->value_type->type == INT) {
        gen_operand(expr, "eax");
      } else if (expr->value_type->type == POINTER) {
        gen_expr(expr);
        printf("  pop %%rax\n");
      }
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
  printf("  push %%rbp\n");
  printf("  mov %%rsp, %%rbp\n");
  printf("  sub $%d, %%rsp\n", node->local_vars_size);

  for (int i = 0; i < node->params->length; i++) {
    Symbol *symbol = (Symbol *) node->params->array[i];
    if (symbol->value_type->type == INT) {
      printf("  mov %%%s, %%rax\n", arg_reg[i]);
      printf("  movl %%eax, %d(%%rbp)\n", -symbol->offset);
    } else if (symbol->value_type->type == POINTER) {
      printf("  mov %%%s, %d(%%rbp)\n", arg_reg[i], -symbol->offset);
    }
  }

  gen_stmt(node->function_body);

  gen_label(return_label);
  printf("  add $%d, %%rsp\n", node->local_vars_size);
  printf("  pop %%rbp\n");
  printf("  ret\n");
}

void gen_trans_unit(Node *node) {
  for (int i = 0; i < node->definitions->length; i++) {
    gen_func_def((Node *) node->definitions->array[i]);

    if (i + 1 < node->definitions->length) {
      printf("\n");
    }
  }
}

void gen(Node *node) {
  continue_labels = vector_new();
  break_labels = vector_new();

  gen_trans_unit(node);
}
