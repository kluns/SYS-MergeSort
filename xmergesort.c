#include <asm/unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <sys/syscall.h>
#include <unistd.h>

#ifndef __NR_mergesort
#error mergesort system call not defined
#endif

struct intput_args{
        
        unsigned int flags;
        unsigned int *data;
        const char *op_file;
        const char **ip_files;
};


int main(int argc, char *argv[])
{
	int rc;
	int ch;
	struct intput_args args;
	
	args.data = malloc(sizeof(unsigned int));
	args.op_file = argv[2];
	args.ip_files = (const char *[]){argv[3], argv[4]};
	args.flags = 0x00;

	while((ch = getopt(argc,argv, "uaitd")) != -1){
	        switch(ch){
		        case 'u':
			        args.flags |= 0x01;
				break;
		        case 'a':
			        args.flags |= 0x02;
				break;
		        case 'i':
			        args.flags |= 0x04;
				break;
		        case 't':
			        args.flags |= 0x10;
				break;
		        case 'd':
			        args.flags |= 0x20;
				break;
		        case '?':
			        printf("unknown flags");
				break;
	        }
	}
	
  	rc = syscall(__NR_mergesort, (void *) &args);
	if (rc == 0){
		if(args.flags&0x20)
			printf("total bytes that writed to output: %d.\n", *(args.data));
		else
			printf("merge sort finished.\n");
	}
	else
		printf("syscall returned %d (errno=%d)\n", rc, errno);
		
	exit(rc);
}
