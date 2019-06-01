// The preprocessor is not sufficiently supported
// enough to read the header file of the system.

typedef signed char int8_t;
typedef unsigned char uint8_t;
typedef signed short int16_t;
typedef unsigned short uint16_t;
typedef signed int int32_t;
typedef unsigned int uint32_t;
typedef signed long long int64_t;
typedef unsigned long long uint64_t;

typedef unsigned long long size_t;

// assert.h
#define assert(x) \
  do { \
    if (!(x)) { \
      fprintf(stderr, "%s:%d assertion failed.\n", __FILE__, __LINE__); \
      exit(1); \
    } \
  } while (0)

// stdbool.h
#define bool _Bool
#define false 0
#define true 1

// stdnoreturn.h
#define noreturn _Noreturn

// stdarg.h
#define va_start __builtin_va_start
#define va_arg __builtin_va_arg
#define va_end __builtin_va_end

typedef __builtin_va_list va_list;

// stdint.h
typedef long int intptr_t;

// stdio.h
#define EOF (-1)
#define NULL ((void *) 0)

typedef struct _IO_FILE FILE;
extern FILE *stdin;
extern FILE *stdout;
extern FILE *stderr;

int printf(char *format, ...);
int fprintf(FILE *stream, char *format, ...);
int vfprintf(FILE *s, char *format, va_list arg);

FILE *fopen(char *filename, char *modes);
size_t fread(void *ptr, size_t size, size_t n, FILE *stream);
size_t fwrite(void *ptr, size_t size, size_t n, FILE *stream);
int fclose(FILE *stream);

int fgetc(FILE *stream);
int ungetc(int c, FILE *stream);

void perror(char *s);

// stdlib.h
void *calloc(size_t nmemb, size_t size);
void *realloc(void *ptr, size_t size);
void exit(int status);

// string.h
int strcmp(char *s1, char *s2);

// ctype.h
int isprint(int c);
int isalpha(int c);
int isalnum(int c);
int isdigit(int c);
int isspace(int c);
int isxdigit(int c);
int tolower(int c);

// elf.h
#define ELF64_R_INFO(sym, type) ((((Elf64_Xword) (sym)) << 32) + (type))

#define R_X86_64_64 1
#define R_X86_64_PC32 2

#define ELF64_ST_INFO(bind, type) (((bind) << 4) + ((type) & 0xf))

#define STB_LOCAL 0
#define STB_GLOBAL 1

#define STV_DEFAULT 0

#define STT_NOTYPE 0
#define STT_SECTION 3

#define SHT_PROGBITS 1
#define SHT_SYMTAB 2
#define SHT_STRTAB 3
#define SHT_RELA 4

#define SHF_WRITE 0x1
#define SHF_ALLOC 0x2
#define SHF_EXECINSTR 0x4
#define SHF_INFO_LINK 0x40

#define ELFCLASS64 2
#define ELFDATA2LSB 1
#define EV_CURRENT 1
#define ET_REL 1
#define ELFOSABI_NONE 0
#define EM_X86_64 62

typedef uint16_t Elf64_Half;
typedef uint32_t Elf64_Word;
typedef int32_t Elf64_Sword;
typedef uint64_t Elf64_Xword;
typedef int64_t Elf64_Sxword;
typedef uint64_t Elf64_Addr;
typedef uint64_t Elf64_Off;
typedef uint16_t Elf64_Section;

typedef struct {
  unsigned char e_ident[16];
  Elf64_Half e_type;
  Elf64_Half e_machine;
  Elf64_Word e_version;
  Elf64_Addr e_entry;
  Elf64_Off e_phoff;
  Elf64_Off e_shoff;
  Elf64_Word e_flags;
  Elf64_Half e_ehsize;
  Elf64_Half e_phentsize;
  Elf64_Half e_phnum;
  Elf64_Half e_shentsize;
  Elf64_Half e_shnum;
  Elf64_Half e_shstrndx;
} Elf64_Ehdr;

typedef struct {
  Elf64_Word sh_name;
  Elf64_Word sh_type;
  Elf64_Xword sh_flags;
  Elf64_Addr sh_addr;
  Elf64_Off sh_offset;
  Elf64_Xword sh_size;
  Elf64_Word sh_link;
  Elf64_Word sh_info;
  Elf64_Xword sh_addralign;
  Elf64_Xword sh_entsize;
} Elf64_Shdr;

typedef struct {
  Elf64_Word st_name;
  unsigned char st_info;
  unsigned char st_other;
  Elf64_Section st_shndx;
  Elf64_Addr st_value;
  Elf64_Xword st_size;
} Elf64_Sym;

typedef struct {
  Elf64_Addr r_offset;
  Elf64_Xword r_info;
  Elf64_Sxword r_addend;
} Elf64_Rela;
