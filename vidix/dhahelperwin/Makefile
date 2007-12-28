include ../../config.mak

all: dhasetup.exe dhahelper.sys

dhasetup.exe: dhasetup.c
	$(CC) -o $@ $<

dhahelper.o: dhahelper.c dhahelper.h
	$(CC) -Wall -Os -c $< -o $@

dhahelper-rc.o: dhahelper.rc common.ver ntverp.h
	$(WINDRES) -I. $< $@

base.tmp: dhahelper.o dhahelper-rc.o
	$(CC)   -Wl,--base-file,$@ \
		-Wl,--entry,_DriverEntry@8 \
		-nostartfiles -nostdlib \
		-o junk.tmp $^ -lntoskrnl
	-rm -f junk.tmp

temp.exp: base.tmp
	dlltool --dllname dhahelper.sys --base-file $< --output-exp $@

dhahelper.sys: temp.exp dhahelper.o dhahelper-rc.o
	$(CC)	-Wl,--subsystem,native \
		-Wl,--image-base,0x10000 \
		-Wl,--file-alignment,0x1000 \
		-Wl,--section-alignment,0x1000 \
		-Wl,--entry,_DriverEntry@8 \
		-Wl,$< \
		-mdll -nostartfiles -nostdlib \
		-o $@ dhahelper.o dhahelper-rc.o \
		-lntoskrnl
	strip $@

clean:
	rm -f *.o *~ dhahelper.sys dhasetup.exe base.tmp temp.exp

distclean: clean

.PHONY: all clean distclean
