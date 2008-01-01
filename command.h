#ifndef COMMAND_H
#define COMMAND_H

int run_command(struct MPContext *mpctx, mp_cmd_t *cmd);
char *property_expand_string(struct MPContext *mpctx, char *str);
void property_print_help(void);

#endif /* COMMAND_H */
