#include "newfs.h"

extern struct newfs_super super;
extern struct custom_options newfs_options;


/**
 * @brief 获取文件名
 * 
 * @param path 
 * @return char* 
 */
char * get_name(const char * path){
    char ch = '/';
    char *q = strrchr(path, ch) + 1;
    return q;
}
/**
 * @brief 计算路径的层级
 * exm: /av/c/d/f
 * -> lvl = 4
 * @param path 
 * @return int 
 */
int calc_lvl(const char * path){
    char * str = path;
    int lvl = 0;
    if (strcmp(path, "/") == 0) {
        return lvl;
    }
    while (*str != NULL){
        if (*str == '/') {
            lvl++;
        }
        str++;
    }
    return lvl;
}
/**
 * @brief 驱动读
 * 
 * @param offset 
 * @param out_content 
 * @param size 
 * @return int 
 */
int newfs_driver_read(int offset, uint8_t *out_content, int size) {
    int offset_align = ROUND_DOWN(offset, super.sz_io);
    int bias = offset - offset_align;
    int size_aligned = ROUND_UP((size + bias), super.sz_io);

    uint8_t *temp_content = (uint8_t *)malloc(size_aligned);
    uint8_t *cur = temp_content;

    ddriver_seek(super.fd, offset_align, 0);
    while (size_aligned != 0) {
        ddriver_read(super.fd, cur, super.sz_io);
        cur += super.sz_io;
        size_aligned -= super.sz_io;
    }
    memcpy(out_content, temp_content + bias, size);
    free(temp_content);
    return 0;
}
/**
 * @brief 驱动写
 * 
 * @param offset 
 * @param in_content 
 * @param size 
 * @return int 
 */
int newfs_driver_write(int offset, uint8_t *in_content, int size) {
    int offset_aligned = ROUND_DOWN(offset, super.sz_io);
    int bias = offset - offset_aligned;
    int size_aligned = ROUND_UP((size + bias), super.sz_io);

    uint8_t* temp_content = (uint8_t*)malloc(size_aligned);
    uint8_t* cur = temp_content;
    newfs_driver_read(offset_aligned, temp_content, size_aligned);
    memcpy(temp_content + bias, in_content, size);

    ddriver_seek(super.fd, offset_aligned, 0);
    while(size_aligned != 0) {
        ddriver_write(super.fd, (char*)cur, super.sz_io);
        cur += super.sz_io;
        size_aligned -= super.sz_io;
    }
    free(temp_content);
    return 0;
}
/**
 * @brief 将denry插入到inode中，采用头插法
 * 
 * @param inode 
 * @param dentry 
 * @return int 
 */
int newfs_alloc_dentry(struct newfs_inode* inode, struct newfs_dentry* dentry) {
    if (inode->dentrys == NULL) {
        inode->dentrys = dentry;
    }
    else {
        dentry->brother = inode->dentrys;
        inode->dentrys = dentry;
    }
    inode->dir_cnt++;
    return inode->dir_cnt;
}
/**
 * @brief 将dentry从inode的dentrys中取出
 * 
 * @param inode 一个目录的索引结点
 * @param dentry 该目录下的一个目录项
 * @return int 
 */
int newfs_drop_dentry(struct newfs_inode * inode, struct newfs_dentry * dentry) {
    int is_find = 0;
    struct newfs_dentry* dentry_cursor;
    dentry_cursor = inode->dentrys;
    
    if (dentry_cursor == dentry) {
        inode->dentrys = dentry->brother;
        is_find = 1;
    }
    else {
        while (dentry_cursor)
        {
            if (dentry_cursor->brother == dentry) {
                dentry_cursor->brother = dentry->brother;
                is_find = 1;
                break;
            }
            dentry_cursor = dentry_cursor->brother;
        }
    }
    if (!is_find) {
        return -ENOENT;
    }
    inode->dir_cnt--;
    return inode->dir_cnt;
}

/**
 * @brief 分配一个inode，占用位图
 * 
 * @param dentry 该dentry指向分配的inode
 * @return newfs_inode
 */
struct newfs_inode* newfs_alloc_inode(struct newfs_dentry * dentry) {
    struct newfs_inode* inode;
    int byte_cursor = 0; 
    int bit_cursor  = 0; 
    int ino_cursor  = 0;
    int is_find_free_entry = 0;
    /* 检查位图是否有空位 */
    for (byte_cursor = 0; byte_cursor < super.sz_logit; byte_cursor++)
    {
        for (bit_cursor = 0; bit_cursor < UINT8_BITS; bit_cursor++) {
            if((super.inodes_bitmap[byte_cursor] & (0x1 << bit_cursor)) == 0) {    
                                                      /* 当前ino_cursor位置空闲 */
                super.inodes_bitmap[byte_cursor] |= (0x1 << bit_cursor);
                is_find_free_entry = TRUE;           
                break;
            }
            ino_cursor++;
        }
        if (is_find_free_entry) {
            break;
        }
    }

    if (!is_find_free_entry || ino_cursor >= super.inode_blks * super.blk_per_inode){
        printf("no space");
        return NULL;
    }
        

    inode = (struct newfs_inode*)malloc(sizeof(struct newfs_inode));
    inode->ino  = ino_cursor; 
    inode->size = 0;
                                                      /* dentry指向inode */
    dentry->inode = inode;
    dentry->ino   = inode->ino;
                                                      /* inode指回dentry */
    inode->dentry = dentry;
    
    inode->dir_cnt = 0;
    inode->dentrys = NULL;
    inode->data = NULL;
    // if (inode->dentry->ftype == SFS_REG_FILE) {
    //    inode->data = (uint8_t *)malloc(SFS_BLKS_SZ(SFS_DATA_PER_FILE));
    // }

    return inode;
}

/**
 * @brief 将内存inode及其下方结构全部刷回磁盘
 * 
 * @param inode 
 * @return int 
 */
int newfs_sync_inode(struct newfs_inode * inode) {
    struct newfs_inode_d  inode_d;
    struct newfs_dentry*  dentry_cursor;
    struct newfs_dentry_d dentry_d;
    int ino             = inode->ino;
    inode_d.ino         = ino;
    inode_d.size        = inode->size;
    inode_d.ftype       = inode->dentry->ftype;
    inode_d.dir_cnt     = inode->dir_cnt;
    int origin_offset;//数据偏移
    int inode_offset = super.inode_offset + (ino % super.blk_per_inode) * sizeof(struct newfs_inode_d) + (ino / super.blk_per_inode) * super.sz_logit; //相对于索引区起使地址的偏移

    if(inode->dentry->ftype == NEWFS_DIR){
        int data_no = 0;
        dentry_cursor = inode->dentrys;
        if (dentry_cursor != NULL){
            inode_d.block_pointer[data_no++] = newfs_search_data_bitmap();
            origin_offset = super.data_offset + inode_d.block_pointer[0] * super.sz_logit;
        }
        int offset = origin_offset;
        while (dentry_cursor != NULL) {
            memcpy(dentry_d.fname, dentry_cursor->name, MAX_NAME_LEN);
            dentry_d.ftype = dentry_cursor->ftype;
            dentry_d.ino = dentry_cursor->ino;
            if (newfs_driver_write(offset, (uint8_t *)&dentry_d, sizeof(struct newfs_dentry_d)) != 0) {
                return -EIO;                     
            }

            if (dentry_cursor->inode != NULL) {
                newfs_sync_inode(dentry_cursor->inode);
            }

            dentry_cursor = dentry_cursor->brother;
            offset += sizeof(struct newfs_dentry_d);
            if(offset >= origin_offset + super.sz_logit){
                inode_d.block_pointer[data_no++] = newfs_search_data_bitmap();
                origin_offset = super.data_offset + inode_d.block_pointer[data_no] * super.sz_logit;
                offset = origin_offset;
            }
        }
        inode_d.block_pointer[data_no] = -1;
    }else if(inode->dentry->ftype == NEWFS_REG_FILE){
        int size = inode->size;
        uint8_t* data_cursor = inode->data;
        int data_no = 0;
        while(size > 0){
            if((inode_d.block_pointer[data_no] = newfs_search_data_bitmap(data_cursor, super.sz_logit)) < 0){//一次写入一个逻辑块，返回相应的数据块编号
                return -ENOSPC;
            }
            int data_offset = super.data_offset + inode_d.block_pointer[data_no] * super.sz_logit;
            if (newfs_driver_write(data_offset, (uint8_t *)data_cursor, super.sz_logit) != 0) {
                return -EIO;
            }
            data_no++;
            size -= super.sz_logit;
            data_cursor += super.sz_logit;
            if(data_no > NEWFS_DATA_BLK){
                printf("file too big and it will be truncate");
                break;
            }
        }
        inode_d.block_pointer[data_no] = -1;//指示末尾
    }
    /* 在数据块链接完善以后才能写inode本身 */
    if (newfs_driver_write(inode_offset, (uint8_t *)&inode_d, sizeof(struct newfs_inode_d)) != 0){
        return -EIO;
    }
    return 0;
}
/*
 * 寻找空的数据块‘
*/
int newfs_search_data_bitmap(){
    int byte_cursor = 0; 
    int bit_cursor  = 0; 
    int data_no_cursor  = 0;
    int is_find_free_entry = 0;
    /* 检查位图是否有空位 */
    for (byte_cursor = 0; byte_cursor < super.sz_logit; byte_cursor++)
    {
        for (bit_cursor = 0; bit_cursor < UINT8_BITS; bit_cursor++) {
            if((super.data_bitmap[byte_cursor] & (0x1 << bit_cursor)) == 0) {    
                                                      /* 当前ino_cursor位置空闲 */
                super.data_bitmap[byte_cursor] |= (0x1 << bit_cursor);
                is_find_free_entry = TRUE;           
                break;
            }
            data_no_cursor++;
        }
        if (is_find_free_entry) {
            break;
        }
    }
    if(!is_find_free_entry || data_no_cursor >= super.data_blks){
        return -ENOSPC;
    }
    return data_no_cursor;
}
/*
 * 释放数据块索引‘
*/
int newfs_release_data_bitmap(int data_no){
    if(data_no >= super.data_blks){
        return -1;
    }
    int byte_cursor = data_no / 8; 
    int bit_cursor  = data_no % 8;                                                     
    super.data_bitmap[byte_cursor] &= (uint8_t)(~(0x1 << bit_cursor));
    return 0;
}
/**
 * @brief 删除内存中的一个inode
 * Case 1: Reg File
 * 
 *                  Inode
 *                /      \
 *            Dentry -> Dentry (Reg Dentry)
 *                       |
 *                      Inode  (Reg File)
 *  
 *  1) Step 1. Erase Bitmap     
 *  2) Step 2. Free Inode                      (Function of sfs_drop_inode)
 * ------------------------------------------------------------------------
 *  3) *Setp 3. Free Dentry belonging to Inode (Outsider)
 * ========================================================================
 * Case 2: Dir
 *                  Inode
 *                /      \
 *            Dentry -> Dentry (Dir Dentry)
 *                       |
 *                      Inode  (Dir)
 *                    /     \
 *                Dentry -> Dentry
 * 
 *   Recursive
 * @param inode 
 * @return int 
 */
int newfs_drop_inode(struct newfs_inode* inode){
    struct newfs_dentry* dentry_cursor;
    struct newfs_dentry* dentry_to_free;
    struct newfs_inode* inode_cursor;

    int byte_cursor = 0;
    int bit_cursor = 0;


    if (inode == super.root_dentry->inode){
        return -1;
    }
    // 释放索引位图
    byte_cursor = inode->ino / 8;
    bit_cursor = inode->ino % 8;
    super.inodes_bitmap[byte_cursor] &= (uint8_t)(~(0x1 << bit_cursor));
     // 无需释放数据,位图只在sync时指定
    if(inode->dentry->ftype == NEWFS_DIR){
        dentry_cursor = inode->dentrys;
        while(dentry_cursor){
            inode_cursor = dentry_cursor->inode;
            newfs_drop_inode(inode_cursor);
            newfs_drop_dentry(inode, dentry_cursor);
            dentry_to_free = dentry_cursor;
            dentry_cursor = dentry_cursor->brother;
            free(dentry_to_free);
        }   
    }else{
        // 只需释放数据
        if (inode->data)
            free(inode->data);
       
    }
     // 释放inode
    free(inode);
    return 0;
}

/**
 * @brief 
 * 
 * @param dentry dentry指向ino，读取该inode
 * @param ino inode唯一编号
 * @return struct sfs_inode* 
 */
struct newfs_inode* newfs_read_inode(struct newfs_dentry * dentry, int ino) {
    struct newfs_inode* inode = (struct newfs_inode*)malloc(sizeof(struct newfs_inode));
    struct newfs_inode_d inode_d;
    struct newfs_dentry* sub_dentry;
    struct newfs_dentry_d dentry_d;
    int    dir_cnt = 0, i;
    /* 从磁盘读索引结点 */
    int inode_offset = super.inode_offset + ino * sizeof(struct newfs_inode_d);// todo
    if (newfs_driver_read(inode_offset, (uint8_t *)&inode_d, 
                        sizeof(struct newfs_inode_d)) != 0) {
        return NULL;                    
    }
    inode->dir_cnt = 0;
    inode->ino = inode_d.ino;
    inode->size = inode_d.size;
    inode->dentry = dentry;
    inode->dentrys = NULL;
    dentry->inode = inode;

    if (dentry->ftype == NEWFS_DIR){
        
        dir_cnt = inode_d.dir_cnt;
        int block_pointer_no = 0;
        int data_offset = inode_d.block_pointer[block_pointer_no++];
        data_offset = super.data_offset + data_offset * super.sz_logit;
        int block_offset = 0;
        for (i = 0; i < dir_cnt; i++) { 
            if(block_offset >= super.sz_logit && block_pointer_no <= NEWFS_DATA_BLK) {
                data_offset = inode_d.block_pointer[block_pointer_no++];
                data_offset = super.data_offset + data_offset * super.sz_logit;
                block_offset = 0;
            }         
            if (newfs_driver_read(data_offset + block_offset, (uint8_t *)&dentry_d, sizeof(struct newfs_dentry_d)) != 0){
                return NULL;
            } 
            block_offset += sizeof(struct newfs_dentry_d);   
            sub_dentry = new_dentry(dentry_d.fname, dentry_d.ftype);
            sub_dentry->parent = dentry;
            sub_dentry->ino = dentry_d.ino;
            newfs_alloc_dentry(inode, sub_dentry);
        } 
    }else if(dentry->ftype == NEWFS_REG_FILE){
        inode->data = (uint8_t *)malloc(sizeof(uint8_t) * inode_d.size);
        uint8_t* data_cursor = inode->data;
        int block_pointer_no = 0;
        int data_no = -1;
        int data_offset;
        while((data_no = inode_d.block_pointer[block_pointer_no++]) != -1){
            data_offset = super.data_offset + data_no * super.sz_logit;
            int size = inode_d.size;
            if (size > super.sz_logit && newfs_driver_read(data_offset, data_cursor, super.sz_logit) != 0){
                printf("IO error");
                return NULL;
            }else if(newfs_driver_read(data_offset, data_cursor, size) != 0){
                printf("IO error");
                return NULL;
            }
            size -= super.sz_logit;
            data_cursor += super.sz_logit;
        }
        
    }
    return inode;
}

/**
 * @brief 
 * 
 * @param inode 
 * @param name
 * @return struct sfs_dentry* 
 */
struct newfs_dentry* newfs_get_dentry(struct newfs_inode * inode, int dir) {
    struct newfs_dentry* dentry_cursor = inode->dentrys;
    int cnt = 0;
    while (dentry_cursor)
    {
        if (dir == cnt) {
            return dentry_cursor;
        }
        cnt++;
        dentry_cursor = dentry_cursor->brother;
    }
    return NULL;
}
/**
 * @brief 查找文件或目录
 * path: /qwe/ad  total_lvl = 2,
 *      1) find /'s inode       lvl = 1
 *      2) find qwe's dentry 
 *      3) find qwe's inode     lvl = 2
 *      4) find ad's dentry
 *
 * path: /qwe     total_lvl = 1,
 *      1) find /'s inode       lvl = 1
 *      2) find qwe's dentry
 *  
 * 
 * 如果能查找到，返回该目录项
 * 如果查找不到，返回的是上一个有效的路径
 * 
 * path: /a/b/c
 *      1) find /'s inode     lvl = 1
 *      2) find a's dentry 
 *      3) find a's inode     lvl = 2
 *      4) find b's dentry    如果此时找不到了，is_find=FALSE且返回的是a的inode对应的dentry
 * 
 * @param path 
 * @return struct sfs_dentry* 
 */
struct newfs_dentry* newfs_lookup(const char * path, int* is_find, int* is_root) {
    struct newfs_dentry* dentry_cursor = super.root_dentry;
    struct newfs_dentry* dentry_ret = NULL;
    struct newfs_inode* inode;
    int total_lvl = calc_lvl(path);
    int lvl = 0;
    int is_hit;
    char* fname = NULL;
    char* path_cpy = (char *)malloc(sizeof(path));
    *is_root = 0;
    strcpy(path_cpy, path);

    if(total_lvl == 0){
        *is_find = 1;
        *is_root = 1;
        dentry_ret = super.root_dentry;
    }
    fname = strtok(path_cpy, "/");
    while (fname)
    {
        lvl++;
        if(dentry_cursor->inode == NULL){/* Cache机制 */
            newfs_read_inode(dentry_cursor, dentry_cursor->ino);
        }

        inode = dentry_cursor->inode;

        if(inode->dentry->ftype == NEWFS_REG_FILE && lvl < total_lvl){
            printf("Not a dir");
            dentry_ret = inode->dentry;
            break;
        }

        if(inode->dentry->ftype == NEWFS_DIR){
            dentry_cursor = inode->dentrys;
            is_hit = 0;
            while (dentry_cursor){
                if (memcmp(dentry_cursor->name, fname, strlen(fname)) == 0){
                    is_hit = 1;
                    break;
                }
                dentry_cursor = dentry_cursor->brother;
            }

            if(!is_hit) {
                *is_find = 0;
                printf("not found");
                dentry_ret = inode->dentry;
                break;
            }

            if(is_hit && lvl == total_lvl) {
                *is_find = 1;
                dentry_ret = dentry_cursor;
                break;
            }
        }
        fname = strtok(NULL, "/"); 
    }

    if(dentry_ret->inode == NULL){
        dentry_ret->inode = newfs_read_inode(dentry_ret, dentry_ret->ino);
    }
    return dentry_ret;
}

int newfs_mount() {
    super.fd = ddriver_open(newfs_options.device);
	if (super.fd < 0) {
        return super.fd;
    }
	// Variables
    struct newfs_super_d  newfs_super_d; 
    struct newfs_dentry*  root_dentry;
    struct newfs_inode*   root_inode;

    int             is_init = 0;// false

    super.is_mounted = FALSE;
	ddriver_ioctl(super.fd, IOC_REQ_DEVICE_SIZE,  &super.sz_disk);
    ddriver_ioctl(super.fd, IOC_REQ_DEVICE_IO_SZ, &super.sz_io);
	super.sz_logit = 2 * super.sz_io;

	root_dentry = new_dentry("/", NEWFS_DIR);     /* 根目录项每次挂载时新建 */

	if (newfs_driver_read(NEWFS_SUPER_OFS, (uint8_t *)(&newfs_super_d), 
                sizeof(struct newfs_super_d)) != 0) {
        return -EIO;
    }   

	if (newfs_super_d.magic_num != NEWFS_MAGIC_NUM){ /* 幻数不正确，初始化 */
		//磁盘超级块占用的逻辑块数量
		int super_blks = ROUND_UP(sizeof(struct newfs_super_d), super.sz_logit) / super.sz_logit;
		// 每个逻辑块放多少inode
		int blk_per_inode = super.sz_logit / sizeof(struct newfs_inode_d);
		// 假设每个文件最多占用6个逻辑块, NEWFS_DATA_BLK
		int num_logit = super.sz_disk / super.sz_logit;
		// 则有inode_num * 6 + inode_num / blk_per_inode + 1 + 1 + super_blks < num_logit
		// 计算除法时将1 / blk_per_inode 向上取整为1，除完以后在ROUNDUP为blk_per_inode
		int inode_num = (num_logit - 1 - 1 - super_blks) / (NEWFS_DATA_BLK + 1);
		inode_num = ROUND_UP(inode_num, blk_per_inode);
		// 确保索引数量和文件数量不超过位图大小，即一个逻辑块的bit数， 即sz_logit * 8
		// inode_num = inode_num > (8 * super.sz_logit) ? (8 * super.sz_logit) : inode_num;

		newfs_super_d.blk_per_inode = blk_per_inode;
		newfs_super_d.inode_blks = inode_num / blk_per_inode;
		newfs_super_d.inode_bitmap_offset = NEWFS_SUPER_OFS + super_blks * super.sz_logit;
		newfs_super_d.data_bitmap_offset = newfs_super_d.inode_bitmap_offset + super.sz_logit;
		newfs_super_d.inode_offset = newfs_super_d.data_bitmap_offset + super.sz_logit;
		newfs_super_d.data_offset = newfs_super_d.inode_offset + newfs_super_d.inode_blks * super.sz_logit;
        newfs_super_d.data_blks = num_logit - super_blks - newfs_super_d.inode_blks - 2;

		newfs_super_d.sz_usage = 0;
		
		is_init = 1;

	}
	/* 建立 in-memory 结构 */
	super.sz_usage   = newfs_super_d.sz_usage; 

	super.inodes_bitmap = (uint8_t *)malloc(super.sz_logit);
	super.data_bitmap = (uint8_t *)malloc(super.sz_logit);
	super.blk_per_inode = newfs_super_d.blk_per_inode;
	super.inode_blks = newfs_super_d.inode_blks;
	super.inode_bitmap_offset = newfs_super_d.inode_bitmap_offset;
	super.data_bitmap_offset = newfs_super_d.data_bitmap_offset;
	super.inode_offset = newfs_super_d.inode_offset;
	super.data_offset = newfs_super_d.data_offset;
    super.data_blks = newfs_super_d.data_blks;


	if (newfs_driver_read(newfs_super_d.inode_bitmap_offset, (uint8_t *)(super.inodes_bitmap), super.sz_logit) != 0) {
        return -EIO;
    }

	if (newfs_driver_read(newfs_super_d.data_bitmap_offset, (uint8_t *)(super.data_bitmap), super.sz_logit) != 0) {
        return -EIO;
    }


	if (is_init) {                                    /* 分配根节点 */
        root_inode = newfs_alloc_inode(root_dentry);
        newfs_sync_inode(root_inode);
    }
    
    root_inode           = newfs_read_inode(root_dentry, NEWFS_ROOT_INO);  /* 读取根目录 */
    root_dentry->inode   = root_inode;
    super.root_dentry = root_dentry;
    super.is_mounted  = 1;

    return 0;
}
/**
 * @brief 
 * 
 * @return int 
 */
int newfs_umount() {
    struct newfs_super_d  newfs_super_d; 

    if (!super.is_mounted) {
        return 0;
    }

    newfs_sync_inode(super.root_dentry->inode);     /* 从根节点向下刷写节点 */
                                                    
    newfs_super_d.magic_num           = NEWFS_MAGIC_NUM;
    newfs_super_d.sz_usage            = super.sz_usage;
    newfs_super_d.inode_bitmap_offset = super.inode_bitmap_offset;
    newfs_super_d.data_bitmap_offset  = super.data_bitmap_offset;
    newfs_super_d.inode_blks          = super.inode_blks;
    newfs_super_d.blk_per_inode       = super.blk_per_inode;
    newfs_super_d.inode_offset        = super.inode_offset;
    newfs_super_d.data_offset         = super.data_offset;


    if (newfs_driver_write(NEWFS_SUPER_OFS, (uint8_t *)&newfs_super_d, sizeof(struct newfs_super_d)) != 0) {
        return -EIO;
    }

    if (newfs_driver_write(newfs_super_d.inode_bitmap_offset, (uint8_t *)(super.inodes_bitmap), super.sz_logit) != 0) {
        return -EIO;
    }

    if (newfs_driver_write(newfs_super_d.data_bitmap_offset, (uint8_t *)(super.data_bitmap), super.sz_logit) != 0) {
        return -EIO;
    }

    free(super.inodes_bitmap);
    free(super.data_bitmap);
    ddriver_close(super.fd);

    return 0;
}