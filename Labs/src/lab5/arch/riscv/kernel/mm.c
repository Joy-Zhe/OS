#include "mm.h"

#include "vm.h"
#include "stdio.h"


#define set_split(x) ((unsigned int)(x) | 0x80000000)
#define set_unsplit(x) ((unsigned int)(x) & 0x7fffffff)
#define get_size(x) set_unsplit(x)
#define check_split(x) ((unsigned int)(x) & 0x80000000)


uint64_t get_index(uint64_t pa) {

  uint64_t offset = (pa - buddy_system.base_addr) / PAGE_SIZE;
  int block_size = 1;
  while(offset % block_size == 0) {
    block_size <<= 1;
  }
  block_size = block_size >> 1;
  return ((MEMORY_SIZE / PAGE_SIZE) / block_size) + (offset / block_size);
}

uint64_t get_addr(int index) {

  int level = 0, index_copy = index;
  while(index_copy > 1) {
    index_copy >>= 1;
    level++;
  }

  int block_size = MEMORY_SIZE >> level;
  int offset = (index - (1 << level)) * block_size;
  return buddy_system.base_addr + offset;
}

uint32_t get_block_size(int index) {
  int level = 0, index_copy = index;
  while(index_copy > 1) {
    index_copy >>= 1;
    level++;
  }
  return MEMORY_SIZE >> level;
}


uint64_t alloc_page() {
  return alloc_pages(1);
}

int alloced_page_num() {
  // 返回已经分配的物理页面的数量
  // 在buddy system中，我们不再使用该函数
  return -1;
}

void init_buddy_system() {
  // TODO: 初始化buddy system
  // 1. 将buddy system的每个节点初始化为可分配的内存大小，单位为PAGE_SIZE
  // 注意我们用buddy_system.bitmap实现一个满二叉树，其下标变化规律如下：如果当前节点的下标是 X，那么左儿子就是 `X * 2` ，右儿子就是 `X * 2 + 1` ，X 从1开始。
  // 那么，下标为 X 的节点可分配的内存为多少呢？
  // 2. 将buddy system的base_addr设置为&_end的物理地址
  // 3. 将buddy system的initialized设置为true
  unsigned int msize = 8192; // 两倍PAGE数量，方便第一次/2
  int levelIndex = 1;
  for( int i = 1; i < 8192; i++ ){
    if( i == levelIndex ){
      msize /= 2;
      levelIndex *= 2;
    }
    buddy_system.bitmap[i] = msize; 
  }
  buddy_system.base_addr = PHYSICAL_ADDR(&_end);
  buddy_system.initialized = 1;

};

uint64_t alloc_buddy(int index, unsigned int num) {
  // TODO: 找到可分配的节点并完成分配，返回分配的物理页面的首地址（通过get_addr函数可以获取节点index的起始地址）
  // 1. 如果当前节点大于num，则查看当前节点的左右儿子节点
  // 提示：通过get_size函数可以获取节点index的可分配连续内存大小
  // 2. 如果当前节点的可分配连续内存等于num，且当前节点没有被拆分，则分配当前节点
  // 提示：通过check_split函数可以获取节点index的状态
  // 3. 如果当前节点的可分配连续内存等于num且已经被拆分，则查看当前节点的左右儿子节点
  // 4. 如果当前节点的可分配连续内存小于num，则分配失败返回上层节点
  // 如果完成分配，则要递归更新节点信息，将涉及到的节点的可分配连续内存更新，
  // 已拆分则是左右子节点的可分配的最大连续物理内存最大值，未拆分则是两者之和，并使用set_split更新节点的状态
  if( num < get_size(buddy_system.bitmap[index]) ){
    uint64_t addr = alloc_buddy( 2*index, num );
    if( addr == 0 ) addr = alloc_buddy( 2*index + 1, num ); // 左节点分配失败，尝试分配右节点
    if( addr != 0 ){
      buddy_system.bitmap[index] = get_size(buddy_system.bitmap[2*index]) > get_size(buddy_system.bitmap[2*index+1]) ? get_size(buddy_system.bitmap[2*index]) : get_size(buddy_system.bitmap[2*index+1]);
      buddy_system.bitmap[index] = set_split(buddy_system.bitmap[index]); 
    }
    return addr;
  }else if( num == get_size(buddy_system.bitmap[index]) ){
    if( check_split(buddy_system.bitmap[index]) ){ // 已被拆分，检查子节点
      uint64_t addr = alloc_buddy( 2*index, num );
      if( addr == 0 ) addr = alloc_buddy( 2*index + 1, num );
      if( addr != 0 ){
        buddy_system.bitmap[index] = get_size(buddy_system.bitmap[2*index]) > get_size(buddy_system.bitmap[2*index+1]) ? get_size(buddy_system.bitmap[2*index]) : get_size(buddy_system.bitmap[2*index+1]);
        buddy_system.bitmap[index] = set_split(buddy_system.bitmap[index]);    
      }
      return addr;
    }else{ // 当前节点被全部使用，剩余空间更新为0
      buddy_system.bitmap[index] = 0;
      return get_addr(index);
    }
  }else return 0;
  
}

uint64_t alloc_pages(unsigned int num) {
  // 分配num个页面，返回分配到的页面的首地址，如果没有足够的空闲页面，返回0
  if (!buddy_system.initialized) {
    init_buddy_system();
  }
  // TODO:
  // 1. 将num向上对齐到2的幂次
  // 2. 调用alloc_buddy函数完成分配
  unsigned int temp = 1;
  for( int i = 0; i < 32; i++ ){
    if( temp >= num ) break;
    temp *= 2;
  }
  return alloc_buddy( 1, temp);
}


void free_buddy(int index) {
  // TODO: 释放节点index的页面
  // 1. 首先判断节点index的状态，如果已经被拆分，则不能直接释放。异常状态，报错并进入死循环。
  // 2. 如果没有被拆分，则恢复节点index的状态为初始状态
  // 3. 如果该节点与其兄弟节点都没有被使用，则合并这两个节点，并递归处理父节点
  // 提示：使用check_split函数可以获取节点index的状态
  // 提示：使用set_unsplit函数可以将节点index的状态恢复为初始状态
  // 提示：使用get_block_size函数可以获取节点index的初始可分配内存大小
  if( check_split(buddy_system.bitmap[index]) ){ // 已被split，无法直接free
    printf("Error:the node is splitted\n");
    while(1);
  }
  else{
    buddy_system.bitmap[index] = get_block_size(index);  // 重置大小为初始大小
    while( index != 1 ){
      int brother = index%2 == 1 ? index - 1 : index + 1;
      if( buddy_system.bitmap[brother] == buddy_system.bitmap[index] ){
        buddy_system.bitmap[index/2] = buddy_system.bitmap[index]*2;
        buddy_system.bitmap[index/2] = set_unsplit(buddy_system.bitmap[index/2]);
        index /= 2;
      }else break;
    }
  }

}

void free_pages(uint64_t pa) {
  // TODO: find the buddy system node according to pa, and free it.
  // 注意，如果该节点的状态为已经被拆分，则应该释放其左子节点
  // 提示：使用get_index函数可以获取pa对应的最上层节点的下标
  int index = get_index(pa);
  while( check_split(buddy_system.bitmap[index]) ) index *= 2;
  free_buddy(index);
}