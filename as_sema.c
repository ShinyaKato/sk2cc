#include "as.h"

static InstSuffix operand_suffix(Op *op) {
  if (op->type == OP_REG) {
    switch (op->regtype) {
      case REG_BYTE: return INST_BYTE;
      case REG_WORD: return INST_WORD;
      case REG_LONG: return INST_LONG;
      case REG_QUAD: return INST_QUAD;
    }
  }

  return -1;
}

static void sema_inst_op0(Inst *inst, InstSuffix default_suffix) {
  Vector *ops = inst->ops;
  if (ops->length != 0) {
    ERROR(inst->token, "no operand is expected.");
  }

  if (inst->suffix == -1) {
    inst->suffix = default_suffix;
  }

  if (inst->suffix == -1) {
    ERROR(inst->token, "operand type mismatched.");
  }
}

static void sema_inst_op1(Inst *inst, InstSuffix default_suffix) {
  Vector *ops = inst->ops;
  if (ops->length != 1) {
    ERROR(inst->token, "1 operand is expected.");
  }
  inst->op = ops->buffer[0];

  if (inst->suffix == -1) {
    inst->suffix = operand_suffix(inst->op);
  } else {
    InstSuffix op_suffix = operand_suffix(inst->op);
    if (op_suffix != -1 && inst->suffix != op_suffix) {
      ERROR(inst->token, "operand type mismatched.");
    }
  }

  if (inst->suffix == -1) {
    inst->suffix = default_suffix;
  }

  if (inst->suffix == -1) {
    ERROR(inst->token, "operand type mismatched.");
  }
}

static void sema_inst_op2(Inst *inst, InstSuffix default_suffix) {
  Vector *ops = inst->ops;
  if (ops->length != 2) {
    ERROR(inst->token, "2 operands are expected.");
  }
  inst->src = ops->buffer[0];
  inst->dest = ops->buffer[1];

  if (inst->src->type == OP_MEM && inst->dest->type == OP_MEM) {
    ERROR(inst->token, "both of source and destination cannot be memory operands.");
  }
  if (inst->dest->type == OP_IMM) {
    ERROR(inst->token, "destination cannot be an immediate.");
  }

  if (inst->suffix == -1) {
    inst->suffix = operand_suffix(inst->src);
  } else {
    InstSuffix op_suffix = operand_suffix(inst->src);
    if (op_suffix != -1 && inst->suffix != op_suffix) {
      ERROR(inst->token, "operand type mismatched.");
    }
  }

  if (inst->suffix == -1) {
    inst->suffix = operand_suffix(inst->dest);
  } else {
    InstSuffix op_suffix = operand_suffix(inst->dest);
    if (op_suffix != -1 && inst->suffix != op_suffix) {
      ERROR(inst->token, "operand type mismatched.");
    }
  }

  if (inst->suffix == -1) {
    inst->suffix = default_suffix;
  }

  if (inst->suffix == -1) {
    ERROR(inst->token, "operand type mismatched.");
  }
}

static void sema_inst_src_dest(Inst *inst, InstSuffix src_suffix, InstSuffix default_suffix) {
  Vector *ops = inst->ops;
  if (ops->length != 2) {
    ERROR(inst->token, "2 operands are expected.");
  }
  inst->src = ops->buffer[0];
  inst->dest = ops->buffer[1];

  if (inst->src->type == OP_MEM && inst->dest->type == OP_MEM) {
    ERROR(inst->token, "both of source and destination cannot be memory operands.");
  }
  if (inst->dest->type == OP_IMM) {
    ERROR(inst->token, "destination cannot be an immediate.");
  }

  InstSuffix op_suffix = operand_suffix(inst->src);
  if (op_suffix != -1 && src_suffix != op_suffix) {
    ERROR(inst->token, "operand type mismatched.");
  }

  if (inst->suffix == -1) {
    inst->suffix = operand_suffix(inst->dest);
  } else {
    InstSuffix op_suffix = operand_suffix(inst->dest);
    if (op_suffix != -1 && inst->suffix != op_suffix) {
      ERROR(inst->token, "operand type mismatched.");
    }
  }

  if (inst->suffix == -1) {
    inst->suffix = default_suffix;
  }

  if (inst->suffix == -1) {
    ERROR(inst->token, "operand type mismatched.");
  }
}

void as_sema(Vector *stmts) {
  for (int i = 0; i < stmts->length; i++) {
    Stmt *stmt = stmts->buffer[i];
    switch (stmt->type) {
      case ST_CLTD: {
        sema_inst_op0((Inst *) stmt, INST_LONG);
        break;
      }
      case ST_CQTO:
      case ST_LEAVE:
      case ST_RET: {
        sema_inst_op0((Inst *) stmt, INST_QUAD);
        break;
      }
      case ST_PUSH:
      case ST_POP: {
        Inst *inst = (Inst *) stmt;
        sema_inst_op1(inst, INST_QUAD);
        if (inst->op->type != OP_REG) {
          ERROR(inst->token, "only register operand is supported.");
        }
        if (inst->op->regtype != REG_QUAD) {
          ERROR(inst->token, "only 64-bit register is supported.");
        }
        break;
      }
      case ST_NEG:
      case ST_NOT:
      case ST_MUL:
      case ST_IMUL:
      case ST_DIV:
      case ST_IDIV: {
        sema_inst_op1((Inst *) stmt, -1);
        break;
      }
      case ST_JMP:
      case ST_JE:
      case ST_JNE:
      case ST_CALL: {
        Inst *inst = (Inst *) stmt;
        sema_inst_op1(inst, INST_QUAD);
        if (inst->op->type != OP_SYM) {
          ERROR(inst->token, "only symbol is supported.");
        }
        if (inst->suffix != INST_QUAD) {
          ERROR(inst->token, "only 64-bit instruction is supported.");
        }
        break;
      }
      case ST_SETE:
      case ST_SETNE:
      case ST_SETB:
      case ST_SETL:
      case ST_SETG:
      case ST_SETBE:
      case ST_SETLE:
      case ST_SETGE: {
        sema_inst_op1((Inst *) stmt, INST_BYTE);
        break;
      }
      case ST_MOV:
      case ST_ADD:
      case ST_SUB:
      case ST_AND:
      case ST_XOR:
      case ST_OR:
      case ST_CMP: {
        sema_inst_op2((Inst *) stmt, -1);
        break;
      }
      case ST_LEA: {
        Inst *inst = (Inst *) stmt;
        sema_inst_op2(inst, -1);
        if (inst->src->type != OP_MEM || inst->dest->type != OP_REG) {
          ERROR(inst->token, "source should be memory operand and destination should be register operand.");
        }
        break;
      }
      case ST_SAL:
      case ST_SAR: {
        Inst *inst = (Inst *) stmt;
        sema_inst_src_dest(inst, INST_BYTE, -1);
        if (inst->src->regcode != REG_CX) {
          ERROR(inst->token, "only %%cl is supported.");
        }
        break;
      }
      case ST_MOVZB:
      case ST_MOVSB: {
        Inst *inst = (Inst *) stmt;
        sema_inst_src_dest(inst, INST_BYTE, -1);
        if (inst->suffix == INST_BYTE) {
          ERROR(inst->token, "invalid instruction suffix");
        }
        break;
      }
      case ST_MOVZW:
      case ST_MOVSW: {
        Inst *inst = (Inst *) stmt;
        sema_inst_src_dest(inst, INST_WORD, -1);
        if (inst->suffix == INST_BYTE) {
          ERROR(inst->token, "invalid instruction suffix");
        }
        if (inst->suffix == INST_WORD) {
          ERROR(inst->token, "invalid instruction suffix");
        }
        break;
      }
      case ST_MOVSL: {
        Inst *inst = (Inst *) stmt;
        sema_inst_src_dest(inst, INST_LONG, -1);
        if (inst->suffix == INST_BYTE) {
          ERROR(inst->token, "invalid instruction suffix");
        }
        if (inst->suffix == INST_WORD) {
          ERROR(inst->token, "invalid instruction suffix");
        }
        if (inst->suffix == INST_LONG) {
          ERROR(inst->token, "invalid instruction suffix");
        }
        break;
      }
      default: /* nothing to do */;
    }
  }
}
