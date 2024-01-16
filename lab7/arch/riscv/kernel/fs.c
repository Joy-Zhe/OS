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

int strcmp(const char *a, const char *b) {
  while (*a && *b) {
    if (*a < *b)
      return -1;
    if (*a > *b)
      return 1;
    a++;
    b++;
  }
  if (*a && !*b)
    return 1;
  if (*b && !*a)
    return -1;
  return 0;
}

struct sfs_fs SFS = {{0},NULL};
bool isInit = 0;

int sfs_init(){
    struct sfs_super * data = kmalloc(4096);
    if( data == NULL ) return -1;
    disk_read(0,data);
    if( data->magic != SFS_MAGIC ) return -1;
    SFS.super = *data;
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

int getEmptyBlock(){
    if( SFS.super.unused_blocks == 0 ) return -1;
    SFS.super_dirty = 1;
    SFS.super.unused_blocks--;
    SFS.super.blocks++;
    bitmap * temp = SFS.freemap;
    for( int i = 0; i < 512; i++ ){
        if( temp[i] != 0b11111111 ){
            for( int j = 0; j < 8; j++ ){
                if( (temp[i] >> j) & 0b1 == 0 ){
                    temp[i] = temp[i] | (1 << j);
                    return i*8+j;
                }
            }
        }
    }
}

node bufferFind( uint32_t blockNo ){
    node temp = SFS.inode_list;
    for( ; temp != NULL; temp = temp->next ){
        if( temp->block.blockno == blockNo ){     
            if( temp->block.is_inode ) temp->block.reclaim_count++;
            return temp;
        }
    }
    return NULL;
}

node loadBlock( uint32_t blockNo, bool isInode ){
    node temp = kmalloc(sizeof(struct node));
    if( isInode ){
        temp->block.block.din = kmalloc(4096);
        disk_read( blockNo, temp->block.block.din);
    }else{
        temp->block.block.block = kmalloc(4096);
        disk_read( blockNo, temp->block.block.block);
    }
    temp->block.blockno = blockNo;
    temp->block.dirty = 0;
    temp->block.is_inode = isInode;
    temp->block.reclaim_count = 1;
    temp->block.inode_link = temp;
    temp->next = NULL;
    temp->prve = NULL;
    return temp;
}

int sfs_open(const char *path, uint32_t flags){
    if( path[0] != '/' ) return -1;
    char filename[SFS_MAX_FILENAME_LEN + 1];
    int i = 1;
    if( SFS.hash_list[1] == NULL ){
        node newNode = kmalloc(sizeof(struct node));
        SFS.hash_list[1] = newNode;
        newNode->block.blockno = 1;
        newNode->block.reclaim_count = 1;
        newNode->block.is_inode = 1;
        newNode->block.dirty = 0;
        newNode->block.inode_link = newNode;
        newNode->block.block.din = kmalloc(4096);
        disk_read(1,newNode->block.block.din);
        List_add( SFS.inode_list, newNode );
    }else{
        node temp = SFS.hash_list[1];
        temp->block.reclaim_count++;
    }
    node currentNode = SFS.hash_list[1], dir = NULL;
    int j = 0;
    while( path[i] != '\0'){
        if( path[i] != '/' ){
            filename[j++] = path[i];
        }else{
            filename[j] = '\0';
            j = 0;
            dir = currentNode;
            int i = 0;
            struct sfs_entry * entry;
            for( ; i < SFS_NDIRECT; i++ ){
                currentNode = bufferFind( dir->block.block.din->direct[i] );
                if( currentNode == NULL ) currentNode = loadBlock( dir->block.block.din->direct[i], 0 );
                entry = currentNode->block.block.block;
                if( strcmp( filename, entry->filename ) != 0 ){
                    kfree( entry );
                    kfree( currentNode );
                }else{
                    break;
                }
            }
            if( i != SFS_NDIRECT ){
                currentNode = bufferFind(entry->ino);
                if( currentNode == NULL ) currentNode = loadBlock( entry->ino, 1 );
                if( currentNode->block.block.din->type == SFS_FILE ) return -1;
            }else{
                
            }
        }
        i++;
    }
}

int sfs_close(int fd);

int sfs_seek(int fd, int32_t off, int fromwhere);

int sfs_read(int fd, char *buf, uint32_t len);

int sfs_write(int fd, char *buf, uint32_t len);

int sfs_get_files(const char* path, char* files[]);