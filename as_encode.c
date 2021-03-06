#include "as.h"

static Reloc *reloc_new(int offset, char *ident, int type, int addend) {
  Reloc *reloc = (Reloc *) calloc(1, sizeof(Reloc));
  reloc->offset = offset;
  reloc->ident = ident;
  reloc->type = type;
  reloc->addend = addend;
  return reloc;
}

static Section *section_new(void) {
  Section *section = (Section *) calloc(1, sizeof(Section));
  section->bin = binary_new();
  section->relocs = vector_new();
  return section;
}

static Symbol *symbol_new(bool global, int section, int offset) {
  Symbol *symbol = (Symbol *) calloc(1, sizeof(Symbol));
  symbol->global = global;
  symbol->section = section;
  symbol->offset = offset;
  return symbol;
}

static TransUnit *trans_unit_new(void) {
  TransUnit *trans_unit = (TransUnit *) calloc(1, sizeof(TransUnit));
  trans_unit->text = section_new();
  trans_unit->data = section_new();
  trans_unit->rodata = section_new();
  trans_unit->symbols = map_new();
  return trans_unit;
}

static TransUnit *trans_unit;
static Binary *bin;
static Vector *relocs;
static Map *symbols;
static int current;

// encode label

static void gen_label(Label *label) {
  Symbol *symbol = map_lookup(symbols, label->ident);
  if (!symbol) {
    map_put(symbols, label->ident, symbol_new(false, current, bin->length));
  } else if (symbol->section == 0) {
    symbol->section = current;
    symbol->offset = bin->length;
  } else {
    ERROR(label->token, "duplicated symbol declaration: %s.", label->ident);
  }
}

// encode directives

static void gen_text(Dir *dir) {
  bin = trans_unit->text->bin;
  relocs = trans_unit->text->relocs;
  current = TEXT;
}

static void gen_data(Dir *dir) {
  bin = trans_unit->data->bin;
  relocs = trans_unit->data->relocs;
  current = DATA;
}

static void gen_section(Dir *dir) {
  bin = trans_unit->rodata->bin;
  relocs = trans_unit->rodata->relocs;
  current = RODATA;
}

static void gen_global(Dir *dir) {
  Symbol *symbol = map_lookup(symbols, dir->ident);
  if (!symbol) {
    map_put(symbols, dir->ident, symbol_new(true, UNDEF, 0));
  } else {
    symbol->global = true;
  }
}

static void gen_zero(Dir *dir) {
  for (int i = 0; i < dir->num; i++) {
    binary_push(bin, 0);
  }
}

static void gen_long(Dir *dir) {
  binary_push(bin, (((unsigned int) dir->num) >> 0) & 0xff);
  binary_push(bin, (((unsigned int) dir->num) >> 8) & 0xff);
  binary_push(bin, (((unsigned int) dir->num) >> 16) & 0xff);
  binary_push(bin, (((unsigned int) dir->num) >> 24) & 0xff);
}

static void gen_quad(Dir *dir) {
  vector_push(relocs, reloc_new(bin->length, dir->ident, R_X86_64_64, 0));
  for (int i = 0; i < 8; i++) {
    binary_push(bin, 0);
  }
}

static void gen_ascii(Dir *dir) {
  for (int i = 0; i < dir->string->length; i++) {
    binary_push(bin, dir->string->buffer[i]);
  }
}

// encode instructions

static void gen_rex(bool w, RegCode reg, RegCode index, RegCode base, bool required) {
  bool r = reg & 0x08;
  bool x = index & 0x08;
  bool b = base & 0x08;
  if (required || w || r || x || b) {
    binary_push(bin, 0x40 | (w << 3) | (r << 2) | (x << 1) | b);
  }
}

static void gen_prefix(Byte opcode) {
  binary_push(bin, opcode);
}

static void gen_opcode(Byte opcode) {
  binary_push(bin, opcode);
}

static void gen_opcode_reg(Byte opcode, RegCode reg) {
  binary_push(bin, opcode | (reg & 0x07));
}

typedef enum mod {
  MOD_DISP0 = 0,
  MOD_DISP8 = 1,
  MOD_DISP32 = 2,
  MOD_REG = 3,
} Mod;

static Mod mod_mem(int disp, RegCode base) {
  bool bp = base == REG_BP || base == REG_R13;
  if (!bp && disp == 0) return MOD_DISP0;
  if (-128 <= disp && disp < 128) return MOD_DISP8;
  return MOD_DISP32;
}

static void gen_mod_rm(Mod mod, RegCode reg, RegCode rm) {
  binary_push(bin, ((mod & 0x03) << 6) | ((reg & 0x07) << 3) | (rm & 0x07));
}

static void gen_sib(Scale scale, RegCode index, RegCode base) {
  binary_push(bin, ((scale & 0x03) << 6) | ((index & 0x07) << 3) | (base & 0x07));
}

static void gen_disp(Mod mod, int disp) {
  if (mod == MOD_DISP8) {
    binary_push(bin, (unsigned char) disp);
  } else if (mod == MOD_DISP32) {
    binary_push(bin, ((unsigned int) disp >> 0) & 0xff);
    binary_push(bin, ((unsigned int) disp >> 8) & 0xff);
    binary_push(bin, ((unsigned int) disp >> 16) & 0xff);
    binary_push(bin, ((unsigned int) disp >> 24) & 0xff);
  }
}

static void gen_imm8(unsigned char imm) {
  binary_push(bin, (imm >> 0) & 0xff);
}

static void gen_imm16(unsigned short imm) {
  binary_push(bin, (imm >> 0) & 0xff);
  binary_push(bin, (imm >> 8) & 0xff);
}

static void gen_imm32(unsigned int imm) {
  binary_push(bin, (imm >> 0) & 0xff);
  binary_push(bin, (imm >> 8) & 0xff);
  binary_push(bin, (imm >> 16) & 0xff);
  binary_push(bin, (imm >> 24) & 0xff);
}

static void gen_rel32(char *ident) {
  Symbol *symbol = map_lookup(symbols, ident);
  if (!symbol) {
    map_put(symbols, ident, symbol_new(false, UNDEF, 0));
  }
  vector_push(relocs, reloc_new(bin->length, ident, R_X86_64_PC32, -4));

  gen_imm32(0);
}

static void gen_ops(RegCode reg, Op *rm) {
  if (rm->type == OP_REG) {
    gen_mod_rm(MOD_REG, reg, rm->regcode);
  } else if (rm->type == OP_MEM) {
    if (!rm->rip) {
      Mod mod = mod_mem(rm->disp, rm->base);
      if (!rm->sib) {
        gen_mod_rm(mod, reg, rm->base);
        if (rm->base == REG_SP || rm->base == REG_R12) {
          gen_sib(0, 4, rm->base);
        }
      } else {
        gen_mod_rm(mod, reg, 4);
        gen_sib(rm->scale, rm->index, rm->base);
      }
      gen_disp(mod, rm->disp);
    } else {
      gen_mod_rm(0, reg, 5);
      gen_rel32(rm->ident);
    }
  }
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

static void gen_cltd(Inst *inst) {
  // 99
  gen_opcode(0x99);
}

static void gen_cqto(Inst *inst) {
  // REX.W + 99
  gen_rex(1, 0, 0, 0, false);
  gen_opcode(0x99);
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

static void gen_movzb(Inst *inst) {
  Op *src = inst->src, *dest = inst->dest;
  if (inst->suffix == INST_QUAD) {
    if (src->type == OP_REG && dest->type == OP_REG) {
      // REX.W + 0F B6 /r
      gen_rex(1, dest->regcode, 0, src->regcode, false);
      gen_opcode(0x0f);
      gen_opcode(0xb6);
      gen_ops(dest->regcode, src);
    } else if (src->type == OP_MEM && dest->type == OP_REG) {
      // REX.W + 0F B6 /r
      gen_rex(1, dest->regcode, src->index, src->base, false);
      gen_opcode(0x0f);
      gen_opcode(0xb6);
      gen_ops(dest->regcode, src);
    }
  } else if (inst->suffix == INST_LONG) {
    if (src->type == OP_REG && dest->type == OP_REG) {
      // 0F B6 /r
      gen_rex(0, dest->regcode, 0, src->regcode, false);
      gen_opcode(0x0f);
      gen_opcode(0xb6);
      gen_ops(dest->regcode, src);
    } else if (src->type == OP_MEM && dest->type == OP_REG) {
      // 0F B6 /r
      gen_rex(0, dest->regcode, src->index, src->base, false);
      gen_opcode(0x0f);
      gen_opcode(0xb6);
      gen_ops(dest->regcode, src);
    }
  } else if (inst->suffix == INST_WORD) {
    if (src->type == OP_REG && dest->type == OP_REG) {
      // 0F B6 /r
      gen_prefix(0x66);
      gen_rex(0, dest->regcode, 0, src->regcode, false);
      gen_opcode(0x0f);
      gen_opcode(0xb6);
      gen_ops(dest->regcode, src);
    } else if (src->type == OP_MEM && dest->type == OP_REG) {
      // 0F B6 /r
      gen_prefix(0x66);
      gen_rex(0, dest->regcode, src->index, src->base, false);
      gen_opcode(0x0f);
      gen_opcode(0xb6);
      gen_ops(dest->regcode, src);
    }
  }
}

static void gen_movzw(Inst *inst) {
  Op *src = inst->src, *dest = inst->dest;
  if (inst->suffix == INST_QUAD) {
    if (src->type == OP_REG && dest->type == OP_REG) {
      // REX.W + 0F B7 /r
      gen_rex(1, dest->regcode, 0, src->regcode, false);
      gen_opcode(0x0f);
      gen_opcode(0xb7);
      gen_ops(dest->regcode, src);
    } else if (src->type == OP_MEM && dest->type == OP_REG) {
      // REX.W + 0F B7 /r
      gen_rex(1, dest->regcode, src->index, src->base, false);
      gen_opcode(0x0f);
      gen_opcode(0xb7);
      gen_ops(dest->regcode, src);
    }
  } else if (inst->suffix == INST_LONG) {
    if (src->type == OP_REG && dest->type == OP_REG) {
      // 0F B7 /r
      gen_rex(0, dest->regcode, 0, src->regcode, false);
      gen_opcode(0x0f);
      gen_opcode(0xb7);
      gen_ops(dest->regcode, src);
    } else if (src->type == OP_MEM && dest->type == OP_REG) {
      // 0F B7 /r
      gen_rex(0, dest->regcode, src->index, src->base, false);
      gen_opcode(0x0f);
      gen_opcode(0xb7);
      gen_ops(dest->regcode, src);
    }
  }
}

static void gen_movsb(Inst *inst) {
  Op *src = inst->src, *dest = inst->dest;
  if (inst->suffix == INST_QUAD) {
    if (src->type == OP_REG && dest->type == OP_REG) {
      // REX.W + 0F BE /r
      gen_rex(1, dest->regcode, 0, src->regcode, false);
      gen_opcode(0x0f);
      gen_opcode(0xbe);
      gen_ops(dest->regcode, src);
    } else if (src->type == OP_MEM && dest->type == OP_REG) {
      // REX.W + 0F BE /r
      gen_rex(1, dest->regcode, src->index, src->base, false);
      gen_opcode(0x0f);
      gen_opcode(0xbe);
      gen_ops(dest->regcode, src);
    }
  } else if (inst->suffix == INST_LONG) {
    if (src->type == OP_REG && dest->type == OP_REG) {
      // 0F BE /r
      gen_rex(0, dest->regcode, 0, src->regcode, false);
      gen_opcode(0x0f);
      gen_opcode(0xbe);
      gen_ops(dest->regcode, src);
    } else if (src->type == OP_MEM && dest->type == OP_REG) {
      // 0F BE /r
      gen_rex(0, dest->regcode, src->index, src->base, false);
      gen_opcode(0x0f);
      gen_opcode(0xbe);
      gen_ops(dest->regcode, src);
    }
  } else if (inst->suffix == INST_WORD) {
    if (src->type == OP_REG && dest->type == OP_REG) {
      // 0F BE /r
      gen_prefix(0x66);
      gen_rex(0, dest->regcode, 0, src->regcode, false);
      gen_opcode(0x0f);
      gen_opcode(0xbe);
      gen_ops(dest->regcode, src);
    } else if (src->type == OP_MEM && dest->type == OP_REG) {
      // 0F BE /r
      gen_prefix(0x66);
      gen_rex(0, dest->regcode, src->index, src->base, false);
      gen_opcode(0x0f);
      gen_opcode(0xbe);
      gen_ops(dest->regcode, src);
    }
  }
}

static void gen_movsw(Inst *inst) {
  Op *src = inst->src, *dest = inst->dest;
  if (inst->suffix == INST_QUAD) {
    if (src->type == OP_REG && dest->type == OP_REG) {
      // REX.W + 0F BF /r
      gen_rex(1, dest->regcode, 0, src->regcode, false);
      gen_opcode(0x0f);
      gen_opcode(0xbf);
      gen_ops(dest->regcode, src);
    } else if (src->type == OP_MEM && dest->type == OP_REG) {
      // REX.W + 0F BF /r
      gen_rex(1, dest->regcode, src->index, src->base, false);
      gen_opcode(0x0f);
      gen_opcode(0xbf);
      gen_ops(dest->regcode, src);
    }
  } else if (inst->suffix == INST_LONG) {
    if (src->type == OP_REG && dest->type == OP_REG) {
      // 0F BF /r
      gen_rex(0, dest->regcode, 0, src->regcode, false);
      gen_opcode(0x0f);
      gen_opcode(0xbf);
      gen_ops(dest->regcode, src);
    } else if (src->type == OP_MEM && dest->type == OP_REG) {
      // 0F BF /r
      gen_rex(0, dest->regcode, src->index, src->base, false);
      gen_opcode(0x0f);
      gen_opcode(0xbf);
      gen_ops(dest->regcode, src);
    }
  }
}

static void gen_movsl(Inst *inst) {
  Op *src = inst->src, *dest = inst->dest;
  if (src->type == OP_REG && dest->type == OP_REG) {
    // REX.W + 63 /r
    gen_rex(1, dest->regcode, 0, src->regcode, false);
    gen_opcode(0x63);
    gen_ops(dest->regcode, src);
  } else if (src->type == OP_MEM && dest->type == OP_REG) {
    // REX.W + 63 /r
    gen_rex(1, dest->regcode, src->index, src->base, false);
    gen_opcode(0x63);
    gen_ops(dest->regcode, src);
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

static void gen_and(Inst *inst) {
  Op *src = inst->src, *dest = inst->dest;
  if (inst->suffix == INST_QUAD) {
    if (src->type == OP_IMM && dest->type == OP_REG) {
      // REX.W + 81 /4 id
      gen_rex(1, 0, 0, dest->regcode, false);
      gen_opcode(0x81);
      gen_ops(4, dest);
      gen_imm32(src->imm);
    } else if (src->type == OP_IMM && dest->type == OP_MEM) {
      // REX.W + 81 /4 id
      gen_rex(1, 0, dest->index, dest->base, false);
      gen_opcode(0x81);
      gen_ops(4, dest);
      gen_imm32(src->imm);
    } else if (src->type == OP_REG && dest->type == OP_REG) {
      // REX.W + 21 /r
      gen_rex(1, src->regcode, 0, dest->regcode, false);
      gen_opcode(0x21);
      gen_ops(src->regcode, dest);
    } else if (src->type == OP_REG && dest->type == OP_MEM) {
      // REX.W + 21 /r
      gen_rex(1, src->regcode, dest->index, dest->base, false);
      gen_opcode(0x21);
      gen_ops(src->regcode, dest);
    } else if (src->type == OP_MEM && dest->type == OP_REG) {
      // REX.W + 23 /r
      gen_rex(1, dest->regcode, src->index, src->base, false);
      gen_opcode(0x23);
      gen_ops(dest->regcode, src);
    }
  } else if (inst->suffix == INST_LONG) {
    if (src->type == OP_IMM && dest->type == OP_REG) {
      // 81 /4 id
      gen_rex(0, 0, 0, dest->regcode, false);
      gen_opcode(0x81);
      gen_ops(4, dest);
      gen_imm32(src->imm);
    } else if (src->type == OP_IMM && dest->type == OP_MEM) {
      // 81 /4 id
      gen_rex(0, 0, dest->index, dest->base, false);
      gen_opcode(0x81);
      gen_ops(4, dest);
      gen_imm32(src->imm);
    } else if (src->type == OP_REG && dest->type == OP_REG) {
      // 21 /r
      gen_rex(0, src->regcode, 0, dest->regcode, false);
      gen_opcode(0x21);
      gen_ops(src->regcode, dest);
    } else if (src->type == OP_REG && dest->type == OP_MEM) {
      // 21 /r
      gen_rex(0, src->regcode, dest->index, dest->base, false);
      gen_opcode(0x21);
      gen_ops(src->regcode, dest);
    } else if (src->type == OP_MEM && dest->type == OP_REG) {
      // 23 /r
      gen_rex(0, dest->regcode, src->index, src->base, false);
      gen_opcode(0x23);
      gen_ops(dest->regcode, src);
    }
  }
}

static void gen_xor(Inst *inst) {
  Op *src = inst->src, *dest = inst->dest;
  if (inst->suffix == INST_QUAD) {
    if (src->type == OP_IMM && dest->type == OP_REG) {
      // REX.W + 81 /6 id
      gen_rex(1, 0, 0, dest->regcode, false);
      gen_opcode(0x81);
      gen_ops(6, dest);
      gen_imm32(src->imm);
    } else if (src->type == OP_IMM && dest->type == OP_MEM) {
      // REX.W + 81 /6 id
      gen_rex(1, 0, dest->index, dest->base, false);
      gen_opcode(0x81);
      gen_ops(6, dest);
      gen_imm32(src->imm);
    } else if (src->type == OP_REG && dest->type == OP_REG) {
      // REX.W + 31 /r
      gen_rex(1, src->regcode, 0, dest->regcode, false);
      gen_opcode(0x31);
      gen_ops(src->regcode, dest);
    } else if (src->type == OP_REG && dest->type == OP_MEM) {
      // REX.W + 31 /r
      gen_rex(1, src->regcode, dest->index, dest->base, false);
      gen_opcode(0x31);
      gen_ops(src->regcode, dest);
    } else if (src->type == OP_MEM && dest->type == OP_REG) {
      // REX.W + 33 /r
      gen_rex(1, dest->regcode, src->index, src->base, false);
      gen_opcode(0x33);
      gen_ops(dest->regcode, src);
    }
  } else if (inst->suffix == INST_LONG) {
    if (src->type == OP_IMM && dest->type == OP_REG) {
      // 81 /6 id
      gen_rex(0, 0, 0, dest->regcode, false);
      gen_opcode(0x81);
      gen_ops(6, dest);
      gen_imm32(src->imm);
    } else if (src->type == OP_IMM && dest->type == OP_MEM) {
      // 81 /6 id
      gen_rex(0, 0, dest->index, dest->base, false);
      gen_opcode(0x81);
      gen_ops(6, dest);
      gen_imm32(src->imm);
    } else if (src->type == OP_REG && dest->type == OP_REG) {
      // 31 /r
      gen_rex(0, src->regcode, 0, dest->regcode, false);
      gen_opcode(0x31);
      gen_ops(src->regcode, dest);
    } else if (src->type == OP_REG && dest->type == OP_MEM) {
      // 31 /r
      gen_rex(0, src->regcode, dest->index, dest->base, false);
      gen_opcode(0x31);
      gen_ops(src->regcode, dest);
    } else if (src->type == OP_MEM && dest->type == OP_REG) {
      // 33 /r
      gen_rex(0, dest->regcode, src->index, src->base, false);
      gen_opcode(0x33);
      gen_ops(dest->regcode, src);
    }
  }
}

static void gen_or(Inst *inst) {
  Op *src = inst->src, *dest = inst->dest;
  if (inst->suffix == INST_QUAD) {
    if (src->type == OP_IMM && dest->type == OP_REG) {
      // REX.W + 81 /1 id
      gen_rex(1, 0, 0, dest->regcode, false);
      gen_opcode(0x81);
      gen_ops(1, dest);
      gen_imm32(src->imm);
    } else if (src->type == OP_IMM && dest->type == OP_MEM) {
      // REX.W + 81 /1 id
      gen_rex(1, 0, dest->index, dest->base, false);
      gen_opcode(0x81);
      gen_ops(1, dest);
      gen_imm32(src->imm);
    } else if (src->type == OP_REG && dest->type == OP_REG) {
      // REX.W + 09 /r
      gen_rex(1, src->regcode, 0, dest->regcode, false);
      gen_opcode(0x09);
      gen_ops(src->regcode, dest);
    } else if (src->type == OP_REG && dest->type == OP_MEM) {
      // REX.W + 09 /r
      gen_rex(1, src->regcode, dest->index, dest->base, false);
      gen_opcode(0x09);
      gen_ops(src->regcode, dest);
    } else if (src->type == OP_MEM && dest->type == OP_REG) {
      // REX.W + 0B /r
      gen_rex(1, dest->regcode, src->index, src->base, false);
      gen_opcode(0x0b);
      gen_ops(dest->regcode, src);
    }
  } else if (inst->suffix == INST_LONG) {
    if (src->type == OP_IMM && dest->type == OP_REG) {
      // 81 /1 id
      gen_rex(0, 0, 0, dest->regcode, false);
      gen_opcode(0x81);
      gen_ops(1, dest);
      gen_imm32(src->imm);
    } else if (src->type == OP_IMM && dest->type == OP_MEM) {
      // 81 /1 id
      gen_rex(0, 0, dest->index, dest->base, false);
      gen_opcode(0x81);
      gen_ops(1, dest);
      gen_imm32(src->imm);
    } else if (src->type == OP_REG && dest->type == OP_REG) {
      // 09 /r
      gen_rex(0, src->regcode, 0, dest->regcode, false);
      gen_opcode(0x09);
      gen_ops(src->regcode, dest);
    } else if (src->type == OP_REG && dest->type == OP_MEM) {
      // 09 /r
      gen_rex(0, src->regcode, dest->index, dest->base, false);
      gen_opcode(0x09);
      gen_ops(src->regcode, dest);
    } else if (src->type == OP_MEM && dest->type == OP_REG) {
      // 0B /r
      gen_rex(0, dest->regcode, src->index, src->base, false);
      gen_opcode(0x0b);
      gen_ops(dest->regcode, src);
    }
  }
}

static void gen_sal(Inst *inst) {
  Op *dest = inst->dest;
  if (inst->suffix == INST_QUAD) {
    if (dest->type == OP_REG) {
      // REX.W + D3 /4
      gen_rex(1, 0, 0, dest->regcode, false);
      gen_opcode(0xd3);
      gen_ops(4, dest);
    } else if (dest->type == OP_MEM) {
      // REX.W + D3 /4
      gen_rex(1, 0, dest->index, dest->base, false);
      gen_opcode(0xd3);
      gen_ops(4, dest);
    }
  } else if (inst->suffix == INST_LONG) {
    if (dest->type == OP_REG) {
      // D3 /4
      gen_rex(0, 0, 0, dest->regcode, false);
      gen_opcode(0xd3);
      gen_ops(4, dest);
    } else if (dest->type == OP_MEM) {
      // D3 /4
      gen_rex(0, 0, dest->index, dest->base, false);
      gen_opcode(0xd3);
      gen_ops(4, dest);
    }
  }
}

static void gen_sar(Inst *inst) {
  Op *dest = inst->dest;
  if (inst->suffix == INST_QUAD) {
    if (dest->type == OP_REG) {
      // REX.W + D3 /7
      gen_rex(1, 0, 0, dest->regcode, false);
      gen_opcode(0xd3);
      gen_ops(7, dest);
    } else if (dest->type == OP_MEM) {
      // REX.W + D3 /7
      gen_rex(1, 0, dest->index, dest->base, false);
      gen_opcode(0xd3);
      gen_ops(7, dest);
    }
  } else if (inst->suffix == INST_LONG) {
    if (dest->type == OP_REG) {
      // D3 /7
      gen_rex(0, 0, 0, dest->regcode, false);
      gen_opcode(0xd3);
      gen_ops(7, dest);
    } else if (dest->type == OP_MEM) {
      // D3 /7
      gen_rex(0, 0, dest->index, dest->base, false);
      gen_opcode(0xd3);
      gen_ops(7, dest);
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

static void gen_setb(Inst *inst) {
  Op *op = inst->op;
  if (op->type == OP_REG) {
    // 0F 92
    bool required = (op->regcode & 12) == 4;
    gen_rex(0, 0, 0, op->regcode, required);
    gen_opcode(0x0f);
    gen_opcode(0x92);
    gen_ops(0, op); // reg filed is not used
  } else {
    // 0F 92
    gen_rex(0, 0, op->index, op->base, 0);
    gen_opcode(0x0f);
    gen_opcode(0x92);
    gen_ops(0, op); // reg filed is not used
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

static void gen_setbe(Inst *inst) {
  Op *op = inst->op;
  if (op->type == OP_REG) {
    // 0F 96
    bool required = (op->regcode & 12) == 4;
    gen_rex(0, 0, 0, op->regcode, required);
    gen_opcode(0x0f);
    gen_opcode(0x96);
    gen_ops(0, op); // reg filed is not used
  } else {
    // 0F 96
    gen_rex(0, 0, op->index, op->base, 0);
    gen_opcode(0x0f);
    gen_opcode(0x96);
    gen_ops(0, op); // reg filed is not used
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

TransUnit *as_encode(Vector *stmts) {
  trans_unit = trans_unit_new();
  bin = trans_unit->text->bin;
  relocs = trans_unit->text->relocs;
  symbols = trans_unit->symbols;
  current = TEXT;

  for (int i = 0; i < stmts->length; i++) {
    Stmt *stmt = stmts->buffer[i];
    switch (stmt->type) {
      // label
      case ST_LABEL: gen_label((Label *) stmt); break;

      // directives
      case ST_TEXT: gen_text((Dir *) stmt); break;
      case ST_DATA: gen_data((Dir *) stmt); break;
      case ST_SECTION: gen_section((Dir *) stmt); break;
      case ST_GLOBAL: gen_global((Dir *) stmt); break;
      case ST_ZERO: gen_zero((Dir *) stmt); break;
      case ST_LONG: gen_long((Dir *) stmt); break;
      case ST_QUAD: gen_quad((Dir *) stmt); break;
      case ST_ASCII: gen_ascii((Dir *) stmt); break;

      // instructions
      case ST_PUSH: gen_push((Inst *) stmt); break;
      case ST_POP: gen_pop((Inst *) stmt); break;
      case ST_CLTD: gen_cltd((Inst *) stmt); break;
      case ST_CQTO: gen_cqto((Inst *) stmt); break;
      case ST_MOV: gen_mov((Inst *) stmt); break;
      case ST_MOVZB: gen_movzb((Inst *) stmt); break;
      case ST_MOVZW: gen_movzw((Inst *) stmt); break;
      case ST_MOVSB: gen_movsb((Inst *) stmt); break;
      case ST_MOVSW: gen_movsw((Inst *) stmt); break;
      case ST_MOVSL: gen_movsl((Inst *) stmt); break;
      case ST_LEA: gen_lea((Inst *) stmt); break;
      case ST_NEG: gen_neg((Inst *) stmt); break;
      case ST_NOT: gen_not((Inst *) stmt); break;
      case ST_ADD: gen_add((Inst *) stmt); break;
      case ST_SUB: gen_sub((Inst *) stmt); break;
      case ST_MUL: gen_mul((Inst *) stmt); break;
      case ST_IMUL: gen_imul((Inst *) stmt); break;
      case ST_DIV: gen_div((Inst *) stmt); break;
      case ST_IDIV: gen_idiv((Inst *) stmt); break;
      case ST_AND: gen_and((Inst *) stmt); break;
      case ST_XOR: gen_xor((Inst *) stmt); break;
      case ST_OR: gen_or((Inst *) stmt); break;
      case ST_SAL: gen_sal((Inst *) stmt); break;
      case ST_SAR: gen_sar((Inst *) stmt); break;
      case ST_CMP: gen_cmp((Inst *) stmt); break;
      case ST_SETE: gen_sete((Inst *) stmt); break;
      case ST_SETNE: gen_setne((Inst *) stmt); break;
      case ST_SETB: gen_setb((Inst *) stmt); break;
      case ST_SETL: gen_setl((Inst *) stmt); break;
      case ST_SETG: gen_setg((Inst *) stmt); break;
      case ST_SETBE: gen_setbe((Inst *) stmt); break;
      case ST_SETLE: gen_setle((Inst *) stmt); break;
      case ST_SETGE: gen_setge((Inst *) stmt); break;
      case ST_JMP: gen_jmp((Inst *) stmt); break;
      case ST_JE: gen_je((Inst *) stmt); break;
      case ST_JNE: gen_jne((Inst *) stmt); break;
      case ST_CALL: gen_call((Inst *) stmt); break;
      case ST_LEAVE: gen_leave((Inst *) stmt); break;
      case ST_RET: gen_ret((Inst *) stmt); break;
    }
  }

  return trans_unit;
}
