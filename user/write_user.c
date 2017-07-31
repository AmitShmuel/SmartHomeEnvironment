/*
 *  write_user.c - the process to use ioctl's to control the kernel module
 */

/* 
 * device specifics, such as ioctl numbers and the
 * major device file. 
 */

#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libswscale/swscale.h>

#include <SDL.h>

#ifdef __MINGW32__
#undef main /* Prevents SDL from overriding main() */
#endif

#include "user_chardev.h"

#include <stdio.h>
#include <fcntl.h>		/* open */
#include <unistd.h>		/* exit */
#include <sys/ioctl.h>		/* ioctl */
#include <pthread.h>

pthread_mutex_t lock;

typedef struct VideoState {

	AVFormatContext   *pFormatCtx;
	int               i, videoStream;
	AVCodecContext    *pCodecCtx;
	AVCodec           *pCodec;
	AVFrame           *pFrame;
	AVPacket          packet;
	int               frameFinished;

	AVDictionary      *optionsDict;
	struct SwsContext *sws_ctx;

	SDL_Overlay       *bmp;
	SDL_Surface       *screen;

	int 			file_desc, tape, quit;

} VideoState;


size_t size_of_Overlay(SDL_Overlay *bmp) {
	/*
	 * typedef struct  {
	 *	
	 *	Uint32 format;
	 *	int w, h;
	 * 	int planes;
	 * 	Uint16 *pitches;
	 *	Uint8  **pixels;
	 *	Uint32 hw_overlay:1; <- doesn't being accounted
	 *	
	 *  } SDL_Overlay;
	 * one more int for camera number */
	return sizeof(int)*4 + sizeof(Uint32) + sizeof(Uint16)*3 + sizeof(Uint8)*bmp->h*bmp->w*3 + sizeof(size_t) /*size of overlay*/;
}

void overlay_to_buf(SDL_Overlay* bmp, char* buf, int tape) {

	if(!bmp || !buf) {
		perror("overlay_to_buf");
		return exit(-1);
	}

	size_t size = size_of_Overlay(bmp);
	memcpy(buf, &size, sizeof(size_t));
	buf += sizeof(size_t);

	memcpy(buf, &tape, sizeof(int));
	buf += sizeof(int);

	memcpy(buf, &bmp->format, sizeof(Uint32));
	buf += sizeof(Uint32);

	memcpy(buf, &bmp->w, sizeof(int));
	buf += sizeof(int);

	memcpy(buf, &bmp->h, sizeof(int));
	buf += sizeof(int);

	memcpy(buf, &bmp->planes, sizeof(int));
	buf += sizeof(int);

	memcpy(buf, &bmp->pitches[0], sizeof(Uint16));
	buf += sizeof(Uint16);

	memcpy(buf, &bmp->pitches[1], sizeof(Uint16));
	buf += sizeof(Uint16);

	memcpy(buf, &bmp->pitches[2], sizeof(Uint16));
	buf += sizeof(Uint16);

	memcpy(buf, bmp->pixels[0], sizeof(Uint8)*bmp->h*bmp->w);	
	buf += sizeof(Uint8)*bmp->h*bmp->w;

	memcpy(buf, bmp->pixels[1], sizeof(Uint8)*bmp->h*bmp->w);	
	buf += sizeof(Uint8)*bmp->h*bmp->w;

	//original is -90
	memcpy(buf, bmp->pixels[2], sizeof(Uint8)*(bmp->h-200)*bmp->w);
}


void init_video(VideoState** is, char* filename, int file_desc) {

	int i;

	*is = av_mallocz(sizeof(VideoState));

	(*is)->quit = 0;

	(*is)->file_desc = file_desc;
	// Register all formats and codecs
	av_register_all();

	if(SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_TIMER)) {
		fprintf(stderr, "Could not initialize SDL - %s\n", SDL_GetError());
		exit(1);
	}
	// Open video file
	if(avformat_open_input(&((*is)->pFormatCtx), filename, NULL, NULL)!=0)
		exit(1); // Couldn't open file

	// Retrieve stream information
	if(avformat_find_stream_info((*is)->pFormatCtx, NULL)<0)
		exit(1); // Couldn't find stream information

	// Find the first video stream
	(*is)->videoStream=-1;
	for(i=0; i<(*is)->pFormatCtx->nb_streams; i++)
		if((*is)->pFormatCtx->streams[i]->codec->codec_type==AVMEDIA_TYPE_VIDEO) {
			(*is)->videoStream=i;
			break;
		}
	if((*is)->videoStream==-1)
		exit(1); // Didn't find a video stream

	// Get a pointer to the codec context for the video stream
	(*is)->pCodecCtx=(*is)->pFormatCtx->streams[(*is)->videoStream]->codec;

	// Find the decoder for the video stream
	(*is)->pCodec=avcodec_find_decoder((*is)->pCodecCtx->codec_id);
	if((*is)->pCodec==NULL) {
		fprintf(stderr, "Unsupported codec!\n");
		exit(1); // Codec not found
	}

	// Open codec
	if(avcodec_open2((*is)->pCodecCtx, (*is)->pCodec, &((*is)->optionsDict))<0)
		exit(1); // Could not open codec

	// Allocate video frame
	(*is)->pFrame=av_frame_alloc();

	// Make a screen to put our video
#ifndef __DARWIN__
	(*is)->screen = SDL_SetVideoMode(200, 200, 0, 0);
#else
	(*is)->screen = SDL_SetVideoMode(200, 200, 24, 0);
#endif
	if(!(*is)->screen) {
		fprintf(stderr, "SDL: could not set video mode - exiting\n");
		exit(1);
	}

	// Allocate a place to put our YUV image on that screen
	(*is)->bmp = SDL_CreateYUVOverlay((*is)->pCodecCtx->width, (*is)->pCodecCtx->height, SDL_YV12_OVERLAY, (*is)->screen);

	(*is)->sws_ctx = sws_getContext
			(
					(*is)->pCodecCtx->width,
					(*is)->pCodecCtx->height,
					(*is)->pCodecCtx->pix_fmt,
					(*is)->pCodecCtx->width,
					(*is)->pCodecCtx->height,
					PIX_FMT_YUV420P,
					SWS_BILINEAR, NULL, NULL, NULL
			);
}

/* 
 * Functions for the ioctl calls 
 */

void ioctl_set_msg(void* arg) {

	int 		  ret_val;
	VideoState**  is  = (VideoState**)arg;

	// will hold the serialized overlay data
	char          *buf = NULL;

	buf = (char*)malloc(size_of_Overlay((*is)->bmp));

	// Read frames
	while(av_read_frame((*is)->pFormatCtx, &(*is)->packet)>=0 && !(*is)->quit) {

		// Is this a packet from the video stream?
		if((*is)->packet.stream_index==(*is)->videoStream) {

			// Decode video frame
			avcodec_decode_video2((*is)->pCodecCtx, (*is)->pFrame, &(*is)->frameFinished, &(*is)->packet);

			// Did we get a video frame?
			if((*is)->frameFinished) {

				SDL_LockYUVOverlay((*is)->bmp);

				AVPicture pict;
				pict.data[0] = (*is)->bmp->pixels[0];
				pict.data[1] = (*is)->bmp->pixels[2];
				pict.data[2] = (*is)->bmp->pixels[1];

				pict.linesize[0] = (*is)->bmp->pitches[0];
				pict.linesize[1] = (*is)->bmp->pitches[2];
				pict.linesize[2] = (*is)->bmp->pitches[1];

				// Convert the image into YUV format that SDL uses
				sws_scale
				(
						(*is)->sws_ctx,
						(uint8_t const * const *)(*is)->pFrame->data,
						(*is)->pFrame->linesize, 0,
						(*is)->pCodecCtx->height,
						pict.data,
						pict.linesize
				);
				overlay_to_buf((*is)->bmp, buf, (*is)->tape);

				SDL_UnlockYUVOverlay((*is)->bmp);

				pthread_mutex_lock(&lock);

				ret_val = ioctl((*is)->file_desc, IOCTL_WRITE, buf);

				pthread_mutex_unlock(&lock);

				if (ret_val < 0) {
					printf("ioctl_set_msg failed: %d\n", ret_val);
					exit(-1);
				}
			}
		}
		av_free_packet(&(*is)->packet);
	}

	// Free allocated buff
	free(buf);

	// Free the YUV frame
	av_free((*is)->pFrame);

	// Close the codec
	avcodec_close((*is)->pCodecCtx);

	// Close the video file
	avformat_close_input(&(*is)->pFormatCtx);	
}


void ioctl_ch_tape(int file_desc, int n) {

	int ret_val = ioctl(file_desc, IOCTL_CHANGE_TAPE, n);

	if (ret_val < 0) 
		printf("ioctl_ch_tape failed: %d\n", ret_val);
	else
		printf("the selected tape is now tape %d\n", n);
}


/* 
 * Main - BACKEND, init videos and write serialized frames to kernel 
 */
int main(int argc, char* argv[]) {

	SDL_Event event;
	pthread_t thread;				// no used tapes at the beginning (0 = not used, 1 = used)
	int file_desc, rc, i, quit = 0, num_of_videos;
	VideoState* is_arr[10] = {NULL};

	if(argc == 1 || argc > 11) {
		fprintf(stderr, "Usage: .exe <video1> <video2>... <video10>\n");
		exit(1);
	}
	num_of_videos = argc-1;
	file_desc = open(DEVICE_FILE_NAME_W, 0);
	if (file_desc < 0) {
		printf("Can't open device file: %s\n", DEVICE_FILE_NAME_W);
		exit(-1);
	}

	for(i = 0; i < num_of_videos; i++)
		init_video(&is_arr[i], argv[i+1], file_desc);

	if(pthread_mutex_init(&lock, NULL) != 0) {
		printf("\n mutex init failed\n");
		exit(-1);
	}

	for(i = 0; i < num_of_videos; i++) {
			/*
			 * set the tape to the current tape,
			 * we need to serialize it to the buffer after, to let the kernel know
			 */
			is_arr[i]->tape = i;
			rc = pthread_create(&thread, NULL, (void*)ioctl_set_msg, &is_arr[i]);
			if(rc) {
				fprintf(stderr,"ERROR; return code from pthread_create() is %d\n", rc);
				exit(-1);
			}
			printf("tape number %d is written to kernel\n",i+1);
	}

	while(SDL_WaitEvent( &event ) && !quit) {
		switch(event.type) {
		case SDL_KEYDOWN:
			switch(event.key.keysym.sym) {
			case SDLK_q:
				for (i = 0; i < num_of_videos; i++) is_arr[i]->quit = 1;
				quit = 1;
				break;
			default: break;
			}
			break;
			default: break;
		}
	}
	pthread_mutex_destroy(&lock);
	close(file_desc);
	return 0;
}
