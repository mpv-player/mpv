#!/usr/bin/perl -w

use POSIX;

sub round {
  my $v = shift;
  
  return floor($v + 0.5);
}

$raw_aspect = 720/576;

if (scalar(@ARGV) < 4) {
  print("Please provide a) the cropped but unscaled resolution (e.g. " .
        "716x524), b) the aspect ratio (either 4/3 or 16/9 for most DVDs), " .
        "c) the video bitrate in kbps (e.g. 800) and d) the movie's fps.\n");
  print("If your DVD is not encoded at 720x576 then change the \$raw_aspect" .
        "variable at the beginning of this script.\n");
  exit(1);
}

($unscaled_width, $unscaled_height) = split('x', $ARGV[0]);
$encoded_at = $ARGV[1];
if ($encoded_at =~ /\//) {
  my @a = split(/\//, $encoded_at);
  $encoded_at = $a[0] / $a[1];
}
$scaled_width = $unscaled_width * ($encoded_at / ($raw_aspect));
$scaled_height = $unscaled_height;
$picture_ar = $scaled_width / $scaled_height;
($bps, $fps) = @ARGV[2, 3];

printf("Prescaled picture: %dx%d, AR %.2f\n", $scaled_width, $scaled_height,
       $picture_ar);
for ($width = 720; $width >= 320; $width -= 16) {
  $height = 16 * round($width / $picture_ar / 16);
  $diff = round($width / $picture_ar - $height);
  $new_ar = $width / $height;
  $picture_ar_error = abs(100 - $picture_ar / $new_ar * 100);
  printf("${width}x${height}, diff % 3d, new AR %.2f, AR error %.2f%% " .
         "scale=%d:%d bpp: %.3f\n", $diff, $new_ar, $picture_ar_error, $width,
         $height, ($bps * 1000) / ($width * $height * $fps));
}
