/*
 *  read_chardev.c - Create an output character device
 */
#include <linux/kernel.h>	/* We're doing kernel work */
#include <linux/module.h>	/* Specifically, a module */
#include <linux/fs.h>
#include <asm/uaccess.h>	/* for get_user and put_user */
#include "chardev.h"
#define DEVICE_NAME "read_char_dev"

/* 
 * Is the device open right now? Used to prevent
 * concurent access into the same device 
 */
static int Device_Open = 0;

/* 
 * The cameras holding frames from user
 */
extern char cameras[CAM_NUM][CAM_LEN];

/* 
 * used for seeking
 */
extern char *cam_ptr;

/*
 * used for changing tape
 */
extern int cur_cam;

/*
 * so we won't read empty cam
 */
extern int written_to_cam[CAM_NUM];

/*
 * represents the size of serialized buf
 */
extern size_t size_of_buf;

/*
 * spinlock for cameras
 */
extern spinlock_t my_lock;

/*
 * validate that we don't open read_user application to read nothing.
 */
extern int write_user;

/* 
 * This is called whenever a process attempts to open the device file 
 */
static int device_open(struct inode *inode, struct file *file) {

#ifdef DEBUG
	printk(KERN_INFO "device_open(%p)\n", file);
#endif

	// We don't want to talk to two processes at the same time 
	if (Device_Open)
		return -EBUSY;
	Device_Open++;
	spin_lock_init(&my_lock);

	// Initialize the message 
	cam_ptr = cameras[cur_cam];
	try_module_get(THIS_MODULE);
	return SUCCESS;
}

static int device_release(struct inode *inode, struct file *file) {

#ifdef DEBUG
	printk(KERN_INFO "device_release(read_chardev,%p,%p)\n", inode, file);
#endif
	// We're now ready for our next caller 
	Device_Open--;
	module_put(THIS_MODULE);
	return SUCCESS;
}

/* 
 * This function is called whenever a process which has already opened the
 * device file attempts to read from it.
 */
static ssize_t device_read(struct file *file, char __user *buffer, size_t length, loff_t *offset) {

	int bytes_read = 0;

#ifdef DEBUG
	printk(KERN_INFO "device_read(%p,%p,%lu)\n", file, buffer, length);
#endif

	cam_ptr = cameras[cur_cam];

	spin_lock(&my_lock);
	//Actually put the data into the buffer
	while (length) {
		/* 
		 * Because the buffer is in the user data segment,
		 * not the kernel data segment, assignment wouldn't work.
		 * Instead, we have to use put_user which copies data
		 * from the kernel data segment to the user data segment.
		 */
		put_user(*(cam_ptr++), buffer++);
		length--;
		bytes_read++;
	}
	spin_unlock(&my_lock);

#ifdef DEBUG
	printk(KERN_INFO "Read %d bytes, %lu left\n", bytes_read, length);
#endif

	return bytes_read;
}


/* 
 * This function is called whenever a process tries to do an ioctl on our
 * device file. We get two extra parameters (additional to the inode and file
 * structures, which all device functions get): the number of the ioctl called
 * and the parameter given to the ioctl function.
 *
 * If the ioctl is write or read/write (meaning output is returned to the
 * calling process), the ioctl call returns the output of this function.
 *
 */
int device_ioctl(struct file *file, unsigned int ioctl_num, unsigned long ioctl_param) {

	int i;
	//Switch according to the ioctl called  
	switch (ioctl_num) {
	case IOCTL_READ:
		/* 
		 * Give the current message to the calling process - 
		 * the parameter we got is a pointer, fill it. 
		 */
		while(!written_to_cam[cur_cam]);
		i = device_read(file, (char *)ioctl_param, size_of_buf-sizeof(size_t), 0);
		break;

	case IOCTL_GET_VALIDATE:
		return write_user;
		// return size_of_buf;

	case IOCTL_CHANGE_TAPE:
		cur_cam = (int)ioctl_param;
		break;

	case IOCTL_GET_TAPE_NUMBER:
		return cur_cam;

	case IOCTL_CHECK_IF_WRITTEN:
		return written_to_cam[ioctl_param];
	}
	return SUCCESS;
}

/* Module Declarations */

/* 
 * This structure will hold the functions to be called
 * when a process does something to the device we created.
 * Since a pointer to this structure is kept in
 * the devices table, it can't be local to
 * init_module. NULL is for unimplemented functions. 
 */
struct file_operations Fops = {

	.read = device_read,
	.unlocked_ioctl = (void*)device_ioctl,
	.open = device_open,
	.release = device_release,
};


/* 
 * Initialize the module - Register the character device 
 */
int init_module() {

	int ret_val;
	
	// Register the character device (atleast try) 
	ret_val = register_chrdev(READ_MAJOR_NUM, DEVICE_NAME, &Fops);

	if (ret_val < 0) {
		printk(KERN_ALERT "Failed registering the char device with %d\n", ret_val);
		return ret_val;
	}

	printk(KERN_INFO "================ Registeration is a success ================\n");
	printk(KERN_INFO "The major device number is %d.\n", READ_MAJOR_NUM);
	printk(KERN_INFO "If you want to talk to the device driver,\n");
	printk(KERN_INFO "you'll have to create a device file\n");
	printk(KERN_INFO "We suggest you use:\n");
	printk(KERN_INFO "mknod %s c %d 0\n", DEVICE_FILE_NAME_R, READ_MAJOR_NUM);
	printk(KERN_INFO "The device file name is important, because\n");
	printk(KERN_INFO "the ioctl program assumes that's the file you'll use.\n");

	return 0;
}

/* 
 * Cleanup - unregister the appropriate file from /proc 
 */
void cleanup_module() {

	printk(KERN_INFO "======== Unregistering %s, with major number %d ========\n", DEVICE_NAME, READ_MAJOR_NUM);
	unregister_chrdev(READ_MAJOR_NUM, DEVICE_NAME);
}
