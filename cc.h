#ifndef _CC_H_
#define _CC_H_

#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <stdbool.h>

struct string {
  int size, length;
  char *buffer;
};
typedef struct string String;

extern String *string_new();
extern void string_push(String *string, char c);

#endif
