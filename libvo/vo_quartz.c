/* 
 * vo_quartz.c
 *
 * Copyright (c) Nicolas Plourde - January 2004
 *
 * MPlayer Mac OSX Quartz video out module.
 *
 * TODO: -Fullscreen
 *       -Better event handling
 *
 * Note on performance:
 *  Right now i can play fullsize dvd video with -framedrop on my 
 *  iBook G4 800mhz. YUV to RGB converstion will speed up thing alot.
 *  Another thing is the slow fps when you maximize the window, I was
 *  not expecting that. I will fix this a.s.a.p. Im new to Mac 
 *  programming so help is welcome.
 */

//SYS
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <errno.h>

//OSX
#include <Carbon/Carbon.h>
#include <QuickTime/QuickTime.h>

//MPLAYER
#include "config.h"
#include "video_out.h"
#include "video_out_internal.h"
#include "aspect.h"

#include "../input/input.h"
#include "../input/mouse.h"

#include "vo_quartz.h"

static vo_info_t info = {
  "MacOS X (Quartz)",
  "quartz",
  "Nicolas Plourde <nicolasplourde@hotmail.com>",
  ""
};

LIBVO_EXTERN (quartz)
static unsigned char *ImageData = NULL;

static uint32_t image_width;
static uint32_t image_height;
static uint32_t image_depth;
static uint32_t image_bytes;
static uint32_t image_format;

static int int_pause = 0;

int screen_width, screen_height;

WindowRef theWindow;
CGContextRef context;
CGRect bounds;
CGRect winBounds;
Rect contentRect;
CGImageRef image;
CGDataProviderRef dataProviderRef;
Ptr oldscreenstate;
RGBColor black = { 0, 0, 0 };
float winAlpha = 1;

#include "../osdep/keycodes.h"
extern void mplayer_put_key (int code);

//PROTOTYPE/////////////////////////////////////////////////////////////////
void resize_window (uint32_t width, uint32_t height);
static OSStatus MainWindowEventHandler (EventHandlerCallRef nextHandler,
					EventRef event, void *userData);
static OSStatus MainKeyEventHandler (EventHandlerCallRef nextHandler,
				     EventRef event, void *userData);


//default window event handler
static OSStatus
MainWindowEventHandler (EventHandlerCallRef nextHandler, EventRef event,
			void *userData)
{
  OSStatus err = noErr;
  WindowRef window;
  Rect rectPort = { 0, 0, 0, 0 };
  OSStatus result = eventNotHandledErr;
  UInt32 class = GetEventClass (event);
  UInt32 kind = GetEventKind (event);

  GetEventParameter (event, kEventParamDirectObject, typeWindowRef, NULL,
		     sizeof (WindowRef), NULL, &window);
  if (window)
    {
      GetWindowPortBounds (window, &rectPort);
    }

  switch (kind)
    {
    case kEventWindowActivated:

    case kEventWindowDrawContent:
      break;

    case kEventWindowClosed:
      HideWindow (window);
      mplayer_put_key (KEY_ESC);
      break;

    case kEventWindowShown:
      InvalWindowRect (window, &rectPort);
      break;

    case kEventWindowBoundsChanged:
      resize_window (rectPort.right, rectPort.bottom);
      break;

    case kEventWindowZoomed:
      resize_window (rectPort.right, rectPort.bottom);
      break;

    default:
      err = eventNotHandledErr;
      break;
    }

  return err;
}

//keyboard event handler
static OSStatus
MainKeyEventHandler (EventHandlerCallRef nextHandler, EventRef event,
		     void *userData)
{
  OSStatus err = noErr;
  UInt32 macKeyCode;

  GetEventParameter (event, kEventParamKeyCode, typeUInt32, NULL,
		     sizeof (macKeyCode), NULL, &macKeyCode);

  switch (GetEventKind (event))
    {
    case kEventRawKeyDown:
      {
	switch (macKeyCode)
	  {
	  case QZ_RETURN:
	    mplayer_put_key (KEY_ENTER);
	    break;
	  case QZ_ESCAPE:
	    EndFullScreen (oldscreenstate, 0);
	    QuitApplicationEventLoop ();
	    mplayer_put_key (KEY_ESC);
	    break;
	  case QZ_q:
	    mplayer_put_key ('q');
	    break;
	  case QZ_F1:
	    mplayer_put_key (KEY_F + 1);
	    break;
	  case QZ_F2:
	    mplayer_put_key (KEY_F + 2);
	    break;
	  case QZ_F3:
	    mplayer_put_key (KEY_F + 3);
	    break;
	  case QZ_F4:
	    mplayer_put_key (KEY_F + 4);
	    break;
	  case QZ_F5:
	    mplayer_put_key (KEY_F + 5);
	    break;
	  case QZ_F6:
	    mplayer_put_key (KEY_F + 6);
	    break;
	  case QZ_F7:
	    mplayer_put_key (KEY_F + 7);
	    break;
	  case QZ_F8:
	    mplayer_put_key (KEY_F + 8);
	    break;
	  case QZ_F9:
	    mplayer_put_key (KEY_F + 9);
	    break;
	  case QZ_F10:
	    mplayer_put_key (KEY_F + 10);
	    break;
	  case QZ_F11:
	    mplayer_put_key (KEY_F + 11);
	    break;
	  case QZ_F12:
	    mplayer_put_key (KEY_F + 12);
	    break;
	  case QZ_o:
	    mplayer_put_key ('o');
	    break;
	  case QZ_SPACE:
	    mplayer_put_key (' ');
	    break;
	  case QZ_p:
	    mplayer_put_key ('p');
	    break;
	    //case QZ_7: mplayer_put_key(shift_key?'/':'7');
	    //case QZ_PLUS: mplayer_put_key(shift_key?'*':'+');
	  case QZ_KP_PLUS:
	    mplayer_put_key ('+');
	    break;
	  case QZ_MINUS:
	  case QZ_KP_MINUS:
	    mplayer_put_key ('-');
	    break;
	  case QZ_TAB:
	    mplayer_put_key ('\t');
	    break;
	  case QZ_PAGEUP:
	    mplayer_put_key (KEY_PAGE_UP);
	    break;
	  case QZ_PAGEDOWN:
	    mplayer_put_key (KEY_PAGE_DOWN);
	    break;
	  case QZ_UP:
	    mplayer_put_key (KEY_UP);
	    break;
	  case QZ_DOWN:
	    mplayer_put_key (KEY_DOWN);
	    break;
	  case QZ_LEFT:
	    mplayer_put_key (KEY_LEFT);
	    break;
	  case QZ_RIGHT:
	    mplayer_put_key (KEY_RIGHT);
	    break;
	    //case QZ_LESS: mplayer_put_key(shift_key?'>':'<'); break;
	    //case QZ_GREATER: mplayer_put_key('>'); break;
	    //case QZ_ASTERISK:
	  case QZ_KP_MULTIPLY:
	    mplayer_put_key ('*');
	    break;
	  case QZ_SLASH:
	  case QZ_KP_DIVIDE:
	    mplayer_put_key ('/');
	    break;
	  case QZ_KP0:
	    mplayer_put_key (KEY_KP0);
	    break;
	  case QZ_KP1:
	    mplayer_put_key (KEY_KP1);
	    break;
	  case QZ_KP2:
	    mplayer_put_key (KEY_KP2);
	    break;
	  case QZ_KP3:
	    mplayer_put_key (KEY_KP3);
	    break;
	  case QZ_KP4:
	    mplayer_put_key (KEY_KP4);
	    break;
	  case QZ_KP5:
	    mplayer_put_key (KEY_KP5);
	    break;
	  case QZ_KP6:
	    mplayer_put_key (KEY_KP6);
	    break;
	  case QZ_KP7:
	    mplayer_put_key (KEY_KP7);
	    break;
	  case QZ_KP8:
	    mplayer_put_key (KEY_KP8);
	    break;
	  case QZ_KP9:
	    mplayer_put_key (KEY_KP9);
	    break;
	  case QZ_KP_PERIOD:
	    mplayer_put_key (KEY_KPDEC);
	    break;
	  case QZ_KP_ENTER:
	    mplayer_put_key (KEY_KPENTER);
	    break;
	  case QZ_LEFTBRACKET:
	    SetWindowAlpha (theWindow, winAlpha -= 0.05);
	    break;
	  case QZ_RIGHTBRACKET:
	    SetWindowAlpha (theWindow, winAlpha += 0.05);
	    break;
	  case QZ_f:
	    break;
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

static uint32_t
config (uint32_t width, uint32_t height, uint32_t d_width, uint32_t d_height,
	uint32_t flags, char *title, uint32_t format)
{
  WindowAttributes windowAttrs;
  CFStringRef titleKey;
  CFStringRef windowTitle;
  OSStatus result;
  GDHandle deviceHdl;
  Rect deviceRect;

  //Get Main device info///////////////////////////////////////////////////
  deviceHdl = GetMainDevice ();
  deviceRect = (*deviceHdl)->gdRect;

  screen_width = deviceRect.right;
  screen_height = deviceRect.bottom;

  //misc mplayer setup/////////////////////////////////////////////////////
  image_width = width;
  image_height = height;
  image_depth = IMGFMT_RGB_DEPTH (format);
  image_bytes = (IMGFMT_RGB_DEPTH (format) + 7) / 8;

  aspect_save_orig (width, height);
  aspect_save_prescale (d_width, d_height);
  aspect_save_screenres (screen_width, screen_height);

  aspect (&d_width, &d_height, A_NOZOOM);

  if (ImageData)
    free (ImageData);
  ImageData = malloc (image_width * image_height * image_bytes);

  //Create player window//////////////////////////////////////////////////
  windowAttrs = kWindowStandardDocumentAttributes
    | kWindowMetalAttribute
    | kWindowStandardHandlerAttribute
    | kWindowInWindowMenuAttribute
    | kWindowLiveResizeAttribute | kWindowCompositingAttribute;

  SetRect (&contentRect, 0, 0, d_width, d_height);
  CreateNewWindow (kDocumentWindowClass, windowAttrs, &contentRect,
		   &theWindow);

  titleKey = CFSTR ("MPlayer");
  windowTitle = CFCopyLocalizedString (titleKey, NULL);
  result = SetWindowTitleWithCFString (theWindow, windowTitle);
  CFRelease (titleKey);
  CFRelease (windowTitle);

  const EventTypeSpec winEvents[] = {
    {kEventClassWindow, kEventWindowActivated},
    {kEventClassWindow, kEventWindowDrawContent},
    {kEventClassWindow, kEventWindowClosed},
    {kEventClassWindow, kEventWindowShown},
    {kEventClassWindow, kEventWindowBoundsChanged},
    {kEventClassWindow, kEventWindowZoomed}
  };

  const EventTypeSpec keyEvents[] =
    { {kEventClassKeyboard, kEventRawKeyDown} };
  InstallWindowEventHandler (theWindow,
			     NewEventHandlerUPP (MainWindowEventHandler),
			     GetEventTypeCount (winEvents), winEvents,
			     theWindow, NULL);
  InstallWindowEventHandler (theWindow,
			     NewEventHandlerUPP (MainKeyEventHandler),
			     GetEventTypeCount (keyEvents), keyEvents,
			     theWindow, NULL);

  RepositionWindow (theWindow, NULL, kWindowCascadeOnMainScreen);
  ShowWindow (theWindow);

  //Setup Quartz context
  CreateCGContextForPort (GetWindowPort (theWindow), &context);

  //set size and aspect for current window
  resize_window (d_width, d_height);

  return 0;
}

//resize drawing context to fit window
void
resize_window (uint32_t width, uint32_t height)
{
  //this is a "wow it work". Need some improvement.
  uint32_t d_width;
  uint32_t d_height;
  uint32_t size;
  Rect tmpRect;

  float aspectX;
  float aspectY;

  aspect (&d_width, &d_height, A_NOZOOM);

  aspectX = (float) ((float) d_width * (width / (float) d_width));
  aspectY = (float) ((float) d_height * (width / (float) d_width));

  if (aspectY > height)
    {
      aspectX = (float) ((float) d_width * (height / (float) d_height));
      aspectY = (float) ((float) d_height * (height / (float) d_height));

      bounds = CGRectMake ((width - aspectX) / 2, 0, aspectX, aspectY);
    }
  else
    {
      bounds = CGRectMake (0, (height - aspectY) / 2, aspectX, aspectY);
    }

  //create a graphic context for the window
  GetWindowPortBounds (theWindow, &tmpRect);
  SetPortBounds (GetWindowPort (theWindow), &tmpRect);
  CreateCGContextForPort (GetWindowPort (theWindow), &context);

  //fill background with black
  winBounds =
    CGRectMake (tmpRect.top, tmpRect.left, tmpRect.right, tmpRect.bottom);
  CGContextSetRGBFillColor (context, 0.0, 0.0, 0.0, 1.0);
  CGContextFillRect (context, winBounds);
}

static void
check_events (void)
{
  EventRef theEvent;
  EventTargetRef theTarget;

  theTarget = GetEventDispatcherTarget ();

  ReceiveNextEvent (0, NULL, kEventDurationNoWait, true, &theEvent);
  SendEventToEventTarget (theEvent, theTarget);
  ReleaseEvent (theEvent);

  //if(VO_EVENT_RESIZE) resize_window(vo_dwidth,vo_dheight);
  if (VO_EVENT_EXPOSE && int_pause)
    flip_page ();
}

static void
draw_osd (void)
{
}

static void
flip_page (void)
{
  CGContextFlush (context);
}

static uint32_t
draw_slice (uint8_t * src[], int stride[], int w, int h, int x, int y)
{
  return -1;
}

static uint32_t
draw_frame (uint8_t * src[])
{
  //this is very slow. I have to find another way.
  CGImageAlphaInfo alphaInfo;

  dataProviderRef =
    CGDataProviderCreateWithData (0, src[0],
				  image_width * image_height * image_bytes,
				  0);

  if (image_format == IMGFMT_RGB24)
    alphaInfo = kCGImageAlphaNone;
  else if (image_format == IMGFMT_RGB32)
    alphaInfo = kCGImageAlphaNoneSkipFirst;

  image = CGImageCreate (image_width,
			 image_height,
			 8,
			 image_depth,
			 ((image_width * image_depth) + 7) / 8,
			 CGColorSpaceCreateDeviceRGB (),
			 alphaInfo,
			 dataProviderRef, 0, 0, kCGRenderingIntentDefault);

  CGContextDrawImage (context, bounds, image);

  return 0;
}

static uint32_t
query_format (uint32_t format)
{
  image_format = format;

  //Curently supporting only rgb format.
  if ((format == IMGFMT_RGB24) || (format == IMGFMT_RGB32))
    return VFCAP_CSP_SUPPORTED | VFCAP_CSP_SUPPORTED_BY_HW;
  return 0;
}


static void
uninit (void)
{
}

static uint32_t
preinit (const char *arg)
{
  return 0;
}

static uint32_t
control (uint32_t request, void *data, ...)
{
  switch (request)
    {
    case VOCTRL_PAUSE:
      return (int_pause = 1);
    case VOCTRL_RESUME:
      return (int_pause = 0);
    case VOCTRL_QUERY_FORMAT:
      return query_format (*((uint32_t *) data));
    }
  return VO_NOTIMPL;
}
