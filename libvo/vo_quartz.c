/*
	vo_quartz.c
	
	by Nicolas Plourde <nicolasplourde@hotmail.com>
	
	Copyright (c) Nicolas Plourde - April 2004

	YUV support Copyright (C) 2004 Romain Dolbeau <romain@dolbeau.org>
	
	MPlayer Mac OSX Quartz video out module.
	
	todo:   -'plist' resource
			-Choose fullscreen display device (-xineramascreen / -multiscreen).
			-resize black bar without CGContext
			-rootwin
			-screen overlay output
			-non-blocking event
			-(add sugestion here)
 */

//SYS
#include <stdio.h>

//OSX
#include <Carbon/Carbon.h>
#include <QuickTime/QuickTime.h>

//MPLAYER
#include "config.h"
#include "fastmemcpy.h"
#include "video_out.h"
#include "video_out_internal.h"
#include "aspect.h"
#include "mp_msg.h"
#include "m_option.h"

#include "../input/input.h"
#include "../input/mouse.h"

#include "vo_quartz.h"

static vo_info_t info = 
{
	"Mac OSX (Quartz)",
	"quartz",
	"Nicolas Plourde <nicolasplourde@hotmail.com>, Romain Dolbeau <romain@dolbeau.org>",
	""
};

LIBVO_EXTERN(quartz)

static uint32_t image_depth;
static uint32_t image_format;
static uint32_t image_size;
static uint32_t image_buffer_size;
static char *image_data;

static ImageSequence seqId;
static CodecType image_qtcodec;
static PlanarPixmapInfoYUV420 *P = NULL;
static struct
{
	ImageDescriptionHandle desc;
	Handle extension_colr;
	Handle extension_fiel;
	Handle extension_clap;
	Handle extension_pasp;
} yuv_qt_stuff;
static MatrixRecord matrix;
static int EnterMoviesDone = 0;
static int get_image_done = 0;

extern int vo_ontop;
extern int vo_fs; // user want fullscreen
static int vo_quartz_fs; // we are in fullscreen

static int int_pause = 0;
static float winAlpha = 1;

static int device_width;
static int device_height;

static WindowRef theWindow = NULL;

static Rect imgRect; // size of the original image (unscaled)
static Rect dstRect; // size of the displayed image (after scaling)
static Rect winRect; // size of the window containg the displayed image (include padding)
static Rect oldWinRect; // size of the window containg the displayed image (include padding) when NOT in FS mode

static CGContextRef context;

#include "../osdep/keycodes.h"
extern void mplayer_put_key(int code);

extern void vo_draw_text(int dxs,int dys,void (*draw_alpha)(int x0,int y0, int w,int h, unsigned char* src, unsigned char *srca, int stride));

//PROTOTYPE/////////////////////////////////////////////////////////////////
void window_resized();
void window_ontop();
void window_fullscreen();

static OSStatus MainEventHandler(EventHandlerCallRef nextHandler, EventRef event, void *userData);

static void draw_alpha(int x0, int y0, int w, int h, unsigned char *src, unsigned char *srca, int stride)
{
	switch (image_format)
	{
		case IMGFMT_RGB32:
			vo_draw_alpha_rgb32(w,h,src,srca,stride,image_data+4*(y0*imgRect.right+x0),4*imgRect.right);
			break;
		case IMGFMT_YV12:
		case IMGFMT_IYUV:
		case IMGFMT_I420:
			vo_draw_alpha_yv12(w,h,src,srca,stride, ((char*)P) + P->componentInfoY.offset + x0 + y0 * imgRect.right, imgRect.right);
			break;
		case IMGFMT_UYVY:
			vo_draw_alpha_uyvy(w,h,src,srca,stride,((char*)P) + (x0 + y0 * imgRect.right) * 2,imgRect.right*2);
			break;
		case IMGFMT_YUY2:
			vo_draw_alpha_yuy2(w,h,src,srca,stride,((char*)P) + (x0 + y0 * imgRect.right) * 2,imgRect.right*2);
			break;
	}
}

//default window event handler
static OSStatus MainEventHandler(EventHandlerCallRef nextHandler, EventRef event, void *userData)
{
    OSStatus err = noErr;
	OSStatus result = eventNotHandledErr;
	UInt32 class = GetEventClass (event);
	UInt32 kind = GetEventKind (event); 
  
	if(class == kEventClassWindow)
	{
		WindowRef     window;
		Rect          rectPort = {0,0,0,0};
		
		GetEventParameter(event, kEventParamDirectObject, typeWindowRef, NULL, sizeof(WindowRef), NULL, &window);
	
		if(window)
		{
			GetWindowPortBounds (window, &rectPort);
		}   
	
		switch (kind)
		{
			//close window
			case kEventWindowClosed:
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
	}
	else if(class == kEventClassKeyboard)
	{
		char macCharCodes;
		UInt32 macKeyCode;
		UInt32 macKeyModifiers;
	
		GetEventParameter(event, kEventParamKeyMacCharCodes, typeChar, NULL, sizeof(macCharCodes), NULL, &macCharCodes);
		GetEventParameter(event, kEventParamKeyCode, typeUInt32, NULL, sizeof(macKeyCode), NULL, &macKeyCode);
		GetEventParameter(event, kEventParamKeyModifiers, typeUInt32, NULL, sizeof(macKeyModifiers), NULL, &macKeyModifiers);
	
		switch (kind)
		{
			case kEventRawKeyDown: 
			{			
				switch(macKeyCode)
				{ 
					case QZ_RETURN: mplayer_put_key(KEY_ENTER);break;
					case QZ_ESCAPE: mplayer_put_key(KEY_ESC);break;
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
				
					default:mplayer_put_key(macCharCodes);break;
				}
			}
			
			default:
				err = eventNotHandledErr;
				break;
		}
	}
	else if(class == kEventClassMouse)
	{
		WindowPtr tmpWin;
		Point mousePos;

		GetEventParameter(event, kEventParamMouseLocation, typeQDPoint, 0, sizeof(Point), 0, &mousePos);

		switch (kind)
		{
			case kEventMouseDown:
			{
				EventMouseButton button;
				GetEventParameter(event, kEventParamMouseButton, typeMouseButton, 0, sizeof(EventMouseButton), 0, &button);
				
				short part = FindWindow(mousePos,&tmpWin);
				
				if(part == inMenuBar)
				{
					MenuSelect(mousePos);
					HiliteMenu(0);
				}
				else if(part == inContent)
				{
					switch(button)
					{ 
						case 1: mplayer_put_key(MOUSE_BTN0);break;
						case 2: mplayer_put_key(MOUSE_BTN2);break;
						case 3: mplayer_put_key(MOUSE_BTN1);break;
				
						default:break;
					}
				}
			}		
			break;
			
			case kEventMouseWheelMoved:
			{
				int wheel;
				GetEventParameter(event, kEventParamMouseWheelDelta, typeSInt32, 0, sizeof(int), 0, &wheel);

				short part = FindWindow(mousePos,&tmpWin);
				
				if(part == inContent)
				{
					if(wheel > 0)
						mplayer_put_key(MOUSE_BTN3);
					else
						mplayer_put_key(MOUSE_BTN4);
				}
			}
			break;
			
			default:
				err = eventNotHandledErr;
				break;
		}
	}
	
    return err;
}

static void quartz_CreateWindow(uint32_t d_width, uint32_t d_height, WindowAttributes windowAttrs) 
{
	CFStringRef		titleKey;
	CFStringRef		windowTitle; 
	OSStatus	       	result;
 
	SetRect(&winRect, 0, 0, d_width, d_height);
	SetRect(&oldWinRect, 0, 0, d_width, d_height);
	SetRect(&dstRect, 0, 0, d_width, d_height);
  
	CreateNewWindow(kDocumentWindowClass, windowAttrs, &winRect, &theWindow);
  
	//Set window title
	titleKey	= CFSTR("MPlayer");
	windowTitle = CFCopyLocalizedString(titleKey, NULL);
	result		= SetWindowTitleWithCFString(theWindow, windowTitle);
	CFRelease(titleKey);
	CFRelease(windowTitle);
  
	//Install event handler
	const EventTypeSpec winEvents[] = { { kEventClassKeyboard, kEventRawKeyDown },
										{ kEventClassMouse, kEventMouseDown },
										{ kEventClassMouse, kEventMouseWheelMoved },
										{ kEventClassWindow, kEventWindowClosed }, 
										{ kEventClassWindow, kEventWindowBoundsChanged } };
  
	InstallApplicationEventHandler (NewEventHandlerUPP (MainEventHandler), GetEventTypeCount(winEvents), winEvents, 0, NULL);
}

static uint32_t config(uint32_t width, uint32_t height, uint32_t d_width, uint32_t d_height, uint32_t flags, char *title, uint32_t format)
{
	WindowAttributes	windowAttrs;
	GDHandle			deviceHdl;
	Rect				deviceRect;
	OSErr				qterr;
	
	//Get Main device info///////////////////////////////////////////////////
	deviceHdl = GetMainDevice();
	deviceRect = (*deviceHdl)->gdRect;
	
	device_width = deviceRect.right;
	device_height = deviceRect.bottom;
	
	//misc mplayer setup/////////////////////////////////////////////////////
	SetRect(&imgRect, 0, 0, width, height);
	switch (image_format) 
	{
		case IMGFMT_RGB32:
			image_depth = 32;
			break;
		case IMGFMT_YV12:
		case IMGFMT_IYUV:
        case IMGFMT_I420:
        case IMGFMT_UYVY:
        case IMGFMT_YUY2:
			image_depth = 16;
			break;
	}
	image_size = ((imgRect.right*imgRect.bottom*image_depth)+7)/8;

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
					
	windowAttrs &= (~kWindowResizableAttribute);

 	if (theWindow == NULL)
	{
		quartz_CreateWindow(d_width, d_height, windowAttrs);
		
		if (theWindow == NULL)
		{
			mp_msg(MSGT_VO, MSGL_FATAL, "Quartz error: Couldn't create window !!!!!\n");
			return -1;
		}
	}
	else 
	{
		HideWindow(theWindow);
		ChangeWindowAttributes(theWindow, ~windowAttrs, windowAttrs);
		SetRect(&winRect, 0, 0, d_width, d_height);
		SetRect(&oldWinRect, 0, 0, d_width, d_height);
		SizeWindow (theWindow, d_width, d_height, 1);
 	}
 	
 	get_image_done = 0;

	if (!EnterMoviesDone)
	{
		qterr = EnterMovies();
		EnterMoviesDone = 1;
	}
	else
		qterr = 0;
	
	if (qterr)
	{
		mp_msg(MSGT_VO, MSGL_FATAL, "Quartz error: EnterMovies (%d)\n", qterr);
		return -1;
	}
	
	SetPort(GetWindowPort(theWindow));
	SetIdentityMatrix(&matrix);
	
	if ((d_width != width) || (d_height != height))
	{
		ScaleMatrix(&matrix, FixDiv(Long2Fix(d_width),Long2Fix(width)), FixDiv(Long2Fix(d_height),Long2Fix(height)), 0, 0);
	}
	
	switch (image_format) 
	{
		case IMGFMT_RGB32:
		{
			ImageDescriptionHandle desc;
			GWorldPtr imgGWorld;
			image_data = calloc(sizeof(image_size),1);
			NewGWorldFromPtr (&imgGWorld, k32ARGBPixelFormat, &imgRect, 0, 0, 0, image_data, imgRect.right * 4);
			MakeImageDescriptionForPixMap(GetGWorldPixMap(imgGWorld), &desc);
			DisposeGWorld(imgGWorld);
		
			qterr = DecompressSequenceBeginS (  &seqId,
												desc,
												image_data,
												image_size,
												GetWindowPort(theWindow),
												NULL,
												NULL,
												((d_width != width) || (d_height != height)) ? &matrix : NULL,
												srcCopy,
												NULL,
												0,
												codecLosslessQuality,
												bestSpeedCodec);
			free(image_data);
			image_data = NULL;
			if (qterr)
			{
				mp_msg(MSGT_VO, MSGL_FATAL, "Quartz error: DecompressSequenceBeginS (%d)\n", qterr);
				return -1;
			}
		}
		break;
		
		case IMGFMT_YV12:
		case IMGFMT_IYUV:
		case IMGFMT_I420:
		case IMGFMT_UYVY:
		case IMGFMT_YUY2:
		{
			yuv_qt_stuff.desc = (ImageDescriptionHandle)NewHandleClear( sizeof(ImageDescription) );
		
			yuv_qt_stuff.extension_colr = NewHandleClear(sizeof(NCLCColorInfoImageDescriptionExtension));
			((NCLCColorInfoImageDescriptionExtension*)(*yuv_qt_stuff.extension_colr))->colorParamType = kVideoColorInfoImageDescriptionExtensionType;
			((NCLCColorInfoImageDescriptionExtension*)(*yuv_qt_stuff.extension_colr))->primaries = 2;
			((NCLCColorInfoImageDescriptionExtension*)(*yuv_qt_stuff.extension_colr))->transferFunction = 2;
			((NCLCColorInfoImageDescriptionExtension*)(*yuv_qt_stuff.extension_colr))->matrix = 2;
		
			yuv_qt_stuff.extension_fiel = NewHandleClear(sizeof(FieldInfoImageDescriptionExtension));
			((FieldInfoImageDescriptionExtension*)(*yuv_qt_stuff.extension_fiel))->fieldCount = 1;
			((FieldInfoImageDescriptionExtension*)(*yuv_qt_stuff.extension_fiel))->fieldOrderings = 0;
		
			yuv_qt_stuff.extension_clap = NewHandleClear(sizeof(CleanApertureImageDescriptionExtension));
			((CleanApertureImageDescriptionExtension*)(*yuv_qt_stuff.extension_clap))->cleanApertureWidthN = imgRect.right;
			((CleanApertureImageDescriptionExtension*)(*yuv_qt_stuff.extension_clap))->cleanApertureWidthD = 1;
			((CleanApertureImageDescriptionExtension*)(*yuv_qt_stuff.extension_clap))->cleanApertureHeightN = imgRect.bottom;
			((CleanApertureImageDescriptionExtension*)(*yuv_qt_stuff.extension_clap))->cleanApertureHeightD = 1;
			((CleanApertureImageDescriptionExtension*)(*yuv_qt_stuff.extension_clap))->horizOffN = 0;
			((CleanApertureImageDescriptionExtension*)(*yuv_qt_stuff.extension_clap))->horizOffD = 1;
			((CleanApertureImageDescriptionExtension*)(*yuv_qt_stuff.extension_clap))->vertOffN = 0;
			((CleanApertureImageDescriptionExtension*)(*yuv_qt_stuff.extension_clap))->vertOffD = 1;
        
			yuv_qt_stuff.extension_pasp = NewHandleClear(sizeof(PixelAspectRatioImageDescriptionExtension));
			((PixelAspectRatioImageDescriptionExtension*)(*yuv_qt_stuff.extension_pasp))->hSpacing = 1;
			((PixelAspectRatioImageDescriptionExtension*)(*yuv_qt_stuff.extension_pasp))->vSpacing = 1;

			(*yuv_qt_stuff.desc)->idSize = sizeof(ImageDescription);
			(*yuv_qt_stuff.desc)->cType = image_qtcodec;
			(*yuv_qt_stuff.desc)->version = 2;
			(*yuv_qt_stuff.desc)->revisionLevel = 0;
			(*yuv_qt_stuff.desc)->vendor = 'mpla';
			(*yuv_qt_stuff.desc)->width = imgRect.right;
			(*yuv_qt_stuff.desc)->height = imgRect.bottom;
			(*yuv_qt_stuff.desc)->hRes = Long2Fix(72);
			(*yuv_qt_stuff.desc)->vRes = Long2Fix(72);
			(*yuv_qt_stuff.desc)->temporalQuality = 0;
			(*yuv_qt_stuff.desc)->spatialQuality = codecLosslessQuality;
			(*yuv_qt_stuff.desc)->frameCount = 1;
			(*yuv_qt_stuff.desc)->dataSize = 0;
			(*yuv_qt_stuff.desc)->depth = 24;
			(*yuv_qt_stuff.desc)->clutID = -1;
	  
			qterr = AddImageDescriptionExtension(yuv_qt_stuff.desc, yuv_qt_stuff.extension_colr, kColorInfoImageDescriptionExtension);
			if (qterr)
			{
				mp_msg(MSGT_VO, MSGL_ERR, "Quartz error: AddImageDescriptionExtension [colr] (%d)\n", qterr);
			}
			
			qterr = AddImageDescriptionExtension(yuv_qt_stuff.desc, yuv_qt_stuff.extension_fiel, kFieldInfoImageDescriptionExtension);
			if (qterr)
			{
				mp_msg(MSGT_VO, MSGL_ERR, "Quartz error: AddImageDescriptionExtension [fiel] (%d)\n", qterr);
			}
		
			qterr = AddImageDescriptionExtension(yuv_qt_stuff.desc, yuv_qt_stuff.extension_clap, kCleanApertureImageDescriptionExtension);
			if (qterr)
			{
				mp_msg(MSGT_VO, MSGL_ERR, "Quartz error: AddImageDescriptionExtension [clap] (%d)\n", qterr);
			}
		
			qterr = AddImageDescriptionExtension(yuv_qt_stuff.desc, yuv_qt_stuff.extension_pasp, kCleanApertureImageDescriptionExtension);
			if (qterr)
			{
				mp_msg(MSGT_VO, MSGL_ERR, "Quartz error: AddImageDescriptionExtension [pasp] (%d)\n", qterr);
			}
			if (P != NULL) { // second or subsequent movie
				free(P);
			}
			P = calloc(sizeof(PlanarPixmapInfoYUV420) + image_size, 1);
			switch (image_format)
			{
				case IMGFMT_YV12:
				case IMGFMT_IYUV:
				case IMGFMT_I420:
					P->componentInfoY.offset = sizeof(PlanarPixmapInfoYUV420);
					P->componentInfoCb.offset = P->componentInfoY.offset + image_size / 2;
					P->componentInfoCr.offset = P->componentInfoCb.offset + image_size / 4;
					P->componentInfoY.rowBytes = imgRect.right;
					P->componentInfoCb.rowBytes =  imgRect.right / 2;
					P->componentInfoCr.rowBytes =  imgRect.right / 2;
					image_buffer_size = image_size + sizeof(PlanarPixmapInfoYUV420);
					break;
				case IMGFMT_UYVY:
				case IMGFMT_YUY2:
					image_buffer_size = image_size;
					break;
			}

			qterr = DecompressSequenceBeginS(&seqId,
					   yuv_qt_stuff.desc,
					   (char *)P,
					   image_buffer_size,
					   GetWindowPort(theWindow),
					   NULL,
					   NULL,
					   ((d_width != width) || (d_height != height)) ? 
					   &matrix : NULL,
					   srcCopy,
					   NULL,
					   0,
					   codecLosslessQuality,
					   bestSpeedCodec);

			if (qterr)
			{
				mp_msg(MSGT_VO, MSGL_FATAL, "Quartz error: DecompressSequenceBeginS (%d)\n", qterr);
				return -1;
			}
		}
		break;
	}

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
	vo_draw_text(imgRect.right,imgRect.bottom,draw_alpha);
}

static void flip_page(void)
{
	switch (image_format) 
	{
		case IMGFMT_RGB32:
		{
		  if (EnterMoviesDone && (image_data != NULL)) 
			{
				OSErr qterr;
				CodecFlags flags = 0;
				
				qterr = DecompressSequenceFrameWhen(seqId,
													image_data,
													image_size,
													0,
													&flags,
													NULL,
													NULL);
				image_data = NULL;
				if (qterr)
				{
					mp_msg(MSGT_VO, MSGL_ERR, "Quartz error: DecompressSequenceFrameWhen in flip_page (%d) flags:0x%08x\n", qterr, flags);
				}
			}
		}
		break;

		case IMGFMT_YV12:
		case IMGFMT_IYUV:
		case IMGFMT_I420:
		case IMGFMT_UYVY:
		case IMGFMT_YUY2:
			if (EnterMoviesDone)
			{
				OSErr qterr;
				CodecFlags flags = 0;
				qterr = DecompressSequenceFrameWhen(seqId,
													(char *)P,
													image_buffer_size,
													0, //codecFlagUseImageBuffer,
													&flags,
													NULL,
													NULL);
				if (qterr)
				{
					mp_msg(MSGT_VO, MSGL_ERR, "Quartz error: DecompressSequenceFrameWhen in flip_page (%d) flags:0x%08x\n", qterr, flags);
				}
			}
		break;
	}
}

static uint32_t draw_slice(uint8_t *src[], int stride[], int w,int h,int x,int y)
{
	switch (image_format)
	{
  		case IMGFMT_YV12:
  		case IMGFMT_I420:
 			memcpy_pic(((char*)P) + P->componentInfoY.offset + x + imgRect.right * y, src[0], w, h, imgRect.right, stride[0]);
  			x=x/2;y=y/2;w=w/2;h=h/2;
  
 			memcpy_pic(((char*)P) + P->componentInfoCb.offset + x + imgRect.right / 2 * y, src[1], w, h, imgRect.right / 2, stride[1]);
 			memcpy_pic(((char*)P) + P->componentInfoCr.offset + x + imgRect.right / 2 * y, src[2], w, h, imgRect.right / 2, stride[2]);
  			return 0;
  
  		case IMGFMT_IYUV:
 			memcpy_pic(((char*)P) + P->componentInfoY.offset + x + imgRect.right * y, src[0], w, h, imgRect.right, stride[0]);
  			x=x/2;y=y/2;w=w/2;h=h/2;
  			
 			memcpy_pic(((char*)P) + P->componentInfoCr.offset + x + imgRect.right / 2 * y, src[1], w, h, imgRect.right / 2, stride[1]);
 			memcpy_pic(((char*)P) + P->componentInfoCb.offset + x + imgRect.right / 2 * y, src[2], w, h, imgRect.right / 2, stride[2]);
  			return 0;
	}
	return -1;
}

static uint32_t draw_frame(uint8_t *src[])
{
	switch (image_format)
	{
		case IMGFMT_RGB32:
			image_data = src[0];
			return 0;

		case IMGFMT_UYVY:
		case IMGFMT_YUY2:
			memcpy_pic(((char*)P), src[0], imgRect.right * 2, imgRect.bottom, imgRect.right * 2, imgRect.right * 2);
			return 0;
	}
	return -1;
}

static uint32_t query_format(uint32_t format)
{
	image_format = format;
	image_qtcodec = 0;
    
	if (format == IMGFMT_RGB32)
	{
		return VFCAP_CSP_SUPPORTED | VFCAP_OSD | VFCAP_HWSCALE_UP | VFCAP_HWSCALE_DOWN;
	}
    
    if ((format == IMGFMT_YV12) || (format == IMGFMT_IYUV) || (format == IMGFMT_I420))
	{
		image_qtcodec = kMpegYUV420CodecType; //kYUV420CodecType ?;
		return VFCAP_CSP_SUPPORTED | VFCAP_OSD | VFCAP_HWSCALE_UP | VFCAP_HWSCALE_DOWN | VFCAP_ACCEPT_STRIDE;
    }

    if (format == IMGFMT_YUY2)
	{
		image_qtcodec = kComponentVideoUnsigned;
		return VFCAP_CSP_SUPPORTED | VFCAP_OSD | VFCAP_HWSCALE_UP | VFCAP_HWSCALE_DOWN;
    }
    
    if (format == IMGFMT_UYVY)
	{
		image_qtcodec = k422YpCbCr8CodecType;
		return VFCAP_CSP_SUPPORTED | VFCAP_OSD | VFCAP_HWSCALE_UP | VFCAP_HWSCALE_DOWN;
	}

    return 0;
}

static void uninit(void)
{
	OSErr qterr;
			
	if (EnterMoviesDone)
	{
		qterr = CDSequenceEnd(seqId);
		if (qterr)
		{
			mp_msg(MSGT_VO, MSGL_ERR, "Quartz error: CDSequenceEnd (%d)\n", qterr);
		}
	}

	ShowMenuBar();
}

static uint32_t preinit(const char *arg)
{
    return 0;
}

static uint32_t draw_yuv_image(mp_image_t *mpi)
{
	// ATM we're only called for planar IMGFMT
	// drawing is done directly in P
	// and displaying is in flip_page.
	return get_image_done ? VO_TRUE : VO_FALSE;  
}

static uint32_t get_yuv_image(mp_image_t *mpi)
{
	if(mpi->type!=MP_IMGTYPE_EXPORT) return VO_FALSE;
  
	if(mpi->imgfmt!=image_format) return VO_FALSE;
  
	if(mpi->flags&MP_IMGFLAG_PLANAR)
	{
		if (mpi->num_planes != 3)
		{
			mp_msg(MSGT_VO, MSGL_ERR, "Quartz error: only 3 planes allowed in get_yuv_image for planar (%d) \n", mpi->num_planes);
			return VO_FALSE;
		}

		mpi->planes[0]=((char*)P) + P->componentInfoY.offset;
		mpi->stride[0]=imgRect.right;
		mpi->width=imgRect.right;

		if(mpi->flags&MP_IMGFLAG_SWAPPED)
		{
			// I420
			mpi->planes[1]=((char*)P) + P->componentInfoCb.offset;
			mpi->planes[2]=((char*)P) + P->componentInfoCr.offset;
			mpi->stride[1]=imgRect.right/2;
			mpi->stride[2]=imgRect.right/2;
		} 
		else 
		{
			// YV12
			mpi->planes[1]=((char*)P) + P->componentInfoCr.offset;
			mpi->planes[2]=((char*)P) + P->componentInfoCb.offset;
			mpi->stride[1]=imgRect.right/2;
			mpi->stride[2]=imgRect.right/2;
		}
		
		mpi->flags|=MP_IMGFLAG_DIRECT;
		get_image_done = 1;
		return VO_TRUE;
	}
	else 
	{ 
		// doesn't work yet
		if (mpi->num_planes != 1)
		{
			mp_msg(MSGT_VO, MSGL_ERR, "Quartz error: only 1 plane allowed in get_yuv_image for packed (%d) \n", mpi->num_planes);
			return VO_FALSE;
		}

		mpi->planes[0] = (char*)P;
		mpi->stride[0] = imgRect.right * 2;
		mpi->width=imgRect.right;
		mpi->flags|=MP_IMGFLAG_DIRECT;
		get_image_done = 1;
		return VO_TRUE;
	}
	return VO_FALSE;
}

static uint32_t control(uint32_t request, void *data, ...)
{
	switch (request)
	{
		case VOCTRL_PAUSE: return (int_pause=1);
		case VOCTRL_RESUME: return (int_pause=0);
		case VOCTRL_FULLSCREEN: vo_fs = (!(vo_fs)); window_fullscreen(); return VO_TRUE;
		case VOCTRL_ONTOP: vo_ontop = (!(vo_ontop)); window_ontop(); return VO_TRUE;
		case VOCTRL_QUERY_FORMAT: return query_format(*((uint32_t*)data));
		case VOCTRL_GET_IMAGE:
			switch (image_format)
			{
				case IMGFMT_YV12:
				case IMGFMT_IYUV:
				case IMGFMT_I420:
				case IMGFMT_UYVY:
				case IMGFMT_YUY2:
					return get_yuv_image(data);
					break;
				default:
					break;
			}
		case VOCTRL_DRAW_IMAGE:
			switch (image_format)
			{
				case IMGFMT_YV12:
				case IMGFMT_IYUV:
				case IMGFMT_I420:
				case IMGFMT_UYVY:
				case IMGFMT_YUY2:
					return draw_yuv_image(data);
					break;
				default:
					break;
			}
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

	long scale_X = FixDiv(Long2Fix(dstRect.right - dstRect.left),Long2Fix(imgRect.right));
	long scale_Y = FixDiv(Long2Fix(dstRect.bottom - dstRect.top),Long2Fix(imgRect.bottom));

	SetIdentityMatrix(&matrix);
	if (((dstRect.right - dstRect.left)   != imgRect.right) || ((dstRect.bottom - dstRect.right) != imgRect.bottom))
	{
		ScaleMatrix(&matrix, scale_X, scale_Y, 0, 0);
	      
		if (padding > 0)
		{
			TranslateMatrix(&matrix, Long2Fix(dstRect.left), Long2Fix(dstRect.top));
		}
	}
			
	SetDSequenceMatrix(seqId, &matrix);
}

void window_ontop()
{
	if(vo_ontop)
		SetWindowClass( theWindow, kUtilityWindowClass);
	else
		SetWindowClass( theWindow, kDocumentWindowClass);
}

void window_fullscreen()
{
	GDHandle deviceHdl;
	Rect deviceRect;

	//go fullscreen
	if(vo_fs)
	{
		HideMenuBar();

		//save old window size
 		if (!vo_quartz_fs)
			GetWindowPortBounds(theWindow, &oldWinRect);
		
		//hide mouse cursor
		HideCursor();
		
		//go fullscreen
		//ChangeWindowAttributes(theWindow, 0, kWindowResizableAttribute);
			
		MoveWindow (theWindow, 0, 0, 1);		
		SizeWindow(theWindow, device_width, device_height,1);

		vo_quartz_fs = 1;
	}
	else //go back to windowed mode
	{
		ShowMenuBar();

		//show mouse cursor
		ShowCursor();
		
		//revert window to previous setting
		//ChangeWindowAttributes(theWindow, kWindowResizableAttribute, 0);
			
		SizeWindow(theWindow, oldWinRect.right, oldWinRect.bottom,1);
		RepositionWindow(theWindow, NULL, kWindowCascadeOnMainScreen);

 		vo_quartz_fs = 0;
	}
	
	window_resized();
}
