# Exercise 3 - UNIX System Programming 

## This application was created by Yoav Saroya & Amit Shmuel 

Notes:
	
- Please try to run it at least 2-3 times. 
- the read_user sometimes flies on initializing.
	
- We assumed that synchronizing the video and playing audio is not important to this exercise.
	
- We assumed that the resolution of the video is not important to this exercise.
	
- Please note that writing 5+ videos to kernel - decreases the performence.
	


Compile + insert the devices:
	
- Compile the project with 'make' command.
	
- sudo insmod write_chardev.ko
	
- sudo insmod read_chardev.ko
	
- sudo mknod /dev/write_char_dev c 101 0
	
- sudo mknod /dev/read_char_dev c 100 0
	


Clean + removing the devices:
	Clean the project and any other cruft with 'make clean' command.
	
- sudo rmmod read_chardev
	
- sudo rmmod write_chardev
	
- sudo rm /dev/write_char_dev
	
- sudo rm /dev/read_char_dev
	


How to run:
	
- run the write_user.out application with up-to 10 arguments of video filenames.
	   
  Usage: .exe <video1> <video2>... <video10>
	   
  Example: ./write_user.out movie.mp4 movie2.mp4
	
- run the read_user.out application.
	

### PLEASE quit first read_user app and only then quit write_user app.
	
	


How to use:

	
- write_user app:
 q - stop writing to kernel and quit the application.

	
- read_user app:
  q- close the video screen and quit the application.
		
- 1,2,....,0 keys - change the video