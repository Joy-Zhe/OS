#include "defs.h"

struct sbiret sbi_call(uint64_t ext, uint64_t fid, uint64_t arg0, uint64_t arg1,
                       uint64_t arg2, uint64_t arg3, uint64_t arg4,
                       uint64_t arg5) {
  struct sbiret ret;
  __asm__ volatile(
    "addi a7, %[ext], 0\n"
    "addi a6, %[fid], 0\n"
    "addi a0, %[arg0], 0\n"
    "addi a1, %[arg1], 0\n"
    "addi a2, %[arg2], 0\n"
    "addi a3, %[arg3], 0\n"
    "addi a4, %[arg4], 0\n"
    "addi a5, %[arg5], 0\n"
    "ecall\n"


    
    "addi %[error], a0, 0\n"
    "addi %[ret_val], a1, 0\n"
    : [error] "=r"(ret.error) , [ret_val] "=r"(ret.value)
    : [ext] "r"(ext), [fid] "r"(fid), [arg0] "r"(arg0), [arg1] "r"(arg1), [arg2] "r"(arg2), [arg3] "r"(arg3), [arg4] "r"(arg4), [arg5] "r"(arg5)
    : "memory", "a0", "a1", "a2", "a3", "a4", "a5", "a6", "a7"
  );
  return ret;
}
