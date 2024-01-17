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
    kfree(data);
    if( SFS.freemap == NULL ) SFS.freemap = kmalloc(4096);
    if( SFS.freemap == NULL ) return -1;
    disk_read(2,SFS.freemap);
    SFS.super_dirty = 0;
    SFS.inode_list->next = NULL;
    SFS.inode_list->prve =NULL;
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

void freeBlock( uint32_t blockno );

int newEntry( struct sfs_entry * entry ){
    node newDir = kmalloc(sizeof(struct node));
    newDir->block.block.block = entry;
    newDir->block.dirty = 1;
    newDir->block.is_inode = 0;
    newDir->block.blockno = getEmptyBlock();
    newDir->block.reclaim_count = 1;
    newDir->block.inode_link = newDir;
    List_add(SFS.inode_list,newDir);
    SFS.hash_list[newDir->block.blockno] = newDir;
    return newDir->block.blockno;
}

node loadBlock( uint32_t blockNo, bool isInode ){
    node temp = kmalloc(sizeof(struct node));
    if( isInode ){
        temp->block.block.din = kmalloc(4096);
        disk_read(blockNo, temp->block.block.din);
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
    SFS.hash_list[blockNo] = temp;
    List_add( SFS.inode_list, temp );
    return temp;
}

void freeBlock( uint32_t blockno ){
    node t = SFS.hash_list[blockno];
    if( t == NULL ) return;
    if( t->block.is_inode ){
        for( int i = 0; i < SFS_NDIRECT; i++ ){
            freeBlock(t->block.block.din->direct[i]);
        }
        if( t->block.block.din->indirect != 0 ){
            node t1 = SFS.hash_list[t->block.block.din->indirect];
            uint32_t * t2 = t1->block.block.block;
            for( int j = 0; j < 1024; j++ ){
                freeBlock(t2[j]);
            }
        }
        t->block.reclaim_count--;
        if( t->block.reclaim_count == 0 ){
            if( t->block.dirty ){
                disk_write( blockno, t->block.block.din );
            }
            kfree(t->block.block.din);
            List_del(t);
            kfree(t);
            SFS.hash_list[blockno] = NULL;
        }
    }else{
        if( t->block.dirty ){
            disk_write( blockno, t->block.block.din );
        }
        kfree(t->block.block.din);
        List_del(t);
        kfree(t);
        SFS.hash_list[blockno] = NULL;
    }
}

int sfs_open(const char *path, uint32_t flags){
    if( path[0] != '/' ) return -1;
    if( flags != SFS_FLAG_READ && flags != SFS_FLAG_WRITE ) return -1;
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
        List_del( temp );
        List_add( SFS.inode_list, temp );
    }
    node currentNode = SFS.hash_list[1], dir = NULL;
    int j = 0;
    bool flag = 1;
    while( flags ){
        if( path[i] != '/' && path[i] != '\0' ){
            filename[j++] = path[i];
        }else{
            if( path[i] == '\0' ) flags = 0;
            filename[j] = '\0';
            j = 0;
            dir = currentNode;
            int k = 0;
            int emptyEntry = -1;
            struct sfs_entry * entry;
            for( ; k < SFS_NDIRECT; k++ ){
                if( dir->block.block.din->direct[k] == 0 ){
                    emptyEntry = k;
                    continue;
                }
                currentNode = SFS.hash_list[dir->block.block.din->direct[k]];
                if( currentNode == NULL ) currentNode = loadBlock( dir->block.block.din->direct[k], 0 );
                entry = currentNode->block.block.block;
                if( strcmp( filename, entry->filename ) == 0 ){
                    currentNode = SFS.hash_list[entry->ino];
                    break;
                }
            }
            if( k != SFS_NDIRECT ){
                currentNode = SFS.hash_list[entry->ino];
                if( currentNode == NULL ) currentNode = loadBlock( entry->ino, 1 );
                else{
                    currentNode->block.reclaim_count++;
                    List_del(currentNode);
                    List_add(SFS.inode_list,currentNode);
                }
                if( currentNode->block.block.din->type == SFS_FILE ){
                    if( flag == 0 ) break;
                    node t = SFS.inode_list->next;
                    for( ; t != SFS.hash_list[1]; t = t->next ){
                        if( t->block.is_inode ) freeBlock( t->block.blockno );
                    }
                    return -1;
                }
            }else{
                int m = 0;
                if( dir->block.block.din->indirect != 0 ){
                    node t1 = SFS.hash_list[dir->block.block.din->indirect];
                    uint32_t * t2 = t1->block.block.block;
                    for( ; m < 1024; m++ ){
                        if( t2[m] == 0 ){
                            emptyEntry = SFS_NDIRECT + m;
                            continue;
                        }
                        currentNode = SFS.hash_list[t2[m]];
                        if( currentNode == NULL ) currentNode = loadBlock( t2[m], 0 );
                        else currentNode->block.reclaim_count++;
                        entry = currentNode->block.block.block;
                        if( strcmp( filename, entry->filename ) == 0 ){
                            currentNode = SFS.hash_list[entry->ino];
                            break;
                        }
                    }
                    if( m != 1024 ){
                        currentNode = SFS.hash_list[entry->ino];
                        if( currentNode == NULL ) currentNode = loadBlock( entry->ino, 1 );
                        else{
                            currentNode->block.reclaim_count++;
                            List_del(currentNode);
                            List_add(SFS.inode_list,currentNode);
                        }
                        if( currentNode->block.block.din->type == SFS_FILE ){
                            if( flag == 0 ) break;
                            node t = SFS.inode_list->next;
                            for( ; t != SFS.hash_list[1]; t = t->next ){
                                if( t->block.is_inode ) freeBlock( t->block.blockno );
                            }
                            return -1;
                        }
                    }
                }
                if( dir->block.block.din->indirect == 0 || m == 1024 ){
                    if( flags == SFS_FLAG_WRITE ){
                        entry = kmalloc(4096); 
                        entry->ino = getEmptyBlock();
                        int index = 0;
                        while( filename[index] != '\0' ){
                            entry->filename[index] = filename[index++];
                        }
                        entry->filename[index] = '\0';
                        if( emptyEntry < SFS_NDIRECT ){
                            dir->block.block.din->direct[emptyEntry] = newEntry(entry);
                        }else{
                            node t3 = SFS.hash_list[dir->block.block.din->indirect];
                            uint32_t * t4 = t3->block.block.block;
                            t4[emptyEntry-SFS_NDIRECT] = newEntry(entry); 
                        }
                        struct sfs_inode * din = kmalloc(4096);
                        din->links = 1;
                        din->blocks = 1;
                        din->indirect = 0;
                        if( flag != 0 ){
                            din->type = SFS_DIRECTORY;
                            din->size = 2*sizeof(struct sfs_entry);
                            struct sfs_entry * entry1 = kmalloc(4096);
                            entry1->ino = entry->ino;
                            entry1->filename[0]='.';
                            entry1->filename[1]='\0';
                            din->direct[0] = newEntry(entry1);
                            entry1 = kmalloc(4096);
                            entry1->ino = dir->block.blockno;
                            entry1->filename[0]='.';
                            entry1->filename[1]='.';
                            entry1->filename[2]='\0';
                            din->direct[1] = newEntry(entry1);
                        }else{
                            din->type = SFS_FILE;
                            din->size = 0;
                        }
                        node newNode = kmalloc(sizeof(struct node));
                        newNode->block.block.din = din;
                        newNode->block.blockno = entry->ino;
                        newNode->block.dirty = 1;
                        newNode->block.is_inode = 1;
                        newNode->block.reclaim_count = 1;
                        newNode->block.inode_link = newNode;
                        List_add(SFS.inode_list,newNode);
                        SFS.hash_list[newNode->block.blockno] = newNode;
                        currentNode = newNode;
                    }else{
                        node t = SFS.inode_list->next;
                        for( ; t != SFS.hash_list[1]; t = t->next ){
                            if( t->block.is_inode ) freeBlock( t->block.blockno );
                        }
                        return -1;
                    }
                }
            }
        }
        i++;
    }
    for( int fd = 0; fd < 16; fd ++ ){
        if(current->fs.fds[fd] == NULL ){
            struct file * f = kmalloc(sizeof(struct file));
            f->inode = currentNode->block.block.din;
            f->path = dir->block.block.din;
            f->flags = flags;
            f->off = 0;
            current->fs.fds[fd] = f;
            return fd;
        }
    }
    return -1;
}

int sfs_close(int fd);

int sfs_seek(int fd, int32_t off, int fromwhere){
    struct file * f = current->fs.fds[fd];
    uint64_t offtemp = -1;
    switch(fromwhere){
        case SEEK_SET:{
            offtemp = off;
            break;
        }
        case SEEK_CUR:{
            offtemp = f->off + off;
            break;
        }
        case SEEK_END:{
            offtemp = f->inode->size-off;
            break;
        }
        default:
            break;
    }
    if( offtemp < 0 || offtemp > f->inode->size ) return -1;
    f->off = offtemp;
    return 0;
}

int sfs_read(int fd, char *buf, uint32_t len);

int sfs_write(int fd, char *buf, uint32_t len);

int sfs_get_files(const char* path, char* files[]);