#include "sched.h"
#include "stdio.h"
#include "sched.h"
#include "mm.h"

int start_kernel() {
  puts("ZJU OSLAB 6 3210103924 陈震翔 3210104169 郑乔尹\n");
  
  slub_init();
  task_init();

  // 设置第一次时钟中断
  asm volatile("ecall");
  
  call_first_process();
  dead_loop();
  return 0;
}
