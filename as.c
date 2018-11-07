#include <stdio.h>
#include <stdlib.h>
#include <elf.h>

int main(int argc, char *argv[]) {
  if (argc != 3) {
    fprintf(stderr, "usage: ./as [integer] [output file]\n");
    exit(1);
  }
  int n = atoi(argv[1]);
  char *file = argv[2];

  // .text
  //   mov $n, %rax
  //   ret
  uint8_t text[8] = {
    0x48, 0xc7, 0xc0, n & 0xff, (n >> 8) & 0xff, (n >> 16) & 0xff, (n >> 24) & 0xff,
    0xc3,
  };

  // .symtab
  Elf64_Sym *symtab = (Elf64_Sym *) calloc(2, sizeof(Elf64_Sym));
  symtab[1].st_name = 1;
  symtab[1].st_info = ELF64_ST_INFO(STB_GLOBAL, STT_NOTYPE);
  symtab[1].st_other = STV_DEFAULT;
  symtab[1].st_shndx = 1;
  symtab[1].st_value = 0;

  // .strtab
  char strtab[6] = "\0main\0";

  // .shstrtab
  char shstrtab[33] = "\0.text\0.symtab\0.strtab\0.shstrtab\0";

  // section header table
  Elf64_Shdr *shdrtab = (Elf64_Shdr *) calloc(5, sizeof(Elf64_Shdr));

  // section header for .text
  shdrtab[1].sh_name = 1;
  shdrtab[1].sh_type = SHT_PROGBITS;
  shdrtab[1].sh_flags = SHF_ALLOC | SHF_EXECINSTR;
  shdrtab[1].sh_offset = sizeof(Elf64_Ehdr);
  shdrtab[1].sh_size = sizeof(text);

  // section header for .symtab
  shdrtab[2].sh_name = 7;
  shdrtab[2].sh_type = SHT_SYMTAB;
  shdrtab[2].sh_offset = sizeof(Elf64_Ehdr) + sizeof(text);
  shdrtab[2].sh_size = sizeof(Elf64_Sym) * 2;
  shdrtab[2].sh_link = 3;
  shdrtab[2].sh_info = 1;
  shdrtab[2].sh_entsize = sizeof(Elf64_Sym);

  // section header for .strtab
  shdrtab[3].sh_name = 15;
  shdrtab[3].sh_type = SHT_STRTAB;
  shdrtab[3].sh_offset = sizeof(Elf64_Ehdr) + sizeof(text) + sizeof(Elf64_Sym) * 2;
  shdrtab[3].sh_size = sizeof(strtab);

  // section header for .shstrtab
  shdrtab[4].sh_name = 23;
  shdrtab[4].sh_type = SHT_STRTAB;
  shdrtab[4].sh_offset = sizeof(Elf64_Ehdr) + sizeof(text) + sizeof(Elf64_Sym) * 2 + sizeof(strtab);
  shdrtab[4].sh_size = sizeof(shstrtab);

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
  ehdr->e_shoff = sizeof(Elf64_Ehdr) + sizeof(text) + sizeof(Elf64_Sym) * 2 + sizeof(strtab) + sizeof(shstrtab);
  ehdr->e_ehsize = sizeof(Elf64_Ehdr);
  ehdr->e_shentsize = sizeof(Elf64_Shdr);
  ehdr->e_shnum = 5;
  ehdr->e_shstrndx = 4;

  // generate ELF file
  FILE *fp = fopen(file, "wb");
  fwrite(ehdr, sizeof(Elf64_Ehdr), 1, fp);
  fwrite(text, sizeof(text), 1, fp);
  fwrite(symtab, sizeof(Elf64_Sym), 2, fp);
  fwrite(strtab, sizeof(strtab), 1, fp);
  fwrite(shstrtab, sizeof(shstrtab), 1, fp);
  fwrite(shdrtab, sizeof(Elf64_Shdr), 5, fp);
  fclose(fp);

  return 0;
}
