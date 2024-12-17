obj-m += dpf.o
dpf-objs := kernel_dpf.o ../tuners/kernel_primitive.o

PWD := $(CURDIR)

CC := /usr/bin/gcc-12

all:
	make -C /lib/modules/$(shell uname -r)/build M=$(PWD) modules

clean:
	make -C /lib/modules/$(shell uname -r)/build M=$(PWD) clean
