obj-m += write_chardev.o
obj-m += read_chardev.o

all:
	make -C /lib/modules/$(shell uname -r)/build M=$(PWD) modules
	cd user && $(MAKE)

clean:
	make -C /lib/modules/$(shell uname -r)/build M=$(PWD) clean
	cd user && $(MAKE) clean
