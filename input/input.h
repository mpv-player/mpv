
#ifdef HAVE_NEW_INPUT

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

#define MP_CMD_ARG_INT 0
#define MP_CMD_ARG_FLOAT 1
#define MP_CMD_ARG_STRING 2

#define MP_CMD_MAX_ARGS 10

#define MP_INPUT_ERROR -1
#define MP_INPUT_DEAD -2
#define MP_INPUT_NOTHING -3

typedef union mp_cmd_arg_value {
  int i;
  float f;
  char* s;
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
  int input;
  char* cmd;
} mp_cmd_bind_t;

typedef int (*mp_key_func_t)(int fd);
typedef int (*mp_cmd_func_t)(int fd,char* dest,int size);
typedef void (*mp_close_func_t)(int fd);

int
mp_input_add_cmd_fd(int fd, int select, mp_cmd_func_t read_func, mp_close_func_t close_func);

void
mp_input_rm_cmd_fd(int fd);

int
mp_input_add_key_fd(int fd, int select, mp_key_func_t read_func, mp_close_func_t close_func);

void
mp_input_rm_key_fd(int fd);

mp_cmd_t*
mp_input_get_cmd(int time, int paused);

void
mp_cmd_free(mp_cmd_t* cmd);

void
mp_input_init(void);

#endif /* HAVE_NEW_INPUT */
