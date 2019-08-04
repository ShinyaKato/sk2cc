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

static Inst *inst_new(StmtType type, InstSuffix suffix, Vector *ops, Token *token) {
  Inst *inst = (Inst *) calloc(1, sizeof(Inst));
  inst->type = type;
  inst->suffix = suffix;
  inst->ops = ops;
  inst->token = token;
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

typedef struct {
  StmtType type;
  InstSuffix suffix;
} InstTypeSuffix;

static Map *dirs;
static Map *insts;

static Map *create_dirs(void) {
  Map *map = map_new();

  map_puti(map, ".text", ST_TEXT);
  map_puti(map, ".data", ST_DATA);
  map_puti(map, ".section", ST_SECTION);
  map_puti(map, ".global", ST_GLOBAL);
  map_puti(map, ".zero", ST_ZERO);
  map_puti(map, ".long", ST_LONG);
  map_puti(map, ".quad", ST_QUAD);
  map_puti(map, ".ascii", ST_ASCII);

  return map;
}

static void put_insts(Map *map, char *inst, StmtType type) {
  char suffix_char[4] = { 'b', 'w', 'l', 'q' };
  InstSuffix suffix[4] = { INST_BYTE, INST_WORD, INST_LONG, INST_QUAD };

  for (int i = 0; i < 4; i++) {
    String *key = string_new();
    string_write(key, inst);
    string_push(key, suffix_char[i]);

    InstTypeSuffix *value = calloc(1, sizeof(InstTypeSuffix));
    value->type = type;
    value->suffix = suffix[i];

    map_put(map, key->buffer, value);
  }

  InstTypeSuffix *value = calloc(1, sizeof(InstTypeSuffix));
  value->type = type;
  value->suffix = -1;

  map_put(map, inst, value);
}

static Map *create_insts(void) {
  Map *map = map_new();

  put_insts(map, "push", ST_PUSH);
  put_insts(map, "pop", ST_POP);
  put_insts(map, "cltd", ST_CLTD);
  put_insts(map, "cqto", ST_CQTO);
  put_insts(map, "mov", ST_MOV);
  put_insts(map, "movzb", ST_MOVZB);
  put_insts(map, "movzw", ST_MOVZW);
  put_insts(map, "movsb", ST_MOVSB);
  put_insts(map, "movsw", ST_MOVSW);
  put_insts(map, "movsl", ST_MOVSL);
  put_insts(map, "lea", ST_LEA);
  put_insts(map, "neg", ST_NEG);
  put_insts(map, "not", ST_NOT);
  put_insts(map, "add", ST_ADD);
  put_insts(map, "sub", ST_SUB);
  put_insts(map, "mul", ST_MUL);
  put_insts(map, "imul", ST_IMUL);
  put_insts(map, "div", ST_DIV);
  put_insts(map, "idiv", ST_IDIV);
  put_insts(map, "and", ST_AND);
  put_insts(map, "xor", ST_XOR);
  put_insts(map, "or", ST_OR);
  put_insts(map, "sal", ST_SAL);
  put_insts(map, "sar", ST_SAR);
  put_insts(map, "cmp", ST_CMP);
  put_insts(map, "sete", ST_SETE);
  put_insts(map, "setne", ST_SETNE);
  put_insts(map, "setb", ST_SETB);
  put_insts(map, "setl", ST_SETL);
  put_insts(map, "setg", ST_SETG);
  put_insts(map, "setbe", ST_SETBE);
  put_insts(map, "setle", ST_SETLE);
  put_insts(map, "setge", ST_SETGE);
  put_insts(map, "jmp", ST_JMP);
  put_insts(map, "je", ST_JE);
  put_insts(map, "jne", ST_JNE);
  put_insts(map, "call", ST_CALL);
  put_insts(map, "leave", ST_LEAVE);
  put_insts(map, "ret", ST_RET);

  return map;
}

static Stmt *parse_dir(StmtType dir_type, Token *token) {
  switch (dir_type) {
    case ST_TEXT: return (Stmt *) dir_new(ST_TEXT, token);
    case ST_DATA: return (Stmt *) dir_new(ST_DATA, token);
    case ST_SECTION: {
      char *ident = expect(TK_IDENT)->ident;
      if (strcmp(ident, ".rodata") != 0) {
        ERROR(token, "only '.rodata' is supported.");
      }
      Dir *dir = dir_new(ST_SECTION, token);
      dir->ident = ident;
      return (Stmt *) dir;
    }
    case ST_GLOBAL: {
      char *ident = expect(TK_IDENT)->ident;
      Dir *dir = dir_new(ST_GLOBAL, token);
      dir->ident = ident;
      return (Stmt *) dir;
    }
    case ST_ZERO: {
      int num = expect(TK_NUM)->num;
      Dir *dir = dir_new(ST_ZERO, token);
      dir->num = num;
      return (Stmt *) dir;
    }
    case ST_LONG: {
      int num = expect(TK_NUM)->num;
      Dir *dir = dir_new(ST_LONG, token);
      dir->num = num;
      return (Stmt *) dir;
    }
    case ST_QUAD: {
      char *ident = expect(TK_IDENT)->ident;
      Dir *dir = dir_new(ST_QUAD, token);
      dir->ident = ident;
      return (Stmt *) dir;
    }
    case ST_ASCII: {
      String *string = expect(TK_STR)->string;
      Dir *dir = dir_new(ST_ASCII, token);
      dir->string = string;
      return (Stmt *) dir;
    }
    default: assert(false); // unreachable
  }
}

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

  if (read(TK_LPAREN)) {
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

  if (check(TK_NEWLINE)) return ops;
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
  } while (read(TK_COMMA));

  return ops;
}

static Inst *parse_inst(StmtType type, StmtType suffix, Token *token) {
  Vector *ops = parse_ops();

  return inst_new(type, suffix, ops, token);
}

static Stmt *parse_stmt(void) {
  // the first token should be identifier.
  Token *token = expect(TK_IDENT);

  // label
  if (read(TK_SEMICOLON)) {
    return (Stmt *) label_new(token->ident, token);
  }

  // directives
  StmtType dir_type = map_lookupi(dirs, token->ident);
  if (dir_type) {
    return (Stmt *) parse_dir(dir_type, token);
  }

  // instructions
  InstTypeSuffix *inst = map_lookup(insts, token->ident);
  if (inst) {
    return (Stmt *) parse_inst(inst->type, inst->suffix, token);
  }

  ERROR(token, "invalid assembler statement.");
}

Vector *as_parse(Vector *_tokens) {
  Vector *stmts = vector_new();

  tokens = (Token **) _tokens->buffer;
  pos = 0;

  if (!dirs) {
    dirs = create_dirs();
  }
  if (!insts) {
    insts = create_insts();
  }

  while (!check(TK_EOF)) {
    if (read(TK_NEWLINE)) continue;

    Stmt *stmt = parse_stmt();

    // label statement can be followed by another statement
    if (stmt->type == ST_LABEL && !check(TK_NEWLINE)) {
      vector_push(stmts, stmt);
      continue;
    }
    expect(TK_NEWLINE);

    vector_push(stmts, stmt);
  }

  return stmts;
}
