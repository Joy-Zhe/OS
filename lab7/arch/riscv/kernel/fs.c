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
    memset(SFS.hash_list, NULL, sizeof(SFS.hash_list));
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
    newDir->block = kmalloc(sizeof(struct sfs_memory_block));
    newDir->block->block.block = entry;
    newDir->block->dirty = 1;
    newDir->block->is_inode = 0;
    newDir->block->blockno = getEmptyBlock();
    newDir->block->reclaim_count = 1;
    newDir->block->inode_link = newDir;
    List_add(SFS.inode_list,newDir);
    SFS.hash_list[newDir->block->blockno] = newDir;
    return newDir->block->blockno;
}

node loadBlock( uint32_t blockNo, bool isInode ){
    node temp = kmalloc(sizeof(struct node));
    temp->block = kmalloc(sizeof(struct sfs_memory_block));
    if( isInode ){
        temp->block->block.din = kmalloc(4096);
        disk_read(blockNo, temp->block->block.din);
    }else{
        temp->block->block.block = kmalloc(4096);
        disk_read( blockNo, temp->block->block.block);
    }
    temp->block->blockno = blockNo;
    temp->block->dirty = 0;
    temp->block->is_inode = isInode;
    temp->block->reclaim_count = 1;
    temp->block->inode_link = temp;
    temp->next = NULL;
    temp->prve = NULL;
    SFS.hash_list[blockNo] = temp;
    List_add( SFS.inode_list, temp );
    return temp;
}

void freeBlock( uint32_t blockno ){
    node t = SFS.hash_list[blockno];
    if( t == NULL ) return;
    if( t->block->is_inode ){
        for( int i = 0; i < SFS_NDIRECT; i++ ){
            freeBlock(t->block->block.din->direct[i]);
        }
        if( t->block->block.din->indirect != 0 ){
            node t1 = SFS.hash_list[t->block->block.din->indirect];
            uint32_t * t2 = t1->block->block.block;
            for( int j = 0; j < 1024; j++ ){
                freeBlock(t2[j]);
            }
        }
        t->block->reclaim_count--;
        if( t->block->dirty ){
            disk_write( blockno, t->block->block.din );
        }
        if( t->block->reclaim_count == 0 ){
            kfree(t->block->block.din);
            kfree(t->block);
            List_del(t);
            kfree(t);
            SFS.hash_list[blockno] = NULL;
        }
    }else{
        if( t->block->dirty ){
            disk_write( blockno, t->block->block.din );
        }
        kfree(t->block->block.din);
        kfree(t->block);
        List_del(t);
        kfree(t);
        SFS.hash_list[blockno] = NULL;
    }
}

int sfs_open(const char *path, uint32_t flags){
    if( isInit == 0 ){
        if( sfs_init() != 0 ) return -1;
    }
    if( path[0] != '/' ) return -1;
    if( flags != SFS_FLAG_READ && flags != SFS_FLAG_WRITE ) return -1;
    char filename[SFS_MAX_FILENAME_LEN + 1];
    int i = 1;
    if( SFS.hash_list[1] == NULL ){
        node newNode = kmalloc(sizeof(struct node));
        SFS.hash_list[1] = newNode;
        newNode->block = kmalloc(sizeof(struct sfs_memory_block));
        newNode->block->blockno = 1;
        newNode->block->reclaim_count = 1;
        newNode->block->is_inode = 1;
        newNode->block->dirty = 0;
        newNode->block->inode_link = newNode;
        newNode->block->block.din = kmalloc(4096);
        disk_read(1,newNode->block->block.din);
        List_add( SFS.inode_list, newNode );
    }else{
        node temp = SFS.hash_list[1];
        temp->block->reclaim_count++;
        List_del( temp );
        List_add( SFS.inode_list, temp );
    }
    node currentNode = SFS.hash_list[1], dir = NULL;
    int j = 0;
    bool flag = 1;
    while( flag ){
        if( path[i] != '/' && path[i] != '\0' ){
            filename[j++] = path[i];
        }else{
            if( path[i] == '\0' ) flag = 0;
            filename[j] = '\0';
            j = 0;
            dir = currentNode;
            int k = 0;
            int emptyEntry = -1;
            struct sfs_entry * entry;
            for( ; k < SFS_NDIRECT; k++ ){
                if( dir->block->block.din->direct[k] == 0 ){
                    emptyEntry = k;
                    continue;
                }
                currentNode = SFS.hash_list[dir->block->block.din->direct[k]];
                if( currentNode == NULL ) currentNode = loadBlock( dir->block->block.din->direct[k], 0 );
                entry = currentNode->block->block.block;
                if( strcmp( filename, entry->filename ) == 0 ){
                    currentNode = SFS.hash_list[entry->ino];
                    break;
                }
            }
            if( k != SFS_NDIRECT ){
                currentNode = SFS.hash_list[entry->ino];
                if( currentNode == NULL ) currentNode = loadBlock( entry->ino, 1 );
                else{
                    currentNode->block->reclaim_count++;
                    List_del(currentNode);
                    List_add(SFS.inode_list,currentNode);
                }
                if( currentNode->block->block.din->type == SFS_FILE ){
                    if( flag == 0 ) break;
                    node t = SFS.inode_list->next;
                    for( ; t != SFS.hash_list[1]; t = t->next ){
                        if( t->block->is_inode ) freeBlock( t->block->blockno );
                    }
                    return -1;
                }
            }else{
                int m = 0;
                if( dir->block->block.din->indirect != 0 ){
                    node t1 = SFS.hash_list[dir->block->block.din->indirect];
                    uint32_t * t2 = t1->block->block.block;
                    for( ; m < 1024; m++ ){
                        if( t2[m] == 0 ){
                            emptyEntry = SFS_NDIRECT + m;
                            continue;
                        }
                        currentNode = SFS.hash_list[t2[m]];
                        if( currentNode == NULL ) currentNode = loadBlock( t2[m], 0 );
                        else currentNode->block->reclaim_count++;
                        entry = currentNode->block->block.block;
                        if( strcmp( filename, entry->filename ) == 0 ){
                            currentNode = SFS.hash_list[entry->ino];
                            break;
                        }
                    }
                    if( m != 1024 ){
                        currentNode = SFS.hash_list[entry->ino];
                        if( currentNode == NULL ) currentNode = loadBlock( entry->ino, 1 );
                        else{
                            currentNode->block->reclaim_count++;
                            List_del(currentNode);
                            List_add(SFS.inode_list,currentNode);
                        }
                        if( currentNode->block->block.din->type == SFS_FILE ){
                            if( flag == 0 ) break;
                            node t = SFS.inode_list->next;
                            for( ; t != SFS.hash_list[1]; t = t->next ){
                                if( t->block->is_inode ) freeBlock( t->block->blockno );
                            }
                            return -1;
                        }
                    }
                }
                if( dir->block->block.din->indirect == 0 || m == 1024 ){
                    if( flags == SFS_FLAG_WRITE ){
                        if( emptyEntry == -1 ){
                            node t = SFS.inode_list->next;
                            for( ; t != SFS.hash_list[1]; t = t->next ){
                                if( t->block->is_inode ) freeBlock( t->block->blockno );
                            }
                            return -1;
                        }
                        entry = kmalloc(4096); 
                        entry->ino = getEmptyBlock();
                        int index = 0;
                        while( filename[index] != '\0' ){
                            entry->filename[index] = filename[index++];
                        }
                        entry->filename[index] = '\0';
                        if( emptyEntry < SFS_NDIRECT ){
                            dir->block->block.din->direct[emptyEntry] = newEntry(entry);
                        }else{
                            node t3 = SFS.hash_list[dir->block->block.din->indirect];
                            uint32_t * t4 = t3->block->block.block;
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
                            entry1->ino = dir->block->blockno;
                            entry1->filename[0]='.';
                            entry1->filename[1]='.';
                            entry1->filename[2]='\0';
                            din->direct[1] = newEntry(entry1);
                        }else{
                            din->type = SFS_FILE;
                            din->size = 0;
                        }
                        node newNode = kmalloc(sizeof(struct node));
                        newNode->block = kmalloc(sizeof(struct sfs_memory_block));
                        newNode->block->block.din = din;
                        newNode->block->blockno = entry->ino;
                        newNode->block->dirty = 1;
                        newNode->block->is_inode = 1;
                        newNode->block->reclaim_count = 1;
                        newNode->block->inode_link = newNode;
                        List_add(SFS.inode_list,newNode);
                        SFS.hash_list[newNode->block->blockno] = newNode;
                        currentNode = newNode;
                    }else{
                        node t = SFS.inode_list->next;
                        for( ; t != SFS.hash_list[1]; t = t->next ){
                            if( t->block->is_inode ) freeBlock( t->block->blockno );
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
            f->inode = currentNode->block->block.din;
            f->path = dir->block->block.din;
            f->flags = flags;
            f->off = 0;
            f->blockno = currentNode->block->blockno;
            f->pathBlockno = dir->block->blockno;
            current->fs.fds[fd] = f;
            return fd;
        }
    }
    return -1;
}

int sfs_close(int fd){
    if( isInit == 0 ){
        if( sfs_init() != 0 ) return -1;
    }
    node currentNode;
    node path;
    struct file * f = current->fs.fds[fd];
    currentNode = SFS.hash_list[current->fs.fds[fd]->blockno];
    path = SFS.hash_list[current->fs.fds[fd]->pathBlockno];
    while( path->block->blockno != 1 ){
        freeBlock( currentNode->block->blockno );
        currentNode = path;
        int i = 0;
        struct sfs_entry * entry;
        for( ; i < SFS_NDIRECT; i++ ){
            node temp = SFS.hash_list[path->block->block.din->direct[i]];
            if( temp == NULL ) loadBlock( path->block->block.din->direct[i], 0);
            entry = temp->block->block.block;
            if( strcmp( entry->filename,".." ) == 0 ){
                path = SFS.hash_list[entry->ino];
                break;
            }
        }
        if( i == SFS_NDIRECT ){
            if( path->block->block.din->indirect == 0 ) return -1;
            node temp = SFS.hash_list[path->block->block.din->indirect];
            uint32_t * t1 = temp->block->block.block;
            i = 0;
            for( ; i < 1024; i++ ){
                node t2 = SFS.hash_list[t1[i]];
                if( t2 == NULL ) loadBlock( t1[i], 0 );
                entry = t2->block->block.block;
                if( strcmp( entry->filename,".." ) == 0 ){
                    path = SFS.hash_list[entry->ino];
                    break;
                }   
            }
            if( i == 1024 ) return -1;
        }
    }
    freeBlock( path->block->blockno );
    if( SFS.super_dirty ){
        disk_write( 0, &(SFS.super) );
        disk_write( 2, SFS.freemap );
    }
}

int sfs_seek(int fd, int32_t off, int fromwhere){
    if( isInit == 0 ){
        if( sfs_init() != 0 ) return -1;
    }
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

int read_indirect_block(uint32_t indirect_block_number, uint32_t block_index) {
    node indirect_node = SFS.hash_list[indirect_block_number];
    if (indirect_node == NULL)
    {
        indirect_node = loadBlock(indirect_block_number, 0);
        SFS.hash_list[indirect_block_number] = indirect_node;
    }
    uint32_t *indirect_blocks = (uint32_t *)(indirect_node->block->block.block);
    return indirect_blocks[block_index];
}

int sfs_read(int fd, char *buf, uint32_t len) {
    struct file *f = current->fs.fds[fd];
    if (f == NULL || buf == NULL)
    {
        return -1;
    }
    struct sfs_inode *inode = f->inode;
    uint32_t offset = f->off;
    if (offset >= inode->size) // 文件指针超出文件大小
    {
        return 0; // 不读了
    }

    if (offset + len > inode->size) // 加上长度后超出文件大小
    {
        len = inode->size - offset; // 取短
    }

    uint32_t remain_len = len;
    while (remain_len > 0)
    {
        uint32_t block_index = offset / 4096;
        uint32_t block_offset = offset % 4096;
        uint32_t size = min(remain_len, 4096 - block_offset);
        uint32_t block_number;
        if (block_index < SFS_NDIRECT) // 直接索引
        {
            block_number = inode->direct[block_index];
        }
        else { // 间接索引
            block_number = read_indirect_block(inode->indirect, block_index - SFS_NDIRECT);
        }
        // 是否在buffer中
        node block_node = SFS.hash_list[block_number];
        if (block_node == NULL) // 不在buffer, load进buffer
        {
            block_node = loadBlock(block_number, 0);
            SFS.hash_list[block_number] = block_node;
        }

        char *temp = (char *)(block_node->block->block.block);
        memcpy(buf, temp + block_offset, size);
        buf += size;
        offset += size;
        remain_len -= size;
    }
    f->off += len;
    return len;
}

uint32_t get_indirect_block(struct sfs_inode *inode, uint32_t indirect_index) {
    if (inode->indirect == 0) { // 确保inode有一个间接块
        inode->indirect = getEmptyBlock();
        if (inode->indirect == 0) return 0; // 无法分配间接块
    }

    // 从缓存中获取间接块
    node indirect_block_node = SFS.hash_list[inode->indirect];
    if (indirect_block_node == NULL) {
        indirect_block_node = loadBlock(inode->indirect, 0);
        SFS.hash_list[inode->indirect] = indirect_block_node;
    }

    uint32_t *indirect_blocks = (uint32_t *)(indirect_block_node->block->block.block);
    
    if (indirect_blocks[indirect_index] == 0) { // 检查指定索引的块是否已分配
        indirect_blocks[indirect_index] = getEmptyBlock();
        if (indirect_blocks[indirect_index] == 0) return 0; // 无法分配新块
    }

    return indirect_blocks[indirect_index];
}


int sfs_write(int fd, char *buf, uint32_t len) {
    struct file *f = current->fs.fds[fd];
    if (f == NULL || buf == NULL || (f->flags & SFS_FLAG_WRITE) == 0) // 比read多个权限检测
    {
        return -1;
    }
    struct sfs_inode *inode = f->inode;
    uint32_t offset = f->off;
    uint32_t written_len = 0;
    while (written_len < len)
    {
        uint32_t block_index = offset / 4096;
        uint32_t block_offset = offset % 4096;
        uint32_t write_size = min(len - written_len, 4096 - block_offset);
        uint32_t block_number;
        if (block_index < SFS_NDIRECT) {
            if (inode->direct[block_index] == 0) // 写入新块
            {
                inode->direct[block_index] = getEmptyBlock(); // 分配新块 
                if (inode->direct[block_index] == 0) // 分配失败
                {
                    return -1;
                }
            }
            block_number = inode->direct[block_index];
        } 
        else {
            block_number = get_indirect_block(inode, block_index - SFS_NDIRECT);
            if (block_number == 0)
            {
                return -1;
            }
        }

        // buffer
        node block_node = SFS.hash_list[block_number];
        if (block_node == NULL) //buffer 里没有
        {
            block_node = loadBlock(block_number, 0);
        }
        char *temp = (char *)(block_node->block->block.block);
        memcpy(temp + block_offset, buf + written_len, write_size);
        block_node->block->dirty = 1; // set dirty
        offset += write_size;
        written_len += write_size;
    }

    if (offset > inode->size) // 写入之后变大，大小更改
    {
        inode->size = offset;
    }
    f->off = offset;

    return written_len;
}

int sfs_get_files(const char* path, char* files[]){
    if( isInit == 0 ){
        if( sfs_init() != 0 ) return -1;
    }
    if( path[0] != '/' ) return -1;
    char filename[SFS_MAX_FILENAME_LEN + 1];
    int i = 1;
    if( SFS.hash_list[1] == NULL ){
        node newNode = kmalloc(sizeof(struct node));
        newNode->block = kmalloc(sizeof(struct sfs_memory_block));
        SFS.hash_list[1] = newNode;
        newNode->block->blockno = 1;
        newNode->block->reclaim_count = 1;
        newNode->block->is_inode = 1;
        newNode->block->dirty = 0;
        newNode->block->inode_link = newNode;
        newNode->block->block.din = kmalloc(4096);
        disk_read(1,newNode->block->block.din);
        List_add( SFS.inode_list, newNode );
    }else{
        node temp = SFS.hash_list[1];
        temp->block->reclaim_count++;
        List_del( temp );
        List_add( SFS.inode_list, temp );
    }
    node currentNode = SFS.hash_list[1], dir = NULL;
    int j = 0;
    bool flag = 1;
    while( flag ){
        if( path[i] != '/' && path[i] != '\0' ){
            filename[j++] = path[i];
        }else{
            if( path[i] == '\0' ) flag = 0;
            filename[j] = '\0';
            j = 0;
            dir = currentNode;
            int k = 0;
            struct sfs_entry * entry;
            for( ; k < SFS_NDIRECT; k++ ){
                if( dir->block->block.din->direct[k] == 0 ){
                    continue;
                }
                currentNode = SFS.hash_list[dir->block->block.din->direct[k]];
                if( currentNode == NULL ) currentNode = loadBlock( dir->block->block.din->direct[k], 0 );
                entry = currentNode->block->block.block;
                if( strcmp( filename, entry->filename ) == 0 ){
                    currentNode = SFS.hash_list[entry->ino];
                    break;
                }
            }
            if( k != SFS_NDIRECT ){
                currentNode = SFS.hash_list[entry->ino];
                if( currentNode == NULL ) currentNode = loadBlock( entry->ino, 1 );
                else{
                    currentNode->block->reclaim_count++;
                    List_del(currentNode);
                    List_add(SFS.inode_list,currentNode);
                }
                if( currentNode->block->block.din->type == SFS_FILE ){
                    node t = SFS.inode_list->next;
                    for( ; t != SFS.hash_list[1]; t = t->next ){
                        if( t->block->is_inode ) freeBlock( t->block->blockno );
                    }
                    if( flag == 0 ) return 0;
                    else return -1;
                }
            }else{
                int m = 0;
                if( dir->block->block.din->indirect != 0 ){
                    node t1 = SFS.hash_list[dir->block->block.din->indirect];
                    uint32_t * t2 = t1->block->block.block;
                    for( ; m < 1024; m++ ){
                        if( t2[m] == 0 ){
                            continue;
                        }
                        currentNode = SFS.hash_list[t2[m]];
                        if( currentNode == NULL ) currentNode = loadBlock( t2[m], 0 );
                        else currentNode->block->reclaim_count++;
                        entry = currentNode->block->block.block;
                        if( strcmp( filename, entry->filename ) == 0 ){
                            currentNode = SFS.hash_list[entry->ino];
                            break;
                        }
                    }
                    if( m != 1024 ){
                        currentNode = SFS.hash_list[entry->ino];
                        if( currentNode == NULL ) currentNode = loadBlock( entry->ino, 1 );
                        else{
                            currentNode->block->reclaim_count++;
                            List_del(currentNode);
                            List_add(SFS.inode_list,currentNode);
                        }
                        if( currentNode->block->block.din->type == SFS_FILE ){
                            node t = SFS.inode_list->next;
                            for( ; t != SFS.hash_list[1]; t = t->next ){
                                if( t->block->is_inode ) freeBlock( t->block->blockno );
                            }
                            if( flag == 0 ) return 0;
                            else return -1;
                        }
                    }
                }
                if( dir->block->block.din->indirect == 0 || m == 1024 ){
                    node t = SFS.inode_list->next;
                    for( ; t != SFS.hash_list[1]; t = t->next ){
                        if( t->block->is_inode ) freeBlock( t->block->blockno );
                    }
                    return -1;
                }
            }
        }
        i++;
    }
    int f = 0;
    node temp;
    for( int k = 0; k < SFS_NDIRECT; k++ ){
        if( currentNode->block->block.din->direct[k] == 0 ) continue;
        temp = SFS.hash_list[currentNode->block->block.din->direct[k]];
        if( temp == NULL ) temp = loadBlock( currentNode->block->block.din->direct[k], 0 );
        struct sfs_entry * entry = temp->block->block.block;
        files[f++] = entry->filename;
    }
    if( currentNode->block->block.din->indirect != 0){
        temp = SFS.hash_list[currentNode->block->block.din->indirect];
        uint32_t * t = temp->block->block.block;
        struct sfs_entry * entry;
        for( int k = 0; k < 1024; k++ ){
            if( t[k] == 0 ) continue;
            temp = SFS.hash_list[t[k]];
            if( temp == NULL ) temp = loadBlock( t[k], 0 ); 
            entry = temp->block->block.block;
            files[f++] = entry->filename;
        }
    }
    temp = SFS.inode_list->next;
    for( ; temp != SFS.hash_list[1]; temp = temp->next ){
        if( temp->block->is_inode ) freeBlock( temp->block->blockno );
    }
    return f;
}