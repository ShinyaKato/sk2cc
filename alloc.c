#include "cc.h"

#define AVAILABLE_REGS 14
#define CALLER_SAVED_REGS 9
#define CALLEE_SAVED_REGS 5
#define ARG_REGS 6

// AX, CX, DX, SI, DI, R8, R9, R10, R11, BX, R12, R13, R14, R15
static RegCode available_regs[AVAILABLE_REGS] = {
  0, 1, 2, 6, 7, 8, 9, 10, 11, 3, 12, 13, 14, 15
};

// AX, CX, DX, SI, DI, R8, R9, R10, R11
static RegCode caller_saved_regs[CALLER_SAVED_REGS] = {
  0, 1, 2, 6, 7, 8, 9, 10, 11
};

// DI, SI, DX, CX, R8, R9
static RegCode arg_regs[ARG_REGS] = {
  7, 6, 2, 1, 8, 9
};

static bool *func_used;

// utilities

static RegCode choose(bool *used) {
  for (int i = 0; i < AVAILABLE_REGS; i++) {
    RegCode reg = available_regs[i];
    if (!used[reg]) {
      func_used[reg] = true;
      return reg;
    }
  }

  assert(false && "failed to allocate register.");
}

static RegCode choose_arg(bool *used, int index, int length) {
  if (index < 6 && !used[index]) {
    return arg_regs[index];
  }

  // if the register is already used in the subtree,
  // allocate another register except DI, SI, DX, CX, R8, R9.

  for (int i = 0; i < AVAILABLE_REGS; i++) {
    RegCode reg = available_regs[i];

    bool skip = false;
    for (int j = 0; j < ARG_REGS; j++) {
      if (reg == arg_regs[j]) {
        skip = true;
        break;
      }
    }
    if (skip) continue;

    if (!used[reg]) {
      func_used[reg] = true;
      return reg;
    }
  }

  assert(false && "failed to allocate register.");
}

static void merge(Expr *parent, Expr *child) {
  for (int i = 0; i < REGS; i++) {
    parent->used[i] = parent->used[i] || child->used[i];
  }
}

// allocation of expression

static void alloc_expr(Expr *expr, RegCode reg);

static void alloc_va_start(Expr *expr, RegCode reg) {
  alloc_expr(expr->macro_ap, REG_AX);
  merge(expr, expr->macro_ap);
}

static void alloc_va_arg(Expr *expr, RegCode reg) {
  alloc_expr(expr->macro_ap, REG_AX);
  merge(expr, expr->macro_ap);

  expr->used[REG_DX] = true; // used in code generation
}

static void alloc_va_end(Expr *expr, RegCode reg) {
  alloc_expr(expr->macro_ap, REG_AX);
  merge(expr, expr->macro_ap);
}

static void alloc_primary(Expr *expr, RegCode reg) {
  // nothing to do
}

static void alloc_call(Expr *expr, RegCode reg) {
  for (int i = expr->args->length - 1; i >= 0; i--) {
    Expr *arg = expr->args->buffer[i];
    alloc_expr(arg, choose_arg(expr->used, i, expr->args->length));
    merge(expr, arg);
  }

  // add all caller-saved registers to 'used' to avoid register saving.
  for (int i = 0; i < CALLER_SAVED_REGS; i++) {
    expr->used[caller_saved_regs[i]] = true;
  }
}

static void alloc_unary(Expr *expr, RegCode reg) {
  alloc_expr(expr->expr, reg);
  merge(expr, expr->expr);
}

static void alloc_binary(Expr *expr, RegCode reg) {
  alloc_expr(expr->rhs, reg);
  merge(expr, expr->rhs);

  alloc_expr(expr->lhs, choose(expr->used));
  merge(expr, expr->lhs);
}

static void alloc_mul(Expr *expr, RegCode reg) {
  alloc_expr(expr->rhs, REG_DX);
  merge(expr, expr->rhs);

  alloc_expr(expr->lhs, choose(expr->used));
  merge(expr, expr->lhs);

  expr->used[REG_AX] = true;
  expr->used[REG_DX] = true;
}

static void alloc_div(Expr *expr, RegCode reg) {
  alloc_expr(expr->rhs, REG_CX);
  merge(expr, expr->rhs);

  alloc_expr(expr->lhs, choose(expr->used));
  merge(expr, expr->lhs);

  expr->used[REG_AX] = true;
  expr->used[REG_DX] = true;
}

static void alloc_sub(Expr *expr, RegCode reg) {
  Expr *tmp = expr->lhs;
  expr->lhs = expr->rhs;
  expr->rhs = tmp;

  alloc_binary(expr, reg);
}

static void alloc_shift(Expr *expr, RegCode reg) {
  alloc_expr(expr->rhs, REG_CX);
  merge(expr, expr->rhs);

  alloc_expr(expr->lhs, choose(expr->used));
  merge(expr, expr->lhs);
}

static void alloc_logical(Expr *expr, RegCode reg) {
  alloc_expr(expr->lhs, reg);
  merge(expr, expr->lhs);

  alloc_expr(expr->rhs, reg);
  merge(expr, expr->rhs);
}

static void alloc_condition(Expr *expr, RegCode reg) {
  alloc_expr(expr->cond, reg);
  merge(expr, expr->cond);

  alloc_expr(expr->lhs, reg);
  merge(expr, expr->lhs);

  alloc_expr(expr->rhs, reg);
  merge(expr, expr->rhs);
}

static void alloc_comma(Expr *expr, RegCode reg) {
  alloc_expr(expr->lhs, reg);
  merge(expr, expr->lhs);

  alloc_expr(expr->rhs, reg);
  merge(expr, expr->rhs);
}

static void alloc_expr(Expr *expr, RegCode reg) {
  switch (expr->nd_type) {
    case ND_VA_START:   alloc_va_start(expr, reg); break;
    case ND_VA_ARG:     alloc_va_arg(expr, reg); break;
    case ND_VA_END:     alloc_va_end(expr, reg); break;
    case ND_IDENTIFIER: alloc_primary(expr, reg); break;
    case ND_INTEGER:    alloc_primary(expr, reg); break;
    case ND_STRING:     alloc_primary(expr, reg); break;
    case ND_CALL:       alloc_call(expr, reg); break;
    case ND_DOT:        alloc_unary(expr, reg); break;
    case ND_ADDRESS:    alloc_unary(expr, reg); break;
    case ND_INDIRECT:   alloc_unary(expr, reg); break;
    case ND_UMINUS:     alloc_unary(expr, reg); break;
    case ND_NOT:        alloc_unary(expr, reg); break;
    case ND_LNOT:       alloc_unary(expr, reg); break;
    case ND_CAST:       alloc_unary(expr, reg); break;
    case ND_MUL:        alloc_mul(expr, reg); break;
    case ND_DIV:        alloc_div(expr, reg); break;
    case ND_MOD:        alloc_div(expr, reg); break;
    case ND_ADD:        alloc_binary(expr, reg); break;
    case ND_SUB:        alloc_sub(expr, reg); break;
    case ND_LSHIFT:     alloc_shift(expr, reg); break;
    case ND_RSHIFT:     alloc_shift(expr, reg); break;
    case ND_LT:         alloc_binary(expr, reg); break;
    case ND_LTE:        alloc_binary(expr, reg); break;
    case ND_EQ:         alloc_binary(expr, reg); break;
    case ND_NEQ:        alloc_binary(expr, reg); break;
    case ND_AND:        alloc_binary(expr, reg); break;
    case ND_XOR:        alloc_binary(expr, reg); break;
    case ND_OR:         alloc_binary(expr, reg); break;
    case ND_LAND:       alloc_logical(expr, reg); break;
    case ND_LOR:        alloc_logical(expr, reg); break;
    case ND_CONDITION:  alloc_condition(expr, reg); break;
    case ND_ASSIGN:     alloc_binary(expr, reg); break;
    case ND_COMMA:      alloc_comma(expr, reg); break;
    default:            assert(false); // unreachable
  }

  expr->reg = reg;
  expr->used[reg] = true;
}

// allocation of declaration

static void alloc_init(Initializer *init) {
  if (init->list) {
    for (int i = 0; i < init->list->length; i++) {
      Initializer *item = init->list->buffer[i];
      alloc_init(item);
    }
  } else if (init->expr) {
    alloc_expr(init->expr, REG_AX);
  }
}

static void alloc_decl(Decl *decl) {
  for (int i = 0; i < decl->symbols->length; i++) {
    Symbol *symbol = decl->symbols->buffer[i];
    if (!symbol->definition) continue;
    if (symbol->init) {
      alloc_init(symbol->init);
    }
  }
}

// allocation of statement

static void alloc_stmt(Stmt *stmt) {
  switch (stmt->nd_type) {
    case ND_LABEL: {
      alloc_stmt(stmt->label_stmt);
      break;
    }
    case ND_CASE: {
      alloc_stmt(stmt->case_stmt);
      break;
    }
    case ND_DEFAULT: {
      alloc_stmt(stmt->default_stmt);
      break;
    }
    case ND_COMP: {
      for (int i = 0; i < stmt->block_items->length; i++) {
        Node *item = stmt->block_items->buffer[i];
        if (item->nd_type == ND_DECL) {
          alloc_decl((Decl *) item);
        } else {
          alloc_stmt((Stmt *) item);
        }
      }
      break;
    }
    case ND_EXPR: {
      if (stmt->expr) {
        alloc_expr(stmt->expr, REG_AX);
      }
      break;
    }
    case ND_IF: {
      alloc_expr(stmt->if_cond, REG_AX);
      alloc_stmt(stmt->then_body);
      if (stmt->else_body) {
        alloc_stmt(stmt->else_body);
      }
      break;
    }
    case ND_SWITCH: {
      alloc_expr(stmt->switch_cond, REG_AX);
      alloc_stmt(stmt->switch_body);
      break;
    }
    case ND_WHILE: {
      alloc_expr(stmt->while_cond, REG_AX);
      alloc_stmt(stmt->while_body);
      break;
    }
    case ND_DO: {
      alloc_expr(stmt->do_cond, REG_AX);
      alloc_stmt(stmt->do_body);
      break;
    }
    case ND_FOR: {
      if (stmt->for_init) {
        if (stmt->for_init->nd_type == ND_DECL) {
          alloc_decl((Decl *) stmt->for_init);
        } else {
          alloc_expr((Expr *) stmt->for_init, REG_AX);
        }
      }
      if (stmt->for_cond) {
        alloc_expr(stmt->for_cond, REG_AX);
      }
      if (stmt->for_after) {
        alloc_expr(stmt->for_after, REG_AX);
      }
      alloc_stmt(stmt->for_body);
      break;
    }
    case ND_GOTO: break;
    case ND_CONTINUE: break;
    case ND_BREAK: break;
    case ND_RETURN: {
      if (stmt->ret_expr) {
        alloc_expr(stmt->ret_expr, REG_AX);
      }
      break;
    }
    default: assert(false); // unreachable
  }
}

// allocatino of external declaration

static void alloc_func(Func *func) {
  func_used = func->used;

  alloc_stmt(func->body);
}

static void alloc_trans_unit(TransUnit *trans_unit) {
  for (int i = 0; i < trans_unit->decls->length; i++) {
    Node *decl = trans_unit->decls->buffer[i];
    if (decl->nd_type == ND_FUNC) {
      alloc_func((Func *) decl);
    }
  }
}

void alloc(TransUnit *trans_unit) {
  alloc_trans_unit(trans_unit);
}
