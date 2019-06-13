#include "cc.h"

typedef enum {
  REG_BYTE,
  REG_WORD,
  REG_LONG,
  REG_QUAD,
} RegSize;

typedef enum {
  REG_AX,
  REG_CX,
  REG_DX,
  REG_BX,
  REG_SP,
  REG_BP,
  REG_SI,
  REG_DI,
  REG_R8,
  REG_R9,
  REG_R10,
  REG_R11,
  REG_R12,
  REG_R13,
  REG_R14,
  REG_R15,
} RegCode;

static char *reg[16][4] = {
  { "al", "ax", "eax", "rax" },
  { "cl", "cx", "ecx", "rcx" },
  { "dl", "dx", "edx", "rdx" },
  { "bl", "bx", "ebx", "rbx" },
  { "spl", "sp", "esp", "rsp" },
  { "bpl", "bp", "ebp", "rbp" },
  { "sil", "si", "esi", "rsi" },
  { "dil", "di", "edi", "rdi" },
  { "r8b", "r8w", "r8d", "r8" },
  { "r9b", "r9w", "r9d", "r9" },
  { "r10b", "r10w", "r10d", "r10" },
  { "r11b", "r11w", "r11d", "r11" },
  { "r12b", "r12w", "r12d", "r12" },
  { "r13b", "r13w", "r13d", "r13" },
  { "r14b", "r14w", "r14d", "r14" },
  { "r15b", "r15w", "r15d", "r15" },
};

// rdi, rsi, rdx, rcx, r8, r9
// currently initializer with enum constant is not supported.
static RegCode arg_reg[6] = { 7, 6, 2, 1, 8, 9 };

static int lbl_no;

static int gp_offset;
static int overflow_arg_area;
static int stack_depth;

// generation of expression

static void gen_expr(Expr *expr);

#define GEN_PUSH(reg) \
  do { \
    stack_depth += 8; \
    printf("  pushq %%%s\n", reg); \
  } while (0)

#define GEN_POP(reg) \
  do { \
    stack_depth -= 8; \
    printf("  popq %%%s\n", reg); \
  } while (0)

#define GEN_PUSH_GARBAGE() \
  do { \
    stack_depth += 8; \
    printf("  subq $8, %%rsp\n"); \
  } while (0)

#define GEN_POP_DISCARD() \
  do { \
    stack_depth -= 8; \
    printf("  addq $8, %%rsp\n"); \
  } while (0)

#define GEN_EVAL(expr) \
  do { \
    gen_expr(expr); \
    GEN_POP_DISCARD(); \
  } while (0)

#define GEN_LABEL(label) \
  do { \
    printf(".L%d:\n", label); \
  } while (0)

#define GEN_JUMP(inst, label) \
  do { \
    printf("  %s .L%d\n", inst, label); \
  } while (0)

#define GEN_OP(expr, reg) \
  do { \
    gen_expr(expr); \
    GEN_POP(reg); \
  } while (0)

#define GEN_OP2(expr_lhs, expr_rhs, reg_lhs, reg_rhs) \
  do { \
    gen_expr(expr_lhs); \
    gen_expr(expr_rhs); \
    GEN_POP(reg_rhs); \
    GEN_POP(reg_lhs); \
  } while (0)

static void gen_lvalue(Expr *expr) {
  switch (expr->nd_type) {
    case ND_IDENTIFIER: {
      switch (expr->symbol->link) {
        case LN_EXTERNAL:
        case LN_INTERNAL: {
          printf("  leaq %s(%%rip), %%rax\n", expr->symbol->identifier);
          break;
        }
        case LN_NONE: {
          printf("  leaq %d(%%rbp), %%rax\n", -expr->symbol->offset);
          break;
        }
      }
      GEN_PUSH("rax");
      break;
    }
    case ND_INDIRECT: {
      gen_expr(expr->expr);
      break;
    }
    case ND_DOT: {
      gen_lvalue(expr->expr);
      GEN_POP("rax");
      printf("  leaq %d(%%rax), %%rax\n", expr->offset);
      GEN_PUSH("rax");
      break;
    }
    default: assert(false);
  }
}

static void gen_load(Type *type) {
  GEN_POP("rax");
  switch (type->ty_type) {
    case TY_BOOL:
    case TY_CHAR:
    case TY_UCHAR: {
      printf("  movb (%%rax), %%al\n");
      break;
    }
    case TY_SHORT:
    case TY_USHORT: {
      printf("  movw (%%rax), %%ax\n");
      break;
    }
    case TY_INT:
    case TY_UINT: {
      printf("  movl (%%rax), %%eax\n");
      break;
    }
    case TY_LONG:
    case TY_ULONG: {
      printf("  movq (%%rax), %%rax\n");
      break;
    }
    case TY_POINTER: {
      if (type == type->original) {
        printf("  movq (%%rax), %%rax\n");
      }
      break;
    }
    default: assert(false);
  }
  GEN_PUSH("rax");
}

static void gen_store_by_addr(RegCode value, RegCode addr, Type *type) {
  switch (type->ty_type) {
    case TY_BOOL:
    case TY_CHAR:
    case TY_UCHAR: {
      printf("  movb %%%s, (%%%s)\n", reg[value][REG_BYTE], reg[addr][REG_QUAD]);
      break;
    }
    case TY_SHORT:
    case TY_USHORT: {
      printf("  movw %%%s, (%%%s)\n", reg[value][REG_WORD], reg[addr][REG_QUAD]);
      break;
    }
    case TY_INT:
    case TY_UINT: {
      printf("  movl %%%s, (%%%s)\n", reg[value][REG_LONG], reg[addr][REG_QUAD]);
      break;
    }
    case TY_LONG:
    case TY_ULONG:
    case TY_POINTER: {
      printf("  movq %%%s, (%%%s)\n", reg[value][REG_QUAD], reg[addr][REG_QUAD]);
      break;
    }
    default: assert(false);
  }
}

static void gen_store_by_offset(RegCode value, int offset, Type *type) {
  switch (type->ty_type) {
    case TY_BOOL:
    case TY_CHAR:
    case TY_UCHAR: {
      printf("  movb %%%s, %d(%%rbp)\n", reg[value][REG_BYTE], offset);
      break;
    }
    case TY_SHORT:
    case TY_USHORT: {
      printf("  movw %%%s, %d(%%rbp)\n", reg[value][REG_WORD], offset);
      break;
    }
    case TY_INT:
    case TY_UINT: {
      printf("  movl %%%s, %d(%%rbp)\n", reg[value][REG_LONG], offset);
      break;
    }
    case TY_LONG:
    case TY_ULONG:
    case TY_POINTER: {
      printf("  movq %%%s, %d(%%rbp)\n", reg[value][REG_QUAD], offset);
      break;
    }
    default: assert(false);
  }
}

static void gen_va_start(Expr *expr) {
  gen_lvalue(expr->macro_ap);
  GEN_POP("rax");

  printf("  movl $%d, (%%rax)\n", gp_offset);
  printf("  movl $48, 4(%%rax)\n");
  printf("  leaq %d(%%rbp), %%rcx\n", overflow_arg_area + 16);
  printf("  movq %%rcx, 8(%%rax)\n");
  printf("  leaq -176(%%rbp), %%rcx\n");
  printf("  movq %%rcx, 16(%%rax)\n");

  GEN_PUSH_GARBAGE();
}

static void gen_va_arg(Expr *expr) {
  gen_lvalue(expr->macro_ap);
  GEN_POP("rax");

  int label_overflow = lbl_no++;
  int label_load = lbl_no++;

  printf("  movl (%%rax), %%ecx\n");
  printf("  cmpl $48, %%ecx\n");
  GEN_JUMP("je", label_overflow);

  printf("  movl %%ecx, %%edx\n");
  printf("  addl $8, %%edx\n");
  printf("  movl %%edx, (%%rax)\n");
  printf("  movq 16(%%rax), %%rdx\n");
  printf("  addq %%rdx, %%rcx\n");
  GEN_JUMP("jmp", label_load);

  GEN_LABEL(label_overflow);
  printf("  movq 8(%%rax), %%rcx\n");
  printf("  movq %%rcx, %%rdx\n");
  printf("  addq $8, %%rdx\n");
  printf("  movq %%rdx, 8(%%rax)\n");

  GEN_LABEL(label_load);
  printf("  movq (%%rcx), %%rax\n");
  GEN_PUSH("rax");
}

static void gen_va_end(Expr *expr) {
  GEN_PUSH_GARBAGE();
}

static void gen_identifier(Expr *expr) {
  gen_lvalue(expr);
  gen_load(expr->type);
}

static void gen_integer(Expr *expr) {
  if (expr->type->ty_type == TY_INT || expr->type->ty_type == TY_UINT) {
    printf("  movl $%llu, %%eax\n", expr->int_value);
  } else if (expr->type->ty_type == TY_LONG || expr->type->ty_type == TY_ULONG) {
    printf("  movq $%llu, %%rax\n", expr->int_value);
  }
  GEN_PUSH("rax");
}

static void gen_string(Expr *expr) {
  printf("  leaq .S%d(%%rip), %%rax\n", expr->string_label);
  GEN_PUSH("rax");
}

static void gen_call(Expr *expr) {
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
  int stack_args = expr->args->length > 6 ? expr->args->length - 6 : 0;
  int stack_top = stack_depth + stack_args * 8;
  int padding = stack_top % 16 ? 16 - stack_top % 16 : 0;
  if (padding > 0) {
    printf("  subq $%d, %%rsp\n", padding);
    stack_depth += padding;
  }

  // evaluate arguments
  for (int i = expr->args->length - 1; i >= 0; i--) {
    Expr *arg = expr->args->buffer[i];
    gen_expr(arg);
  }
  for (int i = 0; i < expr->args->length; i++) {
    if (i >= 6) break;
    GEN_POP(reg[arg_reg[i]][REG_QUAD]);
  }

  // for function with variable length arguments
  if (!expr->expr->symbol || expr->expr->symbol->type->ellipsis) {
    printf("  movb $0, %%al\n");
  }

  printf("  call %s\n", expr->expr->identifier);

  // restore rsp
  if (padding + stack_args * 8 > 0) {
    printf("  addq $%d, %%rsp\n", padding + stack_args * 8);
    stack_depth -= padding + stack_args * 8;
  }

  GEN_PUSH("rax");
}

static void gen_dot(Expr *expr) {
  gen_lvalue(expr);
  gen_load(expr->type);
}

static void gen_address(Expr *expr) {
  gen_lvalue(expr->expr);
}

static void gen_indirect(Expr *expr) {
  gen_lvalue(expr);
  gen_load(expr->type);
}

static void gen_uplus(Expr *expr) {
  GEN_OP(expr->expr, "rax");
  GEN_PUSH("rax");
}

static void gen_uminus(Expr *expr) {
  GEN_OP(expr->expr, "rax");
  if (expr->expr->type->ty_type == TY_INT || expr->expr->type->ty_type == TY_UINT) {
    printf("  negl %%eax\n");
  } else if (expr->expr->type->ty_type == TY_LONG || expr->expr->type->ty_type == TY_ULONG) {
    printf("  negq %%rax\n");
  }
  GEN_PUSH("rax");
}

static void gen_not(Expr *expr) {
  GEN_OP(expr->expr, "rax");
  if (expr->expr->type->ty_type == TY_INT || expr->expr->type->ty_type == TY_UINT) {
    printf("  notl %%eax\n");
  } else if (expr->expr->type->ty_type == TY_LONG || expr->expr->type->ty_type == TY_ULONG) {
    printf("  notq %%rax\n");
  }
  GEN_PUSH("rax");
}

static void gen_lnot(Expr *expr) {
  GEN_OP(expr->expr, "rax");
  printf("  cmpq $0, %%rax\n");
  printf("  sete %%al\n");
  printf("  movzbl %%al, %%eax\n");
  GEN_PUSH("rax");
}

static void gen_cast(Expr *expr) {
  GEN_OP(expr->expr, "rax");

  Type *to = expr->type;
  Type *from = expr->expr->type;

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

  GEN_PUSH("rax");
}

static void gen_mul(Expr *expr) {
  GEN_OP2(expr->lhs, expr->rhs, "rax", "rcx");
  if (expr->lhs->type->ty_type == TY_INT) {
    printf("  imull %%ecx\n");
  } else if (expr->lhs->type->ty_type == TY_UINT) {
    printf("  mull %%ecx\n");
  } else if (expr->lhs->type->ty_type == TY_LONG) {
    printf("  imulq %%rcx\n");
  } else if (expr->lhs->type->ty_type == TY_ULONG) {
    printf("  mulq %%rcx\n");
  }
  GEN_PUSH("rax");
}

static void gen_div(Expr *expr) {
  GEN_OP2(expr->lhs, expr->rhs, "rax", "rcx");
  printf("  movq $0, %%rdx\n");
  if (expr->lhs->type->ty_type == TY_INT) {
    printf("  idivl %%ecx\n");
  } else if (expr->lhs->type->ty_type == TY_UINT) {
    printf("  divl %%ecx\n");
  } else if (expr->lhs->type->ty_type == TY_LONG) {
    printf("  idivq %%rcx\n");
  } else if (expr->lhs->type->ty_type == TY_ULONG) {
    printf("  divq %%rcx\n");
  }
  GEN_PUSH("rax");
}

static void gen_mod(Expr *expr) {
  GEN_OP2(expr->lhs, expr->rhs, "rax", "rcx");
  printf("  movq $0, %%rdx\n");
  if (expr->lhs->type->ty_type == TY_INT) {
    printf("  idivl %%ecx\n");
  } else if (expr->lhs->type->ty_type == TY_UINT) {
    printf("  divl %%ecx\n");
  } else if (expr->lhs->type->ty_type == TY_LONG) {
    printf("  idivq %%rcx\n");
  } else if (expr->lhs->type->ty_type == TY_ULONG) {
    printf("  divq %%rcx\n");
  }
  GEN_PUSH("rdx");
}

static void gen_add(Expr *expr) {
  if (expr->lhs->type->ty_type == TY_INT || expr->lhs->type->ty_type == TY_UINT) {
    GEN_OP2(expr->lhs, expr->rhs, "rax", "rcx");
    printf("  addl %%ecx, %%eax\n");
    GEN_PUSH("rax");
  } else if (expr->lhs->type->ty_type == TY_LONG || expr->lhs->type->ty_type == TY_ULONG) {
    GEN_OP2(expr->lhs, expr->rhs, "rax", "rcx");
    printf("  addq %%rcx, %%rax\n");
    GEN_PUSH("rax");
  } else if (expr->lhs->type->ty_type == TY_POINTER) {
    int size = expr->lhs->type->pointer_to->size;
    gen_expr(expr->lhs);
    GEN_OP(expr->rhs, "rax");
    printf("  movq $%d, %%rcx\n", size);
    printf("  mulq %%rcx\n");
    GEN_POP("rcx");
    printf("  addq %%rax, %%rcx\n");
    GEN_PUSH("rcx");
  }
}

static void gen_sub(Expr *expr) {
  if (expr->lhs->type->ty_type == TY_INT || expr->lhs->type->ty_type == TY_UINT) {
    GEN_OP2(expr->lhs, expr->rhs, "rax", "rcx");
    printf("  subl %%ecx, %%eax\n");
    GEN_PUSH("rax");
  } else if (expr->lhs->type->ty_type == TY_LONG || expr->lhs->type->ty_type == TY_ULONG) {
    GEN_OP2(expr->lhs, expr->rhs, "rax", "rcx");
    printf("  subq %%rcx, %%rax\n");
    GEN_PUSH("rax");
  } else if (expr->lhs->type->ty_type == TY_POINTER) {
    int size = expr->lhs->type->pointer_to->size;
    gen_expr(expr->lhs);
    GEN_OP(expr->rhs, "rax");
    printf("  movq $%d, %%rcx\n", size);
    printf("  mulq %%rcx\n");
    GEN_POP("rcx");
    printf("  subq %%rax, %%rcx\n");
    GEN_PUSH("rcx");
  }
}

static void gen_lshift(Expr *expr) {
  GEN_OP2(expr->lhs, expr->rhs, "rax", "rcx");
  if (expr->lhs->type->ty_type == TY_INT || expr->lhs->type->ty_type == TY_UINT) {
    printf("  sall %%cl, %%eax\n");
  } else if (expr->lhs->type->ty_type == TY_LONG || expr->lhs->type->ty_type == TY_ULONG) {
    printf("  salq %%cl, %%rax\n");
  }
  GEN_PUSH("rax");
}

static void gen_rshift(Expr *expr) {
  GEN_OP2(expr->lhs, expr->rhs, "rax", "rcx");
  if (expr->lhs->type->ty_type == TY_INT || expr->lhs->type->ty_type == TY_UINT) {
    printf("  sarl %%cl, %%eax\n");
  } else if (expr->lhs->type->ty_type == TY_LONG || expr->lhs->type->ty_type == TY_ULONG) {
    printf("  sarq %%cl, %%rax\n");
  }
  GEN_PUSH("rax");
}

static void gen_lt(Expr *expr) {
  GEN_OP2(expr->lhs, expr->rhs, "rax", "rcx");
  if (expr->lhs->type->ty_type == TY_INT) {
    printf("  cmpl %%ecx, %%eax\n");
    printf("  setl %%al\n");
  } else if (expr->lhs->type->ty_type == TY_UINT) {
    printf("  cmpl %%ecx, %%eax\n");
    printf("  setb %%al\n");
  } else if (expr->lhs->type->ty_type == TY_LONG) {
    printf("  cmpq %%rcx, %%rax\n");
    printf("  setl %%al\n");
  } else if (expr->lhs->type->ty_type == TY_ULONG) {
    printf("  cmpq %%rcx, %%rax\n");
    printf("  setb %%al\n");
  } else if (expr->lhs->type->ty_type == TY_POINTER) {
    printf("  cmpq %%rcx, %%rax\n");
    printf("  setb %%al\n");
  }
  printf("  movzbl %%al, %%eax\n");
  GEN_PUSH("rax");
}

static void gen_lte(Expr *expr) {
  GEN_OP2(expr->lhs, expr->rhs, "rax", "rcx");
  if (expr->lhs->type->ty_type == TY_INT) {
    printf("  cmpl %%ecx, %%eax\n");
    printf("  setle %%al\n");
  } else if (expr->lhs->type->ty_type == TY_UINT) {
    printf("  cmpl %%ecx, %%eax\n");
    printf("  setbe %%al\n");
  } else if (expr->lhs->type->ty_type == TY_LONG) {
    printf("  cmpq %%rcx, %%rax\n");
    printf("  setle %%al\n");
  } else if (expr->lhs->type->ty_type == TY_ULONG) {
    printf("  cmpq %%rcx, %%rax\n");
    printf("  setbe %%al\n");
  } else if (expr->lhs->type->ty_type == TY_POINTER) {
    printf("  cmpq %%rcx, %%rax\n");
    printf("  setbe %%al\n");
  }
  printf("  movzbl %%al, %%eax\n");
  GEN_PUSH("rax");
}

static void gen_eq(Expr *expr) {
  GEN_OP2(expr->lhs, expr->rhs, "rax", "rcx");
  if (expr->lhs->type->ty_type == TY_INT || expr->lhs->type->ty_type == TY_UINT) {
    printf("  cmpl %%ecx, %%eax\n");
  } else if (expr->lhs->type->ty_type == TY_LONG || expr->lhs->type->ty_type == TY_ULONG) {
    printf("  cmpq %%rcx, %%rax\n");
  } else if (expr->lhs->type->ty_type == TY_POINTER) {
    printf("  cmpq %%rcx, %%rax\n");
  }
  printf("  sete %%al\n");
  printf("  movzbl %%al, %%eax\n");
  GEN_PUSH("rax");
}

static void gen_neq(Expr *expr) {
  GEN_OP2(expr->lhs, expr->rhs, "rax", "rcx");
  if (expr->lhs->type->ty_type == TY_INT || expr->lhs->type->ty_type == TY_UINT) {
    printf("  cmpl %%ecx, %%eax\n");
  } else if (expr->lhs->type->ty_type == TY_LONG || expr->lhs->type->ty_type == TY_ULONG) {
    printf("  cmpq %%rcx, %%rax\n");
  } else if (expr->lhs->type->ty_type == TY_POINTER) {
    printf("  cmpq %%rcx, %%rax\n");
  }
  printf("  setne %%al\n");
  printf("  movzbl %%al, %%eax\n");
  GEN_PUSH("rax");
}

static void gen_and(Expr *expr) {
  GEN_OP2(expr->lhs, expr->rhs, "rax", "rcx");
  if (expr->lhs->type->ty_type == TY_INT || expr->lhs->type->ty_type == TY_UINT) {
    printf("  andl %%ecx, %%eax\n");
  } else if (expr->lhs->type->ty_type == TY_LONG || expr->lhs->type->ty_type == TY_ULONG) {
    printf("  andq %%rcx, %%rax\n");
  }
  GEN_PUSH("rax");
}

static void gen_xor(Expr *expr) {
  GEN_OP2(expr->lhs, expr->rhs, "rax", "rcx");
  if (expr->lhs->type->ty_type == TY_INT || expr->lhs->type->ty_type == TY_UINT) {
    printf("  xorl %%ecx, %%eax\n");
  } else if (expr->lhs->type->ty_type == TY_LONG || expr->lhs->type->ty_type == TY_ULONG) {
    printf("  xorq %%rcx, %%rax\n");
  }
  GEN_PUSH("rax");
}

static void gen_or(Expr *expr) {
  GEN_OP2(expr->lhs, expr->rhs, "rax", "rcx");
  if (expr->lhs->type->ty_type == TY_INT || expr->lhs->type->ty_type == TY_UINT) {
    printf("  orl %%ecx, %%eax\n");
  } else if (expr->lhs->type->ty_type == TY_LONG || expr->lhs->type->ty_type == TY_ULONG) {
    printf("  orq %%rcx, %%rax\n");
  }
  GEN_PUSH("rax");
}

static void gen_land(Expr *expr) {
  int lbl_false = lbl_no++;
  int lbl_end = lbl_no++;

  GEN_OP(expr->lhs, "rax");
  printf("  cmpq $0, %%rax\n");
  GEN_JUMP("je", lbl_false);

  GEN_OP(expr->rhs, "rax");
  printf("  cmpq $0, %%rax\n");
  GEN_JUMP("je", lbl_false);

  printf("  movl $1, %%eax\n");
  GEN_JUMP("jmp", lbl_end);

  GEN_LABEL(lbl_false);
  printf("  movl $0, %%eax\n");

  GEN_LABEL(lbl_end);
  GEN_PUSH("rax");
}

static void gen_lor(Expr *expr) {
  int lbl_true = lbl_no++;
  int lbl_end = lbl_no++;

  GEN_OP(expr->lhs, "rax");
  printf("  cmpq $0, %%rax\n");
  GEN_JUMP("jne", lbl_true);

  GEN_OP(expr->rhs, "rax");
  printf("  cmpq $0, %%rax\n");
  GEN_JUMP("jne", lbl_true);

  printf("  movl $0, %%eax\n");
  GEN_JUMP("jmp", lbl_end);

  GEN_LABEL(lbl_true);
  printf("  movl $1, %%eax\n");

  GEN_LABEL(lbl_end);
  GEN_PUSH("rax");
}

static void gen_condition(Expr *expr) {
  int lbl_false = lbl_no++;
  int lbl_end = lbl_no++;

  GEN_OP(expr->cond, "rax");
  printf("  cmpq $0, %%rax\n");
  GEN_JUMP("je", lbl_false);

  GEN_OP(expr->lhs, "rax");
  GEN_JUMP("jmp", lbl_end);

  GEN_LABEL(lbl_false);
  GEN_OP(expr->rhs, "rax");

  GEN_LABEL(lbl_end);
  GEN_PUSH("rax");
}

static void gen_assign(Expr *expr) {
  gen_lvalue(expr->lhs);
  GEN_OP(expr->rhs, "rax");
  GEN_POP("rcx");
  gen_store_by_addr(REG_AX, REG_CX, expr->lhs->type);
  GEN_PUSH("rax");
}

static void gen_comma(Expr *expr) {
  GEN_EVAL(expr->lhs);
  gen_expr(expr->rhs);
}

static void gen_expr(Expr *expr) {
  switch (expr->nd_type) {
    case ND_VA_START:
      gen_va_start(expr);
      break;
    case ND_VA_ARG:
      gen_va_arg(expr);
      break;
    case ND_VA_END:
      gen_va_end(expr);
      break;

    case ND_IDENTIFIER:
      gen_identifier(expr);
      break;
    case ND_INTEGER:
      gen_integer(expr);
      break;
    case ND_STRING:
      gen_string(expr);
      break;

    case ND_CALL:
      gen_call(expr);
      break;
    case ND_DOT:
      gen_dot(expr);
      break;

    case ND_ADDRESS:
      gen_address(expr);
      break;
    case ND_INDIRECT:
      gen_indirect(expr);
      break;
    case ND_UPLUS:
      gen_uplus(expr);
      break;
    case ND_UMINUS:
      gen_uminus(expr);
      break;
    case ND_NOT:
      gen_not(expr);
      break;
    case ND_LNOT:
      gen_lnot(expr);
      break;

    case ND_CAST:
      gen_cast(expr);
      break;

    case ND_MUL:
      gen_mul(expr);
      break;
    case ND_DIV:
      gen_div(expr);
      break;
    case ND_MOD:
      gen_mod(expr);
      break;

    case ND_ADD:
      gen_add(expr);
      break;
    case ND_SUB:
      gen_sub(expr);
      break;

    case ND_LSHIFT:
      gen_lshift(expr);
      break;
    case ND_RSHIFT:
      gen_rshift(expr);
      break;

    case ND_LT:
      gen_lt(expr);
      break;
    case ND_LTE:
      gen_lte(expr);
      break;

    case ND_EQ:
      gen_eq(expr);
      break;
    case ND_NEQ:
      gen_neq(expr);
      break;

    case ND_AND:
      gen_and(expr);
      break;
    case ND_XOR:
      gen_xor(expr);
      break;
    case ND_OR:
      gen_or(expr);
      break;

    case ND_LAND:
      gen_land(expr);
      break;
    case ND_LOR:
      gen_lor(expr);
      break;

    case ND_CONDITION:
      gen_condition(expr);
      break;

    case ND_ASSIGN:
      gen_assign(expr);
      break;

    case ND_COMMA:
      gen_comma(expr);
      break;

    default:
      assert(false); // unreachable
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
    GEN_OP(init->expr, "rax");
    gen_store_by_offset(REG_AX, offset, init->expr->type);
  }
}

static void gen_decl_local(Decl *decl) {
  for (int i = 0; i < decl->symbols->length; i++) {
    Symbol *symbol = decl->symbols->buffer[i];
    if (!symbol->definition) continue;
    if (symbol->init) {
      gen_init_local(symbol->init, -symbol->offset);
    }
  }
}

// generation of statement

static void gen_stmt(Stmt *stmt);

static void gen_label(Stmt *stmt) {
  GEN_LABEL(stmt->lbl_label);
  gen_stmt(stmt->label_stmt);
}

static void gen_case(Stmt *stmt) {
  GEN_LABEL(stmt->lbl_label);
  gen_stmt(stmt->case_stmt);
}

static void gen_default(Stmt *stmt) {
  GEN_LABEL(stmt->lbl_label);
  gen_stmt(stmt->default_stmt);
}

static void gen_if(Stmt *stmt) {
  int lbl_else = lbl_no++;
  int lbl_end = lbl_no++;

  GEN_OP(stmt->if_cond, "rax");
  printf("  cmpq $0, %%rax\n");
  GEN_JUMP("je", lbl_else);

  gen_stmt(stmt->then_body);
  GEN_JUMP("jmp", lbl_end);

  GEN_LABEL(lbl_else);

  if (stmt->else_body) {
    gen_stmt(stmt->else_body);
  }

  GEN_LABEL(lbl_end);
}

static void gen_switch(Stmt *stmt) {
  stmt->lbl_break = lbl_no++;

  GEN_OP(stmt->switch_cond, "rax");
  for (int i = 0; i < stmt->switch_cases->length; i++) {
    Stmt *case_stmt = stmt->switch_cases->buffer[i];
    case_stmt->lbl_label = lbl_no++;
    if (case_stmt->nd_type == ND_CASE) {
      printf("  cmpq $%llu, %%rax\n", case_stmt->case_const->int_value);
      GEN_JUMP("je", case_stmt->lbl_label);
    } else if (case_stmt->nd_type == ND_DEFAULT) {
      GEN_JUMP("jmp", case_stmt->lbl_label);
    }
  }

  gen_stmt(stmt->switch_body);

  GEN_LABEL(stmt->lbl_break);
}

static void gen_while(Stmt *stmt) {
  stmt->lbl_continue = lbl_no++;
  stmt->lbl_break = lbl_no++;

  GEN_LABEL(stmt->lbl_continue);

  GEN_OP(stmt->while_cond, "rax");
  printf("  cmpq $0, %%rax\n");
  GEN_JUMP("je", stmt->lbl_break);

  gen_stmt(stmt->while_body);

  GEN_JUMP("jmp", stmt->lbl_continue);

  GEN_LABEL(stmt->lbl_break);
}

static void gen_do(Stmt *stmt) {
  int lbl_begin = lbl_no++;
  stmt->lbl_continue = lbl_no++;
  stmt->lbl_break = lbl_no++;

  GEN_LABEL(lbl_begin);

  gen_stmt(stmt->do_body);

  GEN_LABEL(stmt->lbl_continue);

  GEN_OP(stmt->do_cond, "rax");
  printf("  cmpq $0, %%rax\n");
  GEN_JUMP("jne", lbl_begin);

  GEN_LABEL(stmt->lbl_break);
}

static void gen_for(Stmt *stmt) {
  int lbl_begin = lbl_no++;
  stmt->lbl_continue = lbl_no++;
  stmt->lbl_break = lbl_no++;

  if (stmt->for_init) {
    if (stmt->for_init->nd_type == ND_DECL) {
      gen_decl_local((Decl *) stmt->for_init);
    } else {
      GEN_EVAL((Expr *) stmt->for_init);
    }
  }

  GEN_LABEL(lbl_begin);

  if (stmt->for_cond) {
    GEN_OP(stmt->for_cond, "rax");
    printf("  cmpq $0, %%rax\n");
    GEN_JUMP("je", stmt->lbl_break);
  }

  gen_stmt(stmt->for_body);

  GEN_LABEL(stmt->lbl_continue);

  if (stmt->for_after) {
    GEN_EVAL(stmt->for_after);
  }
  GEN_JUMP("jmp", lbl_begin);

  GEN_LABEL(stmt->lbl_break);
}

static void gen_goto(Stmt *stmt) {
  GEN_JUMP("jmp", stmt->goto_target->lbl_label);
}

static void gen_continue(Stmt *stmt) {
  GEN_JUMP("jmp", stmt->continue_target->lbl_continue);
}

static void gen_break(Stmt *stmt) {
  GEN_JUMP("jmp", stmt->break_target->lbl_break);
}

static void gen_return(Stmt *stmt) {
  if (stmt->ret_expr) {
    GEN_OP(stmt->ret_expr, "rax");
  }
  GEN_JUMP("jmp", stmt->ret_func->lbl_return);
}

static void gen_stmt(Stmt *stmt) {
  switch (stmt->nd_type) {
    case ND_LABEL:
      gen_label(stmt);
      break;
    case ND_CASE:
      gen_case(stmt);
      break;
    case ND_DEFAULT:
      gen_default(stmt);
      break;

    case ND_COMP:
      for (int i = 0; i < stmt->block_items->length; i++) {
        Node *item = stmt->block_items->buffer[i];
        if (item->nd_type == ND_DECL) {
          gen_decl_local((Decl *) item);
        } else {
          gen_stmt((Stmt *) item);
        }
      }
      break;

    case ND_EXPR:
      if (stmt->expr) {
        GEN_EVAL(stmt->expr);
      }
      break;

    case ND_IF:
      gen_if(stmt);
      break;
    case ND_SWITCH:
      gen_switch(stmt);
      break;

    case ND_WHILE:
      gen_while(stmt);
      break;
    case ND_DO:
      gen_do(stmt);
      break;
    case ND_FOR:
      gen_for(stmt);
      break;

    case ND_GOTO:
      gen_goto(stmt);
      break;
    case ND_CONTINUE:
      gen_continue(stmt);
      break;
    case ND_BREAK:
      gen_break(stmt);
      break;
    case ND_RETURN:
      gen_return(stmt);
      break;

    default:
      assert(false); // unreachable
  }
}

// generation of translation unit

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

static void gen_decl_global(Decl *decl) {
  for (int i = 0; i < decl->symbols->length; i++) {
    Symbol *symbol = decl->symbols->buffer[i];
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

static void gen_func(Func *func) {
  Symbol *symbol = func->symbol;
  Type *type = symbol->type;

  if (type->params->length <= 6) {
    gp_offset = type->params->length * 8;
    overflow_arg_area = 0;
  } else {
    gp_offset = 48;
    overflow_arg_area = (type->params->length - 6) * 8;
  }
  stack_depth = 8;

  // assign labels
  func->lbl_return = lbl_no++;
  for (int i = 0; i < func->label_stmts->length; i++) {
    Stmt *label_stmt = func->label_stmts->buffer[i];
    label_stmt->lbl_label = lbl_no++;
  }

  printf("  .text\n");
  if (symbol->link == LN_EXTERNAL) {
    printf("  .global %s\n", symbol->identifier);
  }
  printf("%s:\n", symbol->identifier);

  GEN_PUSH("rbp");
  printf("  movq %%rsp, %%rbp\n");

  if (func->stack_size > 0) {
    printf("  subq $%d, %%rsp\n", func->stack_size);
    stack_depth += func->stack_size;
  }

  if (type->ellipsis) {
    for (int i = type->params->length; i < 6; i++) {
      printf("  movq %%%s, %d(%%rbp)\n", reg[arg_reg[i]][REG_QUAD], -176 + i * 8);
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
      gen_store_by_offset(arg_reg[i], -param->offset, param->type);
    } else {
      printf("  movq %d(%%rbp), %%rax\n", 16 + (i - 6) * 8);
      gen_store_by_offset(REG_AX, -param->offset, param->type);
    }
  }

  gen_stmt(func->body);

  GEN_LABEL(func->lbl_return);
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

static void gen_trans_unit(TransUnit *trans_unit) {
  if (trans_unit->literals->length > 0) {
    printf("  .section .rodata\n");
    for (int i = 0; i < trans_unit->literals->length; i++) {
      gen_string_literal(trans_unit->literals->buffer[i], i);
    }
  }

  for (int i = 0; i < trans_unit->decls->length; i++) {
    Node *decl = trans_unit->decls->buffer[i];
    if (decl->nd_type == ND_DECL) {
      gen_decl_global((Decl *) decl);
    } else if (decl->nd_type == ND_FUNC) {
      gen_func((Func *) decl);
    }
  }
}

void gen(TransUnit *trans_unit) {
  lbl_no = 0;
  gen_trans_unit(trans_unit);
}
