/*
 *  write_chardev.c - Create an input character device
 */
#include <linux/kernel.h>	/* We're doing kernel work */
#include <linux/module.h>	/* Specifically, a module */
#include <linux/fs.h>
#include <asm/uaccess.h>	/* for get_user and put_user */
#include "chardev.h"
#define DEVICE_NAME "write_char_dev"

/* 
 * The cameras holding frames from user
 */
char cameras[CAM_NUM][CAM_LEN];
EXPORT_SYMBOL(cameras);

/* 
 * used for seeking
 */
char *cam_ptr;
EXPORT_SYMBOL(cam_ptr);

/*
 * used for changing tape
 */
int cur_cam = 1;
EXPORT_SYMBOL(cur_cam);

/*
 * represent the size of the serialized buf
 */
size_t size_of_buf;
EXPORT_SYMBOL(size_of_buf);

int written_to_cam[CAM_NUM] = {0,0,0,0,0,0,0,0,0,0};
EXPORT_SYMBOL(written_to_cam);

/*
 * spin lock for the cameras
 */
DEFINE_SPINLOCK(my_lock);
EXPORT_SYMBOL(my_lock);


int write_user = 0;
EXPORT_SYMBOL(write_user);

/* 
 * Is the device open right now? Used to prevent
 * concurent access into the same device 
 */
static int Device_Open = 0;

/*
 * Implementation at the bottom
 */
static ssize_t device_read(struct file *file, char __user *buffer, size_t length, loff_t *offset);

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
	write_user = 1;
	// Initialize the cameras 
	cam_ptr = cameras[cur_cam];
	try_module_get(THIS_MODULE);
	return SUCCESS;
}

static int device_release(struct inode *inode, struct file *file) {

	int i;
#ifdef DEBUG
	printk(KERN_INFO "device_release(write_chardev,%p,%p)\n", inode, file);
#endif

	// We're now ready for our next caller 
	Device_Open--;
	write_user = 0;
	cur_cam = 1;
	for(i =0; i < CAM_NUM; i++) written_to_cam[i] = 0;
	module_put(THIS_MODULE);
	return SUCCESS;
}


/* 
 * This function is called when somebody tries to
 * write into our device file. 
 */
static ssize_t device_write(struct file *file, const char __user *buffer, size_t length, loff_t *offset) {

	int i, cam_number;

#ifdef DEBUG
	printk(KERN_INFO "device_write(%p,%s,%lu)", file, buffer, length);
#endif
	
	spin_lock(&my_lock);
	memcpy(&cam_number,buffer,sizeof(int));

	buffer+=sizeof(int);
	for (i = 0; i < length && i < CAM_LEN; i++)
		get_user(cameras[cam_number][i], buffer + i); // get from user
	
	//written_to_cam[cam_number] = 1;

	spin_unlock(&my_lock);
	written_to_cam[cam_number] = 1;
	// Again, return the number of input characters used 
	return i;
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
int device_ioctl(struct file *file,unsigned int ioctl_num,unsigned long ioctl_param) {

	// Switch according to the ioctl called 
	switch (ioctl_num) {
	
		case IOCTL_WRITE:
			/* 
			 * Receive a pointer to a frame data buf (in user space) 
			 * and set that to be the device's frame. 
			 * Get the parameter given to ioctl by the process. 
			 */
	 
			// Find the length of the camera
			
			/*
			 * if we want the length from the buffer, maybe use it later in the project
			 */
			memcpy(&size_of_buf, (char*)ioctl_param, sizeof(size_t));
			return device_write(file,(char *) (ioctl_param+sizeof(size_t)),size_of_buf-sizeof(size_t),0);
	}

	return SUCCESS;
}

/* Module Declarations */

/* 
 * This structure will hold the functions to be called
 * when a process does something to the device we
 * created. Since a pointer to this structure is kept in
 * the devices table, it can't be local to
 * init_module. NULL is for unimplemented functions. 
 */
struct file_operations Fops = {

	.write = device_write,
	.read = device_read, // for cat 
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
	ret_val = register_chrdev(WRITE_MAJOR_NUM, DEVICE_NAME, &Fops);

	if (ret_val < 0) {
		printk(KERN_ALERT "Failed registering the char device with %d\n", ret_val);
		return ret_val;
	}

	printk(KERN_INFO "================ Registeration is a success ================\n");
	printk(KERN_INFO "The major device number is %d.\n", WRITE_MAJOR_NUM);
	printk(KERN_INFO "If you want to talk to the device driver,\n");
	printk(KERN_INFO "you'll have to create a device file. \n");
	printk(KERN_INFO "We suggest you use:\n");
	printk(KERN_INFO "mknod %s c %d 0\n", DEVICE_FILE_NAME_W, WRITE_MAJOR_NUM);
	printk(KERN_INFO "The device file name is important, because\n");
	printk(KERN_INFO "the ioctl program assumes that's the file you'll use.\n");

	return 0;
}

/* 
 * Cleanup - unregister the appropriate file from /proc 
 */
void cleanup_module() {

	printk(KERN_INFO "======== Unregistering %s, with major number %d ========\n", DEVICE_NAME, WRITE_MAJOR_NUM);
	unregister_chrdev(WRITE_MAJOR_NUM, DEVICE_NAME);
}


/* 
 * USED FOR CAT CHECKING PORPUSE, THIS MODULE DOES NOT SEND DATA TO USER !!! 
 * ============= SEE FULLY COMMENTED FUNC IN READ_CHARDEV.C =============
 * This function is called whenever a process which has already opened the
 * device file attempts to read from it.
 */
static ssize_t device_read(struct file *file, char __user *buffer, size_t length, loff_t *offset) {
	int bytes_read = 0;
#ifdef DEBUG
	printk(KERN_INFO "device_read(%p,%p,%lu)\n", file, buffer, length);
#endif
	if (*cam_ptr == 0) 
		return 0;
	while (length && *cam_ptr) {
		put_user(*(cam_ptr++), buffer++);
		length--;
		bytes_read++;
	}
#ifdef DEBUG
	printk(KERN_INFO "Read %d bytes, %lu left\n", bytes_read, length);
#endif
	return bytes_read;
}
