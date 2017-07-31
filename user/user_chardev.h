/*
 *  chardev.h - the header file with the ioctl definitions.
 *
 *  The declarations here have to be in a header file, because
 *  they need to be known both to the kernel module
 *  (in chardev.c) and the process calling ioctl (read_user.c & write_user.c)
 */

#ifndef CHARDEV_H
#define CHARDEV_H

#define DEBUG
#define FAIL -1
#define SUCCESS 0

#define MAX_FRAME_SIZE 10000000000

#include <linux/ioctl.h>

/* 
 * The major device number. We can't rely on dynamic 
 * registration any more, because ioctls need to know 
 * it. 
 */
#define READ_MAJOR_NUM 100
#define WRITE_MAJOR_NUM 101


#define IOCTL_WRITE _IOR(WRITE_MAJOR_NUM, 0, char *)

#define IOCTL_READ _IOW(READ_MAJOR_NUM, 1, char *)
#define IOCTL_CHANGE_TAPE _IOR(READ_MAJOR_NUM, 2, int)
#define IOCTL_GET_VALIDATE _IO(READ_MAJOR_NUM, 3)
#define IOCTL_GET_TAPE_NUMBER _IO(READ_MAJOR_NUM,4)
#define IOCTL_CHECK_IF_WRITTEN _IOR(READ_MAJOR_NUM, 5, int)

/* 
 * The name of the device file 
 */
#define DEVICE_FILE_NAME_R "/dev/read_char_dev"
#define DEVICE_FILE_NAME_W "/dev/write_char_dev"

/*
 * 4 cameras of 1.5MB each
 */
#define CAM_NUM 10
#define CAM_LEN 1500000

#endif
