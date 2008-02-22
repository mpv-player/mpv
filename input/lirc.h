#ifndef MPLAYER_LIRC_H
#define MPLAYER_LIRC_H

int 
mp_input_lirc_init(void);

int
mp_input_lirc_read(int fd,char* dest, int s);

void
mp_input_lirc_close(int fd);

#endif /* MPLAYER_LIRC_H */
