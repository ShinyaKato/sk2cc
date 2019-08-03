#include "cc.h"

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

static int label_no;

static int gp_offset;
static int overflow_arg_area;

// generation of expression

static void gen_expr(Expr *expr);

#define GEN_LABEL(label) \
  do { \
    printf(".L%d:\n", label); \
  } while (0)

#define GEN_JUMP(inst, label) \
  do { \
    printf("  %s .L%d\n", inst, label); \
  } while (0)

static void gen_lvalue(Expr *expr) {
  switch (expr->nd_type) {
    case ND_IDENTIFIER: {
      switch (expr->symbol->link) {
        case LN_EXTERNAL:
        case LN_INTERNAL: {
          printf("  leaq %s(%%rip), %%%s\n", expr->symbol->identifier, reg[expr->reg][3]);
          break;
        }
        case LN_NONE: {
          printf("  leaq %d(%%rbp), %%%s\n", -expr->symbol->offset, reg[expr->reg][3]);
          break;
        }
      }
      break;
    }
    case ND_INDIRECT: {
      gen_expr(expr->expr);
      break;
    }
    case ND_DOT: {
      gen_lvalue(expr->expr);
      printf("  leaq %d(%%%s), %%%s\n", expr->offset, reg[expr->expr->reg][3], reg[expr->reg][3]);
      break;
    }
    default: assert(false);
  }
}

static void gen_load(Expr *expr) {
  switch (expr->type->ty_type) {
    case TY_BOOL:
    case TY_CHAR:
    case TY_UCHAR: {
      printf("  movb (%%%s), %%%s\n", reg[expr->reg][3], reg[expr->reg][0]);
      break;
    }
    case TY_SHORT:
    case TY_USHORT: {
      printf("  movw (%%%s), %%%s\n", reg[expr->reg][3], reg[expr->reg][1]);
      break;
    }
    case TY_INT:
    case TY_UINT: {
      printf("  movl (%%%s), %%%s\n", reg[expr->reg][3], reg[expr->reg][2]);
      break;
    }
    case TY_LONG:
    case TY_ULONG: {
      printf("  movq (%%%s), %%%s\n", reg[expr->reg][3], reg[expr->reg][3]);
      break;
    }
    case TY_POINTER: {
      if (expr->type == expr->type->original) {
        printf("  movq (%%%s), %%%s\n", reg[expr->reg][3], reg[expr->reg][3]);
      }
      break;
    }
    default: assert(false);
  }
}

static void gen_store(Expr *lvalue, Expr *expr) {
  switch (expr->type->ty_type) {
    case TY_BOOL:
    case TY_CHAR:
    case TY_UCHAR: {
      printf("  movb %%%s, (%%%s)\n", reg[expr->reg][REG_BYTE], reg[lvalue->reg][REG_QUAD]);
      break;
    }
    case TY_SHORT:
    case TY_USHORT: {
      printf("  movw %%%s, (%%%s)\n", reg[expr->reg][REG_WORD], reg[lvalue->reg][REG_QUAD]);
      break;
    }
    case TY_INT:
    case TY_UINT: {
      printf("  movl %%%s, (%%%s)\n", reg[expr->reg][REG_LONG], reg[lvalue->reg][REG_QUAD]);
      break;
    }
    case TY_LONG:
    case TY_ULONG:
    case TY_POINTER: {
      printf("  movq %%%s, (%%%s)\n", reg[expr->reg][REG_QUAD], reg[lvalue->reg][REG_QUAD]);
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

  printf("  movl $%d, (%%rax)\n", gp_offset);
  printf("  movl $48, 4(%%rax)\n");
  printf("  leaq %d(%%rbp), %%rcx\n", 16 + 40 + overflow_arg_area);
  printf("  movq %%rcx, 8(%%rax)\n");
  printf("  leaq -176(%%rbp), %%rcx\n");
  printf("  movq %%rcx, 16(%%rax)\n");
}

static void gen_va_arg(Expr *expr) {
  gen_lvalue(expr->macro_ap);

  int label_overflow = label_no++;
  int label_load = label_no++;

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
  printf("  movq (%%rcx), %%%s\n", reg[expr->reg][3]);
}

static void gen_va_end(Expr *expr) {
  gen_lvalue(expr->macro_ap);
}

static void gen_identifier(Expr *expr) {
  switch (expr->symbol->link) {
    case LN_EXTERNAL:
    case LN_INTERNAL: {
      switch (expr->type->ty_type) {
        case TY_BOOL:
        case TY_CHAR:
        case TY_UCHAR: {
          printf("  movb %s(%%rip), %%%s\n", expr->symbol->identifier, reg[expr->reg][0]);
          break;
        }
        case TY_SHORT:
        case TY_USHORT: {
          printf("  movw %s(%%rip), %%%s\n", expr->symbol->identifier, reg[expr->reg][1]);
          break;
        }
        case TY_INT:
        case TY_UINT: {
          printf("  movl %s(%%rip), %%%s\n", expr->symbol->identifier, reg[expr->reg][2]);
          break;
        }
        case TY_LONG:
        case TY_ULONG: {
          printf("  movq %s(%%rip), %%%s\n", expr->symbol->identifier, reg[expr->reg][3]);
          break;
        }
        case TY_POINTER: {
          if (expr->type == expr->type->original) {
            printf("  movq %s(%%rip), %%%s\n", expr->symbol->identifier, reg[expr->reg][3]);
          } else {
            printf("  leaq %s(%%rip), %%%s\n", expr->symbol->identifier, reg[expr->reg][3]);
          }
          break;
        }
        default: assert(false);
      }
      break;
    }
    case LN_NONE: {
      switch (expr->type->ty_type) {
        case TY_BOOL:
        case TY_CHAR:
        case TY_UCHAR: {
          printf("  movb %d(%%rbp), %%%s\n", -expr->symbol->offset, reg[expr->reg][0]);
          break;
        }
        case TY_SHORT:
        case TY_USHORT: {
          printf("  movw %d(%%rbp), %%%s\n", -expr->symbol->offset, reg[expr->reg][1]);
          break;
        }
        case TY_INT:
        case TY_UINT: {
          printf("  movl %d(%%rbp), %%%s\n", -expr->symbol->offset, reg[expr->reg][2]);
          break;
        }
        case TY_LONG:
        case TY_ULONG: {
          printf("  movq %d(%%rbp), %%%s\n", -expr->symbol->offset, reg[expr->reg][3]);
          break;
        }
        case TY_POINTER: {
          if (expr->type == expr->type->original) {
            printf("  movq %d(%%rbp), %%%s\n", -expr->symbol->offset, reg[expr->reg][3]);
          } else {
            printf("  leaq %d(%%rbp), %%%s\n", -expr->symbol->offset, reg[expr->reg][3]);
          }
          break;
        }
        default: assert(false);
      }
      break;
    }
  }
}

static void gen_integer(Expr *expr) {
  switch (expr->type->ty_type) {
    case TY_INT:
    case TY_UINT: {
      printf("  movl $%llu, %%%s\n", expr->int_value, reg[expr->reg][2]);
      break;
    }
    case TY_LONG:
    case TY_ULONG: {
      printf("  movq $%llu, %%%s\n", expr->int_value, reg[expr->reg][3]);
      break;
    }
    default: assert(false);
  }
}

static void gen_string(Expr *expr) {
  printf("  leaq .S%d(%%rip), %%%s\n", expr->string_label, reg[expr->reg][3]);
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

  // evaluate arguments
  for (int i = 0; i < expr->args->length; i++) {
    Expr *arg = expr->args->buffer[i];
    gen_expr(arg);
  }

  int args_size = 0;
  if (expr->args->length > 6) {
    args_size = (expr->args->length - 6) * 8;
    if (args_size % 16 > 0) {
      args_size += 16 - args_size % 16;
    }
  }

  if (args_size > 0) {
    printf("  subq $%d, %%rsp\n", args_size);
  }

  for (int i = 0; i < expr->args->length; i++) {
    Expr *arg = expr->args->buffer[i];
    if (i < 6) {
      if (arg->reg != arg_reg[i]) {
        printf("  movq %%%s, %%%s\n", reg[arg->reg][3], reg[arg_reg[i]][3]);
      }
    } else {
      printf("  movq %%%s, %d(%%rsp)\n", reg[arg->reg][3], (i - 6) * 8);
    }
  }

  if (!expr->expr->symbol || expr->expr->symbol->type->ellipsis) {
    printf("  movb $0, %%al\n");
  }

  printf("  call %s\n", expr->expr->identifier);

  if (args_size > 0) {
    printf("  addq $%d, %%rsp\n", args_size);
  }

  if (expr->reg != REG_AX) {
    printf("  movq %%rax, %%%s\n", reg[expr->reg][3]);
  }
}

static void gen_dot(Expr *expr) {
  gen_lvalue(expr);
  gen_load(expr);
}

static void gen_address(Expr *expr) {
  gen_lvalue(expr->expr);
}

static void gen_indirect(Expr *expr) {
  gen_lvalue(expr);
  gen_load(expr);
}

static void gen_uminus(Expr *expr) {
  gen_expr(expr->expr);

  switch (expr->expr->type->ty_type) {
    case TY_INT:
    case TY_UINT: {
      printf("  negl %%%s\n", reg[expr->reg][2]);
      break;
    }
    case TY_LONG:
    case TY_ULONG: {
      printf("  negq %%%s\n", reg[expr->reg][3]);
      break;
    }
    default: assert(false);
  }
}

static void gen_not(Expr *expr) {
  gen_expr(expr->expr);

  switch (expr->expr->type->ty_type) {
    case TY_INT:
    case TY_UINT: {
      printf("  notl %%%s\n", reg[expr->reg][2]);
      break;
    }
    case TY_LONG:
    case TY_ULONG: {
      printf("  notq %%%s\n", reg[expr->reg][3]);
      break;
    }
    default: assert(false);
  }
}

static void gen_lnot(Expr *expr) {
  gen_expr(expr->expr);

  printf("  cmpq $0, %%%s\n", reg[expr->expr->reg][3]);
  printf("  sete %%%s\n", reg[expr->reg][0]);
  printf("  movzbl %%%s, %%%s\n", reg[expr->reg][0], reg[expr->reg][2]);
}

static void gen_cast(Expr *expr) {
  gen_expr(expr->expr);

  Type *to = expr->type;
  Type *from = expr->expr->type;

  if (to->ty_type == TY_BOOL) {
    if (from->ty_type == TY_CHAR || from->ty_type == TY_UCHAR) {
      printf("  cmpb $0, %%%s\n", reg[expr->expr->reg][0]);
      printf("  setne %%%s\n", reg[expr->reg][0]);
    } else if (from->ty_type == TY_SHORT || from->ty_type == TY_USHORT) {
      printf("  cmpw $0, %%%s\n", reg[expr->expr->reg][1]);
      printf("  setne %%%s\n", reg[expr->reg][0]);
    } else if (from->ty_type == TY_INT || from->ty_type == TY_UINT) {
      printf("  cmpl $0, %%%s\n", reg[expr->expr->reg][2]);
      printf("  setne %%%s\n", reg[expr->reg][0]);
    }
  } else if (to->ty_type == TY_SHORT || to->ty_type == TY_USHORT) {
    if (from->ty_type == TY_BOOL) {
      printf("  movzbw %%%s, %%%s\n", reg[expr->expr->reg][0], reg[expr->reg][1]);
    } else if (from->ty_type == TY_CHAR) {
      printf("  movsbw %%%s, %%%s\n", reg[expr->expr->reg][0], reg[expr->reg][1]);
    } else if (from->ty_type == TY_UCHAR) {
      printf("  movzbw %%%s, %%%s\n", reg[expr->expr->reg][0], reg[expr->reg][1]);
    }
  } else if (to->ty_type == TY_INT || to->ty_type == TY_UINT) {
    if (from->ty_type == TY_BOOL) {
      printf("  movzbl %%%s, %%%s\n", reg[expr->expr->reg][0], reg[expr->reg][2]);
    } else if (from->ty_type == TY_CHAR) {
      printf("  movsbl %%%s, %%%s\n", reg[expr->expr->reg][0], reg[expr->reg][2]);
    } else if (from->ty_type == TY_UCHAR) {
      printf("  movzbl %%%s, %%%s\n", reg[expr->expr->reg][0], reg[expr->reg][2]);
    } else if (from->ty_type == TY_SHORT) {
      printf("  movswl %%%s, %%%s\n", reg[expr->expr->reg][1], reg[expr->reg][2]);
    } else if (from->ty_type == TY_USHORT) {
      printf("  movzwl %%%s, %%%s\n", reg[expr->expr->reg][1], reg[expr->reg][2]);
    }
  } else if (to->ty_type == TY_LONG || to->ty_type == TY_ULONG) {
    if (from->ty_type == TY_BOOL) {
      printf("  movzbl %%%s, %%%s\n", reg[expr->expr->reg][0], reg[expr->reg][2]);
    } else if (from->ty_type == TY_CHAR) {
      printf("  movsbq %%%s, %%%s\n", reg[expr->expr->reg][0], reg[expr->reg][3]);
    } else if (from->ty_type == TY_UCHAR) {
      printf("  movzbl %%%s, %%%s\n", reg[expr->expr->reg][0], reg[expr->reg][2]);
    } else if (from->ty_type == TY_SHORT) {
      printf("  movswq %%%s, %%%s\n", reg[expr->expr->reg][1], reg[expr->reg][3]);
    } else if (from->ty_type == TY_USHORT) {
      printf("  movzwl %%%s, %%%s\n", reg[expr->expr->reg][1], reg[expr->reg][2]);
    } else if (from->ty_type == TY_INT) {
      printf("  movslq %%%s, %%%s\n", reg[expr->expr->reg][2], reg[expr->reg][3]);
    }
  }
}

static void gen_mul(Expr *expr) {
  gen_expr(expr->lhs);
  gen_expr(expr->rhs);

  if (expr->lhs->reg != REG_AX) {
    printf("  movq %%%s, %%rax\n", reg[expr->lhs->reg][3]);
  }

  switch (expr->type->ty_type) {
    case TY_INT: {
      printf("  imull %%%s\n", reg[expr->rhs->reg][2]);
      break;
    }
    case TY_UINT: {
      printf("  mull %%%s\n", reg[expr->rhs->reg][2]);
      break;
    }
    case TY_LONG: {
      printf("  imulq %%%s\n", reg[expr->rhs->reg][3]);
      break;
    }
    case TY_ULONG: {
      printf("  mulq %%%s\n", reg[expr->rhs->reg][3]);
      break;
    }
    default: assert(false);
  }

  if (expr->reg != REG_AX) {
    printf("  movq %%rax, %%%s\n", reg[expr->reg][3]);
  }
}

static void gen_div(Expr *expr) {
  gen_expr(expr->lhs);
  gen_expr(expr->rhs);

  if (expr->lhs->reg != REG_AX) {
    printf("  movq %%%s, %%rax\n", reg[expr->lhs->reg][3]);
  }

  switch (expr->type->ty_type) {
    case TY_INT: {
      // cltd: sign extension (eax -> edx:eax)
      // idivl: signed devide edx:eax by 32-bit register.
      printf("  cltd\n");
      printf("  idivl %%%s\n", reg[expr->rhs->reg][2]);
      break;
    }
    case TY_UINT: {
      // divl: unsigned devide edx:eax by 32-bit register.
      printf("  movl $0, %%edx\n");
      printf("  divl %%%s\n", reg[expr->rhs->reg][2]);
      break;
    }
    case TY_LONG: {
      // cqto: sign extension (rax -> rdx:rax)
      // idivq: signed devide rdx:rax by 64-bit register.
      printf("  cqto\n");
      printf("  idivq %%%s\n", reg[expr->rhs->reg][3]);
      break;
    }
    case TY_ULONG: {
      // divq: unsigned devide rdx:rax by 64-bit register.
      printf("  movq $0, %%rdx\n");
      printf("  divq %%%s\n", reg[expr->rhs->reg][3]);
      break;
    }
    default: assert(false);
  }

  if (expr->reg != REG_AX) {
    printf("  movq %%rax, %%%s\n", reg[expr->reg][3]);
  }
}

static void gen_mod(Expr *expr) {
  gen_expr(expr->lhs);
  gen_expr(expr->rhs);

  if (expr->lhs->reg != REG_AX) {
    printf("  movq %%%s, %%rax\n", reg[expr->lhs->reg][3]);
  }

  switch (expr->type->ty_type) {
    case TY_INT: {
      // cltd: sign extension (eax -> edx:eax)
      // idivl: signed devide edx:eax by 32-bit register.
      printf("  cltd\n");
      printf("  idivl %%%s\n", reg[expr->rhs->reg][2]);
      break;
    }
    case TY_UINT: {
      // divl: unsigned devide edx:eax by 32-bit register.
      printf("  movl $0, %%edx\n");
      printf("  divl %%%s\n", reg[expr->rhs->reg][2]);
      break;
    }
    case TY_LONG: {
      // cqto: sign extension (rax -> rdx:rax)
      // idivq: signed devide rdx:rax by 64-bit register.
      printf("  cqto\n");
      printf("  idivq %%%s\n", reg[expr->rhs->reg][3]);
      break;
    }
    case TY_ULONG: {
      // divq: unsigned devide rdx:rax by 64-bit register.
      printf("  movq $0, %%rdx\n");
      printf("  divq %%%s\n", reg[expr->rhs->reg][3]);
      break;
    }
    default: assert(false);
  }

  if (expr->reg != REG_DX) {
    printf("  movq %%rdx, %%%s\n", reg[expr->reg][3]);
  }
}

static void gen_add(Expr *expr) {
  gen_expr(expr->lhs);
  gen_expr(expr->rhs);

  switch (expr->type->ty_type) {
    case TY_INT:
    case TY_UINT: {
      printf("  addl %%%s, %%%s\n", reg[expr->rhs->reg][2], reg[expr->lhs->reg][2]);
      break;
    }
    case TY_LONG:
    case TY_ULONG: {
      printf("  addq %%%s, %%%s\n", reg[expr->rhs->reg][3], reg[expr->lhs->reg][3]);
      break;
    }
    default: assert(false);
  }

  if (expr->lhs->reg != expr->reg) {
    printf("  movq %%%s, %%%s\n", reg[expr->lhs->reg][3], reg[expr->reg][3]);
  }
}

static void gen_sub(Expr *expr) {
  gen_expr(expr->lhs);
  gen_expr(expr->rhs);

  switch (expr->type->ty_type) {
    case TY_INT:
    case TY_UINT: {
      printf("  subl %%%s, %%%s\n", reg[expr->rhs->reg][2], reg[expr->lhs->reg][2]);
      break;
    }
    case TY_LONG:
    case TY_ULONG: {
      printf("  subq %%%s, %%%s\n", reg[expr->rhs->reg][3], reg[expr->lhs->reg][3]);
      break;
    }
    default: assert(false);
  }

  if (expr->lhs->reg != expr->reg) {
    printf("  movq %%%s, %%%s\n", reg[expr->lhs->reg][3], reg[expr->reg][3]);
  }
}

static void gen_lshift(Expr *expr) {
  gen_expr(expr->lhs);
  gen_expr(expr->rhs);

  if (expr->rhs->reg != REG_CX) {
    printf("movq %%%s, %%rcx\n", reg[expr->rhs->reg][3]);
  }

  switch (expr->type->ty_type) {
    case TY_INT:
    case TY_UINT: {
      printf("  sall %%cl, %%%s\n", reg[expr->lhs->reg][2]);
      break;
    }
    case TY_LONG:
    case TY_ULONG: {
      printf("  salq %%cl, %%%s\n", reg[expr->lhs->reg][3]);
      break;
    }
    default: assert(false);
  }

  if (expr->lhs->reg != expr->reg) {
    printf("  movq %%%s, %%%s\n", reg[expr->lhs->reg][3], reg[expr->reg][3]);
  }
}

static void gen_rshift(Expr *expr) {
  gen_expr(expr->lhs);
  gen_expr(expr->rhs);

  if (expr->rhs->reg != REG_CX) {
    printf("movq %%%s, %%rcx\n", reg[expr->rhs->reg][3]);
  }

  switch (expr->type->ty_type) {
    case TY_INT:
    case TY_UINT: {
      printf("  sarl %%cl, %%%s\n", reg[expr->lhs->reg][2]);
      break;
    }
    case TY_LONG:
    case TY_ULONG: {
      printf("  sarq %%cl, %%%s\n", reg[expr->lhs->reg][3]);
      break;
    }
    default: assert(false);
  }

  if (expr->lhs->reg != expr->reg) {
    printf("  movq %%%s, %%%s\n", reg[expr->lhs->reg][3], reg[expr->reg][3]);
  }
}

static void gen_lt(Expr *expr) {
  gen_expr(expr->lhs);
  gen_expr(expr->rhs);

  switch (expr->lhs->type->ty_type) {
    case TY_INT: {
      printf("  cmpl %%%s, %%%s\n", reg[expr->rhs->reg][2], reg[expr->lhs->reg][2]);
      printf("  setl %%%s\n", reg[expr->reg][0]);
      break;
    }
    case TY_UINT: {
      printf("  cmpl %%%s, %%%s\n", reg[expr->rhs->reg][2], reg[expr->lhs->reg][2]);
      printf("  setb %%%s\n", reg[expr->reg][0]);
      break;
    }
    case TY_LONG: {
      printf("  cmpq %%%s, %%%s\n", reg[expr->rhs->reg][3], reg[expr->lhs->reg][3]);
      printf("  setl %%%s\n", reg[expr->reg][0]);
      break;
    }
    case TY_ULONG:
    case TY_POINTER: {
      printf("  cmpq %%%s, %%%s\n", reg[expr->rhs->reg][3], reg[expr->lhs->reg][3]);
      printf("  setb %%%s\n", reg[expr->reg][0]);
      break;
    }
    default: assert(false);
  }
  printf("  movzbl %%%s, %%%s\n", reg[expr->reg][0], reg[expr->reg][2]);
}

static void gen_lte(Expr *expr) {
  gen_expr(expr->lhs);
  gen_expr(expr->rhs);

  switch (expr->lhs->type->ty_type) {
    case TY_INT: {
      printf("  cmpl %%%s, %%%s\n", reg[expr->rhs->reg][2], reg[expr->lhs->reg][2]);
      printf("  setle %%%s\n", reg[expr->reg][0]);
      break;
    }
    case TY_UINT: {
      printf("  cmpl %%%s, %%%s\n", reg[expr->rhs->reg][2], reg[expr->lhs->reg][2]);
      printf("  setbe %%%s\n", reg[expr->reg][0]);
      break;
    }
    case TY_LONG: {
      printf("  cmpq %%%s, %%%s\n", reg[expr->rhs->reg][3], reg[expr->lhs->reg][3]);
      printf("  setle %%%s\n", reg[expr->reg][0]);
      break;
    }
    case TY_ULONG:
    case TY_POINTER: {
      printf("  cmpq %%%s, %%%s\n", reg[expr->rhs->reg][3], reg[expr->lhs->reg][3]);
      printf("  setbe %%%s\n", reg[expr->reg][0]);
      break;
    }
    default: assert(false);
  }
  printf("  movzbl %%al, %%eax\n");
}

static void gen_eq(Expr *expr) {
  gen_expr(expr->lhs);
  gen_expr(expr->rhs);

  switch (expr->lhs->type->ty_type) {
    case TY_INT:
    case TY_UINT: {
      printf("  cmpl %%%s, %%%s\n", reg[expr->rhs->reg][2], reg[expr->lhs->reg][2]);
      break;
    }
    case TY_LONG:
    case TY_ULONG:
    case TY_POINTER: {
      printf("  cmpq %%%s, %%%s\n", reg[expr->rhs->reg][3], reg[expr->lhs->reg][3]);
      break;
    }
    default: assert(false);
  }
  printf("  sete %%%s\n", reg[expr->reg][0]);
  printf("  movzbl %%%s, %%%s\n", reg[expr->reg][0], reg[expr->reg][2]);
}

static void gen_neq(Expr *expr) {
  gen_expr(expr->lhs);
  gen_expr(expr->rhs);

  switch (expr->lhs->type->ty_type) {
    case TY_INT:
    case TY_UINT: {
      printf("  cmpl %%%s, %%%s\n", reg[expr->rhs->reg][2], reg[expr->lhs->reg][2]);
      break;
    }
    case TY_LONG:
    case TY_ULONG:
    case TY_POINTER: {
      printf("  cmpq %%%s, %%%s\n", reg[expr->rhs->reg][3], reg[expr->lhs->reg][3]);
      break;
    }
    default: assert(false);
  }
  printf("  setne %%%s\n", reg[expr->reg][0]);
  printf("  movzbl %%%s, %%%s\n", reg[expr->reg][0], reg[expr->reg][2]);
}

static void gen_and(Expr *expr) {
  gen_expr(expr->lhs);
  gen_expr(expr->rhs);

  switch (expr->type->ty_type) {
    case TY_INT:
    case TY_UINT: {
      printf("  andl %%%s, %%%s\n", reg[expr->rhs->reg][2], reg[expr->lhs->reg][2]);
      break;
    }
    case TY_LONG:
    case TY_ULONG: {
      printf("  andq %%%s, %%%s\n", reg[expr->rhs->reg][3], reg[expr->lhs->reg][3]);
      break;
    }
    default: assert(false);
  }

  if (expr->lhs->reg != expr->reg) {
    printf("  movq %%%s, %%%s\n", reg[expr->lhs->reg][3], reg[expr->reg][3]);
  }
}

static void gen_xor(Expr *expr) {
  gen_expr(expr->lhs);
  gen_expr(expr->rhs);

  switch (expr->type->ty_type) {
    case TY_INT:
    case TY_UINT: {
      printf("  xorl %%%s, %%%s\n", reg[expr->rhs->reg][2], reg[expr->lhs->reg][2]);
      break;
    }
    case TY_LONG:
    case TY_ULONG: {
      printf("  xorq %%%s, %%%s\n", reg[expr->rhs->reg][3], reg[expr->lhs->reg][3]);
      break;
    }
    default: assert(false);
  }

  if (expr->lhs->reg != expr->reg) {
    printf("  movq %%%s, %%%s\n", reg[expr->lhs->reg][3], reg[expr->reg][3]);
  }
}

static void gen_or(Expr *expr) {
  gen_expr(expr->lhs);
  gen_expr(expr->rhs);

  switch (expr->type->ty_type) {
    case TY_INT:
    case TY_UINT: {
      printf("  orl %%%s, %%%s\n", reg[expr->rhs->reg][2], reg[expr->lhs->reg][2]);
      break;
    }
    case TY_LONG:
    case TY_ULONG: {
      printf("  orq %%%s, %%%s\n", reg[expr->rhs->reg][3], reg[expr->lhs->reg][3]);
      break;
    }
    default: assert(false);
  }

  if (expr->lhs->reg != expr->reg) {
    printf("  movq %%%s, %%%s\n", reg[expr->lhs->reg][3], reg[expr->reg][3]);
  }
}

static void gen_land(Expr *expr) {
  int label_false = label_no++;
  int label_end = label_no++;

  gen_expr(expr->lhs);
  printf("  cmpq $0, %%%s\n", reg[expr->lhs->reg][3]);
  GEN_JUMP("je", label_false);

  gen_expr(expr->rhs);
  printf("  cmpq $0, %%%s\n", reg[expr->rhs->reg][3]);
  GEN_JUMP("je", label_false);

  printf("  movl $1, %%%s\n", reg[expr->reg][2]);
  GEN_JUMP("jmp", label_end);

  GEN_LABEL(label_false);
  printf("  movl $0, %%%s\n", reg[expr->reg][2]);

  GEN_LABEL(label_end);
}

static void gen_lor(Expr *expr) {
  int label_true = label_no++;
  int label_end = label_no++;

  gen_expr(expr->lhs);
  printf("  cmpq $0, %%%s\n", reg[expr->lhs->reg][3]);
  GEN_JUMP("jne", label_true);

  gen_expr(expr->rhs);
  printf("  cmpq $0, %%%s\n", reg[expr->rhs->reg][3]);
  GEN_JUMP("jne", label_true);

  printf("  movl $0, %%%s\n", reg[expr->reg][2]);
  GEN_JUMP("jmp", label_end);

  GEN_LABEL(label_true);
  printf("  movl $1, %%%s\n", reg[expr->reg][2]);

  GEN_LABEL(label_end);
}

static void gen_condition(Expr *expr) {
  int label_false = label_no++;
  int label_end = label_no++;

  gen_expr(expr->cond);
  printf("  cmpq $0, %%%s\n", reg[expr->cond->reg][3]);
  GEN_JUMP("je", label_false);

  gen_expr(expr->lhs);
  printf("  movq %%%s, %%%s\n", reg[expr->lhs->reg][3], reg[expr->reg][3]);
  GEN_JUMP("jmp", label_end);

  GEN_LABEL(label_false);
  gen_expr(expr->rhs);
  printf("  movq %%%s, %%%s\n", reg[expr->rhs->reg][3], reg[expr->reg][3]);

  GEN_LABEL(label_end);
}

static void gen_assign(Expr *expr) {
  gen_lvalue(expr->lhs);
  gen_expr(expr->rhs);

  gen_store(expr->lhs, expr->rhs);

  if (expr->rhs->reg != expr->reg) {
    printf("  movq %%%s, %%%s\n", reg[expr->rhs->reg][3], reg[expr->reg][3]);
  }
}

static void gen_comma(Expr *expr) {
  gen_expr(expr->lhs);
  gen_expr(expr->rhs);

  if (expr->rhs->reg != expr->reg) {
    printf("  movq %%%s, %%%s\n", reg[expr->rhs->reg][3], reg[expr->reg][3]);
  }
}

static void gen_expr(Expr *expr) {
  /* fprintf(stderr, "%d: %s\n", expr->nd_type, reg[expr->reg][1]); */

  switch (expr->nd_type) {
    case ND_VA_START: gen_va_start(expr); break;
    case ND_VA_ARG: gen_va_arg(expr); break;
    case ND_VA_END: gen_va_end(expr); break;
    case ND_IDENTIFIER: gen_identifier(expr); break;
    case ND_INTEGER: gen_integer(expr); break;
    case ND_STRING: gen_string(expr); break;
    case ND_CALL: gen_call(expr); break;
    case ND_DOT: gen_dot(expr); break;
    case ND_ADDRESS: gen_address(expr); break;
    case ND_INDIRECT: gen_indirect(expr); break;
    case ND_UMINUS: gen_uminus(expr); break;
    case ND_NOT: gen_not(expr); break;
    case ND_LNOT: gen_lnot(expr); break;
    case ND_CAST: gen_cast(expr); break;
    case ND_MUL: gen_mul(expr); break;
    case ND_DIV: gen_div(expr); break;
    case ND_MOD: gen_mod(expr); break;
    case ND_ADD: gen_add(expr); break;
    case ND_SUB: gen_sub(expr); break;
    case ND_LSHIFT: gen_lshift(expr); break;
    case ND_RSHIFT: gen_rshift(expr); break;
    case ND_LT: gen_lt(expr); break;
    case ND_LTE: gen_lte(expr); break;
    case ND_EQ: gen_eq(expr); break;
    case ND_NEQ: gen_neq(expr); break;
    case ND_AND: gen_and(expr); break;
    case ND_XOR: gen_xor(expr); break;
    case ND_OR: gen_or(expr); break;
    case ND_LAND: gen_land(expr); break;
    case ND_LOR: gen_lor(expr); break;
    case ND_CONDITION: gen_condition(expr); break;
    case ND_ASSIGN: gen_assign(expr); break;
    case ND_COMMA: gen_comma(expr); break;
    default: assert(false); // unreachable
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
    gen_expr(init->expr);
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
  GEN_LABEL(stmt->label_no);
  gen_stmt(stmt->label_stmt);
}

static void gen_case(Stmt *stmt) {
  GEN_LABEL(stmt->label_no);
  gen_stmt(stmt->case_stmt);
}

static void gen_default(Stmt *stmt) {
  GEN_LABEL(stmt->label_no);
  gen_stmt(stmt->default_stmt);
}

static void gen_comp_stmt(Stmt *stmt) {
  for (int i = 0; i < stmt->block_items->length; i++) {
    Node *item = stmt->block_items->buffer[i];
    if (item->nd_type == ND_DECL) {
      gen_decl_local((Decl *) item);
    } else {
      gen_stmt((Stmt *) item);
    }
  }
}

static void gen_expr_stmt(Stmt *stmt) {
  if (stmt->expr) {
    gen_expr(stmt->expr);
  }
}

static void gen_if(Stmt *stmt) {
  int label_else = label_no++;
  int label_end = label_no++;

  gen_expr(stmt->if_cond);
  printf("  cmpq $0, %%%s\n", reg[stmt->if_cond->reg][3]);
  GEN_JUMP("je", label_else);

  gen_stmt(stmt->then_body);
  GEN_JUMP("jmp", label_end);

  GEN_LABEL(label_else);

  if (stmt->else_body) {
    gen_stmt(stmt->else_body);
  }

  GEN_LABEL(label_end);
}

static void gen_switch(Stmt *stmt) {
  stmt->label_break = label_no++;

  gen_expr(stmt->switch_cond);
  for (int i = 0; i < stmt->switch_cases->length; i++) {
    Stmt *case_stmt = stmt->switch_cases->buffer[i];
    case_stmt->label_no = label_no++;
    if (case_stmt->nd_type == ND_CASE) {
      printf("  cmpq $%llu, %%%s\n", case_stmt->case_const->int_value, reg[stmt->switch_cond->reg][3]);
      GEN_JUMP("je", case_stmt->label_no);
    } else if (case_stmt->nd_type == ND_DEFAULT) {
      GEN_JUMP("jmp", case_stmt->label_no);
    }
  }

  gen_stmt(stmt->switch_body);

  GEN_LABEL(stmt->label_break);
}

static void gen_while(Stmt *stmt) {
  stmt->label_continue = label_no++;
  stmt->label_break = label_no++;

  GEN_LABEL(stmt->label_continue);

  gen_expr(stmt->while_cond);
  printf("  cmpq $0, %%%s\n", reg[stmt->while_cond->reg][3]);
  GEN_JUMP("je", stmt->label_break);

  gen_stmt(stmt->while_body);

  GEN_JUMP("jmp", stmt->label_continue);

  GEN_LABEL(stmt->label_break);
}

static void gen_do(Stmt *stmt) {
  int label_begin = label_no++;
  stmt->label_continue = label_no++;
  stmt->label_break = label_no++;

  GEN_LABEL(label_begin);

  gen_stmt(stmt->do_body);

  GEN_LABEL(stmt->label_continue);

  gen_expr(stmt->do_cond);
  printf("  cmpq $0, %%%s\n", reg[stmt->do_cond->reg][3]);
  GEN_JUMP("jne", label_begin);

  GEN_LABEL(stmt->label_break);
}

static void gen_for(Stmt *stmt) {
  int label_begin = label_no++;
  stmt->label_continue = label_no++;
  stmt->label_break = label_no++;

  if (stmt->for_init) {
    if (stmt->for_init->nd_type == ND_DECL) {
      gen_decl_local((Decl *) stmt->for_init);
    } else {
      gen_expr((Expr *) stmt->for_init);
    }
  }

  GEN_LABEL(label_begin);

  if (stmt->for_cond) {
    gen_expr(stmt->for_cond);
    printf("  cmpq $0, %%%s\n", reg[stmt->for_cond->reg][3]);
    GEN_JUMP("je", stmt->label_break);
  }

  gen_stmt(stmt->for_body);

  GEN_LABEL(stmt->label_continue);

  if (stmt->for_after) {
    gen_expr(stmt->for_after);
  }
  GEN_JUMP("jmp", label_begin);

  GEN_LABEL(stmt->label_break);
}

static void gen_goto(Stmt *stmt) {
  GEN_JUMP("jmp", stmt->goto_target->label_no);
}

static void gen_continue(Stmt *stmt) {
  GEN_JUMP("jmp", stmt->continue_target->label_continue);
}

static void gen_break(Stmt *stmt) {
  GEN_JUMP("jmp", stmt->break_target->label_break);
}

static void gen_return(Stmt *stmt) {
  if (stmt->ret_expr) {
    gen_expr(stmt->ret_expr);
  }
  GEN_JUMP("jmp", stmt->ret_func->label_return);
}

static void gen_stmt(Stmt *stmt) {
  switch (stmt->nd_type) {
    case ND_LABEL: gen_label(stmt); break;
    case ND_CASE: gen_case(stmt); break;
    case ND_DEFAULT: gen_default(stmt); break;
    case ND_COMP: gen_comp_stmt(stmt); break;
    case ND_EXPR: gen_expr_stmt(stmt); break;
    case ND_IF: gen_if(stmt); break;
    case ND_SWITCH: gen_switch(stmt); break;
    case ND_WHILE: gen_while(stmt); break;
    case ND_DO: gen_do(stmt); break;
    case ND_FOR: gen_for(stmt); break;
    case ND_GOTO: gen_goto(stmt); break;
    case ND_CONTINUE: gen_continue(stmt); break;
    case ND_BREAK: gen_break(stmt); break;
    case ND_RETURN: gen_return(stmt); break;
    default: assert(false); // unreachable
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

  // assign labels
  func->label_return = label_no++;
  for (int i = 0; i < func->label_stmts->length; i++) {
    Stmt *label_stmt = func->label_stmts->buffer[i];
    label_stmt->label_no = label_no++;
  }

  printf("  .text\n");
  if (symbol->link == LN_EXTERNAL) {
    printf("  .global %s\n", symbol->identifier);
  }
  printf("%s:\n", symbol->identifier);

  printf("  pushq %%r15\n");
  printf("  pushq %%r14\n");
  printf("  pushq %%r13\n");
  printf("  pushq %%r12\n");
  printf("  pushq %%rbx\n");
  printf("  pushq %%rbp\n");
  printf("  movq %%rsp, %%rbp\n");

  int stack_size = func->stack_size;
  if (stack_size % 16 > 0) {
    stack_size += 16 - stack_size % 16;
  }
  printf("  subq $%d, %%rsp\n", stack_size + 8);

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
      printf("  movq %d(%%rbp), %%rax\n", 16 + 40 + (i - 6) * 8);
      gen_store_by_offset(REG_AX, -param->offset, param->type);
    }
  }

  gen_stmt(func->body);

  GEN_LABEL(func->label_return);
  printf("  leave\n");
  printf("  popq %%rbx\n");
  printf("  popq %%r12\n");
  printf("  popq %%r13\n");
  printf("  popq %%r14\n");
  printf("  popq %%r15\n");
  printf("  ret\n");
}

static void gen_string_literal(String *string, int label) {
  printf(".S%d:\n", label);
  printf("  .ascii \"");
  for (int i = 0; i < string->length; i++) {
    char c = string->buffer[i];
    switch (c) {
      case '\\': printf("\\\\"); break;
      case '"': printf("\\\""); break;
      case '\a': printf("\\a"); break;
      case '\b': printf("\\b"); break;
      case '\f': printf("\\f"); break;
      case '\n': printf("\\n"); break;
      case '\r': printf("\\r"); break;
      case '\t': printf("\\t"); break;
      case '\v': printf("\\v"); break;
      default: printf(isprint(c) ? "%c" : "\\%o", c);
    }
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
  label_no = 0;
  gen_trans_unit(trans_unit);
}
