#include <linux/buffer_head.h>
#include <linux/slab.h>
#include <linux/stat.h>
#include <linux/backing-dev-defs.h>
#include <linux/genhd.h>
#include "blobfs.h"

const uint64_t kBlobfsMagic0  = (0xac2153479e694d21ULL);
const uint64_t kBlobfsMagic1  = (0x985000d4d4d3d314ULL);
const uint32_t kBlobfsVersion = 0x00000004;

const uint64_t kStartBlockFree       = 0;
const uint32_t kBlobFlagClean        = 1;
const uint32_t kBlobFlagDirty        = 2;
const uint32_t kBlobFlagFVM          = 4;
const uint32_t kBlobfsBlockSize      = 8192;
const uint32_t kBlobfsBlockBits      = (8192 * 8);
const uint32_t kBlobfsBlockMapStart  = 1;
const uint32_t kBlobfsInodeSize      = 64;
const uint32_t kBlobfsInodesPerBlock = (8192 / 64);

const size_t kFVMBlockMapStart  = 0x10000;
const size_t kFVMNodeMapStart   = 0x20000;
const size_t kFVMDataStart      = 0x30000;


static struct inode* blobfs_get_inode(struct super_block* sb, struct inode* parent_dir, struct dentry* dentry, umode_t mode);

static int digest_string(char* bytes_, char* out, size_t len){
  // if (len < sizeof(bytes_) * 2 + 1) {
  //   return -EBUSY;
  // }
  size_t i;
  memset(out, 0, len);
  char* p;

  p = out;
  for ( i = 0; i < DIGEST_LENGTH; ++i) {
      sprintf(p, "%02x", bytes_[i]);
      p += 2;
  }
  return 0;
}

ssize_t blobfs_file_read(struct file *filp, char __user *buf, size_t len, loff_t *ppos){
  ssize_t ret_i = 0;
  loff_t start_block;
  char* __buf;
  blobfs_inode_t* inode_map;

  inode_map = filp->f_inode->i_private;

  if(*ppos == blob_data_blocks(inode_map) * kBlobfsBlockSize)
    return 0;

  if(!inode_map)
    return -EINVAL;
  __buf = kmalloc(kBlobfsBlockSize, GFP_KERNEL);
  if(!buf)
    return 0;
  len = min(len, kBlobfsBlockSize - *ppos % kBlobfsBlockSize);
  len = min(len, inode_map->blob_size - *ppos);

  read_block(filp->f_inode->i_sb, __buf, blobfs_datastart_block(filp->f_inode->i_sb->s_fs_info) + inode_map->start_block + (*ppos) / kBlobfsBlockSize);
  copy_to_user(buf, __buf + *ppos % kBlobfsBlockSize, len);
  *ppos += len;
release:
  kfree(__buf);
  return len;
}

int blobfs_readdir(struct file *file, struct dir_context *ctx){
  char* buf;
  blobfs_inode_t* inode_map;

	if (!dir_emit_dots(file, ctx))
		return 0;
  if(ctx->pos == ((blobfs_info_t*)(file->f_inode->i_sb->s_fs_info))->inode_count + 1)
    return 0;
  buf = (char*)kmalloc(sizeof(char) * (DIGEST_LENGTH * 2 + 1), GFP_KERNEL);
  inode_map = file->f_inode->i_sb->s_root->d_inode->i_private;

  while(inode_map[ctx->pos - 2].start_block == kStartBlockFree){
    if(ctx->pos == ((blobfs_info_t*)(file->f_inode->i_sb->s_fs_info))->inode_count + 1)
      goto release;
    ++(ctx->pos);
  }

  //digest_string(buf, inode_map[ctx->pos - 2].merkle_root_hash, DIGEST_LENGTH * 2 + 1);
	dir_emit(ctx, inode_map[ctx->pos - 2].merkle_root_hash, DIGEST_LENGTH, file->f_inode->i_ino, DT_REG);
  ++(ctx->pos);

release:
  kfree(buf);
	return 0;
}

int blobfs_open(struct inode* inode, struct file *file){
  //file->f_inode = inode;
	return 0;
}

struct dentry* blobfs_lookup(struct inode* parent, struct dentry* child, unsigned int flags){
  size_t inode_i = 0;
  char* buf;
  struct inode* inode;
  blobfs_inode_t* inode_map;
  blobfs_info_t* bfs;
  inode = blobfs_get_inode(parent->i_sb, parent, child, S_IRUSR | S_IRGRP | S_IROTH | S_IFREG );
  if(!inode){
    printk(KERN_ALERT "Blobfs: couldn't create new inode for root dir\n");
    return NULL;
  }
  inode_map = (blobfs_inode_t*)parent->i_sb->s_root->d_inode->i_private;
  bfs = (blobfs_info_t*)parent->i_sb->s_fs_info;
  buf = (char*)kmalloc(sizeof(char) * (DIGEST_LENGTH * 2 + 1), GFP_KERNEL);
  // printk(KERN_ALERT " Blobfs: lookup file %s\n", child->d_name.name);
  for(inode_i = 0; inode_i < bfs->inode_count; ++inode_i){
    //digest_string(buf, inode_map[inode_i].merkle_root_hash, DIGEST_LENGTH * 2 + 1);
    // printk(KERN_ALERT " Blobfs: checking file %s\n", inode_map[inode_i].merkle_root_hash);
    if(!memcmp(inode_map[inode_i].merkle_root_hash, child->d_name.name, strlen(child->d_name.name)))
      break;
  }
  if(inode_i == bfs->inode_count) // couldn't find inode...
    goto release;

  inode->i_size = inode_map[inode_i].blob_size;
  inode->i_flags = flags;
  inode->i_private = &inode_map[inode_i];
release:
  d_add(child, inode);
  kfree(buf);
  return NULL;
}

static struct inode_operations blobfs_inode_operations = {
  .create      = 0,
  .lookup     = blobfs_lookup,
  .link       = 0,
  .unlink     = 0,
  .symlink    = 0,
  .mkdir      = 0,
  .rmdir      = 0,
  .mknod      = 0,
  .rename     = 0,
};

static struct file_operations blobfs_file_operations = {
	.open = blobfs_open,
  .read = blobfs_file_read,
	.write = 0x0,
  .iterate = blobfs_readdir,
};

const struct super_operations blobfs_super_operations = {
	.statfs      = simple_statfs,
	.drop_inode = generic_delete_inode,
	.show_options   = generic_show_options,
	.destroy_inode = __destroy_inode,
	.put_super = 0
};

static struct inode* blobfs_get_inode(struct super_block* sb, struct inode* parent_dir, struct dentry* dentry, umode_t mode){
  // Function for creating inode and instatiating it with dentry. by VFS documentation should call d_instantiate()
  // for making bijection between just created inode and dentry without associated inode.
  // NOTE: if parent_dir and dentry are NULL - we are creating inode for root directory for keeping mounting point

  struct inode* ret = new_inode(sb);
  if(unlikely(!ret))
    goto error;

  inode_init_owner(ret, parent_dir, mode);
  ret->i_atime = ret->i_mtime = ret->i_ctime = CURRENT_TIME;
  ret->i_ino = get_next_ino();

  // For now it's only readonly driver. Code here is very simple.

  ret->i_op = &blobfs_inode_operations;
  ret->i_fop = &blobfs_file_operations;
  ret->i_mode = mode;

  if(mode & S_IFDIR)
    inc_nlink(ret);
  goto success;



error:
  __destroy_inode(ret);
  return NULL;

success:
  return ret;
}

static int blobfs_fill_sb(struct super_block* sb, void* data, int silent){
  // Filling the superblock structure
  int ret = 0;
  struct inode* root_inode;
  char* first_block;
  blobfs_inode_t* inode_map;
  size_t map_i = 0;
  char* buf;
  uint64_t inode_map_size = NULL;
  blobfs_info_t* blobfs_sb;

  buf = (char*)kmalloc(sizeof(char) * (DIGEST_LENGTH * 2 + 1), GFP_KERNEL);
  first_block = (char*)kmalloc(sb->s_blocksize, GFP_KERNEL);
  if(read_block(sb, first_block, 0)){
    printk(KERN_ALERT "Blobfs: Couldn't read first info block\n");
    ret = -EBUSY;
    goto error;
  }
  blobfs_sb = (blobfs_info_t*)first_block;
  if(check_blobfs(blobfs_sb, get_capacity(sb->s_bdev->bd_disk) * sb->s_blocksize / kBlobfsBlockSize)){
    ret = -ENOSPC;
    goto error;
  }
  sb->s_fs_info = blobfs_sb;
  sb->s_maxbytes = MAX_LFS_FILESIZE;
  root_inode = blobfs_get_inode(sb, NULL, NULL, S_IFDIR | S_IRUSR | S_IRGRP | S_IROTH); // read-only
  if(!root_inode){
    ret = -EBUSY;
    goto error;
  }
  sb->s_root = d_make_root(root_inode);
  if(unlikely(!sb->s_root)){
    ret = -ENOMEM;
    goto error;
  }

  // Reading node map

  inode_map_size = blobfs_nodemap_blocks(blobfs_sb) * kBlobfsBlockSize;
  inode_map = (blobfs_inode_t*)kmalloc(inode_map_size, GFP_KERNEL);
  root_inode->i_private = inode_map;
  printk(KERN_ALERT "Blobfs: size of inode_map_blocks == %d == %d, starts at %d\n", blobfs_nodemap_blocks(blobfs_sb),sizeof(blobfs_inode_t), blobfs_nodemap_start(blobfs_sb));
  for(map_i = 0; map_i <= inode_map_size / kBlobfsBlockSize; ++map_i){
    read_cluster(sb, (char*)inode_map + map_i * kBlobfsBlockSize, blobfs_nodemap_start(blobfs_sb) + map_i);
  }

  for(map_i = 0; map_i < blobfs_sb->inode_count; ++map_i){
    memset(inode_map[map_i].merkle_root_hash, 0x0, DIGEST_LENGTH);
    sprintf(inode_map[map_i].merkle_root_hash, "file%d", map_i);
  }
  printk(KERN_ALERT "Blobfs: %d inodes in inode_map\n", blobfs_sb->inode_count);


  goto success;
error:
  kfree(first_block);
  kfree(buf);
  return ret;
success:
  printk(KERN_ALERT "Blobfs: Successfully mounted\n");
  kfree(buf);
  return ret;
}

struct dentry *blobfs_mount(struct file_system_type *fs_type_struct, int flags, const char *dev_name,  void *data){
								return mount_bdev(fs_type_struct, flags, dev_name, data, blobfs_fill_sb);
}

void blobfs_kill_sb(struct super_block* sb){
  kfree(sb->s_root->d_inode->i_private); // freeing blobfs_inode_t array
  kfree(sb->s_fs_info); // freeing first read sector
  kill_block_super(sb);
}

int read_block(struct super_block* sb, char* buf, size_t offset){
  struct buffer_head *bh;
  bh = sb_bread(sb, offset);
  if(unlikely(!bh))
    return -EBUSY;
  get_bh(bh);
  memcpy(buf, bh->b_data, sb->s_blocksize);
  put_bh(bh);
  brelse(bh);
  return 0;
};

int write_block(struct super_block* sb, char* buf, size_t offset){
  struct buffer_head *bh;
  int res = 0;
  bh = sb_bread(sb, offset);
  if(unlikely(!bh))
    return -EIO;
  get_bh(bh);
  memcpy(bh->b_data, buf, sb->s_blocksize);
  mark_buffer_dirty(bh);
  if(unlikely(sync_dirty_buffer(bh))){
    res = -EIO;
    goto release;
  }
release:
  put_bh(bh);
  brelse(bh);
  return res;
};


int read_cluster(struct super_block* sb, char* buf, size_t offset){
  int i = 0;
  for(i = 0; i < kBlobfsBlockSize / sb->s_blocksize; ++i)
    if(read_block(sb, buf + i * sb->s_blocksize, offset * kBlobfsBlockSize / sb->s_blocksize + i))
      return -EIO;

  return 0;
};

int write_cluster(struct super_block* sb, char* buf, size_t offset){
  int i = 0;
  for(i = 0; i < kBlobfsBlockSize / sb->s_blocksize; ++i)
    if(write_block(sb, buf + i * sb->s_blocksize, offset * kBlobfsBlockSize / sb->s_blocksize + i))
      return -EIO;

  return 0;
};
