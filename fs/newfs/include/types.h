#ifndef _TYPES_H_
#define _TYPES_H_

#define MAX_NAME_LEN    128  
#define NEWFS_DEFAULT_PERM        0777
typedef enum fs_file_type {
    NEWFS_REG_FILE,
    NEWFS_DIR,
    NEWFS_SYM_LINK
} FS_FILE_TYPE;   


/******************************************************************************
* SECTION: Macro
*******************************************************************************/
#define TRUE                    1
#define FALSE                   0
#define UINT32_BITS             32
#define UINT8_BITS              8

#define NEWFS_MAGIC_NUM           0x52415453  
#define NEWFS_SUPER_OFS           0
#define NEWFS_INODE_BITMAP_OFS    1
#define NEWFS_DATA_BITMAP_OFS     2
#define NEWFS_INODE_OFS           3
#define NEWFS_DATA_BLK            6
#define NEWFS_ROOT_INO            0
/******************************************************************************
* SECTION: Macro Function
*******************************************************************************/
#define ROUND_DOWN(value, round)    ((value) % (round) == 0 ? (value) : ((value) / (round)) * (round))
#define ROUND_UP(value, round)      ((value) % (round) == 0 ? (value) : ((value) / (round) + 1) * (round))
/******************************************************************************
* SECTION: FS Specific Structure - In memory structure
*******************************************************************************/
struct newfs_dentry;
struct newfs_inode;
struct newfs_super;

struct custom_options {
	const char*        device;
};

struct newfs_super {
    int      fd;
    /* TODO: Define yourself */
    int                sz_io;
    int                sz_disk;
    int                sz_logit;
    int                sz_usage;
    //索引位图, 默认一块逻辑块
    uint32_t           inode_bitmap_offset;
    uint8_t *          inodes_bitmap;
    //数据块位图， 默认一块逻辑块
    uint32_t           data_bitmap_offset;
    uint8_t*           data_bitmap;
    //索引节点
    uint32_t           inode_blks; //索引块数， 
    uint32_t           blk_per_inode; // 每个逻辑块放多少inode
    uint32_t           inode_offset; // 索引偏移

    uint32_t           data_offset;
    uint32_t           data_blks;//数据块个数

    int            is_mounted;

    struct newfs_dentry* root_dentry;// 内存根目录
};


struct newfs_inode {
    uint32_t ino;
    /* TODO: Define yourself */
    uint32_t           size;                          /* 文件已占用空间 */
    //char               target_path[MAX_NAME_LEN];/* store traget path when it is a symlink */
    int                dir_cnt;
    struct newfs_dentry* dentry;                        /* 指向该inode的dentry */
    struct newfs_dentry* dentrys;                       /* 所有目录项 */
    uint8_t*           data;
};

struct newfs_dentry {
    char     name[MAX_NAME_LEN];
    /* TODO: Define yourself */
    FS_FILE_TYPE      ftype;
    uint32_t ino;
    struct newfs_dentry*  parent;
    struct newfs_dentry*  brother;
    struct newfs_inode* inode;
};

static inline struct newfs_dentry* new_dentry(char * fname, FS_FILE_TYPE ftype) {
    struct newfs_dentry * dentry = (struct newfs_dentry *)malloc(sizeof(struct newfs_dentry));
    memset(dentry, 0, sizeof(struct newfs_dentry));
    memcpy(dentry->name, fname, strlen(fname));
    dentry->ftype   = ftype;
    dentry->ino     = -1;
    dentry->inode   = NULL;
    dentry->parent  = NULL;
    dentry->brother = NULL; 
    return dentry;                                           
}
/******************************************************************************
* SECTION: FS Specific Structure - Disk structure
*******************************************************************************/
struct newfs_super_d
{
    uint32_t           magic_num;
    uint32_t           sz_usage;
    //索引位图
    uint32_t           inode_bitmap_offset;
    //数据块位图
    uint32_t           data_bitmap_offset;
    //索引节点
    uint32_t           inode_blks; //索引节点块数
    uint32_t           blk_per_inode; // 每个逻辑块放多少inode
    uint32_t           inode_offset; // 索引节点偏移
    //数据块
    uint32_t           data_offset; // 数据块偏移
    uint32_t           data_blks;//数据块个数
};

struct newfs_inode_d// <= 200B
{
    uint32_t           ino;                           /* 在inode位图中的下标 */
    uint32_t           size;                          /* 文件已占用空间 */
    //char               target_path[MAX_NAME_LEN];/* store traget path when it is a symlink */
    uint32_t           dir_cnt;
    int                block_pointer[NEWFS_DATA_BLK + 1];   // 数据块指针（可固定分配）
    FS_FILE_TYPE       ftype;   
};  

struct newfs_dentry_d
{
    char               fname[MAX_NAME_LEN];
    FS_FILE_TYPE       ftype;
    uint32_t           ino;                           /* 指向的ino号 */
};  
#endif /* _TYPES_H_ */