#include "sk2cc.h"

char *arg_reg[] = { "rdi", "rsi", "rdx", "rcx", "r8", "r9" };

int label_no, return_label;
Vector *continue_labels, *break_labels;
Symbol *function_symbol;

int stack_depth;

void gen_push(char *reg) {
  stack_depth += 8;
  printf("  pushq %%%s\n", reg);
}

void gen_pop(char *reg) {
  stack_depth -= 8;
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
    if (symbol->type == GLOBAL) {
      printf("  leaq %s(%%rip), %%rax\n", symbol->identifier);
    } else if (symbol->type == LOCAL) {
      printf("  leaq %d(%%rbp), %%rax\n", -symbol->offset);
    }
    gen_push("rax");
  } else if (node->type == INDIRECT) {
    gen_expr(node->expr);
  } else if (node->type == DOT) {
    gen_lvalue(node->expr);
    gen_pop("rax");
    printf("  leaq %d(%%rax), %%rax\n", node->member_offset);
    gen_push("rax");
  }
}

void gen_load(Type *type) {
  gen_pop("rax");
  if (type->type == BOOL) {
    printf("  movzbl (%%rax), %%eax\n");
  } else if (type->type == CHAR || type->type == UCHAR) {
    printf("  movsbl (%%rax), %%eax\n");
  } else if (type->type == SHORT || type->type == USHORT) {
    printf("  movswl (%%rax), %%eax\n");
  } else if (type->type == INT || type->type == UINT) {
    printf("  movl (%%rax), %%eax\n");
  } else if (type->type == DOUBLE) {
    printf("  movq (%%rax), %%rax\n");
  } else if (type->type == POINTER && !type->array_pointer) {
    printf("  movq (%%rax), %%rax\n");
  }
  gen_push("rax");
}

void gen_store(Type *type) {
  gen_pop("rax");
  gen_pop("rcx");
  if (type->type == BOOL) {
    printf("  cmpl $0, %%eax\n");
    printf("  setne %%al\n");
    printf("  movb %%al, (%%rcx)\n");
  } else if (type->type == CHAR || type->type == UCHAR) {
    printf("  movb %%al, (%%rcx)\n");
  } else if (type->type == SHORT || type->type == USHORT) {
    printf("  movw %%ax, (%%rcx)\n");
  } else if (type->type == INT || type->type == UINT) {
    printf("  movl %%eax, (%%rcx)\n");
  } else if (type->type == DOUBLE) {
    printf("  movq %%rax, (%%rcx)\n");
  } else if (type->type == POINTER) {
    printf("  movq %%rax, (%%rcx)\n");
  }
  gen_push("rax");
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

void gen_int_const(Node *node) {
  printf("  movl $%d, %%eax\n", node->int_value);
  gen_push("rax");
}

void gen_float_const(Node *node) {
  int *d = (int *) &node->double_value;
  printf("  movq $0x%08x%08x, %%rax\n", d[1], d[0]);
  gen_push("rax");
}

void gen_string_literal(Node *node) {
  printf("  leaq .S%d(%%rip), %%rax\n", node->string_label);
  gen_push("rax");
}

void gen_identifier(Node *node) {
  gen_lvalue(node);
  gen_load(node->value_type);
}

void gen_func_call(Node *node) {
  if (strcmp(node->expr->identifier, "__builtin_va_start") == 0) {
    Node *list = node->args->buffer[0];
    gen_lvalue(list);
    gen_pop("rdx");
    printf("  movl $%d, (%%rdx)\n", function_symbol->value_type->params->length * 8);
    printf("  movl $48, 4(%%rdx)\n");
    printf("  leaq 16(%%rbp), %%rcx\n");
    printf("  movq %%rcx, 8(%%rdx)\n");
    printf("  leaq -176(%%rbp), %%rcx\n");
    printf("  movq %%rcx, 16(%%rdx)\n");
    printf("  movl $0, %%eax\n");
    gen_push("rax");
    return;
  }
  if (strcmp(node->expr->identifier, "__builtin_va_end") == 0) {
    printf("  movl $0, %%eax\n");
    gen_push("rax");
    return;
  }

  for (int i = node->args->length - 1; i >= 0; i--) {
    Node *arg = node->args->buffer[i];
    gen_expr(arg);
    if (arg->value_type->type == BOOL) {
      gen_pop("rax");
      printf("  cmpl $0, %%eax\n");
      printf("  setne %%al\n");
      printf("  movzbl %%al, %%eax\n");
      gen_push("rax");
    }
  }

  int int_reg = 0, float_reg = 0;
  for (int i = 0; i < node->args->length; i++) {
    Node *arg = node->args->buffer[i];
    if (arg->value_type->type == DOUBLE) {
      gen_pop("rax");
      printf("  movq %%rax, %%xmm%d\n", float_reg++);
    } else {
      gen_pop(arg_reg[int_reg++]);
    }
  }

  int top = stack_depth % 16;
  if (top > 0) {
    printf("  subq $%d, %%rsp\n", 16 - top);
  }
  printf("  movl $%d, %%eax\n", float_reg);
  printf("  call %s@PLT\n", node->expr->identifier);
  if (top > 0) {
    printf("  addq $%d, %%rsp\n", 16 - top);
  }

  if (node->value_type->type == DOUBLE) {
    printf("  movq %%xmm0, %%rax\n");
  }
  if (node->value_type->type == BOOL) {
    printf("  movzbl %%al, %%eax\n");
  }
  gen_push("rax");
}

void gen_dot(Node *node) {
  gen_lvalue(node);
  gen_load(node->value_type);
}

void gen_address(Node *node) {
  gen_lvalue(node->expr);
}

void gen_indirect(Node *node) {
  gen_lvalue(node);
  gen_load(node->value_type);
}

void gen_uplus(Node *node) {
  gen_operand(node->expr, "rax");
  gen_push("rax");
}

void gen_uminus(Node *node) {
  gen_operand(node->expr, "rax");
  printf("  negl %%eax\n");
  gen_push("rax");
}

void gen_not(Node *node) {
  gen_operand(node->expr, "rax");
  printf("  notl %%eax\n");
  gen_push("rax");
}

void gen_lnot(Node *node) {
  gen_operand(node->expr, "rax");
  printf("  cmpq $0, %%rax\n");
  printf("  sete %%al\n");
  printf("  movzbl %%al, %%eax\n");
  gen_push("rax");
}

void gen_cast(Node *node) {
  gen_expr(node->expr);
}

void gen_mul(Node *node) {
  gen_operands(node->left, node->right, "rax", "rcx");
  printf("  imull %%ecx\n");
  gen_push("rax");
}

void gen_div(Node *node) {
  gen_operands(node->left, node->right, "rax", "rcx");
  printf("  movl $0, %%edx\n");
  printf("  idivl %%ecx\n");
  gen_push("rax");
}

void gen_mod(Node *node) {
  gen_operands(node->left, node->right, "rax", "rcx");
  printf("  movl $0, %%edx\n");
  printf("  idivl %%ecx\n");
  gen_push("rdx");
}

void gen_add(Node *node) {
  if (type_integer(node->left->value_type) && type_integer(node->right->value_type)) {
    gen_operands(node->left, node->right, "rax", "rcx");
    printf("  addl %%ecx, %%eax\n");
    gen_push("rax");
  } else if (type_pointer(node->left->value_type) && type_integer(node->right->value_type)) {
    int size = node->left->value_type->pointer_to->size;
    gen_expr(node->left);
    gen_operand(node->right, "rax");
    printf("  movq $%d, %%rcx\n", size);
    printf("  mulq %%rcx\n");
    gen_pop("rcx");
    printf("  addq %%rax, %%rcx\n");
    gen_push("rcx");
  }
}

void gen_sub(Node *node) {
  if (type_integer(node->left->value_type) && type_integer(node->right->value_type)) {
    gen_operands(node->left, node->right, "rax", "rcx");
    printf("  subl %%ecx, %%eax\n");
    gen_push("rax");
  } else if (type_pointer(node->left->value_type) && type_integer(node->right->value_type)) {
    int size = node->left->value_type->pointer_to->size;
    gen_expr(node->left);
    gen_operand(node->right, "rax");
    printf("  movq $%d, %%rcx\n", size);
    printf("  mulq %%rcx\n");
    gen_pop("rcx");
    printf("  subq %%rax, %%rcx\n");
    gen_push("rcx");
  }
}

void gen_lshift(Node *node) {
  gen_operands(node->left, node->right, "rax", "rcx");
  printf("  sall %%cl, %%eax\n");
  gen_push("rax");
}

void gen_rshift(Node *node) {
  gen_operands(node->left, node->right, "rax", "rcx");
  printf("  sarl %%cl, %%eax\n");
  gen_push("rax");
}

void gen_lt(Node *node) {
  gen_operands(node->left, node->right, "rax", "rcx");
  if (type_integer(node->left->value_type) && type_integer(node->right->value_type)) {
    printf("  cmpl %%ecx, %%eax\n");
  } else if (type_pointer(node->left->value_type) && type_pointer(node->right->value_type)) {
    printf("  cmpq %%rcx, %%rax\n");
  }
  printf("  setl %%al\n");
  printf("  movzbl %%al, %%eax\n");
  gen_push("rax");
}

void gen_gt(Node *node) {
  gen_operands(node->left, node->right, "rax", "rcx");
  if (type_integer(node->left->value_type) && type_integer(node->right->value_type)) {
    printf("  cmpl %%ecx, %%eax\n");
  } else if (type_pointer(node->left->value_type) && type_pointer(node->right->value_type)) {
    printf("  cmpq %%rcx, %%rax\n");
  }
  printf("  setg %%al\n");
  printf("  movzbl %%al, %%eax\n");
  gen_push("rax");
}

void gen_lte(Node *node) {
  gen_operands(node->left, node->right, "rax", "rcx");
  if (type_integer(node->left->value_type) && type_integer(node->right->value_type)) {
    printf("  cmpl %%ecx, %%eax\n");
  } else if (type_pointer(node->left->value_type) && type_pointer(node->right->value_type)) {
    printf("  cmpq %%rcx, %%rax\n");
  }
  printf("  setle %%al\n");
  printf("  movzbl %%al, %%eax\n");
  gen_push("rax");
}

void gen_gte(Node *node) {
  gen_operands(node->left, node->right, "rax", "rcx");
  if (type_integer(node->left->value_type) && type_integer(node->right->value_type)) {
    printf("  cmpl %%ecx, %%eax\n");
  } else if (type_pointer(node->left->value_type) && type_pointer(node->right->value_type)) {
    printf("  cmpq %%rcx, %%rax\n");
  }
  printf("  setge %%al\n");
  printf("  movzbl %%al, %%eax\n");
  gen_push("rax");
}

void gen_eq(Node *node) {
  gen_operands(node->left, node->right, "rax", "rcx");
  if (type_integer(node->left->value_type) && type_integer(node->right->value_type)) {
    printf("  cmpl %%ecx, %%eax\n");
  } else if (type_pointer(node->left->value_type) && type_pointer(node->right->value_type)) {
    printf("  cmpq %%rcx, %%rax\n");
  }
  printf("  sete %%al\n");
  printf("  movzbl %%al, %%eax\n");
  gen_push("rax");
}

void gen_neq(Node *node) {
  gen_operands(node->left, node->right, "rax", "rcx");
  if (type_integer(node->left->value_type) && type_integer(node->right->value_type)) {
    printf("  cmpl %%ecx, %%eax\n");
  } else if (type_pointer(node->left->value_type) && type_pointer(node->right->value_type)) {
    printf("  cmpq %%rcx, %%rax\n");
  }
  printf("  setne %%al\n");
  printf("  movzbl %%al, %%eax\n");
  gen_push("rax");
}

void gen_and(Node *node) {
  gen_operands(node->left, node->right, "rax", "rcx");
  printf("  andl %%ecx, %%eax\n");
  gen_push("rax");
}

void gen_xor(Node *node) {
  gen_operands(node->left, node->right, "rax", "rcx");
  printf("  xorl %%ecx, %%eax\n");
  gen_push("rax");
}

void gen_or(Node *node) {
  gen_operands(node->left, node->right, "rax", "rcx");
  printf("  orl %%ecx, %%eax\n");
  gen_push("rax");
}

void gen_land(Node *node) {
  int label_false = label_no++;
  int label_end = label_no++;

  gen_operand(node->left, "rax");
  printf("  cmpq $0, %%rax\n");
  gen_jump("je", label_false);
  gen_operand(node->right, "rax");
  printf("  cmpq $0, %%rax\n");
  gen_jump("je", label_false);
  printf("  movl $1, %%eax\n");
  gen_jump("jmp", label_end);
  gen_label(label_false);
  printf("  movl $0, %%eax\n");
  gen_label(label_end);
  gen_push("rax");
}

void gen_lor(Node *node) {
  int label_true = label_no++;
  int label_end = label_no++;

  gen_operand(node->left, "rax");
  printf("  cmpq $0, %%rax\n");
  gen_jump("jne", label_true);
  gen_operand(node->right, "rax");
  printf("  cmpq $0, %%rax\n");
  gen_jump("jne", label_true);
  printf("  movl $0, %%eax\n");
  gen_jump("jmp", label_end);
  gen_label(label_true);
  printf("  movl $1, %%eax\n");
  gen_label(label_end);
  gen_push("rax");
}

void gen_condition(Node *node) {
  int label_false = label_no++;
  int label_end = label_no++;

  gen_operand(node->control, "rax");
  printf("  cmpq $0, %%rax\n");
  gen_jump("je", label_false);
  gen_operand(node->left, "rax");
  gen_jump("jmp", label_end);
  gen_label(label_false);
  gen_operand(node->right, "rax");
  gen_label(label_end);
  gen_push("rax");
}

void gen_assign(Node *node) {
  gen_lvalue(node->left);
  gen_expr(node->right);
  gen_store(node->left->value_type);
}

void gen_comma(Node *node) {
  gen_expr(node->left);
  gen_pop("rax");
  gen_expr(node->right);
}

void gen_expr(Node *node) {
  if (node->type == INT_CONST) gen_int_const(node);
  else if (node->type == FLOAT_CONST) gen_float_const(node);
  else if (node->type == STRING_LITERAL) gen_string_literal(node);
  else if (node->type == IDENTIFIER) gen_identifier(node);
  else if (node->type == FUNC_CALL) gen_func_call(node);
  else if (node->type == DOT) gen_dot(node);
  else if (node->type == ADDRESS) gen_address(node);
  else if (node->type == INDIRECT) gen_indirect(node);
  else if (node->type == UPLUS) gen_uplus(node);
  else if (node->type == UMINUS) gen_uminus(node);
  else if (node->type == NOT) gen_not(node);
  else if (node->type == LNOT) gen_lnot(node);
  else if (node->type == CAST) gen_cast(node);
  else if (node->type == MUL) gen_mul(node);
  else if (node->type == DIV) gen_div(node);
  else if (node->type == MOD) gen_mod(node);
  else if (node->type == ADD) gen_add(node);
  else if (node->type == SUB) gen_sub(node);
  else if (node->type == LSHIFT) gen_lshift(node);
  else if (node->type == RSHIFT) gen_rshift(node);
  else if (node->type == LT) gen_lt(node);
  else if (node->type == GT) gen_gt(node);
  else if (node->type == LTE) gen_lte(node);
  else if (node->type == GTE) gen_gte(node);
  else if (node->type == EQ) gen_eq(node);
  else if (node->type == NEQ) gen_neq(node);
  else if (node->type == AND) gen_and(node);
  else if (node->type == XOR) gen_xor(node);
  else if (node->type == OR) gen_or(node);
  else if (node->type == LAND) gen_land(node);
  else if (node->type == LOR) gen_lor(node);
  else if (node->type == CONDITION) gen_condition(node);
  else if (node->type == ASSIGN) gen_assign(node);
  else if (node->type == COMMA) gen_comma(node);
}

void gen_stmt(Node *node);

void gen_var_decl_init(Node *node, int offset) {
  if (node->type == ARRAY_INIT) {
    int size = node->value_type->array_of->size;
    for (int i = 0; i < node->array_init->length; i++) {
      Node *init = node->array_init->buffer[i];
      gen_var_decl_init(init, offset + size * i);
    }
  } else if (node->type == INIT) {
    Node *init = node->init;
    printf("  leaq %d(%%rbp), %%rax\n", offset);
    gen_push("rax");
    gen_expr(init);
    gen_store(init->value_type);
    gen_pop("rax");
  }
}

void gen_var_decl(Node *node) {
  for (int i = 0; i < node->declarations->length; i++) {
    Symbol *symbol = node->declarations->buffer[i];
    if (symbol->initializer) {
      gen_var_decl_init(symbol->initializer, -symbol->offset);
    }
  }
}

void gen_comp_stmt(Node *node) {
  for (int i = 0; i < node->statements->length; i++) {
    Node *stmt = node->statements->buffer[i];
    gen_stmt(stmt);
  }
}

void gen_expr_stmt(Node *node) {
  if (node->expr) {
    gen_expr(node->expr);
    gen_pop("rax");
  }
}

void gen_if_stmt(Node *node) {
  int label_else = label_no++;
  int label_end = label_no++;

  gen_operand(node->if_control, "rax");
  printf("  cmpq $0, %%rax\n");
  gen_jump("je", label_else);
  gen_stmt(node->if_body);
  gen_jump("jmp", label_end);
  gen_label(label_else);
  if (node->else_body) {
    gen_stmt(node->else_body);
  }
  gen_label(label_end);
}

void gen_while_stmt(Node *node) {
  int label_begin = label_no++;
  int label_end = label_no++;

  vector_push(continue_labels, &label_begin);
  vector_push(break_labels, &label_end);

  gen_label(label_begin);
  gen_operand(node->loop_control, "rax");
  printf("  cmpq $0, %%rax\n");
  gen_jump("je", label_end);
  gen_stmt(node->loop_body);
  gen_jump("jmp", label_begin);
  gen_label(label_end);

  vector_pop(continue_labels);
  vector_pop(break_labels);
}

void gen_do_while_stmt(Node *node) {
  int label_begin = label_no++;
  int label_control = label_no++;
  int label_end = label_no++;

  vector_push(continue_labels, &label_control);
  vector_push(break_labels, &label_end);

  gen_label(label_begin);
  gen_stmt(node->loop_body);
  gen_label(label_control);
  gen_operand(node->loop_control, "rax");
  printf("  cmpq $0, %%rax\n");
  gen_jump("jne", label_begin);
  gen_label(label_end);

  vector_pop(continue_labels);
  vector_pop(break_labels);
}

void gen_for_stmt(Node *node) {
  int label_begin = label_no++;
  int label_afterthrough = label_no++;
  int label_end = label_no++;

  vector_push(continue_labels, &label_afterthrough);
  vector_push(break_labels, &label_end);

  gen_stmt(node->loop_init);
  gen_label(label_begin);
  if (node->loop_control) {
    gen_operand(node->loop_control, "rax");
    printf("  cmpq $0, %%rax\n");
    gen_jump("je", label_end);
  }
  gen_stmt(node->loop_body);
  gen_label(label_afterthrough);
  gen_stmt(node->loop_afterthrough);
  gen_jump("jmp", label_begin);
  gen_label(label_end);

  vector_pop(continue_labels);
  vector_pop(break_labels);
}

void gen_continue_stmt(Node *node) {
  int *label = continue_labels->buffer[continue_labels->length - 1];
  gen_jump("jmp", *label);
}

void gen_break_stmt(Node *node) {
  int *label = break_labels->buffer[break_labels->length - 1];
  gen_jump("jmp", *label);
}

void gen_return_stmt(Node *node) {
  if (node->expr) {
    gen_operand(node->expr, "rax");
  }
  gen_jump("jmp", return_label);
}

void gen_stmt(Node *node) {
  if (node->type == VAR_DECL) gen_var_decl(node);
  else if (node->type == COMP_STMT) gen_comp_stmt(node);
  else if (node->type == EXPR_STMT) gen_expr_stmt(node);
  else if (node->type == IF_STMT) gen_if_stmt(node);
  else if (node->type == WHILE_STMT) gen_while_stmt(node);
  else if (node->type == DO_WHILE_STMT) gen_do_while_stmt(node);
  else if (node->type == FOR_STMT) gen_for_stmt(node);
  else if (node->type == CONTINUE_STMT) gen_continue_stmt(node);
  else if (node->type == BREAK_STMT) gen_break_stmt(node);
  else if (node->type == RETURN_STMT) gen_return_stmt(node);
}

void gen_func_def(Node *node) {
  function_symbol = node->symbol;

  return_label = label_no++;
  stack_depth = 8;

  printf("  .global %s\n", node->symbol->identifier);
  printf("%s:\n", node->symbol->identifier);

  gen_push("rbp");
  printf("  movq %%rsp, %%rbp\n");
  printf("  subq $%d, %%rsp\n", node->local_vars_size);
  stack_depth += node->local_vars_size;

  Type *type = node->symbol->value_type;
  int int_param = 0, double_param = 0;
  for (int i = 0; i < type->params->length; i++) {
    Symbol *symbol = (Symbol *) type->params->buffer[i];
    if (symbol->value_type->type == BOOL) {
      printf("  movq %%%s, %%rax\n", arg_reg[int_param++]);
      printf("  cmpb $0, %%al\n");
      printf("  setne %%al\n");
      printf("  movb %%al, %d(%%rbp)\n", -symbol->offset);
    } else if (symbol->value_type->type == CHAR || symbol->value_type->type == UCHAR) {
      printf("  movq %%%s, %%rax\n", arg_reg[int_param++]);
      printf("  movb %%al, %d(%%rbp)\n", -symbol->offset);
    } else if (symbol->value_type->type == SHORT || symbol->value_type->type == USHORT) {
      printf("  movq %%%s, %%rax\n", arg_reg[int_param++]);
      printf("  movw %%ax, %d(%%rbp)\n", -symbol->offset);
    } else if (symbol->value_type->type == INT || symbol->value_type->type == UINT) {
      printf("  movq %%%s, %%rax\n", arg_reg[int_param++]);
      printf("  movl %%eax, %d(%%rbp)\n", -symbol->offset);
    } else if (symbol->value_type->type == DOUBLE) {
      printf("  movsd %%xmm%d, %d(%%rbp)\n", double_param++, -symbol->offset);
    } else if (symbol->value_type->type == POINTER) {
      printf("  movq %%%s, %%rax\n", arg_reg[int_param++]);
      printf("  movq %%rax, %d(%%rbp)\n", -symbol->offset);
    }
  }

  if (node->symbol->value_type->ellipsis) {
    for (int i = type->params->length; i < 6; i++) {
      printf("  movq %%%s, %d(%%rbp)\n", arg_reg[i], -176 + i * 8);
    }
  }

  gen_stmt(node->function_body);

  gen_label(return_label);
  if (type->function_returning->type == BOOL) {
    printf("  cmpl $0, %%eax\n");
    printf("  setne %%al\n");
  }
  if (type->function_returning->type == DOUBLE) {
    printf("  movq %%rax, %%xmm0\n");
  }
  printf("  leave\n");
  printf("  ret\n");
}

void gen_gvar_init(Node *node) {
  if (node->type == ARRAY_INIT) {
    Type *type = node->value_type;
    int length = node->array_init->length;
    int padding = type->size - type->array_of->size * length;
    for (int i = 0; i < length; i++) {
      gen_gvar_init(node->array_init->buffer[i]);
    }
    if (padding > 0) {
      printf("  .zero %d\n", padding);
    }
  } else if (node->type == INIT) {
    if (node->init->type == INT_CONST) {
      printf("  .long %d\n", node->init->int_value);
    } else if (node->init->type == STRING_LITERAL) {
      printf("  .quad .S%d\n", node->init->string_label);
    }
  }
}

void gen_gvar_decl(Vector *declarations) {
  printf("  .data\n");
  for (int i = 0; i < declarations->length; i++) {
    Symbol *symbol = declarations->buffer[i];
    printf("  .global %s\n", symbol->identifier);
    printf("%s:\n", symbol->identifier);
    if (symbol->initializer) {
      gen_gvar_init(symbol->initializer);
    } else {
      printf("  .zero %d\n", symbol->value_type->size);
    }
  }
}

void gen_trans_unit(Node *node) {
  if (node->string_literals->length > 0) {
    printf("  .text\n");
    for (int i = 0; i < node->string_literals->length; i++) {
      String *string_value = node->string_literals->buffer[i];
      printf(".S%d:\n", i);
      printf("  .ascii \"");
      for (int j = 0; j < string_value->length; j++) {
        char c = string_value->buffer[j];
        if (c == '\\') printf("\\\\");
        else if (c == '"') printf("\\\"");
        else if (c == '\a') printf("\\a");
        else if (c == '\b') printf("\\b");
        else if (c == '\f') printf("\\f");
        else if (c == '\n') printf("\\n");
        else if (c == '\r') printf("\\r");
        else if (c == '\v') printf("\\v");
        else if (c == '\t') printf("\\t");
        else if (c == '\0') printf("\\0");
        else if (isprint(c)) printf("%c", c);
        else printf("\\%03o", c);
      }
      printf("\"\n");
    }
  }

  if (node->declarations->length > 0) {
    gen_gvar_decl(node->declarations);
  }

  if (node->definitions->length > 0) {
    printf("  .text\n");
    for (int i = 0; i < node->definitions->length; i++) {
      Node *func_def = node->definitions->buffer[i];
      gen_func_def(func_def);
    }
  }
}

void gen(Node *node) {
  label_no = 0;

  continue_labels = vector_new();
  break_labels = vector_new();

  gen_trans_unit(node);
}
