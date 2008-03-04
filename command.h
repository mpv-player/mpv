#ifndef MPLAYER_COMMAND_H
#define MPLAYER_COMMAND_H

#include "mp_core.h"
#include "input/input.h"

int run_command(struct MPContext *mpctx, mp_cmd_t *cmd);
char *property_expand_string(struct MPContext *mpctx, char *str);
void property_print_help(void);

#endif /* MPLAYER_COMMAND_H */
