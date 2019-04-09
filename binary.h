typedef unsigned char Byte;

typedef struct {
  int length, capacity;
  Byte *buffer;
} Binary;

extern Binary *binary_new();
extern void binary_push(Binary *binary, Byte byte);
extern void binary_append(Binary *binary, int size, ...);
extern void binary_write(Binary *binary, void *buffer, int size);
