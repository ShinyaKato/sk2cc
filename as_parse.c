#include "as.h"

Op *op_new(OpType type, Token *token) {
  Op *op = (Op *) calloc(1, sizeof(Op));
  op->type = type;
  op->token = token;
  return op;
}

Op *op_reg(int reg, Token *token) {
  Op *op = op_new(OP_REG, token);
  op->reg = reg;
  return op;
}

Op *op_mem(int base, int disp, Token *token) {
  Op *op = op_new(OP_MEM, token);
  op->base = base;
  op->disp = disp;
  return op;
}

Op *op_sym(char *sym, Token *token) {
  Op *op = op_new(OP_SYM, token);
  op->sym = sym;
  return op;
}

Op *op_imm(int imm, Token *token) {
  Op *op = op_new(OP_IMM, token);
  op->imm = imm;
  return op;
}

Inst *inst_new(InstType type, Op *op, Op *src, Op *dest, Token *token) {
  Inst *inst = (Inst *) calloc(1, sizeof(Inst));
  inst->type = type;
  inst->op = op;
  inst->src = src;
  inst->dest = dest;
  inst->token = token;
  return inst;
}

Label *label_new(int inst) {
  Label *label = (Label *) calloc(1, sizeof(Label));
  label->inst = inst;
  return label;
}

#define EXPECT(token, expect_type, ...) \
  do { \
    Token *t = (token); \
    if (t->type != (expect_type)) { \
      ERROR(t, __VA_ARGS__); \
    } \
  } while (0)

static InstType inst_type(Token *token) {
  char *ident = token->ident;
  if (strcmp(ident, "pushq") == 0) return INST_PUSH;
  if (strcmp(ident, "popq") == 0) return INST_POP;
  if (strcmp(ident, "movq") == 0) return INST_MOV;
  if (strcmp(ident, "call") == 0) return INST_CALL;
  if (strcmp(ident, "leave") == 0) return INST_LEAVE;
  if (strcmp(ident, "ret") == 0) return INST_RET;
  ERROR(token, "invalide instruction.");
}

static Inst *parse_inst(Token **token) {
  Token *inst_tok = *token++;
  InstType type = inst_type(inst_tok);

  Vector *ops = vector_new();
  if (*token) {
    do {
      Token *op_head = *token;

      switch ((*token)->type) {
        case TOK_REG: {
          int reg = (*token++)->reg;
          vector_push(ops, op_reg(reg, op_head));
        }
        break;

        case TOK_LPAREN:
        case TOK_DISP: {
          int disp = (*token)->type == TOK_DISP ? (*token++)->disp : 0;
          EXPECT(*token++, TOK_LPAREN, "'(' is expected.");
          EXPECT(*token, TOK_REG, "register is expected.");
          int base = (*token++)->reg;
          EXPECT(*token++, TOK_RPAREN, "')' is expected.");
          vector_push(ops, op_mem(base, disp, op_head));
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
  }

  Op *op = NULL, *src = NULL, *dest = NULL;
  switch (type) {
    case INST_LEAVE:
    case INST_RET: {
      if (ops->length != 0) {
        ERROR(inst_tok, "'%s' expects no operand.", inst_tok->ident);
      }
    }
    break;

    case INST_PUSH:
    case INST_POP:
    case INST_CALL: {
      if (ops->length != 1) {
        ERROR(inst_tok, "'%s' expects 1 operand.", inst_tok->ident);
      }
      op = ops->array[0];
    }
    break;

    case INST_MOV: {
      if (ops->length != 2) {
        ERROR(inst_tok, "'%s' expects 2 operands.", inst_tok->ident);
      }
      src = ops->array[0];
      dest = ops->array[1];
    }
    break;
  }

  return inst_new(type, op, src, dest, inst_tok);
}

Unit *parse(Vector *lines) {
  Vector *insts = vector_new();
  Map *labels = map_new();

  for (int i = 0; i < lines->length; i++) {
    Vector *line = lines->array[i];
    Token **token = (Token **) line->array;

    if (line->length == 0) continue;
    EXPECT(token[0], TOK_IDENT, "identifier is expected.");

    if (token[1] && token[1]->type == TOK_SEMICOLON) {
      if (token[2]) {
        ERROR(token[2], "invalid symbol declaration.");
      }
      if (map_lookup(labels, token[0]->ident)) {
        ERROR(token[0], "duplicated symbol declaration: %s.", token[0]->ident);
      }
      map_put(labels, token[0]->ident, label_new(insts->length));
    } else {
      vector_push(insts, parse_inst(token));
    }
  }

  Unit *unit = (Unit *) calloc(1, sizeof(Unit));
  unit->insts = insts;
  unit->labels = labels;

  return unit;
}
