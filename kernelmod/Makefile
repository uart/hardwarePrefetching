obj-m += dpf.o
dpf-objs := kernel_dpf.o kernel_common.o kernel_primitive.o kernel_pmu_ddr.o

PWD := $(CURDIR)

CC := /usr/bin/gcc

all:
	make -C /lib/modules/$(shell uname -r)/build M=$(PWD) modules

clean:
	make -C /lib/modules/$(shell uname -r)/build M=$(PWD) clean
