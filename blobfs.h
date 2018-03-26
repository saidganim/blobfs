#ifndef CCF_BLOBFS_H
#define CCF_BLOBFS_H

#include <asm/errno.h>
#include <linux/types.h>
#include <linux/kernel.h>
#include <linux/fs.h>

// From zircon/system/ulib/blobfs/include/blobfs/format.h

extern const uint64_t kBlobfsMagic0;
extern const uint64_t kBlobfsMagic1;
extern const uint32_t kBlobfsVersion;
extern const uint64_t kStartBlockFree;
extern const uint32_t kBlobFlagClean;
extern const uint32_t kBlobFlagDirty;
extern const uint32_t kBlobFlagFVM;
extern const uint32_t kBlobfsBlockSize;
extern const uint32_t kBlobfsBlockBits;
extern const uint32_t kBlobfsBlockMapStart;
extern const uint32_t kBlobfsInodeSize;
extern const uint32_t kBlobfsInodesPerBlock;

extern const size_t kFVMBlockMapStart;
extern const size_t kFVMNodeMapStart;
extern const size_t kFVMDataStart;


// Notes:
// - block 0 is always allocated
// - inode 0 is never used, should be marked allocated but ignored



#ifndef SHA256_DIGEST_LENGTH
#define SHA256_DIGEST_LENGTH 32
#endif

#define DIGEST_LENGTH SHA256_DIGEST_LENGTH

typedef struct { // SIZE OF STRUCT == 104 BYTES (832 BITs)
    uint64_t magic0;
    uint64_t magic1;
    uint32_t version;
    uint32_t flags;
    uint32_t block_size;       // 8K typical
    uint64_t block_count;      // Number of data blocks in this area
    uint64_t inode_count;      // Number of blobs in this area
    uint64_t alloc_block_count; // Total number of allocated blocks
    uint64_t alloc_inode_count; // Total number of allocated blobs
    uint64_t blob_header_next; // Block containing next blobfs, or zero if this is the last one ( NOT SUPPORTED YET)
    // The following flags are only valid with (flags & kBlobFlagFVM):
    uint64_t slice_size;    // Underlying slice size
    uint64_t vslice_count;  // Number of underlying slices
    uint32_t abm_slices;    // Slices allocated to block bitmap
    uint32_t ino_slices;    // Slices allocated to node map
    uint32_t dat_slices;    // Slices allocated to file data section
} blobfs_info_t;


typedef struct {
    uint8_t  merkle_root_hash[DIGEST_LENGTH];
    uint64_t start_block;
    uint64_t num_blocks;
    uint64_t blob_size;
    uint64_t reserved;
} blobfs_inode_t;

static inline size_t blobfs_blockmap_start(blobfs_info_t *bfs){
  if(bfs->flags & kBlobFlagFVM)
    return kFVMBlockMapStart;
  else
    return kBlobfsBlockMapStart;
};

static inline size_t blobfs_blockmap_blocks(blobfs_info_t *bfs){
  return roundup(bfs->block_count, kBlobfsBlockBits) / kBlobfsBlockBits;
};

static inline size_t blobfs_nodemap_start(blobfs_info_t *bfs){
    if(bfs->flags & kBlobFlagFVM)
      return kFVMNodeMapStart;
    else
      return blobfs_blockmap_start(bfs) + blobfs_blockmap_blocks(bfs);
};

static inline size_t blobfs_nodemap_blocks(blobfs_info_t *bfs){
  return roundup(bfs->inode_count, kBlobfsInodesPerBlock) / kBlobfsInodesPerBlock;
};

static inline size_t blobfs_datastart_block(blobfs_info_t *bfs){
  if(bfs->flags & kBlobFlagFVM)
    return kFVMDataStart;
  else
    return blobfs_nodemap_start(bfs) + blobfs_nodemap_blocks(bfs);
};

static inline int64_t blob_data_blocks(blobfs_inode_t* blobNode) {
    return roundup(blobNode->blob_size, kBlobfsBlockSize) / kBlobfsBlockSize;
}

static inline int check_blobfs(blobfs_info_t *blobfs_sb, size_t max){
  int ret = 0;
  if(blobfs_sb->magic0 != kBlobfsMagic0 || blobfs_sb->magic1 != kBlobfsMagic1){
    printk(KERN_WARNING "Blobfs: bad magic\n");
    ret = -EACCES;
    goto error;
  } else if(blobfs_sb->version != kBlobfsVersion){
    printk(KERN_WARNING "Blobfs: FS version %d; Driver version %d\n", blobfs_sb->version, kBlobfsVersion);
    ret = -EACCES;
    goto error;
  } else if(blobfs_sb->block_size != kBlobfsBlockSize){
    printk(KERN_WARNING "Blobfs: Block size is not supported\n");
    ret = -EACCES;
    goto error;
  } else if((blobfs_sb->flags & kBlobFlagFVM) == 0){
      if(blobfs_sb->block_count + blobfs_datastart_block(blobfs_sb) > max){
        printk(KERN_WARNING "Blobfs: too large for device\n");
        ret = -ENOSPC;
        goto error;
      }
  } else{
    // We don't need all sanity checks of metadata, since our driver is readonly.
  }

error:
  return ret;
};

struct dentry *blobfs_mount(struct file_system_type *fs_type_struct, int flags, const char *dev_name,  void *data);
void blobfs_kill_sb(struct super_block*);

int read_block(struct super_block*, char*, size_t);
int write_block(struct super_block*, char*, size_t);

int read_cluster(struct super_block*, char*, size_t);
int write_cluster(struct super_block*, char*, size_t);
#endif
