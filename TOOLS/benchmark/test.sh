
echo 'File: ' $1
mplayer -frames 0 -vo null $1 | grep VIDEO

echo -n 'DShow DLL  : '
./test1.sh mplayer $1 -vc divxds -vo null |cut -c 15-23 |xargs echo
echo -n 'VfW DLL    : '
./test1.sh mplayer $1 -vc divx -vo null |cut -c 15-23 |xargs echo
echo -n 'DivX4-YV12 : '
./test1.sh mplayer $1 -vc odivx -vo null |cut -c 15-23 |xargs echo
echo -n 'DivX4-YUY2 : '
./test1.sh mplayer $1 -vc divx4 -vo null |cut -c 15-23 |xargs echo
echo -n 'FFmpeg     : '
./test1.sh mplayer $1 -vc ffdivx -vo null |cut -c 15-23 |xargs echo

echo ""
