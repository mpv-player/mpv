#include "../config.h"

#ifdef HAVE_NEW_INPUT

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <errno.h>
#include <signal.h>
#include <sys/types.h>
#include <fcntl.h>



#include "input.h"
#ifdef MP_DEBUG
#include <assert.h>
#endif
#include "../linux/getch2.h"
#include "../linux/keycodes.h"
#include "../linux/timer.h"

#ifdef HAVE_JOYSTICK
#include "joystick.h"
#endif

#ifdef HAVE_LIRC
#include "lirc.h"
#endif

// If the args field is not NULL, the command will only be passed if
// an argument exist.

static mp_cmd_t mp_cmds[] = {
  { MP_CMD_SEEK, "seek", 1, { {MP_CMD_ARG_INT,{0}}, {MP_CMD_ARG_INT,{0}}, {-1,{0}} } },
  { MP_CMD_AUDIO_DELAY, "audio_delay", 1, { {MP_CMD_ARG_FLOAT,{0}}, {-1,{0}} } },
  { MP_CMD_QUIT, "quit", 0, { {-1,{0}} } },
  { MP_CMD_PAUSE, "pause", 0, { {-1,{0}} } },
  { MP_CMD_GRAB_FRAMES, "grap_frames",0, { {-1,{0}} }  },
  { MP_CMD_PLAY_TREE_STEP, "pt_step",1, { { MP_CMD_ARG_INT ,{0}}, {-1,{0}} } },
  { MP_CMD_PLAY_TREE_UP_STEP, "pt_up_step",1,  { { MP_CMD_ARG_INT,{0} }, {-1,{0}} } },
  { MP_CMD_PLAY_ALT_SRC_STEP, "alt_src_step",1, { { MP_CMD_ARG_INT,{0} }, {-1,{0}} } },
  { MP_CMD_SUB_DELAY, "sub_delay",1,  { {MP_CMD_ARG_FLOAT,{0}}, {MP_CMD_ARG_INT,{0}}, {-1,{0}} } },
  { MP_CMD_OSD, "osd",0, { {MP_CMD_ARG_INT,{-1}}, {-1,{0}} } },
  { MP_CMD_VOLUME, "volume", 1, { { MP_CMD_ARG_INT,{0} }, {-1,{0}} } },
  { MP_CMD_MIXER_USEMASTER, "use_master", 0, { {-1,{0}} } },
  { MP_CMD_CONTRAST, "contrast",1,  { {MP_CMD_ARG_INT,{0}}, {MP_CMD_ARG_INT,{0}}, {-1,{0}} } },
  { MP_CMD_BRIGHTNESS, "brightness",1,  { {MP_CMD_ARG_INT,{0}}, {MP_CMD_ARG_INT,{0}}, {-1,{0}} }  },
  { MP_CMD_HUE, "hue",1,  { {MP_CMD_ARG_INT,{0}}, {MP_CMD_ARG_INT,{0}}, {-1,{0}} } },
  { MP_CMD_SATURATION, "saturation",1,  { {MP_CMD_ARG_INT,{0}}, {MP_CMD_ARG_INT,{0}}, {-1,{0}} }  },
  { MP_CMD_FRAMEDROPPING, "frame_drop",0, { { MP_CMD_ARG_INT,{-1} }, {-1,{0}} } },
#ifdef USE_TV
  { MP_CMD_TV_STEP_CHANNEL, "tv_step_channel", 1,  { { MP_CMD_ARG_INT ,{0}}, {-1,{0}} }},
  { MP_CMD_TV_STEP_NORM, "tv_step_norm",0, { {-1,{0}} }  },
  { MP_CMD_TV_STEP_CHANNEL_LIST, "tv_step_chanlist", 0, { {-1,{0}} }  },
#endif
  { 0, NULL, 0, {} }
};

static mp_cmd_bind_t key_names[] = {
  { ' ', "SPACE" },
  { KEY_ENTER, "ENTER" },
  { KEY_TAB, "TAB" },
  { KEY_CTRL, "CTRL" },
  { KEY_BACKSPACE, "BS" },
  { KEY_DELETE, "DEL" },
  { KEY_INSERT, "INS" },
  { KEY_HOME, "HOME" },
  { KEY_END, "END" },
  { KEY_PAGE_UP, "PGUP" },
  { KEY_PAGE_DOWN, "PGDWN" },
  { KEY_ESC, "ESC" },
  { KEY_RIGHT, "RIGHT" },
  { KEY_LEFT, "LEFT" },
  { KEY_DOWN, "DOWN" },
  { KEY_UP, "UP" },
#ifdef HAVE_JOYSTICK
  { JOY_AXIS1_PLUS, "JOY_UP" },
  { JOY_AXIS1_MINUS, "JOY_DOWN" },
  { JOY_AXIS0_PLUS, "JOY_LEFT" },
  { JOY_AXIS0_MINUS, "JOY_RIGHT" },

  { JOY_AXIS0_PLUS, "JOY_AXIS0_PLUS" },
  { JOY_AXIS0_MINUS, "JOY_AXIS0_MINUS" },
  { JOY_AXIS1_PLUS, " JOY_AXIS1_PLUS" },
  { JOY_AXIS1_MINUS, "JOY_AXIS1_MINUS" },
  { JOY_AXIS2_PLUS, "JOY_AXIS2_PLUS" },
  { JOY_AXIS2_MINUS, "JOY_AXIS2_MINUS" },
  { JOY_AXIS3_PLUS, " JOY_AXIS3_PLUS" },
  { JOY_AXIS3_MINUS, "JOY_AXIS3_MINUS" },
  { JOY_AXIS4_PLUS, "JOY_AXIS4_PLUS" },
  { JOY_AXIS4_MINUS, "JOY_AXIS4_MINUS" },
  { JOY_AXIS5_PLUS, " JOY_AXIS5_PLUS" },
  { JOY_AXIS5_MINUS, "JOY_AXIS5_MINUS" },
  { JOY_AXIS6_PLUS, "JOY_AXIS6_PLUS" },
  { JOY_AXIS6_MINUS, "JOY_AXIS6_MINUS" },
  { JOY_AXIS7_PLUS, " JOY_AXIS7_PLUS" },
  { JOY_AXIS7_MINUS, "JOY_AXIS7_MINUS" },
  { JOY_AXIS8_PLUS, "JOY_AXIS8_PLUS" },
  { JOY_AXIS8_MINUS, "JOY_AXIS8_MINUS" },
  { JOY_AXIS9_PLUS, " JOY_AXIS9_PLUS" },
  { JOY_AXIS9_MINUS, "JOY_AXIS9_MINUS" },

  { JOY_BTN0, "JOY_BTN0" },
  { JOY_BTN1, "JOY_BTN1" },
  { JOY_BTN2, "JOY_BTN2" },
  { JOY_BTN3, "JOY_BTN3" },
  { JOY_BTN4, "JOY_BTN4" },
  { JOY_BTN5, "JOY_BTN5" },
  { JOY_BTN6, "JOY_BTN6" },
  { JOY_BTN7, "JOY_BTN7" },
  { JOY_BTN8, "JOY_BTN8" },
  { JOY_BTN9, "JOY_BTN9" },
#endif
  { 0, NULL }
};

// This is the default binding we use when no config file is here

static mp_cmd_bind_t def_cmd_binds[] = {
  { KEY_RIGHT, "seek 10" },
  { KEY_LEFT, "seek -10" },
  { KEY_UP, "seek 60" },
  { KEY_DOWN, "seek -60" },
  { KEY_PAGE_UP, "seek 600" },
  { KEY_PAGE_DOWN, "seek -600" },
  { '+', "audio_delay 0.100" },
  { '-', "audio_delay -0.100" },
  { 'q', "quit" },
  { KEY_ESC, "quit" },
  { 'p', "pause" },
  { ' ', "pause" },
  { KEY_HOME, "pt_up_step 1" },
  { KEY_END, "pt_up_step -1" },
  { '>', "pt_step 1" },
  { '<', "pt_step -1" },
  { KEY_INS, "alt_src_step 1" },
  { KEY_DEL, "alt_src_step -1" },
  { 'o', "osd" },
  { 'z', "sub_delay -0.1" },
  { 'x', "sub_delay +0.1" },
  { '9', "volume -1" },
  { '/', "volume -1" },
  { '0', "volume 1" },
  { '*', "volume 1" },
  { 'm', "use_master" },
  { '1', "contrast -1" },
  { '2', "contrast 1" },
  { '3', "brightness -1" },
  { '4', "brightness 1" },
  { '5', "hue -1" },
  { '6', "hue 1" },
  { '7', "saturation -1" },
  { '8', "saturation 1" },
  { 'd', "frame_drop" },
#ifdef USE_TV
  { 'h', "tv_step_channel 1" },
  { 'l', "tv_step_channel -1" },
  { 'n', "tv_step_norm" },
  { 'b', "tv_step_chanlist" },
#endif
  { 0, NULL }
};

#ifndef MP_MAX_KEY_FD
#define MP_MAX_KEY_FD 10
#endif

#ifndef MP_MAX_CMD_FD
#define MP_MAX_CMD_FD 10
#endif

#define MP_FD_EOF (1<<0)
#define MP_FD_DROP (1<<1)
#define MP_FD_DEAD (1<<2)
#define MP_FD_GOT_CMD (1<<3)
#define MP_FD_NO_SELECT (1<<4)

typedef struct mp_input_fd {
  int fd;
  void* read_func;
  mp_close_func_t close_func;
  int flags;
  // This fields are for the cmd fds
  char* buffer;
  int pos,size;
} mp_input_fd_t;


static mp_cmd_bind_t* cmd_binds = def_cmd_binds;

static mp_input_fd_t key_fds[MP_MAX_KEY_FD];
static unsigned int num_key_fd = 0;
static mp_input_fd_t cmd_fds[MP_MAX_CMD_FD];
static unsigned int num_cmd_fd = 0;

static int key_max_fd = -1, cmd_max_fd = -1;

static int
mp_input_default_key_func(int fd);


int
mp_input_add_cmd_fd(int fd, int select, mp_cmd_func_t read_func, mp_close_func_t close_func) {
  if(num_cmd_fd == MP_MAX_CMD_FD) {
    printf("Too much command fd, unable to register fd %d\n",fd);
    return 0;
  }

  memset(&cmd_fds[num_cmd_fd],0,sizeof(mp_input_fd_t));
  cmd_fds[num_cmd_fd].fd = fd;
  cmd_fds[num_cmd_fd].read_func = read_func ? read_func : (mp_cmd_func_t)read;
  cmd_fds[num_cmd_fd].close_func = close_func;
  if(!select)
    cmd_fds[num_cmd_fd].flags = MP_FD_NO_SELECT;
  num_cmd_fd++;
  if(fd > cmd_max_fd)
    cmd_max_fd = fd;

  return 1;
}

void
mp_input_rm_cmd_fd(int fd) {
  unsigned int i;

  for(i = 0; i < num_cmd_fd; i++) {
    if(cmd_fds[i].fd == fd)
      break;
  }
  if(i == num_cmd_fd)
    return;
  if(cmd_fds[i].close_func)
    cmd_fds[i].close_func(cmd_fds[i].fd);

  if(i + 1 < num_cmd_fd)
    memmove(&cmd_fds[i],&cmd_fds[i+1],(num_cmd_fd - i - 1)*sizeof(mp_input_fd_t));
  num_cmd_fd--;
}

void
mp_input_rm_key_fd(int fd) {
  unsigned int i;

  for(i = 0; i < num_key_fd; i++) {
    if(key_fds[i].fd == fd)
      break;
  }
  if(i == num_key_fd)
    return;
  if(key_fds[i].close_func)
    key_fds[i].close_func(key_fds[i].fd);

  if(i + 1 < num_key_fd)
    memmove(&key_fds[i],&key_fds[i+1],(num_key_fd - i - 1)*sizeof(mp_input_fd_t));
  num_key_fd--;
}

int
mp_input_add_key_fd(int fd, int select, mp_key_func_t read_func, mp_close_func_t close_func) {
  if(num_key_fd == MP_MAX_KEY_FD) {
    printf("Too much key fd, unable to register fd %d\n",fd);
    return 0;
  }

  memset(&key_fds[num_key_fd],0,sizeof(mp_input_fd_t));
  key_fds[num_key_fd].fd = fd;
  key_fds[num_key_fd].read_func = read_func ? read_func : mp_input_default_key_func;
  key_fds[num_key_fd].close_func = close_func;
  if(!select)
    key_fds[num_key_fd].flags |= MP_FD_NO_SELECT;
  num_key_fd++;
  if(fd > key_max_fd)
    key_max_fd = fd;

  return 1;
}



static mp_cmd_t*
mp_input_parse_cmd(char* str) {
  int i,l;
  char *ptr,*e;
  mp_cmd_t *cmd, *cmd_def;

#ifdef MP_DEBUG
  assert(str != NULL);
#endif

  ptr = strchr(str,' ');
  if(ptr)
    l = ptr-str;
  else
    l = strlen(str);

  if(l == 0)
    return NULL;

  for(i=0; mp_cmds[i].name != NULL; i++) {
    if(strncasecmp(mp_cmds[i].name,str,l) == 0)
      break;
  }

  if(mp_cmds[i].name == NULL)
    return NULL;

  cmd_def = &mp_cmds[i];

  cmd = (mp_cmd_t*)malloc(sizeof(mp_cmd_t));
  cmd->id = cmd_def->id;
  cmd->name = strdup(cmd_def->name);

  ptr = str;

  for(i=0; ptr && i < MP_CMD_MAX_ARGS; i++) {
    ptr = strchr(ptr,' ');
    if(!ptr) break;
    while(ptr[0] == ' ') ptr++;
    if(ptr[0] == '\0') break;	
    switch(cmd_def->args[i].type) {
    case MP_CMD_ARG_INT:
      errno = 0;
      cmd->args[i].v.i = atoi(ptr);
      if(errno != 0) {
	printf("Command %s : argument %d isn't an integer\n",cmd_def->name,i+1);
	ptr = NULL;
      }
      break;
    case MP_CMD_ARG_FLOAT:
      errno = 0;
      cmd->args[i].v.f = atof(ptr);
      if(errno != 0) {
	printf("Command %s : argument %d isn't a float\n",cmd_def->name,i+1);
	ptr = NULL;
      }
      break;
    case MP_CMD_ARG_STRING:
      e = strchr(ptr,' ');
      if(!e) e = ptr+strlen(ptr);
      l = e-ptr;
      cmd->args[i].v.s = (char*)malloc((l+1)*sizeof(char));
      strncpy(cmd->args[i].v.s,ptr,l);
      cmd->args[i].v.s[l] = '\0';
      break;
    case -1:
      ptr = NULL;
    default :
      printf("Unknow argument %d\n",i);
    }
  }
  cmd->nargs = i;

  if(cmd_def->nargs > cmd->nargs) {
    printf("Got [%s] but\n",str);
    printf("Command %s require at least %d arguments, we found only %d so far\n",cmd_def->name,cmd_def->nargs,cmd->nargs);
    mp_cmd_free(cmd);
    return NULL;
  }

  for( ; i < MP_CMD_MAX_ARGS && cmd_def->args[i].type != -1 ; i++)
    memcpy(&cmd->args[i].v,&cmd_def->args[i].v,sizeof(mp_cmd_arg_value_t));

  return cmd;
}

static int
mp_input_default_key_func(int fd) {
  int r,code=0;
  unsigned int l;
  l = 0;
  while(l < sizeof(int)) {
    r = read(fd,(&code)+l,sizeof(int)-l);
    if(r <= 0)
      break;
    l +=r;
  }
  return code;
}

#define MP_CMD_MAX_SIZE 256

static int
mp_input_read_cmd(mp_input_fd_t* mp_fd, char** ret) {
  char* end;
  (*ret) = NULL;

  if(!mp_fd->buffer) {
    mp_fd->buffer = (char*)malloc(MP_CMD_MAX_SIZE*sizeof(char));
    mp_fd->pos = 0;
    mp_fd->size = MP_CMD_MAX_SIZE;
  } 

  if(mp_fd->size - mp_fd->pos == 0) {
    printf("Cmd buffer of fd %d is full : dropping content\n",mp_fd->fd);
    mp_fd->pos = 0;
    mp_fd->flags |= MP_FD_DROP;
  }      

  while( !(mp_fd->flags & MP_FD_EOF) && (mp_fd->size - mp_fd->pos > 1) ) {
    int r = ((mp_cmd_func_t)mp_fd->read_func)(mp_fd->fd,mp_fd->buffer+mp_fd->pos,mp_fd->size - 1 - mp_fd->pos);
    if(r < 0) {
      if(errno == EINTR)
	continue;
      else if(errno == EAGAIN)
	break;
      printf("Error while reading cmd fd %d : %s\n",mp_fd->fd,strerror(errno));
      return MP_INPUT_ERROR;
    } else if(r == 0) {
      mp_fd->flags |= MP_FD_EOF;
      break;
    }
    mp_fd->pos += r;
    break;
  }


  while(1) {
    int l = 0;
    mp_fd->buffer[mp_fd->pos] = '\0';
    end = strchr(mp_fd->buffer,'\n');
    if(!end)
      break;
    else if((*ret)) {
      mp_fd->flags |= MP_FD_GOT_CMD;
      break;
    }

    l = end - mp_fd->buffer;

    if( ! (mp_fd->flags & MP_FD_DROP)) {
      (*ret) = (char*)malloc((l+1)*sizeof(char));
      strncpy((*ret),mp_fd->buffer,l);
      (*ret)[l] = '\0';
    } else {
      mp_fd->flags &= ~MP_FD_DROP;
    }
    if( mp_fd->pos - (l+1) > 0)
      memmove(mp_fd->buffer,end,mp_fd->pos-(l+1));
    mp_fd->pos -= l+1;
  }
   
  if(*ret)
    return 1;
  else
    return MP_INPUT_NOTHING;
}

static mp_cmd_t*
mp_input_read_keys(int time,int paused) {
  fd_set fds;
  struct timeval tv;
  int i,n=0;
  static int last_loop = 0;

  if(num_key_fd == 0)
    return NULL;

  FD_ZERO(&fds);
  for(i = 0; (unsigned int)i < num_key_fd; i++) {
    if( (key_fds[i].flags & MP_FD_DEAD) ) {
      mp_input_rm_key_fd(key_fds[i].fd);
      i--;
      continue;
    } else if(key_fds[i].flags & MP_FD_NO_SELECT)
      continue;

    FD_SET(key_fds[i].fd,&fds);
    n++;
  }

  if(n > 0 ) {

    tv.tv_sec=time/1000; 
    tv.tv_usec = (time%1000)*1000;
  
    while(1) {
      if(select(key_max_fd+1,&fds,NULL,NULL,&tv) < 0) {
	if(errno == EINTR)
	  continue;
	printf("Select error : %s\n",strerror(errno));
      }
      break;
    }
  } else {
    FD_ZERO(&fds);
  }
    
  for(i = last_loop + 1 ; i != last_loop ; i++) {
    int code = -1,j;

    if((unsigned int)i >= num_key_fd) {
      i = -1;
      last_loop++;
      continue;
    }
    if(! (key_fds[i].flags & MP_FD_NO_SELECT) && ! FD_ISSET(key_fds[i].fd,&fds))
      continue;
    if(key_fds[i].fd == 0) { // stdin is handled by getch2
      code = getch2(time);
      if(code < 0)
	code = MP_INPUT_NOTHING;
    }
    else
      code = ((mp_key_func_t)key_fds[i].read_func)(key_fds[i].fd);

    if(code < 0) {
      if(code == MP_INPUT_ERROR)
	printf("Error on key input fd %d\n",key_fds[i].fd);
      else if(code == MP_INPUT_DEAD) {
	printf("Dead key input on fd %d\n",key_fds[i].fd);
	key_fds[i].flags |= MP_FD_DEAD;
      }
      continue;
    }
    if(paused)
      return mp_input_parse_cmd("pause");
    for(j = 0; cmd_binds[j].cmd != NULL; j++) {
      if(cmd_binds[j].input == code)
	break;
    }
    if(cmd_binds[j].cmd == NULL) {
      printf("No bind found for key %d\n",code);
      continue;
    }
    last_loop = i;
    return mp_input_parse_cmd(cmd_binds[j].cmd);
  }

  last_loop = 0;
  return NULL;
}

static mp_cmd_t*
mp_input_read_cmds(int time) {
  fd_set fds;
  struct timeval tv;
  int i,n = 0;
  mp_cmd_t* ret;
  static int last_loop = 0;

  if(num_cmd_fd == 0)
    return NULL;

  FD_ZERO(&fds);
  for(i = 0; (unsigned int)i < num_cmd_fd ; i++) {
    if( (cmd_fds[i].flags & MP_FD_DEAD) || (cmd_fds[i].flags & MP_FD_EOF) ) {
      mp_input_rm_cmd_fd(cmd_fds[i].fd);
      i--;
      continue;
    } else if(cmd_fds[i].flags & MP_FD_NO_SELECT)
      continue;    
    FD_SET(cmd_fds[i].fd,&fds);
    n++;
  }

  if(n > 0) {

    tv.tv_sec=time/1000; 
    tv.tv_usec = (time%1000)*1000;
    
    while(1) {
      if((i = select(cmd_max_fd+1,&fds,NULL,NULL,&tv)) <= 0) {
	if(i < 0) {
	  if(errno == EINTR)
	    continue;
	  printf("Select error : %s\n",strerror(errno));
	}
	return NULL;
      }
      break;
    }
  } else {
    FD_ZERO(&fds);
  }    

  for(i = last_loop + 1; i !=  last_loop ; i++) {
    int r = 0;
    char* cmd;
    if((unsigned int)i >= num_cmd_fd) {
      i = -1;
      last_loop++;
      continue;
    }
    if( ! (cmd_fds[i].flags & MP_FD_NO_SELECT) && ! FD_ISSET(cmd_fds[i].fd,&fds) && ! (cmd_fds[i].flags & MP_FD_GOT_CMD) )
      continue;

    r = mp_input_read_cmd(&cmd_fds[i],&cmd);
    if(r < 0) {
      if(r == MP_INPUT_ERROR)
	printf("Error on cmd fd %d\n",cmd_fds[i].fd);
      else if(r == MP_INPUT_DEAD)
	cmd_fds[i].flags |= MP_FD_DEAD;
      continue;
    }
    ret = mp_input_parse_cmd(cmd);
    free(cmd);
    if(!ret)
      continue;
    last_loop = i;
    return ret;
  }
  
  last_loop = 0;
  return NULL;  
}

mp_cmd_t*
mp_input_get_cmd(int time, int paused) {
  mp_cmd_t* ret;

  ret = mp_input_read_keys(time,paused);
  if(ret)
    return ret;

  return mp_input_read_cmds(time);
}

void
mp_cmd_free(mp_cmd_t* cmd) {
  int i;
#ifdef MP_DEBUG
  assert(cmd != NULL);
#endif

  if(cmd->name)
    free(cmd->name);
  
  for(i=0; i < MP_CMD_MAX_ARGS && cmd->args[i].type != -1; i++) {
    if(cmd->args[i].type == MP_CMD_ARG_STRING)
      free(cmd->args[i].v.s);
  }
  free(cmd);
}

static int
mp_input_get_key_from_name(char* name) {
  int i,ret = 0;

  if(strlen(name) == 1) { // Direct key code
    (char)ret = name[0];
    return ret;
  }

  for(i = 0; key_names[i].cmd != NULL; i++) {
    if(strcasecmp(key_names[i].cmd,name) == 0)
      return key_names[i].input;
  }

  return -1;
}

static void
mp_input_free_binds(mp_cmd_bind_t* binds) {
  int i;

  if(!binds)
    return;

  for(i = 0; binds[i].cmd != NULL; i++)
    free(binds[i].cmd);

  free(binds);

}
  

#define BS_MAX 256
#define SPACE_CHAR " \n\r\t"

static void
mp_input_parse_config(char *file) {
  int fd,code=-1;
  int bs = 0,r,eof = 0;
  char *iter,*end;
  char buffer[BS_MAX];
  int n_binds = 0;
  mp_cmd_bind_t* binds = NULL;

  fd = open(file,O_RDONLY);

  if(fd < 0) {
    printf("Can't open input config file %s : %s\n",file,strerror(errno));
    return;
  }

  printf("Parsing input config file %s\n",file);

  while(1) {
    if(! eof && bs < BS_MAX-1) {
      if(bs > 0) bs--;
      r = read(fd,buffer+bs,BS_MAX-1-bs);
      if(r < 0) {
	if(errno == EINTR)
	  continue;
	printf("Error while reading input config file %s : %s\n",file,strerror(errno));
	mp_input_free_binds(binds);
	return;
      } else if(r == 0) 
	eof = 1;
      else {
	bs += r+1;
	buffer[bs-1] = '\0';
      }
    }
    // Empty buffer : return
    if(bs <= 1) {
      printf("Input config file %s parsed : %d binds\n",file,n_binds);
      if(binds)
	cmd_binds = binds;
      return;
    }
      
    iter = buffer;

    // Find the wanted key
    if(code < 0) {
      // Jump beginnig space
      for(  ; iter[0] != '\0' && strchr(SPACE_CHAR,iter[0]) != NULL ; iter++)
	/* NOTHING */;
      if(iter[0] == '\0') { // Buffer was full of space char
	bs = 0;
	continue;
      }
      // Find the end of the key code name
      for(end = iter; end[0] != '\0' && strchr(SPACE_CHAR,end[0]) == NULL ; end++)
	/*NOTHING */;
      if(end[0] == '\0') { // Key name don't fit in the buffer
	if(buffer == iter) {
	  if(eof && (buffer-iter) == bs)
	    printf("Unfinished binding %s\n",iter);
	  else
	    printf("Buffer is too small for this key name : %s\n",iter);
	  mp_input_free_binds(binds);
	  return;
	}
	memmove(buffer,iter,end-iter);
	bs = end-iter;
	continue;
      }
      {
	char name[end-iter+1];
	strncpy(name,iter,end-iter);
	name[end-iter] = '\0';
	code = mp_input_get_key_from_name(name);
	if(code < 0) {
	  printf("Unknow key %s\n",name);
	  mp_input_free_binds(binds);
	  return;
	}
      }
      if( bs > (end-buffer))
	memmove(buffer,end,bs - (end-buffer));
      bs -= end-buffer;
      continue;
    } else { // Get the command
      while(iter[0] == ' ' || iter[0] == '\t') iter++;
      // Found new line
      if(iter[0] == '\n' || iter[0] == '\r') {
	printf("No command found for key (TODO)\n" /*mp_input_get_key_name(code)*/);
	code = -1;
	if(iter > buffer) {
	  memmove(buffer,iter,bs- (iter-buffer));
	  bs -= (iter-buffer);
	}
	continue;
      }
      for(end = iter ; end[0] != '\n' && end[0] != '\r' && end[0] != '\0' ; end++)
	/* NOTHING */;
      if(end[0] == '\0' && ! (eof && (end - buffer) == bs)) {
	if(iter == buffer) {
	  printf("Buffer is too small for command %s\n",buffer);
	  mp_input_free_binds(binds);
	  return;
	}
	memmove(buffer,iter,end - iter);
	bs = end - iter;
	continue;
      }
      {
	char cmd[end-iter+1];
	strncpy(cmd,iter,end-iter);
	cmd[end-iter] = '\0';
	//printf("Set bind %d => %s\n",code,cmd);
	binds = (mp_cmd_bind_t*)realloc(binds,(n_binds+2)*sizeof(mp_cmd_bind_t));
	binds[n_binds].input = code;
	binds[n_binds].cmd = strdup(cmd);
	n_binds++;
	memset(&binds[n_binds],0,sizeof(mp_cmd_bind_t));
      }
      code = -1;
      if(bs > (end-buffer))
	memmove(buffer,end,bs-(end-buffer));
      bs -= (end-buffer);
      continue;
    }
  }
  printf("What are we doing here ?\n");
}

extern char *get_path(char *filename);

void
mp_input_init(void) {
  char* file;

  file = get_path("input.conf");
  if(!file)
    return;
  
  mp_input_parse_config(file);

#ifdef HAVE_JOYSTICK
  {
    int fd = mp_input_joystick_init(NULL);
    if(fd < 0)
      printf("Can't init input joystick\n");
    else
      mp_input_add_key_fd(fd,1,mp_input_joystick_read,mp_input_joystick_close);
  }
#endif

#ifdef HAVE_LIRC
  {
    int fd = mp_input_lirc_init();
    if(fd > 0)
      mp_input_add_cmd_fd(fd,1,NULL,(mp_close_func_t)close);
  }
#endif

}

void
mp_input_uninit(void) {
  unsigned int i;

  for(i=0; i < num_key_fd; i++) {
    if(key_fds[i].close_func)
      key_fds[i].close_func(key_fds[i].fd);
  }

  for(i=0; i < num_cmd_fd; i++) {
    if(cmd_fds[i].close_func)
      cmd_fds[i].close_func(cmd_fds[i].fd);
  }
  

#ifdef HAVE_LIRC
  mp_input_lirc_uninit();
#endif

}

#endif /* HAVE_NEW_INPUT */
