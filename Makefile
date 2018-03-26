CURRENT = $(shell uname -r)
KDIR =  /lib/modules/$(CURRENT)/build
PWD = $(shell pwd)
TARGET = blobfs

obj-m := $(TARGET).o

$(TARGET)-objs := blobfs_module.o blobfs_oper.o

build:
	$(MAKE) CONFIG_STACK_VALIDATION= -C $(KDIR) M=$(PWD) modules

clean:
	$(MAKE) -C $(KDIR) M=$(PWD) clean
