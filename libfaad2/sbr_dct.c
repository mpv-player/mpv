/*
** FAAD2 - Freeware Advanced Audio (AAC) Decoder including SBR decoding
** Copyright (C) 2003-2004 M. Bakker, Ahead Software AG, http://www.nero.com
**
** This program is free software; you can redistribute it and/or modify
** it under the terms of the GNU General Public License as published by
** the Free Software Foundation; either version 2 of the License, or
** (at your option) any later version.
**
** This program is distributed in the hope that it will be useful,
** but WITHOUT ANY WARRANTY; without even the implied warranty of
** MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
** GNU General Public License for more details.
**
** You should have received a copy of the GNU General Public License
** along with this program; if not, write to the Free Software
** Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
**
** Any non-GPL usage of this software or parts of this software is strictly
** forbidden.
**
** Commercial non-GPL licensing of this software is possible.
** For more info contact Ahead Software through Mpeg4AAClicense@nero.com.
**
** $Id: sbr_dct.c,v 1.15 2004/09/04 14:56:28 menno Exp $
**/

#include "common.h"

#ifdef SBR_DEC

#ifdef _MSC_VER
#pragma warning(disable:4305)
#pragma warning(disable:4244)
#endif


#include "sbr_dct.h"

void DCT4_32(real_t *y, real_t *x)
{
    real_t f0, f1, f2, f3, f4, f5, f6, f7, f8, f9, f10;
    real_t f11, f12, f13, f14, f15, f16, f17, f18, f19, f20;
    real_t f21, f22, f23, f24, f25, f26, f27, f28, f29, f30;
    real_t f31, f32, f33, f34, f35, f36, f37, f38, f39, f40;
    real_t f41, f42, f43, f44, f45, f46, f47, f48, f49, f50;
    real_t f51, f52, f53, f54, f55, f56, f57, f58, f59, f60;
    real_t f61, f62, f63, f64, f65, f66, f67, f68, f69, f70;
    real_t f71, f72, f73, f74, f75, f76, f77, f78, f79, f80;
    real_t f81, f82, f83, f84, f85, f86, f87, f88, f89, f90;
    real_t f91, f92, f93, f94, f95, f96, f97, f98, f99, f100;
    real_t f101, f102, f103, f104, f105, f106, f107, f108, f109, f110;
    real_t f111, f112, f113, f114, f115, f116, f117, f118, f119, f120;
    real_t f121, f122, f123, f124, f125, f126, f127, f128, f129, f130;
    real_t f131, f132, f133, f134, f135, f136, f137, f138, f139, f140;
    real_t f141, f142, f143, f144, f145, f146, f147, f148, f149, f150;
    real_t f151, f152, f153, f154, f155, f156, f157, f158, f159, f160;
    real_t f161, f162, f163, f164, f165, f166, f167, f168, f169, f170;
    real_t f171, f172, f173, f174, f175, f176, f177, f178, f179, f180;
    real_t f181, f182, f183, f184, f185, f186, f187, f188, f189, f190;
    real_t f191, f192, f193, f194, f195, f196, f197, f198, f199, f200;
    real_t f201, f202, f203, f204, f205, f206, f207, f208, f209, f210;
    real_t f211, f212, f213, f214, f215, f216, f217, f218, f219, f220;
    real_t f221, f222, f223, f224, f225, f226, f227, f228, f229, f230;
    real_t f231, f232, f233, f234, f235, f236, f237, f238, f239, f240;
    real_t f241, f242, f243, f244, f245, f246, f247, f248, f249, f250;
    real_t f251, f252, f253, f254, f255, f256, f257, f258, f259, f260;
    real_t f261, f262, f263, f264, f265, f266, f267, f268, f269, f270;
    real_t f271, f272, f273, f274, f275, f276, f277, f278, f279, f280;
    real_t f281, f282, f283, f284, f285, f286, f287, f288, f289, f290;
    real_t f291, f292, f293, f294, f295, f296, f297, f298, f299, f300;
    real_t f301, f302, f303, f304, f305, f306, f307, f310, f311, f312;
    real_t f313, f316, f317, f318, f319, f322, f323, f324, f325, f328;
    real_t f329, f330, f331, f334, f335, f336, f337, f340, f341, f342;
    real_t f343, f346, f347, f348, f349, f352, f353, f354, f355, f358;
    real_t f359, f360, f361, f364, f365, f366, f367, f370, f371, f372;
    real_t f373, f376, f377, f378, f379, f382, f383, f384, f385, f388;
    real_t f389, f390, f391, f394, f395, f396, f397;

    f0 = x[15] - x[16];
    f1 = x[15] + x[16];
    f2 = MUL_F(FRAC_CONST(0.7071067811865476), f1);
    f3 = MUL_F(FRAC_CONST(0.7071067811865476), f0);
    f4 = x[8] - x[23];
    f5 = x[8] + x[23];
    f6 = MUL_F(FRAC_CONST(0.7071067811865476), f5);
    f7 = MUL_F(FRAC_CONST(0.7071067811865476), f4);
    f8 = x[12] - x[19];
    f9 = x[12] + x[19];
    f10 = MUL_F(FRAC_CONST(0.7071067811865476), f9);
    f11 = MUL_F(FRAC_CONST(0.7071067811865476), f8);
    f12 = x[11] - x[20];
    f13 = x[11] + x[20];
    f14 = MUL_F(FRAC_CONST(0.7071067811865476), f13);
    f15 = MUL_F(FRAC_CONST(0.7071067811865476), f12);
    f16 = x[14] - x[17];
    f17 = x[14] + x[17];
    f18 = MUL_F(FRAC_CONST(0.7071067811865476), f17);
    f19 = MUL_F(FRAC_CONST(0.7071067811865476), f16);
    f20 = x[9] - x[22];
    f21 = x[9] + x[22];
    f22 = MUL_F(FRAC_CONST(0.7071067811865476), f21);
    f23 = MUL_F(FRAC_CONST(0.7071067811865476), f20);
    f24 = x[13] - x[18];
    f25 = x[13] + x[18];
    f26 = MUL_F(FRAC_CONST(0.7071067811865476), f25);
    f27 = MUL_F(FRAC_CONST(0.7071067811865476), f24);
    f28 = x[10] - x[21];
    f29 = x[10] + x[21];
    f30 = MUL_F(FRAC_CONST(0.7071067811865476), f29);
    f31 = MUL_F(FRAC_CONST(0.7071067811865476), f28);
    f32 = x[0] - f2;
    f33 = x[0] + f2;
    f34 = x[31] - f3;
    f35 = x[31] + f3;
    f36 = x[7] - f6;
    f37 = x[7] + f6;
    f38 = x[24] - f7;
    f39 = x[24] + f7;
    f40 = x[3] - f10;
    f41 = x[3] + f10;
    f42 = x[28] - f11;
    f43 = x[28] + f11;
    f44 = x[4] - f14;
    f45 = x[4] + f14;
    f46 = x[27] - f15;
    f47 = x[27] + f15;
    f48 = x[1] - f18;
    f49 = x[1] + f18;
    f50 = x[30] - f19;
    f51 = x[30] + f19;
    f52 = x[6] - f22;
    f53 = x[6] + f22;
    f54 = x[25] - f23;
    f55 = x[25] + f23;
    f56 = x[2] - f26;
    f57 = x[2] + f26;
    f58 = x[29] - f27;
    f59 = x[29] + f27;
    f60 = x[5] - f30;
    f61 = x[5] + f30;
    f62 = x[26] - f31;
    f63 = x[26] + f31;
    f64 = f39 + f37;
    f65 = MUL_F(FRAC_CONST(-0.5411961001461969), f39);
    f66 = MUL_F(FRAC_CONST(0.9238795325112867), f64);
    f67 = MUL_C(COEF_CONST(1.3065629648763766), f37);
    f68 = f65 + f66;
    f69 = f67 - f66;
    f70 = f38 + f36;
    f71 = MUL_C(COEF_CONST(1.3065629648763770), f38);
    f72 = MUL_F(FRAC_CONST(-0.3826834323650904), f70);
    f73 = MUL_F(FRAC_CONST(0.5411961001461961), f36);
    f74 = f71 + f72;
    f75 = f73 - f72;
    f76 = f47 + f45;
    f77 = MUL_F(FRAC_CONST(-0.5411961001461969), f47);
    f78 = MUL_F(FRAC_CONST(0.9238795325112867), f76);
    f79 = MUL_C(COEF_CONST(1.3065629648763766), f45);
    f80 = f77 + f78;
    f81 = f79 - f78;
    f82 = f46 + f44;
    f83 = MUL_C(COEF_CONST(1.3065629648763770), f46);
    f84 = MUL_F(FRAC_CONST(-0.3826834323650904), f82);
    f85 = MUL_F(FRAC_CONST(0.5411961001461961), f44);
    f86 = f83 + f84;
    f87 = f85 - f84;
    f88 = f55 + f53;
    f89 = MUL_F(FRAC_CONST(-0.5411961001461969), f55);
    f90 = MUL_F(FRAC_CONST(0.9238795325112867), f88);
    f91 = MUL_C(COEF_CONST(1.3065629648763766), f53);
    f92 = f89 + f90;
    f93 = f91 - f90;
    f94 = f54 + f52;
    f95 = MUL_C(COEF_CONST(1.3065629648763770), f54);
    f96 = MUL_F(FRAC_CONST(-0.3826834323650904), f94);
    f97 = MUL_F(FRAC_CONST(0.5411961001461961), f52);
    f98 = f95 + f96;
    f99 = f97 - f96;
    f100 = f63 + f61;
    f101 = MUL_F(FRAC_CONST(-0.5411961001461969), f63);
    f102 = MUL_F(FRAC_CONST(0.9238795325112867), f100);
    f103 = MUL_C(COEF_CONST(1.3065629648763766), f61);
    f104 = f101 + f102;
    f105 = f103 - f102;
    f106 = f62 + f60;
    f107 = MUL_C(COEF_CONST(1.3065629648763770), f62);
    f108 = MUL_F(FRAC_CONST(-0.3826834323650904), f106);
    f109 = MUL_F(FRAC_CONST(0.5411961001461961), f60);
    f110 = f107 + f108;
    f111 = f109 - f108;
    f112 = f33 - f68;
    f113 = f33 + f68;
    f114 = f35 - f69;
    f115 = f35 + f69;
    f116 = f32 - f74;
    f117 = f32 + f74;
    f118 = f34 - f75;
    f119 = f34 + f75;
    f120 = f41 - f80;
    f121 = f41 + f80;
    f122 = f43 - f81;
    f123 = f43 + f81;
    f124 = f40 - f86;
    f125 = f40 + f86;
    f126 = f42 - f87;
    f127 = f42 + f87;
    f128 = f49 - f92;
    f129 = f49 + f92;
    f130 = f51 - f93;
    f131 = f51 + f93;
    f132 = f48 - f98;
    f133 = f48 + f98;
    f134 = f50 - f99;
    f135 = f50 + f99;
    f136 = f57 - f104;
    f137 = f57 + f104;
    f138 = f59 - f105;
    f139 = f59 + f105;
    f140 = f56 - f110;
    f141 = f56 + f110;
    f142 = f58 - f111;
    f143 = f58 + f111;
    f144 = f123 + f121;
    f145 = MUL_F(FRAC_CONST(-0.7856949583871021), f123);
    f146 = MUL_F(FRAC_CONST(0.9807852804032304), f144);
    f147 = MUL_C(COEF_CONST(1.1758756024193588), f121);
    f148 = f145 + f146;
    f149 = f147 - f146;
    f150 = f127 + f125;
    f151 = MUL_F(FRAC_CONST(0.2758993792829431), f127);
    f152 = MUL_F(FRAC_CONST(0.5555702330196022), f150);
    f153 = MUL_C(COEF_CONST(1.3870398453221475), f125);
    f154 = f151 + f152;
    f155 = f153 - f152;
    f156 = f122 + f120;
    f157 = MUL_C(COEF_CONST(1.1758756024193591), f122);
    f158 = MUL_F(FRAC_CONST(-0.1950903220161287), f156);
    f159 = MUL_F(FRAC_CONST(0.7856949583871016), f120);
    f160 = f157 + f158;
    f161 = f159 - f158;
    f162 = f126 + f124;
    f163 = MUL_C(COEF_CONST(1.3870398453221473), f126);
    f164 = MUL_F(FRAC_CONST(-0.8314696123025455), f162);
    f165 = MUL_F(FRAC_CONST(-0.2758993792829436), f124);
    f166 = f163 + f164;
    f167 = f165 - f164;
    f168 = f139 + f137;
    f169 = MUL_F(FRAC_CONST(-0.7856949583871021), f139);
    f170 = MUL_F(FRAC_CONST(0.9807852804032304), f168);
    f171 = MUL_C(COEF_CONST(1.1758756024193588), f137);
    f172 = f169 + f170;
    f173 = f171 - f170;
    f174 = f143 + f141;
    f175 = MUL_F(FRAC_CONST(0.2758993792829431), f143);
    f176 = MUL_F(FRAC_CONST(0.5555702330196022), f174);
    f177 = MUL_C(COEF_CONST(1.3870398453221475), f141);
    f178 = f175 + f176;
    f179 = f177 - f176;
    f180 = f138 + f136;
    f181 = MUL_C(COEF_CONST(1.1758756024193591), f138);
    f182 = MUL_F(FRAC_CONST(-0.1950903220161287), f180);
    f183 = MUL_F(FRAC_CONST(0.7856949583871016), f136);
    f184 = f181 + f182;
    f185 = f183 - f182;
    f186 = f142 + f140;
    f187 = MUL_C(COEF_CONST(1.3870398453221473), f142);
    f188 = MUL_F(FRAC_CONST(-0.8314696123025455), f186);
    f189 = MUL_F(FRAC_CONST(-0.2758993792829436), f140);
    f190 = f187 + f188;
    f191 = f189 - f188;
    f192 = f113 - f148;
    f193 = f113 + f148;
    f194 = f115 - f149;
    f195 = f115 + f149;
    f196 = f117 - f154;
    f197 = f117 + f154;
    f198 = f119 - f155;
    f199 = f119 + f155;
    f200 = f112 - f160;
    f201 = f112 + f160;
    f202 = f114 - f161;
    f203 = f114 + f161;
    f204 = f116 - f166;
    f205 = f116 + f166;
    f206 = f118 - f167;
    f207 = f118 + f167;
    f208 = f129 - f172;
    f209 = f129 + f172;
    f210 = f131 - f173;
    f211 = f131 + f173;
    f212 = f133 - f178;
    f213 = f133 + f178;
    f214 = f135 - f179;
    f215 = f135 + f179;
    f216 = f128 - f184;
    f217 = f128 + f184;
    f218 = f130 - f185;
    f219 = f130 + f185;
    f220 = f132 - f190;
    f221 = f132 + f190;
    f222 = f134 - f191;
    f223 = f134 + f191;
    f224 = f211 + f209;
    f225 = MUL_F(FRAC_CONST(-0.8971675863426361), f211);
    f226 = MUL_F(FRAC_CONST(0.9951847266721968), f224);
    f227 = MUL_C(COEF_CONST(1.0932018670017576), f209);
    f228 = f225 + f226;
    f229 = f227 - f226;
    f230 = f215 + f213;
    f231 = MUL_F(FRAC_CONST(-0.4105245275223571), f215);
    f232 = MUL_F(FRAC_CONST(0.8819212643483549), f230);
    f233 = MUL_C(COEF_CONST(1.3533180011743529), f213);
    f234 = f231 + f232;
    f235 = f233 - f232;
    f236 = f219 + f217;
    f237 = MUL_F(FRAC_CONST(0.1386171691990915), f219);
    f238 = MUL_F(FRAC_CONST(0.6343932841636455), f236);
    f239 = MUL_C(COEF_CONST(1.4074037375263826), f217);
    f240 = f237 + f238;
    f241 = f239 - f238;
    f242 = f223 + f221;
    f243 = MUL_F(FRAC_CONST(0.6666556584777466), f223);
    f244 = MUL_F(FRAC_CONST(0.2902846772544623), f242);
    f245 = MUL_C(COEF_CONST(1.2472250129866711), f221);
    f246 = f243 + f244;
    f247 = f245 - f244;
    f248 = f210 + f208;
    f249 = MUL_C(COEF_CONST(1.0932018670017574), f210);
    f250 = MUL_F(FRAC_CONST(-0.0980171403295605), f248);
    f251 = MUL_F(FRAC_CONST(0.8971675863426364), f208);
    f252 = f249 + f250;
    f253 = f251 - f250;
    f254 = f214 + f212;
    f255 = MUL_C(COEF_CONST(1.3533180011743529), f214);
    f256 = MUL_F(FRAC_CONST(-0.4713967368259979), f254);
    f257 = MUL_F(FRAC_CONST(0.4105245275223569), f212);
    f258 = f255 + f256;
    f259 = f257 - f256;
    f260 = f218 + f216;
    f261 = MUL_C(COEF_CONST(1.4074037375263826), f218);
    f262 = MUL_F(FRAC_CONST(-0.7730104533627369), f260);
    f263 = MUL_F(FRAC_CONST(-0.1386171691990913), f216);
    f264 = f261 + f262;
    f265 = f263 - f262;
    f266 = f222 + f220;
    f267 = MUL_C(COEF_CONST(1.2472250129866711), f222);
    f268 = MUL_F(FRAC_CONST(-0.9569403357322089), f266);
    f269 = MUL_F(FRAC_CONST(-0.6666556584777469), f220);
    f270 = f267 + f268;
    f271 = f269 - f268;
    f272 = f193 - f228;
    f273 = f193 + f228;
    f274 = f195 - f229;
    f275 = f195 + f229;
    f276 = f197 - f234;
    f277 = f197 + f234;
    f278 = f199 - f235;
    f279 = f199 + f235;
    f280 = f201 - f240;
    f281 = f201 + f240;
    f282 = f203 - f241;
    f283 = f203 + f241;
    f284 = f205 - f246;
    f285 = f205 + f246;
    f286 = f207 - f247;
    f287 = f207 + f247;
    f288 = f192 - f252;
    f289 = f192 + f252;
    f290 = f194 - f253;
    f291 = f194 + f253;
    f292 = f196 - f258;
    f293 = f196 + f258;
    f294 = f198 - f259;
    f295 = f198 + f259;
    f296 = f200 - f264;
    f297 = f200 + f264;
    f298 = f202 - f265;
    f299 = f202 + f265;
    f300 = f204 - f270;
    f301 = f204 + f270;
    f302 = f206 - f271;
    f303 = f206 + f271;
    f304 = f275 + f273;
    f305 = MUL_F(FRAC_CONST(-0.9751575901732920), f275);
    f306 = MUL_F(FRAC_CONST(0.9996988186962043), f304);
    f307 = MUL_C(COEF_CONST(1.0242400472191164), f273);
    y[0] = f305 + f306;
    y[31] = f307 - f306;
    f310 = f279 + f277;
    f311 = MUL_F(FRAC_CONST(-0.8700688593994936), f279);
    f312 = MUL_F(FRAC_CONST(0.9924795345987100), f310);
    f313 = MUL_C(COEF_CONST(1.1148902097979263), f277);
    y[2] = f311 + f312;
    y[29] = f313 - f312;
    f316 = f283 + f281;
    f317 = MUL_F(FRAC_CONST(-0.7566008898816587), f283);
    f318 = MUL_F(FRAC_CONST(0.9757021300385286), f316);
    f319 = MUL_C(COEF_CONST(1.1948033701953984), f281);
    y[4] = f317 + f318;
    y[27] = f319 - f318;
    f322 = f287 + f285;
    f323 = MUL_F(FRAC_CONST(-0.6358464401941451), f287);
    f324 = MUL_F(FRAC_CONST(0.9495281805930367), f322);
    f325 = MUL_C(COEF_CONST(1.2632099209919283), f285);
    y[6] = f323 + f324;
    y[25] = f325 - f324;
    f328 = f291 + f289;
    f329 = MUL_F(FRAC_CONST(-0.5089684416985408), f291);
    f330 = MUL_F(FRAC_CONST(0.9142097557035307), f328);
    f331 = MUL_C(COEF_CONST(1.3194510697085207), f289);
    y[8] = f329 + f330;
    y[23] = f331 - f330;
    f334 = f295 + f293;
    f335 = MUL_F(FRAC_CONST(-0.3771887988789273), f295);
    f336 = MUL_F(FRAC_CONST(0.8700869911087114), f334);
    f337 = MUL_C(COEF_CONST(1.3629851833384954), f293);
    y[10] = f335 + f336;
    y[21] = f337 - f336;
    f340 = f299 + f297;
    f341 = MUL_F(FRAC_CONST(-0.2417766217337384), f299);
    f342 = MUL_F(FRAC_CONST(0.8175848131515837), f340);
    f343 = MUL_C(COEF_CONST(1.3933930045694289), f297);
    y[12] = f341 + f342;
    y[19] = f343 - f342;
    f346 = f303 + f301;
    f347 = MUL_F(FRAC_CONST(-0.1040360035527077), f303);
    f348 = MUL_F(FRAC_CONST(0.7572088465064845), f346);
    f349 = MUL_C(COEF_CONST(1.4103816894602612), f301);
    y[14] = f347 + f348;
    y[17] = f349 - f348;
    f352 = f274 + f272;
    f353 = MUL_F(FRAC_CONST(0.0347065382144002), f274);
    f354 = MUL_F(FRAC_CONST(0.6895405447370668), f352);
    f355 = MUL_C(COEF_CONST(1.4137876276885337), f272);
    y[16] = f353 + f354;
    y[15] = f355 - f354;
    f358 = f278 + f276;
    f359 = MUL_F(FRAC_CONST(0.1731148370459795), f278);
    f360 = MUL_F(FRAC_CONST(0.6152315905806268), f358);
    f361 = MUL_C(COEF_CONST(1.4035780182072330), f276);
    y[18] = f359 + f360;
    y[13] = f361 - f360;
    f364 = f282 + f280;
    f365 = MUL_F(FRAC_CONST(0.3098559453626100), f282);
    f366 = MUL_F(FRAC_CONST(0.5349976198870972), f364);
    f367 = MUL_C(COEF_CONST(1.3798511851368043), f280);
    y[20] = f365 + f366;
    y[11] = f367 - f366;
    f370 = f286 + f284;
    f371 = MUL_F(FRAC_CONST(0.4436129715409088), f286);
    f372 = MUL_F(FRAC_CONST(0.4496113296546065), f370);
    f373 = MUL_C(COEF_CONST(1.3428356308501219), f284);
    y[22] = f371 + f372;
    y[9] = f373 - f372;
    f376 = f290 + f288;
    f377 = MUL_F(FRAC_CONST(0.5730977622997509), f290);
    f378 = MUL_F(FRAC_CONST(0.3598950365349881), f376);
    f379 = MUL_C(COEF_CONST(1.2928878353697271), f288);
    y[24] = f377 + f378;
    y[7] = f379 - f378;
    f382 = f294 + f292;
    f383 = MUL_F(FRAC_CONST(0.6970633083205415), f294);
    f384 = MUL_F(FRAC_CONST(0.2667127574748984), f382);
    f385 = MUL_C(COEF_CONST(1.2304888232703382), f292);
    y[26] = f383 + f384;
    y[5] = f385 - f384;
    f388 = f298 + f296;
    f389 = MUL_F(FRAC_CONST(0.8143157536286401), f298);
    f390 = MUL_F(FRAC_CONST(0.1709618887603012), f388);
    f391 = MUL_C(COEF_CONST(1.1562395311492424), f296);
    y[28] = f389 + f390;
    y[3] = f391 - f390;
    f394 = f302 + f300;
    f395 = MUL_F(FRAC_CONST(0.9237258930790228), f302);
    f396 = MUL_F(FRAC_CONST(0.0735645635996674), f394);
    f397 = MUL_C(COEF_CONST(1.0708550202783576), f300);
    y[30] = f395 + f396;
    y[1] = f397 - f396;
}

#ifdef SBR_LOW_POWER

void DCT2_16_unscaled(real_t *y, real_t *x)
{
    real_t f0, f1, f2, f3, f4, f5, f6, f7, f8, f9, f10;
    real_t f11, f12, f13, f14, f15, f16, f17, f18, f19, f20;
    real_t f21, f22, f23, f24, f25, f26, f27, f28, f31, f32;
    real_t f33, f34, f37, f38, f39, f40, f41, f42, f43, f44;
    real_t f45, f46, f47, f48, f49, f51, f53, f54, f57, f58;
    real_t f59, f60, f61, f62, f63, f64, f65, f66, f67, f68;
    real_t f69, f70, f71, f72, f73, f74, f75, f76, f77, f78;
    real_t f79, f80, f81, f82, f83, f84, f85, f86, f87, f88;
    real_t f89, f90, f91, f92, f95, f96, f97, f98, f101, f102;
    real_t f103, f104, f107, f108, f109, f110;

    f0 = x[0] - x[15];
    f1 = x[0] + x[15];
    f2 = x[1] - x[14];
    f3 = x[1] + x[14];
    f4 = x[2] - x[13];
    f5 = x[2] + x[13];
    f6 = x[3] - x[12];
    f7 = x[3] + x[12];
    f8 = x[4] - x[11];
    f9 = x[4] + x[11];
    f10 = x[5] - x[10];
    f11 = x[5] + x[10];
    f12 = x[6] - x[9];
    f13 = x[6] + x[9];
    f14 = x[7] - x[8];
    f15 = x[7] + x[8];
    f16 = f1 - f15;
    f17 = f1 + f15;
    f18 = f3 - f13;
    f19 = f3 + f13;
    f20 = f5 - f11;
    f21 = f5 + f11;
    f22 = f7 - f9;
    f23 = f7 + f9;
    f24 = f17 - f23;
    f25 = f17 + f23;
    f26 = f19 - f21;
    f27 = f19 + f21;
    f28 = f25 - f27;
    y[0] = f25 + f27;
    y[8] = MUL_F(f28, FRAC_CONST(0.7071067811865476));
    f31 = f24 + f26;
    f32 = MUL_C(f24, COEF_CONST(1.3065629648763766));
    f33 = MUL_F(f31, FRAC_CONST(-0.9238795325112866));
    f34 = MUL_F(f26, FRAC_CONST(-0.5411961001461967));
    y[12] = f32 + f33;
    y[4] = f34 - f33;
    f37 = f16 + f22;
    f38 = MUL_C(f16, COEF_CONST(1.1758756024193588));
    f39 = MUL_F(f37, FRAC_CONST(-0.9807852804032304));
    f40 = MUL_F(f22, FRAC_CONST(-0.7856949583871021));
    f41 = f38 + f39;
    f42 = f40 - f39;
    f43 = f18 + f20;
    f44 = MUL_C(f18, COEF_CONST(1.3870398453221473));
    f45 = MUL_F(f43, FRAC_CONST(-0.8314696123025455));
    f46 = MUL_F(f20, FRAC_CONST(-0.2758993792829436));
    f47 = f44 + f45;
    f48 = f46 - f45;
    f49 = f42 - f48;
    y[2] = f42 + f48;
    f51 = MUL_F(f49, FRAC_CONST(0.7071067811865476));
    y[14] = f41 - f47;
    f53 = f41 + f47;
    f54 = MUL_F(f53, FRAC_CONST(0.7071067811865476));
    y[10] = f51 - f54;
    y[6] = f51 + f54;
    f57 = f2 - f4;
    f58 = f2 + f4;
    f59 = f6 - f8;
    f60 = f6 + f8;
    f61 = f10 - f12;
    f62 = f10 + f12;
    f63 = MUL_F(f60, FRAC_CONST(0.7071067811865476));
    f64 = f0 - f63;
    f65 = f0 + f63;
    f66 = f58 + f62;
    f67 = MUL_C(f58, COEF_CONST(1.3065629648763766));
    f68 = MUL_F(f66, FRAC_CONST(-0.9238795325112866));
    f69 = MUL_F(f62, FRAC_CONST(-0.5411961001461967));
    f70 = f67 + f68;
    f71 = f69 - f68;
    f72 = f65 - f71;
    f73 = f65 + f71;
    f74 = f64 - f70;
    f75 = f64 + f70;
    f76 = MUL_F(f59, FRAC_CONST(0.7071067811865476));
    f77 = f14 - f76;
    f78 = f14 + f76;
    f79 = f61 + f57;
    f80 = MUL_C(f61, COEF_CONST(1.3065629648763766));
    f81 = MUL_F(f79, FRAC_CONST(-0.9238795325112866));
    f82 = MUL_F(f57, FRAC_CONST(-0.5411961001461967));
    f83 = f80 + f81;
    f84 = f82 - f81;
    f85 = f78 - f84;
    f86 = f78 + f84;
    f87 = f77 - f83;
    f88 = f77 + f83;
    f89 = f86 + f73;
    f90 = MUL_F(f86, FRAC_CONST(-0.8971675863426361));
    f91 = MUL_F(f89, FRAC_CONST(0.9951847266721968));
    f92 = MUL_C(f73, COEF_CONST(1.0932018670017576));
    y[1] = f90 + f91;
    y[15] = f92 - f91;
    f95 = f75 - f88;
    f96 = MUL_F(f88, FRAC_CONST(-0.6666556584777466));
    f97 = MUL_F(f95, FRAC_CONST(0.9569403357322089));
    f98 = MUL_C(f75, COEF_CONST(1.2472250129866713));
    y[3] = f97 - f96;
    y[13] = f98 - f97;
    f101 = f87 + f74;
    f102 = MUL_F(f87, FRAC_CONST(-0.4105245275223571));
    f103 = MUL_F(f101, FRAC_CONST(0.8819212643483549));
    f104 = MUL_C(f74, COEF_CONST(1.3533180011743529));
    y[5] = f102 + f103;
    y[11] = f104 - f103;
    f107 = f72 - f85;
    f108 = MUL_F(f85, FRAC_CONST(-0.1386171691990915));
    f109 = MUL_F(f107, FRAC_CONST(0.7730104533627370));
    f110 = MUL_C(f72, COEF_CONST(1.4074037375263826));
    y[7] = f109 - f108;
    y[9] = f110 - f109;
}

void DCT4_16(real_t *y, real_t *x)
{
    real_t f0, f1, f2, f3, f4, f5, f6, f7, f8, f9, f10;
    real_t f11, f12, f13, f14, f15, f16, f17, f18, f19, f20;
    real_t f21, f22, f23, f24, f25, f26, f27, f28, f29, f30;
    real_t f31, f32, f33, f34, f35, f36, f37, f38, f39, f40;
    real_t f41, f42, f43, f44, f45, f46, f47, f48, f49, f50;
    real_t f51, f52, f53, f54, f55, f56, f57, f58, f59, f60;
    real_t f61, f62, f63, f64, f65, f66, f67, f68, f69, f70;
    real_t f71, f72, f73, f74, f75, f76, f77, f78, f79, f80;
    real_t f81, f82, f83, f84, f85, f86, f87, f88, f89, f90;
    real_t f91, f92, f93, f94, f95, f96, f97, f98, f99, f100;
    real_t f101, f102, f103, f104, f105, f106, f107, f108, f109, f110;
    real_t f111, f112, f113, f114, f115, f116, f117, f118, f119, f120;
    real_t f121, f122, f123, f124, f125, f126, f127, f128, f130, f132;
    real_t f134, f136, f138, f140, f142, f144, f145, f148, f149, f152;
    real_t f153, f156, f157;

    f0 = x[0] + x[15];
    f1 = MUL_C(COEF_CONST(1.0478631305325901), x[0]);
    f2 = MUL_F(FRAC_CONST(-0.9987954562051724), f0);
    f3 = MUL_F(FRAC_CONST(-0.9497277818777548), x[15]);
    f4 = f1 + f2;
    f5 = f3 - f2;
    f6 = x[2] + x[13];
    f7 = MUL_C(COEF_CONST(1.2130114330978077), x[2]);
    f8 = MUL_F(FRAC_CONST(-0.9700312531945440), f6);
    f9 = MUL_F(FRAC_CONST(-0.7270510732912803), x[13]);
    f10 = f7 + f8;
    f11 = f9 - f8;
    f12 = x[4] + x[11];
    f13 = MUL_C(COEF_CONST(1.3315443865537255), x[4]);
    f14 = MUL_F(FRAC_CONST(-0.9039892931234433), f12);
    f15 = MUL_F(FRAC_CONST(-0.4764341996931612), x[11]);
    f16 = f13 + f14;
    f17 = f15 - f14;
    f18 = x[6] + x[9];
    f19 = MUL_C(COEF_CONST(1.3989068359730781), x[6]);
    f20 = MUL_F(FRAC_CONST(-0.8032075314806453), f18);
    f21 = MUL_F(FRAC_CONST(-0.2075082269882124), x[9]);
    f22 = f19 + f20;
    f23 = f21 - f20;
    f24 = x[8] + x[7];
    f25 = MUL_C(COEF_CONST(1.4125100802019777), x[8]);
    f26 = MUL_F(FRAC_CONST(-0.6715589548470187), f24);
    f27 = MUL_F(FRAC_CONST(0.0693921705079402), x[7]);
    f28 = f25 + f26;
    f29 = f27 - f26;
    f30 = x[10] + x[5];
    f31 = MUL_C(COEF_CONST(1.3718313541934939), x[10]);
    f32 = MUL_F(FRAC_CONST(-0.5141027441932219), f30);
    f33 = MUL_F(FRAC_CONST(0.3436258658070501), x[5]);
    f34 = f31 + f32;
    f35 = f33 - f32;
    f36 = x[12] + x[3];
    f37 = MUL_C(COEF_CONST(1.2784339185752409), x[12]);
    f38 = MUL_F(FRAC_CONST(-0.3368898533922200), f36);
    f39 = MUL_F(FRAC_CONST(0.6046542117908008), x[3]);
    f40 = f37 + f38;
    f41 = f39 - f38;
    f42 = x[14] + x[1];
    f43 = MUL_C(COEF_CONST(1.1359069844201433), x[14]);
    f44 = MUL_F(FRAC_CONST(-0.1467304744553624), f42);
    f45 = MUL_F(FRAC_CONST(0.8424460355094185), x[1]);
    f46 = f43 + f44;
    f47 = f45 - f44;
    f48 = f5 - f29;
    f49 = f5 + f29;
    f50 = f4 - f28;
    f51 = f4 + f28;
    f52 = f11 - f35;
    f53 = f11 + f35;
    f54 = f10 - f34;
    f55 = f10 + f34;
    f56 = f17 - f41;
    f57 = f17 + f41;
    f58 = f16 - f40;
    f59 = f16 + f40;
    f60 = f23 - f47;
    f61 = f23 + f47;
    f62 = f22 - f46;
    f63 = f22 + f46;
    f64 = f48 + f50;
    f65 = MUL_C(COEF_CONST(1.1758756024193588), f48);
    f66 = MUL_F(FRAC_CONST(-0.9807852804032304), f64);
    f67 = MUL_F(FRAC_CONST(-0.7856949583871021), f50);
    f68 = f65 + f66;
    f69 = f67 - f66;
    f70 = f52 + f54;
    f71 = MUL_C(COEF_CONST(1.3870398453221475), f52);
    f72 = MUL_F(FRAC_CONST(-0.5555702330196022), f70);
    f73 = MUL_F(FRAC_CONST(0.2758993792829431), f54);
    f74 = f71 + f72;
    f75 = f73 - f72;
    f76 = f56 + f58;
    f77 = MUL_F(FRAC_CONST(0.7856949583871022), f56);
    f78 = MUL_F(FRAC_CONST(0.1950903220161283), f76);
    f79 = MUL_C(COEF_CONST(1.1758756024193586), f58);
    f80 = f77 + f78;
    f81 = f79 - f78;
    f82 = f60 + f62;
    f83 = MUL_F(FRAC_CONST(-0.2758993792829430), f60);
    f84 = MUL_F(FRAC_CONST(0.8314696123025452), f82);
    f85 = MUL_C(COEF_CONST(1.3870398453221475), f62);
    f86 = f83 + f84;
    f87 = f85 - f84;
    f88 = f49 - f57;
    f89 = f49 + f57;
    f90 = f51 - f59;
    f91 = f51 + f59;
    f92 = f53 - f61;
    f93 = f53 + f61;
    f94 = f55 - f63;
    f95 = f55 + f63;
    f96 = f69 - f81;
    f97 = f69 + f81;
    f98 = f68 - f80;
    f99 = f68 + f80;
    f100 = f75 - f87;
    f101 = f75 + f87;
    f102 = f74 - f86;
    f103 = f74 + f86;
    f104 = f88 + f90;
    f105 = MUL_C(COEF_CONST(1.3065629648763766), f88);
    f106 = MUL_F(FRAC_CONST(-0.9238795325112866), f104);
    f107 = MUL_F(FRAC_CONST(-0.5411961001461967), f90);
    f108 = f105 + f106;
    f109 = f107 - f106;
    f110 = f92 + f94;
    f111 = MUL_F(FRAC_CONST(0.5411961001461969), f92);
    f112 = MUL_F(FRAC_CONST(0.3826834323650898), f110);
    f113 = MUL_C(COEF_CONST(1.3065629648763766), f94);
    f114 = f111 + f112;
    f115 = f113 - f112;
    f116 = f96 + f98;
    f117 = MUL_C(COEF_CONST(1.3065629648763766), f96);
    f118 = MUL_F(FRAC_CONST(-0.9238795325112866), f116);
    f119 = MUL_F(FRAC_CONST(-0.5411961001461967), f98);
    f120 = f117 + f118;
    f121 = f119 - f118;
    f122 = f100 + f102;
    f123 = MUL_F(FRAC_CONST(0.5411961001461969), f100);
    f124 = MUL_F(FRAC_CONST(0.3826834323650898), f122);
    f125 = MUL_C(COEF_CONST(1.3065629648763766), f102);
    f126 = f123 + f124;
    f127 = f125 - f124;
    f128 = f89 - f93;
    y[0] = f89 + f93;
    f130 = f91 - f95;
    y[15] = f91 + f95;
    f132 = f109 - f115;
    y[3] = f109 + f115;
    f134 = f108 - f114;
    y[12] = f108 + f114;
    f136 = f97 - f101;
    y[1] = f97 + f101;
    f138 = f99 - f103;
    y[14] = f99 + f103;
    f140 = f121 - f127;
    y[2] = f121 + f127;
    f142 = f120 - f126;
    y[13] = f120 + f126;
    f144 = f128 - f130;
    f145 = f128 + f130;
    y[8] = MUL_F(FRAC_CONST(0.7071067811865474), f144);
    y[7] = MUL_F(FRAC_CONST(0.7071067811865474), f145);
    f148 = f132 - f134;
    f149 = f132 + f134;
    y[11] = MUL_F(FRAC_CONST(0.7071067811865474), f148);
    y[4] = MUL_F(FRAC_CONST(0.7071067811865474), f149);
    f152 = f136 - f138;
    f153 = f136 + f138;
    y[9] = MUL_F(FRAC_CONST(0.7071067811865474), f152);
    y[6] = MUL_F(FRAC_CONST(0.7071067811865474), f153);
    f156 = f140 - f142;
    f157 = f140 + f142;
    y[10] = MUL_F(FRAC_CONST(0.7071067811865474), f156);
    y[5] = MUL_F(FRAC_CONST(0.7071067811865474), f157);
}

void DCT3_32_unscaled(real_t *y, real_t *x)
{
    real_t f0, f1, f2, f3, f4, f5, f6, f7, f8, f9, f10;
    real_t f11, f12, f13, f14, f15, f16, f17, f18, f19, f20;
    real_t f21, f22, f23, f24, f25, f26, f27, f28, f29, f30;
    real_t f31, f32, f33, f34, f35, f36, f37, f38, f39, f40;
    real_t f41, f42, f43, f44, f45, f46, f47, f48, f49, f50;
    real_t f51, f52, f53, f54, f55, f56, f57, f58, f59, f60;
    real_t f61, f62, f63, f64, f65, f66, f67, f68, f69, f70;
    real_t f71, f72, f73, f74, f75, f76, f77, f78, f79, f80;
    real_t f81, f82, f83, f84, f85, f86, f87, f88, f89, f90;
    real_t f91, f92, f93, f94, f95, f96, f97, f98, f99, f100;
    real_t f101, f102, f103, f104, f105, f106, f107, f108, f109, f110;
    real_t f111, f112, f113, f114, f115, f116, f117, f118, f119, f120;
    real_t f121, f122, f123, f124, f125, f126, f127, f128, f129, f130;
    real_t f131, f132, f133, f134, f135, f136, f137, f138, f139, f140;
    real_t f141, f142, f143, f144, f145, f146, f147, f148, f149, f150;
    real_t f151, f152, f153, f154, f155, f156, f157, f158, f159, f160;
    real_t f161, f162, f163, f164, f165, f166, f167, f168, f169, f170;
    real_t f171, f172, f173, f174, f175, f176, f177, f178, f179, f180;
    real_t f181, f182, f183, f184, f185, f186, f187, f188, f189, f190;
    real_t f191, f192, f193, f194, f195, f196, f197, f198, f199, f200;
    real_t f201, f202, f203, f204, f205, f206, f207, f208, f209, f210;
    real_t f211, f212, f213, f214, f215, f216, f217, f218, f219, f220;
    real_t f221, f222, f223, f224, f225, f226, f227, f228, f229, f230;
    real_t f231, f232, f233, f234, f235, f236, f237, f238, f239, f240;
    real_t f241, f242, f243, f244, f245, f246, f247, f248, f249, f250;
    real_t f251, f252, f253, f254, f255, f256, f257, f258, f259, f260;
    real_t f261, f262, f263, f264, f265, f266, f267, f268, f269, f270;
    real_t f271, f272;

    f0 = MUL_F(x[16], FRAC_CONST(0.7071067811865476));
    f1 = x[0] - f0;
    f2 = x[0] + f0;
    f3 = x[8] + x[24];
    f4 = MUL_C(x[8], COEF_CONST(1.3065629648763766));
    f5 = MUL_F(f3, FRAC_CONST((-0.9238795325112866)));
    f6 = MUL_F(x[24], FRAC_CONST((-0.5411961001461967)));
    f7 = f4 + f5;
    f8 = f6 - f5;
    f9 = f2 - f8;
    f10 = f2 + f8;
    f11 = f1 - f7;
    f12 = f1 + f7;
    f13 = x[4] + x[28];
    f14 = MUL_C(x[4], COEF_CONST(1.1758756024193588));
    f15 = MUL_F(f13, FRAC_CONST((-0.9807852804032304)));
    f16 = MUL_F(x[28], FRAC_CONST((-0.7856949583871021)));
    f17 = f14 + f15;
    f18 = f16 - f15;
    f19 = x[12] + x[20];
    f20 = MUL_C(x[12], COEF_CONST(1.3870398453221473));
    f21 = MUL_F(f19, FRAC_CONST((-0.8314696123025455)));
    f22 = MUL_F(x[20], FRAC_CONST((-0.2758993792829436)));
    f23 = f20 + f21;
    f24 = f22 - f21;
    f25 = f18 - f24;
    f26 = f18 + f24;
    f27 = MUL_F(f25, FRAC_CONST(0.7071067811865476));
    f28 = f17 - f23;
    f29 = f17 + f23;
    f30 = MUL_F(f29, FRAC_CONST(0.7071067811865476));
    f31 = f27 - f30;
    f32 = f27 + f30;
    f33 = f10 - f26;
    f34 = f10 + f26;
    f35 = f12 - f32;
    f36 = f12 + f32;
    f37 = f11 - f31;
    f38 = f11 + f31;
    f39 = f9 - f28;
    f40 = f9 + f28;
    f41 = x[2] + x[30];
    f42 = MUL_C(x[2], COEF_CONST(1.0932018670017569));
    f43 = MUL_F(f41, FRAC_CONST((-0.9951847266721969)));
    f44 = MUL_F(x[30], FRAC_CONST((-0.8971675863426368)));
    f45 = f42 + f43;
    f46 = f44 - f43;
    f47 = x[6] + x[26];
    f48 = MUL_C(x[6], COEF_CONST(1.2472250129866711));
    f49 = MUL_F(f47, FRAC_CONST((-0.9569403357322089)));
    f50 = MUL_F(x[26], FRAC_CONST((-0.6666556584777469)));
    f51 = f48 + f49;
    f52 = f50 - f49;
    f53 = x[10] + x[22];
    f54 = MUL_C(x[10], COEF_CONST(1.3533180011743526));
    f55 = MUL_F(f53, FRAC_CONST((-0.8819212643483551)));
    f56 = MUL_F(x[22], FRAC_CONST((-0.4105245275223575)));
    f57 = f54 + f55;
    f58 = f56 - f55;
    f59 = x[14] + x[18];
    f60 = MUL_C(x[14], COEF_CONST(1.4074037375263826));
    f61 = MUL_F(f59, FRAC_CONST((-0.7730104533627369)));
    f62 = MUL_F(x[18], FRAC_CONST((-0.1386171691990913)));
    f63 = f60 + f61;
    f64 = f62 - f61;
    f65 = f46 - f64;
    f66 = f46 + f64;
    f67 = f52 - f58;
    f68 = f52 + f58;
    f69 = f66 - f68;
    f70 = f66 + f68;
    f71 = MUL_F(f69, FRAC_CONST(0.7071067811865476));
    f72 = f65 + f67;
    f73 = MUL_C(f65, COEF_CONST(1.3065629648763766));
    f74 = MUL_F(f72, FRAC_CONST((-0.9238795325112866)));
    f75 = MUL_F(f67, FRAC_CONST((-0.5411961001461967)));
    f76 = f73 + f74;
    f77 = f75 - f74;
    f78 = f45 - f63;
    f79 = f45 + f63;
    f80 = f51 - f57;
    f81 = f51 + f57;
    f82 = f79 + f81;
    f83 = MUL_C(f79, COEF_CONST(1.3065629648763770));
    f84 = MUL_F(f82, FRAC_CONST((-0.3826834323650904)));
    f85 = MUL_F(f81, FRAC_CONST(0.5411961001461961));
    f86 = f83 + f84;
    f87 = f85 - f84;
    f88 = f78 - f80;
    f89 = f78 + f80;
    f90 = MUL_F(f89, FRAC_CONST(0.7071067811865476));
    f91 = f77 - f87;
    f92 = f77 + f87;
    f93 = f71 - f90;
    f94 = f71 + f90;
    f95 = f76 - f86;
    f96 = f76 + f86;
    f97 = f34 - f70;
    f98 = f34 + f70;
    f99 = f36 - f92;
    f100 = f36 + f92;
    f101 = f38 - f91;
    f102 = f38 + f91;
    f103 = f40 - f94;
    f104 = f40 + f94;
    f105 = f39 - f93;
    f106 = f39 + f93;
    f107 = f37 - f96;
    f108 = f37 + f96;
    f109 = f35 - f95;
    f110 = f35 + f95;
    f111 = f33 - f88;
    f112 = f33 + f88;
    f113 = x[1] + x[31];
    f114 = MUL_C(x[1], COEF_CONST(1.0478631305325901));
    f115 = MUL_F(f113, FRAC_CONST((-0.9987954562051724)));
    f116 = MUL_F(x[31], FRAC_CONST((-0.9497277818777548)));
    f117 = f114 + f115;
    f118 = f116 - f115;
    f119 = x[5] + x[27];
    f120 = MUL_C(x[5], COEF_CONST(1.2130114330978077));
    f121 = MUL_F(f119, FRAC_CONST((-0.9700312531945440)));
    f122 = MUL_F(x[27], FRAC_CONST((-0.7270510732912803)));
    f123 = f120 + f121;
    f124 = f122 - f121;
    f125 = x[9] + x[23];
    f126 = MUL_C(x[9], COEF_CONST(1.3315443865537255));
    f127 = MUL_F(f125, FRAC_CONST((-0.9039892931234433)));
    f128 = MUL_F(x[23], FRAC_CONST((-0.4764341996931612)));
    f129 = f126 + f127;
    f130 = f128 - f127;
    f131 = x[13] + x[19];
    f132 = MUL_C(x[13], COEF_CONST(1.3989068359730781));
    f133 = MUL_F(f131, FRAC_CONST((-0.8032075314806453)));
    f134 = MUL_F(x[19], FRAC_CONST((-0.2075082269882124)));
    f135 = f132 + f133;
    f136 = f134 - f133;
    f137 = x[17] + x[15];
    f138 = MUL_C(x[17], COEF_CONST(1.4125100802019777));
    f139 = MUL_F(f137, FRAC_CONST((-0.6715589548470187)));
    f140 = MUL_F(x[15], FRAC_CONST(0.0693921705079402));
    f141 = f138 + f139;
    f142 = f140 - f139;
    f143 = x[21] + x[11];
    f144 = MUL_C(x[21], COEF_CONST(1.3718313541934939));
    f145 = MUL_F(f143, FRAC_CONST((-0.5141027441932219)));
    f146 = MUL_F(x[11], FRAC_CONST(0.3436258658070501));
    f147 = f144 + f145;
    f148 = f146 - f145;
    f149 = x[25] + x[7];
    f150 = MUL_C(x[25], COEF_CONST(1.2784339185752409));
    f151 = MUL_F(f149, FRAC_CONST((-0.3368898533922200)));
    f152 = MUL_F(x[7], FRAC_CONST(0.6046542117908008));
    f153 = f150 + f151;
    f154 = f152 - f151;
    f155 = x[29] + x[3];
    f156 = MUL_C(x[29], COEF_CONST(1.1359069844201433));
    f157 = MUL_F(f155, FRAC_CONST((-0.1467304744553624)));
    f158 = MUL_F(x[3], FRAC_CONST(0.8424460355094185));
    f159 = f156 + f157;
    f160 = f158 - f157;
    f161 = f118 - f142;
    f162 = f118 + f142;
    f163 = f117 - f141;
    f164 = f117 + f141;
    f165 = f124 - f148;
    f166 = f124 + f148;
    f167 = f123 - f147;
    f168 = f123 + f147;
    f169 = f130 - f154;
    f170 = f130 + f154;
    f171 = f129 - f153;
    f172 = f129 + f153;
    f173 = f136 - f160;
    f174 = f136 + f160;
    f175 = f135 - f159;
    f176 = f135 + f159;
    f177 = f161 + f163;
    f178 = MUL_C(f161, COEF_CONST(1.1758756024193588));
    f179 = MUL_F(f177, FRAC_CONST((-0.9807852804032304)));
    f180 = MUL_F(f163, FRAC_CONST((-0.7856949583871021)));
    f181 = f178 + f179;
    f182 = f180 - f179;
    f183 = f165 + f167;
    f184 = MUL_C(f165, COEF_CONST(1.3870398453221475));
    f185 = MUL_F(f183, FRAC_CONST((-0.5555702330196022)));
    f186 = MUL_F(f167, FRAC_CONST(0.2758993792829431));
    f187 = f184 + f185;
    f188 = f186 - f185;
    f189 = f169 + f171;
    f190 = MUL_F(f169, FRAC_CONST(0.7856949583871022));
    f191 = MUL_F(f189, FRAC_CONST(0.1950903220161283));
    f192 = MUL_C(f171, COEF_CONST(1.1758756024193586));
    f193 = f190 + f191;
    f194 = f192 - f191;
    f195 = f173 + f175;
    f196 = MUL_F(f173, FRAC_CONST((-0.2758993792829430)));
    f197 = MUL_F(f195, FRAC_CONST(0.8314696123025452));
    f198 = MUL_C(f175, COEF_CONST(1.3870398453221475));
    f199 = f196 + f197;
    f200 = f198 - f197;
    f201 = f162 - f170;
    f202 = f162 + f170;
    f203 = f164 - f172;
    f204 = f164 + f172;
    f205 = f166 - f174;
    f206 = f166 + f174;
    f207 = f168 - f176;
    f208 = f168 + f176;
    f209 = f182 - f194;
    f210 = f182 + f194;
    f211 = f181 - f193;
    f212 = f181 + f193;
    f213 = f188 - f200;
    f214 = f188 + f200;
    f215 = f187 - f199;
    f216 = f187 + f199;
    f217 = f201 + f203;
    f218 = MUL_C(f201, COEF_CONST(1.3065629648763766));
    f219 = MUL_F(f217, FRAC_CONST((-0.9238795325112866)));
    f220 = MUL_F(f203, FRAC_CONST((-0.5411961001461967)));
    f221 = f218 + f219;
    f222 = f220 - f219;
    f223 = f205 + f207;
    f224 = MUL_F(f205, FRAC_CONST(0.5411961001461969));
    f225 = MUL_F(f223, FRAC_CONST(0.3826834323650898));
    f226 = MUL_C(f207, COEF_CONST(1.3065629648763766));
    f227 = f224 + f225;
    f228 = f226 - f225;
    f229 = f209 + f211;
    f230 = MUL_C(f209, COEF_CONST(1.3065629648763766));
    f231 = MUL_F(f229, FRAC_CONST((-0.9238795325112866)));
    f232 = MUL_F(f211, FRAC_CONST((-0.5411961001461967)));
    f233 = f230 + f231;
    f234 = f232 - f231;
    f235 = f213 + f215;
    f236 = MUL_F(f213, FRAC_CONST(0.5411961001461969));
    f237 = MUL_F(f235, FRAC_CONST(0.3826834323650898));
    f238 = MUL_C(f215, COEF_CONST(1.3065629648763766));
    f239 = f236 + f237;
    f240 = f238 - f237;
    f241 = f202 - f206;
    f242 = f202 + f206;
    f243 = f204 - f208;
    f244 = f204 + f208;
    f245 = f222 - f228;
    f246 = f222 + f228;
    f247 = f221 - f227;
    f248 = f221 + f227;
    f249 = f210 - f214;
    f250 = f210 + f214;
    f251 = f212 - f216;
    f252 = f212 + f216;
    f253 = f234 - f240;
    f254 = f234 + f240;
    f255 = f233 - f239;
    f256 = f233 + f239;
    f257 = f241 - f243;
    f258 = f241 + f243;
    f259 = MUL_F(f257, FRAC_CONST(0.7071067811865474));
    f260 = MUL_F(f258, FRAC_CONST(0.7071067811865474));
    f261 = f245 - f247;
    f262 = f245 + f247;
    f263 = MUL_F(f261, FRAC_CONST(0.7071067811865474));
    f264 = MUL_F(f262, FRAC_CONST(0.7071067811865474));
    f265 = f249 - f251;
    f266 = f249 + f251;
    f267 = MUL_F(f265, FRAC_CONST(0.7071067811865474));
    f268 = MUL_F(f266, FRAC_CONST(0.7071067811865474));
    f269 = f253 - f255;
    f270 = f253 + f255;
    f271 = MUL_F(f269, FRAC_CONST(0.7071067811865474));
    f272 = MUL_F(f270, FRAC_CONST(0.7071067811865474));
    y[31] = f98 - f242;
    y[0] = f98 + f242;
    y[30] = f100 - f250;
    y[1] = f100 + f250;
    y[29] = f102 - f254;
    y[2] = f102 + f254;
    y[28] = f104 - f246;
    y[3] = f104 + f246;
    y[27] = f106 - f264;
    y[4] = f106 + f264;
    y[26] = f108 - f272;
    y[5] = f108 + f272;
    y[25] = f110 - f268;
    y[6] = f110 + f268;
    y[24] = f112 - f260;
    y[7] = f112 + f260;
    y[23] = f111 - f259;
    y[8] = f111 + f259;
    y[22] = f109 - f267;
    y[9] = f109 + f267;
    y[21] = f107 - f271;
    y[10] = f107 + f271;
    y[20] = f105 - f263;
    y[11] = f105 + f263;
    y[19] = f103 - f248;
    y[12] = f103 + f248;
    y[18] = f101 - f256;
    y[13] = f101 + f256;
    y[17] = f99 - f252;
    y[14] = f99 + f252;
    y[16] = f97 - f244;
    y[15] = f97 + f244;
}

void DCT2_32_unscaled(real_t *y, real_t *x)
{
    real_t f0, f1, f2, f3, f4, f5, f6, f7, f8, f9, f10;
    real_t f11, f12, f13, f14, f15, f16, f17, f18, f19, f20;
    real_t f21, f22, f23, f24, f25, f26, f27, f28, f29, f30;
    real_t f31, f32, f33, f34, f35, f36, f37, f38, f39, f40;
    real_t f41, f42, f43, f44, f45, f46, f47, f48, f49, f50;
    real_t f51, f52, f53, f54, f55, f56, f57, f58, f59, f60;
    real_t f63, f64, f65, f66, f69, f70, f71, f72, f73, f74;
    real_t f75, f76, f77, f78, f79, f80, f81, f83, f85, f86;
    real_t f89, f90, f91, f92, f93, f94, f95, f96, f97, f98;
    real_t f99, f100, f101, f102, f103, f104, f105, f106, f107, f108;
    real_t f109, f110, f111, f112, f113, f114, f115, f116, f117, f118;
    real_t f119, f120, f121, f122, f123, f124, f127, f128, f129, f130;
    real_t f133, f134, f135, f136, f139, f140, f141, f142, f145, f146;
    real_t f147, f148, f149, f150, f151, f152, f153, f154, f155, f156;
    real_t f157, f158, f159, f160, f161, f162, f163, f164, f165, f166;
    real_t f167, f168, f169, f170, f171, f172, f173, f174, f175, f176;
    real_t f177, f178, f179, f180, f181, f182, f183, f184, f185, f186;
    real_t f187, f188, f189, f190, f191, f192, f193, f194, f195, f196;
    real_t f197, f198, f199, f200, f201, f202, f203, f204, f205, f206;
    real_t f207, f208, f209, f210, f211, f212, f213, f214, f215, f216;
    real_t f217, f218, f219, f220, f221, f222, f223, f224, f225, f226;
    real_t f227, f228, f229, f230, f231, f232, f233, f234, f235, f236;
    real_t f237, f238, f239, f240, f241, f242, f243, f244, f247, f248;
    real_t f249, f250, f253, f254, f255, f256, f259, f260, f261, f262;
    real_t f265, f266, f267, f268, f271, f272, f273, f274, f277, f278;
    real_t f279, f280, f283, f284, f285, f286;

    f0 = x[0] - x[31];
    f1 = x[0] + x[31];
    f2 = x[1] - x[30];
    f3 = x[1] + x[30];
    f4 = x[2] - x[29];
    f5 = x[2] + x[29];
    f6 = x[3] - x[28];
    f7 = x[3] + x[28];
    f8 = x[4] - x[27];
    f9 = x[4] + x[27];
    f10 = x[5] - x[26];
    f11 = x[5] + x[26];
    f12 = x[6] - x[25];
    f13 = x[6] + x[25];
    f14 = x[7] - x[24];
    f15 = x[7] + x[24];
    f16 = x[8] - x[23];
    f17 = x[8] + x[23];
    f18 = x[9] - x[22];
    f19 = x[9] + x[22];
    f20 = x[10] - x[21];
    f21 = x[10] + x[21];
    f22 = x[11] - x[20];
    f23 = x[11] + x[20];
    f24 = x[12] - x[19];
    f25 = x[12] + x[19];
    f26 = x[13] - x[18];
    f27 = x[13] + x[18];
    f28 = x[14] - x[17];
    f29 = x[14] + x[17];
    f30 = x[15] - x[16];
    f31 = x[15] + x[16];
    f32 = f1 - f31;
    f33 = f1 + f31;
    f34 = f3 - f29;
    f35 = f3 + f29;
    f36 = f5 - f27;
    f37 = f5 + f27;
    f38 = f7 - f25;
    f39 = f7 + f25;
    f40 = f9 - f23;
    f41 = f9 + f23;
    f42 = f11 - f21;
    f43 = f11 + f21;
    f44 = f13 - f19;
    f45 = f13 + f19;
    f46 = f15 - f17;
    f47 = f15 + f17;
    f48 = f33 - f47;
    f49 = f33 + f47;
    f50 = f35 - f45;
    f51 = f35 + f45;
    f52 = f37 - f43;
    f53 = f37 + f43;
    f54 = f39 - f41;
    f55 = f39 + f41;
    f56 = f49 - f55;
    f57 = f49 + f55;
    f58 = f51 - f53;
    f59 = f51 + f53;
    f60 = f57 - f59;
    y[0] = f57 + f59;
    y[16] = MUL_F(FRAC_CONST(0.7071067811865476), f60);
    f63 = f56 + f58;
    f64 = MUL_C(COEF_CONST(1.3065629648763766), f56);
    f65 = MUL_F(FRAC_CONST(-0.9238795325112866), f63);
    f66 = MUL_F(FRAC_CONST(-0.5411961001461967), f58);
    y[24] = f64 + f65;
    y[8] = f66 - f65;
    f69 = f48 + f54;
    f70 = MUL_C(COEF_CONST(1.1758756024193588), f48);
    f71 = MUL_F(FRAC_CONST(-0.9807852804032304), f69);
    f72 = MUL_F(FRAC_CONST(-0.7856949583871021), f54);
    f73 = f70 + f71;
    f74 = f72 - f71;
    f75 = f50 + f52;
    f76 = MUL_C(COEF_CONST(1.3870398453221473), f50);
    f77 = MUL_F(FRAC_CONST(-0.8314696123025455), f75);
    f78 = MUL_F(FRAC_CONST(-0.2758993792829436), f52);
    f79 = f76 + f77;
    f80 = f78 - f77;
    f81 = f74 - f80;
    y[4] = f74 + f80;
    f83 = MUL_F(FRAC_CONST(0.7071067811865476), f81);
    y[28] = f73 - f79;
    f85 = f73 + f79;
    f86 = MUL_F(FRAC_CONST(0.7071067811865476), f85);
    y[20] = f83 - f86;
    y[12] = f83 + f86;
    f89 = f34 - f36;
    f90 = f34 + f36;
    f91 = f38 - f40;
    f92 = f38 + f40;
    f93 = f42 - f44;
    f94 = f42 + f44;
    f95 = MUL_F(FRAC_CONST(0.7071067811865476), f92);
    f96 = f32 - f95;
    f97 = f32 + f95;
    f98 = f90 + f94;
    f99 = MUL_C(COEF_CONST(1.3065629648763766), f90);
    f100 = MUL_F(FRAC_CONST(-0.9238795325112866), f98);
    f101 = MUL_F(FRAC_CONST(-0.5411961001461967), f94);
    f102 = f99 + f100;
    f103 = f101 - f100;
    f104 = f97 - f103;
    f105 = f97 + f103;
    f106 = f96 - f102;
    f107 = f96 + f102;
    f108 = MUL_F(FRAC_CONST(0.7071067811865476), f91);
    f109 = f46 - f108;
    f110 = f46 + f108;
    f111 = f93 + f89;
    f112 = MUL_C(COEF_CONST(1.3065629648763766), f93);
    f113 = MUL_F(FRAC_CONST(-0.9238795325112866), f111);
    f114 = MUL_F(FRAC_CONST(-0.5411961001461967), f89);
    f115 = f112 + f113;
    f116 = f114 - f113;
    f117 = f110 - f116;
    f118 = f110 + f116;
    f119 = f109 - f115;
    f120 = f109 + f115;
    f121 = f118 + f105;
    f122 = MUL_F(FRAC_CONST(-0.8971675863426361), f118);
    f123 = MUL_F(FRAC_CONST(0.9951847266721968), f121);
    f124 = MUL_C(COEF_CONST(1.0932018670017576), f105);
    y[2] = f122 + f123;
    y[30] = f124 - f123;
    f127 = f107 - f120;
    f128 = MUL_F(FRAC_CONST(-0.6666556584777466), f120);
    f129 = MUL_F(FRAC_CONST(0.9569403357322089), f127);
    f130 = MUL_C(COEF_CONST(1.2472250129866713), f107);
    y[6] = f129 - f128;
    y[26] = f130 - f129;
    f133 = f119 + f106;
    f134 = MUL_F(FRAC_CONST(-0.4105245275223571), f119);
    f135 = MUL_F(FRAC_CONST(0.8819212643483549), f133);
    f136 = MUL_C(COEF_CONST(1.3533180011743529), f106);
    y[10] = f134 + f135;
    y[22] = f136 - f135;
    f139 = f104 - f117;
    f140 = MUL_F(FRAC_CONST(-0.1386171691990915), f117);
    f141 = MUL_F(FRAC_CONST(0.7730104533627370), f139);
    f142 = MUL_C(COEF_CONST(1.4074037375263826), f104);
    y[14] = f141 - f140;
    y[18] = f142 - f141;
    f145 = f2 - f4;
    f146 = f2 + f4;
    f147 = f6 - f8;
    f148 = f6 + f8;
    f149 = f10 - f12;
    f150 = f10 + f12;
    f151 = f14 - f16;
    f152 = f14 + f16;
    f153 = f18 - f20;
    f154 = f18 + f20;
    f155 = f22 - f24;
    f156 = f22 + f24;
    f157 = f26 - f28;
    f158 = f26 + f28;
    f159 = MUL_F(FRAC_CONST(0.7071067811865476), f152);
    f160 = f0 - f159;
    f161 = f0 + f159;
    f162 = f148 + f156;
    f163 = MUL_C(COEF_CONST(1.3065629648763766), f148);
    f164 = MUL_F(FRAC_CONST(-0.9238795325112866), f162);
    f165 = MUL_F(FRAC_CONST(-0.5411961001461967), f156);
    f166 = f163 + f164;
    f167 = f165 - f164;
    f168 = f161 - f167;
    f169 = f161 + f167;
    f170 = f160 - f166;
    f171 = f160 + f166;
    f172 = f146 + f158;
    f173 = MUL_C(COEF_CONST(1.1758756024193588), f146);
    f174 = MUL_F(FRAC_CONST(-0.9807852804032304), f172);
    f175 = MUL_F(FRAC_CONST(-0.7856949583871021), f158);
    f176 = f173 + f174;
    f177 = f175 - f174;
    f178 = f150 + f154;
    f179 = MUL_C(COEF_CONST(1.3870398453221473), f150);
    f180 = MUL_F(FRAC_CONST(-0.8314696123025455), f178);
    f181 = MUL_F(FRAC_CONST(-0.2758993792829436), f154);
    f182 = f179 + f180;
    f183 = f181 - f180;
    f184 = f177 - f183;
    f185 = f177 + f183;
    f186 = MUL_F(FRAC_CONST(0.7071067811865476), f184);
    f187 = f176 - f182;
    f188 = f176 + f182;
    f189 = MUL_F(FRAC_CONST(0.7071067811865476), f188);
    f190 = f186 - f189;
    f191 = f186 + f189;
    f192 = f169 - f185;
    f193 = f169 + f185;
    f194 = f171 - f191;
    f195 = f171 + f191;
    f196 = f170 - f190;
    f197 = f170 + f190;
    f198 = f168 - f187;
    f199 = f168 + f187;
    f200 = MUL_F(FRAC_CONST(0.7071067811865476), f151);
    f201 = f30 - f200;
    f202 = f30 + f200;
    f203 = f155 + f147;
    f204 = MUL_C(COEF_CONST(1.3065629648763766), f155);
    f205 = MUL_F(FRAC_CONST(-0.9238795325112866), f203);
    f206 = MUL_F(FRAC_CONST(-0.5411961001461967), f147);
    f207 = f204 + f205;
    f208 = f206 - f205;
    f209 = f202 - f208;
    f210 = f202 + f208;
    f211 = f201 - f207;
    f212 = f201 + f207;
    f213 = f157 + f145;
    f214 = MUL_C(COEF_CONST(1.1758756024193588), f157);
    f215 = MUL_F(FRAC_CONST(-0.9807852804032304), f213);
    f216 = MUL_F(FRAC_CONST(-0.7856949583871021), f145);
    f217 = f214 + f215;
    f218 = f216 - f215;
    f219 = f153 + f149;
    f220 = MUL_C(COEF_CONST(1.3870398453221473), f153);
    f221 = MUL_F(FRAC_CONST(-0.8314696123025455), f219);
    f222 = MUL_F(FRAC_CONST(-0.2758993792829436), f149);
    f223 = f220 + f221;
    f224 = f222 - f221;
    f225 = f218 - f224;
    f226 = f218 + f224;
    f227 = MUL_F(FRAC_CONST(0.7071067811865476), f225);
    f228 = f217 - f223;
    f229 = f217 + f223;
    f230 = MUL_F(FRAC_CONST(0.7071067811865476), f229);
    f231 = f227 - f230;
    f232 = f227 + f230;
    f233 = f210 - f226;
    f234 = f210 + f226;
    f235 = f212 - f232;
    f236 = f212 + f232;
    f237 = f211 - f231;
    f238 = f211 + f231;
    f239 = f209 - f228;
    f240 = f209 + f228;
    f241 = f234 + f193;
    f242 = MUL_F(FRAC_CONST(-0.9497277818777543), f234);
    f243 = MUL_F(FRAC_CONST(0.9987954562051724), f241);
    f244 = MUL_C(COEF_CONST(1.0478631305325905), f193);
    y[1] = f242 + f243;
    y[31] = f244 - f243;
    f247 = f195 - f236;
    f248 = MUL_F(FRAC_CONST(-0.8424460355094192), f236);
    f249 = MUL_F(FRAC_CONST(0.9891765099647810), f247);
    f250 = MUL_C(COEF_CONST(1.1359069844201428), f195);
    y[3] = f249 - f248;
    y[29] = f250 - f249;
    f253 = f238 + f197;
    f254 = MUL_F(FRAC_CONST(-0.7270510732912801), f238);
    f255 = MUL_F(FRAC_CONST(0.9700312531945440), f253);
    f256 = MUL_C(COEF_CONST(1.2130114330978079), f197);
    y[5] = f254 + f255;
    y[27] = f256 - f255;
    f259 = f199 - f240;
    f260 = MUL_F(FRAC_CONST(-0.6046542117908007), f240);
    f261 = MUL_F(FRAC_CONST(0.9415440651830208), f259);
    f262 = MUL_C(COEF_CONST(1.2784339185752409), f199);
    y[7] = f261 - f260;
    y[25] = f262 - f261;
    f265 = f239 + f198;
    f266 = MUL_F(FRAC_CONST(-0.4764341996931611), f239);
    f267 = MUL_F(FRAC_CONST(0.9039892931234433), f265);
    f268 = MUL_C(COEF_CONST(1.3315443865537255), f198);
    y[9] = f266 + f267;
    y[23] = f268 - f267;
    f271 = f196 - f237;
    f272 = MUL_F(FRAC_CONST(-0.3436258658070505), f237);
    f273 = MUL_F(FRAC_CONST(0.8577286100002721), f271);
    f274 = MUL_C(COEF_CONST(1.3718313541934939), f196);
    y[11] = f273 - f272;
    y[21] = f274 - f273;
    f277 = f235 + f194;
    f278 = MUL_F(FRAC_CONST(-0.2075082269882114), f235);
    f279 = MUL_F(FRAC_CONST(0.8032075314806448), f277);
    f280 = MUL_C(COEF_CONST(1.3989068359730783), f194);
    y[13] = f278 + f279;
    y[19] = f280 - f279;
    f283 = f192 - f233;
    f284 = MUL_F(FRAC_CONST(-0.0693921705079408), f233);
    f285 = MUL_F(FRAC_CONST(0.7409511253549591), f283);
    f286 = MUL_C(COEF_CONST(1.4125100802019774), f192);
    y[15] = f285 - f284;
    y[17] = f286 - f285;
}

#else


#define n 32
#define log2n 5

// w_array_real[i] = cos(2*M_PI*i/32)
static const real_t w_array_real[] = {
    FRAC_CONST(1.000000000000000), FRAC_CONST(0.980785279337272),
    FRAC_CONST(0.923879528329380), FRAC_CONST(0.831469603195765),
    FRAC_CONST(0.707106765732237), FRAC_CONST(0.555570210304169),
    FRAC_CONST(0.382683402077046), FRAC_CONST(0.195090284503576),
    FRAC_CONST(0.000000000000000), FRAC_CONST(-0.195090370246552),
    FRAC_CONST(-0.382683482845162), FRAC_CONST(-0.555570282993553),
    FRAC_CONST(-0.707106827549476), FRAC_CONST(-0.831469651765257),
    FRAC_CONST(-0.923879561784627), FRAC_CONST(-0.980785296392607)
};

// w_array_imag[i] = sin(-2*M_PI*i/32)
static const real_t w_array_imag[] = {
    FRAC_CONST(0.000000000000000), FRAC_CONST(-0.195090327375064),
    FRAC_CONST(-0.382683442461104), FRAC_CONST(-0.555570246648862),
    FRAC_CONST(-0.707106796640858), FRAC_CONST(-0.831469627480512),
    FRAC_CONST(-0.923879545057005), FRAC_CONST(-0.980785287864940),
    FRAC_CONST(-1.000000000000000), FRAC_CONST(-0.980785270809601),
    FRAC_CONST(-0.923879511601754), FRAC_CONST(-0.831469578911016),
    FRAC_CONST(-0.707106734823616), FRAC_CONST(-0.555570173959476),
    FRAC_CONST(-0.382683361692986), FRAC_CONST(-0.195090241632088)
};

// FFT decimation in frequency
// 4*16*2+16=128+16=144 multiplications
// 6*16*2+10*8+4*16*2=192+80+128=400 additions
static void fft_dif(real_t * Real, real_t * Imag)
{
    real_t w_real, w_imag; // For faster access
    real_t point1_real, point1_imag, point2_real, point2_imag; // For faster access
    uint32_t j, i, i2, w_index; // Counters

    // First 2 stages of 32 point FFT decimation in frequency
    // 4*16*2=64*2=128 multiplications
    // 6*16*2=96*2=192 additions
	// Stage 1 of 32 point FFT decimation in frequency
    for (i = 0; i < 16; i++)
    {
        point1_real = Real[i];
        point1_imag = Imag[i];
        i2 = i+16;
        point2_real = Real[i2];
        point2_imag = Imag[i2];

        w_real = w_array_real[i];
        w_imag = w_array_imag[i];

        // temp1 = x[i] - x[i2]
        point1_real -= point2_real;
        point1_imag -= point2_imag;

        // x[i1] = x[i] + x[i2]
        Real[i] += point2_real;
        Imag[i] += point2_imag;

        // x[i2] = (x[i] - x[i2]) * w
        Real[i2] = (MUL_F(point1_real,w_real) - MUL_F(point1_imag,w_imag));
        Imag[i2] = (MUL_F(point1_real,w_imag) + MUL_F(point1_imag,w_real));
     }
    // Stage 2 of 32 point FFT decimation in frequency
    for (j = 0, w_index = 0; j < 8; j++, w_index += 2)
    {
        w_real = w_array_real[w_index];
        w_imag = w_array_imag[w_index];

    	i = j;
        point1_real = Real[i];
        point1_imag = Imag[i];
        i2 = i+8;
        point2_real = Real[i2];
        point2_imag = Imag[i2];

        // temp1 = x[i] - x[i2]
        point1_real -= point2_real;
        point1_imag -= point2_imag;

        // x[i1] = x[i] + x[i2]
        Real[i] += point2_real;
        Imag[i] += point2_imag;

        // x[i2] = (x[i] - x[i2]) * w
        Real[i2] = (MUL_F(point1_real,w_real) - MUL_F(point1_imag,w_imag));
        Imag[i2] = (MUL_F(point1_real,w_imag) + MUL_F(point1_imag,w_real));

        i = j+16;
        point1_real = Real[i];
        point1_imag = Imag[i];
        i2 = i+8;
        point2_real = Real[i2];
        point2_imag = Imag[i2];

        // temp1 = x[i] - x[i2]
        point1_real -= point2_real;
        point1_imag -= point2_imag;

        // x[i1] = x[i] + x[i2]
        Real[i] += point2_real;
        Imag[i] += point2_imag;

        // x[i2] = (x[i] - x[i2]) * w
        Real[i2] = (MUL_F(point1_real,w_real) - MUL_F(point1_imag,w_imag));
        Imag[i2] = (MUL_F(point1_real,w_imag) + MUL_F(point1_imag,w_real));
    }

    // Stage 3 of 32 point FFT decimation in frequency
    // 2*4*2=16 multiplications
    // 4*4*2+6*4*2=10*8=80 additions
    for (i = 0; i < n; i += 8)
    {
        i2 = i+4;
        point1_real = Real[i];
        point1_imag = Imag[i];

        point2_real = Real[i2];
        point2_imag = Imag[i2];

        // out[i1] = point1 + point2
        Real[i] += point2_real;
        Imag[i] += point2_imag;

        // out[i2] = point1 - point2
        Real[i2] = point1_real - point2_real;
        Imag[i2] = point1_imag - point2_imag;
    }
    w_real = w_array_real[4]; // = sqrt(2)/2
    // w_imag = -w_real; // = w_array_imag[4]; // = -sqrt(2)/2
    for (i = 1; i < n; i += 8)
    {
        i2 = i+4;
        point1_real = Real[i];
        point1_imag = Imag[i];

        point2_real = Real[i2];
        point2_imag = Imag[i2];

        // temp1 = x[i] - x[i2]
        point1_real -= point2_real;
        point1_imag -= point2_imag;

        // x[i1] = x[i] + x[i2]
        Real[i] += point2_real;
        Imag[i] += point2_imag;

        // x[i2] = (x[i] - x[i2]) * w
        Real[i2] = MUL_F(point1_real+point1_imag, w_real);
        Imag[i2] = MUL_F(point1_imag-point1_real, w_real);
    }
    for (i = 2; i < n; i += 8)
    {
        i2 = i+4;
        point1_real = Real[i];
        point1_imag = Imag[i];

        point2_real = Real[i2];
        point2_imag = Imag[i2];

        // x[i] = x[i] + x[i2]
        Real[i] += point2_real;
        Imag[i] += point2_imag;

        // x[i2] = (x[i] - x[i2]) * (-i)
        Real[i2] = point1_imag - point2_imag;
        Imag[i2] = point2_real - point1_real;
    }
    w_real = w_array_real[12]; // = -sqrt(2)/2
    // w_imag = w_real; // = w_array_imag[12]; // = -sqrt(2)/2
    for (i = 3; i < n; i += 8)
    {
        i2 = i+4;
        point1_real = Real[i];
        point1_imag = Imag[i];

        point2_real = Real[i2];
        point2_imag = Imag[i2];

        // temp1 = x[i] - x[i2]
        point1_real -= point2_real;
        point1_imag -= point2_imag;

        // x[i1] = x[i] + x[i2]
        Real[i] += point2_real;
        Imag[i] += point2_imag;

        // x[i2] = (x[i] - x[i2]) * w
        Real[i2] = MUL_F(point1_real-point1_imag, w_real);
        Imag[i2] = MUL_F(point1_real+point1_imag, w_real);
    }


    // Stage 4 of 32 point FFT decimation in frequency (no multiplications)
    // 16*4=64 additions
    for (i = 0; i < n; i += 4)
    {
        i2 = i+2;
        point1_real = Real[i];
        point1_imag = Imag[i];

        point2_real = Real[i2];
        point2_imag = Imag[i2];

        // x[i1] = x[i] + x[i2]
        Real[i] += point2_real;
        Imag[i] += point2_imag;

        // x[i2] = x[i] - x[i2]
        Real[i2] = point1_real - point2_real;
        Imag[i2] = point1_imag - point2_imag;
    }
    for (i = 1; i < n; i += 4)
    {
        i2 = i+2;
        point1_real = Real[i];
        point1_imag = Imag[i];

        point2_real = Real[i2];
        point2_imag = Imag[i2];

        // x[i] = x[i] + x[i2]
        Real[i] += point2_real;
        Imag[i] += point2_imag;

        // x[i2] = (x[i] - x[i2]) * (-i)
        Real[i2] = point1_imag - point2_imag;
        Imag[i2] = point2_real - point1_real;
    }

    // Stage 5 of 32 point FFT decimation in frequency (no multiplications)
    // 16*4=64 additions
    for (i = 0; i < n; i += 2)
    {
        i2 = i+1;
        point1_real = Real[i];
        point1_imag = Imag[i];

        point2_real = Real[i2];
        point2_imag = Imag[i2];

        // out[i1] = point1 + point2
        Real[i] += point2_real;
        Imag[i] += point2_imag;

        // out[i2] = point1 - point2
        Real[i2] = point1_real - point2_real;
        Imag[i2] = point1_imag - point2_imag;
    }

#ifdef REORDER_IN_FFT
    FFTReorder(Real, Imag);
#endif // #ifdef REORDER_IN_FFT
}
#undef n
#undef log2n

static const real_t dct4_64_tab[] = {
    COEF_CONST(0.999924719333649), COEF_CONST(0.998118102550507),
    COEF_CONST(0.993906974792480), COEF_CONST(0.987301409244537),
    COEF_CONST(0.978317379951477), COEF_CONST(0.966976463794708),
    COEF_CONST(0.953306019306183), COEF_CONST(0.937339007854462),
    COEF_CONST(0.919113874435425), COEF_CONST(0.898674488067627),
    COEF_CONST(0.876070082187653), COEF_CONST(0.851355195045471),
    COEF_CONST(0.824589252471924), COEF_CONST(0.795836925506592),
    COEF_CONST(0.765167236328125), COEF_CONST(0.732654273509979),
    COEF_CONST(0.698376238346100), COEF_CONST(0.662415742874146),
    COEF_CONST(0.624859452247620), COEF_CONST(0.585797846317291),
    COEF_CONST(0.545324981212616), COEF_CONST(0.503538429737091),
    COEF_CONST(0.460538715124130), COEF_CONST(0.416429549455643),
    COEF_CONST(0.371317148208618), COEF_CONST(0.325310230255127),
    COEF_CONST(0.278519600629807), COEF_CONST(0.231058135628700),
    COEF_CONST(0.183039888739586), COEF_CONST(0.134580686688423),
    COEF_CONST(0.085797272622585), COEF_CONST(0.036807164549828),
    COEF_CONST(-1.012196302413940), COEF_CONST(-1.059438824653626),
    COEF_CONST(-1.104129195213318), COEF_CONST(-1.146159529685974),
    COEF_CONST(-1.185428738594055), COEF_CONST(-1.221842169761658),
    COEF_CONST(-1.255311965942383), COEF_CONST(-1.285757660865784),
    COEF_CONST(-1.313105940818787), COEF_CONST(-1.337290763854981),
    COEF_CONST(-1.358253836631775), COEF_CONST(-1.375944852828980),
    COEF_CONST(-1.390321016311646), COEF_CONST(-1.401347875595093),
    COEF_CONST(-1.408998727798462), COEF_CONST(-1.413255214691162),
    COEF_CONST(-1.414107084274292), COEF_CONST(-1.411552190780640),
    COEF_CONST(-1.405596733093262), COEF_CONST(-1.396255016326904),
    COEF_CONST(-1.383549690246582), COEF_CONST(-1.367511272430420),
    COEF_CONST(-1.348178386688232), COEF_CONST(-1.325597524642944),
    COEF_CONST(-1.299823284149170), COEF_CONST(-1.270917654037476),
    COEF_CONST(-1.238950133323669), COEF_CONST(-1.203998088836670),
    COEF_CONST(-1.166145324707031), COEF_CONST(-1.125483393669128),
    COEF_CONST(-1.082109928131104), COEF_CONST(-1.036129593849182),
    COEF_CONST(-0.987653195858002), COEF_CONST(-0.936797380447388),
    COEF_CONST(-0.883684754371643), COEF_CONST(-0.828443288803101),
    COEF_CONST(-0.771206021308899), COEF_CONST(-0.712110757827759),
    COEF_CONST(-0.651300072669983), COEF_CONST(-0.588920354843140),
    COEF_CONST(-0.525121808052063), COEF_CONST(-0.460058242082596),
    COEF_CONST(-0.393886327743530), COEF_CONST(-0.326765477657318),
    COEF_CONST(-0.258857429027557), COEF_CONST(-0.190325915813446),
    COEF_CONST(-0.121335685253143), COEF_CONST(-0.052053272724152),
    COEF_CONST(0.017354607582092), COEF_CONST(0.086720645427704),
    COEF_CONST(0.155877828598022), COEF_CONST(0.224659323692322),
    COEF_CONST(0.292899727821350), COEF_CONST(0.360434412956238),
    COEF_CONST(0.427100926637650), COEF_CONST(0.492738455533981),
    COEF_CONST(0.557188928127289), COEF_CONST(0.620297133922577),
    COEF_CONST(0.681910991668701), COEF_CONST(0.741881847381592),
    COEF_CONST(0.800065577030182), COEF_CONST(0.856321990489960),
    COEF_CONST(0.910515367984772), COEF_CONST(0.962515234947205),
    COEF_CONST(1.000000000000000), COEF_CONST(0.998795449733734),
    COEF_CONST(0.995184719562531), COEF_CONST(0.989176511764526),
    COEF_CONST(0.980785250663757), COEF_CONST(0.970031261444092),
    COEF_CONST(0.956940352916718), COEF_CONST(0.941544055938721),
    COEF_CONST(0.923879504203796), COEF_CONST(0.903989315032959),
    COEF_CONST(0.881921231746674), COEF_CONST(0.857728600502014),
    COEF_CONST(0.831469595432281), COEF_CONST(0.803207516670227),
    COEF_CONST(0.773010432720184), COEF_CONST(0.740951120853424),
    COEF_CONST(0.707106769084930), COEF_CONST(0.671558916568756),
    COEF_CONST(0.634393274784088), COEF_CONST(0.595699310302734),
    COEF_CONST(0.555570185184479), COEF_CONST(0.514102697372437),
    COEF_CONST(0.471396654844284), COEF_CONST(0.427555114030838),
    COEF_CONST(0.382683426141739), COEF_CONST(0.336889833211899),
    COEF_CONST(0.290284633636475), COEF_CONST(0.242980122566223),
    COEF_CONST(0.195090234279633), COEF_CONST(0.146730497479439),
    COEF_CONST(0.098017133772373), COEF_CONST(0.049067649990320),
    COEF_CONST(-1.000000000000000), COEF_CONST(-1.047863125801086),
    COEF_CONST(-1.093201875686646), COEF_CONST(-1.135906934738159),
    COEF_CONST(-1.175875544548035), COEF_CONST(-1.213011503219605),
    COEF_CONST(-1.247225046157837), COEF_CONST(-1.278433918952942),
    COEF_CONST(-1.306562900543213), COEF_CONST(-1.331544399261475),
    COEF_CONST(-1.353317975997925), COEF_CONST(-1.371831417083740),
    COEF_CONST(-1.387039899826050), COEF_CONST(-1.398906826972961),
    COEF_CONST(-1.407403707504273), COEF_CONST(-1.412510156631470),
    COEF_CONST(0), COEF_CONST(-1.412510156631470),
    COEF_CONST(-1.407403707504273), COEF_CONST(-1.398906826972961),
    COEF_CONST(-1.387039899826050), COEF_CONST(-1.371831417083740),
    COEF_CONST(-1.353317975997925), COEF_CONST(-1.331544399261475),
    COEF_CONST(-1.306562900543213), COEF_CONST(-1.278433918952942),
    COEF_CONST(-1.247225046157837), COEF_CONST(-1.213011384010315),
    COEF_CONST(-1.175875544548035), COEF_CONST(-1.135907053947449),
    COEF_CONST(-1.093201875686646), COEF_CONST(-1.047863125801086),
    COEF_CONST(-1.000000000000000), COEF_CONST(-0.949727773666382),
    COEF_CONST(-0.897167563438416), COEF_CONST(-0.842446029186249),
    COEF_CONST(-0.785694956779480), COEF_CONST(-0.727051079273224),
    COEF_CONST(-0.666655659675598), COEF_CONST(-0.604654192924500),
    COEF_CONST(-0.541196048259735), COEF_CONST(-0.476434230804443),
    COEF_CONST(-0.410524487495422), COEF_CONST(-0.343625843524933),
    COEF_CONST(-0.275899350643158), COEF_CONST(-0.207508206367493),
    COEF_CONST(-0.138617098331451), COEF_CONST(-0.069392144680023),
    COEF_CONST(0), COEF_CONST(0.069392263889313),
    COEF_CONST(0.138617157936096), COEF_CONST(0.207508206367493),
    COEF_CONST(0.275899469852448), COEF_CONST(0.343625962734222),
    COEF_CONST(0.410524636507034), COEF_CONST(0.476434201002121),
    COEF_CONST(0.541196107864380), COEF_CONST(0.604654192924500),
    COEF_CONST(0.666655719280243), COEF_CONST(0.727051138877869),
    COEF_CONST(0.785695075988770), COEF_CONST(0.842446029186249),
    COEF_CONST(0.897167563438416), COEF_CONST(0.949727773666382)
};

/* size 64 only! */
void dct4_kernel(real_t * in_real, real_t * in_imag, real_t * out_real, real_t * out_imag)
{
    // Tables with bit reverse values for 5 bits, bit reverse of i at i-th position
    const uint8_t bit_rev_tab[32] = { 0,16,8,24,4,20,12,28,2,18,10,26,6,22,14,30,1,17,9,25,5,21,13,29,3,19,11,27,7,23,15,31 };
    uint16_t i, i_rev;

    /* Step 2: modulate */
    // 3*32=96 multiplications
    // 3*32=96 additions
    for (i = 0; i < 32; i++)
    {
    	real_t x_re, x_im, tmp;
    	x_re = in_real[i];
    	x_im = in_imag[i];
        tmp =        MUL_C(x_re + x_im, dct4_64_tab[i]);
        in_real[i] = MUL_C(x_im, dct4_64_tab[i + 64]) + tmp;
        in_imag[i] = MUL_C(x_re, dct4_64_tab[i + 32]) + tmp;
    }

    /* Step 3: FFT, but with output in bit reverse order */
    fft_dif(in_real, in_imag);

    /* Step 4: modulate + bitreverse reordering */
    // 3*31+2=95 multiplications
    // 3*31+2=95 additions
    for (i = 0; i < 16; i++)
    {
    	real_t x_re, x_im, tmp;
    	i_rev = bit_rev_tab[i];
    	x_re = in_real[i_rev];
    	x_im = in_imag[i_rev];

        tmp =         MUL_C(x_re + x_im, dct4_64_tab[i + 3*32]);
        out_real[i] = MUL_C(x_im, dct4_64_tab[i + 5*32]) + tmp;
        out_imag[i] = MUL_C(x_re, dct4_64_tab[i + 4*32]) + tmp;
    }
    // i = 16, i_rev = 1 = rev(16);
    out_imag[16] = MUL_C(in_imag[1] - in_real[1], dct4_64_tab[16 + 3*32]);
    out_real[16] = MUL_C(in_real[1] + in_imag[1], dct4_64_tab[16 + 3*32]);
    for (i = 17; i < 32; i++)
    {
    	real_t x_re, x_im, tmp;
    	i_rev = bit_rev_tab[i];
    	x_re = in_real[i_rev];
    	x_im = in_imag[i_rev];
        tmp =         MUL_C(x_re + x_im, dct4_64_tab[i + 3*32]);
        out_real[i] = MUL_C(x_im, dct4_64_tab[i + 5*32]) + tmp;
        out_imag[i] = MUL_C(x_re, dct4_64_tab[i + 4*32]) + tmp;
    }

}

void DST4_32(real_t *y, real_t *x)
{
    real_t f0, f1, f2, f3, f4, f5, f6, f7, f8, f9;
    real_t f10, f11, f12, f13, f14, f15, f16, f17, f18, f19;
    real_t f20, f21, f22, f23, f24, f25, f26, f27, f28, f29;
    real_t f30, f31, f32, f33, f34, f35, f36, f37, f38, f39;
    real_t f40, f41, f42, f43, f44, f45, f46, f47, f48, f49;
    real_t f50, f51, f52, f53, f54, f55, f56, f57, f58, f59;
    real_t f60, f61, f62, f63, f64, f65, f66, f67, f68, f69;
    real_t f70, f71, f72, f73, f74, f75, f76, f77, f78, f79;
    real_t f80, f81, f82, f83, f84, f85, f86, f87, f88, f89;
    real_t f90, f91, f92, f93, f94, f95, f96, f97, f98, f99;
    real_t f100, f101, f102, f103, f104, f105, f106, f107, f108, f109;
    real_t f110, f111, f112, f113, f114, f115, f116, f117, f118, f119;
    real_t f120, f121, f122, f123, f124, f125, f126, f127, f128, f129;
    real_t f130, f131, f132, f133, f134, f135, f136, f137, f138, f139;
    real_t f140, f141, f142, f143, f144, f145, f146, f147, f148, f149;
    real_t f150, f151, f152, f153, f154, f155, f156, f157, f158, f159;
    real_t f160, f161, f162, f163, f164, f165, f166, f167, f168, f169;
    real_t f170, f171, f172, f173, f174, f175, f176, f177, f178, f179;
    real_t f180, f181, f182, f183, f184, f185, f186, f187, f188, f189;
    real_t f190, f191, f192, f193, f194, f195, f196, f197, f198, f199;
    real_t f200, f201, f202, f203, f204, f205, f206, f207, f208, f209;
    real_t f210, f211, f212, f213, f214, f215, f216, f217, f218, f219;
    real_t f220, f221, f222, f223, f224, f225, f226, f227, f228, f229;
    real_t f230, f231, f232, f233, f234, f235, f236, f237, f238, f239;
    real_t f240, f241, f242, f243, f244, f245, f246, f247, f248, f249;
    real_t f250, f251, f252, f253, f254, f255, f256, f257, f258, f259;
    real_t f260, f261, f262, f263, f264, f265, f266, f267, f268, f269;
    real_t f270, f271, f272, f273, f274, f275, f276, f277, f278, f279;
    real_t f280, f281, f282, f283, f284, f285, f286, f287, f288, f289;
    real_t f290, f291, f292, f293, f294, f295, f296, f297, f298, f299;
    real_t f300, f301, f302, f303, f304, f305, f306, f307, f308, f309;
    real_t f310, f311, f312, f313, f314, f315, f316, f317, f318, f319;
    real_t f320, f321, f322, f323, f324, f325, f326, f327, f328, f329;
    real_t f330, f331, f332, f333, f334, f335;

    f0 = x[0] - x[1];
    f1 = x[2] - x[1];
    f2 = x[2] - x[3];
    f3 = x[4] - x[3];
    f4 = x[4] - x[5];
    f5 = x[6] - x[5];
    f6 = x[6] - x[7];
    f7 = x[8] - x[7];
    f8 = x[8] - x[9];
    f9 = x[10] - x[9];
    f10 = x[10] - x[11];
    f11 = x[12] - x[11];
    f12 = x[12] - x[13];
    f13 = x[14] - x[13];
    f14 = x[14] - x[15];
    f15 = x[16] - x[15];
    f16 = x[16] - x[17];
    f17 = x[18] - x[17];
    f18 = x[18] - x[19];
    f19 = x[20] - x[19];
    f20 = x[20] - x[21];
    f21 = x[22] - x[21];
    f22 = x[22] - x[23];
    f23 = x[24] - x[23];
    f24 = x[24] - x[25];
    f25 = x[26] - x[25];
    f26 = x[26] - x[27];
    f27 = x[28] - x[27];
    f28 = x[28] - x[29];
    f29 = x[30] - x[29];
    f30 = x[30] - x[31];
    f31 = MUL_F(FRAC_CONST(0.7071067811865476), f15);
    f32 = x[0] - f31;
    f33 = x[0] + f31;
    f34 = f7 + f23;
    f35 = MUL_C(COEF_CONST(1.3065629648763766), f7);
    f36 = MUL_F(FRAC_CONST(-0.9238795325112866), f34);
    f37 = MUL_F(FRAC_CONST(-0.5411961001461967), f23);
    f38 = f35 + f36;
    f39 = f37 - f36;
    f40 = f33 - f39;
    f41 = f33 + f39;
    f42 = f32 - f38;
    f43 = f32 + f38;
    f44 = f11 - f19;
    f45 = f11 + f19;
    f46 = MUL_F(FRAC_CONST(0.7071067811865476), f45);
    f47 = f3 - f46;
    f48 = f3 + f46;
    f49 = MUL_F(FRAC_CONST(0.7071067811865476), f44);
    f50 = f49 - f27;
    f51 = f49 + f27;
    f52 = f51 + f48;
    f53 = MUL_F(FRAC_CONST(-0.7856949583871021), f51);
    f54 = MUL_F(FRAC_CONST(0.9807852804032304), f52);
    f55 = MUL_C(COEF_CONST(1.1758756024193588), f48);
    f56 = f53 + f54;
    f57 = f55 - f54;
    f58 = f50 + f47;
    f59 = MUL_F(FRAC_CONST(-0.2758993792829430), f50);
    f60 = MUL_F(FRAC_CONST(0.8314696123025452), f58);
    f61 = MUL_C(COEF_CONST(1.3870398453221475), f47);
    f62 = f59 + f60;
    f63 = f61 - f60;
    f64 = f41 - f56;
    f65 = f41 + f56;
    f66 = f43 - f62;
    f67 = f43 + f62;
    f68 = f42 - f63;
    f69 = f42 + f63;
    f70 = f40 - f57;
    f71 = f40 + f57;
    f72 = f5 - f9;
    f73 = f5 + f9;
    f74 = f13 - f17;
    f75 = f13 + f17;
    f76 = f21 - f25;
    f77 = f21 + f25;
    f78 = MUL_F(FRAC_CONST(0.7071067811865476), f75);
    f79 = f1 - f78;
    f80 = f1 + f78;
    f81 = f73 + f77;
    f82 = MUL_C(COEF_CONST(1.3065629648763766), f73);
    f83 = MUL_F(FRAC_CONST(-0.9238795325112866), f81);
    f84 = MUL_F(FRAC_CONST(-0.5411961001461967), f77);
    f85 = f82 + f83;
    f86 = f84 - f83;
    f87 = f80 - f86;
    f88 = f80 + f86;
    f89 = f79 - f85;
    f90 = f79 + f85;
    f91 = MUL_F(FRAC_CONST(0.7071067811865476), f74);
    f92 = f29 - f91;
    f93 = f29 + f91;
    f94 = f76 + f72;
    f95 = MUL_C(COEF_CONST(1.3065629648763766), f76);
    f96 = MUL_F(FRAC_CONST(-0.9238795325112866), f94);
    f97 = MUL_F(FRAC_CONST(-0.5411961001461967), f72);
    f98 = f95 + f96;
    f99 = f97 - f96;
    f100 = f93 - f99;
    f101 = f93 + f99;
    f102 = f92 - f98;
    f103 = f92 + f98;
    f104 = f101 + f88;
    f105 = MUL_F(FRAC_CONST(-0.8971675863426361), f101);
    f106 = MUL_F(FRAC_CONST(0.9951847266721968), f104);
    f107 = MUL_C(COEF_CONST(1.0932018670017576), f88);
    f108 = f105 + f106;
    f109 = f107 - f106;
    f110 = f90 - f103;
    f111 = MUL_F(FRAC_CONST(-0.6666556584777466), f103);
    f112 = MUL_F(FRAC_CONST(0.9569403357322089), f110);
    f113 = MUL_C(COEF_CONST(1.2472250129866713), f90);
    f114 = f112 - f111;
    f115 = f113 - f112;
    f116 = f102 + f89;
    f117 = MUL_F(FRAC_CONST(-0.4105245275223571), f102);
    f118 = MUL_F(FRAC_CONST(0.8819212643483549), f116);
    f119 = MUL_C(COEF_CONST(1.3533180011743529), f89);
    f120 = f117 + f118;
    f121 = f119 - f118;
    f122 = f87 - f100;
    f123 = MUL_F(FRAC_CONST(-0.1386171691990915), f100);
    f124 = MUL_F(FRAC_CONST(0.7730104533627370), f122);
    f125 = MUL_C(COEF_CONST(1.4074037375263826), f87);
    f126 = f124 - f123;
    f127 = f125 - f124;
    f128 = f65 - f108;
    f129 = f65 + f108;
    f130 = f67 - f114;
    f131 = f67 + f114;
    f132 = f69 - f120;
    f133 = f69 + f120;
    f134 = f71 - f126;
    f135 = f71 + f126;
    f136 = f70 - f127;
    f137 = f70 + f127;
    f138 = f68 - f121;
    f139 = f68 + f121;
    f140 = f66 - f115;
    f141 = f66 + f115;
    f142 = f64 - f109;
    f143 = f64 + f109;
    f144 = f0 + f30;
    f145 = MUL_C(COEF_CONST(1.0478631305325901), f0);
    f146 = MUL_F(FRAC_CONST(-0.9987954562051724), f144);
    f147 = MUL_F(FRAC_CONST(-0.9497277818777548), f30);
    f148 = f145 + f146;
    f149 = f147 - f146;
    f150 = f4 + f26;
    f151 = MUL_F(FRAC_CONST(1.2130114330978077), f4);
    f152 = MUL_F(FRAC_CONST(-0.9700312531945440), f150);
    f153 = MUL_F(FRAC_CONST(-0.7270510732912803), f26);
    f154 = f151 + f152;
    f155 = f153 - f152;
    f156 = f8 + f22;
    f157 = MUL_C(COEF_CONST(1.3315443865537255), f8);
    f158 = MUL_F(FRAC_CONST(-0.9039892931234433), f156);
    f159 = MUL_F(FRAC_CONST(-0.4764341996931612), f22);
    f160 = f157 + f158;
    f161 = f159 - f158;
    f162 = f12 + f18;
    f163 = MUL_C(COEF_CONST(1.3989068359730781), f12);
    f164 = MUL_F(FRAC_CONST(-0.8032075314806453), f162);
    f165 = MUL_F(FRAC_CONST(-0.2075082269882124), f18);
    f166 = f163 + f164;
    f167 = f165 - f164;
    f168 = f16 + f14;
    f169 = MUL_C(COEF_CONST(1.4125100802019777), f16);
    f170 = MUL_F(FRAC_CONST(-0.6715589548470187), f168);
    f171 = MUL_F(FRAC_CONST(0.0693921705079402), f14);
    f172 = f169 + f170;
    f173 = f171 - f170;
    f174 = f20 + f10;
    f175 = MUL_C(COEF_CONST(1.3718313541934939), f20);
    f176 = MUL_F(FRAC_CONST(-0.5141027441932219), f174);
    f177 = MUL_F(FRAC_CONST(0.3436258658070501), f10);
    f178 = f175 + f176;
    f179 = f177 - f176;
    f180 = f24 + f6;
    f181 = MUL_C(COEF_CONST(1.2784339185752409), f24);
    f182 = MUL_F(FRAC_CONST(-0.3368898533922200), f180);
    f183 = MUL_F(FRAC_CONST(0.6046542117908008), f6);
    f184 = f181 + f182;
    f185 = f183 - f182;
    f186 = f28 + f2;
    f187 = MUL_C(COEF_CONST(1.1359069844201433), f28);
    f188 = MUL_F(FRAC_CONST(-0.1467304744553624), f186);
    f189 = MUL_F(FRAC_CONST(0.8424460355094185), f2);
    f190 = f187 + f188;
    f191 = f189 - f188;
    f192 = f149 - f173;
    f193 = f149 + f173;
    f194 = f148 - f172;
    f195 = f148 + f172;
    f196 = f155 - f179;
    f197 = f155 + f179;
    f198 = f154 - f178;
    f199 = f154 + f178;
    f200 = f161 - f185;
    f201 = f161 + f185;
    f202 = f160 - f184;
    f203 = f160 + f184;
    f204 = f167 - f191;
    f205 = f167 + f191;
    f206 = f166 - f190;
    f207 = f166 + f190;
    f208 = f192 + f194;
    f209 = MUL_C(COEF_CONST(1.1758756024193588), f192);
    f210 = MUL_F(FRAC_CONST(-0.9807852804032304), f208);
    f211 = MUL_F(FRAC_CONST(-0.7856949583871021), f194);
    f212 = f209 + f210;
    f213 = f211 - f210;
    f214 = f196 + f198;
    f215 = MUL_C(COEF_CONST(1.3870398453221475), f196);
    f216 = MUL_F(FRAC_CONST(-0.5555702330196022), f214);
    f217 = MUL_F(FRAC_CONST(0.2758993792829431), f198);
    f218 = f215 + f216;
    f219 = f217 - f216;
    f220 = f200 + f202;
    f221 = MUL_F(FRAC_CONST(0.7856949583871022), f200);
    f222 = MUL_F(FRAC_CONST(0.1950903220161283), f220);
    f223 = MUL_C(COEF_CONST(1.1758756024193586), f202);
    f224 = f221 + f222;
    f225 = f223 - f222;
    f226 = f204 + f206;
    f227 = MUL_F(FRAC_CONST(-0.2758993792829430), f204);
    f228 = MUL_F(FRAC_CONST(0.8314696123025452), f226);
    f229 = MUL_C(COEF_CONST(1.3870398453221475), f206);
    f230 = f227 + f228;
    f231 = f229 - f228;
    f232 = f193 - f201;
    f233 = f193 + f201;
    f234 = f195 - f203;
    f235 = f195 + f203;
    f236 = f197 - f205;
    f237 = f197 + f205;
    f238 = f199 - f207;
    f239 = f199 + f207;
    f240 = f213 - f225;
    f241 = f213 + f225;
    f242 = f212 - f224;
    f243 = f212 + f224;
    f244 = f219 - f231;
    f245 = f219 + f231;
    f246 = f218 - f230;
    f247 = f218 + f230;
    f248 = f232 + f234;
    f249 = MUL_C(COEF_CONST(1.3065629648763766), f232);
    f250 = MUL_F(FRAC_CONST(-0.9238795325112866), f248);
    f251 = MUL_F(FRAC_CONST(-0.5411961001461967), f234);
    f252 = f249 + f250;
    f253 = f251 - f250;
    f254 = f236 + f238;
    f255 = MUL_F(FRAC_CONST(0.5411961001461969), f236);
    f256 = MUL_F(FRAC_CONST(0.3826834323650898), f254);
    f257 = MUL_C(COEF_CONST(1.3065629648763766), f238);
    f258 = f255 + f256;
    f259 = f257 - f256;
    f260 = f240 + f242;
    f261 = MUL_C(COEF_CONST(1.3065629648763766), f240);
    f262 = MUL_F(FRAC_CONST(-0.9238795325112866), f260);
    f263 = MUL_F(FRAC_CONST(-0.5411961001461967), f242);
    f264 = f261 + f262;
    f265 = f263 - f262;
    f266 = f244 + f246;
    f267 = MUL_F(FRAC_CONST(0.5411961001461969), f244);
    f268 = MUL_F(FRAC_CONST(0.3826834323650898), f266);
    f269 = MUL_C(COEF_CONST(1.3065629648763766), f246);
    f270 = f267 + f268;
    f271 = f269 - f268;
    f272 = f233 - f237;
    f273 = f233 + f237;
    f274 = f235 - f239;
    f275 = f235 + f239;
    f276 = f253 - f259;
    f277 = f253 + f259;
    f278 = f252 - f258;
    f279 = f252 + f258;
    f280 = f241 - f245;
    f281 = f241 + f245;
    f282 = f243 - f247;
    f283 = f243 + f247;
    f284 = f265 - f271;
    f285 = f265 + f271;
    f286 = f264 - f270;
    f287 = f264 + f270;
    f288 = f272 - f274;
    f289 = f272 + f274;
    f290 = MUL_F(FRAC_CONST(0.7071067811865474), f288);
    f291 = MUL_F(FRAC_CONST(0.7071067811865474), f289);
    f292 = f276 - f278;
    f293 = f276 + f278;
    f294 = MUL_F(FRAC_CONST(0.7071067811865474), f292);
    f295 = MUL_F(FRAC_CONST(0.7071067811865474), f293);
    f296 = f280 - f282;
    f297 = f280 + f282;
    f298 = MUL_F(FRAC_CONST(0.7071067811865474), f296);
    f299 = MUL_F(FRAC_CONST(0.7071067811865474), f297);
    f300 = f284 - f286;
    f301 = f284 + f286;
    f302 = MUL_F(FRAC_CONST(0.7071067811865474), f300);
    f303 = MUL_F(FRAC_CONST(0.7071067811865474), f301);
    f304 = f129 - f273;
    f305 = f129 + f273;
    f306 = f131 - f281;
    f307 = f131 + f281;
    f308 = f133 - f285;
    f309 = f133 + f285;
    f310 = f135 - f277;
    f311 = f135 + f277;
    f312 = f137 - f295;
    f313 = f137 + f295;
    f314 = f139 - f303;
    f315 = f139 + f303;
    f316 = f141 - f299;
    f317 = f141 + f299;
    f318 = f143 - f291;
    f319 = f143 + f291;
    f320 = f142 - f290;
    f321 = f142 + f290;
    f322 = f140 - f298;
    f323 = f140 + f298;
    f324 = f138 - f302;
    f325 = f138 + f302;
    f326 = f136 - f294;
    f327 = f136 + f294;
    f328 = f134 - f279;
    f329 = f134 + f279;
    f330 = f132 - f287;
    f331 = f132 + f287;
    f332 = f130 - f283;
    f333 = f130 + f283;
    f334 = f128 - f275;
    f335 = f128 + f275;
    y[31] = MUL_F(FRAC_CONST(0.5001506360206510), f305);
    y[30] = MUL_F(FRAC_CONST(0.5013584524464084), f307);
    y[29] = MUL_F(FRAC_CONST(0.5037887256810443), f309);
    y[28] = MUL_F(FRAC_CONST(0.5074711720725553), f311);
    y[27] = MUL_F(FRAC_CONST(0.5124514794082247), f313);
    y[26] = MUL_F(FRAC_CONST(0.5187927131053328), f315);
    y[25] = MUL_F(FRAC_CONST(0.5265773151542700), f317);
    y[24] = MUL_F(FRAC_CONST(0.5359098169079920), f319);
    y[23] = MUL_F(FRAC_CONST(0.5469204379855088), f321);
    y[22] = MUL_F(FRAC_CONST(0.5597698129470802), f323);
    y[21] = MUL_F(FRAC_CONST(0.5746551840326600), f325);
    y[20] = MUL_F(FRAC_CONST(0.5918185358574165), f327);
    y[19] = MUL_F(FRAC_CONST(0.6115573478825099), f329);
    y[18] = MUL_F(FRAC_CONST(0.6342389366884031), f331);
    y[17] = MUL_F(FRAC_CONST(0.6603198078137061), f333);
    y[16] = MUL_F(FRAC_CONST(0.6903721282002123), f335);
    y[15] = MUL_F(FRAC_CONST(0.7251205223771985), f334);
    y[14] = MUL_F(FRAC_CONST(0.7654941649730891), f332);
    y[13] = MUL_F(FRAC_CONST(0.8127020908144905), f330);
    y[12] = MUL_F(FRAC_CONST(0.8683447152233481), f328);
    y[11] = MUL_F(FRAC_CONST(0.9345835970364075), f326);
    y[10] = MUL_C(COEF_CONST(1.0144082649970547), f324);
    y[9] = MUL_C(COEF_CONST(1.1120716205797176), f322);
    y[8] = MUL_C(COEF_CONST(1.2338327379765710), f320);
    y[7] = MUL_C(COEF_CONST(1.3892939586328277), f318);
    y[6] = MUL_C(COEF_CONST(1.5939722833856311), f316);
    y[5] = MUL_C(COEF_CONST(1.8746759800084078), f314);
    y[4] = MUL_C(COEF_CONST(2.2820500680051619), f312);
    y[3] = MUL_C(COEF_CONST(2.9246284281582162), f310);
    y[2] = MUL_C(COEF_CONST(4.0846110781292477), f308);
    y[1] = MUL_C(COEF_CONST(6.7967507116736332), f306);
    y[0] = MUL_R(REAL_CONST(20.3738781672314530), f304);
}

#endif

#endif
