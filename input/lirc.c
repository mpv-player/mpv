/*
 * This file is part of MPlayer.
 *
 * MPlayer is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * MPlayer is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with MPlayer; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#include <lirc/lirc_client.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <poll.h>

#include "common/common.h"
#include "common/msg.h"
#include "input.h"

struct ctx {
    struct mp_log *log;
    struct lirc_config *lirc_config;
    char* cmd_buf;
    int fd;
};

static struct ctx *mp_input_lirc_init(struct input_ctx *ictx, struct mp_log *log,
                                      char *lirc_configfile)
{
  int lirc_sock;
  int mode;

  mp_verbose(log,"Setting up LIRC support...\n");
  if((lirc_sock=lirc_init("mpv",0))==-1){
    mp_verbose(log,"Failed to open LIRC support. You will not be able to use your remote control.\n");
    return NULL;
  }

  mode = fcntl(lirc_sock, F_GETFL);
  if (mode < 0 || fcntl(lirc_sock, F_SETFL, mode | O_NONBLOCK) < 0) {
    mp_err(log, "setting non-blocking mode failed: %s\n", mp_strerror(errno));
    lirc_deinit();
    return NULL;
  }

  struct lirc_config *lirc_config = NULL;
  if(lirc_readconfig( lirc_configfile,&lirc_config,NULL )!=0 ){
    mp_err(log, "Failed to read LIRC config file %s.\n",
                    lirc_configfile == NULL ? "~/.lircrc" : lirc_configfile);
    lirc_deinit();
    return NULL;
  }

  struct ctx *ctx = talloc_ptrtype(NULL, ctx);
  *ctx = (struct ctx){
      .log = log,
      .lirc_config = lirc_config,
      .fd = lirc_sock,
  };
  return ctx;
}

static int mp_input_lirc_read(void *pctx,int fd,char* dest, int s) {
  int r,cl = 0;
  char *code = NULL,*c = NULL;
  struct ctx *ctx = pctx;

  // We have something in the buffer return it
  if(ctx->cmd_buf != NULL) {
    int l = strlen(ctx->cmd_buf), w = l > s ? s : l;
    memcpy(dest,ctx->cmd_buf,w);
    l -= w;
    if(l > 0)
      memmove(ctx->cmd_buf,&ctx->cmd_buf[w],l+1);
    else {
      free(ctx->cmd_buf);
      ctx->cmd_buf = NULL;
    }
    return w;
  }

  // Nothing in the buffer, poll the lirc fd
  if(lirc_nextcode(&code) != 0) {
    MP_ERR(ctx, "Lirc error.\n");
    return -1;
  }

  if(!code) return 0;

  // We put all cmds in a single buffer separated by \n
  while((r = lirc_code2char(ctx->lirc_config,code,&c))==0 && c!=NULL) {
    int l = strlen(c);
    if(l <= 0)
      continue;
    ctx->cmd_buf = realloc(ctx->cmd_buf,cl+l+2);
    memcpy(&ctx->cmd_buf[cl],c,l);
    cl += l+1;
    ctx->cmd_buf[cl-1] = '\n';
    ctx->cmd_buf[cl] = '\0';
  }

  free(code);

  if(r < 0)
    return -1;
  else if(ctx->cmd_buf) // return the first command in the buffer
    return mp_input_lirc_read(ctx,fd,dest,s);
  else
    return 0;

}

static int mp_input_lirc_close(void *pctx,int fd)
{
  struct ctx *ctx = pctx;
  free(ctx->cmd_buf);
  lirc_freeconfig(ctx->lirc_config);
  lirc_deinit();
  close(fd);
  talloc_free(ctx);
  return 0;
}

static void read_lirc_thread(struct mp_input_src *src, void *param)
{
    int wakeup_fd = mp_input_src_get_wakeup_fd(src);
    struct ctx *ctx = mp_input_lirc_init(src->input_ctx, src->log, param);

    if (!ctx)
        return;

    mp_input_src_init_done(src);

    while (1) {
        struct pollfd fds[2] = {
            { .fd = ctx->fd, .events = POLLIN },
            { .fd = wakeup_fd, .events = POLLIN },
        };
        poll(fds, 2, -1);
        if (!(fds[0].revents & POLLIN))
            break;
        char buffer[128];
        int r = mp_input_lirc_read(ctx, ctx->fd, buffer, sizeof(buffer));
        if (r < 0)
            break;
        mp_input_src_feed_cmd_text(src, buffer, r);
    }

    mp_input_lirc_close(ctx, ctx->fd);
}

void mp_input_lirc_add(struct input_ctx *ictx, char *lirc_configfile)
{
    mp_input_add_thread_src(ictx, (void *)lirc_configfile, read_lirc_thread);
}
