/* this is userspace utility which allows you to redirect console to another fb device 
 * You can specify devices & consoles by both numbers and devices. Framebuffers numbers
 * are zero based (/dev/fb0 ... ), consoles begins with 1 (/dev/tty1 ... )
 */
#include <linux/fb.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

int main(int argc, char* argv[]) {
	struct fb_con2fbmap c2m;
	char* fbPath;
	u_int32_t con, fb;
	char* e;
	char* progname = strrchr(argv[0], '/');
	int f;

	if (progname)
		progname++;
	else
		progname = argv[0];
	if (argc < 3) {
		fprintf(stderr, "usage: %s fbdev console\n", progname);
		return 1;
	}
	fb = strtoul(argv[1], &e, 10);
	if (*e) {
		struct stat sbf;

		if (stat(argv[1], &sbf)) {	
			fprintf(stderr, "%s: are you sure that %s can be used to describe fbdev?\n", progname, argv[1]);
			return 1;
		}
		if (!S_ISCHR(sbf.st_mode)) {
			fprintf(stderr, "%s: %s must be character device\n", progname, argv[1]);
			return 1;
		}
		fb = sbf.st_rdev & 0xFF;
		if (fb >= 32)
			fb >>= 5;
		fbPath = argv[1];
	} else 
		fbPath = "/dev/fb0";
	con = strtoul(argv[2], &e, 10);
	if (*e) {
		struct stat sbf;

		if (stat(argv[2], &sbf)) {
			fprintf(stderr, "%s: are you sure that %s can be used to describe vt?\n", progname, argv[2]);
			return 1;
		}
		if (!S_ISCHR(sbf.st_mode)) {
			fprintf(stderr, "%s: %s must be character device\n", progname, argv[2]);
			return 1;
		}
		con = sbf.st_rdev & 0xFF;
	}
	c2m.console = con;
	c2m.framebuffer = fb;
	f = open(fbPath, O_RDWR);
	if (f < 0) {
		fprintf(stderr, "%s: Cannot open %s\n", progname, fbPath);
		return 1;
	}
	if (ioctl(f, FBIOPUT_CON2FBMAP, &c2m)) {
		fprintf(stderr, "%s: Cannot set console mapping\n", progname);
		close(f);
		return 1;
	}
	close(f);
	return 0;
}


