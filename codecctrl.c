
//#define DEBUG_SIGNALS
#define DEBUG_SIGNALS_SLEEP ;
//#define DEBUG_SIGNALS_SLEEP sleep(2);

#ifdef DEBUG_SIGNALS
#define DEBUG_SIG if(1)
#else
#define DEBUG_SIG if(0)
#endif

//======= Interprocess Comminication (IPC) between player & codec =========

static int child_pid=0;
static int codec_pid=0;

// player:
static int data_fifo=-1;
static int control_fifo=-1;
// codec:
static int data_fifo2=-1;
static int control_fifo2=-1;


// SIGTERM handler of codec controller (2nd process):
static void codec_ctrl_sighandler(int x){
DEBUG_SIG  printf("\nCTRL: received signal %d, terminating child first:\n",x);
  // first terminate the codec:
  //kill(child_pid,SIGTERM);
  kill(child_pid,x);
  usleep(50000); // 50ms must be enough
DEBUG_SIG  printf("CTRL: Sending KILL signal to child:\n");
  kill(child_pid,SIGKILL); // worst case
  usleep(10000);
  // and exit
  if(x!=SIGHUP){
DEBUG_SIG    printf("CTRL: Exiting...\n");
    exit(0);
  }
}

static vo_functions_t *codec_video_out_ptr=NULL;

// SIGTERM handler of the codec (3nd process):
static void codec_sighandler(int x){
DEBUG_SIG  printf("\nCHILD: received signal %d, exiting...\n",x);
  if(x==SIGTERM){
    //mpeg2_close(codec_video_out_ptr);
    codec_video_out_ptr->uninit();  // closing video_out
  }
  exit(0);
}

void mpeg_codec_controller(vo_functions_t *video_out){
//================== CODEC Controller: ==========================
    signal(SIGTERM,codec_ctrl_sighandler); // set our SIGTERM handler
    signal(SIGHUP,codec_ctrl_sighandler);  // set our SIGHUP handler
    printf("starting video codec...\n");
    while(1){
      int status;
      if((child_pid=fork())==0){
        // child:
        unsigned int t=0;
        codec_video_out_ptr=video_out;
#if 0
        signal(SIGTERM,codec_sighandler); // set our SIGTERM handler
        signal(SIGHUP,codec_sighandler);  // set our SIGHUP handler
#else
  // terminate requests:
  signal(SIGTERM,codec_sighandler); // kill
  signal(SIGHUP,codec_sighandler);  // kill -HUP  /  xterm closed
  signal(SIGINT,codec_sighandler);  // Interrupt from keyboard
  signal(SIGQUIT,codec_sighandler); // Quit from keyboard
  // fatal errors:
  signal(SIGBUS,codec_sighandler);  // bus error
  signal(SIGSEGV,codec_sighandler); // segfault
  signal(SIGILL,codec_sighandler);  // illegal instruction
  signal(SIGFPE,codec_sighandler);  // floating point exc.
  signal(SIGABRT,codec_sighandler); // abort()
#endif

        send_cmd(control_fifo2,0x22222222); // Send WE_ARE_READY command
        send_cmd(control_fifo2,getpid());   // Send out PID
        while(1){
          unsigned int syncword=0;
          read(data_fifo2,&syncword,4);
          if(syncword==0x22222222) break;
          printf("codec: drop bad frame (%X)\n",syncword);
        }
        //printf("codec: connection synced\n");
        
        while(1){
          int len=0;
          int len2;
          send_cmd(control_fifo2,0x3030303);
          len2=my_read(data_fifo2,(unsigned char*) &len,4);
          if(len2!=4){
            printf("FATAL: cannot read packet len from data fifo (ret=%d, errno=%d)\n",len2,errno);
            break;
          }
          if(len==0){ printf("mpeg2dec: EOF, exiting...\n");break; }
//          printf("mpeg2dec: frame (%d bytes) read\n",len);
          t-=GetTimer();
          mpeg2_decode_data(video_out, videobuffer, videobuffer+len);
          t+=GetTimer();
          send_cmd(control_fifo2,0); // FRAME_COMPLETED command
          send_cmd(control_fifo2,frameratecode2framerate[picture->frame_rate_code]); // fps
          send_cmd(control_fifo2,100+picture->repeat_count);picture->repeat_count=0;
//          send_cmd(control_fifo2,100); // FIXME!
          send_cmd(control_fifo2,t);t=0;
        }
		video_out->uninit();
        exit(0); // leave process
      }
      wait(&status); // Waiting for the child!
//      printf("restarting video codec...\n");
    }
    exit(0);
}

