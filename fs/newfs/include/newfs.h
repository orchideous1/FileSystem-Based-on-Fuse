#ifndef _NEWFS_H_
#define _NEWFS_H_

#define FUSE_USE_VERSION 26
#include "stdio.h"
#include "stdlib.h"
#include <unistd.h>
#include "fcntl.h"
#include "string.h"
#include "fuse.h"
#include <stddef.h>
#include "ddriver.h"
#include "errno.h"
#include "types.h"
#include "stdint.h"

#define NEWFS_MAGIC                  /* TODO: Define by yourself */
#define NEWFS_DEFAULT_PERM    0777   /* 全权限打开 */

/******************************************************************************
* SECTION: newfs.c
*******************************************************************************/
void* 			   newfs_init(struct fuse_conn_info *);
void  			   newfs_destroy(void *);
int   			   newfs_mkdir(const char *, mode_t);
int   			   newfs_getattr(const char *, struct stat *);
int   			   newfs_readdir(const char *, void *, fuse_fill_dir_t, off_t,
						                struct fuse_file_info *);
int   			   newfs_mknod(const char *, mode_t, dev_t);
int   			   newfs_write(const char *, const char *, size_t, off_t,
					                  struct fuse_file_info *);
int   			   newfs_read(const char *, char *, size_t, off_t,
					                 struct fuse_file_info *);
int   			   newfs_access(const char *, int);
int   			   newfs_unlink(const char *);
int   			   newfs_rmdir(const char *);
int   			   newfs_rename(const char *, const char *);
int   			   newfs_utimens(const char *, const struct timespec tv[2]);
int   			   newfs_truncate(const char *, off_t);
			
int   			   newfs_open(const char *, struct fuse_file_info *);
int   			   newfs_opendir(const char *, struct fuse_file_info *);
/******************************************************************************
* SECTION: sfs_utils.c
*******************************************************************************/
char* 			   get_name(const char* );
int 			   calc_lvl(const char * );
int 			   newfs_driver_read(int , uint8_t *, int );
int 			   newfs_driver_write(int , uint8_t *, int );
int                newfs_search_data_bitmap();

int 			   newfs_mount();
int 			   newfs_umount();

int 			   newfs_alloc_dentry(struct newfs_inode *, struct newfs_dentry *);
int 			   newfs_drop_dentry(struct newfs_inode * , struct newfs_dentry *);
struct newfs_inode*  newfs_alloc_inode(struct newfs_dentry *);
int 			   newfs_sync_inode(struct newfs_inode * );
int 			   newfs_drop_inode(struct newfs_inode * );
struct newfs_inode*  newfs_read_inode(struct newfs_dentry * , int );
struct newfs_dentry* newfs_get_dentry(struct newfs_inode *, int);

struct newfs_dentry* newfs_lookup(const char * , int * , int* );

#endif  /* _newfs_H_ */