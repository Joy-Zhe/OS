#include "fs.h"
#include "buf.h"
#include "defs.h"
#include "slub.h"
#include "task_manager.h"
#include "virtio.h"
#include "vm.h"
#include "mm.h"

// --------------------------------------------------
// ----------- read and write interface -------------

void disk_op(int blockno, uint8_t *data, bool write) {
    struct buf b;
    b.disk = 0;
    b.blockno = blockno;
    b.data = (uint8_t *)PHYSICAL_ADDR(data);
    virtio_disk_rw((struct buf *)(PHYSICAL_ADDR(&b)), write);
}

#define disk_read(blockno, data) disk_op((blockno), (data), 0)
#define disk_write(blockno, data) disk_op((blockno), (data), 1)

// -------------------------------------------------
// ------------------ your code --------------------

struct sfs_fs SFS = {{0},NULL};
bool isInit = 0;

int sfs_init(){
    uint8_t * data = kmalloc(4096);
    disk_read(0,data);
    uint32_t * temp = data;
    if( temp[0] != SFS_MAGIC ) return -1;
    SFS.super.magic = temp[0];
    SFS.super.blocks = temp[1];
    SFS.super.unused_blocks = temp[2];
    if( SFS.freemap == NULL ) SFS.freemap = kmalloc(4096);
    if( SFS.freemap == NULL ) return -1;
    disk_read(2,SFS.freemap);
    SFS.super_dirty = 0;
    SFS.inode_list->next = NULL;
    SFS.inode_list->prve =NULL;
    kfree(data);
    isInit = 1;
    return 0;
}

int sfs_open(const char *path, uint32_t flags);

int sfs_close(int fd);

int sfs_seek(int fd, int32_t off, int fromwhere);

int sfs_read(int fd, char *buf, uint32_t len);

int sfs_write(int fd, char *buf, uint32_t len);

int sfs_get_files(const char* path, char* files[]);