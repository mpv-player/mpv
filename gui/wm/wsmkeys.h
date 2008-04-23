/*
 * This file is part of MPlayer.
 *
 * MPlayer is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * MPlayer is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with MPlayer; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#ifndef MPLAYER_GUI_WSMKEYS_H
#define MPLAYER_GUI_WSMKEYS_H

#include "wskeys.h"

const TwsKeyNames wsKeyNames[ wsKeyNumber ] =
 {
  { wsq, "q" }, { wsa, "a" }, { wsz, "z" }, { wsw, "w" }, { wss, "s" }, { wsx, "x" },
  { wse, "e" }, { wsd, "d" }, { wsr, "r" }, { wsf, "f" }, { wsv, "v" }, { wst, "t" },
  { wsg, "g" }, { wsb, "b" }, { wsy, "y" }, { wsh, "h" }, { wsn, "n" }, { wsu, "u" },
  { wsj, "j" }, { wsm, "m" }, { wsi, "i" }, { wsk, "k" }, { wso, "o" }, { wsl, "l" },
  { wsp, "p" }, { wsc, "c" },

  { wsQ, "Q" }, { wsA, "A" }, { wsZ, "Z" }, { wsW, "W" }, { wsS, "S" }, { wsX, "X" },
  { wsE, "E" }, { wsD, "D" }, { wsR, "R" }, { wsF, "F" }, { wsV, "V" }, { wsT, "T" },
  { wsG, "G" }, { wsB, "B" }, { wsY, "Y" }, { wsH, "H" }, { wsN, "N" }, { wsU, "U" },
  { wsJ, "J" }, { wsM, "M" }, { wsI, "I" }, { wsK, "K" }, { wsO, "O" }, { wsL, "L" },
  { wsP, "P" }, { wsC, "C" },

  { wsUp,         "Up"    }, { wsDown,       "Down"  }, { wsLeft,        "Left"  },
  { wsRight,      "Right" }, { wsPageUp,    "PageUp" }, { wsPageDown, "PageDown" },

  { wsLeftCtrl,   "LeftCtrl" }, { wsRightCtrl,  "RightCtrl" }, { wsLeftAlt,    "LeftAlt"    },
  { wsRightAlt,   "RightAlt" }, { wsLeftShift,  "LeftShift" }, { wsRightShift, "RightShift" },


  { wsBackSpace,  "BackSpace" },
  { wsCapsLock,   "CapsLock" },
  { wsNumLock,    "NumLock" },

  { wsF1, "F1" }, { wsF2, "F2" }, { wsF3, "F3" }, { wsF4, "F4" }, { wsF5,   "F5" },
  { wsF6, "F6" }, { wsF7, "F7" }, { wsF8, "F8" }, { wsF9, "F9" }, { wsF10, "F10" },
  { wsF11, "F11" }, { wsF12, "F12" },

  { wsEnter,         "Enter" }, { wsTab,             "Tab" }, { wsSpace,         "Space" },
  { wsInsert,       "Insert" }, { wsDelete,       "Delete" }, { wsHome,           "Home" },
  { wsEnd,             "End" }, { wsEscape,       "Escape" },

  { wsosbrackets, "[" }, { wscsbrackets, "]" },
  { wsMore,       "<" }, { wsLess,       ">" },
  { wsMinus,	  "-" }, { wsPlus,  	 "+" },
  { wsMul,	  "*" }, { wsDiv,	 "/" },

  { ws0, "0" }, { ws1, "1" }, { ws2, "2" }, { ws3, "3" }, { ws4, "4" },
  { ws5, "5" }, { ws6, "6" }, { ws7, "7" }, { ws8, "8" }, { ws9, "9" },

  { wsGrayEnter,       "GrayEnter" }, { wsGrayPlus,         "GrayPlus" },
  { wsGrayMinus,       "GrayMinus" }, { wsGrayMul,           "GrayMul" },
  { wsGrayDiv,           "GrayDiv" }, { wsGrayInsert,     "GrayInsert" },
  { wsGrayDelete,     "GrayDelete" }, { wsGrayEnd,           "GrayEnd" },
  { wsGrayDown,         "GrayDown" }, { wsGrayPageDown, "GrayPageDown" },
  { wsGrayLeft,         "GrayLeft" }, { wsGray5,               "Gray5" },
  { wsGrayRight,       "GrayRight" }, { wsGrayHome,         "GrayHome" },
  { wsGrayUp,             "GrayUp" }, { wsGrayPageUp,     "GrayPageUp" },

  { wsXF86LowerVolume, "XF86LowerVolume" },
  { wsXF86RaiseVolume, "XF86RaiseVolume" },
  { wsXF86Mute,               "XF86Mute" },
  { wsXF86Play,               "XF86Play" },
  { wsXF86Stop,               "XF86Stop" },
  { wsXF86Prev,               "XF86Prev" },
  { wsXF86Next,               "XF86Next" },
  { wsXF86Media,             "XF86Media" },

  { wsKeyNone, "None" }
 };

#endif /* MPLAYER_GUI_WSMKEYS_H */
