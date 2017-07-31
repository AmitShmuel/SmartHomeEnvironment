/*
clean*  read_user.c - the process to use ioctl's to control the kernel module
 */

/* 
 * device specifics, such as ioctl numbers and the
 * major device file. 
 */

#include <SDL.h>
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>

#ifdef __MINGW32__
#undef main /* Prevents SDL from overriding main() */
#endif

#include "user_chardev.h"

#include <stdio.h>
#include <fcntl.h>		/* open */
#include <unistd.h>		/* exit */
#include <sys/ioctl.h>		/* ioctl */
#include <pthread.h>

int quit = 0;

/*
 * Deserializing the buf to the overlay
 */ 
void buf_to_overlay(SDL_Overlay *bmp, char* buf) {

	static int first_call = 1;

	if(!bmp || !buf) {
		perror("to_message");
		exit(-1);
	}

	memcpy(&bmp->format, buf, sizeof(Uint32));
	buf += sizeof(Uint32);

	memcpy(&bmp->w, buf, sizeof(int));
	buf += sizeof(int);

	memcpy(&bmp->h, buf, sizeof(int));
	buf += sizeof(int);

	memcpy(&bmp->planes, buf, sizeof(int));
	buf += sizeof(int);

	memcpy(&bmp->pitches[0], buf, sizeof(Uint16));
	buf += sizeof(Uint16);

	memcpy(&bmp->pitches[1], buf, sizeof(Uint16));
	buf += sizeof(Uint16);

	memcpy(&bmp->pitches[2], buf, sizeof(Uint16));
	buf += sizeof(Uint16);

	if(first_call) {
		first_call = 0;
		bmp->pixels[0] = (Uint8*)malloc(sizeof(Uint8)*bmp->h*bmp->w);
		bmp->pixels[1] = (Uint8*)malloc(sizeof(Uint8)*bmp->h*bmp->w);
		bmp->pixels[2] = (Uint8*)malloc(sizeof(Uint8)*(bmp->h-90)*bmp->w);
	}
	memcpy(bmp->pixels[0], buf, sizeof(Uint8)*bmp->h*bmp->w);
	buf += sizeof(Uint8)*bmp->h*bmp->w;

	memcpy(bmp->pixels[1], buf, sizeof(Uint8)*bmp->h*bmp->w);
	buf += sizeof(Uint8)*bmp->h*bmp->w;

	memcpy(bmp->pixels[2], buf, sizeof(Uint8)*(bmp->h-90)*bmp->w);
}

/* 
 * Functions for the ioctl calls 
 */
void ioctl_get_msg(void* arg) {

	int ret_val, *file_desc = (int*)arg;
	char* buf;
	SDL_Rect rect;
	rect.x = rect.y = 0;

	if (!ioctl(*file_desc, IOCTL_GET_VALIDATE)) {
		quit = 1;
		printf("write_user app must be opened\n");
		exit(-1);
	}

	// my_bmp, will be deserialized next
	SDL_Overlay *my_bmp = NULL;
	my_bmp = SDL_CreateYUVOverlay(1, 1, SDL_YV12_OVERLAY, SDL_SetVideoMode(640, 360, 0, 0));

	buf = (char*)malloc(MAX_FRAME_SIZE);
	while(!quit) {
		/* 
		 * Warning - this is dangerous because we don't tell
		 * the kernel how far it's allowed to write, so it
		 * might overflow the buffer. In a real production
		 * program, we would have used two ioctls - one to tell
		 * the kernel the buffer length and another to give
		 * it the buffer to fill
		 */
		ret_val = ioctl(*file_desc, IOCTL_READ, buf);
		if (ret_val < 0) {
			printf("ioctl_get_msg failed: %d\n", ret_val);
			exit(-1);
		}
		buf_to_overlay(my_bmp, buf);
		rect.w = my_bmp->w;
		rect.h = my_bmp->h;
		SDL_DisplayYUVOverlay(my_bmp, &rect);
	}
	// we need to see how we tell this process that the video is finished..

	free(my_bmp->pixels[0]);
	free(my_bmp->pixels[1]);
	free(my_bmp->pixels[2]);
	free(buf);
}

void ioctl_ch_tape(int file_desc, int n) {

	int ret_val = ioctl(file_desc, IOCTL_CHANGE_TAPE, n);
	if (ret_val < 0)  printf("ioctl_ch_tape failed: %d\n", ret_val);
	else printf("the selected tape is now tape %d\n", n+1);
}

/* 
 * Main - FRONTEND, Call the ioctl functions 
 */
int main() {

	SDL_Event event;
	int file_desc, rc, choice, selected_tape;
	pthread_t thread;

	av_register_all();

	if(SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_TIMER)) {
		fprintf(stderr, "Could not initialize SDL - %s\n", SDL_GetError());
		exit(1);
	}

	file_desc = open(DEVICE_FILE_NAME_R, 0);
	if (file_desc < 0) {
		printf("Can't open device file: %s\n", DEVICE_FILE_NAME_R);
		exit(-1);
	}
	sleep(2);	//We have to wait till write_user writes at least 1 frame from each video
	rc = pthread_create(&thread, NULL, (void*)ioctl_get_msg, &file_desc);
	if(rc) {
		fprintf(stderr,"ERROR; return code from pthread_create() is %d\n", rc);
		exit(-1);
	}

	selected_tape = ioctl(file_desc, IOCTL_GET_TAPE_NUMBER);
	if (!ioctl(file_desc ,IOCTL_CHECK_IF_WRITTEN,1)) {choice = 0; goto change_tape;}
	while(SDL_WaitEvent( &event ) && !quit) {
		switch(event.type) {
		case SDL_KEYDOWN:
			switch(event.key.keysym.sym) {
			case SDLK_q:
				quit = 1;
				break;

			case SDLK_1:
				choice = 0;
				goto change_tape;

			case SDLK_2:
				choice = 1;
				goto change_tape;

			case SDLK_3:
				choice = 2;
				goto change_tape;

			case SDLK_4:
				choice = 3;
				goto change_tape;

			case SDLK_5:
				choice = 4;
				goto change_tape;

			case SDLK_6:
				choice = 5;
				goto change_tape;
			case SDLK_7:
				choice = 6;
				goto change_tape;

			case SDLK_8:
				choice = 7;
				goto change_tape;

			case SDLK_9:
				choice = 8;
				goto change_tape;

			case SDLK_0:
				choice = 9;
				goto change_tape;

				change_tape:
				if(selected_tape == choice)
					printf("tape %d is already the selected tape\n", selected_tape+1);
				else if(!ioctl(file_desc ,IOCTL_CHECK_IF_WRITTEN,choice)) {
					if (choice == 0) continue;
					printf("tape %d has not been written by write_user app\n", choice+1);
				}
				else {
					selected_tape = choice;
					ioctl_ch_tape(file_desc, choice);
				}
				break;
			default: break;
			}
			break;
			default: break;
		}
	}
	close(file_desc);
	return 0;
}
