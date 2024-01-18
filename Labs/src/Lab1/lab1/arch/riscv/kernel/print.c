#include "defs.h"
extern struct sbiret sbi_call(uint64_t ext, uint64_t fid, uint64_t arg0,
                              uint64_t arg1, uint64_t arg2, uint64_t arg3,
                              uint64_t arg4, uint64_t arg5);

int puts(char *str) {
  // TODO
  while (*str != '\0')
  {
    sbi_call(1, 0, *str, 0, 0, 0, 0, 0);
    str++;
  }
  return 0;
}

int put_num(uint64_t n) {
  // TODO
  char str[20];
  int i = 0;
  while (n != 0)
  {
    str[i] = n % 10 + '0';
    n /= 10;
    i++;
  }
  str[i] = '\0';
  for (int j = i - 1; j >= 0; j--)
  {
    sbi_call(1, 0, str[j], 0, 0, 0, 0, 0);
  }
  return 0;
}