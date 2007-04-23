
#ifndef _XDND_H_
#define _XDND_H_


void wsXDNDInitialize(void);
Bool wsXDNDProcessSelection(wsTWindow* wnd,XEvent *event);
Bool wsXDNDProcessClientMessage(wsTWindow* wnd, XClientMessageEvent *event);
void wsXDNDMakeAwareness(wsTWindow* window);
void wsXDNDClearAwareness(wsTWindow* window);

/* header was ripped from xdnd's example on its page */

#define XDND_THREE 3
#define XDND_ENTER_SOURCE_WIN(e)	((e)->xclient.data.l[0])
#define XDND_ENTER_THREE_TYPES(e)	(((e)->xclient.data.l[1] & 0x1UL) == 0)
#define XDND_ENTER_THREE_TYPES_SET(e,b)	(e)->xclient.data.l[1] = ((e)->xclient.data.l[1] & ~0x1UL) | (((b) == 0) ? 0 : 0x1UL)
#define XDND_ENTER_VERSION(e)		((e)->xclient.data.l[1] >> 24)
#define XDND_ENTER_VERSION_SET(e,v)	(e)->xclient.data.l[1] = ((e)->xclient.data.l[1] & ~(0xFF << 24)) | ((v) << 24)
#define XDND_ENTER_TYPE(e,i)		((e)->xclient.data.l[2 + (i)])	/* i => (0, 1, 2) */

/* XdndPosition */
#define XDND_POSITION_SOURCE_WIN(e)	((e)->xclient.data.l[0])
#define XDND_POSITION_ROOT_X(e)		((e)->xclient.data.l[2] >> 16)
#define XDND_POSITION_ROOT_Y(e)		((e)->xclient.data.l[2] & 0xFFFFUL)
#define XDND_POSITION_ROOT_SET(e,x,y)	(e)->xclient.data.l[2]  = ((x) << 16) | ((y) & 0xFFFFUL)
#define XDND_POSITION_TIME(e)		((e)->xclient.data.l[3])
#define XDND_POSITION_ACTION(e)		((e)->xclient.data.l[4])

/* XdndStatus */
#define XDND_STATUS_TARGET_WIN(e)	((e)->xclient.data.l[0])
#define XDND_STATUS_WILL_ACCEPT(e)	((e)->xclient.data.l[1] & 0x1L)
#define XDND_STATUS_WILL_ACCEPT_SET(e,b) (e)->xclient.data.l[1] = ((e)->xclient.data.l[1] & ~0x1UL) | (((b) == 0) ? 0 : 0x1UL)
#define XDND_STATUS_WANT_POSITION(e)	((e)->xclient.data.l[1] & 0x2UL)
#define XDND_STATUS_WANT_POSITION_SET(e,b) (e)->xclient.data.l[1] = ((e)->xclient.data.l[1] & ~0x2UL) | (((b) == 0) ? 0 : 0x2UL)
#define XDND_STATUS_RECT_X(e)		((e)->xclient.data.l[2] >> 16)
#define XDND_STATUS_RECT_Y(e)		((e)->xclient.data.l[2] & 0xFFFFL)
#define XDND_STATUS_RECT_WIDTH(e)	((e)->xclient.data.l[3] >> 16)
#define XDND_STATUS_RECT_HEIGHT(e)	((e)->xclient.data.l[3] & 0xFFFFL)
#define XDND_STATUS_RECT_SET(e,x,y,w,h)	{(e)->xclient.data.l[2] = ((x) << 16) | ((y) & 0xFFFFUL); (e)->xclient.data.l[3] = ((w) << 16) | ((h) & 0xFFFFUL); }
#define XDND_STATUS_ACTION(e)		((e)->xclient.data.l[4])

/* XdndLeave */
#define XDND_LEAVE_SOURCE_WIN(e)	((e)->xclient.data.l[0])

/* XdndDrop */
#define XDND_DROP_SOURCE_WIN(e)		((e)->xclient.data.l[0])
#define XDND_DROP_TIME(e)		((e)->xclient.data.l[2])

/* XdndFinished */
#define XDND_FINISHED_TARGET_WIN(e)	((e)->xclient.data.l[0])

#endif
