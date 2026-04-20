obj-m += monitor.o

KDIR := /lib/modules/$(shell uname -r)/build
PWD := $(shell pwd)

all:
	make -C $(KDIR) M=$(PWD) modules
	gcc -Wall -Wextra -o engine engine.c
	gcc -Wall -Wextra -o cpu_hog cpu_hog.c
	gcc -Wall -Wextra -o io_pulse io_pulse.c
	gcc -Wall -Wextra -o memory_hog memory_hog.c

clean:
	make -C $(KDIR) M=$(PWD) clean
	rm -f engine cpu_hog io_pulse memory_hog