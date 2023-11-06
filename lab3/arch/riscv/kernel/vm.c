#include "vm.h"
#include "sched.h"
#include "stdio.h"
#include "test.h"

extern uint64_t text_start;
extern uint64_t rodata_start;
extern uint64_t data_start;
extern uint64_t _end;
extern uint64_t user_program_start;

uint64_t alloc_page() {
  // 从 &_end 开始分配一个页面，返回该页面的首地址
  // 注意：alloc_page始终返回物理地址
  return PHYSICAL_ADDR((uint64_t)(&_end) + PAGE_SIZE * alloc_page_num++);
}

int alloced_page_num(){
  // 返回已经分配的物理页面的数量
  return alloc_page_num;
}

void create_mapping(uint64_t *pgtbl, uint64_t va, uint64_t pa, uint64_t sz,
                    int perm) {
  // pgtbl 为根页表的基地址
  // va，pa 分别为需要映射的虚拟、物理地址的基地址
  // sz 为映射的大小，单位为字节
  // perm 为映射的读写权限

  // pgtbl 是根页表的首地址，它已经占用了一页的大小，不需要再分配
  // （调用create_mapping前需要先分配好根页表）

  // 1. 通过 va 得到一级页表项的索引
  // 2. 通过 va 得到二级页表项的索引
  // 3. 通过 va 得到三级页表项的索引
  // 4. 如果一级页表项不存在，分配一个二级页表
  // 5. 设置一级页表项的内容
  // 6. 如果二级页表项不存在，分配一个三级页表
  // 7. 设置二级页表项的内容
  // 8. 设置三级页表项的内容

  // TODO: 请完成你的代码

  while(sz > 0){
    
    uint64_t vpn1 = (va >> 12) & 0x1ff;
    uint64_t vpn2 = (va >> 21) & 0x1ff;
    uint64_t vpn3 = (va >> 30) & 0x1ff;
    
    uint64_t *level2;  
    if( (uint64_t)pgtbl[vpn3] & 0x1 == 1 ){
      level2 = PHYSICAL_ADDR( ((uint64_t)pgtbl[vpn3] >> 10) << 12 );
    }else{
      // printf("pgtbl %llx\n", &pgtbl[vpn3]);
      level2 = alloc_page();
      pgtbl[vpn3] = (uint64_t)( ((uint64_t)pgtbl[vpn3] & (uint64_t)0xffc0000000000000) | (((uint64_t)level2 >> 12) << 10) | (uint64_t)0x1 ) ;
    }

    // printf("level2 %llx\n", &level2[vpn2]);
    uint64_t *level1;
    if( (uint64_t)level2[vpn2] & 0x1 == 1 ){
      level1 = PHYSICAL_ADDR( ((uint64_t)level2[vpn2] >> 10) << 12 );
    }else{
      level1 = alloc_page();
      level2[vpn2] = (uint64_t)( ((uint64_t)level2[vpn2] & (uint64_t)0xffc0000000000000) | (((uint64_t)level1 >> 12) << 10) | (uint64_t)0x1 );
    }
    // printf("level1 = %llx\n", &level1[vpn1]);
    level1[vpn1] = (uint64_t)( ((uint64_t)level1[vpn1] & (uint64_t)0xffc0000000000000) | (((uint64_t)pa >> 12) << 10) | (uint64_t)perm );

    va += PAGE_SIZE;
    pa += PAGE_SIZE;
    sz -= PAGE_SIZE;
  }


}

void paging_init() { 
  // 在 vm.c 中编写 paging_init 函数，该函数完成以下工作：
  // 1. 创建内核的虚拟地址空间，调用 create_mapping 函数将虚拟地址 0xffffffc000000000 开始的 16 MB 空间映射到起始物理地址为 0x80000000 的 16MB 空间，PTE_V | PTE_R | PTE_W | PTE_X 为映射的读写权限。
  // 2. 对内核起始地址 0x80000000 的16MB空间做等值映射（将虚拟地址 0x80000000 开始的 16 MB 空间映射到起始物理地址为 0x80000000 的 16MB 空间），PTE_V | PTE_R | PTE_W | PTE_X 为映射的读写权限。
  // 3. 修改对内核空间不同 section 所在页属性的设置，完成对不同section的保护，其中text段的权限为 r-x, rodata 段为 r--, 其他段为 rw-，注意上述两个映射都需要做保护。
  // 4. 将必要的硬件地址（如 0x10000000 为起始地址的 UART ）进行等值映射 ( 可以映射连续 1MB 大小 )，无偏移，PTE_V | PTE_R 为映射的读写权限

  // 注意：paging_init函数创建的页表只用于内核开启页表之后，进入第一个用户进程之前。进入第一个用户进程之后，就会使用进程页表，而不再使用 paging_init 创建的页表。

  uint64_t *pgtbl = alloc_page();
  // TODO: 请完成你的代码
  create_mapping( pgtbl, 0xffffffc000000000, 0x80000000, 0x1000000, PTE_V | PTE_R | PTE_W | PTE_X );
  create_mapping( pgtbl, 0x80000000, 0x80000000, 0x1000000, PTE_V | PTE_R | PTE_W | PTE_X );
  create_mapping( pgtbl, (uint64_t)(&text_start) + offset, (uint64_t)(&text_start), (uint64_t)(&rodata_start) - (uint64_t)(&text_start), PTE_V | PTE_R | PTE_X) ;
  create_mapping( pgtbl, (uint64_t)(&text_start), (uint64_t)(&text_start), (uint64_t)(&rodata_start) - (uint64_t)(&text_start), PTE_V | PTE_R | PTE_X) ;
  create_mapping( pgtbl, (uint64_t)(&rodata_start) + offset, (uint64_t)(&rodata_start), (uint64_t)(&data_start) - (uint64_t)(&rodata_start), PTE_V | PTE_R ) ;
  create_mapping( pgtbl, (uint64_t)(&rodata_start), (uint64_t)(&rodata_start), (uint64_t)(&data_start) - (uint64_t)(&rodata_start), PTE_V | PTE_R ) ;
  create_mapping( pgtbl, (uint64_t)(&data_start) + offset, (uint64_t)(&data_start), (uint64_t)(&user_program_start) - (uint64_t)(&data_start), PTE_V | PTE_R | PTE_W);
  create_mapping( pgtbl, (uint64_t)(&data_start), (uint64_t)(&data_start), (uint64_t)(&user_program_start) - (uint64_t)(&data_start), PTE_V | PTE_R | PTE_W);
  create_mapping( pgtbl, 0x10000000, 0x10000000, 0x100000, PTE_V | PTE_R | PTE_W);
  
}


