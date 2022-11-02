// Physical memory allocator, for user processes,
// kernel stacks, page-table pages,
// and pipe buffers. Allocates whole 4096-byte pages.

#include "types.h"
#include "param.h"
#include "memlayout.h"
#include "spinlock.h"
#include "riscv.h"
#include "defs.h"

void freerange(void *pa_start, void *pa_end);
struct run* freelist_pop(int cpu_id);
void freelist_push(int cpu_id, struct run* r);

extern char end[]; // first address after kernel.
                   // defined by kernel.ld.

struct run {
  struct run *next;
};

struct {
  struct spinlock lock;
  struct run *freelist;
} kmems[NCPU];


/**
 * 为每一个CPU分配kmem，并将锁初始化
*/
void
kinit()
{
  // 自带实现
  // initlock(&kmem.lock, "kmem");
  
  // 多链表实现
  // 对每个CPU的锁进行初始化
  for (int i = 0; i < NCPU; i++) {
    initlock(&kmems[i].lock, "kmem");
  }

  freerange(end, (void*)PHYSTOP);
}

void
freerange(void *pa_start, void *pa_end)
{
  char *p;
  p = (char*)PGROUNDUP((uint64)pa_start);
  for(; p + PGSIZE <= (char*)pa_end; p += PGSIZE)
    kfree(p);
}

// Free the page of physical memory pointed at by v,
// which normally should have been returned by a
// call to kalloc().  (The exception is when
// initializing the allocator; see kinit above.)
void
kfree(void *pa)
{
  struct run *r;

  if(((uint64)pa % PGSIZE) != 0 || (char*)pa < end || (uint64)pa >= PHYSTOP)
    panic("kfree");

  // Fill with junk to catch dangling refs.
  memset(pa, 1, PGSIZE);

  r = (struct run*)pa;

  // 获取当前CPU核id
  push_off();
  int cpu_id = cpuid();
  pop_off(); 

  // 对CPU_ID对应的链表加锁
  acquire(&kmems[cpu_id].lock);
  
  // 将释放好的内存块插入freelist中
  freelist_push(cpu_id, r);

  release(&kmems[cpu_id].lock);
}

// Allocate one 4096-byte page of physical memory.
// Returns a pointer that the kernel can use.
// Returns 0 if the memory cannot be allocated.
void *
kalloc(void)
{
  struct run *r;

  // 获取当前CPU号
  push_off();
  int cpu_id = cpuid();
  pop_off();

  // 窃取内存的标志变量
  int steal_success = 0;

  acquire(&kmems[cpu_id].lock);
  
  r = freelist_pop(cpu_id);

  // 如果r为空，则说明当前CPU对应的list没有空闲内存
  if (!r) {
    // 此时需要偷盗
    for (int i = 0; i < NCPU; i++) {
      // 保证CPU号不相同
      if (i != cpu_id) {

        // 先判断当前i对应的CPU是否有空闲内存
        if (kmems[i].freelist) {
          
          // 存在空闲内存则加锁
          acquire(&kmems[i].lock);
          r = freelist_pop(i);               
          steal_success = 1;        // 窃取成功

          // 将窃取得到的内存加入当前cpu的free list中
          freelist_push(cpu_id, r);

          release(&kmems[i].lock);

          // 窃取成功，跳出循环
          break;
        }
      }
    }
  }

  // 窃取成功后返回空闲内存
  if(steal_success) {
    r = freelist_pop(cpu_id);
  }
  release(&kmems[cpu_id].lock);

  if(r) {
    memset((char*)r, 5, PGSIZE); // fill with junk
    return (void*)r;
  } else {
    // 当所有的内存块都无空闲空间时
    return 0;
  }
}

/**
 * 封装对freelist的pop操作
 * 参数：cpu_id
*/
struct run* freelist_pop(int cpu_id) {
  struct run *r;
  r = kmems[cpu_id].freelist;

  // 如果存在空闲内存
  if (r) {
    kmems[cpu_id].freelist = r->next;
  }

  return r;
}

/**
 * 封装对freelist的push操作
 * 参数：cpu_id: 待插入链表对应的CPU_id，r：待插入的页面指针
*/
void freelist_push(int cpu_id, struct run* r) {
  if (r) {
    r->next = kmems[cpu_id].freelist;
    kmems[cpu_id].freelist = r;
  } else {
    // 如果待插入的页面不存在
    panic("Null run! Cannot push!");
  }
}
