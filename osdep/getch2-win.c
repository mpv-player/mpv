/* windows TermIO for MPlayer          (C) 2003 Sascha Sommer */

// See  http://msdn.microsoft.com/library/default.asp?url=/library/en-us/winui/WinUI/WindowsUserInterface/UserInput/VirtualKeyCodes.asp
// for additional virtual keycodes


#include <stdio.h>
#include <windows.h>
#include "keycodes.h"
#include "input/input.h"
// HACK, stdin is used as something else below
#undef stdin

int mp_input_win32_slave_cmd_func(int fd,char* dest,int size){
  DWORD retval;
  HANDLE stdin = GetStdHandle(STD_INPUT_HANDLE);
  if(!PeekNamedPipe(stdin, NULL, size, &retval, NULL, NULL) || !retval){
	  return MP_INPUT_NOTHING;
  }
  if(retval>size)retval=size;
  ReadFile(stdin, dest, retval, &retval, NULL);
  if(retval)return retval;
  return MP_INPUT_NOTHING;
}

int screen_width=80;
int screen_height=24;
char * erase_to_end_of_line = NULL;

void get_screen_size(){
}

static HANDLE stdin;
static int getch2_status=0;

int getch2(int time){
	INPUT_RECORD eventbuffer[128];
    DWORD retval;
   	int i=0;
    if(!getch2_status)return -1;    
    /*check if there are input events*/
	WaitForSingleObject(stdin, time);
	if(!GetNumberOfConsoleInputEvents(stdin,&retval))
	{
		printf("getch2: can't get number of input events: %i\n",GetLastError());
		return -1;
	}
    if(retval<=0)return -1;
    
	/*read all events*/	
	if(!ReadConsoleInput(stdin,eventbuffer,128,&retval))
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
						return KEY_HOME;
					case VK_END:
						return KEY_END;
					case VK_DELETE:
						return KEY_DEL;
                    case VK_INSERT:
						return KEY_INS;
					case VK_BACK:
						return KEY_BS;
					case VK_PRIOR:
						return KEY_PGUP;
					case VK_NEXT:
						return KEY_PGDWN;
					case VK_RETURN:
						return KEY_ENTER;
					case VK_ESCAPE:
						return KEY_ESC;
					case VK_LEFT:
						return KEY_LEFT;
					case VK_UP:
						return KEY_UP;
					case VK_RIGHT:
						return KEY_RIGHT;
					case VK_DOWN:
						return KEY_DOWN;
                    case VK_SHIFT:
                        continue;              
					}
					/*check for function keys*/
        			if(0x87 >= eventbuffer[i].Event.KeyEvent.wVirtualKeyCode && eventbuffer[i].Event.KeyEvent.wVirtualKeyCode >= 0x70)
						return (KEY_F + 1 + eventbuffer[i].Event.KeyEvent.wVirtualKeyCode - 0x70);
 						
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


void getch2_enable(){
	DWORD retval;
    stdin = GetStdHandle(STD_INPUT_HANDLE);
   	if(!GetNumberOfConsoleInputEvents(stdin,&retval))
	{
		printf("getch2: %i can't get number of input events  [disabling console input]\n",GetLastError());
		getch2_status = 0;
	}
    else getch2_status=1;
}

void getch2_disable(){
    if(!getch2_status) return; // already disabled / never enabled
    getch2_status=0;
}

