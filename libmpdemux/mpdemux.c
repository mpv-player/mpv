#include "config.h"
#include <stdlib.h>

#include "../input/input.h"
int mpdemux_check_interrupt(int time) {
  mp_cmd_t* cmd;
  if((cmd = mp_input_get_cmd(time,0)) == NULL)
    return 0;

  switch(cmd->id) {
  case MP_CMD_QUIT:
  case MP_CMD_PLAY_TREE_STEP:
  case MP_CMD_PLAY_TREE_UP_STEP:
  case MP_CMD_PLAY_ALT_SRC_STEP:
    // The cmd will be executed when we are back in the main loop
    if(! mp_input_queue_cmd(cmd)) {
      printf("mpdemux_check_interrupt: can't queue cmd %s\n",cmd->name);
      mp_cmd_free(cmd);
    }
    return 1;
  default:
    mp_cmd_free(cmd);
    return 0;
  }
}
