#include "sk2cc.h"

char *arg_reg[] = { "rdi", "rsi", "rdx", "rcx", "r8", "r9" };

Vector *continue_labels, *break_labels;

int label_no;
int return_label;

int gp_offset;
int stack_depth;

void begin_loop_gen(int *label_continue, int *label_break) {
  vector_push(continue_labels, label_continue);
  vector_push(break_labels, label_break);
}

void end_loop_gen() {
  vector_pop(continue_labels);
  vector_pop(break_labels);
}

void gen_push(char *reg) {
  stack_depth += 8;
  printf("  pushq %%%s\n", reg);
}

void gen_pop(char *reg) {
  stack_depth -= 8;
  if (reg) {
    printf("  popq %%%s\n", reg);
  } else {
    printf("  addq $8, %%rsp\n");
  }
}

void gen_jump(char *inst, int label) {
  printf("  %s .L%d\n", inst, label);
}

void gen_label(int label) {
  printf(".L%d:\n", label);
}

void gen_expr(Expr *node);

void gen_lvalue(Expr *node) {
  if (node->nd_type == ND_IDENTIFIER) {
    Symbol *symbol = node->symbol;
    if (symbol->link == LN_EXTERNAL) {
      printf("  leaq %s(%%rip), %%rax\n", symbol->identifier);
    } else if (symbol->link == LN_NONE) {
      printf("  leaq %d(%%rbp), %%rax\n", -symbol->offset);
    }
    gen_push("rax");
  } else if (node->nd_type == ND_INDIRECT) {
    gen_expr(node->expr);
  } else if (node->nd_type == ND_DOT) {
    gen_lvalue(node->expr);
    gen_pop("rax");
    printf("  leaq %d(%%rax), %%rax\n", node->offset);
    gen_push("rax");
  }
}

void gen_load(Type *type) {
  gen_pop("rax");
  if (type->ty_type == TY_BOOL) {
    printf("  movb (%%rax), %%al\n");
  } else if (type->ty_type == TY_CHAR || type->ty_type == TY_UCHAR) {
    printf("  movb (%%rax), %%al\n");
  } else if (type->ty_type == TY_SHORT || type->ty_type == TY_USHORT) {
    printf("  movw (%%rax), %%ax\n");
  } else if (type->ty_type == TY_INT || type->ty_type == TY_UINT) {
    printf("  movl (%%rax), %%eax\n");
  } else if (type->ty_type == TY_POINTER && type == type->original) {
    printf("  movq (%%rax), %%rax\n");
  }
  gen_push("rax");
}

void gen_store(Type *type) {
  gen_pop("rax");
  gen_pop("rbx");
  if (type->ty_type == TY_BOOL) {
    printf("  movb %%al, (%%rbx)\n");
  } else if (type->ty_type == TY_CHAR || type->ty_type == TY_UCHAR) {
    printf("  movb %%al, (%%rbx)\n");
  } else if (type->ty_type == TY_SHORT || type->ty_type == TY_USHORT) {
    printf("  movw %%ax, (%%rbx)\n");
  } else if (type->ty_type == TY_INT || type->ty_type == TY_UINT) {
    printf("  movl %%eax, (%%rbx)\n");
  } else if (type->ty_type == TY_POINTER) {
    printf("  movq %%rax, (%%rbx)\n");
  }
  gen_push("rax");
}

void gen_operand(Expr *node, char *reg) {
  gen_expr(node);
  gen_pop(reg);
}

void gen_operands(Expr *lhs, Expr *rhs, char *reg_lhs, char *reg_rhs) {
  gen_expr(lhs);
  gen_expr(rhs);
  gen_pop(reg_rhs);
  gen_pop(reg_lhs);
}

void gen_identifier(Expr *node) {
  gen_lvalue(node);
  gen_load(node->type);
}

void gen_integer(Expr *node) {
  printf("  movl $%d, %%eax\n", node->int_value);
  gen_push("rax");
}

void gen_string(Expr *node) {
  printf("  leaq .S%d(%%rip), %%rax\n", node->string_label);
  gen_push("rax");
}

void gen_call(Expr *node) {
  if (strcmp(node->expr->identifier, "__builtin_va_start") == 0) {
    Expr *list = node->args->buffer[0];
    gen_lvalue(list);
    gen_pop("rdx");
    printf("  movl $%d, (%%rdx)\n", gp_offset);
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

  // generate arguments
  for (int i = node->args->length - 1; i >= 0; i--) {
    Expr *arg = node->args->buffer[i];
    gen_expr(arg);
  }
  for (int i = 0; i < node->args->length; i++) {
    gen_pop(arg_reg[i]);
  }

  // 16-byte alignment
  int top = stack_depth % 16;
  if (top > 0) {
    printf("  subq $%d, %%rsp\n", 16 - top);
  }

  // for function with variable length arguments
  if (!node->expr->symbol || node->expr->symbol->type->ellipsis) {
    printf("  movb $0, %%al\n");
  }

  printf("  call %s@PLT\n", node->expr->identifier);

  // restore rsp
  if (top > 0) {
    printf("  addq $%d, %%rsp\n", 16 - top);
  }

  gen_push("rax");
}

void gen_dot(Expr *node) {
  gen_lvalue(node);
  gen_load(node->type);
}

void gen_address(Expr *node) {
  gen_lvalue(node->expr);
}

void gen_indirect(Expr *node) {
  gen_lvalue(node);
  gen_load(node->type);
}

void gen_uplus(Expr *node) {
  gen_operand(node->expr, "rax");
  gen_push("rax");
}

void gen_uminus(Expr *node) {
  gen_operand(node->expr, "rax");
  printf("  negl %%eax\n");
  gen_push("rax");
}

void gen_not(Expr *node) {
  gen_operand(node->expr, "rax");
  printf("  notl %%eax\n");
  gen_push("rax");
}

void gen_lnot(Expr *node) {
  gen_operand(node->expr, "rax");
  printf("  cmpq $0, %%rax\n");
  printf("  sete %%al\n");
  printf("  movzbl %%al, %%eax\n");
  gen_push("rax");
}

void gen_cast(Expr *node) {
  gen_operand(node->expr, "rax");

  Type *to = node->type;
  Type *from = node->expr->type;

  if (to->ty_type == TY_BOOL) {
    if (from->ty_type == TY_CHAR || from->ty_type == TY_UCHAR) {
      printf("  cmpb $0, %%al\n");
      printf("  setne %%al\n");
    } else if (from->ty_type == TY_SHORT || from->ty_type == TY_USHORT) {
      printf("  cmpw $0, %%ax\n");
      printf("  setne %%al\n");
    } else if (from->ty_type == TY_INT || from->ty_type == TY_UINT) {
      printf("  cmpl $0, %%eax\n");
      printf("  setne %%al\n");
    }
  } else if (to->ty_type == TY_SHORT || to->ty_type == TY_USHORT) {
    if (from->ty_type == TY_BOOL) {
      printf("  movzbw %%al, %%ax\n");
    } else if (from->ty_type == TY_CHAR) {
      printf("  movsbw %%al, %%ax\n");
    } else if (from->ty_type == TY_UCHAR) {
      printf("  movzbw %%al, %%ax");
    }
  } else if (to->ty_type == TY_INT || to->ty_type == TY_UINT) {
    if (from->ty_type == TY_BOOL) {
      printf("  movzbl %%al, %%eax\n");
    } else if (from->ty_type == TY_CHAR) {
      printf("  movsbl %%al, %%eax\n");
    } else if (from->ty_type == TY_UCHAR) {
      printf("  movzbl %%al, %%eax\n");
    } else if (from->ty_type == TY_SHORT) {
      printf("  movswl %%ax, %%eax\n");
    } else if (from->ty_type == TY_USHORT) {
      printf("  movzwl %%ax, %%eax\n");
    }
  }

  gen_push("rax");
}

void gen_mul(Expr *node) {
  gen_operands(node->lhs, node->rhs, "rax", "rcx");
  printf("  imull %%ecx\n");
  gen_push("rax");
}

void gen_div(Expr *node) {
  gen_operands(node->lhs, node->rhs, "rax", "rcx");
  printf("  movl $0, %%edx\n");
  printf("  idivl %%ecx\n");
  gen_push("rax");
}

void gen_mod(Expr *node) {
  gen_operands(node->lhs, node->rhs, "rax", "rcx");
  printf("  movl $0, %%edx\n");
  printf("  idivl %%ecx\n");
  gen_push("rdx");
}

void gen_add(Expr *node) {
  if (check_integer(node->lhs->type) && check_integer(node->rhs->type)) {
    gen_operands(node->lhs, node->rhs, "rax", "rcx");
    printf("  addl %%ecx, %%eax\n");
    gen_push("rax");
  } else if (check_pointer(node->lhs->type) && check_integer(node->rhs->type)) {
    int size = node->lhs->type->pointer_to->size;
    gen_expr(node->lhs);
    gen_operand(node->rhs, "rax");
    printf("  movq $%d, %%rcx\n", size);
    printf("  mulq %%rcx\n");
    gen_pop("rcx");
    printf("  addq %%rax, %%rcx\n");
    gen_push("rcx");
  }
}

void gen_sub(Expr *node) {
  if (check_integer(node->lhs->type) && check_integer(node->rhs->type)) {
    gen_operands(node->lhs, node->rhs, "rax", "rcx");
    printf("  subl %%ecx, %%eax\n");
    gen_push("rax");
  } else if (check_pointer(node->lhs->type) && check_integer(node->rhs->type)) {
    int size = node->lhs->type->pointer_to->size;
    gen_expr(node->lhs);
    gen_operand(node->rhs, "rax");
    printf("  movq $%d, %%rcx\n", size);
    printf("  mulq %%rcx\n");
    gen_pop("rcx");
    printf("  subq %%rax, %%rcx\n");
    gen_push("rcx");
  }
}

void gen_lshift(Expr *node) {
  gen_operands(node->lhs, node->rhs, "rax", "rcx");
  printf("  sall %%cl, %%eax\n");
  gen_push("rax");
}

void gen_rshift(Expr *node) {
  gen_operands(node->lhs, node->rhs, "rax", "rcx");
  printf("  sarl %%cl, %%eax\n");
  gen_push("rax");
}

void gen_lt(Expr *node) {
  gen_operands(node->lhs, node->rhs, "rax", "rcx");
  if (check_integer(node->lhs->type) && check_integer(node->rhs->type)) {
    printf("  cmpl %%ecx, %%eax\n");
  } else if (check_pointer(node->lhs->type) && check_pointer(node->rhs->type)) {
    printf("  cmpq %%rcx, %%rax\n");
  }
  printf("  setl %%al\n");
  printf("  movzbl %%al, %%eax\n");
  gen_push("rax");
}

void gen_lte(Expr *node) {
  gen_operands(node->lhs, node->rhs, "rax", "rcx");
  if (check_integer(node->lhs->type) && check_integer(node->rhs->type)) {
    printf("  cmpl %%ecx, %%eax\n");
  } else if (check_pointer(node->lhs->type) && check_pointer(node->rhs->type)) {
    printf("  cmpq %%rcx, %%rax\n");
  }
  printf("  setle %%al\n");
  printf("  movzbl %%al, %%eax\n");
  gen_push("rax");
}

void gen_eq(Expr *node) {
  gen_operands(node->lhs, node->rhs, "rax", "rcx");
  if (check_integer(node->lhs->type) && check_integer(node->rhs->type)) {
    printf("  cmpl %%ecx, %%eax\n");
  } else if (check_pointer(node->lhs->type) && check_pointer(node->rhs->type)) {
    printf("  cmpq %%rcx, %%rax\n");
  }
  printf("  sete %%al\n");
  printf("  movzbl %%al, %%eax\n");
  gen_push("rax");
}

void gen_neq(Expr *node) {
  gen_operands(node->lhs, node->rhs, "rax", "rcx");
  if (check_integer(node->lhs->type) && check_integer(node->rhs->type)) {
    printf("  cmpl %%ecx, %%eax\n");
  } else if (check_pointer(node->lhs->type) && check_pointer(node->rhs->type)) {
    printf("  cmpq %%rcx, %%rax\n");
  }
  printf("  setne %%al\n");
  printf("  movzbl %%al, %%eax\n");
  gen_push("rax");
}

void gen_and(Expr *node) {
  gen_operands(node->lhs, node->rhs, "rax", "rcx");
  printf("  andl %%ecx, %%eax\n");
  gen_push("rax");
}

void gen_xor(Expr *node) {
  gen_operands(node->lhs, node->rhs, "rax", "rcx");
  printf("  xorl %%ecx, %%eax\n");
  gen_push("rax");
}

void gen_or(Expr *node) {
  gen_operands(node->lhs, node->rhs, "rax", "rcx");
  printf("  orl %%ecx, %%eax\n");
  gen_push("rax");
}

void gen_land(Expr *node) {
  int label_false = label_no++;
  int label_end = label_no++;

  gen_operand(node->lhs, "rax");
  printf("  cmpq $0, %%rax\n");
  gen_jump("je", label_false);

  gen_operand(node->rhs, "rax");
  printf("  cmpq $0, %%rax\n");
  gen_jump("je", label_false);

  printf("  movl $1, %%eax\n");
  gen_jump("jmp", label_end);

  gen_label(label_false);
  printf("  movl $0, %%eax\n");

  gen_label(label_end);
  gen_push("rax");
}

void gen_lor(Expr *node) {
  int label_true = label_no++;
  int label_end = label_no++;

  gen_operand(node->lhs, "rax");
  printf("  cmpq $0, %%rax\n");
  gen_jump("jne", label_true);

  gen_operand(node->rhs, "rax");
  printf("  cmpq $0, %%rax\n");
  gen_jump("jne", label_true);

  printf("  movl $0, %%eax\n");
  gen_jump("jmp", label_end);

  gen_label(label_true);
  printf("  movl $1, %%eax\n");

  gen_label(label_end);
  gen_push("rax");
}

void gen_condition(Expr *node) {
  int label_false = label_no++;
  int label_end = label_no++;

  gen_operand(node->cond, "rax");
  printf("  cmpq $0, %%rax\n");
  gen_jump("je", label_false);

  gen_operand(node->lhs, "rax");
  gen_jump("jmp", label_end);

  gen_label(label_false);
  gen_operand(node->rhs, "rax");

  gen_label(label_end);
  gen_push("rax");
}

void gen_assign(Expr *node) {
  gen_lvalue(node->lhs);
  gen_expr(node->rhs);
  gen_store(node->lhs->type);
}

void gen_comma(Expr *node) {
  gen_operand(node->lhs, NULL);
  gen_expr(node->rhs);
}

void gen_expr(Expr *node) {
  if (node->nd_type == ND_IDENTIFIER) {
    gen_identifier(node); }
  else if (node->nd_type == ND_INTEGER) {
    gen_integer(node);
  } else if (node->nd_type == ND_STRING) {
    gen_string(node);
  } else if (node->nd_type == ND_CALL) {
    gen_call(node);
  } else if (node->nd_type == ND_DOT) {
    gen_dot(node);
  } else if (node->nd_type == ND_ADDRESS) {
    gen_address(node);
  } else if (node->nd_type == ND_INDIRECT) {
    gen_indirect(node);
  } else if (node->nd_type == ND_UPLUS) {
    gen_uplus(node);
  } else if (node->nd_type == ND_UMINUS) {
    gen_uminus(node);
  } else if (node->nd_type == ND_NOT) {
    gen_not(node);
  } else if (node->nd_type == ND_LNOT) {
    gen_lnot(node);
  } else if (node->nd_type == ND_CAST) {
    gen_cast(node);
  } else if (node->nd_type == ND_MUL) {
    gen_mul(node);
  } else if (node->nd_type == ND_DIV) {
    gen_div(node);
  } else if (node->nd_type == ND_MOD) {
    gen_mod(node);
  } else if (node->nd_type == ND_ADD) {
    gen_add(node);
  } else if (node->nd_type == ND_SUB) {
    gen_sub(node);
  } else if (node->nd_type == ND_LSHIFT) {
    gen_lshift(node);
  } else if (node->nd_type == ND_RSHIFT) {
    gen_rshift(node);
  } else if (node->nd_type == ND_LT) {
    gen_lt(node);
  } else if (node->nd_type == ND_LTE) {
    gen_lte(node);
  } else if (node->nd_type == ND_EQ) {
    gen_eq(node);
  } else if (node->nd_type == ND_NEQ) {
    gen_neq(node);
  } else if (node->nd_type == ND_AND) {
    gen_and(node);
  } else if (node->nd_type == ND_XOR) {
    gen_xor(node);
  } else if (node->nd_type == ND_OR) {
    gen_or(node);
  } else if (node->nd_type == ND_LAND) {
    gen_land(node);
  } else if (node->nd_type == ND_LOR) {
    gen_lor(node);
  } else if (node->nd_type == ND_CONDITION) {
    gen_condition(node);
  } else if (node->nd_type == ND_ASSIGN) {
    gen_assign(node);
  } else if (node->nd_type == ND_COMMA) {
    gen_comma(node);
  }
}

void gen_init_local(Initializer *init, int offset) {
  if (init->list) {
    int size = init->type->array_of->size;
    for (int i = 0; i < init->list->length; i++) {
      Initializer *item = init->list->buffer[i];
      gen_init_local(item, offset + size * i);
    }
  } else if (init->expr) {
    printf("  leaq %d(%%rbp), %%rax\n", offset);
    gen_push("rax");
    gen_expr(init->expr);
    gen_store(init->expr->type);
    gen_pop(NULL);
  }
}

void gen_decl_local(Decl *node) {
  for (int i = 0; i < node->symbols->length; i++) {
    Symbol *symbol = node->symbols->buffer[i];
    if (!symbol->definition) continue;
    if (symbol->init) {
      gen_init_local(symbol->init, -symbol->offset);
    }
  }
}

void gen_stmt(Stmt *node);

void gen_if(Stmt *node) {
  int label_else = label_no++;
  int label_end = label_no++;

  gen_operand(node->if_cond, "rax");
  printf("  cmpq $0, %%rax\n");
  gen_jump("je", label_else);

  gen_stmt(node->then_body);
  gen_jump("jmp", label_end);

  gen_label(label_else);

  if (node->else_body) {
    gen_stmt(node->else_body);
  }

  gen_label(label_end);
}

void gen_while(Stmt *node) {
  int label_begin = label_no++;
  int label_break = label_no++;

  gen_label(label_begin);

  gen_operand(node->while_cond, "rax");
  printf("  cmpq $0, %%rax\n");
  gen_jump("je", label_break);

  begin_loop_gen(&label_begin, &label_break);
  gen_stmt(node->while_body);
  end_loop_gen();

  gen_jump("jmp", label_begin);

  gen_label(label_break);
}

void gen_do(Stmt *node) {
  int label_begin = label_no++;
  int label_continue = label_no++;
  int label_break = label_no++;

  gen_label(label_begin);

  begin_loop_gen(&label_continue, &label_break);
  gen_stmt(node->do_body);
  end_loop_gen();

  gen_label(label_continue);

  gen_operand(node->do_cond, "rax");
  printf("  cmpq $0, %%rax\n");
  gen_jump("jne", label_begin);

  gen_label(label_break);
}

void gen_for(Stmt *node) {
  int label_begin = label_no++;
  int label_continue = label_no++;
  int label_break = label_no++;

  if (node->for_init) {
    if (node->for_init->nd_type == ND_DECL) {
      gen_decl_local((Decl *) node->for_init);
    } else {
      gen_operand((Expr *) node->for_init, NULL);
    }
  }

  gen_label(label_begin);

  if (node->for_cond) {
    gen_operand(node->for_cond, "rax");
    printf("  cmpq $0, %%rax\n");
    gen_jump("je", label_break);
  }

  begin_loop_gen(&label_continue, &label_break);
  gen_stmt(node->for_body);
  end_loop_gen();

  gen_label(label_continue);

  if (node->for_after) {
    gen_operand(node->for_after, NULL);
  }
  gen_jump("jmp", label_begin);

  gen_label(label_break);
}

void gen_continue(Stmt *node) {
  int *label = continue_labels->buffer[continue_labels->length - 1];
  gen_jump("jmp", *label);
}

void gen_break(Stmt *node) {
  int *label = break_labels->buffer[break_labels->length - 1];
  gen_jump("jmp", *label);
}

void gen_return(Stmt *node) {
  if (node->ret) {
    gen_operand(node->ret, "rax");
  }
  gen_jump("jmp", return_label);
}

void gen_stmt(Stmt *node) {
  if (node->nd_type == ND_COMP) {
    for (int i = 0; i < node->block_items->length; i++) {
      Node *item = node->block_items->buffer[i];
      if (item->nd_type == ND_DECL) {
        gen_decl_local((Decl *) item);
      } else {
        gen_stmt((Stmt *) item);
      }
    }
  } else if (node->nd_type == ND_EXPR) {
    if (node->expr) {
      gen_operand(node->expr, NULL);
    }
  } else if (node->nd_type == ND_IF) {
    gen_if(node);
  } else if (node->nd_type == ND_WHILE) {
    gen_while(node);
  } else if (node->nd_type == ND_DO) {
    gen_do(node);
  } else if (node->nd_type == ND_FOR) {
    gen_for(node);
  } else if (node->nd_type == ND_CONTINUE) {
    gen_continue(node);
  } else if (node->nd_type == ND_BREAK) {
    gen_break(node);
  } else if (node->nd_type == ND_RETURN) {
    gen_return(node);
  }
}

void gen_init_global(Initializer *init) {
  if (init->list) {
    Type *type = init->type;
    int length = init->list->length;
    int padding = type->size - type->array_of->size * length;

    for (int i = 0; i < length; i++) {
      Initializer *item = init->list->buffer[i];
      gen_init_global(item);
    }

    if (padding > 0) {
      printf("  .zero %d\n", padding);
    }
  } else if (init->expr) {
    Expr *expr = init->expr->expr; // ignore casting
    if (expr->nd_type == ND_INTEGER) {
      printf("  .long %d\n", expr->int_value);
    } else if (expr->nd_type == ND_STRING) {
      printf("  .quad .S%d\n", expr->string_label);
    }
  }
}

void gen_decl_global(Decl *node) {
  for (int i = 0; i < node->symbols->length; i++) {
    Symbol *symbol = node->symbols->buffer[i];
    if (!symbol->definition) continue;

    printf("  .data\n");
    printf("  .global %s\n", symbol->identifier);
    printf("%s:\n", symbol->identifier);

    if (symbol->init) {
      gen_init_global(symbol->init);
    } else {
      printf("  .zero %d\n", symbol->type->size);
    }
  }
}

void gen_func(Func *node) {
  Symbol *symbol = node->symbol;
  Type *type = symbol->type;

  gp_offset = type->params->length * 8;
  stack_depth = 8;
  return_label = label_no++;

  printf("  .text\n");
  printf("  .global %s\n", symbol->identifier);
  printf("%s:\n", symbol->identifier);

  gen_push("rbp");
  printf("  movq %%rsp, %%rbp\n");

  if (node->stack_size > 0) {
    printf("  subq $%d, %%rsp\n", node->stack_size);
    stack_depth += node->stack_size;
  }

  if (type->ellipsis) {
    for (int i = type->params->length; i < 6; i++) {
      printf("  movq %%%s, %d(%%rbp)\n", arg_reg[i], -176 + i * 8);
    }
  }

  for (int i = 0; i < type->params->length; i++) {
    Symbol *param = type->params->buffer[i];
    printf("  leaq %d(%%rbp), %%rax\n", -param->offset);
    gen_push("rax");
    gen_push(arg_reg[i]);
    gen_store(param->type);
    gen_pop(NULL);
  }

  gen_stmt(node->body);

  gen_label(return_label);
  printf("  leave\n");
  printf("  ret\n");
}

void gen_string_literal(String *string, int label) {
  printf(".S%d:\n", label);
  printf("  .ascii \"");
  for (int i = 0; i < string->length; i++) {
    char c = string->buffer[i];
    if (c == '\\') printf("\\\\");
    else if (c == '"') printf("\\\"");
    else if (c == '\a') printf("\\a");
    else if (c == '\b') printf("\\b");
    else if (c == '\f') printf("\\f");
    else if (c == '\n') printf("\\n");
    else if (c == '\r') printf("\\r");
    else if (c == '\t') printf("\\t");
    else if (c == '\v') printf("\\v");
    else if (isprint(c)) printf("%c", c);
    else printf("\\%o", c);
  }
  printf("\"\n");
}

void gen_trans_unit(TransUnit *node) {
  if (node->literals->length > 0) {
    printf("  .section .rodata\n");
    for (int i = 0; i < node->literals->length; i++) {
      gen_string_literal(node->literals->buffer[i], i);
    }
  }

  for (int i = 0; i < node->decls->length; i++) {
    Node *decl = node->decls->buffer[i];
    if (decl->nd_type == ND_DECL) {
      gen_decl_global((Decl *) decl);
    } else if (decl->nd_type == ND_FUNC) {
      gen_func((Func *) decl);
    }
  }
}

void gen(TransUnit *node) {
  label_no = 0;

  continue_labels = vector_new();
  break_labels = vector_new();

  gen_trans_unit(node);
}
