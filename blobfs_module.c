#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/fs.h>
#include "blobfs.h"

MODULE_AUTHOR("Saidgani Musaev <saidgani.musaev@gmail.com");
MODULE_DESCRIPTION("BLOBFS filesystem driver for Linux kernel. (Written for kernel 4.11.3");
MODULE_LICENSE("GPL");


static struct file_system_type blobfs_struct = {
								.owner = THIS_MODULE,
								// name of filesystem in kernel table
								.name = "blobfs",
								// mounting filesystem
								.mount = blobfs_mount,
								// kill_block_super is provided by the kernel
								.kill_sb = blobfs_kill_sb, //  TODO: Implement own function with releasing resources
								// blobfs needs disk for work
								.fs_flags = FS_REQUIRES_DEV,
};


int blobfs_init(void){
  return register_filesystem(&blobfs_struct);
}

void blobfs_release(void){
  unregister_filesystem(&blobfs_struct);
}

module_init(blobfs_init);
module_exit(blobfs_release);
