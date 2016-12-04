#include <linux/linkage.h>
#include <linux/moduleloader.h>
#include <asm/uaccess.h>
#include <linux/slab.h>
#include <linux/fs.h>
#include <linux/namei.h>

asmlinkage extern long (*sysptr)(void *arg);
 
struct kernel_args{
  
        unsigned int flags;
        unsigned int *data;
        char *out_file;
        char **in_files;
};


int read_file(struct file *filp, char *buf, int len, int *pos){

	mm_segment_t oldfs;
	int bytes;

	filp->f_pos = *pos;
	oldfs = get_fs();
	set_fs(KERNEL_DS);
	bytes = vfs_read(filp,(void *) buf, len, &filp->f_pos);
	set_fs(oldfs);
	
	//clean up buf tails
	for(bytes=strlen(buf)-1; bytes>=0;bytes--){
		if(buf[bytes]=='\n')
			break;
	}
	buf[++bytes]='\0';
	*pos += bytes;
	bytes -= bytes;
	//printk("after clean, buf: %s\n",buf);
	return bytes;

}

int write_file(struct file *filp, char *buf, int *size, int *pos){

	mm_segment_t oldfs;
	int bytes;
	
	filp->f_pos = *pos;
	oldfs = get_fs();
	set_fs(KERNEL_DS);
	bytes = vfs_write(filp, (void *) buf, strlen(buf), &filp->f_pos);
	set_fs(oldfs);
	
	*pos = filp->f_pos;
	*size = 0;
	buf[0] = '\0';

      	return bytes;

}

void concat_to_op(char* buf_op, int *size_op, int *size_in, char* line_in){
	
	strcat(buf_op, line_in);
	strcat(buf_op, "\n");
	*size_op = *size_op + strlen(line_in) + 1;
	*size_in = *size_in - strlen(line_in) - 1;
	return;
	
}

int check_sorted(char *pre, char *cur, unsigned int flag_i){
	
	if(!pre)
		return -1; //represent no pre element

	if(flag_i)
		return strcasecmp(pre, cur);
	else
		return strcmp(pre, cur);
	
	
}

int merge(struct file *filp_f1, struct file *filp_f2, struct file *filp_tem, unsigned int flags){

	int pos_f1 = 0;
	int pos_f2 = 0;
	int pos_tem = 0;
	int size_f1 = 0;
	int size_f2 = 0;
	int size_tem = 0;
	int size_page = 4096;
	int readbytes = 0;
	int writebytes = 0;
	int cp = 0;
	
	char *buf_f1 = NULL;
	char *buf_f2 = NULL;
	char *buf_tem = NULL;
	char *line_f1 = NULL;
	char *line_f2 = NULL;
	char *pre_f1 = NULL;
	char *pre_f2 = NULL;
	char *buf_f1_bp = NULL;
	char *buf_f2_bp = NULL;

	unsigned int flag_u = flags&(0x01);
	unsigned int flag_a = flags&(0x02);
	unsigned int flag_i = flags&(0x04);
	unsigned int flag_t = flags&(0x10);

	if (flag_a&&flag_u){
		writebytes = -EINVAL;
		printk("-u and -a cannot appear at the same time. \n");
		goto exit;
	}

	buf_f1 = (char *) kmalloc(size_page, GFP_KERNEL);
	if(!buf_f1){
		
		writebytes = -ENOMEM;
		goto exit;
	}
	buf_f1_bp = buf_f1;
	
	buf_f2 = (char *) kmalloc(size_page, GFP_KERNEL);
	if(!buf_f2){
		
		writebytes = -ENOMEM;
		goto exit;
	}
	buf_f2_bp = buf_f2;

	buf_tem = (char *) kmalloc(size_page, GFP_KERNEL);
	if(!buf_tem){
		
		writebytes = -ENOMEM;
		goto exit;
	}
	buf_tem[0] = '\0';
	//printk("success to open three buffers. \n");

	size_f1 = filp_f1->f_inode->i_size;
	size_f2 = filp_f2->f_inode->i_size;
	
	while(size_f1>0 && size_f2>0){

		readbytes = read_file(filp_f1, buf_f1, size_page, &pos_f1);
		readbytes = read_file(filp_f2, buf_f2, size_page, &pos_f2);
		
		line_f1 = strsep(&buf_f1, "\n");
		line_f2 = strsep(&buf_f2, "\n");
		
		while(strlen(line_f1)!=0 && strlen(line_f2)!=0){
			
			if(flag_i)
				cp = strcasecmp(line_f1, line_f2);
			else
				cp = strcmp(line_f1, line_f2);


			if(cp>0){ // line_f1 > line_f2
				if(check_sorted(pre_f2, line_f2, flag_i) > 0){
					if(flag_t)
						writebytes = -EINVAL;
					else
						writebytes += write_file(filp_tem, buf_tem, &size_tem, &pos_tem);
					goto exit;

				}else if(check_sorted(pre_f2, line_f2, flag_i)==0 && flag_u){// no need to write with -u
					size_f2 = size_f2 - strlen(line_f2) - 1;
					line_f2 = strsep(&buf_f2, "\n");
					continue;
				}
				
				// if bigger than output buffer, write to file
				if(strlen(line_f2)+size_tem>=size_page)
					writebytes += write_file(filp_tem, buf_tem, &size_tem, &pos_tem);

				pre_f2 = line_f2;
				concat_to_op(buf_tem, &size_tem, &size_f2, line_f2);
				line_f2 = strsep(&buf_f2,"\n");

			}else if(cp<0){ // line_f1 < line_f2
				if(check_sorted(pre_f1, line_f1, flag_i) > 0){
					if(flag_t)
						writebytes = -EINVAL;
					else
						writebytes += write_file(filp_tem, buf_tem, &size_tem, &pos_tem);
					goto exit;

				}else if(check_sorted(pre_f1, line_f1, flag_i)==0 && flag_u){
					size_f1 = size_f1 - strlen(line_f1) - 1;
					line_f1 = strsep(&buf_f1, "\n");
					continue;
				}

				if(strlen(line_f1)+size_tem>=size_page)
					writebytes += write_file(filp_tem, buf_tem, &size_tem, &pos_tem);

				pre_f1 = line_f1;
				concat_to_op(buf_tem, &size_tem, &size_f1, line_f1);
				line_f1 = strsep(&buf_f1, "\n");

			}else{
				// line_f1 == line_f2
				if(flag_u){
					size_f2 = size_f2-strlen(line_f2)-1;
					line_f2 = strsep(&buf_f2, "\n");
					
					if(strlen(line_f1)+size_tem>=size_page)
						writebytes += write_file(filp_tem, buf_tem, &size_tem, &pos_tem);
					
					pre_f1 = line_f1;
					concat_to_op(buf_tem, &size_tem, &size_f1, line_f1);
					line_f1 = strsep(&buf_f1, "\n");
				
				}
				else{ // suppose default is write all
					if(strlen(line_f1)+size_tem>=size_page)
						writebytes += write_file(filp_tem, buf_tem, &size_tem, &pos_tem);
					
					pre_f1 = line_f1;
					concat_to_op(buf_tem, &size_tem, &size_f1, line_f1);
					line_f1 = strsep(&buf_f1, "\n");
					
					if(strlen(line_f2)+size_tem>=size_page)
						writebytes += write_file(filp_tem, buf_tem, &size_tem, &pos_tem);
					
					pre_f2 = line_f2;
					concat_to_op(buf_tem, &size_tem, &size_f2, line_f2);
					line_f2 = strsep(&buf_f2, "\n");
				}
				
			}
		}
		
		// one of the read buffers ends ... 
		while(strlen(line_f1)!=0){
			
			if(check_sorted(pre_f1, line_f1, flag_i) > 0){
				if(flag_t)
					writebytes = -EINVAL;
				else
					writebytes += write_file(filp_tem, buf_tem, &size_tem, &pos_tem);
				goto exit;
				
			}else if(check_sorted(pre_f1, line_f1, flag_i)==0 && flag_u){
				size_f1 = size_f1 - strlen(line_f1) - 1;
				line_f1 = strsep(&buf_f1, "\n");
				continue;
			}

			if(strlen(line_f1)+size_tem>=size_page)
				writebytes += write_file(filp_tem, buf_tem, &size_tem, &pos_tem);

			pre_f1 = line_f1;
			concat_to_op(buf_tem, &size_tem, &size_f1, line_f1);
			line_f1 = strsep(&buf_f1, "\n");
			
		}
		while(strlen(line_f2)!=0){
			
			if(check_sorted(pre_f2, line_f2, flag_i) > 0){
				if(flag_t)
					writebytes = -EINVAL;
				else
					writebytes += write_file(filp_tem, buf_tem, &size_tem, &pos_tem);
				goto exit;
				
			}else if(check_sorted(pre_f2, line_f2, flag_i)==0 && flag_u){// no need to write with -u
				size_f2 = size_f2 - strlen(line_f2) - 1;
				line_f2 = strsep(&buf_f2, "\n");
				continue;
			}
			
			if(strlen(line_f2)+size_tem>=size_page)
				writebytes += write_file(filp_tem, buf_tem, &size_tem, &pos_tem);

			pre_f2 = line_f2;
			concat_to_op(buf_tem, &size_tem, &size_f2, line_f2);
			line_f2 = strsep(&buf_f2, "\n");
			
		}
		// because sum of two buffers is two time as write buffer, so code below may only execute once if needed.
		writebytes += write_file(filp_tem, buf_tem, &size_tem, &pos_tem);
	 
		buf_f1 = buf_f1_bp;
		buf_f2 = buf_f2_bp;
			
	}

	while(size_f1>0){
	
		readbytes = read_file(filp_f1, buf_f1, size_page, &pos_f1);
		line_f1 = strsep(&buf_f1, "\n");
		
		while(strlen(line_f1)!=0){
			
			if(check_sorted(pre_f1, line_f1, flag_i) > 0){
				if(flag_t)
					writebytes = -EINVAL;
				else
					writebytes += write_file(filp_tem, buf_tem, &size_tem, &pos_tem);
				goto exit;
				
			}else if(check_sorted(pre_f1, line_f1, flag_i)==0 && flag_u){
				size_f1 = size_f1 - strlen(line_f1) - 1;
				line_f1 = strsep(&buf_f1, "\n");
				continue;
			}

			if(strlen(line_f1)+size_tem>=size_page)
				writebytes += write_file(filp_tem, buf_tem, &size_tem, &pos_tem);

			pre_f1 = line_f1;
			concat_to_op(buf_tem, &size_tem, &size_f1, line_f1);
			line_f1 = strsep(&buf_f1, "\n");
			
		}

		
	}

	while(size_f2>0){
	
		readbytes = read_file(filp_f2, buf_f2, size_page, &pos_f2);
		line_f2 = strsep(&buf_f2, "\n");

		while(strlen(line_f2)!=0){
			
			if(check_sorted(pre_f2, line_f2, flag_i) > 0){
				if(flag_t)
					writebytes = -EINVAL;
				else
					writebytes += write_file(filp_tem, buf_tem, &size_tem, &pos_tem);
				goto exit;
				
			}else if(check_sorted(pre_f2, line_f2, flag_i)==0 && flag_u){// no need to write with -u
				size_f2 = size_f2 - strlen(line_f2) - 1;
				line_f2 = strsep(&buf_f2, "\n");
				continue;
			}
			
			if(strlen(line_f2)+size_tem>=size_page)
				writebytes += write_file(filp_tem, buf_tem, &size_tem, &pos_tem);

			pre_f2 = line_f2;
			concat_to_op(buf_tem, &size_tem, &size_f2, line_f2);
			line_f2 = strsep(&buf_f2, "\n");
			
		}
	}
	
	writebytes += write_file(filp_tem, buf_tem, &size_tem, &pos_tem);


 exit:
	if(buf_tem)
		kfree(buf_tem);
	if(buf_f2)
		kfree(buf_f2);
	if(buf_f1)
		kfree(buf_f1);

	return writebytes;
}

asmlinkage long mergesort(void *arg)
{
        int rc = 0;
	int writebytes = 0;
        
	unsigned int flag_d = 0;

	struct kernel_args *args = NULL;
	struct file *filp_f1 = NULL;
	struct file *filp_f2 = NULL;
	struct file *filp_op = NULL;
	struct file *filp_tem = NULL;
	struct dentry* dentry_op = NULL;
	struct dentry* dentry_tem = NULL;
	struct filename *name_f1 = NULL;
	struct filename *name_f2 = NULL;
	struct filename *name_op = NULL;
	
	
	//validation
	if (!arg)
		return -EINVAL;
	
	args = (struct kernel_args *) kmalloc(sizeof(struct kernel_args), GFP_KERNEL);
	
	if (!args){
	        return -ENOMEM;
	}

	rc = copy_from_user(args, arg, sizeof(struct kernel_args));
	if(rc){
		rc = -EFAULT;
		goto exit;
	}

	name_f1 = getname(args->in_files[0]);
	name_f2 = getname(args->in_files[1]);
	name_op = getname(args->out_file);
        
	if(IS_ERR(name_f1)|IS_ERR(name_f2)|IS_ERR(name_op)){
                rc = PTR_ERR(name_f1)|PTR_ERR(name_f2)|PTR_ERR(name_op);
		goto name_fail;
	}

	filp_f1 = filp_open(name_f1->name, O_RDONLY, 0);
	if(IS_ERR(filp_f1)){
	        rc = PTR_ERR(filp_f1);
		filp_f1 = NULL;
		goto name_fail;
	}
	//printk("opened filp_f1. \n");

	filp_f2 = filp_open(name_f2->name, O_RDONLY, 0);
	if(IS_ERR(filp_f2)){
	        rc = PTR_ERR(filp_f2);
		filp_f2 = NULL;
		goto open_fail;
	}
	//printk("opened filp_f2. \n");
	
	filp_op = filp_open(name_op->name, O_RDWR|O_CREAT, 0644);
	if(IS_ERR(filp_op)){
	        rc = PTR_ERR(filp_op);
		filp_op = NULL;
		goto open_fail;
	}
	dentry_op = file_dentry(filp_op);
	//printk("opened filp_op. \n");

	filp_tem = filp_open("temp.txt", O_RDWR|O_CREAT, 0644);
	if(IS_ERR(filp_tem)){
	        rc = PTR_ERR(filp_tem);
		filp_tem = NULL;
		goto open_fail;
	}
	dentry_tem = file_dentry(filp_tem);
	//printk("opened filp_tem. \n");
	
	writebytes = merge(filp_f1, filp_f2, filp_tem, args->flags);
	if(writebytes<0){
		rc = writebytes;
		goto open_fail;
	}

	rc = vfs_rename(dentry_op->d_parent->d_inode, dentry_op, dentry_tem->d_parent->d_inode, dentry_tem, NULL, O_RDWR);
	if(rc<0){
		printk("vfs_rename err, cannot rename tem to output, cleanup! \n");
		goto open_fail;
	}
	
	rc = vfs_unlink(dentry_op->d_parent->d_inode, dentry_op, NULL);
	if(rc<0){
		printk("vfs_unlink err, cannot delete temp.txt. \n");
		goto open_fail;
	}
	
	flag_d = args->flags&(0x20);
	printk("flag_d: %x\n", flag_d);
	
	
	if(flag_d){
		rc = copy_to_user((void*)(args->data), (void*)&writebytes, sizeof(struct kernel_args));
		if(rc)
			rc = -EFAULT;
	}
	
	
 open_fail:
	if(filp_tem)
	        filp_close(filp_tem, NULL);
	if(filp_op)
	        filp_close(filp_op, NULL);
	if(filp_f2)
	        filp_close(filp_f2, NULL);
	if(filp_f1)
	        filp_close(filp_f1, NULL);

 name_fail:
	if(name_op)
		putname(name_op);
	if(name_f2)
		putname(name_f2);
	if(name_f1)
		putname(name_f1);

exit:
	kfree(args);
	return rc;
}

static int __init init_sys_mergesort(void)
{
        printk("installed new sys_mergesort module\n");
	if (sysptr == NULL)
		sysptr = mergesort;
	return 0;
}
static void  __exit exit_sys_mergesort(void)
{
	if (sysptr != NULL)
		sysptr = NULL;
	printk("removed sys_mergesort module\n");
}
module_init(init_sys_mergesort);
module_exit(exit_sys_mergesort);
MODULE_LICENSE("GPL");
