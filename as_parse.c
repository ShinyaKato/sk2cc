#include "as.h"

Op *op_new(OpType type, Token *token) {
  Op *op = (Op *) calloc(1, sizeof(Op));
  op->type = type;
  op->token = token;
  return op;
}

Op *op_reg(RegType regtype, Reg regcode, Token *token) {
  Op *op = op_new(OP_REG, token);
  op->regtype = regtype;
  op->regcode = regcode;
  return op;
}

Op *op_mem_base(Reg base, Reg disp, Token *token) {
  Op *op = op_new(OP_MEM, token);
  op->base = base;
  op->disp = disp;
  return op;
}

Op *op_mem_sib(Scale scale, Reg index, Reg base, Reg disp, Token *token) {
  Op *op = op_new(OP_MEM, token);
  op->sib = true;
  op->scale = scale;
  op->index = index;
  op->base = base;
  op->disp = disp;
  return op;
}

Op *op_sym(char *ident, Token *token) {
  Op *op = op_new(OP_SYM, token);
  op->ident = ident;
  return op;
}

Op *op_imm(int imm, Token *token) {
  Op *op = op_new(OP_IMM, token);
  op->imm = imm;
  return op;
}

Inst *inst_new(InstType type, InstSuffix suffix, Token *token) {
  Inst *inst = (Inst *) calloc(1, sizeof(Inst));
  inst->type = type;
  inst->suffix = suffix;
  inst->token = token;
  return inst;
}

Inst *inst_op0(InstType type, InstSuffix suffix, Token *token) {
  return inst_new(type, suffix, token);
}

Inst *inst_op1(InstType type, InstSuffix suffix, Op *op, Token *token) {
  Inst *inst = inst_new(type, suffix, token);
  inst->op = op;
  return inst;
}

Inst *inst_op2(InstType type, InstSuffix suffix, Op *src, Op *dest, Token *token) {
  Inst *inst = inst_new(type, suffix, token);
  inst->src = src;
  inst->dest = dest;
  return inst;
}

Symbol *symbol_new(int inst) {
  Symbol *symbol = (Symbol *) calloc(1, sizeof(Symbol));
  symbol->global = false;
  symbol->undef = false;
  symbol->inst = inst;
  return symbol;
}

Symbol *undef_symbol() {
  Symbol *symbol = (Symbol *) calloc(1, sizeof(Symbol));
  symbol->global = true;
  symbol->undef = true;
  return symbol;
}

#define EXPECT(token, expect_type, ...) \
  do { \
    Token *t = (token); \
    if (t->type != (expect_type)) { \
      ERROR(t, __VA_ARGS__); \
    } \
  } while (0)

static Vector *parse_ops(Token **token) {
  Vector *ops = vector_new();
  if (!(*token)) return ops;

  do {
    Token *op_head = *token;

    switch ((*token)->type) {
      case TOK_REG: {
        RegType regtype = (*token)->regtype;
        Reg regcode = (*token)->regcode;
        token++;
        vector_push(ops, op_reg(regtype, regcode, op_head));
      }
      break;

      case TOK_LPAREN:
      case TOK_NUM: {
        Op *op;
        int disp = (*token)->type == TOK_NUM ? (*token++)->num : 0;
        EXPECT(*token++, TOK_LPAREN, "'(' is expected.");
        EXPECT(*token, TOK_REG, "register is expected.");
        if ((*token)->regtype != REG64) {
          ERROR(*token, "64-bit register is expected.");
        }
        Reg base = (*token++)->regcode;
        if ((*token)->type == TOK_COMMA) {
          token++;
          EXPECT(*token, TOK_REG, "register is expected.");
          if ((*token)->regtype != REG64) {
            ERROR(*token, "64-bit register is expected.");
          }
          if ((*token)->regcode == SP) {
            ERROR(*token, "cannot use rsp as index.");
          }
          Reg index = (*token++)->regcode;
          if ((*token)->type == TOK_COMMA) {
            token++;
            EXPECT(*token, TOK_NUM, "scale is expected.");
            Token *scale = *token++;
            if (scale->num == 1) {
              op = op_mem_sib(SCALE1, index, base, disp, op_head);
            } else if (scale->num == 2) {
              op = op_mem_sib(SCALE2, index, base, disp, op_head);
            } else if (scale->num == 4) {
              op = op_mem_sib(SCALE4, index, base, disp, op_head);
            } else if (scale->num == 8) {
              op = op_mem_sib(SCALE8, index, base, disp, op_head);
            } else {
              ERROR(scale, "one of 1, 2, 4, 8 is expected.");
            }
          } else {
            op = op_mem_sib(SCALE1, index, base, disp, op_head);
          }
        } else {
          op = op_mem_base(base, disp, op_head);
        }
        EXPECT(*token++, TOK_RPAREN, "')' is expected.");
        vector_push(ops, op);
      }
      break;

      case TOK_IDENT: {
        char *sym = (*token++)->ident;
        vector_push(ops, op_sym(sym, op_head));
      }
      break;

      case TOK_IMM: {
        int imm = (*token++)->imm;
        vector_push(ops, op_imm(imm, op_head));
      }
      break;

      default: {
        ERROR(*token, "invalid operand.");
      }
    }
  } while (*token && (*token++)->type == TOK_COMMA);

  return ops;
}

static Inst *parse_inst(Token **token) {
  Token *inst = *token++;
  Vector *ops = parse_ops(token);

  if (strcmp(inst->ident, "pushq") == 0) {
    if (ops->length != 1) {
      ERROR(inst, "'%s' expects 1 operand.", inst->ident);
    }
    Op *op = ops->array[0];
    if (op->type != OP_REG || op->regtype != REG64) {
      ERROR(op->token, "only 64-bits register is supported.");
    }
    return inst_op1(INST_PUSH, INST_QUAD, op, inst);
  }

  if (strcmp(inst->ident, "popq") == 0) {
    if (ops->length != 1) {
      ERROR(inst, "'%s' expects 1 operand.", inst->ident);
    }
    Op *op = ops->array[0];
    if (op->type != OP_REG || op->regtype != REG64) {
      ERROR(op->token, "only 64-bits register is supported.");
    }
    return inst_op1(INST_POP, INST_QUAD, op, inst);
  }

  if (strcmp(inst->ident, "movq") == 0) {
    if (ops->length != 2) {
      ERROR(inst, "'%s' expects 2 operands.", inst->ident);
    }
    Op *src = ops->array[0], *dest = ops->array[1];
    if (dest->type == OP_IMM) {
      ERROR(dest->token, "destination cannot be an immediate.");
    }
    if (src->type == OP_MEM && dest->type == OP_MEM) {
      ERROR(inst, "both of source and destination cannot be memory operands.");
    }
    if (src->type == OP_REG && src->regtype != REG64) {
      ERROR(src->token, "operand type mismatched.");
    }
    if (dest->type == OP_REG && dest->regtype != REG64) {
      ERROR(dest->token, "operand type mismatched.");
    }
    return inst_op2(INST_MOV, INST_QUAD, src, dest, inst);
  }

  if (strcmp(inst->ident, "movl") == 0) {
    if (ops->length != 2) {
      ERROR(inst, "'%s' expects 2 operands.", inst->ident);
    }
    Op *src = ops->array[0], *dest = ops->array[1];
    if (dest->type == OP_IMM) {
      ERROR(dest->token, "destination cannot be an immediate.");
    }
    if (src->type == OP_MEM && dest->type == OP_MEM) {
      ERROR(inst, "both of source and destination cannot be memory operands.");
    }
    if (src->type == OP_REG && src->regtype != REG32) {
      ERROR(src->token, "operand type mismatched.");
    }
    if (dest->type == OP_REG && dest->regtype != REG32) {
      ERROR(dest->token, "operand type mismatched.");
    }
    return inst_op2(INST_MOV, INST_LONG, src, dest, inst);
  }

  if (strcmp(inst->ident, "movw") == 0) {
    if (ops->length != 2) {
      ERROR(inst, "'%s' expects 2 operands.", inst->ident);
    }
    Op *src = ops->array[0], *dest = ops->array[1];
    if (dest->type == OP_IMM) {
      ERROR(dest->token, "destination cannot be an immediate.");
    }
    if (src->type == OP_MEM && dest->type == OP_MEM) {
      ERROR(inst, "both of source and destination cannot be memory operands.");
    }
    if (src->type == OP_REG && src->regtype != REG16) {
      ERROR(src->token, "operand type mismatched.");
    }
    if (dest->type == OP_REG && dest->regtype != REG16) {
      ERROR(dest->token, "operand type mismatched.");
    }
    return inst_op2(INST_MOV, INST_WORD, src, dest, inst);
  }

  if (strcmp(inst->ident, "movb") == 0) {
    if (ops->length != 2) {
      ERROR(inst, "'%s' expects 2 operands.", inst->ident);
    }
    Op *src = ops->array[0], *dest = ops->array[1];
    if (dest->type == OP_IMM) {
      ERROR(dest->token, "destination cannot be an immediate.");
    }
    if (src->type == OP_MEM && dest->type == OP_MEM) {
      ERROR(inst, "both of source and destination cannot be memory operands.");
    }
    if (src->type == OP_REG && src->regtype != REG8) {
      ERROR(src->token, "operand type mismatched.");
    }
    if (dest->type == OP_REG && dest->regtype != REG8) {
      ERROR(dest->token, "operand type mismatched.");
    }
    return inst_op2(INST_MOV, INST_BYTE, src, dest, inst);
  }

  if (strcmp(inst->ident, "leaq") == 0) {
    if (ops->length != 2) {
      ERROR(inst, "'%s' expects 2 operands.", inst->ident);
    }
    Op *src = ops->array[0], *dest = ops->array[1];
    if (src->type != OP_MEM) {
      ERROR(src->token, "first operand should be memory operand.");
    }
    if (dest->type != OP_REG) {
      ERROR(dest->token, "second operand should be register operand.");
    }
    if (dest->regtype != REG64) {
      ERROR(dest->token, "only 64-bit register is supported.");
    }
    return inst_op2(INST_LEA, INST_QUAD, src, dest, inst);
  }

  if (strcmp(inst->ident, "addq") == 0) {
    if (ops->length != 2) {
      ERROR(inst, "'%s' expects 2 operands.", inst->ident);
    }
    Op *src = ops->array[0], *dest = ops->array[1];
    if (dest->type == OP_IMM) {
      ERROR(dest->token, "destination cannot be an immediate.");
    }
    if (src->type == OP_MEM && dest->type == OP_MEM) {
      ERROR(inst, "both of source and destination cannot be memory operands.");
    }
    if (src->type == OP_REG && src->regtype != REG64) {
      ERROR(src->token, "operand type mismatched.");
    }
    if (dest->type == OP_REG && dest->regtype != REG64) {
      ERROR(dest->token, "operand type mismatched.");
    }
    return inst_op2(INST_ADD, INST_QUAD, src, dest, inst);
  }

  if (strcmp(inst->ident, "addl") == 0) {
    if (ops->length != 2) {
      ERROR(inst, "'%s' expects 2 operands.", inst->ident);
    }
    Op *src = ops->array[0], *dest = ops->array[1];
    if (dest->type == OP_IMM) {
      ERROR(dest->token, "destination cannot be an immediate.");
    }
    if (src->type == OP_MEM && dest->type == OP_MEM) {
      ERROR(inst, "both of source and destination cannot be memory operands.");
    }
    if (src->type == OP_REG && src->regtype != REG32) {
      ERROR(src->token, "operand type mismatched.");
    }
    if (dest->type == OP_REG && dest->regtype != REG32) {
      ERROR(dest->token, "operand type mismatched.");
    }
    return inst_op2(INST_ADD, INST_LONG, src, dest, inst);
  }

  if (strcmp(inst->ident, "subq") == 0) {
    if (ops->length != 2) {
      ERROR(inst, "'%s' expects 2 operands.", inst->ident);
    }
    Op *src = ops->array[0], *dest = ops->array[1];
    if (dest->type == OP_IMM) {
      ERROR(dest->token, "destination cannot be an immediate.");
    }
    if (src->type == OP_MEM && dest->type == OP_MEM) {
      ERROR(inst, "both of source and destination cannot be memory operands.");
    }
    if (src->type == OP_REG && src->regtype != REG64) {
      ERROR(src->token, "operand type mismatched.");
    }
    if (dest->type == OP_REG && dest->regtype != REG64) {
      ERROR(dest->token, "operand type mismatched.");
    }
    return inst_op2(INST_SUB, INST_QUAD, src, dest, inst);
  }

  if (strcmp(inst->ident, "subl") == 0) {
    if (ops->length != 2) {
      ERROR(inst, "'%s' expects 2 operands.", inst->ident);
    }
    Op *src = ops->array[0], *dest = ops->array[1];
    if (dest->type == OP_IMM) {
      ERROR(dest->token, "destination cannot be an immediate.");
    }
    if (src->type == OP_MEM && dest->type == OP_MEM) {
      ERROR(inst, "both of source and destination cannot be memory operands.");
    }
    if (src->type == OP_REG && src->regtype != REG32) {
      ERROR(src->token, "operand type mismatched.");
    }
    if (dest->type == OP_REG && dest->regtype != REG32) {
      ERROR(dest->token, "operand type mismatched.");
    }
    return inst_op2(INST_SUB, INST_LONG, src, dest, inst);
  }

  if (strcmp(inst->ident, "mulq") == 0) {
    if (ops->length != 1) {
      ERROR(inst, "'%s' expects 1 operand.", inst->ident);
    }
    Op *op = ops->array[0];
    if (op->type != OP_REG && op->type != OP_MEM) {
      ERROR(inst, "register or memory operand is expected.");
    }
    if (op->type == OP_REG && op->regtype != REG64) {
      ERROR(inst, "operand type mismatched.");
    }
    return inst_op1(INST_MUL, INST_QUAD, op, inst);
  }

  if (strcmp(inst->ident, "mull") == 0) {
    if (ops->length != 1) {
      ERROR(inst, "'%s' expects 1 operand.", inst->ident);
    }
    Op *op = ops->array[0];
    if (op->type != OP_REG && op->type != OP_MEM) {
      ERROR(inst, "register or memory operand is expected.");
    }
    if (op->type == OP_REG && op->regtype != REG32) {
      ERROR(inst, "operand type mismatched.");
    }
    return inst_op1(INST_MUL, INST_LONG, op, inst);
  }

  if (strcmp(inst->ident, "imulq") == 0) {
    if (ops->length != 1) {
      ERROR(inst, "'%s' expects 1 operand.", inst->ident);
    }
    Op *op = ops->array[0];
    if (op->type != OP_REG && op->type != OP_MEM) {
      ERROR(inst, "register or memory operand is expected.");
    }
    if (op->type == OP_REG && op->regtype != REG64) {
      ERROR(inst, "operand type mismatched.");
    }
    return inst_op1(INST_IMUL, INST_QUAD, op, inst);
  }

  if (strcmp(inst->ident, "imull") == 0) {
    if (ops->length != 1) {
      ERROR(inst, "'%s' expects 1 operand.", inst->ident);
    }
    Op *op = ops->array[0];
    if (op->type != OP_REG && op->type != OP_MEM) {
      ERROR(inst, "register or memory operand is expected.");
    }
    if (op->type == OP_REG && op->regtype != REG32) {
      ERROR(inst, "operand type mismatched.");
    }
    return inst_op1(INST_IMUL, INST_LONG, op, inst);
  }

  if (strcmp(inst->ident, "divq") == 0) {
    if (ops->length != 1) {
      ERROR(inst, "'%s' expects 1 operand.", inst->ident);
    }
    Op *op = ops->array[0];
    if (op->type != OP_REG && op->type != OP_MEM) {
      ERROR(inst, "register or memory operand is expected.");
    }
    if (op->type == OP_REG && op->regtype != REG64) {
      ERROR(inst, "operand type mismatched.");
    }
    return inst_op1(INST_DIV, INST_QUAD, op, inst);
  }

  if (strcmp(inst->ident, "divl") == 0) {
    if (ops->length != 1) {
      ERROR(inst, "'%s' expects 1 operand.", inst->ident);
    }
    Op *op = ops->array[0];
    if (op->type != OP_REG && op->type != OP_MEM) {
      ERROR(inst, "register or memory operand is expected.");
    }
    if (op->type == OP_REG && op->regtype != REG32) {
      ERROR(inst, "operand type mismatched.");
    }
    return inst_op1(INST_DIV, INST_LONG, op, inst);
  }

  if (strcmp(inst->ident, "idivq") == 0) {
    if (ops->length != 1) {
      ERROR(inst, "'%s' expects 1 operand.", inst->ident);
    }
    Op *op = ops->array[0];
    if (op->type != OP_REG && op->type != OP_MEM) {
      ERROR(inst, "register or memory operand is expected.");
    }
    if (op->type == OP_REG && op->regtype != REG64) {
      ERROR(inst, "operand type mismatched.");
    }
    return inst_op1(INST_IDIV, INST_QUAD, op, inst);
  }

  if (strcmp(inst->ident, "idivl") == 0) {
    if (ops->length != 1) {
      ERROR(inst, "'%s' expects 1 operand.", inst->ident);
    }
    Op *op = ops->array[0];
    if (op->type != OP_REG && op->type != OP_MEM) {
      ERROR(inst, "register or memory operand is expected.");
    }
    if (op->type == OP_REG && op->regtype != REG32) {
      ERROR(inst, "operand type mismatched.");
    }
    return inst_op1(INST_IDIV, INST_LONG, op, inst);
  }

  if (strcmp(inst->ident, "cmpq") == 0) {
    if (ops->length != 2) {
      ERROR(inst, "'%s' expects 2 operands.", inst->ident);
    }
    Op *src = ops->array[0], *dest = ops->array[1];
    if (dest->type == OP_IMM) {
      ERROR(dest->token, "destination cannot be an immediate.");
    }
    if (src->type == OP_MEM && dest->type == OP_MEM) {
      ERROR(inst, "both of source and destination cannot be memory operands.");
    }
    if (src->type == OP_REG && src->regtype != REG64) {
      ERROR(src->token, "operand type mismatched.");
    }
    if (dest->type == OP_REG && dest->regtype != REG64) {
      ERROR(dest->token, "operand type mismatched.");
    }
    return inst_op2(INST_CMP, INST_QUAD, src, dest, inst);
  }

  if (strcmp(inst->ident, "cmpl") == 0) {
    if (ops->length != 2) {
      ERROR(inst, "'%s' expects 2 operands.", inst->ident);
    }
    Op *src = ops->array[0], *dest = ops->array[1];
    if (dest->type == OP_IMM) {
      ERROR(dest->token, "destination cannot be an immediate.");
    }
    if (src->type == OP_MEM && dest->type == OP_MEM) {
      ERROR(inst, "both of source and destination cannot be memory operands.");
    }
    if (src->type == OP_REG && src->regtype != REG32) {
      ERROR(src->token, "operand type mismatched.");
    }
    if (dest->type == OP_REG && dest->regtype != REG32) {
      ERROR(dest->token, "operand type mismatched.");
    }
    return inst_op2(INST_CMP, INST_LONG, src, dest, inst);
  }

  if (strcmp(inst->ident, "cmpw") == 0) {
    if (ops->length != 2) {
      ERROR(inst, "'%s' expects 2 operands.", inst->ident);
    }
    Op *src = ops->array[0], *dest = ops->array[1];
    if (dest->type == OP_IMM) {
      ERROR(dest->token, "destination cannot be an immediate.");
    }
    if (src->type == OP_MEM && dest->type == OP_MEM) {
      ERROR(inst, "both of source and destination cannot be memory operands.");
    }
    if (src->type == OP_REG && src->regtype != REG16) {
      ERROR(src->token, "operand type mismatched.");
    }
    if (dest->type == OP_REG && dest->regtype != REG16) {
      ERROR(dest->token, "operand type mismatched.");
    }
    return inst_op2(INST_CMP, INST_WORD, src, dest, inst);
  }

  if (strcmp(inst->ident, "cmpb") == 0) {
    if (ops->length != 2) {
      ERROR(inst, "'%s' expects 2 operands.", inst->ident);
    }
    Op *src = ops->array[0], *dest = ops->array[1];
    if (dest->type == OP_IMM) {
      ERROR(dest->token, "destination cannot be an immediate.");
    }
    if (src->type == OP_MEM && dest->type == OP_MEM) {
      ERROR(inst, "both of source and destination cannot be memory operands.");
    }
    if (src->type == OP_REG && src->regtype != REG8) {
      ERROR(src->token, "operand type mismatched.");
    }
    if (dest->type == OP_REG && dest->regtype != REG8) {
      ERROR(dest->token, "operand type mismatched.");
    }
    return inst_op2(INST_CMP, INST_BYTE, src, dest, inst);
  }

  if (strcmp(inst->ident, "sete") == 0) {
    if (ops->length != 1) {
      ERROR(inst, "'%s' expects 1 operand.", inst->ident);
    }
    Op *op = ops->array[0];
    if (op->type != OP_REG && op->type != OP_MEM) {
      ERROR(inst, "register or memory operand is expected.");
    }
    if (op->type == OP_REG && op->regtype != REG8) {
      ERROR(inst, "operand type mismatched.");
    }
    return inst_op1(INST_SETE, INST_BYTE, op, inst);
  }

  if (strcmp(inst->ident, "setne") == 0) {
    if (ops->length != 1) {
      ERROR(inst, "'%s' expects 1 operand.", inst->ident);
    }
    Op *op = ops->array[0];
    if (op->type != OP_REG && op->type != OP_MEM) {
      ERROR(inst, "register or memory operand is expected.");
    }
    if (op->type == OP_REG && op->regtype != REG8) {
      ERROR(inst, "operand type mismatched.");
    }
    return inst_op1(INST_SETNE, INST_BYTE, op, inst);
  }

  if (strcmp(inst->ident, "setl") == 0) {
    if (ops->length != 1) {
      ERROR(inst, "'%s' expects 1 operand.", inst->ident);
    }
    Op *op = ops->array[0];
    if (op->type != OP_REG && op->type != OP_MEM) {
      ERROR(inst, "register or memory operand is expected.");
    }
    if (op->type == OP_REG && op->regtype != REG8) {
      ERROR(inst, "operand type mismatched.");
    }
    return inst_op1(INST_SETL, INST_BYTE, op, inst);
  }

  if (strcmp(inst->ident, "setg") == 0) {
    if (ops->length != 1) {
      ERROR(inst, "'%s' expects 1 operand.", inst->ident);
    }
    Op *op = ops->array[0];
    if (op->type != OP_REG && op->type != OP_MEM) {
      ERROR(inst, "register or memory operand is expected.");
    }
    if (op->type == OP_REG && op->regtype != REG8) {
      ERROR(inst, "operand type mismatched.");
    }
    return inst_op1(INST_SETG, INST_BYTE, op, inst);
  }

  if (strcmp(inst->ident, "setle") == 0) {
    if (ops->length != 1) {
      ERROR(inst, "'%s' expects 1 operand.", inst->ident);
    }
    Op *op = ops->array[0];
    if (op->type != OP_REG && op->type != OP_MEM) {
      ERROR(inst, "register or memory operand is expected.");
    }
    if (op->type == OP_REG && op->regtype != REG8) {
      ERROR(inst, "operand type mismatched.");
    }
    return inst_op1(INST_SETLE, INST_BYTE, op, inst);
  }

  if (strcmp(inst->ident, "setge") == 0) {
    if (ops->length != 1) {
      ERROR(inst, "'%s' expects 1 operand.", inst->ident);
    }
    Op *op = ops->array[0];
    if (op->type != OP_REG && op->type != OP_MEM) {
      ERROR(inst, "register or memory operand is expected.");
    }
    if (op->type == OP_REG && op->regtype != REG8) {
      ERROR(inst, "operand type mismatched.");
    }
    return inst_op1(INST_SETGE, INST_BYTE, op, inst);
  }

  if (strcmp(inst->ident, "jmp") == 0) {
    if (ops->length != 1) {
      ERROR(inst, "'%s' expects 1 operand.", inst->ident);
    }
    Op *op = ops->array[0];
    if (op->type != OP_SYM) {
      ERROR(op->token, "only symbol is supported.");
    }
    return inst_op1(INST_JMP, INST_QUAD, op, inst);
  }

  if (strcmp(inst->ident, "je") == 0) {
    if (ops->length != 1) {
      ERROR(inst, "'%s' expects 1 operand.", inst->ident);
    }
    Op *op = ops->array[0];
    if (op->type != OP_SYM) {
      ERROR(op->token, "only symbol is supported.");
    }
    return inst_op1(INST_JE, INST_QUAD, op, inst);
  }

  if (strcmp(inst->ident, "jne") == 0) {
    if (ops->length != 1) {
      ERROR(inst, "'%s' expects 1 operand.", inst->ident);
    }
    Op *op = ops->array[0];
    if (op->type != OP_SYM) {
      ERROR(op->token, "only symbol is supported.");
    }
    return inst_op1(INST_JNE, INST_QUAD, op, inst);
  }

  if (strcmp(inst->ident, "call") == 0) {
    if (ops->length != 1) {
      ERROR(inst, "'%s' expects 1 operand.", inst->ident);
    }
    Op *op = ops->array[0];
    if (op->type != OP_SYM) {
      ERROR(op->token, "only symbol is supported.");
    }
    return inst_op1(INST_CALL, INST_QUAD, op, inst);
  }

  if (strcmp(inst->ident, "leave") == 0) {
    if (ops->length != 0) {
      ERROR(inst, "'%s' expects no operand.", inst->ident);
    }
    return inst_op0(INST_LEAVE, INST_QUAD, inst);
  }

  if (strcmp(inst->ident, "ret") == 0) {
    if (ops->length != 0) {
      ERROR(inst, "'%s' expects no operand.", inst->ident);
    }
    return inst_op0(INST_RET, INST_QUAD, inst);
  }

  ERROR(inst, "unknown instruction '%s'.", inst->ident);
}

Unit *parse(Vector *lines) {
  Vector *insts = vector_new();
  Map *symbols = map_new();

  for (int i = 0; i < lines->length; i++) {
    Vector *line = lines->array[i];
    Token **token = (Token **) line->array;

    if (line->length == 0) continue;
    EXPECT(token[0], TOK_IDENT, "identifier is expected.");

    if (token[1] && token[1]->type == TOK_SEMICOLON) {
      if (token[2]) {
        ERROR(token[2], "invalid symbol declaration.");
      }
      char *ident = token[0]->ident;
      Symbol *symbol = map_lookup(symbols, ident);
      if (symbol) {
        if (!symbol->undef) {
          ERROR(token[0], "duplicated symbol declaration: %s.", ident);
        }
        symbol->undef = false;
        symbol->inst = insts->length;
      } else {
        map_put(symbols, ident, symbol_new(insts->length));
      }
    } else if (strcmp(token[0]->ident, ".global") == 0) {
      if (!token[1] || token[1]->type != TOK_IDENT) {
        ERROR(token[0], "identifier is expected.");
      }
      if (token[2]) {
        ERROR(token[0], "invalid directive.");
      }
      char *ident = token[1]->ident;
      Symbol *symbol = map_lookup(symbols, ident);
      if (symbol) {
        symbol->global = true;
      } else {
        map_put(symbols, ident, undef_symbol());
      }
    } else {
      vector_push(insts, parse_inst(token));
    }
  }

  Unit *unit = (Unit *) calloc(1, sizeof(Unit));
  unit->insts = insts;
  unit->symbols = symbols;

  return unit;
}
