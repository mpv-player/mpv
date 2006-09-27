/* Took WindowMaker implementation and adopted for MPlayer */


#include <X11/Xlib.h>
#include "ws.h"
#include "wsxdnd.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <X11/Xatom.h>

#include "../mp_msg.h"
#include "../help_mp.h"

#define XDND_VERSION 3L

Atom _XA_XdndAware;
Atom _XA_XdndEnter;
Atom _XA_XdndLeave;
Atom _XA_XdndDrop;
Atom _XA_XdndPosition;
Atom _XA_XdndStatus;
Atom _XA_XdndActionCopy;
Atom _XA_XdndSelection;
Atom _XA_XdndFinished;
Atom _XA_XdndTypeList;

Atom atom_support;

void wsXDNDInitialize(void)
{

    _XA_XdndAware = XInternAtom(wsDisplay, "XdndAware", False);
    _XA_XdndEnter = XInternAtom(wsDisplay, "XdndEnter", False);
    _XA_XdndLeave = XInternAtom(wsDisplay, "XdndLeave", False);
    _XA_XdndDrop = XInternAtom(wsDisplay, "XdndDrop", False);
    _XA_XdndPosition = XInternAtom(wsDisplay, "XdndPosition", False);
    _XA_XdndStatus = XInternAtom(wsDisplay, "XdndStatus", False);
    _XA_XdndActionCopy = XInternAtom(wsDisplay, "XdndActionCopy", False);
    _XA_XdndSelection = XInternAtom(wsDisplay, "XdndSelection", False);
    _XA_XdndFinished = XInternAtom(wsDisplay, "XdndFinished", False);
    _XA_XdndTypeList = XInternAtom(wsDisplay, "XdndTypeList", False);
}

void wsXDNDMakeAwareness(wsTWindow* window) {
    long int xdnd_version = XDND_VERSION;
    XChangeProperty (wsDisplay, window->WindowID, _XA_XdndAware, XA_ATOM,
            32, PropModeAppend, (char *)&xdnd_version, 1);
}

void wsXDNDClearAwareness(wsTWindow* window) {
    XDeleteProperty (wsDisplay, window->WindowID, _XA_XdndAware);
}

#define MAX_DND_FILES 64
Bool
wsXDNDProcessSelection(wsTWindow* wnd, XEvent *event)
{
    Atom ret_type;
    int ret_format;
    unsigned long ret_items;
    unsigned long remain_byte;
    char * delme;
    XEvent xevent;

    Window selowner = XGetSelectionOwner(wsDisplay,_XA_XdndSelection);

    XGetWindowProperty(wsDisplay, event->xselection.requestor,
            event->xselection.property,
            0, 65536, True, atom_support, &ret_type, &ret_format,
            &ret_items, &remain_byte, (unsigned char **)&delme);

    /*send finished*/
    memset (&xevent, 0, sizeof(xevent));
    xevent.xany.type = ClientMessage;
    xevent.xany.display = wsDisplay;
    xevent.xclient.window = selowner;
    xevent.xclient.message_type = _XA_XdndFinished;
    xevent.xclient.format = 32;
    XDND_FINISHED_TARGET_WIN(&xevent) = wnd->WindowID;
    XSendEvent(wsDisplay, selowner, 0, 0, &xevent);

    if (!delme){
      mp_msg( MSGT_GPLAYER,MSGL_WARN,MSGTR_WS_DDNothing );
      return False;
    }

    {
      /* Handle dropped files */
      char * retain = delme;
      char * files[MAX_DND_FILES];
      int num = 0;

      while(retain < delme + ret_items) {
	if (!strncmp(retain,"file:",5)) {
	  /* add more 2 chars while removing 5 is harmless */
	  retain+=5;
	}

	/* add the "retain" to the list */
	files[num++]=retain;


	/* now check for special characters */
	{
	  int newone = 0;
	  while(retain < (delme + ret_items)){
	    if(*retain == '\r' || *retain == '\n'){
	      *retain=0;
	      newone = 1;
	    } else {
	      if (newone)
		break;
	    }
	    retain++;
	  }
	}

	if (num >= MAX_DND_FILES)
	  break;
      }
	
      /* Handle the files */
      if(wnd->DandDHandler){
	wnd->DandDHandler(num,files);
      }
    }

    free(delme);
    return True;
}

Bool
wsXDNDProcessClientMessage(wsTWindow* wnd, XClientMessageEvent *event)
{
  /* test */
  /*{
    char * name = XGetAtomName(wsDisplay, event->message_type);
    printf("Got %s\n",name);
    XFree(name);
    }*/

  if (event->message_type == _XA_XdndEnter) {
    Atom ok = XInternAtom(wsDisplay, "text/uri-list", False);
    atom_support = None;
    if ((event->data.l[1] & 1) == 0){
      int index;
      for(index = 0; index <= 2 ; index++){
	if (event->data.l[2+index] == ok) {
	  atom_support = ok;
	}
      }
      if (atom_support == None) {
	mp_msg( MSGT_GPLAYER,MSGL_WARN,MSGTR_WS_NotAFile );
      }
    } else {
      /* need to check the whole list here */
      unsigned long ret_left = 1;
      int offset = 0;
      Atom* ret_buff;
      Atom ret_type;
      int ret_format;
      unsigned long ret_items;

      /* while there is data left...*/
      while(ret_left && atom_support == None){
	XGetWindowProperty(wsDisplay,event->data.l[0],_XA_XdndTypeList,
			   offset,256,False,XA_ATOM,&ret_type,
			   &ret_format,&ret_items,&ret_left,
			   (unsigned char**)&ret_buff);
	
	/* sanity checks...*/
	if(ret_buff == NULL || ret_type != XA_ATOM || ret_format != 8*sizeof(Atom)){
	  XFree(ret_buff);
	  break;
	}
	/* now chek what we've got */
	{
	  int i;
	  for(i=0; i<ret_items; i++){
	    if(ret_buff[i] == ok){
	      atom_support = ok;
	      break;
	    }
	  }
	}
	/* maybe next time ... */
	XFree(ret_buff);
	offset += 256;
      }
    }
    return True;
  }
  
  if (event->message_type == _XA_XdndLeave) {
    return True;
  }
  
  if (event->message_type == _XA_XdndDrop) {
    if (event->data.l[0] != XGetSelectionOwner(wsDisplay, _XA_XdndSelection)){
      puts("Wierd selection owner... QT?");
    }
    if (atom_support != None) {
      XConvertSelection(wsDisplay, _XA_XdndSelection, atom_support,
			_XA_XdndSelection, event->window,
			CurrentTime);
    }
    return True;
  }
  
  if (event->message_type == _XA_XdndPosition) {
    Window srcwin = event->data.l[0];
    if (atom_support == None){
      return True;
    }

    /* send response */
    {
      XEvent xevent;
      memset (&xevent, 0, sizeof(xevent));
      xevent.xany.type = ClientMessage;
      xevent.xany.display = wsDisplay;
      xevent.xclient.window = srcwin;
      xevent.xclient.message_type = _XA_XdndStatus;
      xevent.xclient.format = 32; 
      
      XDND_STATUS_TARGET_WIN (&xevent) = event->window;
      XDND_STATUS_WILL_ACCEPT_SET (&xevent, True);
      XDND_STATUS_WANT_POSITION_SET(&xevent, True);
      /* actually need smth real here */
      XDND_STATUS_RECT_SET(&xevent, 0, 0, 1024,768);
      XDND_STATUS_ACTION(&xevent) = _XA_XdndActionCopy;
      
      XSendEvent(wsDisplay, srcwin, 0, 0, &xevent);
    }
    return True;
  }
  
  return False;
}
