#include "as.h"

typedef struct rel {
  int offset;
  char *ident;
} Rel;

static Rel *rel_new(int offset, char *ident) {
  Rel *rel = (Rel *) calloc(1, sizeof(Rel));
  rel->offset = offset;
  rel->ident = ident;
  return rel;
}

static Vector *insts;
static Map *symbols;

static Binary *text;
static Binary *symtab;
static String *strtab;
static Binary *rela_text;

static int *offsets;
static Vector *rels;
static Map *gsyms;

static void gen_rex(bool w, Reg reg, Reg index, Reg base, bool required) {
  bool r = reg & 0x08;
  bool x = index & 0x08;
  bool b = base & 0x08;
  if (required || w || r || x || b) {
    binary_push(text, 0x40 | (w << 3) | (r << 2) | (x << 1) | b);
  }
}

static void gen_prefix(Byte opcode) {
  binary_push(text, opcode);
}

static void gen_opcode(Byte opcode) {
  binary_push(text, opcode);
}

static void gen_opcode_reg(Byte opcode, Reg reg) {
  binary_push(text, opcode | (reg & 0x07));
}

typedef enum mod {
  MOD_DISP0 = 0,
  MOD_DISP8 = 1,
  MOD_DISP32 = 2,
  MOD_REG = 3,
} Mod;

static Mod mod_mem(int disp, Reg base) {
  bool bp = base == BP || base == R13;
  if (!bp && disp == 0) return MOD_DISP0;
  if (-128 <= disp && disp < 128) return MOD_DISP8;
  return MOD_DISP32;
}

static void gen_mod_rm(Mod mod, Reg reg, Reg rm) {
  binary_push(text, ((mod & 0x03) << 6) | ((reg & 0x07) << 3) | (rm & 0x07));
}

static void gen_sib(Scale scale, Reg index, Reg base) {
  binary_push(text, ((scale & 0x03) << 6) | ((index & 0x07) << 3) | (base & 0x07));
}

static void gen_disp(Mod mod, int disp) {
  if (mod == MOD_DISP8) {
    binary_push(text, (unsigned char) disp);
  } else if (mod == MOD_DISP32) {
    binary_push(text, ((unsigned int) disp >> 0) & 0xff);
    binary_push(text, ((unsigned int) disp >> 8) & 0xff);
    binary_push(text, ((unsigned int) disp >> 16) & 0xff);
    binary_push(text, ((unsigned int) disp >> 24) & 0xff);
  }
}

static void gen_ops(Reg reg, Op *rm) {
  if (rm->type == OP_REG) {
    gen_mod_rm(MOD_REG, reg, rm->regcode);
  } else if (rm->type == OP_MEM) {
    Mod mod = mod_mem(rm->disp, rm->base);
    if (!rm->sib) {
      gen_mod_rm(mod, reg, rm->base);
      if (rm->base == SP || rm->base == R12) {
        gen_sib(0, 4, rm->base);
      }
    } else {
      gen_mod_rm(mod, reg, 4);
      gen_sib(rm->scale, rm->index, rm->base);
    }
    gen_disp(mod, rm->disp);
  }
}

static void gen_imm8(unsigned char imm) {
  binary_push(text, (imm >> 0) & 0xff);
}

static void gen_imm16(unsigned short imm) {
  binary_push(text, (imm >> 0) & 0xff);
  binary_push(text, (imm >> 8) & 0xff);
}

static void gen_imm32(unsigned int imm) {
  binary_push(text, (imm >> 0) & 0xff);
  binary_push(text, (imm >> 8) & 0xff);
  binary_push(text, (imm >> 16) & 0xff);
  binary_push(text, (imm >> 24) & 0xff);
}

extern Symbol *undef_symbol();

static void gen_rel32(char *ident) {
  Symbol *symbol = map_lookup(symbols, ident);
  if (!symbol) {
    map_put(symbols, ident, undef_symbol());
  }
  vector_push(rels, rel_new(text->length, ident));

  gen_imm32(0);
}

static void gen_push(Inst *inst) {
  // 50 +rd
  gen_rex(0, 0, 0, inst->op->regcode, false);
  gen_opcode_reg(0x50, inst->op->regcode);
}

static void gen_pop(Inst *inst) {
  // 58 +rd
  gen_rex(0, 0, 0, inst->op->regcode, false);
  gen_opcode_reg(0x58, inst->op->regcode);
}

static void gen_mov(Inst *inst) {
  Op *src = inst->src, *dest = inst->dest;
  switch (inst->suffix) {
    case INST_QUAD: {
      if (src->type == OP_IMM && dest->type == OP_REG) {
        // REX.W + C7 /0 id
        gen_rex(1, 0, 0, dest->regcode, false);
        gen_opcode(0xc7);
        gen_ops(0, dest);
        gen_imm32(src->imm);
      } else if (src->type == OP_IMM && dest->type == OP_MEM) {
        // REX.W + C7 /0 id
        gen_rex(1, 0, dest->index, dest->base, false);
        gen_opcode(0xc7);
        gen_ops(0, dest);
        gen_imm32(src->imm);
      } else if (src->type == OP_REG && dest->type == OP_REG) {
        // REX.W + 89 /r
        gen_rex(1, src->regcode, 0, dest->regcode, false);
        gen_opcode(0x89);
        gen_ops(src->regcode, dest);
      } else if (src->type == OP_REG && dest->type == OP_MEM) {
        // REX.W + 89 /r
        gen_rex(1, src->regcode, dest->index, dest->base, false);
        gen_opcode(0x89);
        gen_ops(src->regcode, dest);
      } else if (src->type == OP_MEM && dest->type == OP_REG) {
        // REX.W + 8B /r
        gen_rex(1, dest->regcode, src->index, src->base, false);
        gen_opcode(0x8b);
        gen_ops(dest->regcode, src);
      }
    }
    break;

    case INST_LONG: {
      if (src->type == OP_IMM && dest->type == OP_REG) {
        // C7 /0 id
        gen_rex(0, 0, 0, dest->regcode, false);
        gen_opcode(0xc7);
        gen_ops(0, dest);
        gen_imm32(src->imm);
      } else if (src->type == OP_IMM && dest->type == OP_MEM) {
        // C7 /0 id
        gen_rex(0, 0, dest->index, dest->base, false);
        gen_opcode(0xc7);
        gen_ops(0, dest);
        gen_imm32(src->imm);
      } else if (src->type == OP_REG && dest->type == OP_REG) {
        // 89 /r
        gen_rex(0, src->regcode, 0, dest->regcode, false);
        gen_opcode(0x89);
        gen_ops(src->regcode, dest);
      } else if (src->type == OP_REG && dest->type == OP_MEM) {
        // 89 /r
        gen_rex(0, src->regcode, dest->index, dest->base, false);
        gen_opcode(0x89);
        gen_ops(src->regcode, dest);
      } else if (src->type == OP_MEM && dest->type == OP_REG) {
        // 8B /r
        gen_rex(0, dest->regcode, src->index, src->base, false);
        gen_opcode(0x8b);
        gen_ops(dest->regcode, src);
      }
    }
    break;

    case INST_WORD: {
      if (src->type == OP_IMM && dest->type == OP_REG) {
        // C7 /0 id
        gen_prefix(0x66);
        gen_rex(0, 0, 0, dest->regcode, false);
        gen_opcode(0xc7);
        gen_ops(0, dest);
        gen_imm16(src->imm);
      } else if (src->type == OP_IMM && dest->type == OP_MEM) {
        // C7 /0 id
        gen_prefix(0x66);
        gen_rex(0, 0, dest->index, dest->base, false);
        gen_opcode(0xc7);
        gen_ops(0, dest);
        gen_imm16(src->imm);
      } else if (src->type == OP_REG && dest->type == OP_REG) {
        // 89 /r
        gen_prefix(0x66);
        gen_rex(0, src->regcode, 0, dest->regcode, false);
        gen_opcode(0x89);
        gen_ops(src->regcode, dest);
      } else if (src->type == OP_REG && dest->type == OP_MEM) {
        // 89 /r
        gen_prefix(0x66);
        gen_rex(0, src->regcode, dest->index, dest->base, false);
        gen_opcode(0x89);
        gen_ops(src->regcode, dest);
      } else if (src->type == OP_MEM && dest->type == OP_REG) {
        // 8B /r
        gen_prefix(0x66);
        gen_rex(0, dest->regcode, src->index, src->base, false);
        gen_opcode(0x8b);
        gen_ops(dest->regcode, src);
      }
    }
    break;

    case INST_BYTE: {
      if (src->type == OP_IMM && dest->type == OP_REG) {
        // C6 /0 id
        bool required = (dest->regcode & 12) == 4;
        gen_rex(0, 0, 0, dest->regcode, required);
        gen_opcode(0xc6);
        gen_ops(0, dest);
        gen_imm8(src->imm);
      } else if (src->type == OP_IMM && dest->type == OP_MEM) {
        // C6 /0 id
        gen_rex(0, 0, dest->index, dest->base, 0);
        gen_opcode(0xc6);
        gen_ops(0, dest);
        gen_imm8(src->imm);
      } else if (src->type == OP_REG && dest->type == OP_REG) {
        // 88 /r
        bool required = (src->regcode & 12) == 4 || (dest->regcode & 12) == 4;
        gen_rex(0, src->regcode, 0, dest->regcode, required);
        gen_opcode(0x88);
        gen_ops(src->regcode, dest);
      } else if (src->type == OP_REG && dest->type == OP_MEM) {
        // 88 /r
        bool required = (src->regcode & 12) == 4;
        gen_rex(0, src->regcode, dest->index, dest->base, required);
        gen_opcode(0x88);
        gen_ops(src->regcode, dest);
      } else if (src->type == OP_MEM && dest->type == OP_REG) {
        // 8A /r
        bool required = (dest->regcode & 12) == 4;
        gen_rex(0, dest->regcode, src->index, src->base, required);
        gen_opcode(0x8a);
        gen_ops(dest->regcode, src);
      }
    }
    break;
  }
}

static void gen_lea(Inst *inst) {
  // REX.W + 8D /r
  gen_rex(1, inst->dest->regcode, inst->src->index, inst->src->base, false);
  gen_opcode(0x8d);
  gen_ops(inst->dest->regcode, inst->src);
}

static void gen_neg(Inst *inst) {
  Op *op = inst->op;
  if (inst->suffix == INST_QUAD) {
    if (op->type == OP_REG) {
      // REX.W + F7 /3 id
      gen_rex(1, 0, 0, op->regcode, false);
      gen_opcode(0xf7);
      gen_ops(3, op);
    } else if (op->type == OP_MEM) {
      // REX.W + F7 /3 id
      gen_rex(1, 0, op->index, op->base, false);
      gen_opcode(0xf7);
      gen_ops(3, op);
    }
  } else if (inst->suffix == INST_LONG) {
    if (op->type == OP_REG) {
      // F7 /3 id
      gen_rex(0, 0, 0, op->regcode, false);
      gen_opcode(0xf7);
      gen_ops(3, op);
    } else if (op->type == OP_MEM) {
      // F7 /3 id
      gen_rex(0, 0, op->index, op->base, false);
      gen_opcode(0xf7);
      gen_ops(3, op);
    }
  }
}

static void gen_not(Inst *inst) {
  Op *op = inst->op;
  if (inst->suffix == INST_QUAD) {
    if (op->type == OP_REG) {
      // REX.W + F7 /2 id
      gen_rex(1, 0, 0, op->regcode, false);
      gen_opcode(0xf7);
      gen_ops(2, op);
    } else if (op->type == OP_MEM) {
      // REX.W + F7 /2 id
      gen_rex(1, 0, op->index, op->base, false);
      gen_opcode(0xf7);
      gen_ops(2, op);
    }
  } else if (inst->suffix == INST_LONG) {
    if (op->type == OP_REG) {
      // F7 /2 id
      gen_rex(0, 0, 0, op->regcode, false);
      gen_opcode(0xf7);
      gen_ops(2, op);
    } else if (op->type == OP_MEM) {
      // F7 /2 id
      gen_rex(0, 0, op->index, op->base, false);
      gen_opcode(0xf7);
      gen_ops(2, op);
    }
  }
}

static void gen_add(Inst *inst) {
  Op *src = inst->src, *dest = inst->dest;
  if (inst->suffix == INST_QUAD) {
    if (src->type == OP_IMM && dest->type == OP_REG) {
      // REX.W + 81 /0 id
      gen_rex(1, 0, 0, dest->regcode, false);
      gen_opcode(0x81);
      gen_ops(0, dest);
      gen_imm32(src->imm);
    } else if (src->type == OP_IMM && dest->type == OP_MEM) {
      // REX.W + 81 /0 id
      gen_rex(1, 0, dest->index, dest->base, false);
      gen_opcode(0x81);
      gen_ops(0, dest);
      gen_imm32(src->imm);
    } else if (src->type == OP_REG && dest->type == OP_REG) {
      // REX.W + 01 /r
      gen_rex(1, src->regcode, 0, dest->regcode, false);
      gen_opcode(0x01);
      gen_ops(src->regcode, dest);
    } else if (src->type == OP_REG && dest->type == OP_MEM) {
      // REX.W + 01 /r
      gen_rex(1, src->regcode, dest->index, dest->base, false);
      gen_opcode(0x01);
      gen_ops(src->regcode, dest);
    } else if (src->type == OP_MEM && dest->type == OP_REG) {
      // REX.W + 03 /r
      gen_rex(1, dest->regcode, src->index, src->base, false);
      gen_opcode(0x03);
      gen_ops(dest->regcode, src);
    }
  } else if (inst->suffix == INST_LONG) {
    if (src->type == OP_IMM && dest->type == OP_REG) {
      // 81 /0 id
      gen_rex(0, 0, 0, dest->regcode, false);
      gen_opcode(0x81);
      gen_ops(0, dest);
      gen_imm32(src->imm);
    } else if (src->type == OP_IMM && dest->type == OP_MEM) {
      // 81 /0 id
      gen_rex(0, 0, dest->index, dest->base, false);
      gen_opcode(0x81);
      gen_ops(0, dest);
      gen_imm32(src->imm);
    } else if (src->type == OP_REG && dest->type == OP_REG) {
      // 01 /r
      gen_rex(0, src->regcode, 0, dest->regcode, false);
      gen_opcode(0x01);
      gen_ops(src->regcode, dest);
    } else if (src->type == OP_REG && dest->type == OP_MEM) {
      // 01 /r
      gen_rex(0, src->regcode, dest->index, dest->base, false);
      gen_opcode(0x01);
      gen_ops(src->regcode, dest);
    } else if (src->type == OP_MEM && dest->type == OP_REG) {
      // 03 /r
      gen_rex(0, dest->regcode, src->index, src->base, false);
      gen_opcode(0x03);
      gen_ops(dest->regcode, src);
    }
  }
}

static void gen_sub(Inst *inst) {
  Op *src = inst->src, *dest = inst->dest;
  if (inst->suffix == INST_QUAD) {
    if (src->type == OP_IMM && dest->type == OP_REG) {
      // REX.W + 81 /5 id
      gen_rex(1, 0, 0, dest->regcode, false);
      gen_opcode(0x81);
      gen_ops(5, dest);
      gen_imm32(src->imm);
    } else if (src->type == OP_IMM && dest->type == OP_MEM) {
      // REX.W + 81 /5 id
      gen_rex(1, 0, dest->index, dest->base, false);
      gen_opcode(0x81);
      gen_ops(5, dest);
      gen_imm32(src->imm);
    } else if (src->type == OP_REG && dest->type == OP_REG) {
      // REX.W + 29 /r
      gen_rex(1, src->regcode, 0, dest->regcode, false);
      gen_opcode(0x29);
      gen_ops(src->regcode, dest);
    } else if (src->type == OP_REG && dest->type == OP_MEM) {
      // REX.W + 29 /r
      gen_rex(1, src->regcode, dest->index, dest->base, false);
      gen_opcode(0x29);
      gen_ops(src->regcode, dest);
    } else if (src->type == OP_MEM && dest->type == OP_REG) {
      // REX.W + 2B /r
      gen_rex(1, dest->regcode, src->index, src->base, false);
      gen_opcode(0x2b);
      gen_ops(dest->regcode, src);
    }
  } else if (inst->suffix == INST_LONG) {
    if (src->type == OP_IMM && dest->type == OP_REG) {
      // 81 /5 id
      gen_rex(0, 0, 0, dest->regcode, false);
      gen_opcode(0x81);
      gen_ops(5, dest);
      gen_imm32(src->imm);
    } else if (src->type == OP_IMM && dest->type == OP_MEM) {
      // 81 /5 id
      gen_rex(0, 0, dest->index, dest->base, false);
      gen_opcode(0x81);
      gen_ops(5, dest);
      gen_imm32(src->imm);
    } else if (src->type == OP_REG && dest->type == OP_REG) {
      // 29 /r
      gen_rex(0, src->regcode, 0, dest->regcode, false);
      gen_opcode(0x29);
      gen_ops(src->regcode, dest);
    } else if (src->type == OP_REG && dest->type == OP_MEM) {
      // 29 /r
      gen_rex(0, src->regcode, dest->index, dest->base, false);
      gen_opcode(0x29);
      gen_ops(src->regcode, dest);
    } else if (src->type == OP_MEM && dest->type == OP_REG) {
      // 2B /r
      gen_rex(0, dest->regcode, src->index, src->base, false);
      gen_opcode(0x2b);
      gen_ops(dest->regcode, src);
    }
  }
}

static void gen_mul(Inst *inst) {
  Op *op = inst->op;
  if (inst->suffix == INST_QUAD) {
    if (op->type == OP_REG) {
      // REX.W + F7 /4 id
      gen_rex(1, 0, 0, op->regcode, false);
      gen_opcode(0xf7);
      gen_ops(4, op);
    } else if (op->type == OP_MEM) {
      // REX.W + F7 /4 id
      gen_rex(1, 0, op->index, op->base, false);
      gen_opcode(0xf7);
      gen_ops(4, op);
    }
  } else if (inst->suffix == INST_LONG) {
    if (op->type == OP_REG) {
      // F7 /4 id
      gen_rex(0, 0, 0, op->regcode, false);
      gen_opcode(0xf7);
      gen_ops(4, op);
    } else if (op->type == OP_MEM) {
      // F7 /4 id
      gen_rex(0, 0, op->index, op->base, false);
      gen_opcode(0xf7);
      gen_ops(4, op);
    }
  }
}

static void gen_imul(Inst *inst) {
  Op *op = inst->op;
  if (inst->suffix == INST_QUAD) {
    if (op->type == OP_REG) {
      // REX.W + F7 /5 id
      gen_rex(1, 0, 0, op->regcode, false);
      gen_opcode(0xf7);
      gen_ops(5, op);
    } else if (op->type == OP_MEM) {
      // REX.W + F7 /5 id
      gen_rex(1, 0, op->index, op->base, false);
      gen_opcode(0xf7);
      gen_ops(5, op);
    }
  } else if (inst->suffix == INST_LONG) {
    if (op->type == OP_REG) {
      // F7 /5 id
      gen_rex(0, 0, 0, op->regcode, false);
      gen_opcode(0xf7);
      gen_ops(5, op);
    } else if (op->type == OP_MEM) {
      // F7 /5 id
      gen_rex(0, 0, op->index, op->base, false);
      gen_opcode(0xf7);
      gen_ops(5, op);
    }
  }
}

static void gen_div(Inst *inst) {
  Op *op = inst->op;
  if (inst->suffix == INST_QUAD) {
    if (op->type == OP_REG) {
      // REX.W + F7 /6 id
      gen_rex(1, 0, 0, op->regcode, false);
      gen_opcode(0xf7);
      gen_ops(6, op);
    } else if (op->type == OP_MEM) {
      // REX.W + F7 /6 id
      gen_rex(1, 0, op->index, op->base, false);
      gen_opcode(0xf7);
      gen_ops(6, op);
    }
  } else if (inst->suffix == INST_LONG) {
    if (op->type == OP_REG) {
      // F7 /6 id
      gen_rex(0, 0, 0, op->regcode, false);
      gen_opcode(0xf7);
      gen_ops(6, op);
    } else if (op->type == OP_MEM) {
      // F7 /6 id
      gen_rex(0, 0, op->index, op->base, false);
      gen_opcode(0xf7);
      gen_ops(6, op);
    }
  }
}

static void gen_idiv(Inst *inst) {
  Op *op = inst->op;
  if (inst->suffix == INST_QUAD) {
    if (op->type == OP_REG) {
      // REX.W + F7 /7 id
      gen_rex(1, 0, 0, op->regcode, false);
      gen_opcode(0xf7);
      gen_ops(7, op);
    } else if (op->type == OP_MEM) {
      // REX.W + F7 /7 id
      gen_rex(1, 0, op->index, op->base, false);
      gen_opcode(0xf7);
      gen_ops(7, op);
    }
  } else if (inst->suffix == INST_LONG) {
    if (op->type == OP_REG) {
      // F7 /7 id
      gen_rex(0, 0, 0, op->regcode, false);
      gen_opcode(0xf7);
      gen_ops(7, op);
    } else if (op->type == OP_MEM) {
      // F7 /7 id
      gen_rex(0, 0, op->index, op->base, false);
      gen_opcode(0xf7);
      gen_ops(7, op);
    }
  }
}

static void gen_cmp(Inst *inst) {
  Op *src = inst->src, *dest = inst->dest;
  switch (inst->suffix) {
    case INST_QUAD: {
      if (src->type == OP_IMM && dest->type == OP_REG) {
        // REX.W + 81 /7 id
        gen_rex(1, 0, 0, dest->regcode, false);
        gen_opcode(0x81);
        gen_ops(7, dest);
        gen_imm32(src->imm);
      } else if (src->type == OP_IMM && dest->type == OP_MEM) {
        // REX.W + 81 /7 id
        gen_rex(1, 0, dest->index, dest->base, false);
        gen_opcode(0x81);
        gen_ops(7, dest);
        gen_imm32(src->imm);
      } else if (src->type == OP_REG && dest->type == OP_REG) {
        // REX.W + 39 /r
        gen_rex(1, src->regcode, 0, dest->regcode, false);
        gen_opcode(0x39);
        gen_ops(src->regcode, dest);
      } else if (src->type == OP_REG && dest->type == OP_MEM) {
        // REX.W + 39 /r
        gen_rex(1, src->regcode, dest->index, dest->base, false);
        gen_opcode(0x39);
        gen_ops(src->regcode, dest);
      } else if (src->type == OP_MEM && dest->type == OP_REG) {
        // REX.W + 3B /r
        gen_rex(1, dest->regcode, src->index, src->base, false);
        gen_opcode(0x3b);
        gen_ops(dest->regcode, src);
      }
    }
    break;

    case INST_LONG: {
      if (src->type == OP_IMM && dest->type == OP_REG) {
        // 81 /7 id
        gen_rex(0, 0, 0, dest->regcode, false);
        gen_opcode(0x81);
        gen_ops(7, dest);
        gen_imm32(src->imm);
      } else if (src->type == OP_IMM && dest->type == OP_MEM) {
        // 81 /0 id
        gen_rex(0, 0, dest->index, dest->base, false);
        gen_opcode(0x81);
        gen_ops(7, dest);
        gen_imm32(src->imm);
      } else if (src->type == OP_REG && dest->type == OP_REG) {
        // 39 /r
        gen_rex(0, src->regcode, 0, dest->regcode, false);
        gen_opcode(0x39);
        gen_ops(src->regcode, dest);
      } else if (src->type == OP_REG && dest->type == OP_MEM) {
        // 39 /r
        gen_rex(0, src->regcode, dest->index, dest->base, false);
        gen_opcode(0x39);
        gen_ops(src->regcode, dest);
      } else if (src->type == OP_MEM && dest->type == OP_REG) {
        // 3B /r
        gen_rex(0, dest->regcode, src->index, src->base, false);
        gen_opcode(0x3b);
        gen_ops(dest->regcode, src);
      }
    }
    break;

    case INST_WORD: {
      if (src->type == OP_IMM && dest->type == OP_REG) {
        // 81 /7 id
        gen_prefix(0x66);
        gen_rex(0, 0, 0, dest->regcode, false);
        gen_opcode(0x81);
        gen_ops(7, dest);
        gen_imm16(src->imm);
      } else if (src->type == OP_IMM && dest->type == OP_MEM) {
        // 81 /7 id
        gen_prefix(0x66);
        gen_rex(0, 0, dest->index, dest->base, false);
        gen_opcode(0x81);
        gen_ops(7, dest);
        gen_imm16(src->imm);
      } else if (src->type == OP_REG && dest->type == OP_REG) {
        // 39 /r
        gen_prefix(0x66);
        gen_rex(0, src->regcode, 0, dest->regcode, false);
        gen_opcode(0x39);
        gen_ops(src->regcode, dest);
      } else if (src->type == OP_REG && dest->type == OP_MEM) {
        // 39 /r
        gen_prefix(0x66);
        gen_rex(0, src->regcode, dest->index, dest->base, false);
        gen_opcode(0x39);
        gen_ops(src->regcode, dest);
      } else if (src->type == OP_MEM && dest->type == OP_REG) {
        // 3B /r
        gen_prefix(0x66);
        gen_rex(0, dest->regcode, src->index, src->base, false);
        gen_opcode(0x3b);
        gen_ops(dest->regcode, src);
      }
    }
    break;

    case INST_BYTE: {
      if (src->type == OP_IMM && dest->type == OP_REG) {
        // 80 /7 id
        bool required = (dest->regcode & 12) == 4;
        gen_rex(0, 0, 0, dest->regcode, required);
        gen_opcode(0x80);
        gen_ops(7, dest);
        gen_imm8(src->imm);
      } else if (src->type == OP_IMM && dest->type == OP_MEM) {
        // 80 /0 id
        gen_rex(0, 0, dest->index, dest->base, 0);
        gen_opcode(0x80);
        gen_ops(7, dest);
        gen_imm8(src->imm);
      } else if (src->type == OP_REG && dest->type == OP_REG) {
        // 38 /r
        bool required = (src->regcode & 12) == 4 || (dest->regcode & 12) == 4;
        gen_rex(0, src->regcode, 0, dest->regcode, required);
        gen_opcode(0x38);
        gen_ops(src->regcode, dest);
      } else if (src->type == OP_REG && dest->type == OP_MEM) {
        // 38 /r
        bool required = (src->regcode & 12) == 4;
        gen_rex(0, src->regcode, dest->index, dest->base, required);
        gen_opcode(0x38);
        gen_ops(src->regcode, dest);
      } else if (src->type == OP_MEM && dest->type == OP_REG) {
        // 3A /r
        bool required = (dest->regcode & 12) == 4;
        gen_rex(0, dest->regcode, src->index, src->base, required);
        gen_opcode(0x3a);
        gen_ops(dest->regcode, src);
      }
    }
    break;
  }
}

static void gen_sete(Inst *inst) {
  Op *op = inst->op;
  if (op->type == OP_REG) {
    // 0F 94
    bool required = (op->regcode & 12) == 4;
    gen_rex(0, 0, 0, op->regcode, required);
    gen_opcode(0x0f);
    gen_opcode(0x94);
    gen_ops(0, op); // reg field is not used.
  } else if (op->type == OP_MEM) {
    // 0F 94
    gen_rex(0, 0, op->index, op->base, 0);
    gen_opcode(0x0f);
    gen_opcode(0x94);
    gen_ops(0, op); // reg field is not used.
  }
}

static void gen_setne(Inst *inst) {
  Op *op = inst->op;
  if (op->type == OP_REG) {
    // 0F 95
    bool required = (op->regcode & 12) == 4;
    gen_rex(0, 0, 0, op->regcode, required);
    gen_opcode(0x0f);
    gen_opcode(0x95);
    gen_ops(0, op); // reg field is not used.
  } else if (op->type == OP_MEM) {
    // 0F 95
    gen_rex(0, 0, op->index, op->base, 0);
    gen_opcode(0x0f);
    gen_opcode(0x95);
    gen_ops(0, op); // reg field is not used.
  }
}

static void gen_setl(Inst *inst) {
  Op *op = inst->op;
  if (op->type == OP_REG) {
    // 0F 9C
    bool required = (op->regcode & 12) == 4;
    gen_rex(0, 0, 0, op->regcode, required);
    gen_opcode(0x0f);
    gen_opcode(0x9c);
    gen_ops(0, op); // reg field is not used.
  } else if (op->type == OP_MEM) {
    // 0F 9C
    gen_rex(0, 0, op->index, op->base, 0);
    gen_opcode(0x0f);
    gen_opcode(0x9c);
    gen_ops(0, op); // reg field is not used.
  }
}

static void gen_setg(Inst *inst) {
  Op *op = inst->op;
  if (op->type == OP_REG) {
    // 0F 9F
    bool required = (op->regcode & 12) == 4;
    gen_rex(0, 0, 0, op->regcode, required);
    gen_opcode(0x0f);
    gen_opcode(0x9f);
    gen_ops(0, op); // reg field is not used.
  } else if (op->type == OP_MEM) {
    // 0F 9F
    gen_rex(0, 0, op->index, op->base, 0);
    gen_opcode(0x0f);
    gen_opcode(0x9f);
    gen_ops(0, op); // reg field is not used.
  }
}

static void gen_setle(Inst *inst) {
  Op *op = inst->op;
  if (op->type == OP_REG) {
    // 0F 9E
    bool required = (op->regcode & 12) == 4;
    gen_rex(0, 0, 0, op->regcode, required);
    gen_opcode(0x0f);
    gen_opcode(0x9e);
    gen_ops(0, op); // reg field is not used.
  } else if (op->type == OP_MEM) {
    // 0F 9E
    gen_rex(0, 0, op->index, op->base, 0);
    gen_opcode(0x0f);
    gen_opcode(0x9e);
    gen_ops(0, op); // reg field is not used.
  }
}

static void gen_setge(Inst *inst) {
  Op *op = inst->op;
  if (op->type == OP_REG) {
    // 0F 9D
    bool required = (op->regcode & 12) == 4;
    gen_rex(0, 0, 0, op->regcode, required);
    gen_opcode(0x0f);
    gen_opcode(0x9d);
    gen_ops(0, op); // reg field is not used.
  } else if (op->type == OP_MEM) {
    // 0F 9D
    gen_rex(0, 0, op->index, op->base, 0);
    gen_opcode(0x0f);
    gen_opcode(0x9d);
    gen_ops(0, op); // reg field is not used.
  }
}

static void gen_jmp(Inst *inst) {
  // E9 cd
  gen_opcode(0xe9);
  gen_rel32(inst->op->ident);
}

static void gen_je(Inst *inst) {
  // E9 cd
  gen_opcode(0x0f);
  gen_opcode(0x84);
  gen_rel32(inst->op->ident);
}

static void gen_jne(Inst *inst) {
  // E9 cd
  gen_opcode(0x0f);
  gen_opcode(0x85);
  gen_rel32(inst->op->ident);
}

static void gen_call(Inst *inst) {
  // E8 cd
  gen_opcode(0xe8);
  gen_rel32(inst->op->ident);
}

static void gen_leave(Inst *inst) {
  // C9
  gen_opcode(0xc9);
}

static void gen_ret(Inst *inst) {
  // C3
  gen_opcode(0xc3);
}

static void gen_text() {
  for (int i = 0; i < insts->length; i++) {
    Inst *inst = insts->array[i];
    offsets[i] = text->length;

    switch (inst->type) {
      case INST_PUSH:
        gen_push(inst);
        break;
      case INST_POP:
        gen_pop(inst);
        break;
      case INST_MOV:
        gen_mov(inst);
        break;
      case INST_LEA:
        gen_lea(inst);
        break;
      case INST_NEG:
        gen_neg(inst);
        break;
      case INST_NOT:
        gen_not(inst);
        break;
      case INST_ADD:
        gen_add(inst);
        break;
      case INST_SUB:
        gen_sub(inst);
        break;
      case INST_MUL:
        gen_mul(inst);
        break;
      case INST_IMUL:
        gen_imul(inst);
        break;
      case INST_DIV:
        gen_div(inst);
        break;
      case INST_IDIV:
        gen_idiv(inst);
        break;
      case INST_CMP:
        gen_cmp(inst);
        break;
      case INST_SETE:
        gen_sete(inst);
        break;
      case INST_SETNE:
        gen_setne(inst);
        break;
      case INST_SETL:
        gen_setl(inst);
        break;
      case INST_SETG:
        gen_setg(inst);
        break;
      case INST_SETLE:
        gen_setle(inst);
        break;
      case INST_SETGE:
        gen_setge(inst);
        break;
      case INST_JMP:
        gen_jmp(inst);
        break;
      case INST_JE:
        gen_je(inst);
        break;
      case INST_JNE:
        gen_jne(inst);
        break;
      case INST_CALL:
        gen_call(inst);
        break;
      case INST_LEAVE:
        gen_leave(inst);
        break;
      case INST_RET:
        gen_ret(inst);
        break;
    }
  }
}

static void gen_symtab() {
  binary_write(symtab, calloc(1, sizeof(Elf64_Sym)), sizeof(Elf64_Sym));
  string_push(strtab, '\0');

  for (int i = 0; i < symbols->count; i++) {
    char *ident = symbols->keys[i];
    Symbol *symbol = symbols->values[i];

    if (symbol->global) {
      Elf64_Sym *sym = (Elf64_Sym *) calloc(1, sizeof(Elf64_Sym));
      sym->st_name = strtab->length;
      sym->st_info = ELF64_ST_INFO(STB_GLOBAL, STT_NOTYPE);
      sym->st_other = STV_DEFAULT;
      if (!symbol->undef) {
        sym->st_shndx = 1;
        sym->st_value = offsets[symbol->inst];
      } else {
        sym->st_shndx = SHN_UNDEF;
      }

      binary_write(symtab, sym, sizeof(Elf64_Sym));
      string_write(strtab, ident);
      string_push(strtab, '\0');
      map_put(gsyms, ident, (void *) (intptr_t) (gsyms->count + 1));
    }
  }
}

static void gen_rela_text() {
  for (int i = 0; i < rels->length; i++) {
    Rel *rel = rels->array[i];
    Symbol *symbol = map_lookup(symbols, rel->ident);

    if (symbol->global) {
      int index = (int) (intptr_t) map_lookup(gsyms, rel->ident);

      Elf64_Rela *rela = (Elf64_Rela *) calloc(1, sizeof(Elf64_Rela));
      rela->r_offset = rel->offset;
      rela->r_info = ELF64_R_INFO(index, R_X86_64_PC32);
      rela->r_addend = -4;

      binary_write(rela_text, rela, sizeof(Elf64_Rela));
    } else {
      int *rel32 = (int *) &text->buffer[rel->offset];
      *rel32 = offsets[symbol->inst] - (rel->offset + 4);
    }
  }
}

Section *gen(Unit *unit) {
  insts = unit->insts;
  symbols = unit->symbols;

  text = binary_new();
  symtab = binary_new();
  strtab = string_new();
  rela_text = binary_new();

  offsets = (int *) calloc(insts->length, sizeof(int));
  rels = vector_new();
  gsyms = map_new();

  gen_text();
  gen_symtab();
  gen_rela_text();

  Section *section = (Section *) calloc(1, sizeof(Section));
  section->text = text;
  section->symtab = symtab;
  section->strtab = strtab;
  section->rela_text = rela_text;

  return section;
}
