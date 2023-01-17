#ifndef __ufs_h__
#define __ufs_h__

#define UFS_DIRECTORY (0)
#define UFS_REGULAR_FILE (1)

#define UFS_BLOCK_SIZE (4096)

#define DIRECT_PTRS (30)



typedef struct {
    int type;   // MFS_DIRECTORY or MFS_REGULAR
    int size;   // bytes
    unsigned int direct[DIRECT_PTRS];
} inode_t;

typedef struct {
    char name[28];  // up to 28 bytes of name in directory (including \0)
    int  inum;      // inode number of entry (-1 means entry not used)
} dir_ent_t; //32 bytes in total

typedef struct {

	dir_ent_t entries[128];

} data_block_t;

// presumed: block 0 is the super block
typedef struct __super {
    int inode_bitmap_addr; // block address (in blocks)1
    int inode_bitmap_len;  // in blocks                1     
    int data_bitmap_addr;  // block address (in blocks)2
    int data_bitmap_len;   // in blocks                1
    int inode_region_addr; // block address (in blocks)3
    int inode_region_len;  // in blocks                1
    int data_region_addr;  // block address (in blocks)4
    int data_region_len;   // in blocks                32
    int num_inodes;        // just the number of inodes 32
    int num_data;          // and data blocks...        32
} super_t;

// Disk region offsets
#define OFFSET_INODESBITMAP (1*UFS_BLOCK_SIZE) 
#define OFFSET_DATABITMAP (2*UFS_BLOCK_SIZE)
#define OFFSET_INODES (3*UFS_BLOCK_SIZE)
#define OFFSET_DATA (4*UFS_BLOCK_SIZE)

#endif // __ufs_h__
