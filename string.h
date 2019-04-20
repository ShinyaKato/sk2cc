typedef struct string {
  int capacity, length;
  char *buffer;
} String;

extern String *string_new(void);
extern void string_push(String *string, char c);
extern void string_write(String *string, char *s);
