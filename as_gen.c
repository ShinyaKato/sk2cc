#include "as.h"

#define LOCAL_SYMS 4
#define TEXT_SYM 1
#define DATA_SYM 2
#define RODATA_SYM 3

static Binary *gen_rela(Section *section, Map *symbols, Map *gsyms, int current) {
  Binary *bin = binary_new();
  int section_syms[SHNUM] = { 0, TEXT_SYM, 0, DATA_SYM, 0, RODATA_SYM, 0, 0, 0, 0 };

  for (int i = 0; i < section->relocs->length; i++) {
    Reloc *reloc = section->relocs->buffer[i];
    Symbol *symbol = map_lookup(symbols, reloc->ident);

    if (symbol->global || symbol->section == UNDEF) {
      int index = (int) (intptr_t) map_lookup(gsyms, reloc->ident);

      Elf64_Rela *rela = (Elf64_Rela *) calloc(1, sizeof(Elf64_Rela));
      rela->r_offset = reloc->offset;
      rela->r_info = ELF64_R_INFO(index, reloc->type);
      rela->r_addend = reloc->addend;

      binary_write(bin, rela, sizeof(Elf64_Rela));
    } else if (symbol->section != current) {
      Elf64_Rela *rela = (Elf64_Rela *) calloc(1, sizeof(Elf64_Rela));
      rela->r_offset = reloc->offset;
      rela->r_info = ELF64_R_INFO(section_syms[symbol->section], reloc->type);
      rela->r_addend = symbol->offset + reloc->addend;

      binary_write(bin, rela, sizeof(Elf64_Rela));
    } else {
      int *rel32 = (int *) &section->bin->buffer[reloc->offset];
      *rel32 = symbol->offset - reloc->offset + reloc->addend;
    }
  }

  return bin;
}

void gen_obj(TransUnit *trans_unit, char *output) {
  Section *text = trans_unit->text;
  Section *data = trans_unit->data;
  Section *rodata = trans_unit->rodata;
  Map *symbols = trans_unit->symbols;
  Map *gsyms = map_new();

  // .symtab and .strtab
  Binary *symtab = binary_new();
  String *strtab = string_new();

  binary_write(symtab, calloc(1, sizeof(Elf64_Sym)), sizeof(Elf64_Sym));
  string_push(strtab, '\0');

  Elf64_Sym *text_sym = (Elf64_Sym *) calloc(1, sizeof(Elf64_Sym));
  text_sym->st_name = strtab->length;
  text_sym->st_info = ELF64_ST_INFO(STB_LOCAL, STT_SECTION);
  text_sym->st_other = STV_DEFAULT;
  text_sym->st_shndx = TEXT;
  text_sym->st_value = 0;

  binary_write(symtab, text_sym, sizeof(Elf64_Sym));
  string_write(strtab, ".text");
  string_push(strtab, '\0');

  Elf64_Sym *data_sym = (Elf64_Sym *) calloc(1, sizeof(Elf64_Sym));
  data_sym->st_name = strtab->length;
  data_sym->st_info = ELF64_ST_INFO(STB_LOCAL, STT_SECTION);
  data_sym->st_other = STV_DEFAULT;
  data_sym->st_shndx = DATA;
  data_sym->st_value = 0;

  binary_write(symtab, data_sym, sizeof(Elf64_Sym));
  string_write(strtab, ".data");
  string_push(strtab, '\0');

  Elf64_Sym *rodata_sym = (Elf64_Sym *) calloc(1, sizeof(Elf64_Sym));
  rodata_sym->st_name = strtab->length;
  rodata_sym->st_info = ELF64_ST_INFO(STB_LOCAL, STT_SECTION);
  rodata_sym->st_other = STV_DEFAULT;
  rodata_sym->st_shndx = RODATA;
  rodata_sym->st_value = 0;

  binary_write(symtab, rodata_sym, sizeof(Elf64_Sym));
  string_write(strtab, ".rodata");
  string_push(strtab, '\0');

  for (int i = 0; i < symbols->count; i++) {
    char *ident = symbols->keys[i];
    Symbol *symbol = symbols->values[i];

    if (symbol->global || symbol->section == UNDEF) {
      Elf64_Sym *sym = (Elf64_Sym *) calloc(1, sizeof(Elf64_Sym));
      sym->st_name = strtab->length;
      sym->st_info = ELF64_ST_INFO(STB_GLOBAL, STT_NOTYPE);
      sym->st_other = STV_DEFAULT;
      sym->st_shndx = symbol->section;
      sym->st_value = symbol->offset;

      binary_write(symtab, sym, sizeof(Elf64_Sym));
      string_write(strtab, ident);
      string_push(strtab, '\0');
      map_put(gsyms, ident, (void *) (intptr_t) (gsyms->count + LOCAL_SYMS));
    }
  }

  // .reloc.text, .rela.data and .rela.rodata
  Binary *rela_text = gen_rela(text, symbols, gsyms, TEXT);
  Binary *rela_data = gen_rela(data, symbols, gsyms, DATA);
  Binary *rela_rodata = gen_rela(rodata, symbols, gsyms, RODATA);

  // .shstrtab
  String *shstrtab = string_new();
  int names[SHNUM];
  char *sections[SHNUM] = {
    "",
    ".text",
    ".rela.text",
    ".data",
    ".rela.data",
    ".rodata",
    ".rela.rodata",
    ".symtab",
    ".strtab",
    ".shstrtab",
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
  shdrtab[TEXT].sh_size = text->bin->length;
  offset += text->bin->length;

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

  // section header for .data
  shdrtab[DATA].sh_name = names[DATA];
  shdrtab[DATA].sh_type = SHT_PROGBITS;
  shdrtab[DATA].sh_flags = SHF_ALLOC | SHF_WRITE;
  shdrtab[DATA].sh_offset = offset;
  shdrtab[DATA].sh_size = data->bin->length;
  offset += data->bin->length;

  // section header for .rela.data
  if (rela_data->length > 0) {
    shdrtab[RELA_DATA].sh_name = names[RELA_DATA];
    shdrtab[RELA_DATA].sh_type = SHT_RELA;
    shdrtab[RELA_DATA].sh_flags = SHF_INFO_LINK;
    shdrtab[RELA_DATA].sh_offset = offset;
    shdrtab[RELA_DATA].sh_size = rela_data->length;
    shdrtab[RELA_DATA].sh_link = SYMTAB;
    shdrtab[RELA_DATA].sh_info = DATA;
    shdrtab[RELA_DATA].sh_entsize = sizeof(Elf64_Rela);
    offset += rela_data->length;
  }

  // section header for .rodata
  shdrtab[RODATA].sh_name = names[RODATA];
  shdrtab[RODATA].sh_type = SHT_PROGBITS;
  shdrtab[RODATA].sh_flags = SHF_ALLOC;
  shdrtab[RODATA].sh_offset = offset;
  shdrtab[RODATA].sh_size = rodata->bin->length;
  offset += rodata->bin->length;

  // section header for .rela.rodata
  if (rela_rodata->length > 0) {
    shdrtab[RELA_RODATA].sh_name = names[RELA_RODATA];
    shdrtab[RELA_RODATA].sh_type = SHT_RELA;
    shdrtab[RELA_RODATA].sh_flags = SHF_INFO_LINK;
    shdrtab[RELA_RODATA].sh_offset = offset;
    shdrtab[RELA_RODATA].sh_size = rela_rodata->length;
    shdrtab[RELA_RODATA].sh_link = SYMTAB;
    shdrtab[RELA_RODATA].sh_info = RODATA;
    shdrtab[RELA_DATA].sh_entsize = sizeof(Elf64_Rela);
    offset += rela_rodata->length;
  }

  // section header for .symtab
  shdrtab[SYMTAB].sh_name = names[SYMTAB];
  shdrtab[SYMTAB].sh_type = SHT_SYMTAB;
  shdrtab[SYMTAB].sh_offset = offset;
  shdrtab[SYMTAB].sh_size = symtab->length;
  shdrtab[SYMTAB].sh_link = STRTAB;
  shdrtab[SYMTAB].sh_info = LOCAL_SYMS;
  shdrtab[SYMTAB].sh_entsize = sizeof(Elf64_Sym);
  offset += symtab->length;

  // section header for .strtab
  shdrtab[STRTAB].sh_name = names[STRTAB];
  shdrtab[STRTAB].sh_type = SHT_STRTAB;
  shdrtab[STRTAB].sh_offset = offset;
  shdrtab[STRTAB].sh_size = strtab->length;
  offset += strtab->length;

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
  fwrite(text->bin->buffer, text->bin->length, 1, out);
  fwrite(rela_text->buffer, rela_text->length, 1, out);
  fwrite(data->bin->buffer, data->bin->length, 1, out);
  fwrite(rela_data->buffer, rela_data->length, 1, out);
  fwrite(rodata->bin->buffer, rodata->bin->length, 1, out);
  fwrite(rela_rodata->buffer, rela_rodata->length, 1, out);
  fwrite(symtab->buffer, symtab->length, 1, out);
  fwrite(strtab->buffer, strtab->length, 1, out);
  fwrite(shstrtab->buffer, shstrtab->length, 1, out);
  fwrite(shdrtab, sizeof(Elf64_Shdr), SHNUM, out);
  fclose(out);
}
