#include "as.h"

typedef struct reloc {
  int offset;
  char *sym;
  Token *token;
} Reloc;

static Reloc *reloc_new(int offset, char *sym, Token *token) {
  Reloc *reloc = (Reloc *) calloc(1, sizeof(Reloc));
  reloc->offset = offset;
  reloc->sym = sym;
  reloc->token = token;
  return reloc;
}

static Vector *insts;
static Map * labels;

static Binary *text;
static Binary *symtab;
static String *strtab;
static Binary *rela_text;

static int *offsets;
static Vector *relocs;
static Map *gsyms;

static void gen_rexw(Reg reg, Reg index, Reg base) {
  bool r = reg & 0x08;
  bool x = index & 0x08;
  bool b = base & 0x08;
  binary_push(text, 0x48 | (r << 2) | (x << 1) | b);
}

static void gen_rex(Reg reg, Reg index, Reg base) {
  bool r = reg & 0x08;
  bool x = index & 0x08;
  bool b = base & 0x08;
  if (r || x || b) {
    binary_push(text, 0x40 | (r << 2) | (x << 1) | b);
  }
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
  if (base == BP || base == R13) return MOD_DISP8;

  if (disp == 0) return MOD_DISP0;
  if (-128 <= disp && disp < 128) return MOD_DISP8;
  return MOD_DISP32;
}

static void gen_mod_rm(Mod mod, Reg reg, Reg rm) {
  binary_push(text, ((mod & 0x03) << 6) | ((reg & 0x07) << 3) | (rm & 0x07));
}

static void gen_sib(int scale, Reg index, Reg base) {
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

static void gen_imm32(unsigned int imm) {
  binary_push(text, (imm >> 0) & 0xff);
  binary_push(text, (imm >> 8) & 0xff);
  binary_push(text, (imm >> 16) & 0xff);
  binary_push(text, (imm >> 24) & 0xff);
}

static void gen_push_inst(Inst *inst) {
  Op *op = inst->op;
  if (op->type != OP_REG) {
    ERROR(op->token, "invalid operand type.");
  }

  // 50 +rd
  gen_rex(0, 0, op->reg);
  gen_opcode_reg(0x50, op->reg);
}

static void gen_pop_inst(Inst *inst) {
  Op *op = inst->op;
  if (op->type != OP_REG) {
    ERROR(op->token, "invalid operand type.");
  }

  // 58 +rd
  gen_rex(0, 0, op->reg);
  gen_opcode_reg(0x58, op->reg);
}

static void gen_mov_inst(Inst *inst) {
  Op *src = inst->src, *dest = inst->dest;

  // REX.W + C7 /0 id
  if (src->type == OP_IMM && dest->type == OP_REG) {
    gen_rexw(0, 0, dest->reg);
    gen_opcode(0xc7);
    gen_mod_rm(MOD_REG, 0, dest->reg);
    gen_imm32(src->imm);
    return;
  }

  // REX.W + C7 /0 id
  if (src->type == OP_IMM && dest->type == OP_MEM) {
    Mod mod = mod_mem(dest->disp, dest->base);
    gen_rexw(0, 0, dest->base);
    gen_opcode(0xc7);
    gen_mod_rm(mod, 0, dest->base);
    if (dest->base == SP || dest->base == R12) {
      gen_sib(0, 4, dest->base);
    }
    gen_disp(mod, dest->disp);
    gen_imm32(src->imm);
    return;
  }

  // REX.W + 89 /r
  if (src->type == OP_REG && dest->type == OP_REG) {
    gen_rexw(src->reg, 0, dest->reg);
    gen_opcode(0x89);
    gen_mod_rm(MOD_REG, src->reg, dest->reg);
    return;
  }

  // REX.W + 89 /r
  if (src->type == OP_REG && dest->type == OP_MEM) {
    Mod mod = mod_mem(dest->disp, dest->base);
    gen_rexw(src->reg, 0, dest->base);
    gen_opcode(0x89);
    gen_mod_rm(mod, src->reg, dest->base);
    if (dest->base == SP || dest->base == R12) {
      gen_sib(0, 4, dest->base);
    }
    gen_disp(mod, dest->disp);
    return;
  }

  // REX.W + 8B /r
  if (src->type == OP_MEM && dest->type == OP_REG) {
    Mod mod = mod_mem(src->disp, src->base);
    gen_rexw(dest->reg, 0, src->base);
    gen_opcode(0x8b);
    gen_mod_rm(mod, dest->reg, src->base);
    if (src->base == SP || src->base == R12) {
      gen_sib(0, 4, src->base);
    }
    gen_disp(mod, src->disp);
    return;
  }

  ERROR(src->token, "invalid operand types.");
}

static void gen_call_inst(Inst *inst) {
  Op *op = inst->op;
  if (op->type != OP_SYM) {
    ERROR(inst->token, "invalid operand type.");
  }

  // E8 cd
  gen_opcode(0xe8);
  gen_imm32(0);

  vector_push(relocs, reloc_new(text->length - 4, op->sym, op->token));
}

static void gen_leave_inst(Inst *inst) {
  gen_opcode(0xc9); // C9
}

static void gen_ret_inst(Inst *inst) {
  gen_opcode(0xc3); // C3
}

static void gen_text() {
  for (int i = 0; i < insts->length; i++) {
    Inst *inst = insts->array[i];
    offsets[i] = text->length;

    switch (inst->type) {
      case INST_PUSH: gen_push_inst(inst); break;
      case INST_POP: gen_pop_inst(inst); break;
      case INST_MOV: gen_mov_inst(inst); break;
      case INST_CALL: gen_call_inst(inst); break;
      case INST_LEAVE: gen_leave_inst(inst); break;
      case INST_RET: gen_ret_inst(inst); break;
      default: ERROR(inst->token, "unknown instruction.");
    }
  }
}

static void gen_symtab() {
  binary_write(symtab, calloc(1, sizeof(Elf64_Sym)), sizeof(Elf64_Sym));
  string_push(strtab, '\0');

  for (int i = 0; i < labels->count; i++) {
    char *key = labels->keys[i];
    Label *val = labels->values[i];

    Elf64_Sym *sym = (Elf64_Sym *) calloc(1, sizeof(Elf64_Sym));
    sym->st_name = strtab->length;
    sym->st_info = ELF64_ST_INFO(STB_GLOBAL, STT_NOTYPE);
    sym->st_other = STV_DEFAULT;
    sym->st_shndx = 1;
    sym->st_value = offsets[val->inst];

    binary_write(symtab, sym, sizeof(Elf64_Sym));
    string_write(strtab, key);
    string_push(strtab, '\0');
    map_put(gsyms, key, (void *) (intptr_t) (i + 1));
  }
}

static void gen_rela_text() {
  for (int i = 0; i < relocs->length; i++) {
    Reloc *reloc = relocs->array[i];
    int index = (int) (intptr_t) map_lookup(gsyms, reloc->sym);
    if (index == 0) {
      ERROR(reloc->token, "undefined symbol: %s.", reloc->sym);
    }

    Elf64_Rela *rela = (Elf64_Rela *) calloc(1, sizeof(Elf64_Rela));
    rela->r_offset = reloc->offset;
    rela->r_info = ELF64_R_INFO(index, R_X86_64_PC32);
    rela->r_addend = -4;

    binary_write(rela_text, rela, sizeof(Elf64_Rela));
  }
}

Section *gen(Unit *unit) {
  insts = unit->insts;
  labels = unit->labels;

  text = binary_new();
  symtab = binary_new();
  strtab = string_new();
  rela_text = binary_new();

  offsets = (int *) calloc(insts->length, sizeof(int));
  relocs = vector_new();
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
