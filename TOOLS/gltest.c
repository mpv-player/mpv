// OpenGL glTexSubImage() test/benchmark prg  (C) 2001. by A'rpi/ESP-team

#include <GL/glut.h>
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <inttypes.h>

// pixel size:  3 or 4
#define BYTES_PP 3

// blit by lines (defined) or frames (not defined)
#define FAST_BLIT

static uint32_t image_width=720;        // DVD size
static uint32_t image_height=576;

static uint32_t image_format;
static uint32_t image_bpp;
static uint32_t image_bytes;

static uint32_t texture_width=512;
static uint32_t texture_height=512;

static unsigned char *ImageData=NULL;

static GLvoid resize(int x,int y){
  printf("Resize: %dx%d\n",x,y);
  glViewport( 0, 0, x, y );

  glMatrixMode(GL_PROJECTION);
  glLoadIdentity();
  glOrtho(0, image_width, image_height, 0, -1,1);

  glMatrixMode(GL_MODELVIEW);
  glLoadIdentity();
}

float akarmi=0;

int counter=0;
float gen_time=0;
float up_time=0;
float render_time=0;

unsigned char sintable[4096];

extern float GetRelativeTime();

static void redraw(void)
{
//  glClear(GL_COLOR_BUFFER_BIT);
  int x,y,i;
  unsigned char *d=ImageData;
  int dstride=BYTES_PP*image_width;
  
  GetRelativeTime();
  
  // generate some image:
  for(y=0;y<image_height;y++){
    int y1=2048*sin(akarmi*0.36725+y*0.0165);
    int y2=2048*sin(akarmi*0.45621+y*0.02753);
    int y3=2048*sin(akarmi*0.15643+y*0.03732);
    for(x=0;x<image_width;x++){
      d[0]=sintable[(y1+x*135)&4095];
      d[1]=sintable[(y2+x*62)&4095];
      d[2]=sintable[(y3+x*23)&4095];
      d+=BYTES_PP;
    }
  }
  
  gen_time+=GetRelativeTime();

#ifdef FAST_BLIT
  // upload texture:
    for(i=0;i<image_height;i++){
      glTexSubImage2D( GL_TEXTURE_2D,  // target
		       0,              // level
		       0,              // x offset
		       i,            // y offset
		       image_width,              // width
		       1,              // height
		       (BYTES_PP==4)?GL_RGBA:GL_RGB,        // format
		       GL_UNSIGNED_BYTE, // type
		       ImageData+i*dstride );        // *pixels
    }
#else
      glTexSubImage2D( GL_TEXTURE_2D,  // target
		       0,              // level
		       0,              // x offset
		       0,            // y offset
		       image_width,              // width
		       image_height,              // height
		       (BYTES_PP==4)?GL_RGBA:GL_RGB,        // format
		       GL_UNSIGNED_BYTE, // type
		       ImageData );        // *pixels
#endif

  up_time+=GetRelativeTime();

  glColor3f(1,1,1);
  glBegin(GL_QUADS);
    glTexCoord2f(0,0);glVertex2i(0,0);
    glTexCoord2f(0,1);glVertex2i(0,texture_height);
    glTexCoord2f(1,1);glVertex2i(texture_width,texture_height);
    glTexCoord2f(1,0);glVertex2i(texture_width,0);
  glEnd();
  
  glFinish();
  glutSwapBuffers();

  render_time+=GetRelativeTime();

  ++counter;
  { float total=gen_time+up_time+render_time;
    if(total>2.0){
    printf("%8.3f fps  (gen: %2d%%  upload: %2d%%  render: %2d%%)\n",
       (float)counter/total,
       (int)(100.0*gen_time/total),
       (int)(100.0*up_time/total),
       (int)(100.0*render_time/total)
    );
    gen_time=up_time=render_time=0;
    counter=0;
  } }

}

static GLvoid IdleFunc(){
  akarmi+=0.1;
  glutPostRedisplay();
}

int
main(int argc, char **argv)
{
  int i;
  
  glutInit(&argc, argv);
  glutInitWindowSize(640, 480);
  glutInitDisplayMode(GLUT_DOUBLE);
  (void) glutCreateWindow("csg");

  glutDisplayFunc(redraw);
  glutReshapeFunc(resize);
  glutIdleFunc(IdleFunc);

  texture_width=32;
  while(texture_width<image_width) texture_width*=2;
  while(texture_width<image_height) texture_width*=2;
  texture_height=texture_width;

    image_bpp=8*BYTES_PP;
    image_bytes=BYTES_PP;

  ImageData=malloc(texture_width*texture_height*image_bytes);
  memset(ImageData,128,texture_width*texture_height*image_bytes);

  glDisable(GL_BLEND); 
  glDisable(GL_DEPTH_TEST);
  glDepthMask(GL_FALSE);
  glDisable(GL_CULL_FACE);

  glEnable(GL_TEXTURE_2D);

  printf("Creating %dx%d texture...\n",texture_width,texture_height);

#if 1
//  glBindTexture(GL_TEXTURE_2D, texture_id);
  glTexParameterf(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER, GL_LINEAR);
  glTexParameterf(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER, GL_LINEAR);
#ifdef TEXTUREFORMAT_32BPP
  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB8, texture_width, texture_height, 0,
#else
  glTexImage2D(GL_TEXTURE_2D, 0, BYTES_PP, texture_width, texture_height, 0,
#endif
       (image_bytes==4)?GL_RGBA:GL_BGR, GL_UNSIGNED_BYTE, ImageData);
#endif

  resize(640,480);

  glClearColor( 1.0f,0.0f,1.0f,0.0f );
  glClear( GL_COLOR_BUFFER_BIT );
  
  for(i=0;i<4096;i++) sintable[i]=128+127*sin(2.0*3.14159265*i/4096.0);

  glutMainLoop();
  return 0;             /* ANSI C requires main to return int. */
}
