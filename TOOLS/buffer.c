
// General purpose Ring-buffering routines

#define BUFFSIZE (1024)
#define NUM_BUFS (64)

static unsigned char *buffer[NUM_BUFS];

static unsigned int buf_read=0;
static unsigned int buf_write=0;
static unsigned int buf_read_pos=0;
static unsigned int buf_write_pos=0;

static int full_buffers=0;
static int buffered_bytes=0;

static int write_buffer(unsigned char* data,int len){
  int len2=0;
  int x;
  while(len>0){
    if(full_buffers==NUM_BUFS) break;
    x=BUFFSIZE-buf_write_pos;
    if(x>len) x=len;
    memcpy(buffer[buf_write]+buf_write_pos,data+len2,x);
    len2+=x; len-=x;
    buffered_bytes+=x; buf_write_pos+=x;
    if(buf_write_pos>=BUFFSIZE){
       // block is full, find next!
       buf_write=(buf_write+1)%NUM_BUFS;
       ++full_buffers;
       buf_write_pos=0;
    }
  }
  return len2;
}

static int read_buffer(unsigned char* data,int len){
  int len2=0;
  int x;
  while(len>0){
    if(full_buffers==0) break; // no more data buffered!
    x=BUFFSIZE-buf_read_pos;
    if(x>len) x=len;
    memcpy(data+len2,buffer[buf_read]+buf_read_pos,x);
    len2+=x; len-=x;
    buffered_bytes-=x; buf_read_pos+=x;
    if(buf_read_pos>=BUFFSIZE){
       // block is empty, find next!
       buf_read=(buf_read+1)%NUM_BUFS;
       --full_buffers;
       buf_read_pos=0;
    }
  }
  return len2;
}

static int get_space(){
    return (NUM_BUFS-full_buffers)*BUFFSIZE - buf_write_pos;
}

static int get_delay(){
    return buffered_bytes;
}

int main(int argc,char* argv[]){

	   int i;
	   
	   for(i=0;i<NUM_BUFS;i++) buffer[i]=malloc(BUFFSIZE);

return 0;
}

