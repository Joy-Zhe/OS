#ifndef PRINT_ONLY
#include "defs.h"

extern main(), puts(), put_num(), ticks;
extern void clock_set_next_event(void);

void handler_s(uint64_t cause) {
  // TODO
  // interrupt
  if (cause & (0x8000000000000000) == 0x8000000000000000)
  {
    // supervisor timer interrupt
    if ((cause & (0b1101)) == 5)
    {
      // 设置下一个时钟中断，打印当前的中断数目
      // TODO
      clock_set_next_event();
      // ticks++;
      put_num(ticks);
      puts("\n");
    }
  }
}
#endif