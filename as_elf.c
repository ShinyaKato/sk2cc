#include "as.h"

#define SHNUM 6
#define TEXT 1
#define SYMTAB 2
#define STRTAB 3
#define RELA_TEXT 4
#define SHSTRTAB 5

void gen_elf(Unit *unit, char *output) {
  Binary *text = unit->text;
  Binary *symtab = unit->symtab;
  String *strtab = unit->strtab;
  Binary *rela_text = unit->rela_text;

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
