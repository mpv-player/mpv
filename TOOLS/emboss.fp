!!ARBfp1.0
# Custom YUV->RGB conversion program for MPlayer's -vo gl.
# Copyleft (C) Reimar Döffinger, 2005
# Licensed under the GNU GPL v2
# Usage: mplayer -vo gl:yuv=4:customprog=emboss.fp
# This is an emboss effect.
PARAM sizes = program.env[0];
TEMP res, y, u, v, xdiff, ydiff, pos, tmp;
TEX y, fragment.texcoord[0], texture[0], 2D;
SUB pos, fragment.texcoord[0], sizes.xwww;
TEX tmp, pos, texture[0], 2D;
SUB xdiff, y, tmp;
MAD xdiff, xdiff, {0.5, 0.5, 0.5, 0}, {0.5, 0.5, 0.5, 0};
SUB pos, fragment.texcoord[0], sizes.wyww;
TEX tmp, pos, texture[0], 2D;
SUB ydiff, y, tmp;
MAD res, ydiff, {0.8660, 0.8660, 0.8660, 0}, xdiff;
# now do the normal YUV -> RGB conversion
MAD res, res, {1.164, 1.164, 1.164, 0}, {-0.87416, 0.53133, -1.08599, 0};
TEX u, fragment.texcoord[1], texture[1], 2D;
MAD res, u, {0, -0.391, 2.018, 0}, res;
TEX v, fragment.texcoord[2], texture[2], 2D;
MAD res, v, {1.596, -0.813, 0, 0}, res;
# do gamma texture lookup
ADD res.a, res.a, 0.125;
TEX res.r, res.raaa, texture[3], 2D;
ADD res.a, res.a, 0.25;
TEX res.g, res.gaaa, texture[3], 2D;
ADD res.a, res.a, 0.25;
TEX res.b, res.baaa, texture[3], 2D;
# move res into result, this allows easily commenting out some parts.
ADD result.color, res, {0, 0, 0, 0};
END
