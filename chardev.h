/*
 *  chardev.h - the header file with the ioctl definitions.
 *
 *  The declarations here have to be in a header file, because
 *  they need to be known both to the kernel module
 *  (in chardev.c) and the process calling ioctl (read_user.c & write_user.c)
 */
#ifndef CHARDEV_H
#define CHARDEV_H

//#define DEBUG
#define FAIL -1
#define SUCCESS 0

#include <linux/ioctl.h>
#include <linux/spinlock.h>

/* 
 * The major device number. We can't rely on dynamic 
 * registration any more, because ioctls need to know 
 * it. 
 */
#define READ_MAJOR_NUM 100
#define WRITE_MAJOR_NUM 101

/* 
 * Set the message of the device driver 
 */
#define IOCTL_WRITE _IOR(WRITE_MAJOR_NUM, 0, char *)
/*
 * _IOR means that we're creating an ioctl command 
 * number for passing information from a user process
 * to the kernel module. 
 *
 * The first arguments, MAJOR_NUM, is the major device 
 * number we're using.
 *
 * The second argument is the number of the command 
 * (there could be several with different meanings).
 *
 * The third argument is the type we want to get from 
 * the process to the kernel.
 */

/* 
 * Get the message of the device driver 
 */
#define IOCTL_READ _IOW(READ_MAJOR_NUM, 1, char *)
/* 
 * _IOW means that we're creating an ioctl command 
 * number for passing information from the kernel module
 * to a user process.
 * 
 * This IOCTL is used for output, to get the message 
 * of the device driver. However, we still need the 
 * buffer to place the message in to be input, 
 * as it is allocated by the process.
 */

/* 
 * This IOCTL receives from the user a number, 0-9,
 * and change the current camera accordingly.
 */
//#define IOCTL_CHANGE_TAPE _IOR(WRITE_MAJOR_NUM, 2, int)
#define IOCTL_CHANGE_TAPE _IOR(READ_MAJOR_NUM, 2, int)


/* 
 * To know if write_user application is open or not
 */
#define IOCTL_GET_VALIDATE _IO(READ_MAJOR_NUM, 3)
/*
 * _IO means that we're creating an ioctl command 
 * number for passing information from the kernel module
 * to a user process, but we're not sending third parameter
 * because all we need is the information.
 */

/*
 * getting the number of the current tape that plays
 */
#define IOCTL_GET_TAPE_NUMBER _IO(READ_MAJOR_NUM,4)

/*
 * get to know if the tape number has been written or not
 */
#define IOCTL_CHECK_IF_WRITTEN _IOR(READ_MAJOR_NUM,5,int)


/* 
 * The name of the device file 
 */
#define DEVICE_FILE_NAME_R "/dev/read_char_dev"
#define DEVICE_FILE_NAME_W "/dev/write_char_dev"

/*
 * 10 cameras of 1.5MB each
 */
#define CAM_NUM 10
#define CAM_LEN 1500000

#endif
