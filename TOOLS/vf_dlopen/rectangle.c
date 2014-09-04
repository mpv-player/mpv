/*
 * Copyright (c) 2012 Rudolf Polzer <divVerent@xonotic.org>
 *
 * This file is part of mpv's vf_dlopen examples.
 *
 * mpv's vf_dlopen examples are free software; you can redistribute them and/or
 * modify them under the terms of the GNU Lesser General Public License as
 * published by the Free Software Foundation; either version 2.1 of the
 * License, or (at your option) any later version.
 *
 * mpv's vf_dlopen examples are distributed in the hope that they will be
 * useful, but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU Lesser
 * General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with mpv's vf_dlopen examples; if not, write to the Free
 * Software Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
 * 02110-1301 USA
 */

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <signal.h>
#include <sys/stat.h>
#include <sys/wait.h>

#include "vf_dlopen.h"
#include "filterutils.h"

/*
 * rectangle
 *
 * usage: --vf=dlopen=/path/to/rectangle.so
 *
 * provides an editable rectangle
 * NOTE: unix only, and requires xterm to be installed. Don't ask.
 */

typedef struct
{
    int x, y, w, h;
    int step;
    int inpid;
    int infd;
    int ansimode;
    int rectmode;
} privdata;

enum {
    RECTMODE_LINES = 0,
    RECTMODE_BOX,
    RECTMODE_COUNT
};

static int put_image(struct vf_dlopen_context *ctx)
{
    privdata *priv = ctx->priv;
    unsigned int p;

    assert(ctx->inpic.planes == ctx->outpic->planes);

    for (p = 0; p < ctx->outpic->planes; ++p) {
        assert(ctx->inpic.planewidth[p] == ctx->outpic->planewidth[p]);
        assert(ctx->inpic.planeheight[p] == ctx->outpic->planeheight[p]);
    }

    char data;
    while(read(priv->infd, &data, sizeof(data)) > 0) {
        // printf("\nMODE: %d, CHAR: %d (%c)\n", priv->ansimode, data, data);
        switch(priv->ansimode) {
            case 0: // initial
                switch(data) {
                    case 27: priv->ansimode = 1; break;
                    default: priv->ansimode = 0; break;
                }
                break;
            case 1: // seen ESC
                switch(data) {
                    case '[': priv->ansimode = 2; break;
                    default: priv->ansimode = 0; break;
                }
                break;
            case 2: // seen ESC [
                switch(data) {
                    case 'D': priv->ansimode = 0; data = 'h'; break; // arrow
                    case 'B': priv->ansimode = 0; data = 'j'; break; // arrow
                    case 'A': priv->ansimode = 0; data = 'k'; break; // arrow
                    case 'C': priv->ansimode = 0; data = 'l'; break; // arrow
                    case 'd': priv->ansimode = 0; data = 'H'; break; // rxvt shift-arrow
                    case 'b': priv->ansimode = 0; data = 'J'; break; // rxvt shift-arrow
                    case 'a': priv->ansimode = 0; data = 'K'; break; // rxvt shift-arrow
                    case 'c': priv->ansimode = 0; data = 'L'; break; // rxvt shift-arrow
                    case '1': priv->ansimode = 3; break;
                    default: priv->ansimode = -1; break;
                }
                break;
            case 3: // seen ESC [ 1
                switch(data) {
                    case ';': priv->ansimode = 4; break;
                    default: priv->ansimode = -1; break;
                }
                break;
            case 4: // seen ESC [ 1 ;
                switch(data) {
                    case '2': priv->ansimode = 5; break;
                    default: priv->ansimode = -1; break;
                }
                break;
            case 5: // seen ESC [ 1 ; 2
                switch(data) {
                    case 'D': priv->ansimode = 0; data = 'H'; break; // xterm shift-arrow
                    case 'B': priv->ansimode = 0; data = 'J'; break; // xterm shift-arrow
                    case 'A': priv->ansimode = 0; data = 'K'; break; // xterm shift-arrow
                    case 'C': priv->ansimode = 0; data = 'L'; break; // xterm shift-arrow
                    default: priv->ansimode = -1; break;
                }
                break;
            case -1: // wait for end of ESC [ sequence
                if((data > '9' || data < '0') && (data != ';')) {
                    priv->ansimode = 0;
                    data = 0; // do not process
                }
                break;
        }

        if(priv->ansimode == 0) {
            switch(data) {
                case 'h': case 'D': priv->x -= priv->step; priv->w += priv->step; break;
                case 'j': case 'B': priv->y += priv->step; priv->h -= priv->step; break;
                case 'k': case 'A': priv->y -= priv->step; priv->h += priv->step; break;
                case 'l': case 'C': priv->x += priv->step; priv->w -= priv->step; break;
                case 'H': case 'd': priv->w -= priv->step; break;
                case 'J': case 'b': priv->h += priv->step; break;
                case 'K': case 'a': priv->h -= priv->step; break;
                case 'L': case 'c': priv->w += priv->step; break;
                case ' ': priv->step ^= 9; break;
                case 9: ++priv->rectmode; priv->rectmode %= RECTMODE_COUNT; break;
            }
        }
    }

    // apply limits
    if(priv->x < 0) {
        priv->w += priv->x;
        priv->x = 0;
    }
    if(priv->y < 0) {
        priv->h += priv->y;
        priv->y = 0;
    }
    if(priv->w < 0) {
        priv->w = 0;
    }
    if(priv->h < 0) {
        priv->h = 0;
    }
    if(priv->x >= (int) ctx->inpic.planewidth[0]) {
        priv->x = ctx->inpic.planewidth[0] - 1;
    }
    if(priv->y >= (int) ctx->inpic.planeheight[0]) {
        priv->y = ctx->inpic.planeheight[0] - 1;
    }
    if(priv->x + priv->w > (int) ctx->inpic.planewidth[0]) {
        priv->w = ctx->inpic.planewidth[0] - priv->x;
    }
    if(priv->y + priv->h > (int) ctx->inpic.planeheight[0]) {
        priv->h = ctx->inpic.planeheight[0] - priv->y;
    }

    // apply step
    priv->x = ((priv->x + priv->step - 1) / priv->step) * priv->step;
    priv->y = ((priv->y + priv->step - 1) / priv->step) * priv->step;
    priv->w = (priv->w / priv->step) * priv->step;
    priv->h = (priv->h / priv->step) * priv->step;

    // print
    printf("\nRECTANGLE: -vf crop=%d:%d:%d:%d\n", priv->w, priv->h, priv->x, priv->y);

    // copy picture
    for (p = 0; p < ctx->outpic->planes; ++p) {
        copy_plane(
                ctx->outpic->plane[p], ctx->outpic->planestride[p],
                ctx->inpic.plane[p], ctx->inpic.planestride[p],
                ctx->inpic.planewidth[p], ctx->inpic.planeheight[p]
            );
    }
    ctx->outpic->pts = ctx->inpic.pts;

    // draw rectangle
#define PUT_PIXEL(x,y) \
    do { \
        int x_ = (x); \
        int y_ = (y); \
        if(x_ >= 0 && y_ >= 0 && x_ < (int) ctx->outpic->planewidth[0] && y_ < (int) ctx->outpic->planeheight[0]) { \
            ctx->outpic->plane[0][y_ * ctx->outpic->planestride[0] + x_] ^= 0x80; \
        } \
    } while(0)
    switch(priv->rectmode) {
        case RECTMODE_LINES:
            {
                unsigned int i;
                unsigned int n;
                if(priv->w > priv->h)
                    n = priv->h / 3;
                else
                    n = priv->w / 3;
                if(n > 64)
                    n = 64;
                for (i = 0; i < n; ++i) {
                    // topleft
                    PUT_PIXEL(priv->x + i, priv->y);
                    if(i)
                        PUT_PIXEL(priv->x, priv->y + i);
                    // topright
                    PUT_PIXEL(priv->x + priv->w - 1 - i, priv->y);
                    if(i)
                        PUT_PIXEL(priv->x + priv->w - 1, priv->y + i);
                    // bottomright
                    PUT_PIXEL(priv->x + priv->w - 1 - i, priv->y + priv->h - 1);
                    if(i)
                        PUT_PIXEL(priv->x + priv->w - 1, priv->y + priv->h - 1 - i);
                    // bottomleft
                    PUT_PIXEL(priv->x + i, priv->y + priv->h - 1);
                    if(i)
                        PUT_PIXEL(priv->x, priv->y + priv->h - 1 - i);
                }
            }
            break;
        case RECTMODE_BOX:
            {
                int x, y;
                for(y = 0; y < priv->y; ++y) {
                    for(x = 0; x < (int) ctx->outpic->planewidth[0]; ++x) {
                        PUT_PIXEL(x, y);
                    }
                }
                for(y = priv->y; y < priv->y + priv->h; ++y) {
                    for(x = 0; x < priv->x; ++x) {
                        PUT_PIXEL(x, y);
                    }
                    for(x = priv->x + priv->w; x < (int) ctx->outpic->planewidth[0]; ++x) {
                        PUT_PIXEL(x, y);
                    }
                }
                for(y = priv->y + priv->h; y < (int) ctx->outpic->planeheight[0]; ++y) {
                    for(x = 0; x < (int) ctx->outpic->planewidth[0]; ++x) {
                        PUT_PIXEL(x, y);
                    }
                }
            }
            break;
    }

    return 1;
}

int config(struct vf_dlopen_context *ctx)
{
    privdata *priv = ctx->priv;
    if(priv->w == 0 || priv->h == 0) {
        priv->x = 0;
        priv->y = 0;
        priv->w = ctx->in_width;
        priv->h = ctx->in_height;
    }
    printf("\nRECTANGLE: reconfigured\n");

    if(priv->inpid == 0) {
        int fd[2];
        if(pipe(fd)) {
            perror("pipe");
            return -1;
        }
        priv->inpid = fork();
        if(priv->inpid < 0) {
            perror("fork");
            return -1;
        }
        if(priv->inpid == 0) {
            close(fd[0]);
            if(fd[1] != 3) {
                dup2(fd[1], 3);
                close(fd[1]);
            }
            execlp("xterm", "xterm",
                    "-geometry", "40x10",
                    "-e",
                    "echo \"rectangle.so control window\";"
                    "echo \"\";"
                    "echo \"Arrow or hjkl: top left corner\";"
                    "echo \"Shift-Arrow or HJKL: bottom right corner\";"
                    "echo \"SPACE: toggle 8-px alignment\";"
                    "echo \"TAB: toggle rectangle display\";"
                    "echo \"\";"
                    "stty raw -echo;"
                    "cat >&3",
                    NULL
            );
            _exit(1);
        }
        close(fd[1]);
        priv->infd = fd[0];
        int flags;
        if(fcntl(priv->infd, F_GETFL, &flags)) {
            perror("fcntl F_GETFL");
            close(priv->infd);
            kill(priv->inpid, SIGTERM);
            waitpid(priv->inpid, NULL, 0);
            priv->inpid = 0;
        }
        flags |= O_NONBLOCK;
        if(fcntl(priv->infd, F_SETFL, flags)) {
            perror("fcntl F_SETFL");
            close(priv->infd);
            kill(priv->inpid, SIGTERM);
            waitpid(priv->inpid, NULL, 0);
            priv->inpid = 0;
        }
    }

    return 1;
}

void uninit(struct vf_dlopen_context *ctx)
{
    privdata *priv = ctx->priv;
    if(priv->inpid) {
        close(priv->infd);
        kill(priv->inpid, SIGTERM);
        waitpid(priv->inpid, NULL, 0);
        priv->inpid = 0;
    }
    printf("\nRECTANGLE: finished\n");
    free(priv);
}

int vf_dlopen_getcontext(struct vf_dlopen_context *ctx, int argc, const char **argv)
{
    VF_DLOPEN_CHECK_VERSION(ctx);
    (void) argc;
    (void) argv;
    static struct vf_dlopen_formatpair map[] = {
        { "yuv420p", "yuv420p" },
        { NULL, NULL }
    };
    privdata *priv = calloc(1,sizeof(privdata));
    priv->step = 8;
    if(argc >= 1)
        priv->w = atoi(argv[0]);
    if(argc >= 2)
        priv->h = atoi(argv[1]);
    if(argc >= 3)
        priv->x = atoi(argv[2]);
    if(argc >= 4)
        priv->y = atoi(argv[3]);
    ctx->priv = priv;
    ctx->format_mapping = map;
    ctx->config = config;
    ctx->put_image = put_image;
    ctx->uninit = uninit;
    return 1;
}
