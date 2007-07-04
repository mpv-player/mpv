/* Small program to test the features of vf_bmovl */

#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <stdlib.h>
#include <SDL/SDL.h>
#include <SDL/SDL_image.h>

#define DEBUG 0

static void
blit(int fifo, unsigned char *bitmap, int width, int height,
     int xpos, int ypos, int alpha, int clear)
{
	char str[100];
	int  nbytes;
	
	sprintf(str, "RGBA32 %d %d %d %d %d %d\n",
	        width, height, xpos, ypos, alpha, clear);
	
	if(DEBUG) printf("Sending %s", str);

	write(fifo, str, strlen(str));
	nbytes = write(fifo, bitmap, width*height*4);

	if(DEBUG) printf("Sent %d bytes of bitmap data...\n", nbytes);
}

static void
set_alpha(int fifo, int width, int height, int xpos, int ypos, int alpha) {
	char str[100];

	sprintf(str, "ALPHA %d %d %d %d %d\n",
	        width, height, xpos, ypos, alpha);
	
	if(DEBUG) printf("Sending %s", str);

	write(fifo, str, strlen(str));
}

static void
paint(unsigned char* bitmap, int size, int red, int green, int blue, int alpha) {

	int i;

	for(i=0; i < size; i+=4) {
		bitmap[i+0] = red;
		bitmap[i+1] = green;
		bitmap[i+2] = blue;
		bitmap[i+3] = alpha;
	}
}

int main(int argc, char **argv) {

	int fifo=-1;
	int width=0, height=0;
	unsigned char *bitmap;
	SDL_Surface *image;
	int i;

	if(argc<3) {
		printf("Usage: %s <bmovl fifo> <image file> <width> <height>\n", argv[0]);
		printf("width and height are w/h of MPlayer's screen!\n");
		exit(10);
	}

	width = atoi(argv[3]);
	height = atoi(argv[4]);

	fifo = open( argv[1], O_RDWR );
	if(!fifo) {
		fprintf(stderr, "Error opening FIFO %s!\n", argv[1]);
		exit(10);
	}

	image = IMG_Load(argv[2]);
	if(!image) {
		fprintf(stderr, "Couldn't load image %s!\n", argv[2]);
		exit(10);
	}

	printf("Loaded image %s: width=%d, height=%d\n", argv[2], image->w, image->h);

	// Display and move image
	for(i=0; (i < (width - image->w)) && (i < (height - image->h)); i += 5)
		blit(fifo, image->pixels, image->w, image->h, i, i, 0, 1);

	// Create a 75x75 bitmap
	bitmap = (unsigned char*)malloc(75*75*4);

	// Paint bitmap red, 50% transparent and blit at position 50,50
	paint(bitmap, (75*75*4), 255, 0, 0, 128);
	blit(fifo, bitmap, 75, 75, 50, 50, 0, 0);

	// Paint bitmap green, 50% transparent and blit at position -50,50
	paint(bitmap, (75*75*4), 0, 255, 0, 128);
	blit(fifo, bitmap, 75, 75, width-50-75, 50, 0, 0);

	// Paint bitmap blue, 50% transparent and blit at position -50,50
	paint(bitmap, (75*75*4), 0, 0, 255, 128);
	blit(fifo, bitmap, 75, 75, 50, height-50-75, 0, 0);

	// Blit another image in the middle, completly transparent
	blit(fifo, image->pixels, image->w, image->h,
	           (width/2)-(image->w/2), (height/2)-(image->h/2), -255, 0);

	// Fade in image
	for(i=-255; i <= 0; i++)
		set_alpha(fifo, image->w, image->h,
		          (width/2)-(image->w/2), (height/2)-(image->h/2), i);
	

	// Clean up
	free(bitmap);
	SDL_FreeSurface(image);

	return 0;
}
