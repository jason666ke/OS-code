// Buffer cache.
//
// The buffer cache is a linked list of buf structures holding
// cached copies of disk block contents.  Caching disk blocks
// in memory reduces the number of disk reads and also provides
// a synchronization point for disk blocks used by multiple processes.
//
// Interface:
// * To get a buffer for a particular disk block, call bread.
// * After changing buffer data, call bwrite to write it to disk.
// * When done with the buffer, call brelse.
// * Do not use the buffer after calling brelse.
// * Only one process at a time can use a buffer,
//     so do not keep them longer than necessary.


#include "types.h"
#include "param.h"
#include "spinlock.h"
#include "sleeplock.h"
#include "riscv.h"
#include "defs.h"
#include "fs.h"
#include "buf.h"

#define NBUCKET 13    // 定义哈希桶个数


struct {
  // buf全局数组对应的全局锁
  struct spinlock lock;
  struct buf buf[NBUF];

  // Linked list of all buffers, through prev/next.
  // Sorted by how recently the buffer was used.
  // head.next is most recent, head.prev is least.
  
  // 使用哈希桶，放弃原来利用链表实现的方法
  // struct buf head;
} bcache;

struct bucket {
  // 哈希桶对应的桶级锁
  struct spinlock lock; 
  struct buf head;
} hashtable[NBUCKET];

// 实现哈希函数, 块号整除于哈希桶个数
int hash(uint dev, uint blockno) {
  return blockno % NBUCKET;
}

void
binit(void)
{
  struct buf *b;

  initlock(&bcache.lock, "bcache");

  // 初始化每一个缓存块的睡眠锁
  for(b = bcache.buf; b < bcache.buf+NBUF; b++){
    initsleeplock(&b->lock, "buffer");
  }

  // 将每一个缓存块分别加入对应哈希桶
  b = bcache.buf;
  for (int i = 0; i < NBUCKET; i++) {
    // 初始化哈希桶的桶级锁
    initlock(&hashtable[i].lock, "bcache_bucket");
    // 将每个缓存块放入对应的哈希桶
    // 前四个桶放三个，后九个桶放两个
    int j_max;
    if (i == 0 || i == 1 || i == 2 || i == 3) {
      j_max = 3;
    } else {
      j_max = 2;
    }
    for (int j = 0; j < j_max; j++) {
      // 更新缓存块块号
      b->blockno = i;  
      // 采用头插法将该缓存块放入对应块号的桶中
      b->next = hashtable[i].head.next;
      hashtable[i].head.next = b;
      b++;
    }
  }
}

// Look through buffer cache for block on device dev.
// If not found, allocate a buffer.
// In either case, return locked buffer.
static struct buf*
bget(uint dev, uint blockno)
{
  struct buf *b;

  // 找到当前blockno对应的哈希桶并用指针bucket指向该桶
  int bucket_index = hash(dev, blockno);
  struct bucket *bucket = hashtable + bucket_index;
  // 为这个桶加锁
  acquire(&bucket->lock);
  
  // Is the block already cached?
  for(b = bucket->head.next; b != 0; b = b->next){
    if(b->dev == dev && b->blockno == blockno){
      // 命中时，引用次数加一
      b->refcnt++;
      // 为该桶解锁
      release(&bucket->lock);
      // 对指定的缓存块加锁
      acquiresleep(&b->lock);
      return b;
    }
  }

  // Not cached.
  // Recycle the least recently used (LRU) unused buffer.
  
  // 用于记录最早空闲时间
  int min_time = 0x8fffffff;
  // 待替换的缓存块
  struct buf *replace_buf = 0;

  // 先在当前桶中查找
  for(b = bucket->head.next; b != 0; b = b->next){
    if(b->refcnt == 0 && b->timestamp < min_time) {
      replace_buf = b;
      min_time = b->timestamp;
    }
  }
  // 在当前桶中找到可替换的块
  if (replace_buf) {
    goto find;
  }

  // 如果在当前桶中没找到对应块，则全局搜索
  // 先为整个缓冲区加锁
  acquire(&bcache.lock);
  // 查找策略：在全局数组找到对应块后，对该块所属桶加锁
  // 将该空闲块从桶的链表取下，释放锁，加入当前桶的链表中
  // 边界条件：
  // 当找到对应快后，到对该块所属的桶加锁之间存在间歇期
  // 而此时如果当前块被别的进程所使用，则需要重新寻找
  refind:
  for (b = bcache.buf; b < bcache.buf + NBUF; b++) {
    if (b->refcnt == 0 && b->timestamp < min_time) {
      replace_buf = b;
      min_time = b->timestamp;
    }
  }
  if (replace_buf) {
    // 先从旧桶中取下
    int old_bucket_index = hash(replace_buf->dev, replace_buf->blockno);
    acquire(&hashtable[old_bucket_index].lock);

    // 如果此时在间歇期被别的进程锁引用
    if (replace_buf->refcnt != 0) {
      release(&hashtable[old_bucket_index].lock);
      goto refind;
    }

    // 该块可被替换
    // 找到该块在哈希表中的位置
    struct buf *pre = &hashtable[old_bucket_index].head;
    struct buf *p = hashtable[old_bucket_index].head.next;
    while (p != replace_buf) {
      pre = pre->next;
      p = p->next;
    }

    // 找到该块位置后，直接取下
    pre->next = p->next;
    release(&hashtable[old_bucket_index].lock);
    
    // 将该块加入到当前桶中
    replace_buf->next = hashtable[bucket_index].head.next;
    hashtable[bucket_index].head.next = replace_buf;

    release(&bcache.lock);
    goto find;
  } 
  else {
    panic("bget: no buffers");
  }
  

  // 找到可替换的块
  find:
  replace_buf->dev = dev;
  replace_buf->blockno = blockno;
  replace_buf->valid = 0;
  replace_buf->refcnt = 1;
  release(&bucket->lock);
  acquiresleep(&replace_buf->lock);
  return replace_buf;

}

// Return a locked buf with the contents of the indicated block.
struct buf*
bread(uint dev, uint blockno)
{
  struct buf *b;

  b = bget(dev, blockno);
  if(!b->valid) {
    virtio_disk_rw(b, 0);
    b->valid = 1;
  }
  return b;
}

// Write b's contents to disk.  Must be locked.
void
bwrite(struct buf *b)
{
  if(!holdingsleep(&b->lock))
    panic("bwrite");
  virtio_disk_rw(b, 1);
}

// Release a locked buffer.
// Move to the head of the most-recently-used list.
void
brelse(struct buf *b)
{
  if(!holdingsleep(&b->lock))
    panic("brelse");

  releasesleep(&b->lock);

  // 计算缓存块b对应的哈希桶号
  int bucket_index = hash(b->dev, b->blockno);

  // 为对应的哈希桶加锁
  acquire(&hashtable[bucket_index].lock);
  b->refcnt--;
  // 如果当前缓存块没有被任何进程拥有，则记录其空闲时间
  if (b->refcnt == 0) {
    // no one is waiting for it.
    b->timestamp = ticks;
  }
  release(&hashtable[bucket_index].lock);
}

void
bpin(struct buf *b) {
  // 对该块所属的哈希桶加锁
  int bucket_index = hash(b->dev, b->blockno);
  acquire(&hashtable[bucket_index].lock);
  b->refcnt++;
  release(&hashtable[bucket_index].lock);
}

void
bunpin(struct buf *b) {
  // 对该块所属的哈希桶加锁
  int bucket_index = hash(b->dev, b->blockno);
  acquire(&hashtable[bucket_index].lock);
  b->refcnt--;
  release(&hashtable[bucket_index].lock);
}


