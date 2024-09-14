#编译命令： make ARCH=arm CROSS_COMPILE=arm-linux-gnueabihf-
CROSS_COMPILE:= arm-linux-gnueabihf-
ARCH:= arm
CC:= $(CROSS_COMPILE)gcc
LD:= $(CROSS_COMPILE)ld

obj-m:=irq_start.o

P_KVER ?= $(shell uname -r)
#KDIR:=/lib/modules/$(shell uname -r)/build
KDIR:=/home/tronlong/ZYNQ/Kernel/Linux-4.9.0  
PWD:=$(shell pwd)

default:
	$(MAKE) -C $(KDIR) SUBDIRS=$(PWD) modules ARCH=$(ARCH)

clean:
	$(MAKE) -C $(KDIR) M=$(PWD) clean
	$(RM) Module.markers modules.order
	$(RM) $(PWD)/src/modules/kmod/client/kmod/Module.markers
	$(RM) $(PWD)/src/modules/kmod/client/kmod/modules.order
