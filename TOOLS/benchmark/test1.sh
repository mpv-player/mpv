
sync
sleep 1
cat $2 >/dev/null
sleep 2
$* -benchmark -nosound | grep BENCHMARKs
$* -benchmark -nosound | grep BENCHMARKs
$* -benchmark -nosound | grep BENCHMARKs
# $* -benchmark -nosound | grep BENCHMARKs
# echo ""
