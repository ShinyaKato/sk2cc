#include "cc.h"

#define REGS_EMPTY 0
#define REGS_ADD(regs, reg) ((regs) | (1 << (reg)))
#define REGS_REMOVE(regs, reg) (~REGS_ADD(~(regs), (reg)))
#define REGS_UNION(regs1, regs2) ((regs1) | (regs2))
#define REGS_CHECK(regs, reg) ((regs) & (1 << (reg)))

typedef int RegSet;

static RegCode arg_reg[6] = { 7, 6, 2, 1, 8, 9 };

static RegSet func_used;

static RegSet alloc_expr(Expr *expr, RegCode reg);

static RegCode select_reg(RegSet used_regs) {
  RegCode regs[] = {
    REG_AX, REG_CX, REG_DX, REG_SI, REG_DI, REG_R8, REG_R9, REG_R10, REG_R11,
    REG_BX, REG_R12, REG_R13, REG_R14, REG_R15,
  };

  for (int i = 0; i < sizeof(regs) / sizeof(RegCode); i++) {
    if (REGS_CHECK(used_regs, regs[i])) continue;
    func_used = REGS_ADD(func_used, regs[i]);
    return regs[i];
  }

  assert(false && "failed to allocate register.");
}

static RegSet alloc_va_start(Expr *expr, RegCode reg) {
  RegSet used = alloc_expr(expr->macro_ap, REG_AX);
  used = REGS_ADD(used, REG_CX);
  return used;
}

static RegSet alloc_va_arg(Expr *expr, RegCode reg) {
  RegSet used = alloc_expr(expr->macro_ap, REG_AX);
  used = REGS_ADD(used, REG_CX);
  used = REGS_ADD(used, REG_DX);
  return used;
}

static RegSet alloc_va_end(Expr *expr, RegCode reg) {
  RegSet used = alloc_expr(expr->macro_ap, REG_AX);
  return used;
}

static RegSet alloc_primary(Expr *expr, RegCode reg) {
  return REGS_ADD(REGS_EMPTY, reg);
}

static RegSet alloc_call(Expr *expr, RegCode reg) {
  RegSet arg_regs = REGS_EMPTY;
  for (int i = 0; i < 6; i++) {
    arg_regs = REGS_ADD(arg_regs, arg_reg[i]);
  }

  RegSet used = REGS_EMPTY;
  for (int i = expr->args->length - 1; i >= 0; i--) {
    Expr *arg = expr->args->buffer[i];
    RegCode reg = i < 6 && !REGS_CHECK(used, arg_reg[i]) ? arg_reg[i] : select_reg(REGS_UNION(used, arg_regs));
    RegSet arg_used = alloc_expr(arg, reg);
    used = REGS_UNION(used, arg_used);
  }

  used = REGS_UNION(used, arg_regs);
  used = REGS_ADD(used, REG_AX);
  used = REGS_ADD(used, REG_R10);
  used = REGS_ADD(used, REG_R11);

  return used;
}

static RegSet alloc_unary(Expr *expr, RegCode reg) {
  return alloc_expr(expr->expr, reg);
}

static RegSet alloc_binary(Expr *expr, RegCode reg) {
  RegSet rhs_used = alloc_expr(expr->rhs, reg);

  RegCode lhs_reg = select_reg(rhs_used);
  RegSet lhs_used = alloc_expr(expr->lhs, lhs_reg);

  RegSet used = REGS_UNION(lhs_used, rhs_used);
  used = REGS_ADD(used, reg);
  return used;
}

static RegSet alloc_mul(Expr *expr, RegCode reg) {
  RegSet rhs_used = alloc_expr(expr->rhs, REG_DX);

  RegCode lhs_reg = select_reg(rhs_used);
  RegSet lhs_used = alloc_expr(expr->lhs, lhs_reg);

  RegSet used = REGS_UNION(lhs_used, rhs_used);
  used = REGS_ADD(used, REG_AX);
  used = REGS_ADD(used, REG_DX);
  used = REGS_ADD(used, reg);
  return used;
}

static RegSet alloc_div(Expr *expr, RegCode reg) {
  RegSet rhs_used = alloc_expr(expr->rhs, REG_CX);

  RegCode lhs_reg = select_reg(rhs_used);
  RegSet lhs_used = alloc_expr(expr->lhs, lhs_reg);

  RegSet used = REGS_UNION(lhs_used, rhs_used);
  used = REGS_ADD(used, REG_DX);
  used = REGS_ADD(used, REG_AX);
  used = REGS_ADD(used, reg);
  return used;
}

static RegSet alloc_sub(Expr *expr, RegCode reg) {
  Expr *tmp = expr->lhs;
  expr->lhs = expr->rhs;
  expr->rhs = tmp;

  return alloc_binary(expr, reg);
}

static RegSet alloc_shift(Expr *expr, RegCode reg) {
  RegSet rhs_used = alloc_expr(expr->rhs, REG_CX);

  RegCode lhs_reg = select_reg(rhs_used);
  RegSet lhs_used = alloc_expr(expr->lhs, lhs_reg);

  RegSet used = REGS_UNION(lhs_used, rhs_used);
  used = REGS_ADD(used, reg);
  return used;
}

static RegSet alloc_logical(Expr *expr, RegCode reg) {
  RegSet lhs_used = alloc_expr(expr->lhs, reg);
  RegSet rhs_used = alloc_expr(expr->rhs, reg);

  RegSet used = REGS_UNION(lhs_used, rhs_used);
  used = REGS_ADD(used, reg);
  return used;
}

static RegSet alloc_condition(Expr *expr, RegCode reg) {
  RegSet cond_used = alloc_expr(expr->cond, reg);
  RegSet lhs_used = alloc_expr(expr->lhs, reg);
  RegSet rhs_used = alloc_expr(expr->rhs, reg);

  RegSet used = REGS_EMPTY;
  used = REGS_UNION(used, cond_used);
  used = REGS_UNION(used, lhs_used);
  used = REGS_UNION(used, rhs_used);
  used = REGS_ADD(used, reg);
  return used;
}

static RegSet alloc_comma(Expr *expr, RegCode reg) {
  RegSet lhs_used = alloc_expr(expr->lhs, reg);
  RegSet rhs_used = alloc_expr(expr->rhs, reg);

  RegSet used = REGS_UNION(lhs_used, rhs_used);
  used = REGS_ADD(used, reg);
  return used;
}

static RegSet alloc_expr(Expr *expr, RegCode reg) {
  expr->reg = reg;

  switch (expr->nd_type) {
    case ND_VA_START:   return alloc_va_start(expr, reg);
    case ND_VA_ARG:     return alloc_va_arg(expr, reg);
    case ND_VA_END:     return alloc_va_end(expr, reg);
    case ND_IDENTIFIER: return alloc_primary(expr, reg);
    case ND_INTEGER:    return alloc_primary(expr, reg);
    case ND_STRING:     return alloc_primary(expr, reg);
    case ND_CALL:       return alloc_call(expr, reg);
    case ND_DOT:        return alloc_unary(expr, reg);
    case ND_ADDRESS:    return alloc_unary(expr, reg);
    case ND_INDIRECT:   return alloc_unary(expr, reg);
    case ND_UMINUS:     return alloc_unary(expr, reg);
    case ND_NOT:        return alloc_unary(expr, reg);
    case ND_LNOT:       return alloc_unary(expr, reg);
    case ND_CAST:       return alloc_unary(expr, reg);
    case ND_MUL:        return alloc_mul(expr, reg);
    case ND_DIV:        return alloc_div(expr, reg);
    case ND_MOD:        return alloc_div(expr, reg);
    case ND_ADD:        return alloc_binary(expr, reg);
    case ND_SUB:        return alloc_sub(expr, reg);
    case ND_LSHIFT:     return alloc_shift(expr, reg);
    case ND_RSHIFT:     return alloc_shift(expr, reg);
    case ND_LT:         return alloc_binary(expr, reg);
    case ND_LTE:        return alloc_binary(expr, reg);
    case ND_EQ:         return alloc_binary(expr, reg);
    case ND_NEQ:        return alloc_binary(expr, reg);
    case ND_AND:        return alloc_binary(expr, reg);
    case ND_XOR:        return alloc_binary(expr, reg);
    case ND_OR:         return alloc_binary(expr, reg);
    case ND_LAND:       return alloc_logical(expr, reg);
    case ND_LOR:        return alloc_logical(expr, reg);
    case ND_CONDITION:  return alloc_condition(expr, reg);
    case ND_ASSIGN:     return alloc_binary(expr, reg);
    case ND_COMMA:      return alloc_comma(expr, reg);
    default:            assert(false); // unreachable
  }
}

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

static void alloc_func(Func *func) {
  func_used = REGS_EMPTY;

  alloc_stmt(func->body);

  func->bx_used = REGS_CHECK(func_used, REG_BX);
  func->r12_used = REGS_CHECK(func_used, REG_R12);
  func->r13_used = REGS_CHECK(func_used, REG_R13);
  func->r14_used = REGS_CHECK(func_used, REG_R14);
  func->r15_used = REGS_CHECK(func_used, REG_R15);
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
