!!ARBfp1.0
# Custom YUV->RGB conversion program for MPlayer's -vo gl.
# Copyleft (C) Reimar Döffinger, 2005
# Licensed under the GNU GPL v2
# Usage: mplayer -vo gl:yuv=4:customprog=edgedetect.fp
# This is some custom edge-detect like effect.
# Try adjusting the gamma!
# program.env[0].xy contains the size of one source texel
TEMP res, yuv, pos, tmp, sizes;
SWZ sizes, program.env[0], x, y, 0, 0;
TEX yuv.r, fragment.texcoord[0], texture[0], 2D;
ADD pos, fragment.texcoord[0].xyxy, sizes.xwwy; # texels to the right and below
TEX tmp.r, pos.xyxy, texture[0], 2D;
MAD yuv.r, yuv.rrrr, {4}, -tmp.rrrr;
TEX tmp.r, pos.zwzw, texture[0], 2D;
SUB yuv.r, yuv.rrrr, tmp.rrrr;
SUB pos, fragment.texcoord[0].xyxy, sizes.xwwy; # texels to the left and above
TEX tmp.r, pos.xyxy, texture[0], 2D;
SUB yuv.r, yuv.rrrr, tmp.rrrr;
TEX tmp.r, pos.zwzw, texture[0], 2D;
SUB yuv.r, yuv.rrrr, tmp.rrrr;
TEX yuv.g, fragment.texcoord[1], texture[1], 2D;
TEX yuv.b, fragment.texcoord[2], texture[2], 2D;
# now do the normal YUV -> RGB conversion but include effect strength
# multiplication by 2 and 0.5 offset
MAD res, yuv.rrrr, {2.328, 2.328, 2.328, 0}, {-0.37416, 1.03133, -0.58599, 0.125};
MAD res.rgb, yuv.gggg, {0, -0.391, 2.018, 0}, res;
MAD res.rgb, yuv.bbbb, {1.596, -0.813, 0, 0}, res;
# do gamma texture lookup
TEX res.r, res.raaa, texture[3], 2D;
ADD res.a, res.a, 0.25;
TEX res.g, res.gaaa, texture[3], 2D;
ADD res.a, res.a, 0.25;
TEX res.b, res.baaa, texture[3], 2D;
# move res into result, this allows easily commenting out some parts.
MOV result.color, res;
END
