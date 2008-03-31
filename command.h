#ifndef MPLAYER_COMMAND_H
#define MPLAYER_COMMAND_H

struct MPContext;
struct mp_cmd;

int run_command(struct MPContext *mpctx, struct mp_cmd *cmd);
char *property_expand_string(struct MPContext *mpctx, char *str);
void property_print_help(void);

#endif /* MPLAYER_COMMAND_H */
