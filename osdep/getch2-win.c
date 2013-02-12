/* Windows TermIO
 *
 * copyright (C) 2003 Sascha Sommer
 *
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

// See  http://msdn.microsoft.com/library/default.asp?url=/library/en-us/winui/WinUI/WindowsUserInterface/UserInput/VirtualKeyCodes.asp
// for additional virtual keycodes


#include "config.h"
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <windows.h>
#include "core/input/keycodes.h"
#include "core/input/input.h"
#include "core/mp_fifo.h"
#include "getch2.h"

int mp_input_slave_cmd_func(int fd,char* dest,int size){
  DWORD retval;
  HANDLE in = GetStdHandle(STD_INPUT_HANDLE);
  if(PeekNamedPipe(in, NULL, size, &retval, NULL, NULL)){
    if (size > retval) size = retval;
  } else {
    if (WaitForSingleObject(in, 0))
      size = 0;
  }
  if(!size){
	  return MP_INPUT_NOTHING;
  }
  ReadFile(in, dest, size, &retval, NULL);
  if(retval)return retval;
  return MP_INPUT_NOTHING;
}

int screen_width=80;
int screen_height=24;
char * erase_to_end_of_line = NULL;

void get_screen_size(void){
    CONSOLE_SCREEN_BUFFER_INFO cinfo;
    if (GetConsoleScreenBufferInfo(GetStdHandle(STD_OUTPUT_HANDLE), &cinfo))
    {
        screen_width = cinfo.dwMaximumWindowSize.X;
        screen_height = cinfo.dwMaximumWindowSize.Y;
    }
}

static HANDLE in;
static int getch2_status=0;

static int getch2_internal(void)
{
	INPUT_RECORD eventbuffer[128];
    DWORD retval;
   	int i=0;
    if(!getch2_status){
      // supports e.g. MinGW xterm, unfortunately keys are only received after
      // enter was pressed.
      uint8_t c;
      if (!PeekNamedPipe(in, NULL, 1, &retval, NULL, NULL) || !retval)
        return -1;
      ReadFile(in, &c, 1, &retval, NULL);
      return retval == 1 ? c : -1;
    }
    /*check if there are input events*/
	if(!GetNumberOfConsoleInputEvents(in,&retval))
	{
		printf("getch2: can't get number of input events: %i\n",(int)GetLastError());
		return -1;
	}
    if(retval<=0)return -1;

	/*read all events*/
	if(!ReadConsoleInput(in,eventbuffer,128,&retval))
	{
		printf("getch: can't read input events\n");
		return -1;
	}

	/*filter out keyevents*/
    for (i = 0; i < retval; i++)
    {
		switch(eventbuffer[i].EventType)
		{
			case KEY_EVENT:
				/*only a pressed key is interresting for us*/
				if(eventbuffer[i].Event.KeyEvent.bKeyDown == TRUE)
				{
					/*check for special keys*/
					switch(eventbuffer[i].Event.KeyEvent.wVirtualKeyCode)
					{
					case VK_HOME:
						return MP_KEY_HOME;
					case VK_END:
						return MP_KEY_END;
					case VK_DELETE:
						return MP_KEY_DEL;
                    case VK_INSERT:
						return MP_KEY_INS;
					case VK_BACK:
						return MP_KEY_BS;
					case VK_PRIOR:
						return MP_KEY_PGUP;
					case VK_NEXT:
						return MP_KEY_PGDWN;
					case VK_RETURN:
						return MP_KEY_ENTER;
					case VK_ESCAPE:
						return MP_KEY_ESC;
					case VK_LEFT:
						return MP_KEY_LEFT;
					case VK_UP:
						return MP_KEY_UP;
					case VK_RIGHT:
						return MP_KEY_RIGHT;
					case VK_DOWN:
						return MP_KEY_DOWN;
                    case VK_SHIFT:
                        continue;
					}
					/*check for function keys*/
        			if(0x87 >= eventbuffer[i].Event.KeyEvent.wVirtualKeyCode && eventbuffer[i].Event.KeyEvent.wVirtualKeyCode >= 0x70)
						return MP_KEY_F + 1 + eventbuffer[i].Event.KeyEvent.wVirtualKeyCode - 0x70;

					/*only characters should be remaining*/
					//printf("getch2: YOU PRESSED \"%c\" \n",eventbuffer[i].Event.KeyEvent.uChar.AsciiChar);
				    return eventbuffer[i].Event.KeyEvent.uChar.AsciiChar;
				}
				break;

			case MOUSE_EVENT:
            case WINDOW_BUFFER_SIZE_EVENT:
            case FOCUS_EVENT:
            case MENU_EVENT:
            default:
				//printf("getch2: unsupported event type");
			    break;
        }
    }
	return -1;
}

bool getch2(struct mp_fifo *fifo)
{
    int r = getch2_internal();
    if (r >= 0)
	mplayer_put_key(fifo, r);
    return true;
}

void getch2_poll(void){
}

void getch2_enable(void)
{
	DWORD retval;
    in = GetStdHandle(STD_INPUT_HANDLE);
   	if(!GetNumberOfConsoleInputEvents(in,&retval))
	{
		printf("getch2: %i can't get number of input events  [disabling console input]\n",(int)GetLastError());
		getch2_status = 0;
	}
    else getch2_status=1;
}

void getch2_disable(void)
{
    if(!getch2_status) return; // already disabled / never enabled
    getch2_status=0;
}
