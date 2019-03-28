#include "sk2cc.h"

static char *arg_reg[] = { "rdi", "rsi", "rdx", "rcx", "r8", "r9" };

static int label_no;
static int return_label;
static Map *labels;
static Vector *continue_labels, *break_labels;

static int gp_offset;
static int overflow_arg_area;
static int stack_depth;

static void begin_switch_gen(int *label_break) {
  vector_push(break_labels, label_break);
}

static void end_switch_gen() {
  vector_pop(break_labels);
}

static void begin_loop_gen(int *label_continue, int *label_break) {
  vector_push(continue_labels, label_continue);
  vector_push(break_labels, label_break);
}

static void end_loop_gen() {
  vector_pop(continue_labels);
  vector_pop(break_labels);
}

static void gen_push(char *reg) {
  stack_depth += 8;
  if (reg) {
    printf("  pushq %%%s\n", reg);
  } else {
    printf("  subq $8, %%rsp\n");
  }
}

static void gen_pop(char *reg) {
  stack_depth -= 8;
  if (reg) {
    printf("  popq %%%s\n", reg);
  } else {
    printf("  addq $8, %%rsp\n");
  }
}

static void gen_jump(char *inst, int label) {
  printf("  %s .L%d\n", inst, label);
}

static void gen_label(int label) {
  printf(".L%d:\n", label);
}

static void gen_expr(Expr *node);

static void gen_lvalue(Expr *node) {
  if (node->nd_type == ND_IDENTIFIER) {
    Symbol *symbol = node->symbol;
    if (symbol->link == LN_EXTERNAL || symbol->link == LN_INTERNAL) {
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

static void gen_load(Type *type) {
  gen_pop("rax");
  if (type->ty_type == TY_BOOL) {
    printf("  movb (%%rax), %%al\n");
  } else if (type->ty_type == TY_CHAR || type->ty_type == TY_UCHAR) {
    printf("  movb (%%rax), %%al\n");
  } else if (type->ty_type == TY_SHORT || type->ty_type == TY_USHORT) {
    printf("  movw (%%rax), %%ax\n");
  } else if (type->ty_type == TY_INT || type->ty_type == TY_UINT) {
    printf("  movl (%%rax), %%eax\n");
  } else if (type->ty_type == TY_LONG || type->ty_type == TY_ULONG) {
    printf("  movq (%%rax), %%rax\n");
  } else if (type->ty_type == TY_POINTER && type == type->original) {
    printf("  movq (%%rax), %%rax\n");
  }
  gen_push("rax");
}

static void gen_store(Type *type) {
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
  } else if (type->ty_type == TY_LONG || type->ty_type == TY_ULONG) {
    printf("  movq %%rax, (%%rbx)\n");
  } else if (type->ty_type == TY_POINTER) {
    printf("  movq %%rax, (%%rbx)\n");
  }
  gen_push("rax");
}

static void gen_operand(Expr *node, char *reg) {
  gen_expr(node);
  gen_pop(reg);
}

static void gen_operands(Expr *lhs, Expr *rhs, char *reg_lhs, char *reg_rhs) {
  gen_expr(lhs);
  gen_expr(rhs);
  gen_pop(reg_rhs);
  gen_pop(reg_lhs);
}

static void gen_va_start(Expr *node) {
  gen_lvalue(node->macro_ap);
  gen_pop("rax");

  printf("  movl $%d, (%%rax)\n", gp_offset);
  printf("  movl $48, 4(%%rax)\n");
  printf("  leaq %d(%%rbp), %%rcx\n", overflow_arg_area + 16);
  printf("  movq %%rcx, 8(%%rax)\n");
  printf("  leaq -176(%%rbp), %%rcx\n");
  printf("  movq %%rcx, 16(%%rax)\n");

  gen_push(NULL);
}

static void gen_va_arg(Expr *node) {
  gen_lvalue(node->macro_ap);
  gen_pop("rax");

  int label_overflow = label_no++;
  int label_load = label_no++;

  printf("  movl (%%rax), %%ecx\n");
  printf("  cmpl $48, %%ecx\n");
  gen_jump("je", label_overflow);

  printf("  movl %%ecx, %%edx\n");
  printf("  addl $8, %%edx\n");
  printf("  movl %%edx, (%%rax)\n");
  printf("  movq 16(%%rax), %%rdx\n");
  printf("  addq %%rdx, %%rcx\n");
  gen_jump("jmp", label_load);

  gen_label(label_overflow);
  printf("  movq 8(%%rax), %%rcx\n");
  printf("  movq %%rcx, %%rdx\n");
  printf("  addq $8, %%rdx\n");
  printf("  movq %%rdx, 8(%%rax)\n");

  gen_label(label_load);
  printf("  movq (%%rcx), %%rax\n");
  gen_push("rax");
}

static void gen_va_end(Expr *node) {
  gen_push(NULL);
}

static void gen_identifier(Expr *node) {
  gen_lvalue(node);
  gen_load(node->type);
}

static void gen_integer(Expr *node) {
  if (node->type->ty_type == TY_INT || node->type->ty_type == TY_UINT) {
    printf("  movl $%llu, %%eax\n", node->int_value);
  } else if (node->type->ty_type == TY_LONG || node->type->ty_type == TY_ULONG) {
    printf("  movq $%llu, %%rax\n", node->int_value);
  }
  gen_push("rax");
}

static void gen_string(Expr *node) {
  printf("  leaq .S%d(%%rip), %%rax\n", node->string_label);
  gen_push("rax");
}

static void gen_call(Expr *node) {
  // In the System V ABI, up to 6 arguments are stored in a register.
  // The remaining arguments are placed on the stack.
  //
  // Additionally, %rsp must be a multiple of 16 just before the call instruction.
  // The stack state just before the call instruction is as follows:
  //
  // [higher address]
  //   --- <= 16-byte aligned
  //   return address
  //   %rbp of the previous frame
  //   --- <= %rbp
  //   local variables of the current frame
  //   evaluated expressions
  //   ---
  //   (padding for 16-byte alignment)
  //   argument N
  //   ...
  //   argument 8
  //   argument 7
  //   --- <= %rsp (16-byte aligned)
  // [lower address]

  // 16-byte alignment
  int stack_args = node->args->length > 6 ? node->args->length - 6 : 0;
  int stack_top = stack_depth + stack_args * 8;
  int padding = stack_top % 16 ? 16 - stack_top % 16 : 0;
  if (padding > 0) {
    printf("  subq $%d, %%rsp\n", padding);
    stack_depth += padding;
  }

  // evaluate arguments
  for (int i = node->args->length - 1; i >= 0; i--) {
    Expr *arg = node->args->buffer[i];
    gen_expr(arg);
  }
  for (int i = 0; i < node->args->length; i++) {
    if (i >= 6) break;
    gen_pop(arg_reg[i]);
  }

  // for function with variable length arguments
  if (!node->expr->symbol || node->expr->symbol->type->ellipsis) {
    printf("  movb $0, %%al\n");
  }

  printf("  call %s@PLT\n", node->expr->identifier);

  // restore rsp
  if (padding + stack_args * 8 > 0) {
    printf("  addq $%d, %%rsp\n", padding + stack_args * 8);
    stack_depth -= padding + stack_args * 8;
  }

  gen_push("rax");
}

static void gen_dot(Expr *node) {
  gen_lvalue(node);
  gen_load(node->type);
}

static void gen_address(Expr *node) {
  gen_lvalue(node->expr);
}

static void gen_indirect(Expr *node) {
  gen_lvalue(node);
  gen_load(node->type);
}

static void gen_uplus(Expr *node) {
  gen_operand(node->expr, "rax");
  gen_push("rax");
}

static void gen_uminus(Expr *node) {
  gen_operand(node->expr, "rax");
  if (node->expr->type->ty_type == TY_INT || node->expr->type->ty_type == TY_UINT) {
    printf("  negl %%eax\n");
  } else if (node->expr->type->ty_type == TY_LONG || node->expr->type->ty_type == TY_ULONG) {
    printf("  negq %%rax\n");
  }
  gen_push("rax");
}

static void gen_not(Expr *node) {
  gen_operand(node->expr, "rax");
  if (node->expr->type->ty_type == TY_INT || node->expr->type->ty_type == TY_UINT) {
    printf("  notl %%eax\n");
  } else if (node->expr->type->ty_type == TY_LONG || node->expr->type->ty_type == TY_ULONG) {
    printf("  notq %%rax\n");
  }
  gen_push("rax");
}

static void gen_lnot(Expr *node) {
  gen_operand(node->expr, "rax");
  printf("  cmpq $0, %%rax\n");
  printf("  sete %%al\n");
  printf("  movzbl %%al, %%eax\n");
  gen_push("rax");
}

static void gen_cast(Expr *node) {
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
  } else if (to->ty_type == TY_LONG || to->ty_type == TY_ULONG) {
    if (from->ty_type == TY_BOOL) {
      printf("  movzbl %%al, %%eax\n");
    } else if (from->ty_type == TY_CHAR) {
      printf("  movsbq %%al, %%rax\n");
    } else if (from->ty_type == TY_UCHAR) {
      printf("  movzbl %%al, %%eax\n");
    } else if (from->ty_type == TY_SHORT) {
      printf("  movswq %%ax, %%rax\n");
    } else if (from->ty_type == TY_USHORT) {
      printf("  movzwl %%ax, %%eax\n");
    } else if (from->ty_type == TY_INT) {
      printf("  movslq %%eax, %%rax\n");
    }
  }

  gen_push("rax");
}

static void gen_mul(Expr *node) {
  gen_operands(node->lhs, node->rhs, "rax", "rcx");
  if (node->lhs->type->ty_type == TY_INT) {
    printf("  imull %%ecx\n");
  } else if (node->lhs->type->ty_type == TY_UINT) {
    printf("  mull %%ecx\n");
  } else if (node->lhs->type->ty_type == TY_LONG) {
    printf("  imulq %%rcx\n");
  } else if (node->lhs->type->ty_type == TY_ULONG) {
    printf("  mulq %%rcx\n");
  }
  gen_push("rax");
}

static void gen_div(Expr *node) {
  gen_operands(node->lhs, node->rhs, "rax", "rcx");
  printf("  movq $0, %%rdx\n");
  if (node->lhs->type->ty_type == TY_INT) {
    printf("  idivl %%ecx\n");
  } else if (node->lhs->type->ty_type == TY_UINT) {
    printf("  divl %%ecx\n");
  } else if (node->lhs->type->ty_type == TY_LONG) {
    printf("  idivq %%rcx\n");
  } else if (node->lhs->type->ty_type == TY_ULONG) {
    printf("  divq %%rcx\n");
  }
  gen_push("rax");
}

static void gen_mod(Expr *node) {
  gen_operands(node->lhs, node->rhs, "rax", "rcx");
  printf("  movq $0, %%rdx\n");
  if (node->lhs->type->ty_type == TY_INT) {
    printf("  idivl %%ecx\n");
  } else if (node->lhs->type->ty_type == TY_UINT) {
    printf("  divl %%ecx\n");
  } else if (node->lhs->type->ty_type == TY_LONG) {
    printf("  idivq %%rcx\n");
  } else if (node->lhs->type->ty_type == TY_ULONG) {
    printf("  divq %%rcx\n");
  }
  gen_push("rdx");
}

static void gen_add(Expr *node) {
  if (node->lhs->type->ty_type == TY_INT || node->lhs->type->ty_type == TY_UINT) {
    gen_operands(node->lhs, node->rhs, "rax", "rcx");
    printf("  addl %%ecx, %%eax\n");
    gen_push("rax");
  } else if (node->lhs->type->ty_type == TY_LONG || node->lhs->type->ty_type == TY_ULONG) {
    gen_operands(node->lhs, node->rhs, "rax", "rcx");
    printf("  addq %%rcx, %%rax\n");
    gen_push("rax");
  } else if (node->lhs->type->ty_type == TY_POINTER) {
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

static void gen_sub(Expr *node) {
  if (node->lhs->type->ty_type == TY_INT || node->lhs->type->ty_type == TY_UINT) {
    gen_operands(node->lhs, node->rhs, "rax", "rcx");
    printf("  subl %%ecx, %%eax\n");
    gen_push("rax");
  } else if (node->lhs->type->ty_type == TY_LONG || node->lhs->type->ty_type == TY_ULONG) {
    gen_operands(node->lhs, node->rhs, "rax", "rcx");
    printf("  subq %%rcx, %%rax\n");
    gen_push("rax");
  } else if (node->lhs->type->ty_type == TY_POINTER) {
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

static void gen_lshift(Expr *node) {
  gen_operands(node->lhs, node->rhs, "rax", "rcx");
  if (node->lhs->type->ty_type == TY_INT || node->lhs->type->ty_type == TY_UINT) {
    printf("  sall %%cl, %%eax\n");
  } else if (node->lhs->type->ty_type == TY_LONG || node->lhs->type->ty_type == TY_ULONG) {
    printf("  salq %%cl, %%rax\n");
  }
  gen_push("rax");
}

static void gen_rshift(Expr *node) {
  gen_operands(node->lhs, node->rhs, "rax", "rcx");
  if (node->lhs->type->ty_type == TY_INT || node->lhs->type->ty_type == TY_UINT) {
    printf("  sarl %%cl, %%eax\n");
  } else if (node->lhs->type->ty_type == TY_LONG || node->lhs->type->ty_type == TY_ULONG) {
    printf("  sarq %%cl, %%rax\n");
  }
  gen_push("rax");
}

static void gen_lt(Expr *node) {
  gen_operands(node->lhs, node->rhs, "rax", "rcx");
  if (node->lhs->type->ty_type == TY_INT) {
    printf("  cmpl %%ecx, %%eax\n");
    printf("  setl %%al\n");
  } else if (node->lhs->type->ty_type == TY_UINT) {
    printf("  cmpl %%ecx, %%eax\n");
    printf("  setb %%al\n");
  } else if (node->lhs->type->ty_type == TY_LONG) {
    printf("  cmpq %%rcx, %%rax\n");
    printf("  setl %%al\n");
  } else if (node->lhs->type->ty_type == TY_ULONG) {
    printf("  cmpq %%rcx, %%rax\n");
    printf("  setb %%al\n");
  } else if (node->lhs->type->ty_type == TY_POINTER) {
    printf("  cmpq %%rcx, %%rax\n");
    printf("  setb %%al\n");
  }
  printf("  movzbl %%al, %%eax\n");
  gen_push("rax");
}

static void gen_lte(Expr *node) {
  gen_operands(node->lhs, node->rhs, "rax", "rcx");
  if (node->lhs->type->ty_type == TY_INT) {
    printf("  cmpl %%ecx, %%eax\n");
    printf("  setle %%al\n");
  } else if (node->lhs->type->ty_type == TY_UINT) {
    printf("  cmpl %%ecx, %%eax\n");
    printf("  setbe %%al\n");
  } else if (node->lhs->type->ty_type == TY_LONG) {
    printf("  cmpq %%rcx, %%rax\n");
    printf("  setle %%al\n");
  } else if (node->lhs->type->ty_type == TY_ULONG) {
    printf("  cmpq %%rcx, %%rax\n");
    printf("  setbe %%al\n");
  } else if (node->lhs->type->ty_type == TY_POINTER) {
    printf("  cmpq %%rcx, %%rax\n");
    printf("  setbe %%al\n");
  }
  printf("  movzbl %%al, %%eax\n");
  gen_push("rax");
}

static void gen_eq(Expr *node) {
  gen_operands(node->lhs, node->rhs, "rax", "rcx");
  if (node->lhs->type->ty_type == TY_INT || node->lhs->type->ty_type == TY_UINT) {
    printf("  cmpl %%ecx, %%eax\n");
  } else if (node->lhs->type->ty_type == TY_LONG || node->lhs->type->ty_type == TY_ULONG) {
    printf("  cmpq %%rcx, %%rax\n");
  } else if (node->lhs->type->ty_type == TY_POINTER) {
    printf("  cmpq %%rcx, %%rax\n");
  }
  printf("  sete %%al\n");
  printf("  movzbl %%al, %%eax\n");
  gen_push("rax");
}

static void gen_neq(Expr *node) {
  gen_operands(node->lhs, node->rhs, "rax", "rcx");
  if (node->lhs->type->ty_type == TY_INT || node->lhs->type->ty_type == TY_UINT) {
    printf("  cmpl %%ecx, %%eax\n");
  } else if (node->lhs->type->ty_type == TY_LONG || node->lhs->type->ty_type == TY_ULONG) {
    printf("  cmpq %%rcx, %%rax\n");
  } else if (node->lhs->type->ty_type == TY_POINTER) {
    printf("  cmpq %%rcx, %%rax\n");
  }
  printf("  setne %%al\n");
  printf("  movzbl %%al, %%eax\n");
  gen_push("rax");
}

static void gen_and(Expr *node) {
  gen_operands(node->lhs, node->rhs, "rax", "rcx");
  if (node->lhs->type->ty_type == TY_INT || node->lhs->type->ty_type == TY_UINT) {
    printf("  andl %%ecx, %%eax\n");
  } else if (node->lhs->type->ty_type == TY_LONG || node->lhs->type->ty_type == TY_ULONG) {
    printf("  andq %%rcx, %%rax\n");
  }
  gen_push("rax");
}

static void gen_xor(Expr *node) {
  gen_operands(node->lhs, node->rhs, "rax", "rcx");
  if (node->lhs->type->ty_type == TY_INT || node->lhs->type->ty_type == TY_UINT) {
    printf("  xorl %%ecx, %%eax\n");
  } else if (node->lhs->type->ty_type == TY_LONG || node->lhs->type->ty_type == TY_ULONG) {
    printf("  xorq %%rcx, %%rax\n");
  }
  gen_push("rax");
}

static void gen_or(Expr *node) {
  gen_operands(node->lhs, node->rhs, "rax", "rcx");
  if (node->lhs->type->ty_type == TY_INT || node->lhs->type->ty_type == TY_UINT) {
    printf("  orl %%ecx, %%eax\n");
  } else if (node->lhs->type->ty_type == TY_LONG || node->lhs->type->ty_type == TY_ULONG) {
    printf("  orq %%rcx, %%rax\n");
  }
  gen_push("rax");
}

static void gen_land(Expr *node) {
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

static void gen_lor(Expr *node) {
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

static void gen_condition(Expr *node) {
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

static void gen_assign(Expr *node) {
  gen_lvalue(node->lhs);
  gen_expr(node->rhs);
  gen_store(node->lhs->type);
}

static void gen_comma(Expr *node) {
  gen_operand(node->lhs, NULL);
  gen_expr(node->rhs);
}

static void gen_expr(Expr *node) {
  if (node->nd_type == ND_VA_START) {
    gen_va_start(node);
  } else if (node->nd_type == ND_VA_ARG) {
    gen_va_arg(node);
  } else if (node->nd_type == ND_VA_END) {
    gen_va_end(node);
  } else if (node->nd_type == ND_IDENTIFIER) {
    gen_identifier(node);
  } else if (node->nd_type == ND_INTEGER) {
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
  } else {
    internal_error("unknown expression type.");
  }
}

static void gen_init_local(Initializer *init, int offset) {
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

static void gen_decl_local(Decl *node) {
  for (int i = 0; i < node->symbols->length; i++) {
    Symbol *symbol = node->symbols->buffer[i];
    if (!symbol->definition) continue;
    if (symbol->init) {
      gen_init_local(symbol->init, -symbol->offset);
    }
  }
}

static void gen_stmt(Stmt *node);

static void gen_case(Stmt *node) {
  gen_label(node->case_label);
  gen_stmt(node->case_stmt);
}

static void gen_default(Stmt *node) {
  gen_label(node->default_label);
  gen_stmt(node->default_stmt);
}

static void gen_if(Stmt *node) {
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

static void gen_switch(Stmt *node) {
  int label_break = label_no++;

  gen_operand(node->switch_cond, "rax");
  for (int i = 0; i < node->switch_cases->length; i++) {
    Stmt *stmt = node->switch_cases->buffer[i];
    if (stmt->nd_type == ND_CASE) {
      stmt->case_label = label_no++;
      printf("  cmpq $%llu, %%rax\n", stmt->case_const->int_value);
      gen_jump("je", stmt->case_label);
    } else if (stmt->nd_type == ND_DEFAULT) {
      stmt->default_label = label_no++;
      gen_jump("jmp", stmt->default_label);
    }
  }

  begin_switch_gen(&label_break);
  gen_stmt(node->switch_body);
  end_switch_gen();

  gen_label(label_break);
}

static void gen_while(Stmt *node) {
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

static void gen_do(Stmt *node) {
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

static void gen_for(Stmt *node) {
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

static void gen_goto(Stmt *node) {
  int *label = map_lookup(labels, node->goto_label);
  gen_jump("jmp", *label);
}

static void gen_continue(Stmt *node) {
  int *label = continue_labels->buffer[continue_labels->length - 1];
  gen_jump("jmp", *label);
}

static void gen_break(Stmt *node) {
  int *label = break_labels->buffer[break_labels->length - 1];
  gen_jump("jmp", *label);
}

static void gen_return(Stmt *node) {
  if (node->ret) {
    gen_operand(node->ret, "rax");
  }
  gen_jump("jmp", return_label);
}

static void gen_stmt(Stmt *node) {
  if (node->nd_type == ND_LABEL) {
    int *label = map_lookup(labels, node->label_name);
    gen_label(*label);
    gen_stmt(node->label_stmt);
  } else if (node->nd_type == ND_CASE) {
    gen_case(node);
  } else if (node->nd_type == ND_DEFAULT) {
    gen_default(node);
  } else if (node->nd_type == ND_COMP) {
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
  } else if (node->nd_type == ND_SWITCH) {
    gen_switch(node);
  } else if (node->nd_type == ND_WHILE) {
    gen_while(node);
  } else if (node->nd_type == ND_DO) {
    gen_do(node);
  } else if (node->nd_type == ND_FOR) {
    gen_for(node);
  } else if (node->nd_type == ND_GOTO) {
    gen_goto(node);
  } else if (node->nd_type == ND_CONTINUE) {
    gen_continue(node);
  } else if (node->nd_type == ND_BREAK) {
    gen_break(node);
  } else if (node->nd_type == ND_RETURN) {
    gen_return(node);
  } else {
    internal_error("unknown statement type.");
  }
}

static void gen_init_global(Initializer *init) {
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
      printf("  .long %llu\n", expr->int_value);
    } else if (expr->nd_type == ND_STRING) {
      printf("  .quad .S%d\n", expr->string_label);
    }
  }
}

static void gen_decl_global(Decl *node) {
  for (int i = 0; i < node->symbols->length; i++) {
    Symbol *symbol = node->symbols->buffer[i];
    if (!symbol->definition) continue;

    printf("  .data\n");
    if (symbol->link == LN_EXTERNAL) {
      printf("  .global %s\n", symbol->identifier);
    }
    printf("%s:\n", symbol->identifier);

    if (symbol->init) {
      gen_init_global(symbol->init);
    } else {
      printf("  .zero %d\n", symbol->type->size);
    }
  }
}

static void gen_func(Func *node) {
  Symbol *symbol = node->symbol;
  Type *type = symbol->type;

  if (type->params->length <= 6) {
    gp_offset = type->params->length * 8;
    overflow_arg_area = 0;
  } else {
    gp_offset = 48;
    overflow_arg_area = (type->params->length - 6) * 8;
  }
  stack_depth = 8;
  return_label = label_no++;

  labels = map_new();
  for (int i = 0; i < node->label_names->length; i++) {
    char *label_name = node->label_names->buffer[i];
    int *label = calloc(1, sizeof(int));
    *label = label_no++;
    map_put(labels, label_name, label);
  }

  printf("  .text\n");
  if (symbol->link == LN_EXTERNAL) {
    printf("  .global %s\n", symbol->identifier);
  }
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

  // The current stack state is as follows:
  //
  // [higher address]
  //   ----
  //   argument N
  //   ...
  //   argument 8
  //   argument 7
  //   ---- <= (16-byte aligned)
  //   return address
  //   %rbp of the previous frame
  //   ---- <= %rbp
  //   local variables of the current frame
  //   ---- <= %rsp
  // [lower address]

  for (int i = 0; i < type->params->length; i++) {
    Symbol *param = type->params->buffer[i];
    if (i < 6) {
      printf("  leaq %d(%%rbp), %%rax\n", -param->offset);
      gen_push("rax");
      gen_push(arg_reg[i]);
      gen_store(param->type);
      gen_pop(NULL);
    } else {
      printf("  leaq %d(%%rbp), %%rax\n", -param->offset);
      gen_push("rax");
      printf("  movq %d(%%rbp), %%rax\n", 16 + (i - 6) * 8);
      gen_push("rax");
      gen_store(param->type);
      gen_pop(NULL);
    }
  }

  gen_stmt(node->body);

  gen_label(return_label);
  printf("  leave\n");
  printf("  ret\n");
}

static void gen_string_literal(String *string, int label) {
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

static void gen_trans_unit(TransUnit *node) {
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
