/*
	vo_quartz.c
	
	by Nicolas Plourde <nicolasplourde@hotmail.com>
	
	Copyright (c) Nicolas Plourde - April 2004
	
	MPlayer Mac OSX Quartz video out module.
	
	todo:   -YUV support.
			-Redo event handling.
			-Choose fullscreen display device.
			-Fullscreen antialiasing.
			-resize black bar without CGContext
			-rootwin
			-non-blocking event
			-(add sugestion here)
 */

//SYS
#include <stdio.h>

//OSX
#include <Carbon/Carbon.h>

//MPLAYER
#include "config.h"
#include "video_out.h"
#include "video_out_internal.h"
#include "aspect.h"

#include "../input/input.h"
#include "../input/mouse.h"

#include "vo_quartz.h"

static vo_info_t info = 
{
	"Mac OSX (Quartz)",
	"quartz",
	"Nicolas Plourde <nicolasplourde@hotmail.com>",
	""
};

LIBVO_EXTERN(quartz)

uint32_t image_width;
uint32_t image_height;
uint32_t image_depth;
uint32_t image_bytes;
uint32_t image_format;
char *image_data;

extern int vo_ontop;
extern int vo_fs;

int int_pause = 0;
float winAlpha = 1;

int device_width, device_height;

WindowRef theWindow;

GWorldPtr imgGWorld;

Rect imgRect;
Rect dstRect;
Rect winRect;

CGContextRef context;

#include "../osdep/keycodes.h"
extern void mplayer_put_key(int code);

extern void vo_draw_text(int dxs,int dys,void (*draw_alpha)(int x0,int y0, int w,int h, unsigned char* src, unsigned char *srca, int stride));

//PROTOTYPE/////////////////////////////////////////////////////////////////
void window_resized();
void window_ontop();
void window_fullscreen();

static OSStatus MainWindowEventHandler(EventHandlerCallRef nextHandler, EventRef event, void *userData);
static OSStatus MainKeyboardEventHandler(EventHandlerCallRef nextHandler, EventRef event, void *userData);
static OSStatus MainMouseEventHandler(EventHandlerCallRef nextHandler, EventRef event, void *userData);

static void draw_alpha(int x0, int y0, int w, int h, unsigned char *src, unsigned char *srca, int stride)
{
	vo_draw_alpha_rgb32(w,h,src,srca,stride,image_data+4*(y0*imgRect.right+x0),4*imgRect.right);
}

//default window event handler
static OSStatus MainWindowEventHandler(EventHandlerCallRef nextHandler, EventRef event, void *userData)
{
    OSStatus err = noErr;
	WindowRef     window;
	Rect          rectPort = {0,0,0,0};
	OSStatus      result = eventNotHandledErr;
	UInt32        class = GetEventClass (event);
	UInt32        kind = GetEventKind (event);

	GetEventParameter(event, kEventParamDirectObject, typeWindowRef, NULL, sizeof(WindowRef), NULL, &window);
	if(window)
	{
		GetWindowPortBounds (window, &rectPort);
	}
  
    switch (kind)
    {
		//close window
        case kEventWindowClosed:
			HideWindow(window);
			mplayer_put_key(KEY_ESC);
		break;
		
		//resize window
		case kEventWindowBoundsChanged:
			window_resized();
			flip_page();
		break;
			
        default:
            err = eventNotHandledErr;
            break;
    }
    
    return err;
}

//keyboard event handler
static OSStatus MainKeyboardEventHandler(EventHandlerCallRef nextHandler, EventRef event, void *userData)
{
    OSStatus err = noErr;
	UInt32 macKeyCode;
	
	GetEventParameter(event, kEventParamKeyCode, typeUInt32, NULL, sizeof(macKeyCode), NULL, &macKeyCode);
	
    switch (GetEventKind (event))
    {
        case kEventRawKeyDown: 
		{
			switch(macKeyCode)
			{
				case QZ_RETURN: mplayer_put_key(KEY_ENTER);break;
				case QZ_ESCAPE: mplayer_put_key(KEY_ESC);break;
				case QZ_q: mplayer_put_key('q');break;
 				case QZ_F1: mplayer_put_key(KEY_F+1);break;
 				case QZ_F2: mplayer_put_key(KEY_F+2);break;
 				case QZ_F3: mplayer_put_key(KEY_F+3);break;
 				case QZ_F4: mplayer_put_key(KEY_F+4);break;
 				case QZ_F5: mplayer_put_key(KEY_F+5);break;
 				case QZ_F6: mplayer_put_key(KEY_F+6);break;
 				case QZ_F7: mplayer_put_key(KEY_F+7);break;
 				case QZ_F8: mplayer_put_key(KEY_F+8);break;
 				case QZ_F9: mplayer_put_key(KEY_F+9);break;
 				case QZ_F10: mplayer_put_key(KEY_F+10);break;
 				case QZ_F11: mplayer_put_key(KEY_F+11);break;
 				case QZ_F12: mplayer_put_key(KEY_F+12);break;
				case QZ_o: mplayer_put_key('o');break;
				case QZ_SPACE: mplayer_put_key(' ');break;
                case QZ_p: mplayer_put_key('p');break;
				//case QZ_7: mplayer_put_key(shift_key?'/':'7');
                //case QZ_PLUS: mplayer_put_key(shift_key?'*':'+');
				case QZ_KP_PLUS: mplayer_put_key('+');break;
				case QZ_MINUS:
				case QZ_KP_MINUS: mplayer_put_key('-');break;
				case QZ_TAB: mplayer_put_key('\t');break;
				case QZ_PAGEUP: mplayer_put_key(KEY_PAGE_UP);break;
				case QZ_PAGEDOWN: mplayer_put_key(KEY_PAGE_DOWN);break;  
				case QZ_UP: mplayer_put_key(KEY_UP);break;
				case QZ_DOWN: mplayer_put_key(KEY_DOWN);break;
                case QZ_LEFT: mplayer_put_key(KEY_LEFT);break;
				case QZ_RIGHT: mplayer_put_key(KEY_RIGHT);break;
				//case QZ_LESS: mplayer_put_key(shift_key?'>':'<'); break;
				//case QZ_GREATER: mplayer_put_key('>'); break;
				//case QZ_ASTERISK:
				case QZ_KP_MULTIPLY: mplayer_put_key('*'); break;
				case QZ_SLASH:
				case QZ_KP_DIVIDE: mplayer_put_key('/'); break;
				case QZ_KP0: mplayer_put_key(KEY_KP0); break;
				case QZ_KP1: mplayer_put_key(KEY_KP1); break;
				case QZ_KP2: mplayer_put_key(KEY_KP2); break;
				case QZ_KP3: mplayer_put_key(KEY_KP3); break;
				case QZ_KP4: mplayer_put_key(KEY_KP4); break;
				case QZ_KP5: mplayer_put_key(KEY_KP5); break;
				case QZ_KP6: mplayer_put_key(KEY_KP6); break;
				case QZ_KP7: mplayer_put_key(KEY_KP7); break;
				case QZ_KP8: mplayer_put_key(KEY_KP8); break;
				case QZ_KP9: mplayer_put_key(KEY_KP9); break;
				case QZ_KP_PERIOD: mplayer_put_key(KEY_KPDEC); break;
				case QZ_KP_ENTER: mplayer_put_key(KEY_KPENTER); break;
				case QZ_LEFTBRACKET: SetWindowAlpha(theWindow, winAlpha-=0.05);break;
				case QZ_RIGHTBRACKET: SetWindowAlpha(theWindow, winAlpha+=0.05);break;
				case QZ_f: mplayer_put_key('f'); break;
				case QZ_t: mplayer_put_key('T'); break;
				default:
					break;
			}
		}		
		break;
        default:
            err = eventNotHandledErr;
            break;
    }
    
    return err;
}

//Mouse event handler
static OSStatus MainMouseEventHandler(EventHandlerCallRef nextHandler, EventRef event, void *userData)
{
    OSStatus err = noErr;
	WindowPtr tmpWin;
	Point mousePos;
	
	GetEventParameter(event, kEventParamMouseLocation, typeQDPoint, 0, sizeof(Point), 0, &mousePos);

    switch (GetEventKind (event))
    {
        case kEventMouseDown: 
		{
				short part = FindWindow(mousePos,&tmpWin);
				
				if(part == inMenuBar)
				{
					MenuSelect(mousePos);
				}
		}		
		break;
        default:
            err = eventNotHandledErr;
            break;
    }
    
	HiliteMenu(0);
    return err;
}

static uint32_t config(uint32_t width, uint32_t height, uint32_t d_width, uint32_t d_height, uint32_t flags, char *title, uint32_t format)
{
	WindowAttributes	windowAttrs;
	CFStringRef			titleKey;
	CFStringRef			windowTitle; 
	OSStatus			result;
	GDHandle			deviceHdl;
	Rect				deviceRect;
	
	//Get Main device info///////////////////////////////////////////////////
	deviceHdl = GetMainDevice();
	deviceRect = (*deviceHdl)->gdRect;
	
	device_width = deviceRect.right;
	device_height = deviceRect.bottom;
	
	//misc mplayer setup/////////////////////////////////////////////////////
	image_width = width;
	image_height = height;
	image_depth = IMGFMT_RGB_DEPTH(format);
	image_bytes = (IMGFMT_RGB_DEPTH(format)+7)/8;
	image_data = malloc(image_width*image_height*4);

	vo_fs = flags & VOFLAG_FULLSCREEN;
	
	//get movie aspect
	aspect_save_orig(width,height);
	aspect_save_prescale(d_width,d_height);
	aspect_save_screenres(device_width, device_height);

	aspect(&d_width,&d_height,A_NOZOOM);

	//Create player window//////////////////////////////////////////////////
	windowAttrs =   kWindowStandardDocumentAttributes
					| kWindowStandardHandlerAttribute
					| kWindowLiveResizeAttribute;

	SetRect(&winRect, 0, 0, d_width, d_height);
	SetRect(&dstRect, 0, 0, d_width, d_height);
	SetRect(&imgRect, 0, 0, image_width, image_height);
	
	CreateNewWindow(kDocumentWindowClass, windowAttrs, &winRect, &theWindow);

	//Set window title
	titleKey	= CFSTR("MPlayer");
	windowTitle = CFCopyLocalizedString(titleKey, NULL);
	result		= SetWindowTitleWithCFString(theWindow, windowTitle);
	CFRelease(titleKey);
	CFRelease(windowTitle);

	//Install event handler
	const EventTypeSpec winEvents[] = { { kEventClassWindow, kEventWindowClosed }, { kEventClassWindow, kEventWindowBoundsChanged } };
	const EventTypeSpec keyEvents[] = { { kEventClassKeyboard, kEventRawKeyDown } };
	const EventTypeSpec mouseEvents[] = { { kEventClassMouse, kEventMouseDown } };
	
    InstallWindowEventHandler (theWindow, NewEventHandlerUPP (MainWindowEventHandler), GetEventTypeCount(winEvents), winEvents, theWindow, NULL);	
	InstallWindowEventHandler (theWindow, NewEventHandlerUPP (MainKeyboardEventHandler), GetEventTypeCount(keyEvents), keyEvents, theWindow, NULL);
	InstallApplicationEventHandler (NewEventHandlerUPP (MainMouseEventHandler), GetEventTypeCount(mouseEvents), mouseEvents, 0, NULL);

	//Show window
	RepositionWindow(theWindow, NULL, kWindowCascadeOnMainScreen);
	ShowWindow (theWindow);
	
	if(vo_fs)
		window_fullscreen();
		
	if(vo_ontop)
		window_ontop();
	
	return 0;
}

static void check_events(void)
{
	EventRef theEvent;
	EventTargetRef theTarget;
	OSStatus	theErr;
	
	//Get event
	theTarget = GetEventDispatcherTarget();
    theErr = ReceiveNextEvent(0, 0, kEventDurationNoWait,true, &theEvent);
    if(theErr == noErr && theEvent != NULL)
	{
		SendEventToEventTarget (theEvent, theTarget);
		ReleaseEvent(theEvent);
	}

	//update activity every 30 seconds to prevent
	//screensaver from starting up.
	DateTimeRec d;
	unsigned long curTime;
	static unsigned long lastTime = 0;
	
	GetTime(&d);
	DateToSeconds( &d, &curTime);
	
	if( ( (curTime - lastTime) >= 30) || (lastTime == 0))
	{
		UpdateSystemActivity(UsrActivity);
		lastTime = curTime;
	}
}

static void draw_osd(void)
{
	vo_draw_text(image_width,image_height,draw_alpha);
}

static void flip_page(void)
{
	OSStatus error;
	CGrafPtr oldPort,deskPort;
	GDHandle oldGDevice;
	OSStatus lockPixelsError;
	Boolean canLockPixels;
	
	GetGWorld (&oldPort, &oldGDevice);
	SetGWorld(GetWindowPort(theWindow), GetMainDevice());
	
	CGrafPtr windowPort = GetWindowPort(theWindow);
	
	lockPixelsError = LockPortBits(windowPort);
	
	if (lockPixelsError == noErr)
		canLockPixels = true;
	else 
		canLockPixels = false;
	
	if (canLockPixels)
	{
		CopyBits( GetPortBitMapForCopyBits (imgGWorld), GetPortBitMapForCopyBits (windowPort), &imgRect, &dstRect, srcCopy, 0 );
		lockPixelsError = UnlockPortBits(windowPort);
	}
	
	RgnHandle theVisibleRegion; 
	
	if (QDIsPortBuffered(windowPort))
	{
		theVisibleRegion = NewRgn();
		GetPortVisibleRegion(windowPort, theVisibleRegion);
		QDFlushPortBuffer(windowPort, theVisibleRegion); 
		DisposeRgn(theVisibleRegion);
	} 

	SetGWorld(oldPort, oldGDevice); 
}

static uint32_t draw_slice(uint8_t *src[], int stride[], int w,int h,int x,int y)
{
	return -1;
}

static uint32_t draw_frame(uint8_t *src[])
{
	image_data = src[0];

	DisposeGWorld(imgGWorld);
	NewGWorldFromPtr (&imgGWorld, k32ARGBPixelFormat, &imgRect, 0, 0, 0, image_data, image_width * 4);

	return 0; 
}

static uint32_t query_format(uint32_t format)
{
	image_format = format;
	
	//Curently supporting only rgb32 format.
    if ((format == IMGFMT_RGB32))
        return VFCAP_CSP_SUPPORTED | VFCAP_CSP_SUPPORTED_BY_HW;
    return 0;
}

static void uninit(void)
{
	ShowMenuBar();
}

static uint32_t preinit(const char *arg)
{
    return 0;
}

static uint32_t control(uint32_t request, void *data, ...)
{
  switch (request)
  {
	case VOCTRL_PAUSE: return (int_pause=1);
	case VOCTRL_RESUME: return (int_pause=0);
	case VOCTRL_FULLSCREEN: window_fullscreen(); return VO_TRUE;
	case VOCTRL_ONTOP: window_ontop(); return VO_TRUE;
	case VOCTRL_QUERY_FORMAT: return query_format(*((uint32_t*)data));
  }
  return VO_NOTIMPL;
}

void window_resized()
{
	float aspectX;
	float aspectY;
	
	int padding;
	
	uint32_t d_width;
	uint32_t d_height;
	
	GetWindowPortBounds(theWindow, &winRect);

	aspect( &d_width, &d_height, A_NOZOOM);
	
	aspectX = (float)((float)winRect.right/(float)d_width);
	aspectY = (float)((float)winRect.bottom/(float)d_height);
	
	if((d_height*aspectX)>winRect.bottom)
	{
		padding = (winRect.right - d_width*aspectY)/2;
		SetRect(&dstRect, padding, 0, d_width*aspectY+padding, d_height*aspectY);
	}
	else
	{
		padding = (winRect.bottom - d_height*aspectX)/2;
		SetRect(&dstRect, 0, padding, (d_width*aspectX), d_height*aspectX+padding);
	}

	//create a graphic context for the window
	SetPortBounds(GetWindowPort(theWindow), &winRect);
	CreateCGContextForPort(GetWindowPort(theWindow),&context);

	//fill background with black
	CGRect winBounds = CGRectMake( winRect.top, winRect.left, winRect.right, winRect.bottom);
	CGContextSetRGBFillColor(context, 0.0, 0.0, 0.0, 1.0);
	CGContextFillRect(context, winBounds);
	CGContextFlush(context);
}

void window_ontop()
{	
	if(!vo_ontop)
		SetWindowClass( theWindow, kUtilityWindowClass);
	else
		SetWindowClass( theWindow, kDocumentWindowClass);
		
	vo_ontop = (!(vo_ontop));
}

void window_fullscreen()
{
	static Rect oldRect;
	static Ptr *restoreState = nil;
	short width=640;
	short height=480;
	RGBColor black={0,0,0};
	GDHandle deviceHdl;
	Rect deviceRect;

	//go fullscreen
	if(!vo_fs)
	{
		//BeginFullScreen( &restoreState,nil,&width,&height,nil,&black,nil);
		HideMenuBar();

		//Get Main device info///////////////////////////////////////////////////
		deviceHdl = GetMainDevice();
		deviceRect = (*deviceHdl)->gdRect;
	
		device_width = deviceRect.right;
		device_height = deviceRect.bottom;

		//save old window size
		GetWindowPortBounds(theWindow, &oldRect);
		
		//hide mouse cursor
		HideCursor();
		
		//go fullscreen
		ChangeWindowAttributes(theWindow, 0, kWindowResizableAttribute);
		MoveWindow (theWindow, 0, 0, 1);		
		SizeWindow(theWindow, device_width, device_height,1);

		vo_fs = 1;
	}
	else //go back to windowed mode
	{
		//EndFullScreen( restoreState,0);
		ShowMenuBar();
		
		//Get Main device info///////////////////////////////////////////////////
		deviceHdl = GetMainDevice();
		deviceRect = (*deviceHdl)->gdRect;
	
		device_width = deviceRect.right;
		device_height = deviceRect.bottom;

		//show mouse cursor
		ShowCursor();
		
		//revert window to previous setting
		ChangeWindowAttributes(theWindow, kWindowResizableAttribute, 0);
		SizeWindow(theWindow, oldRect.right, oldRect.bottom,1);
		RepositionWindow(theWindow, NULL, kWindowCascadeOnMainScreen);
		
		vo_fs = 0;
	}
	
	window_resized();
}

