#include "as.h"

static Label *label_new(char *ident, Token *token) {
  Label *label = (Label *) calloc(1, sizeof(Label));
  label->ident = ident;
  label->token = token;
  return label;
}

static Dir *dir_new(StmtType type, Token *token) {
  Dir *dir = (Dir *) calloc(1, sizeof(Dir));
  dir->type = type;
  dir->token = token;
  return dir;
}

static Dir *dir_text(Token *token) {
  return dir_new(ST_TEXT, token);
}

static Dir *dir_data(Token *token) {
  return dir_new(ST_DATA, token);
}

static Dir *dir_section(char *ident, Token *token) {
  Dir *dir = dir_new(ST_SECTION, token);
  dir->ident = ident;
  return dir;
}

static Dir *dir_global(char *ident, Token *token) {
  Dir *dir = dir_new(ST_GLOBAL, token);
  dir->ident = ident;
  return dir;
}

static Dir *dir_zero(int num, Token *token) {
  Dir *dir = dir_new(ST_ZERO, token);
  dir->num = num;
  return dir;
}

static Dir *dir_long(int num, Token *token) {
  Dir *dir = dir_new(ST_LONG, token);
  dir->num = num;
  return dir;
}

static Dir *dir_quad(char *ident, Token *token) {
  Dir *dir = dir_new(ST_QUAD, token);
  dir->ident = ident;
  return dir;
}

static Dir *dir_ascii(String *string, Token *token) {
  Dir *dir = dir_new(ST_ASCII, token);
  dir->string = string;
  return dir;
}

static Op *op_new(OpType type, Token *token) {
  Op *op = (Op *) calloc(1, sizeof(Op));
  op->type = type;
  op->token = token;
  return op;
}

static Op *op_reg(RegSize regtype, RegCode regcode, Token *token) {
  Op *op = op_new(OP_REG, token);
  op->regtype = regtype;
  op->regcode = regcode;
  return op;
}

static Op *op_mem_base(RegCode base, RegCode disp, Token *token) {
  Op *op = op_new(OP_MEM, token);
  op->base = base;
  op->disp = disp;
  return op;
}

static Op *op_mem_sib(Scale scale, RegCode index, RegCode base, RegCode disp, Token *token) {
  Op *op = op_new(OP_MEM, token);
  op->sib = true;
  op->scale = scale;
  op->index = index;
  op->base = base;
  op->disp = disp;
  return op;
}

static Op *op_rip_rel(char *ident, Token *token) {
  Op *op = op_new(OP_MEM, token);
  op->rip = true;
  op->ident = ident;
  return op;
}

static Op *op_sym(char *ident, Token *token) {
  Op *op = op_new(OP_SYM, token);
  op->ident = ident;
  return op;
}

static Op *op_imm(int imm, Token *token) {
  Op *op = op_new(OP_IMM, token);
  op->imm = imm;
  return op;
}

static Inst *inst_new(StmtType type, InstSuffix suffix, Token *token) {
  Inst *inst = (Inst *) calloc(1, sizeof(Inst));
  inst->type = type;
  inst->suffix = suffix;
  inst->token = token;
  return inst;
}

static Inst *inst_op0(StmtType type, InstSuffix suffix, Token *token) {
  return inst_new(type, suffix, token);
}

static Inst *inst_op1(StmtType type, InstSuffix suffix, Op *op, Token *token) {
  Inst *inst = inst_new(type, suffix, token);
  inst->op = op;
  return inst;
}

static Inst *inst_op2(StmtType type, InstSuffix suffix, Op *src, Op *dest, Token *token) {
  Inst *inst = inst_new(type, suffix, token);
  inst->src = src;
  inst->dest = dest;
  return inst;
}

// tokens

static Token **tokens;
static int pos;

static Token *peek(void) {
  return tokens[pos];
}

static Token *check(TokenType type) {
  if (tokens[pos]->type == type) {
    return tokens[pos];
  }
  return NULL;
}

static Token *read(TokenType type) {
  if (tokens[pos]->type == type) {
    return tokens[pos++];
  }
  return NULL;
}

static Token *expect(TokenType type) {
  if (tokens[pos]->type == type) {
    return tokens[pos++];
  }
  ERROR(tokens[pos], "unexpected token.");
}

// parser

// register operand
static Op *parse_reg(void) {
  Token *token = expect(TK_REG);

  return op_reg(token->regtype, token->regcode, token);
}

// memory operand
//   syntax: disp? '(' base (',' index (',' scale)?)? ')'
static Op *parse_memory(void) {
  Token *token = peek();

  // disp is 0 if it is omitted
  int disp = check(TK_NUM) ? expect(TK_NUM)->num : 0;

  expect(TK_LPAREN);

  // read base register (should be 64-bit)
  Token *base = expect(TK_REG);
  if (base->regtype != REG_QUAD) {
    ERROR(base, "64-bit register is expected.");
  }

  // memory operand like 'disp(base)'
  if (read(TK_RPAREN)) {
    return op_mem_base(base->regcode, disp, token);
  }

  if (read(TK_COMMA)) {
    // read index register (should be 64-bit)
    // rsp is unacceptable as index register in x86 instruction encoding
    Token *index = expect(TK_REG);
    if (index->regtype != REG_QUAD) {
      ERROR(token, "64-bit register is expected.");
    }
    if (index->regcode == REG_SP) {
      ERROR(token, "cannot use rsp as index.");
    }

    // memory operand like 'disp(base, index)'
    if (read(TK_RPAREN)) {
      return op_mem_sib(SCALE1, index->regcode, base->regcode, disp, token);
    }

    // memory operand like 'disp(base, index, scale)'
    if (read(TK_COMMA)) {
      // read scale
      // scale should be one of 1, 2, 4, 8
      Token *scale = expect(TK_NUM);
      Scale scale_val;
      switch (scale->num) {
        case 1: scale_val = SCALE1; break;
        case 2: scale_val = SCALE2; break;
        case 4: scale_val = SCALE4; break;
        case 8: scale_val = SCALE8; break;
        default: {
          ERROR(scale, "one of 1, 2, 4, 8 is expected.");
        }
      }

      expect(TK_RPAREN);

      return op_mem_sib(scale_val, index->regcode, base->regcode, disp, token);
    }
  }

  ERROR(peek(), "',' or ')' is expected.");
}

// symbol operand (including RIP-relative)
static Op *parse_symbol(void) {
  Token *token = expect(TK_IDENT);

  if (peek() && read(TK_LPAREN)) {
    expect(TK_RIP);
    expect(TK_RPAREN);

    return op_rip_rel(token->ident, token);
  }

  return op_sym(token->ident, token);
}

// immediate operand
static Op *parse_immediate(void) {
  Token *token = expect(TK_IMM);

  return op_imm(token->imm, token);
}

static Vector *parse_ops(void) {
  Vector *ops = vector_new();

  if (!peek()) return ops;
  do {
    switch (peek()->type) {
      case TK_REG: {
        vector_push(ops, parse_reg());
        break;
      }
      case TK_LPAREN:
      case TK_NUM: {
        vector_push(ops, parse_memory());
        break;
      }
      case TK_IDENT: {
        vector_push(ops, parse_symbol());
        break;
      }
      case TK_IMM: {
        vector_push(ops, parse_immediate());
        break;
      }
      default: {
        ERROR(peek(), "invalid operand.");
      }
    }
  } while (peek() && read(TK_COMMA));

  return ops;
}

static Inst *parse_inst(Token *inst) {
  Vector *ops = parse_ops();

  if (strcmp(inst->ident, "pushq") == 0) {
    if (ops->length != 1) {
      ERROR(inst, "'%s' expects 1 operand.", inst->ident);
    }
    Op *op = ops->buffer[0];
    if (op->type != OP_REG || op->regtype != REG_QUAD) {
      ERROR(op->token, "only 64-bits register is supported.");
    }
    return inst_op1(ST_PUSH, INST_QUAD, op, inst);
  }

  if (strcmp(inst->ident, "popq") == 0) {
    if (ops->length != 1) {
      ERROR(inst, "'%s' expects 1 operand.", inst->ident);
    }
    Op *op = ops->buffer[0];
    if (op->type != OP_REG || op->regtype != REG_QUAD) {
      ERROR(op->token, "only 64-bits register is supported.");
    }
    return inst_op1(ST_POP, INST_QUAD, op, inst);
  }

  if (strcmp(inst->ident, "movq") == 0) {
    if (ops->length != 2) {
      ERROR(inst, "'%s' expects 2 operands.", inst->ident);
    }
    Op *src = ops->buffer[0], *dest = ops->buffer[1];
    if (dest->type == OP_IMM) {
      ERROR(dest->token, "destination cannot be an immediate.");
    }
    if (src->type == OP_MEM && dest->type == OP_MEM) {
      ERROR(inst, "both of source and destination cannot be memory operands.");
    }
    if (src->type == OP_REG && src->regtype != REG_QUAD) {
      ERROR(src->token, "operand type mismatched.");
    }
    if (dest->type == OP_REG && dest->regtype != REG_QUAD) {
      ERROR(dest->token, "operand type mismatched.");
    }
    return inst_op2(ST_MOV, INST_QUAD, src, dest, inst);
  }

  if (strcmp(inst->ident, "movl") == 0) {
    if (ops->length != 2) {
      ERROR(inst, "'%s' expects 2 operands.", inst->ident);
    }
    Op *src = ops->buffer[0], *dest = ops->buffer[1];
    if (dest->type == OP_IMM) {
      ERROR(dest->token, "destination cannot be an immediate.");
    }
    if (src->type == OP_MEM && dest->type == OP_MEM) {
      ERROR(inst, "both of source and destination cannot be memory operands.");
    }
    if (src->type == OP_REG && src->regtype != REG_LONG) {
      ERROR(src->token, "operand type mismatched.");
    }
    if (dest->type == OP_REG && dest->regtype != REG_LONG) {
      ERROR(dest->token, "operand type mismatched.");
    }
    return inst_op2(ST_MOV, INST_LONG, src, dest, inst);
  }

  if (strcmp(inst->ident, "movw") == 0) {
    if (ops->length != 2) {
      ERROR(inst, "'%s' expects 2 operands.", inst->ident);
    }
    Op *src = ops->buffer[0], *dest = ops->buffer[1];
    if (dest->type == OP_IMM) {
      ERROR(dest->token, "destination cannot be an immediate.");
    }
    if (src->type == OP_MEM && dest->type == OP_MEM) {
      ERROR(inst, "both of source and destination cannot be memory operands.");
    }
    if (src->type == OP_REG && src->regtype != REG_WORD) {
      ERROR(src->token, "operand type mismatched.");
    }
    if (dest->type == OP_REG && dest->regtype != REG_WORD) {
      ERROR(dest->token, "operand type mismatched.");
    }
    return inst_op2(ST_MOV, INST_WORD, src, dest, inst);
  }

  if (strcmp(inst->ident, "movb") == 0) {
    if (ops->length != 2) {
      ERROR(inst, "'%s' expects 2 operands.", inst->ident);
    }
    Op *src = ops->buffer[0], *dest = ops->buffer[1];
    if (dest->type == OP_IMM) {
      ERROR(dest->token, "destination cannot be an immediate.");
    }
    if (src->type == OP_MEM && dest->type == OP_MEM) {
      ERROR(inst, "both of source and destination cannot be memory operands.");
    }
    if (src->type == OP_REG && src->regtype != REG_BYTE) {
      ERROR(src->token, "operand type mismatched.");
    }
    if (dest->type == OP_REG && dest->regtype != REG_BYTE) {
      ERROR(dest->token, "operand type mismatched.");
    }
    return inst_op2(ST_MOV, INST_BYTE, src, dest, inst);
  }

  if (strcmp(inst->ident, "movzbq") == 0) {
    if (ops->length != 2) {
      ERROR(inst, "'%s' expects 2 operands.", inst->ident);
    }
    Op *src = ops->buffer[0], *dest = ops->buffer[1];
    if (src->type != OP_REG && src->type != OP_MEM) {
      ERROR(src->token, "register or memory operand is expected.");
    }
    if (src->type == OP_REG && src->regtype != REG_BYTE) {
      ERROR(src->token, "operand type mismatched.");
    }
    if (dest->type == OP_REG && dest->regtype != REG_QUAD) {
      ERROR(dest->token, "operand type mismatched.");
    }
    return inst_op2(ST_MOVZB, INST_QUAD, src, dest, inst);
  }

  if (strcmp(inst->ident, "movzbl") == 0) {
    if (ops->length != 2) {
      ERROR(inst, "'%s' expects 2 operands.", inst->ident);
    }
    Op *src = ops->buffer[0], *dest = ops->buffer[1];
    if (src->type != OP_REG && src->type != OP_MEM) {
      ERROR(src->token, "register or memory operand is expected.");
    }
    if (src->type == OP_REG && src->regtype != REG_BYTE) {
      ERROR(src->token, "operand type mismatched.");
    }
    if (dest->type == OP_REG && dest->regtype != REG_LONG) {
      ERROR(dest->token, "operand type mismatched.");
    }
    return inst_op2(ST_MOVZB, INST_LONG, src, dest, inst);
  }

  if (strcmp(inst->ident, "movzbw") == 0) {
    if (ops->length != 2) {
      ERROR(inst, "'%s' expects 2 operands.", inst->ident);
    }
    Op *src = ops->buffer[0], *dest = ops->buffer[1];
    if (src->type != OP_REG && src->type != OP_MEM) {
      ERROR(src->token, "register or memory operand is expected.");
    }
    if (src->type == OP_REG && src->regtype != REG_BYTE) {
      ERROR(src->token, "operand type mismatched.");
    }
    if (dest->type == OP_REG && dest->regtype != REG_WORD) {
      ERROR(dest->token, "operand type mismatched.");
    }
    return inst_op2(ST_MOVZB, INST_WORD, src, dest, inst);
  }

  if (strcmp(inst->ident, "movzwq") == 0) {
    if (ops->length != 2) {
      ERROR(inst, "'%s' expects 2 operands.", inst->ident);
    }
    Op *src = ops->buffer[0], *dest = ops->buffer[1];
    if (src->type != OP_REG && src->type != OP_MEM) {
      ERROR(src->token, "register or memory operand is expected.");
    }
    if (src->type == OP_REG && src->regtype != REG_WORD) {
      ERROR(src->token, "operand type mismatched.");
    }
    if (dest->type == OP_REG && dest->regtype != REG_QUAD) {
      ERROR(dest->token, "operand type mismatched.");
    }
    return inst_op2(ST_MOVZW, INST_QUAD, src, dest, inst);
  }

  if (strcmp(inst->ident, "movzwl") == 0) {
    if (ops->length != 2) {
      ERROR(inst, "'%s' expects 2 operands.", inst->ident);
    }
    Op *src = ops->buffer[0], *dest = ops->buffer[1];
    if (src->type != OP_REG && src->type != OP_MEM) {
      ERROR(src->token, "register or memory operand is expected.");
    }
    if (src->type == OP_REG && src->regtype != REG_WORD) {
      ERROR(src->token, "operand type mismatched.");
    }
    if (dest->type == OP_REG && dest->regtype != REG_LONG) {
      ERROR(dest->token, "operand type mismatched.");
    }
    return inst_op2(ST_MOVZW, INST_LONG, src, dest, inst);
  }

  if (strcmp(inst->ident, "movsbq") == 0) {
    if (ops->length != 2) {
      ERROR(inst, "'%s' expects 2 operands.", inst->ident);
    }
    Op *src = ops->buffer[0], *dest = ops->buffer[1];
    if (src->type != OP_REG && src->type != OP_MEM) {
      ERROR(src->token, "register or memory operand is expected.");
    }
    if (src->type == OP_REG && src->regtype != REG_BYTE) {
      ERROR(src->token, "operand type mismatched.");
    }
    if (dest->type == OP_REG && dest->regtype != REG_QUAD) {
      ERROR(dest->token, "operand type mismatched.");
    }
    return inst_op2(ST_MOVSB, INST_QUAD, src, dest, inst);
  }

  if (strcmp(inst->ident, "movsbl") == 0) {
    if (ops->length != 2) {
      ERROR(inst, "'%s' expects 2 operands.", inst->ident);
    }
    Op *src = ops->buffer[0], *dest = ops->buffer[1];
    if (src->type != OP_REG && src->type != OP_MEM) {
      ERROR(src->token, "register or memory operand is expected.");
    }
    if (src->type == OP_REG && src->regtype != REG_BYTE) {
      ERROR(src->token, "operand type mismatched.");
    }
    if (dest->type == OP_REG && dest->regtype != REG_LONG) {
      ERROR(dest->token, "operand type mismatched.");
    }
    return inst_op2(ST_MOVSB, INST_LONG, src, dest, inst);
  }

  if (strcmp(inst->ident, "movsbw") == 0) {
    if (ops->length != 2) {
      ERROR(inst, "'%s' expects 2 operands.", inst->ident);
    }
    Op *src = ops->buffer[0], *dest = ops->buffer[1];
    if (src->type != OP_REG && src->type != OP_MEM) {
      ERROR(src->token, "register or memory operand is expected.");
    }
    if (src->type == OP_REG && src->regtype != REG_BYTE) {
      ERROR(src->token, "operand type mismatched.");
    }
    if (dest->type == OP_REG && dest->regtype != REG_WORD) {
      ERROR(dest->token, "operand type mismatched.");
    }
    return inst_op2(ST_MOVSB, INST_WORD, src, dest, inst);
  }

  if (strcmp(inst->ident, "movswq") == 0) {
    if (ops->length != 2) {
      ERROR(inst, "'%s' expects 2 operands.", inst->ident);
    }
    Op *src = ops->buffer[0], *dest = ops->buffer[1];
    if (src->type != OP_REG && src->type != OP_MEM) {
      ERROR(src->token, "register or memory operand is expected.");
    }
    if (src->type == OP_REG && src->regtype != REG_WORD) {
      ERROR(src->token, "operand type mismatched.");
    }
    if (dest->type == OP_REG && dest->regtype != REG_QUAD) {
      ERROR(dest->token, "operand type mismatched.");
    }
    return inst_op2(ST_MOVSW, INST_QUAD, src, dest, inst);
  }

  if (strcmp(inst->ident, "movswl") == 0) {
    if (ops->length != 2) {
      ERROR(inst, "'%s' expects 2 operands.", inst->ident);
    }
    Op *src = ops->buffer[0], *dest = ops->buffer[1];
    if (src->type != OP_REG && src->type != OP_MEM) {
      ERROR(src->token, "register or memory operand is expected.");
    }
    if (src->type == OP_REG && src->regtype != REG_WORD) {
      ERROR(src->token, "operand type mismatched.");
    }
    if (dest->type == OP_REG && dest->regtype != REG_LONG) {
      ERROR(dest->token, "operand type mismatched.");
    }
    return inst_op2(ST_MOVSW, INST_LONG, src, dest, inst);
  }

  if (strcmp(inst->ident, "movslq") == 0) {
    if (ops->length != 2) {
      ERROR(inst, "'%s' expects 2 operands.", inst->ident);
    }
    Op *src = ops->buffer[0], *dest = ops->buffer[1];
    if (src->type != OP_REG && src->type != OP_MEM) {
      ERROR(src->token, "register or memory operand is expected.");
    }
    if (src->type == OP_REG && src->regtype != REG_LONG) {
      ERROR(src->token, "operand type mismatched.");
    }
    if (dest->type == OP_REG && dest->regtype != REG_QUAD) {
      ERROR(dest->token, "operand type mismatched.");
    }
    return inst_op2(ST_MOVSL, INST_QUAD, src, dest, inst);
  }

  if (strcmp(inst->ident, "leaq") == 0) {
    if (ops->length != 2) {
      ERROR(inst, "'%s' expects 2 operands.", inst->ident);
    }
    Op *src = ops->buffer[0], *dest = ops->buffer[1];
    if (src->type != OP_MEM) {
      ERROR(src->token, "first operand should be memory operand.");
    }
    if (dest->type != OP_REG) {
      ERROR(dest->token, "second operand should be register operand.");
    }
    if (dest->regtype != REG_QUAD) {
      ERROR(dest->token, "only 64-bit register is supported.");
    }
    return inst_op2(ST_LEA, INST_QUAD, src, dest, inst);
  }

  if (strcmp(inst->ident, "negq") == 0) {
    if (ops->length != 1) {
      ERROR(inst, "'%s' expects 1 operand.", inst->ident);
    }
    Op *op = ops->buffer[0];
    if (op->type != OP_REG && op->type != OP_MEM) {
      ERROR(inst, "register or memory operand is expected.");
    }
    if (op->type == OP_REG && op->regtype != REG_QUAD) {
      ERROR(inst, "operand type mismatched.");
    }
    return inst_op1(ST_NEG, INST_QUAD, op, inst);
  }

  if (strcmp(inst->ident, "negl") == 0) {
    if (ops->length != 1) {
      ERROR(inst, "'%s' expects 1 operand.", inst->ident);
    }
    Op *op = ops->buffer[0];
    if (op->type != OP_REG && op->type != OP_MEM) {
      ERROR(inst, "register or memory operand is expected.");
    }
    if (op->type == OP_REG && op->regtype != REG_LONG) {
      ERROR(inst, "operand type mismatched.");
    }
    return inst_op1(ST_NEG, INST_LONG, op, inst);
  }

  if (strcmp(inst->ident, "notq") == 0) {
    if (ops->length != 1) {
      ERROR(inst, "'%s' expects 1 operand.", inst->ident);
    }
    Op *op = ops->buffer[0];
    if (op->type != OP_REG && op->type != OP_MEM) {
      ERROR(inst, "register or memory operand is expected.");
    }
    if (op->type == OP_REG && op->regtype != REG_QUAD) {
      ERROR(inst, "operand type mismatched.");
    }
    return inst_op1(ST_NOT, INST_QUAD, op, inst);
  }

  if (strcmp(inst->ident, "notl") == 0) {
    if (ops->length != 1) {
      ERROR(inst, "'%s' expects 1 operand.", inst->ident);
    }
    Op *op = ops->buffer[0];
    if (op->type != OP_REG && op->type != OP_MEM) {
      ERROR(inst, "register or memory operand is expected.");
    }
    if (op->type == OP_REG && op->regtype != REG_LONG) {
      ERROR(inst, "operand type mismatched.");
    }
    return inst_op1(ST_NOT, INST_LONG, op, inst);
  }

  if (strcmp(inst->ident, "addq") == 0) {
    if (ops->length != 2) {
      ERROR(inst, "'%s' expects 2 operands.", inst->ident);
    }
    Op *src = ops->buffer[0], *dest = ops->buffer[1];
    if (dest->type == OP_IMM) {
      ERROR(dest->token, "destination cannot be an immediate.");
    }
    if (src->type == OP_MEM && dest->type == OP_MEM) {
      ERROR(inst, "both of source and destination cannot be memory operands.");
    }
    if (src->type == OP_REG && src->regtype != REG_QUAD) {
      ERROR(src->token, "operand type mismatched.");
    }
    if (dest->type == OP_REG && dest->regtype != REG_QUAD) {
      ERROR(dest->token, "operand type mismatched.");
    }
    return inst_op2(ST_ADD, INST_QUAD, src, dest, inst);
  }

  if (strcmp(inst->ident, "addl") == 0) {
    if (ops->length != 2) {
      ERROR(inst, "'%s' expects 2 operands.", inst->ident);
    }
    Op *src = ops->buffer[0], *dest = ops->buffer[1];
    if (dest->type == OP_IMM) {
      ERROR(dest->token, "destination cannot be an immediate.");
    }
    if (src->type == OP_MEM && dest->type == OP_MEM) {
      ERROR(inst, "both of source and destination cannot be memory operands.");
    }
    if (src->type == OP_REG && src->regtype != REG_LONG) {
      ERROR(src->token, "operand type mismatched.");
    }
    if (dest->type == OP_REG && dest->regtype != REG_LONG) {
      ERROR(dest->token, "operand type mismatched.");
    }
    return inst_op2(ST_ADD, INST_LONG, src, dest, inst);
  }

  if (strcmp(inst->ident, "subq") == 0) {
    if (ops->length != 2) {
      ERROR(inst, "'%s' expects 2 operands.", inst->ident);
    }
    Op *src = ops->buffer[0], *dest = ops->buffer[1];
    if (dest->type == OP_IMM) {
      ERROR(dest->token, "destination cannot be an immediate.");
    }
    if (src->type == OP_MEM && dest->type == OP_MEM) {
      ERROR(inst, "both of source and destination cannot be memory operands.");
    }
    if (src->type == OP_REG && src->regtype != REG_QUAD) {
      ERROR(src->token, "operand type mismatched.");
    }
    if (dest->type == OP_REG && dest->regtype != REG_QUAD) {
      ERROR(dest->token, "operand type mismatched.");
    }
    return inst_op2(ST_SUB, INST_QUAD, src, dest, inst);
  }

  if (strcmp(inst->ident, "subl") == 0) {
    if (ops->length != 2) {
      ERROR(inst, "'%s' expects 2 operands.", inst->ident);
    }
    Op *src = ops->buffer[0], *dest = ops->buffer[1];
    if (dest->type == OP_IMM) {
      ERROR(dest->token, "destination cannot be an immediate.");
    }
    if (src->type == OP_MEM && dest->type == OP_MEM) {
      ERROR(inst, "both of source and destination cannot be memory operands.");
    }
    if (src->type == OP_REG && src->regtype != REG_LONG) {
      ERROR(src->token, "operand type mismatched.");
    }
    if (dest->type == OP_REG && dest->regtype != REG_LONG) {
      ERROR(dest->token, "operand type mismatched.");
    }
    return inst_op2(ST_SUB, INST_LONG, src, dest, inst);
  }

  if (strcmp(inst->ident, "mulq") == 0) {
    if (ops->length != 1) {
      ERROR(inst, "'%s' expects 1 operand.", inst->ident);
    }
    Op *op = ops->buffer[0];
    if (op->type != OP_REG && op->type != OP_MEM) {
      ERROR(inst, "register or memory operand is expected.");
    }
    if (op->type == OP_REG && op->regtype != REG_QUAD) {
      ERROR(inst, "operand type mismatched.");
    }
    return inst_op1(ST_MUL, INST_QUAD, op, inst);
  }

  if (strcmp(inst->ident, "mull") == 0) {
    if (ops->length != 1) {
      ERROR(inst, "'%s' expects 1 operand.", inst->ident);
    }
    Op *op = ops->buffer[0];
    if (op->type != OP_REG && op->type != OP_MEM) {
      ERROR(inst, "register or memory operand is expected.");
    }
    if (op->type == OP_REG && op->regtype != REG_LONG) {
      ERROR(inst, "operand type mismatched.");
    }
    return inst_op1(ST_MUL, INST_LONG, op, inst);
  }

  if (strcmp(inst->ident, "imulq") == 0) {
    if (ops->length != 1) {
      ERROR(inst, "'%s' expects 1 operand.", inst->ident);
    }
    Op *op = ops->buffer[0];
    if (op->type != OP_REG && op->type != OP_MEM) {
      ERROR(inst, "register or memory operand is expected.");
    }
    if (op->type == OP_REG && op->regtype != REG_QUAD) {
      ERROR(inst, "operand type mismatched.");
    }
    return inst_op1(ST_IMUL, INST_QUAD, op, inst);
  }

  if (strcmp(inst->ident, "imull") == 0) {
    if (ops->length != 1) {
      ERROR(inst, "'%s' expects 1 operand.", inst->ident);
    }
    Op *op = ops->buffer[0];
    if (op->type != OP_REG && op->type != OP_MEM) {
      ERROR(inst, "register or memory operand is expected.");
    }
    if (op->type == OP_REG && op->regtype != REG_LONG) {
      ERROR(inst, "operand type mismatched.");
    }
    return inst_op1(ST_IMUL, INST_LONG, op, inst);
  }

  if (strcmp(inst->ident, "divq") == 0) {
    if (ops->length != 1) {
      ERROR(inst, "'%s' expects 1 operand.", inst->ident);
    }
    Op *op = ops->buffer[0];
    if (op->type != OP_REG && op->type != OP_MEM) {
      ERROR(inst, "register or memory operand is expected.");
    }
    if (op->type == OP_REG && op->regtype != REG_QUAD) {
      ERROR(inst, "operand type mismatched.");
    }
    return inst_op1(ST_DIV, INST_QUAD, op, inst);
  }

  if (strcmp(inst->ident, "divl") == 0) {
    if (ops->length != 1) {
      ERROR(inst, "'%s' expects 1 operand.", inst->ident);
    }
    Op *op = ops->buffer[0];
    if (op->type != OP_REG && op->type != OP_MEM) {
      ERROR(inst, "register or memory operand is expected.");
    }
    if (op->type == OP_REG && op->regtype != REG_LONG) {
      ERROR(inst, "operand type mismatched.");
    }
    return inst_op1(ST_DIV, INST_LONG, op, inst);
  }

  if (strcmp(inst->ident, "idivq") == 0) {
    if (ops->length != 1) {
      ERROR(inst, "'%s' expects 1 operand.", inst->ident);
    }
    Op *op = ops->buffer[0];
    if (op->type != OP_REG && op->type != OP_MEM) {
      ERROR(inst, "register or memory operand is expected.");
    }
    if (op->type == OP_REG && op->regtype != REG_QUAD) {
      ERROR(inst, "operand type mismatched.");
    }
    return inst_op1(ST_IDIV, INST_QUAD, op, inst);
  }

  if (strcmp(inst->ident, "idivl") == 0) {
    if (ops->length != 1) {
      ERROR(inst, "'%s' expects 1 operand.", inst->ident);
    }
    Op *op = ops->buffer[0];
    if (op->type != OP_REG && op->type != OP_MEM) {
      ERROR(inst, "register or memory operand is expected.");
    }
    if (op->type == OP_REG && op->regtype != REG_LONG) {
      ERROR(inst, "operand type mismatched.");
    }
    return inst_op1(ST_IDIV, INST_LONG, op, inst);
  }

  if (strcmp(inst->ident, "andq") == 0) {
    if (ops->length != 2) {
      ERROR(inst, "'%s' expects 2 operands.", inst->ident);
    }
    Op *src = ops->buffer[0], *dest = ops->buffer[1];
    if (dest->type == OP_IMM) {
      ERROR(dest->token, "destination cannot be an immediate.");
    }
    if (src->type == OP_MEM && dest->type == OP_MEM) {
      ERROR(inst, "both of source and destination cannot be memory operands.");
    }
    if (src->type == OP_REG && src->regtype != REG_QUAD) {
      ERROR(src->token, "operand type mismatched.");
    }
    if (dest->type == OP_REG && dest->regtype != REG_QUAD) {
      ERROR(dest->token, "operand type mismatched.");
    }
    return inst_op2(ST_AND, INST_QUAD, src, dest, inst);
  }

  if (strcmp(inst->ident, "andl") == 0) {
    if (ops->length != 2) {
      ERROR(inst, "'%s' expects 2 operands.", inst->ident);
    }
    Op *src = ops->buffer[0], *dest = ops->buffer[1];
    if (dest->type == OP_IMM) {
      ERROR(dest->token, "destination cannot be an immediate.");
    }
    if (src->type == OP_MEM && dest->type == OP_MEM) {
      ERROR(inst, "both of source and destination cannot be memory operands.");
    }
    if (src->type == OP_REG && src->regtype != REG_LONG) {
      ERROR(src->token, "operand type mismatched.");
    }
    if (dest->type == OP_REG && dest->regtype != REG_LONG) {
      ERROR(dest->token, "operand type mismatched.");
    }
    return inst_op2(ST_AND, INST_LONG, src, dest, inst);
  }

  if (strcmp(inst->ident, "xorq") == 0) {
    if (ops->length != 2) {
      ERROR(inst, "'%s' expects 2 operands.", inst->ident);
    }
    Op *src = ops->buffer[0], *dest = ops->buffer[1];
    if (dest->type == OP_IMM) {
      ERROR(dest->token, "destination cannot be an immediate.");
    }
    if (src->type == OP_MEM && dest->type == OP_MEM) {
      ERROR(inst, "both of source and destination cannot be memory operands.");
    }
    if (src->type == OP_REG && src->regtype != REG_QUAD) {
      ERROR(src->token, "operand type mismatched.");
    }
    if (dest->type == OP_REG && dest->regtype != REG_QUAD) {
      ERROR(dest->token, "operand type mismatched.");
    }
    return inst_op2(ST_XOR, INST_QUAD, src, dest, inst);
  }

  if (strcmp(inst->ident, "xorl") == 0) {
    if (ops->length != 2) {
      ERROR(inst, "'%s' expects 2 operands.", inst->ident);
    }
    Op *src = ops->buffer[0], *dest = ops->buffer[1];
    if (dest->type == OP_IMM) {
      ERROR(dest->token, "destination cannot be an immediate.");
    }
    if (src->type == OP_MEM && dest->type == OP_MEM) {
      ERROR(inst, "both of source and destination cannot be memory operands.");
    }
    if (src->type == OP_REG && src->regtype != REG_LONG) {
      ERROR(src->token, "operand type mismatched.");
    }
    if (dest->type == OP_REG && dest->regtype != REG_LONG) {
      ERROR(dest->token, "operand type mismatched.");
    }
    return inst_op2(ST_XOR, INST_LONG, src, dest, inst);
  }

  if (strcmp(inst->ident, "orq") == 0) {
    if (ops->length != 2) {
      ERROR(inst, "'%s' expects 2 operands.", inst->ident);
    }
    Op *src = ops->buffer[0], *dest = ops->buffer[1];
    if (dest->type == OP_IMM) {
      ERROR(dest->token, "destination cannot be an immediate.");
    }
    if (src->type == OP_MEM && dest->type == OP_MEM) {
      ERROR(inst, "both of source and destination cannot be memory operands.");
    }
    if (src->type == OP_REG && src->regtype != REG_QUAD) {
      ERROR(src->token, "operand type mismatched.");
    }
    if (dest->type == OP_REG && dest->regtype != REG_QUAD) {
      ERROR(dest->token, "operand type mismatched.");
    }
    return inst_op2(ST_OR, INST_QUAD, src, dest, inst);
  }

  if (strcmp(inst->ident, "orl") == 0) {
    if (ops->length != 2) {
      ERROR(inst, "'%s' expects 2 operands.", inst->ident);
    }
    Op *src = ops->buffer[0], *dest = ops->buffer[1];
    if (dest->type == OP_IMM) {
      ERROR(dest->token, "destination cannot be an immediate.");
    }
    if (src->type == OP_MEM && dest->type == OP_MEM) {
      ERROR(inst, "both of source and destination cannot be memory operands.");
    }
    if (src->type == OP_REG && src->regtype != REG_LONG) {
      ERROR(src->token, "operand type mismatched.");
    }
    if (dest->type == OP_REG && dest->regtype != REG_LONG) {
      ERROR(dest->token, "operand type mismatched.");
    }
    return inst_op2(ST_OR, INST_LONG, src, dest, inst);
  }

  if (strcmp(inst->ident, "salq") == 0) {
    if (ops->length != 2) {
      ERROR(inst, "'%s' expects 2 operand.", inst->ident);
    }
    Op *src = ops->buffer[0], *dest = ops->buffer[1];
    if (src->type != OP_REG || src->regtype != REG_BYTE || src->regcode != REG_CX) {
      ERROR(src->token, "only %%cl is supported.");
    }
    if (dest->type != OP_REG && dest->type != OP_MEM) {
      ERROR(dest->token, "register or memory operand is expected.");
    }
    if (dest->type == OP_REG && dest->regtype != REG_QUAD) {
      ERROR(dest->token, "operand type mismatched.");
    }
    return inst_op2(ST_SAL, INST_QUAD, src, dest, inst);
  }

  if (strcmp(inst->ident, "sall") == 0) {
    if (ops->length != 2) {
      ERROR(inst, "'%s' expects 2 operand.", inst->ident);
    }
    Op *src = ops->buffer[0], *dest = ops->buffer[1];
    if (src->type != OP_REG || src->regtype != REG_BYTE || src->regcode != REG_CX) {
      ERROR(src->token, "only %%cl is supported.");
    }
    if (dest->type != OP_REG && dest->type != OP_MEM) {
      ERROR(dest->token, "register or memory operand is expected.");
    }
    if (dest->type == OP_REG && dest->regtype != REG_LONG) {
      ERROR(dest->token, "operand type mismatched.");
    }
    return inst_op2(ST_SAL, INST_LONG, src, dest, inst);
  }

  if (strcmp(inst->ident, "sarq") == 0) {
    if (ops->length != 2) {
      ERROR(inst, "'%s' expects 2 operand.", inst->ident);
    }
    Op *src = ops->buffer[0], *dest = ops->buffer[1];
    if (src->type != OP_REG || src->regtype != REG_BYTE || src->regcode != REG_CX) {
      ERROR(src->token, "only %%cl is supported.");
    }
    if (dest->type != OP_REG && dest->type != OP_MEM) {
      ERROR(dest->token, "register or memory operand is expected.");
    }
    if (dest->type == OP_REG && dest->regtype != REG_QUAD) {
      ERROR(dest->token, "operand type mismatched.");
    }
    return inst_op2(ST_SAR, INST_QUAD, src, dest, inst);
  }

  if (strcmp(inst->ident, "sarl") == 0) {
    if (ops->length != 2) {
      ERROR(inst, "'%s' expects 2 operand.", inst->ident);
    }
    Op *src = ops->buffer[0], *dest = ops->buffer[1];
    if (src->type != OP_REG || src->regtype != REG_BYTE || src->regcode != REG_CX) {
      ERROR(src->token, "only %%cl is supported.");
    }
    if (dest->type != OP_REG && dest->type != OP_MEM) {
      ERROR(dest->token, "register or memory operand is expected.");
    }
    if (dest->type == OP_REG && dest->regtype != REG_LONG) {
      ERROR(dest->token, "operand type mismatched.");
    }
    return inst_op2(ST_SAR, INST_LONG, src, dest, inst);
  }

  if (strcmp(inst->ident, "cmpq") == 0) {
    if (ops->length != 2) {
      ERROR(inst, "'%s' expects 2 operands.", inst->ident);
    }
    Op *src = ops->buffer[0], *dest = ops->buffer[1];
    if (dest->type == OP_IMM) {
      ERROR(dest->token, "destination cannot be an immediate.");
    }
    if (src->type == OP_MEM && dest->type == OP_MEM) {
      ERROR(inst, "both of source and destination cannot be memory operands.");
    }
    if (src->type == OP_REG && src->regtype != REG_QUAD) {
      ERROR(src->token, "operand type mismatched.");
    }
    if (dest->type == OP_REG && dest->regtype != REG_QUAD) {
      ERROR(dest->token, "operand type mismatched.");
    }
    return inst_op2(ST_CMP, INST_QUAD, src, dest, inst);
  }

  if (strcmp(inst->ident, "cmpl") == 0) {
    if (ops->length != 2) {
      ERROR(inst, "'%s' expects 2 operands.", inst->ident);
    }
    Op *src = ops->buffer[0], *dest = ops->buffer[1];
    if (dest->type == OP_IMM) {
      ERROR(dest->token, "destination cannot be an immediate.");
    }
    if (src->type == OP_MEM && dest->type == OP_MEM) {
      ERROR(inst, "both of source and destination cannot be memory operands.");
    }
    if (src->type == OP_REG && src->regtype != REG_LONG) {
      ERROR(src->token, "operand type mismatched.");
    }
    if (dest->type == OP_REG && dest->regtype != REG_LONG) {
      ERROR(dest->token, "operand type mismatched.");
    }
    return inst_op2(ST_CMP, INST_LONG, src, dest, inst);
  }

  if (strcmp(inst->ident, "cmpw") == 0) {
    if (ops->length != 2) {
      ERROR(inst, "'%s' expects 2 operands.", inst->ident);
    }
    Op *src = ops->buffer[0], *dest = ops->buffer[1];
    if (dest->type == OP_IMM) {
      ERROR(dest->token, "destination cannot be an immediate.");
    }
    if (src->type == OP_MEM && dest->type == OP_MEM) {
      ERROR(inst, "both of source and destination cannot be memory operands.");
    }
    if (src->type == OP_REG && src->regtype != REG_WORD) {
      ERROR(src->token, "operand type mismatched.");
    }
    if (dest->type == OP_REG && dest->regtype != REG_WORD) {
      ERROR(dest->token, "operand type mismatched.");
    }

    return inst_op2(ST_CMP, INST_WORD, src, dest, inst);
  }

  if (strcmp(inst->ident, "cmpb") == 0) {
    if (ops->length != 2) {
      ERROR(inst, "'%s' expects 2 operands.", inst->ident);
    }
    Op *src = ops->buffer[0], *dest = ops->buffer[1];
    if (dest->type == OP_IMM) {
      ERROR(dest->token, "destination cannot be an immediate.");
    }
    if (src->type == OP_MEM && dest->type == OP_MEM) {
      ERROR(inst, "both of source and destination cannot be memory operands.");
    }
    if (src->type == OP_REG && src->regtype != REG_BYTE) {
      ERROR(src->token, "operand type mismatched.");
    }
    if (dest->type == OP_REG && dest->regtype != REG_BYTE) {
      ERROR(dest->token, "operand type mismatched.");
    }
    return inst_op2(ST_CMP, INST_BYTE, src, dest, inst);
  }

  if (strcmp(inst->ident, "sete") == 0) {
    if (ops->length != 1) {
      ERROR(inst, "'%s' expects 1 operand.", inst->ident);
    }
    Op *op = ops->buffer[0];
    if (op->type != OP_REG && op->type != OP_MEM) {
      ERROR(inst, "register or memory operand is expected.");
    }
    if (op->type == OP_REG && op->regtype != REG_BYTE) {
      ERROR(inst, "operand type mismatched.");
    }
    return inst_op1(ST_SETE, INST_BYTE, op, inst);
  }

  if (strcmp(inst->ident, "setne") == 0) {
    if (ops->length != 1) {
      ERROR(inst, "'%s' expects 1 operand.", inst->ident);
    }
    Op *op = ops->buffer[0];
    if (op->type != OP_REG && op->type != OP_MEM) {
      ERROR(inst, "register or memory operand is expected.");
    }
    if (op->type == OP_REG && op->regtype != REG_BYTE) {
      ERROR(inst, "operand type mismatched.");
    }
    return inst_op1(ST_SETNE, INST_BYTE, op, inst);
  }

  if (strcmp(inst->ident, "setb") == 0) {
    if (ops->length != 1) {
      ERROR(inst, "'%s' expects 1 operand.", inst->ident);
    }
    Op *op = ops->buffer[0];
    if (op->type != OP_REG && op->type != OP_MEM) {
      ERROR(inst, "register or memory operand is expected.");
    }
    if (op->type == OP_REG && op->regtype != REG_BYTE) {
      ERROR(inst, "operand type mismatched.");
    }
    return inst_op1(ST_SETB, INST_BYTE, op, inst);
  }

  if (strcmp(inst->ident, "setl") == 0) {
    if (ops->length != 1) {
      ERROR(inst, "'%s' expects 1 operand.", inst->ident);
    }
    Op *op = ops->buffer[0];
    if (op->type != OP_REG && op->type != OP_MEM) {
      ERROR(inst, "register or memory operand is expected.");
    }
    if (op->type == OP_REG && op->regtype != REG_BYTE) {
      ERROR(inst, "operand type mismatched.");
    }
    return inst_op1(ST_SETL, INST_BYTE, op, inst);
  }

  if (strcmp(inst->ident, "setg") == 0) {
    if (ops->length != 1) {
      ERROR(inst, "'%s' expects 1 operand.", inst->ident);
    }
    Op *op = ops->buffer[0];
    if (op->type != OP_REG && op->type != OP_MEM) {
      ERROR(inst, "register or memory operand is expected.");
    }
    if (op->type == OP_REG && op->regtype != REG_BYTE) {
      ERROR(inst, "operand type mismatched.");
    }
    return inst_op1(ST_SETG, INST_BYTE, op, inst);
  }

  if (strcmp(inst->ident, "setbe") == 0) {
    if (ops->length != 1) {
      ERROR(inst, "'%s' expects 1 operand.", inst->ident);
    }
    Op *op = ops->buffer[0];
    if (op->type != OP_REG && op->type != OP_MEM) {
      ERROR(inst, "register or memory operand is expected.");
    }
    if (op->type == OP_REG && op->regtype != REG_BYTE) {
      ERROR(inst, "operand type mismatched.");
    }
    return inst_op1(ST_SETBE, INST_BYTE, op, inst);
  }

  if (strcmp(inst->ident, "setle") == 0) {
    if (ops->length != 1) {
      ERROR(inst, "'%s' expects 1 operand.", inst->ident);
    }
    Op *op = ops->buffer[0];
    if (op->type != OP_REG && op->type != OP_MEM) {
      ERROR(inst, "register or memory operand is expected.");
    }
    if (op->type == OP_REG && op->regtype != REG_BYTE) {
      ERROR(inst, "operand type mismatched.");
    }
    return inst_op1(ST_SETLE, INST_BYTE, op, inst);
  }

  if (strcmp(inst->ident, "setge") == 0) {
    if (ops->length != 1) {
      ERROR(inst, "'%s' expects 1 operand.", inst->ident);
    }
    Op *op = ops->buffer[0];
    if (op->type != OP_REG && op->type != OP_MEM) {
      ERROR(inst, "register or memory operand is expected.");
    }
    if (op->type == OP_REG && op->regtype != REG_BYTE) {
      ERROR(inst, "operand type mismatched.");
    }
    return inst_op1(ST_SETGE, INST_BYTE, op, inst);
  }

  if (strcmp(inst->ident, "jmp") == 0) {
    if (ops->length != 1) {
      ERROR(inst, "'%s' expects 1 operand.", inst->ident);
    }
    Op *op = ops->buffer[0];
    if (op->type != OP_SYM) {
      ERROR(op->token, "only symbol is supported.");
    }
    return inst_op1(ST_JMP, INST_QUAD, op, inst);
  }

  if (strcmp(inst->ident, "je") == 0) {
    if (ops->length != 1) {
      ERROR(inst, "'%s' expects 1 operand.", inst->ident);
    }
    Op *op = ops->buffer[0];
    if (op->type != OP_SYM) {
      ERROR(op->token, "only symbol is supported.");
    }
    return inst_op1(ST_JE, INST_QUAD, op, inst);
  }

  if (strcmp(inst->ident, "jne") == 0) {
    if (ops->length != 1) {
      ERROR(inst, "'%s' expects 1 operand.", inst->ident);
    }
    Op *op = ops->buffer[0];
    if (op->type != OP_SYM) {
      ERROR(op->token, "only symbol is supported.");
    }
    return inst_op1(ST_JNE, INST_QUAD, op, inst);
  }

  if (strcmp(inst->ident, "call") == 0) {
    if (ops->length != 1) {
      ERROR(inst, "'%s' expects 1 operand.", inst->ident);
    }
    Op *op = ops->buffer[0];
    if (op->type != OP_SYM) {
      ERROR(op->token, "only symbol is supported.");
    }
    return inst_op1(ST_CALL, INST_QUAD, op, inst);
  }

  if (strcmp(inst->ident, "leave") == 0) {
    if (ops->length != 0) {
      ERROR(inst, "'%s' expects no operand.", inst->ident);
    }
    return inst_op0(ST_LEAVE, INST_QUAD, inst);
  }

  if (strcmp(inst->ident, "ret") == 0) {
    if (ops->length != 0) {
      ERROR(inst, "'%s' expects no operand.", inst->ident);
    }
    return inst_op0(ST_RET, INST_QUAD, inst);
  }

  ERROR(inst, "unknown instruction '%s'.", inst->ident);
}

Stmt *parse_stmt(void) {
  // the first token should be identifier.
  Token *token = expect(TK_IDENT);

  // label
  if (peek() && read(TK_SEMICOLON)) {
    return (Stmt *) label_new(token->ident, token);
  }

  // .text directive
  if (strcmp(token->ident, ".text") == 0) {
    return (Stmt *) dir_text(token);
  }

  // .data directive
  if (strcmp(token->ident, ".data") == 0) {
    return (Stmt *) dir_data(token);
  }

  // .section directive
  // only .rodata is supported.
  if (strcmp(token->ident, ".section") == 0) {
    char *ident = expect(TK_IDENT)->ident;
    if (strcmp(ident, ".rodata") != 0) {
      ERROR(token, "only '.rodata' is supported.");
    }
    return (Stmt *) dir_section(ident, token);
  }

  // .global directive
  if (strcmp(token->ident, ".global") == 0) {
    char *ident = expect(TK_IDENT)->ident;
    return (Stmt *) dir_global(ident, token);
  }

  // .zero directive
  if (strcmp(token->ident, ".zero") == 0) {
    int num = expect(TK_NUM)->num;
    return (Stmt *) dir_zero(num, token);
  }

  // .long directive
  if (strcmp(token->ident, ".long") == 0) {
    int num = expect(TK_NUM)->num;
    return (Stmt *) dir_long(num, token);
  }

  // .quad directive
  if (strcmp(token->ident, ".quad") == 0) {
    char *ident = expect(TK_IDENT)->ident;
    return (Stmt *) dir_quad(ident, token);
  }

  // .ascii directive
  if (strcmp(token->ident, ".ascii") == 0) {
    String *string = expect(TK_STR)->string;
    return (Stmt *) dir_ascii(string, token);
  }

  // instruction
  return (Stmt *) parse_inst(token);
}

Vector *as_parse(Vector *lines) {
  Vector *stmts = vector_new();

  for (int i = 0; i < lines->length; i++) {
    Vector *line = lines->buffer[i];

    // skip if the line has no token.
    if (line->length == 0) continue;

    tokens = (Token **) line->buffer;
    pos = 0;

    Stmt *stmt = parse_stmt();
    if (peek()) {
      ERROR(peek(), "invalid statement.");
    }
    vector_push(stmts, stmt);
  }

  return stmts;
}
