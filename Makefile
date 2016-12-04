obj-m += sys_mergesort.o

INC=/lib/modules/$(shell uname -r)/build/arch/x86/include

all: xmergesort mergesort

xmergesort: xmergesort.c
	gcc -Wall -Werror -I$(INC)/generated/uapi -I$(INC)/uapi xmergesort.c -o xmergesort

mergesort:
	make -Wall -Werror -C /lib/modules/$(shell uname -r)/build M=$(PWD) modules

clean:
	make -C /lib/modules/$(shell uname -r)/build M=$(PWD) clean
	rm -f xmergesort
