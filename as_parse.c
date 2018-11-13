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

Inst *inst_new(InstType type, Vector *ops, Token *token) {
  Inst *inst = (Inst *) calloc(1, sizeof(Inst));
  inst->type = type;
  inst->ops = ops;
  return inst;
}

Label *label_new(int inst) {
  Label *label = (Label *) calloc(1, sizeof(Label));
  label->inst = inst;
  return label;
}

Unit *parse(Vector *lines) {
  Vector *insts = vector_new();
  Map *labels = map_new();

  for (int i = 0; i < lines->length; i++) {
    Vector *line = lines->array[i];
    if (line->length == 0) continue;

    Token **token = (Token **) line->array;
    Token *head = *token++;

    if (head->type != TOK_IDENT) {
      ERROR(head, "invalid instruction.");
    }

    if (*token && (*token)->type == TOK_SEMICOLON) {
      token++;
      if (*token) {
        ERROR(*token, "invalid symbol declaration.");
      }
      if (map_lookup(labels, head->ident)) {
        ERROR(head, "duplicated symbol declaration: %s.", head->ident);
      }
      map_put(labels, head->ident, label_new(insts->length));
      continue;
    }

    InstType type;
    if (strcmp(head->ident, "pushq") == 0) {
      type = INST_PUSH;
    } else if (strcmp(head->ident, "popq") == 0) {
      type = INST_POP;
    } else if (strcmp(head->ident, "movq") == 0) {
      type = INST_MOV;
    } else if (strcmp(head->ident, "call") == 0) {
      type = INST_CALL;
    } else if (strcmp(head->ident, "leave") == 0) {
      type = INST_LEAVE;
    } else if (strcmp(head->ident, "ret") == 0) {
      type = INST_RET;
    } else {
      ERROR(head, "invalide instruction.");
    }

    Vector *ops = vector_new();
    if (*token) {
      while (1) {
        Token *op_head = *token;
        switch ((*token)->type) {
          case TOK_REG: {
            int reg = (*token)->reg;
            token++;
            vector_push(ops, op_reg(reg, op_head));
          }
          break;

          case TOK_LPAREN: {
            token++;
            if ((*token)->type != TOK_REG) {
              ERROR(*token, "register is expected.");
            }
            int base = (*token)->reg;
            token++;
            if ((*token)->type != TOK_RPAREN) {
              ERROR(*token, "')' is expected.");
            }
            token++;
            vector_push(ops, op_mem(base, 0, op_head));
          }
          break;

          case TOK_DISP: {
            int disp = (*token)->disp;
            token++;
            if ((*token)->type != TOK_LPAREN) {
              ERROR(*token, "'(' is expected.");
            }
            token++;
            if ((*token)->type != TOK_REG) {
              ERROR(*token, "register is expected.");
            }
            int base = (*token)->reg;
            token++;
            if ((*token)->type != TOK_RPAREN) {
              ERROR(*token, "')' is expected.");
            }
            token++;
            vector_push(ops, op_mem(base, disp, op_head));
          }
          break;

          case TOK_IDENT: {
            char *sym = (*token)->ident;
            token++;
            vector_push(ops, op_sym(sym, op_head));
          }
          break;

          case TOK_IMM: {
            int imm = (*token)->imm;
            token++;
            vector_push(ops, op_imm(imm, op_head));
          }
          break;

          default: {
            ERROR(*token, "invalid operand.");
          }
        }

        if (!*token) {
          break;
        }
        if ((*token)->type != TOK_COMMA) {
          ERROR(*token, "',' is expected.");
        }
        token++;
        if (!*token) {
          ERROR(*token, "invalid operand.");
        }
      }
    }

    vector_push(insts, inst_new(type, ops, head));
  }

  Unit *unit = (Unit *) calloc(1, sizeof(Unit));
  unit->insts = insts;
  unit->labels = labels;

  return unit;
}
