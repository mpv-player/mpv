#!/bin/bash

echo "==========================================="
echo " Welcome to the MPlayer configuration tool "
echo "==========================================="
echo " We'll ask you some questions on how you   "
echo " want your mplayer to be compiled.	 "
echo "==========================================="
echo ""
echo ""

echo "Where should the X11 libraries be found?"
echo "[enter=auto]"
read X11L

echo "Where should the win32 Codecs be fould?"
echo "[enter=auto]"
read W32

echo "Which default screen width would you like?"
echo "[enter=default]"
read X

echo "Which default screen height would you like?"
echo "[enter=default]"
read Y

echo "Would you like MMX support enabled?"
echo "[Y/A(auto)]"
read MMX

echo "Would you like 3dnow support enabled?"
echo "[Y/A(auto)]"
read DNOW

echo "Would you like SSE support enabled?"
echo "[Y/A(auto)]"
read SSE

echo "Would you like OpenGL support enabled?"
echo "[Y/A(auto)]"
read GL

echo "Would you like SDL support enabled?"
echo "[Y/A(auto)]"
read SDL

echo "Would you like MGA support enabled?"
echo "[Y/A(auto)]"
read MGA

echo "Would you like XMGA support enabled?"
echo "[Y/A(auto)]"
read XMGA

echo "Would you like XV support enabled?"
echo "[Y/A(auto)]"
read XV

echo "Would you like X11 support enabled?"
echo "[Y/A(auto)]"
read X11

echo "Would you like MLIB support enabled? (ONLY Solaris)"
echo "[Y/A(auto)]"
read MLIB

echo "Would you like to use the termcap database for key codes?"
echo "[Y/N]"
read TERMCAP

echo "Would you like to use the XMMP audio drivers?"
echo "[Y/N]"
read XMMP

echo "Would you like to enable LIRC support?"
echo "[Y/N]"
read LIRC

CMD=" "

if [ "$MMX" = "Y" ]; then
 CMD="$CMD --enable-mmx"
fi

if [ "$DNOW" = "Y" ]; then
 CMD="$CMD --enable-3dnow"
fi

if [ "$SSE" = "Y" ]; then
 CMD="$CMD --enable-sse"
fi

if [ "$GL" = "Y" ]; then
 CMD="$CMD --enable-gl"
fi

if [ "$SDL" = "Y" ]; then
 CMD="$CMD --enable-sdl"
fi

if [ "$MGA" = "Y" ]; then
 CMD="$CMD --enable-mga"
fi

if [ "$XMGA" = "Y" ]; then
 CMD="$CMD --enable-xmga"
fi

if [ "$XV" = "Y" ]; then
 CMD="$CMD --enable-xv"
fi

if [ "$X11" = "Y" ]; then
 CMD="$CMD --enable-x11"
fi

if [ "$TERMCAP" = "Y" ]; then
 CMD="$CMD --enable-termcap"
fi

if [ "$XMMP" = "Y" ]; then
 CMD="$CMD --enable-xmmp"
fi

if [ "$LIRC" = "Y" ]; then
 CMD="$CMD --enable-lirc"
fi

if [ "$X11L" != "" ]; then
 CMD="$CMD --with-x11libdir=$X11L"
fi

if [ "$W32" != "" ]; then
 CMD="$CMD --with-win32libdir=$W32"
fi

if [ "$X" != "" ]; then
 CMD="$CMD --size-x=$X"
fi

if [ "$Y" != "" ]; then
 CMD="$CMD --size-x=$Y"
fi

echo $CMD > setup.s

echo "Configuration ended, now please run"
echo " ./configure \`cat setup.s\`"

exit 0
