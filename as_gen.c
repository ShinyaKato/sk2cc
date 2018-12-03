#include "as.h"

void gen_obj(TransUnit *trans_unit, char *output) {
  Binary *text = trans_unit->text;
  Vector *relocs = trans_unit->relocs;
  Map *symbols = trans_unit->symbols;

  // .symtab and .strtab
  Binary *symtab = binary_new();
  String *strtab = string_new();
  Map *gsyms = map_new();

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
      sym->st_shndx = symbol->section;
      sym->st_value = symbol->offset;

      binary_write(symtab, sym, sizeof(Elf64_Sym));
      string_write(strtab, ident);
      string_push(strtab, '\0');
      map_put(gsyms, ident, (void *) (intptr_t) (gsyms->count + 1));
    }
  }

  // .reloc.text
  Binary *rela_text = binary_new();

  for (int i = 0; i < relocs->length; i++) {
    Reloc *reloc = relocs->array[i];
    Symbol *symbol = map_lookup(symbols, reloc->ident);

    if (symbol->global) {
      int index = (int) (intptr_t) map_lookup(gsyms, reloc->ident);

      Elf64_Rela *rela = (Elf64_Rela *) calloc(1, sizeof(Elf64_Rela));
      rela->r_offset = reloc->offset;
      rela->r_info = ELF64_R_INFO(index, R_X86_64_PC32);
      rela->r_addend = -4;

      binary_write(rela_text, rela, sizeof(Elf64_Rela));
    } else {
      int *rel32 = (int *) &text->buffer[reloc->offset];
      *rel32 = symbol->offset - (reloc->offset + 4);
    }
  }

  // .shstrtab
  String *shstrtab = string_new();
  int names[SHNUM];
  char *sections[SHNUM] = {
    "", ".text", ".symtab", ".strtab", ".rela.text", ".shstrtab"
  };
  for (int i = 0; i < SHNUM; i++) {
    names[i] = shstrtab->length;
    string_write(shstrtab, sections[i]);
    string_push(shstrtab, '\0');
  }

  // section header table
  int offset = sizeof(Elf64_Ehdr);
  Elf64_Shdr *shdrtab = (Elf64_Shdr *) calloc(SHNUM, sizeof(Elf64_Shdr));

  // section header for .text
  shdrtab[TEXT].sh_name = names[TEXT];
  shdrtab[TEXT].sh_type = SHT_PROGBITS;
  shdrtab[TEXT].sh_flags = SHF_ALLOC | SHF_EXECINSTR;
  shdrtab[TEXT].sh_offset = offset;
  shdrtab[TEXT].sh_size = text->length;
  offset += text->length;

  // section header for .symtab
  shdrtab[SYMTAB].sh_name = names[SYMTAB];
  shdrtab[SYMTAB].sh_type = SHT_SYMTAB;
  shdrtab[SYMTAB].sh_offset = offset;
  shdrtab[SYMTAB].sh_size = symtab->length;
  shdrtab[SYMTAB].sh_link = STRTAB;
  shdrtab[SYMTAB].sh_info = 1;
  shdrtab[SYMTAB].sh_entsize = sizeof(Elf64_Sym);
  offset += symtab->length;

  // section header for .strtab
  shdrtab[STRTAB].sh_name = names[STRTAB];
  shdrtab[STRTAB].sh_type = SHT_STRTAB;
  shdrtab[STRTAB].sh_offset = offset;
  shdrtab[STRTAB].sh_size = strtab->length;
  offset += strtab->length;

  // section header for .rela.text
  if (rela_text->length > 0) {
    shdrtab[RELA_TEXT].sh_name = names[RELA_TEXT];
    shdrtab[RELA_TEXT].sh_type = SHT_RELA;
    shdrtab[RELA_TEXT].sh_flags = SHF_INFO_LINK;
    shdrtab[RELA_TEXT].sh_offset = offset;
    shdrtab[RELA_TEXT].sh_size = rela_text->length;
    shdrtab[RELA_TEXT].sh_link = SYMTAB;
    shdrtab[RELA_TEXT].sh_info = TEXT;
    shdrtab[RELA_TEXT].sh_entsize = sizeof(Elf64_Rela);
    offset += rela_text->length;
  }

  // section header for .shstrtab
  shdrtab[SHSTRTAB].sh_name = names[SHSTRTAB];
  shdrtab[SHSTRTAB].sh_type = SHT_STRTAB;
  shdrtab[SHSTRTAB].sh_offset = offset;
  shdrtab[SHSTRTAB].sh_size = shstrtab->length;
  offset += shstrtab->length;

  // ELF header
  Elf64_Ehdr *ehdr = (Elf64_Ehdr *) calloc(1, sizeof(Elf64_Ehdr));
  ehdr->e_ident[0] = 0x7f;
  ehdr->e_ident[1] = 'E';
  ehdr->e_ident[2] = 'L';
  ehdr->e_ident[3] = 'F';
  ehdr->e_ident[4] = ELFCLASS64;
  ehdr->e_ident[5] = ELFDATA2LSB;
  ehdr->e_ident[6] = EV_CURRENT;
  ehdr->e_ident[7] = ELFOSABI_NONE;
  ehdr->e_ident[8] = 0;
  ehdr->e_type = ET_REL;
  ehdr->e_machine = EM_X86_64;
  ehdr->e_version = EV_CURRENT;
  ehdr->e_shoff = offset;
  ehdr->e_ehsize = sizeof(Elf64_Ehdr);
  ehdr->e_shentsize = sizeof(Elf64_Shdr);
  ehdr->e_shnum = SHNUM;
  ehdr->e_shstrndx = SHSTRTAB;

  // generate ELF file
  FILE *out = fopen(output, "wb");
  if (!out) {
    perror(output);
    exit(1);
  }
  fwrite(ehdr, sizeof(Elf64_Ehdr), 1, out);
  fwrite(text->buffer, text->length, 1, out);
  fwrite(symtab->buffer, symtab->length, 1, out);
  fwrite(strtab->buffer, strtab->length, 1, out);
  fwrite(rela_text->buffer, rela_text->length, 1, out);
  fwrite(shstrtab->buffer, shstrtab->length, 1, out);
  fwrite(shdrtab, sizeof(Elf64_Shdr), SHNUM, out);
  fclose(out);
}
