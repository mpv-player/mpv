
#ifdef HAVE_NEW_INPUT

// All commands id
#define MP_CMD_SEEK   0
#define MP_CMD_AUDIO_DELAY 1
#define MP_CMD_QUIT 2
#define MP_CMD_PAUSE 3
#define MP_CMD_GRAB_FRAMES 4
#define MP_CMD_PLAY_TREE_STEP 5
#define MP_CMD_PLAY_TREE_UP_STEP 6
#define MP_CMD_PLAY_ALT_SRC_STEP 7
#define MP_CMD_SUB_DELAY 8
#define MP_CMD_OSD 9
#define MP_CMD_VOLUME 10
#define MP_CMD_MIXER_USEMASTER 11
#define MP_CMD_CONTRAST 12
#define MP_CMD_BRIGHTNESS 13
#define MP_CMD_HUE 14
#define MP_CMD_SATURATION 15
#define MP_CMD_FRAMEDROPPING 16
#define MP_CMD_TV_STEP_CHANNEL 17
#define MP_CMD_TV_STEP_NORM 18
#define MP_CMD_TV_STEP_CHANNEL_LIST 19
#define MP_CMD_VO_FULLSCREEN 20
#define MP_CMD_SUB_POS 21
#define MP_CMD_DVDNAV 22
#define MP_CMD_SCREENSHOT 23
#define MP_CMD_PANSCAN 24

#define MP_CMD_GUI_EVENTS       5000
#define MP_CMD_GUI_LOADFILE     5001
#define MP_CMD_GUI_LOADSUBTITLE 5002
#define MP_CMD_GUI_ABOUT        5003
#define MP_CMD_GUI_PLAY         5004
#define MP_CMD_GUI_STOP         5005
#define MP_CMD_GUI_PLAYLIST     5006
#define MP_CMD_GUI_PREFERENCES  5007
#define MP_CMD_GUI_FULLSCREEN   5008
#define MP_CMD_GUI_SKINBROWSER  5009

#define MP_CMD_DVDNAV_EVENT     6000

#define MP_CMD_DVDNAV_UP        1
#define MP_CMD_DVDNAV_DOWN      2
#define MP_CMD_DVDNAV_LEFT      3
#define MP_CMD_DVDNAV_RIGHT     4
#define MP_CMD_DVDNAV_MENU      5
#define MP_CMD_DVDNAV_SELECT    6

// The args types
#define MP_CMD_ARG_INT 0
#define MP_CMD_ARG_FLOAT 1
#define MP_CMD_ARG_STRING 2
#define MP_CMD_ARG_VOID 3

#ifndef MP_CMD_MAX_ARGS
#define MP_CMD_MAX_ARGS 10
#endif

// Error codes for the drivers

// An error occured but we can continue
#define MP_INPUT_ERROR -1
// A fatal error occured, this driver should be removed
#define MP_INPUT_DEAD -2
// No input were avaible
#define MP_INPUT_NOTHING -3

// For the keys drivers, if possible you can send key up and key down
// events. Key up is the default, to send a key down you must or the key
// code with MP_KEY_DOWN
#define MP_KEY_DOWN (1<<29)
// Use this when the key shouldn't be auto-repeated (like mouse buttons)
#define MP_NO_REPEAT_KEY (1<<28)

#ifndef MP_MAX_KEY_DOWN
#define MP_MAX_KEY_DOWN 32
#endif

typedef union mp_cmd_arg_value {
  int i;
  float f;
  char* s;
  void* v;
} mp_cmd_arg_value_t;

typedef struct mp_cmd_arg {
  int type;
  mp_cmd_arg_value_t v;
} mp_cmd_arg_t;

typedef struct mp_cmd {
  int id;
  char* name;
  int nargs;
  mp_cmd_arg_t args[MP_CMD_MAX_ARGS];
} mp_cmd_t;


typedef struct mp_cmd_bind {
  int input[MP_MAX_KEY_DOWN+1];
  char* cmd;
} mp_cmd_bind_t;

typedef struct mp_key_name {
  int key;
  char* name;
} mp_key_name_t;

// These typedefs are for the drivers. They are the functions used to retrive
// the next key code or command.

// These functions should return the key code or one of the error code
typedef int (*mp_key_func_t)(int fd);
// These functions should act like read
typedef int (*mp_cmd_func_t)(int fd,char* dest,int size);
// These are used to close the driver
typedef void (*mp_close_func_t)(int fd);

// This function add a new key driver.
// The first arg is a file descriptor (use a negative value if you don't use any fd)
// The second arg tell if we use select on the fd to know if something is avaible.
// The third arg is optional. If null a default function wich read an int from the
// fd will be used.
// The last arg can be NULL if nothing is needed to close the driver. The close
// function can be used
int
mp_input_add_cmd_fd(int fd, int select, mp_cmd_func_t read_func, mp_close_func_t close_func);

// This remove a cmd driver, you usally don't need to use it
void
mp_input_rm_cmd_fd(int fd);

// The args are the sames as for the keys drivers. If you don't use any valid fd you MUST
// give a read_func.
int
mp_input_add_key_fd(int fd, int select, mp_key_func_t read_func, mp_close_func_t close_func);

// As for the cmd one you usally don't need this function
void
mp_input_rm_key_fd(int fd);

// This function can be used to reput a command in the system. It's used by libmpdemux
// when it perform a blocking operation to resend the command it received to the main
// loop.
int
mp_input_queue_cmd(mp_cmd_t* cmd);

// This function retrive the next avaible command waiting no more than time msec.
// If pause is true, the next input will always return a pause command.
mp_cmd_t*
mp_input_get_cmd(int time, int paused);

// After getting a command from mp_input_get_cmd you need to free it using this
// function
void
mp_cmd_free(mp_cmd_t* cmd);

// This create a copy of a command (used by the auto repeat stuff)
mp_cmd_t*
mp_cmd_clone(mp_cmd_t* cmd);

// When you create a new driver you should add it in this 2 functions.
void
mp_input_init(void);

void
mp_input_uninit(void);

#endif /* HAVE_NEW_INPUT */
