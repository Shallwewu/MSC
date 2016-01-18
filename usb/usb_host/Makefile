ifneq ($(KERNELRELEASE),)
obj-m :=usb_mschost.o
else
KERNELDIR :=/lib/modules/$(shell uname -r)/build
PWD := $(shell pwd)
default:
	$(MAKE) -C $(KERNELDIR) M=$(PWD) modules
test:
	gcc -o usb_mschosttest usb_mschosttest.c
clean:
	rm -rf *.o *.mod.c *.mod.o *.ko
endif