#ifndef _WINUSER_
#define _WINUSER_

#ifndef RC_INVOKED
#include <stdarg.h>
#endif

#ifdef __cplusplus
extern "C" {
#endif

#include "pshpack1.h"

/* flags for HIGHCONTRAST dwFlags field */
#define HCF_HIGHCONTRASTON  0x00000001
#define HCF_AVAILABLE       0x00000002
#define HCF_HOTKEYACTIVE    0x00000004
#define HCF_CONFIRMHOTKEY   0x00000008
#define HCF_HOTKEYSOUND     0x00000010
#define HCF_INDICATOR       0x00000020
#define HCF_HOTKEYAVAILABLE 0x00000040

typedef struct tagHIGHCONTRASTA
{
    UINT  cbSize;
    DWORD   dwFlags;
    LPSTR   lpszDefaultScheme;
} HIGHCONTRASTA, *LPHIGHCONTRASTA;

typedef struct tagHIGHCONTRASTW
{
    UINT  cbSize;
    DWORD   dwFlags;
    LPWSTR  lpszDefaultScheme;
} HIGHCONTRASTW, *LPHIGHCONTRASTW;

DECL_WINELIB_TYPE_AW(HIGHCONTRAST)
DECL_WINELIB_TYPE_AW(LPHIGHCONTRAST)

typedef struct
{
    UINT  message;
    UINT  paramL;
    UINT  paramH;
    DWORD   time;
    HWND  hwnd;
} EVENTMSG, *LPEVENTMSG;


    /* Mouse hook structure */

typedef struct
{
    POINT pt;
    HWND  hwnd;
    UINT  wHitTestCode;
    DWORD   dwExtraInfo;
} MOUSEHOOKSTRUCT, *PMOUSEHOOKSTRUCT, *LPMOUSEHOOKSTRUCT;


    /* Hardware hook structure */

typedef struct
{
    HWND    hWnd;
    UINT    wMessage;
    WPARAM  wParam;
    LPARAM    lParam;
} HARDWAREHOOKSTRUCT, *LPHARDWAREHOOKSTRUCT;


  /* Debug hook structure */

typedef struct
{
    DWORD       idThread;
    DWORD       idThreadInstaller;
    LPARAM      lParam;
    WPARAM    wParam;
    INT       code;
} DEBUGHOOKINFO, *LPDEBUGHOOKINFO;

#define HKL_PREV   0
#define HKL_NEXT   1

#define KLF_ACTIVATE       0x00000001
#define KLF_SUBSTITUTE_OK  0x00000002
#define KLF_UNLOADPREVIOUS 0x00000004
#define KLF_REORDER        0x00000008
#define KLF_REPLACELANG    0x00000010
#define KLF_NOTELLSHELL    0x00000080

#define KL_NAMELENGTH      9

  /***** Dialogs *****/
#ifdef FSHIFT
/* Gcc on Solaris has a version of this that we don't care about.  */
#undef FSHIFT
#endif

#define	FVIRTKEY	TRUE          /* Assumed to be == TRUE */
#define	FNOINVERT	0x02
#define	FSHIFT		0x04
#define	FCONTROL	0x08
#define	FALT		0x10


typedef struct tagANIMATIONINFO
{
       UINT          cbSize;
       INT           iMinAnimate;
} ANIMATIONINFO, *LPANIMATIONINFO;

typedef struct tagNMHDR
{
    HWND  hwndFrom;
    UINT  idFrom;
    UINT  code;
} NMHDR, *LPNMHDR;

typedef struct
{
	UINT	cbSize;
	INT	iTabLength;
	INT	iLeftMargin;
	INT	iRightMargin;
	UINT	uiLengthDrawn;
} DRAWTEXTPARAMS,*LPDRAWTEXTPARAMS;

#define WM_USER             0x0400

#define DT_EDITCONTROL      0x00002000
#define DT_PATH_ELLIPSIS    0x00004000
#define DT_END_ELLIPSIS     0x00008000
#define DT_MODIFYSTRING     0x00010000
#define DT_RTLREADING       0x00020000
#define DT_WORD_ELLIPSIS    0x00040000

typedef struct
{
   LPARAM   lParam;
   WPARAM16 wParam;
   UINT16   message;
   HWND16   hwnd;
} CWPSTRUCT16, *LPCWPSTRUCT16;

typedef struct
{
  LPARAM        lParam;
  WPARAM      wParam;
  UINT        message;
  HWND        hwnd;
} CWPSTRUCT, *LPCWPSTRUCT;



typedef struct
{
  LRESULT       lResult;
  LPARAM        lParam;
  WPARAM16      wParam;
  DWORD         message;
  HWND16        hwnd;
} CWPRETSTRUCT16, *LPCWPRETSTRUCT16;

typedef struct
{
  LRESULT       lResult;
  LPARAM        lParam;
  WPARAM      wParam;
  DWORD         message;
  HWND        hwnd;
} CWPRETSTRUCT, *LPCWPRETSTRUCT;

typedef struct
{
    UINT   length;
    UINT   flags;
    UINT   showCmd;
    POINT  ptMinPosition WINE_PACKED;
    POINT  ptMaxPosition WINE_PACKED;
    RECT   rcNormalPosition WINE_PACKED;
} WINDOWPLACEMENT, *LPWINDOWPLACEMENT;


  /* WINDOWPLACEMENT flags */
#define WPF_SETMINPOSITION      0x0001
#define WPF_RESTORETOMAXIMIZED  0x0002

/***** Dialogs *****/

  /* cbWndExtra bytes for dialog class */
#define DLGWINDOWEXTRA      30

/* Button control styles */
#define BS_PUSHBUTTON          0x00000000L
#define BS_DEFPUSHBUTTON       0x00000001L
#define BS_CHECKBOX            0x00000002L
#define BS_AUTOCHECKBOX        0x00000003L
#define BS_RADIOBUTTON         0x00000004L
#define BS_3STATE              0x00000005L
#define BS_AUTO3STATE          0x00000006L
#define BS_GROUPBOX            0x00000007L
#define BS_USERBUTTON          0x00000008L
#define BS_AUTORADIOBUTTON     0x00000009L
#define BS_OWNERDRAW           0x0000000BL
#define BS_LEFTTEXT            0x00000020L

#define BS_TEXT                0x00000000L
#define BS_ICON                0x00000040L
#define BS_BITMAP              0x00000080L
#define BS_LEFT                0x00000100L
#define BS_RIGHT               0x00000200L
#define BS_CENTER              0x00000300L
#define BS_TOP                 0x00000400L
#define BS_BOTTOM              0x00000800L
#define BS_VCENTER             0x00000C00L
#define BS_PUSHLIKE            0x00001000L
#define BS_MULTILINE           0x00002000L
#define BS_NOTIFY              0x00004000L
#define BS_FLAT                0x00008000L

  /* Dialog styles */
#define DS_ABSALIGN		0x0001
#define DS_SYSMODAL		0x0002
#define DS_3DLOOK		0x0004	/* win95 */
#define DS_FIXEDSYS		0x0008	/* win95 */
#define DS_NOFAILCREATE		0x0010	/* win95 */
#define DS_LOCALEDIT		0x0020
#define DS_SETFONT		0x0040
#define DS_MODALFRAME		0x0080
#define DS_NOIDLEMSG		0x0100
#define DS_SETFOREGROUND	0x0200	/* win95 */
#define DS_CONTROL		0x0400	/* win95 */
#define DS_CENTER		0x0800	/* win95 */
#define DS_CENTERMOUSE		0x1000	/* win95 */
#define DS_CONTEXTHELP		0x2000	/* win95 */


  /* Dialog messages */
#define DM_GETDEFID         (WM_USER+0)
#define DM_SETDEFID         (WM_USER+1)

#define DC_HASDEFID         0x534b

/* Owner draw control types */
#define ODT_MENU        1
#define ODT_LISTBOX     2
#define ODT_COMBOBOX    3
#define ODT_BUTTON      4
#define ODT_STATIC      5

/* Owner draw actions */
#define ODA_DRAWENTIRE  0x0001
#define ODA_SELECT      0x0002
#define ODA_FOCUS       0x0004

/* Owner draw state */
#define ODS_SELECTED    0x0001
#define ODS_GRAYED      0x0002
#define ODS_DISABLED    0x0004
#define ODS_CHECKED     0x0008
#define ODS_FOCUS       0x0010
#define ODS_COMBOBOXEDIT 0x1000
#define ODS_HOTLIGHT    0x0040
#define ODS_INACTIVE    0x0080

/* Edit control styles */
#define ES_LEFT         0x00000000
#define ES_CENTER       0x00000001
#define ES_RIGHT        0x00000002
#define ES_MULTILINE    0x00000004
#define ES_UPPERCASE    0x00000008
#define ES_LOWERCASE    0x00000010
#define ES_PASSWORD     0x00000020
#define ES_AUTOVSCROLL  0x00000040
#define ES_AUTOHSCROLL  0x00000080
#define ES_NOHIDESEL    0x00000100
#define ES_OEMCONVERT   0x00000400
#define ES_READONLY     0x00000800
#define ES_WANTRETURN   0x00001000
#define ES_NUMBER       0x00002000

/* OEM Resource Ordinal Numbers */
#define OBM_CLOSED          32731
#define OBM_RADIOCHECK      32732
#define OBM_TRTYPE          32733
#define OBM_LFARROWI        32734
#define OBM_RGARROWI        32735
#define OBM_DNARROWI        32736
#define OBM_UPARROWI        32737
#define OBM_COMBO           32738
#define OBM_MNARROW         32739
#define OBM_LFARROWD        32740
#define OBM_RGARROWD        32741
#define OBM_DNARROWD        32742
#define OBM_UPARROWD        32743
#define OBM_RESTORED        32744
#define OBM_ZOOMD           32745
#define OBM_REDUCED         32746
#define OBM_RESTORE         32747
#define OBM_ZOOM            32748
#define OBM_REDUCE          32749
#define OBM_LFARROW         32750
#define OBM_RGARROW         32751
#define OBM_DNARROW         32752
#define OBM_UPARROW         32753
#define OBM_CLOSE           32754
#define OBM_OLD_RESTORE     32755
#define OBM_OLD_ZOOM        32756
#define OBM_OLD_REDUCE      32757
#define OBM_BTNCORNERS      32758
#define OBM_CHECKBOXES      32759
#define OBM_CHECK           32760
#define OBM_BTSIZE          32761
#define OBM_OLD_LFARROW     32762
#define OBM_OLD_RGARROW     32763
#define OBM_OLD_DNARROW     32764
#define OBM_OLD_UPARROW     32765
#define OBM_SIZE            32766
#define OBM_OLD_CLOSE       32767

#define OCR_BUMMER	    100
#define OCR_DRAGOBJECT	    101

#define OCR_NORMAL          32512
#define OCR_IBEAM           32513
#define OCR_WAIT            32514
#define OCR_CROSS           32515
#define OCR_UP              32516
#define OCR_SIZE            32640
#define OCR_ICON            32641
#define OCR_SIZENWSE        32642
#define OCR_SIZENESW        32643
#define OCR_SIZEWE          32644
#define OCR_SIZENS          32645
#define OCR_SIZEALL         32646
#define OCR_ICOCUR          32647
#define OCR_NO              32648
#define OCR_APPSTARTING     32650
#define OCR_HELP            32651  /* only defined in wine */

#define OIC_SAMPLE          32512
#define OIC_HAND            32513
#define OIC_QUES            32514
#define OIC_BANG            32515
#define OIC_NOTE            32516
#define OIC_PORTRAIT        32517
#define OIC_LANDSCAPE       32518
#define OIC_WINEICON        32519
#define OIC_FOLDER          32520
#define OIC_FOLDER2         32521
#define OIC_FLOPPY          32522
#define OIC_CDROM           32523
#define OIC_HDISK           32524
#define OIC_NETWORK         32525

#define COLOR_SCROLLBAR		    0
#define COLOR_BACKGROUND	    1
#define COLOR_ACTIVECAPTION	    2
#define COLOR_INACTIVECAPTION	    3
#define COLOR_MENU		    4
#define COLOR_WINDOW		    5
#define COLOR_WINDOWFRAME	    6
#define COLOR_MENUTEXT		    7
#define COLOR_WINDOWTEXT	    8
#define COLOR_CAPTIONTEXT  	    9
#define COLOR_ACTIVEBORDER	   10
#define COLOR_INACTIVEBORDER	   11
#define COLOR_APPWORKSPACE	   12
#define COLOR_HIGHLIGHT		   13
#define COLOR_HIGHLIGHTTEXT	   14
#define COLOR_BTNFACE              15
#define COLOR_BTNSHADOW            16
#define COLOR_GRAYTEXT             17
#define COLOR_BTNTEXT		   18
#define COLOR_INACTIVECAPTIONTEXT  19
#define COLOR_BTNHIGHLIGHT         20
/* win95 colors */
#define COLOR_3DDKSHADOW           21
#define COLOR_3DLIGHT              22
#define COLOR_INFOTEXT             23
#define COLOR_INFOBK               24
#define COLOR_DESKTOP              COLOR_BACKGROUND
#define COLOR_3DFACE               COLOR_BTNFACE
#define COLOR_3DSHADOW             COLOR_BTNSHADOW
#define COLOR_3DHIGHLIGHT          COLOR_BTNHIGHLIGHT
#define COLOR_3DHILIGHT            COLOR_BTNHIGHLIGHT
#define COLOR_BTNHILIGHT           COLOR_BTNHIGHLIGHT
/* win98 colors */
#define COLOR_ALTERNATEBTNFACE         25  /* undocumented, constant's name unknown */
#define COLOR_HOTLIGHT                 26
#define COLOR_GRADIENTACTIVECAPTION    27
#define COLOR_GRADIENTINACTIVECAPTION  28

  /* WM_CTLCOLOR values */
#define CTLCOLOR_MSGBOX             0
#define CTLCOLOR_EDIT               1
#define CTLCOLOR_LISTBOX            2
#define CTLCOLOR_BTN                3
#define CTLCOLOR_DLG                4
#define CTLCOLOR_SCROLLBAR          5
#define CTLCOLOR_STATIC             6

/* Edit control messages */
#define EM_GETSEL                0x00b0
#define EM_SETSEL                0x00b1
#define EM_GETRECT               0x00b2
#define EM_SETRECT               0x00b3
#define EM_SETRECTNP             0x00b4
#define EM_SCROLL                0x00b5
#define EM_LINESCROLL            0x00b6
#define EM_SCROLLCARET           0x00b7
#define EM_GETMODIFY             0x00b8
#define EM_SETMODIFY             0x00b9
#define EM_GETLINECOUNT          0x00ba
#define EM_LINEINDEX             0x00bb
#define EM_SETHANDLE             0x00bc
#define EM_GETHANDLE             0x00bd
#define EM_GETTHUMB              0x00be
/* FIXME : missing from specs 0x00bf and 0x00c0 */
#define EM_LINELENGTH            0x00c1
#define EM_REPLACESEL            0x00c2
/* FIXME : missing from specs 0x00c3 */
#define EM_GETLINE               0x00c4
#define EM_LIMITTEXT             0x00c5
#define EM_CANUNDO               0x00c6
#define EM_UNDO                  0x00c7
#define EM_FMTLINES              0x00c8
#define EM_LINEFROMCHAR          0x00c9
/* FIXME : missing from specs 0x00ca */
#define EM_SETTABSTOPS           0x00cb
#define EM_SETPASSWORDCHAR       0x00cc
#define EM_EMPTYUNDOBUFFER       0x00cd
#define EM_GETFIRSTVISIBLELINE   0x00ce
#define EM_SETREADONLY           0x00cf
#define EM_SETWORDBREAKPROC      0x00d0
#define EM_GETWORDBREAKPROC      0x00d1
#define EM_GETPASSWORDCHAR       0x00d2
#define EM_SETMARGINS            0x00d3
#define EM_GETMARGINS            0x00d4
#define EM_GETLIMITTEXT          0x00d5
#define EM_POSFROMCHAR           0x00d6
#define EM_CHARFROMPOS           0x00d7
/* a name change since win95 */
#define EM_SETLIMITTEXT          EM_LIMITTEXT

/* EDITWORDBREAKPROC code values */
#define WB_LEFT         0
#define WB_RIGHT        1
#define WB_ISDELIMITER  2

/* Edit control notification codes */
#define EN_SETFOCUS     0x0100
#define EN_KILLFOCUS    0x0200
#define EN_CHANGE       0x0300
#define EN_UPDATE       0x0400
#define EN_ERRSPACE     0x0500
#define EN_MAXTEXT      0x0501
#define EN_HSCROLL      0x0601
#define EN_VSCROLL      0x0602

/* New since win95 : EM_SETMARGIN parameters */
#define EC_LEFTMARGIN	0x0001
#define EC_RIGHTMARGIN	0x0002
#define EC_USEFONTINFO	0xffff


/* Messages */

  /* WM_GETDLGCODE values */


#define WM_NULL                 0x0000
#define WM_CREATE               0x0001
#define WM_DESTROY              0x0002
#define WM_MOVE                 0x0003
#define WM_SIZEWAIT             0x0004
#define WM_SIZE                 0x0005
#define WM_ACTIVATE             0x0006
#define WM_SETFOCUS             0x0007
#define WM_KILLFOCUS            0x0008
#define WM_SETVISIBLE           0x0009
#define WM_ENABLE               0x000a
#define WM_SETREDRAW            0x000b
#define WM_SETTEXT              0x000c
#define WM_GETTEXT              0x000d
#define WM_GETTEXTLENGTH        0x000e
#define WM_PAINT                0x000f
#define WM_CLOSE                0x0010
#define WM_QUERYENDSESSION      0x0011
#define WM_QUIT                 0x0012
#define WM_QUERYOPEN            0x0013
#define WM_ERASEBKGND           0x0014
#define WM_SYSCOLORCHANGE       0x0015
#define WM_ENDSESSION           0x0016
#define WM_SYSTEMERROR          0x0017
#define WM_SHOWWINDOW           0x0018
#define WM_CTLCOLOR             0x0019
#define WM_WININICHANGE         0x001a
#define WM_SETTINGCHANGE        WM_WININICHANGE
#define WM_DEVMODECHANGE        0x001b
#define WM_ACTIVATEAPP          0x001c
#define WM_FONTCHANGE           0x001d
#define WM_TIMECHANGE           0x001e
#define WM_CANCELMODE           0x001f
#define WM_SETCURSOR            0x0020
#define WM_MOUSEACTIVATE        0x0021
#define WM_CHILDACTIVATE        0x0022
#define WM_QUEUESYNC            0x0023
#define WM_GETMINMAXINFO        0x0024

#define WM_PAINTICON            0x0026
#define WM_ICONERASEBKGND       0x0027
#define WM_NEXTDLGCTL           0x0028
#define WM_ALTTABACTIVE         0x0029
#define WM_SPOOLERSTATUS        0x002a
#define WM_DRAWITEM             0x002b
#define WM_MEASUREITEM          0x002c
#define WM_DELETEITEM           0x002d
#define WM_VKEYTOITEM           0x002e
#define WM_CHARTOITEM           0x002f
#define WM_SETFONT              0x0030
#define WM_GETFONT              0x0031
#define WM_SETHOTKEY            0x0032
#define WM_GETHOTKEY            0x0033
#define WM_FILESYSCHANGE        0x0034
#define WM_ISACTIVEICON         0x0035
#define WM_QUERYPARKICON        0x0036
#define WM_QUERYDRAGICON        0x0037
#define WM_QUERYSAVESTATE       0x0038
#define WM_COMPAREITEM          0x0039
#define WM_TESTING              0x003a

#define WM_OTHERWINDOWCREATED	0x003c
#define WM_OTHERWINDOWDESTROYED	0x003d
#define WM_ACTIVATESHELLWINDOW	0x003e

#define WM_COMPACTING		0x0041

#define WM_COMMNOTIFY		0x0044
#define WM_WINDOWPOSCHANGING 	0x0046
#define WM_WINDOWPOSCHANGED 	0x0047
#define WM_POWER		0x0048

  /* Win32 4.0 messages */
#define WM_COPYDATA		0x004a
#define WM_CANCELJOURNAL	0x004b
#define WM_NOTIFY		0x004e
#define WM_HELP			0x0053
#define WM_NOTIFYFORMAT		0x0055

#define WM_CONTEXTMENU		0x007b
#define WM_STYLECHANGING 	0x007c
#define WM_STYLECHANGED		0x007d
#define WM_DISPLAYCHANGE        0x007e
#define WM_GETICON		0x007f
#define WM_SETICON		0x0080

  /* Non-client system messages */
#define WM_NCCREATE         0x0081
#define WM_NCDESTROY        0x0082
#define WM_NCCALCSIZE       0x0083
#define WM_NCHITTEST        0x0084
#define WM_NCPAINT          0x0085
#define WM_NCACTIVATE       0x0086

#define WM_GETDLGCODE	    0x0087
#define WM_SYNCPAINT	    0x0088
#define WM_SYNCTASK	    0x0089

  /* Non-client mouse messages */
#define WM_NCMOUSEMOVE      0x00a0
#define WM_NCLBUTTONDOWN    0x00a1
#define WM_NCLBUTTONUP      0x00a2
#define WM_NCLBUTTONDBLCLK  0x00a3
#define WM_NCRBUTTONDOWN    0x00a4
#define WM_NCRBUTTONUP      0x00a5
#define WM_NCRBUTTONDBLCLK  0x00a6
#define WM_NCMBUTTONDOWN    0x00a7
#define WM_NCMBUTTONUP      0x00a8
#define WM_NCMBUTTONDBLCLK  0x00a9

  /* Keyboard messages */
#define WM_KEYDOWN          0x0100
#define WM_KEYUP            0x0101
#define WM_CHAR             0x0102
#define WM_DEADCHAR         0x0103
#define WM_SYSKEYDOWN       0x0104
#define WM_SYSKEYUP         0x0105
#define WM_SYSCHAR          0x0106
#define WM_SYSDEADCHAR      0x0107
#define WM_KEYFIRST         WM_KEYDOWN
#define WM_KEYLAST          0x0108

/* Win32 4.0 messages for IME */
#define WM_IME_STARTCOMPOSITION     0x010d
#define WM_IME_ENDCOMPOSITION       0x010e
#define WM_IME_COMPOSITION          0x010f
#define WM_IME_KEYLAST              0x010f

#define WM_INITDIALOG       0x0110 
#define WM_COMMAND          0x0111
#define WM_SYSCOMMAND       0x0112
#define WM_TIMER	    0x0113
#define WM_SYSTIMER	    0x0118

  /* scroll messages */
#define WM_HSCROLL          0x0114
#define WM_VSCROLL          0x0115

/* Menu messages */
#define WM_INITMENU         0x0116
#define WM_INITMENUPOPUP    0x0117

#define WM_MENUSELECT       0x011F
#define WM_MENUCHAR         0x0120
#define WM_ENTERIDLE        0x0121

#define WM_LBTRACKPOINT     0x0131

  /* Win32 CTLCOLOR messages */
#define WM_CTLCOLORMSGBOX    0x0132
#define WM_CTLCOLOREDIT      0x0133
#define WM_CTLCOLORLISTBOX   0x0134
#define WM_CTLCOLORBTN       0x0135
#define WM_CTLCOLORDLG       0x0136
#define WM_CTLCOLORSCROLLBAR 0x0137
#define WM_CTLCOLORSTATIC    0x0138

  /* Mouse messages */
#define WM_MOUSEMOVE	    0x0200
#define WM_LBUTTONDOWN	    0x0201
#define WM_LBUTTONUP	    0x0202
#define WM_LBUTTONDBLCLK    0x0203
#define WM_RBUTTONDOWN	    0x0204
#define WM_RBUTTONUP	    0x0205
#define WM_RBUTTONDBLCLK    0x0206
#define WM_MBUTTONDOWN	    0x0207
#define WM_MBUTTONUP	    0x0208
#define WM_MBUTTONDBLCLK    0x0209
#define WM_MOUSEWHEEL       0x020A
#define WM_MOUSEFIRST	    WM_MOUSEMOVE


#define WM_MOUSELAST	    WM_MOUSEWHEEL
 
#define WHEEL_DELTA      120
#define WHEEL_PAGESCROLL  (UINT_MAX)
#define WM_PARENTNOTIFY     0x0210
#define WM_ENTERMENULOOP    0x0211
#define WM_EXITMENULOOP     0x0212
#define WM_NEXTMENU	    0x0213

  /* Win32 4.0 messages */
#define WM_SIZING	    0x0214
#define WM_CAPTURECHANGED   0x0215
#define WM_MOVING	    0x0216

  /* MDI messages */
#define WM_MDICREATE	    0x0220
#define WM_MDIDESTROY	    0x0221
#define WM_MDIACTIVATE	    0x0222
#define WM_MDIRESTORE	    0x0223
#define WM_MDINEXT	    0x0224
#define WM_MDIMAXIMIZE	    0x0225
#define WM_MDITILE	    0x0226
#define WM_MDICASCADE	    0x0227
#define WM_MDIICONARRANGE   0x0228
#define WM_MDIGETACTIVE     0x0229
#define WM_MDIREFRESHMENU   0x0234

  /* D&D messages */
#define WM_DROPOBJECT	    0x022A
#define WM_QUERYDROPOBJECT  0x022B
#define WM_BEGINDRAG	    0x022C
#define WM_DRAGLOOP	    0x022D
#define WM_DRAGSELECT	    0x022E
#define WM_DRAGMOVE	    0x022F
#define WM_MDISETMENU	    0x0230

#define WM_ENTERSIZEMOVE    0x0231
#define WM_EXITSIZEMOVE     0x0232
#define WM_DROPFILES	    0x0233


/* Win32 4.0 messages for IME */
#define WM_IME_SETCONTEXT           0x0281
#define WM_IME_NOTIFY               0x0282
#define WM_IME_CONTROL              0x0283
#define WM_IME_COMPOSITIONFULL      0x0284
#define WM_IME_SELECT               0x0285
#define WM_IME_CHAR                 0x0286
/* Win32 5.0 messages for IME */
#define WM_IME_REQUEST              0x0288

/* Win32 4.0 messages for IME */
#define WM_IME_KEYDOWN              0x0290
#define WM_IME_KEYUP                0x0291

/* Clipboard command messages */
#define WM_CUT               0x0300
#define WM_COPY              0x0301
#define WM_PASTE             0x0302
#define WM_CLEAR             0x0303
#define WM_UNDO              0x0304

/* Clipboard owner messages */
#define WM_RENDERFORMAT      0x0305
#define WM_RENDERALLFORMATS  0x0306
#define WM_DESTROYCLIPBOARD  0x0307

/* Clipboard viewer messages */
#define WM_DRAWCLIPBOARD     0x0308
#define WM_PAINTCLIPBOARD    0x0309
#define WM_VSCROLLCLIPBOARD  0x030A
#define WM_SIZECLIPBOARD     0x030B
#define WM_ASKCBFORMATNAME   0x030C
#define WM_CHANGECBCHAIN     0x030D
#define WM_HSCROLLCLIPBOARD  0x030E

#define WM_QUERYNEWPALETTE   0x030F
#define WM_PALETTEISCHANGING 0x0310
#define WM_PALETTECHANGED    0x0311
#define WM_HOTKEY	     0x0312

#define WM_PRINT             0x0317
#define WM_PRINTCLIENT       0x0318

  /* FIXME: This does not belong to any libwine interface header */
  /* MFC messages [360-38f] */

#define WM_QUERYAFXWNDPROC  0x0360
#define WM_SIZEPARENT       0x0361
#define WM_SETMESSAGESTRING 0x0362
#define WM_IDLEUPDATECMDUI  0x0363 
#define WM_INITIALUPDATE    0x0364
#define WM_COMMANDHELP      0x0365
#define WM_HELPHITTEST      0x0366
#define WM_EXITHELPMODE     0x0367
#define WM_RECALCPARENT     0x0368
#define WM_SIZECHILD        0x0369
#define WM_KICKIDLE         0x036A 
#define WM_QUERYCENTERWND   0x036B
#define WM_DISABLEMODAL     0x036C
#define WM_FLOATSTATUS      0x036D 
#define WM_ACTIVATETOPLEVEL 0x036E 
#define WM_QUERY3DCONTROLS  0x036F 
#define WM_SOCKET_NOTIFY    0x0373
#define WM_SOCKET_DEAD      0x0374
#define WM_POPMESSAGESTRING 0x0375
#define WM_OCC_LOADFROMSTREAM           0x0376
#define WM_OCC_LOADFROMSTORAGE          0x0377
#define WM_OCC_INITNEW                  0x0378
#define WM_OCC_LOADFROMSTREAM_EX        0x037A
#define WM_OCC_LOADFROMSTORAGE_EX       0x037B
#define WM_QUEUE_SENTINEL   0x0379

#define WM_PENWINFIRST      0x0380
#define WM_PENWINLAST       0x038F

/* end of MFC messages */

/* FIXME: The following two lines do not belong to any libwine interface header */
#define WM_COALESCE_FIRST    0x0390
#define WM_COALESCE_LAST     0x039F

#define WM_APP               0x8000


#define DLGC_WANTARROWS      0x0001
#define DLGC_WANTTAB         0x0002
#define DLGC_WANTALLKEYS     0x0004
#define DLGC_WANTMESSAGE     0x0004
#define DLGC_HASSETSEL       0x0008
#define DLGC_DEFPUSHBUTTON   0x0010
#define DLGC_UNDEFPUSHBUTTON 0x0020
#define DLGC_RADIOBUTTON     0x0040
#define DLGC_WANTCHARS       0x0080
#define DLGC_STATIC          0x0100
#define DLGC_BUTTON          0x2000

/* Standard dialog button IDs */
#define IDOK                1
#define IDCANCEL            2
#define IDABORT             3
#define IDRETRY             4
#define IDIGNORE            5
#define IDYES               6
#define IDNO                7
#define IDCLOSE             8
#define IDHELP              9      

/****** Window classes ******/

typedef struct tagCREATESTRUCTA
{
    LPVOID      lpCreateParams;
    HINSTANCE hInstance;
    HMENU     hMenu;
    HWND      hwndParent;
    INT       cy;
    INT       cx;
    INT       y;
    INT       x;
    LONG        style;
    LPCSTR      lpszName;
    LPCSTR      lpszClass;
    DWORD       dwExStyle;
} CREATESTRUCTA, *LPCREATESTRUCTA;

typedef struct
{
    LPVOID      lpCreateParams;
    HINSTANCE hInstance;
    HMENU     hMenu;
    HWND      hwndParent;
    INT       cy;
    INT       cx;
    INT       y;
    INT       x;
    LONG        style;
    LPCWSTR     lpszName;
    LPCWSTR     lpszClass;
    DWORD       dwExStyle;
} CREATESTRUCTW, *LPCREATESTRUCTW;

DECL_WINELIB_TYPE_AW(CREATESTRUCT)
DECL_WINELIB_TYPE_AW(LPCREATESTRUCT)

typedef struct
{
    HDC   hdc;
    WIN_BOOL  fErase;
    RECT  rcPaint;
    WIN_BOOL  fRestore;
    WIN_BOOL  fIncUpdate;
    BYTE    rgbReserved[32];
} PAINTSTRUCT, *PPAINTSTRUCT, *LPPAINTSTRUCT;

typedef struct 
{
    HMENU   hWindowMenu;
    UINT    idFirstChild;
} CLIENTCREATESTRUCT, *LPCLIENTCREATESTRUCT;


typedef struct
{
    LPCSTR       szClass;
    LPCSTR       szTitle;
    HINSTANCE  hOwner;
    INT        x;
    INT        y;
    INT        cx;
    INT        cy;
    DWORD        style;
    LPARAM       lParam;
} MDICREATESTRUCTA, *LPMDICREATESTRUCTA;

typedef struct
{
    LPCWSTR      szClass;
    LPCWSTR      szTitle;
    HINSTANCE  hOwner;
    INT        x;
    INT        y;
    INT        cx;
    INT        cy;
    DWORD        style;
    LPARAM       lParam;
} MDICREATESTRUCTW, *LPMDICREATESTRUCTW;

DECL_WINELIB_TYPE_AW(MDICREATESTRUCT)
DECL_WINELIB_TYPE_AW(LPMDICREATESTRUCT)

#define MDITILE_VERTICAL     0x0000   
#define MDITILE_HORIZONTAL   0x0001
#define MDITILE_SKIPDISABLED 0x0002

#define MDIS_ALLCHILDSTYLES  0x0001

typedef struct {
    DWORD   styleOld;
    DWORD   styleNew;
} STYLESTRUCT, *LPSTYLESTRUCT;

  /* Offsets for GetWindowLong() and GetWindowWord() */
#define GWL_USERDATA        (-21)
#define GWL_EXSTYLE         (-20)
#define GWL_STYLE           (-16)
#define GWW_ID              (-12)
#define GWL_ID              GWW_ID
#define GWW_HWNDPARENT      (-8)
#define GWL_HWNDPARENT      GWW_HWNDPARENT
#define GWW_HINSTANCE       (-6)
#define GWL_HINSTANCE       GWW_HINSTANCE
#define GWL_WNDPROC         (-4)
#define DWL_MSGRESULT	    0
#define DWL_DLGPROC	    4
#define DWL_USER	    8

  /* GetWindow() constants */
#define GW_HWNDFIRST	0
#define GW_HWNDLAST	1
#define GW_HWNDNEXT	2
#define GW_HWNDPREV	3
#define GW_OWNER	4
#define GW_CHILD	5

  /* WM_GETMINMAXINFO struct */
typedef struct
{
    POINT   ptReserved;
    POINT   ptMaxSize;
    POINT   ptMaxPosition;
    POINT   ptMinTrackSize;
    POINT   ptMaxTrackSize;
} MINMAXINFO, *PMINMAXINFO, *LPMINMAXINFO;


  /* RedrawWindow() flags */
#define RDW_INVALIDATE       0x0001
#define RDW_INTERNALPAINT    0x0002
#define RDW_ERASE            0x0004
#define RDW_VALIDATE         0x0008
#define RDW_NOINTERNALPAINT  0x0010
#define RDW_NOERASE          0x0020
#define RDW_NOCHILDREN       0x0040
#define RDW_ALLCHILDREN      0x0080
#define RDW_UPDATENOW        0x0100
#define RDW_ERASENOW         0x0200
#define RDW_FRAME            0x0400
#define RDW_NOFRAME          0x0800

/* debug flags */
#define DBGFILL_ALLOC  0xfd
#define DBGFILL_FREE   0xfb
#define DBGFILL_BUFFER 0xf9
#define DBGFILL_STACK  0xf7

  /* WM_WINDOWPOSCHANGING/CHANGED struct */
typedef struct tagWINDOWPOS
{
    HWND  hwnd;
    HWND  hwndInsertAfter;
    INT   x;
    INT   y;
    INT   cx;
    INT   cy;
    UINT  flags;
} WINDOWPOS, *PWINDOWPOS, *LPWINDOWPOS;


  /* WM_MOUSEACTIVATE return values */
#define MA_ACTIVATE             1
#define MA_ACTIVATEANDEAT       2
#define MA_NOACTIVATE           3
#define MA_NOACTIVATEANDEAT     4

  /* WM_ACTIVATE wParam values */
#define WA_INACTIVE             0
#define WA_ACTIVE               1
#define WA_CLICKACTIVE          2

/* WM_GETICON/WM_SETICON params values */
#define ICON_SMALL              0
#define ICON_BIG                1

  /* WM_NCCALCSIZE parameter structure */
typedef struct
{
    RECT       rgrc[3];
    WINDOWPOS *lppos;
} NCCALCSIZE_PARAMS, *LPNCCALCSIZE_PARAMS;


  /* WM_NCCALCSIZE return flags */
#define WVR_ALIGNTOP        0x0010
#define WVR_ALIGNLEFT       0x0020
#define WVR_ALIGNBOTTOM     0x0040
#define WVR_ALIGNRIGHT      0x0080
#define WVR_HREDRAW         0x0100
#define WVR_VREDRAW         0x0200
#define WVR_REDRAW          (WVR_HREDRAW | WVR_VREDRAW)
#define WVR_VALIDRECTS      0x0400

  /* WM_NCHITTEST return codes */
#define HTERROR             (-2)
#define HTTRANSPARENT       (-1)
#define HTNOWHERE           0
#define HTCLIENT            1
#define HTCAPTION           2
#define HTSYSMENU           3
#define HTSIZE              4
#define HTMENU              5
#define HTHSCROLL           6
#define HTVSCROLL           7
#define HTMINBUTTON         8
#define HTMAXBUTTON         9
#define HTLEFT              10
#define HTRIGHT             11
#define HTTOP               12
#define HTTOPLEFT           13
#define HTTOPRIGHT          14
#define HTBOTTOM            15
#define HTBOTTOMLEFT        16
#define HTBOTTOMRIGHT       17
#define HTBORDER            18
#define HTGROWBOX           HTSIZE
#define HTREDUCE            HTMINBUTTON
#define HTZOOM              HTMAXBUTTON
#define HTOBJECT            19
#define HTCLOSE             20
#define HTHELP              21
#define HTSIZEFIRST         HTLEFT
#define HTSIZELAST          HTBOTTOMRIGHT

  /* WM_SYSCOMMAND parameters */
#ifdef SC_SIZE /* at least HP-UX: already defined in /usr/include/sys/signal.h */
#undef SC_SIZE
#endif
#define SC_SIZE         0xf000
#define SC_MOVE         0xf010
#define SC_MINIMIZE     0xf020
#define SC_MAXIMIZE     0xf030
#define SC_NEXTWINDOW   0xf040
#define SC_PREVWINDOW   0xf050
#define SC_CLOSE        0xf060
#define SC_VSCROLL      0xf070
#define SC_HSCROLL      0xf080
#define SC_MOUSEMENU    0xf090
#define SC_KEYMENU      0xf100
#define SC_ARRANGE      0xf110
#define SC_RESTORE      0xf120
#define SC_TASKLIST     0xf130
#define SC_SCREENSAVE   0xf140
#define SC_HOTKEY       0xf150

#define CS_VREDRAW          0x0001
#define CS_HREDRAW          0x0002
#define CS_KEYCVTWINDOW     0x0004
#define CS_DBLCLKS          0x0008
#define CS_OWNDC            0x0020
#define CS_CLASSDC          0x0040
#define CS_PARENTDC         0x0080
#define CS_NOKEYCVT         0x0100
#define CS_NOCLOSE          0x0200
#define CS_SAVEBITS         0x0800
#define CS_BYTEALIGNCLIENT  0x1000
#define CS_BYTEALIGNWINDOW  0x2000
#define CS_GLOBALCLASS      0x4000
#define CS_IME              0x00010000

#define PRF_CHECKVISIBLE    0x00000001L
#define PRF_NONCLIENT       0x00000002L
#define PRF_CLIENT          0x00000004L
#define PRF_ERASEBKGND      0x00000008L
#define PRF_CHILDREN        0x00000010L
#define PRF_OWNED           0x00000020L
 
  /* Offsets for GetClassLong() and GetClassWord() */
#define GCL_MENUNAME        (-8)
#define GCW_HBRBACKGROUND   (-10)
#define GCL_HBRBACKGROUND   GCW_HBRBACKGROUND
#define GCW_HCURSOR         (-12)
#define GCL_HCURSOR         GCW_HCURSOR
#define GCW_HICON           (-14)
#define GCL_HICON           GCW_HICON
#define GCW_HMODULE         (-16)
#define GCL_HMODULE         GCW_HMODULE
#define GCW_CBWNDEXTRA      (-18)
#define GCL_CBWNDEXTRA      GCW_CBWNDEXTRA
#define GCW_CBCLSEXTRA      (-20)
#define GCL_CBCLSEXTRA      GCW_CBCLSEXTRA
#define GCL_WNDPROC         (-24)
#define GCW_STYLE           (-26)
#define GCL_STYLE           GCW_STYLE
#define GCW_ATOM            (-32)
#define GCW_HICONSM         (-34)
#define GCL_HICONSM         GCW_HICONSM


/***** Window hooks *****/

  /* Hook values */
#define WH_MIN		    (-1)
#define WH_MSGFILTER	    (-1)
#define WH_JOURNALRECORD    0
#define WH_JOURNALPLAYBACK  1
#define WH_KEYBOARD	    2
#define WH_GETMESSAGE	    3
#define WH_CALLWNDPROC	    4
#define WH_CBT		    5
#define WH_SYSMSGFILTER	    6
#define WH_MOUSE	    7
#define WH_HARDWARE	    8
#define WH_DEBUG	    9
#define WH_SHELL            10
#define WH_FOREGROUNDIDLE   11
#define WH_CALLWNDPROCRET   12
#define WH_MAX              12

#define WH_MINHOOK          WH_MIN
#define WH_MAXHOOK          WH_MAX
#define WH_NB_HOOKS         (WH_MAXHOOK-WH_MINHOOK+1)

  /* Hook action codes */
#define HC_ACTION           0
#define HC_GETNEXT          1
#define HC_SKIP             2
#define HC_NOREMOVE         3
#define HC_NOREM            HC_NOREMOVE
#define HC_SYSMODALON       4
#define HC_SYSMODALOFF      5

  /* CallMsgFilter() values */
#define MSGF_DIALOGBOX      0
#define MSGF_MESSAGEBOX     1
#define MSGF_MENU           2
#define MSGF_MOVE           3
#define MSGF_SIZE           4
#define MSGF_SCROLLBAR      5
#define MSGF_NEXTWINDOW     6
#define MSGF_MAINLOOP       8
#define MSGF_USER        4096

typedef struct
{
    UINT      style;
    WNDPROC   lpfnWndProc;
    INT       cbClsExtra;
    INT       cbWndExtra;
    HINSTANCE hInstance;
    HICON     hIcon;
    HCURSOR   hCursor;
    HBRUSH    hbrBackground;
    LPCSTR      lpszMenuName;
    LPCSTR      lpszClassName;
} WNDCLASSA, *LPWNDCLASSA;

typedef struct
{
    UINT      style;
    WNDPROC   lpfnWndProc;
    INT       cbClsExtra;
    INT       cbWndExtra;
    HINSTANCE hInstance;
    HICON     hIcon;
    HCURSOR   hCursor;
    HBRUSH    hbrBackground;
    LPCWSTR     lpszMenuName;
    LPCWSTR     lpszClassName;
} WNDCLASSW, *LPWNDCLASSW;

DECL_WINELIB_TYPE_AW(WNDCLASS)
DECL_WINELIB_TYPE_AW(LPWNDCLASS)

typedef struct {
    DWORD dwData;
    DWORD cbData;
    LPVOID lpData;
} COPYDATASTRUCT, *PCOPYDATASTRUCT, *LPCOPYDATASTRUCT;

typedef struct {
    HMENU hmenuIn;
    HMENU hmenuNext;
    HWND  hwndNext;
} MDINEXTMENU, *PMDINEXTMENU, *LPMDINEXTMENU;

/* WinHelp internal structure */
typedef struct {
	WORD size;
	WORD command;
	LONG data;
	LONG reserved;
	WORD ofsFilename;
	WORD ofsData;
} WINHELP,*LPWINHELP;

typedef struct
{
    UINT16  mkSize;
    BYTE    mkKeyList;
    BYTE    szKeyphrase[1];
} MULTIKEYHELP, *LPMULTIKEYHELP;

typedef struct {
	WORD wStructSize;
	WORD x;
	WORD y;
	WORD dx;
	WORD dy;
	WORD wMax;
	char rgchMember[2];
} HELPWININFO, *LPHELPWININFO;

#define HELP_CONTEXT        0x0001
#define HELP_QUIT           0x0002
#define HELP_INDEX          0x0003
#define HELP_CONTENTS       0x0003
#define HELP_HELPONHELP     0x0004
#define HELP_SETINDEX       0x0005
#define HELP_SETCONTENTS    0x0005
#define HELP_CONTEXTPOPUP   0x0008
#define HELP_FORCEFILE      0x0009
#define HELP_KEY            0x0101
#define HELP_COMMAND        0x0102
#define HELP_PARTIALKEY     0x0105
#define HELP_MULTIKEY       0x0201
#define HELP_SETWINPOS      0x0203
#define HELP_CONTEXTMENU    0x000a
#define HELP_FINDER	    0x000b
#define HELP_WM_HELP	    0x000c
#define HELP_SETPOPUP_POS   0x000d

#define HELP_TCARD	    0x8000
#define HELP_TCARD_DATA	    0x0010
#define HELP_TCARD_OTHER_CALLER 0x0011


     /* ChangeDisplaySettings return codes */

#define DISP_CHANGE_SUCCESSFUL 0
#define DISP_CHANGE_RESTART    1
#define DISP_CHANGE_FAILED     (-1)
#define DISP_CHANGE_BADMODE    (-2)
#define DISP_CHANGE_NOTUPDATED (-3)
#define DISP_CHANGE_BADFLAGS   (-4)
#define DISP_CHANGE_BADPARAM   (-5)

/* ChangeDisplaySettings.dwFlags */
#define	CDS_UPDATEREGISTRY	0x00000001
#define	CDS_TEST		0x00000002
#define	CDS_FULLSCREEN		0x00000004
#define	CDS_GLOBAL		0x00000008
#define	CDS_SET_PRIMARY		0x00000010
#define	CDS_RESET		0x40000000
#define	CDS_SETRECT		0x20000000
#define	CDS_NORESET		0x10000000

/* flags to FormatMessage */
#define	FORMAT_MESSAGE_ALLOCATE_BUFFER	0x00000100
#define	FORMAT_MESSAGE_IGNORE_INSERTS	0x00000200
#define	FORMAT_MESSAGE_FROM_STRING	0x00000400
#define	FORMAT_MESSAGE_FROM_HMODULE	0x00000800
#define	FORMAT_MESSAGE_FROM_SYSTEM	0x00001000
#define	FORMAT_MESSAGE_ARGUMENT_ARRAY	0x00002000
#define	FORMAT_MESSAGE_MAX_WIDTH_MASK	0x000000FF

typedef struct
{
    UINT      cbSize;
    UINT      style;
    WNDPROC   lpfnWndProc;
    INT       cbClsExtra;
    INT       cbWndExtra;
    HINSTANCE hInstance;
    HICON     hIcon;
    HCURSOR   hCursor;
    HBRUSH    hbrBackground;
    LPCSTR      lpszMenuName;
    LPCSTR      lpszClassName;
    HICON     hIconSm;
} WNDCLASSEXA, *LPWNDCLASSEXA;

typedef struct
{
    UINT      cbSize;
    UINT      style;
    WNDPROC   lpfnWndProc;
    INT       cbClsExtra;
    INT       cbWndExtra;
    HINSTANCE hInstance;
    HICON     hIcon;
    HCURSOR   hCursor;
    HBRUSH    hbrBackground;
    LPCWSTR     lpszMenuName;
    LPCWSTR     lpszClassName;
    HICON     hIconSm;
} WNDCLASSEXW, *LPWNDCLASSEXW;

DECL_WINELIB_TYPE_AW(WNDCLASSEX)
DECL_WINELIB_TYPE_AW(LPWNDCLASSEX)

typedef struct tagMSG
{
    HWND    hwnd;
    UINT    message;
    WPARAM  wParam;
    LPARAM    lParam;
    DWORD     time;
    POINT   pt;
} MSG, *LPMSG;

#define POINTSTOPOINT(pt, pts)                          \
        { (pt).x = (LONG)(SHORT)LOWORD(*(LONG*)&pts);   \
          (pt).y = (LONG)(SHORT)HIWORD(*(LONG*)&pts); }          

#define POINTTOPOINTS(pt)      (MAKELONG((short)((pt).x), (short)((pt).y)))


/* Cursors / Icons */

typedef struct {
	WIN_BOOL	fIcon;
	DWORD		xHotspot;
	DWORD		yHotspot;
	HBITMAP	hbmMask;
	HBITMAP	hbmColor;
} ICONINFO,*LPICONINFO;


/* this is the 6 byte accel struct used in Win32 when presented to the user */
typedef struct
{
    BYTE   fVirt;
    BYTE   pad0;
    WORD   key;
    WORD   cmd;
} ACCEL, *LPACCEL;

/* this is the 8 byte accel struct used in Win32 resources (internal only) */
typedef struct
{
    BYTE   fVirt;
    BYTE   pad0;
    WORD   key;
    WORD   cmd;
    WORD   pad1;
} PE_ACCEL, *LPPE_ACCEL;


/* Flags for TrackPopupMenu */
#define TPM_LEFTBUTTON    0x0000
#define TPM_RIGHTBUTTON   0x0002
#define TPM_LEFTALIGN     0x0000
#define TPM_CENTERALIGN   0x0004
#define TPM_RIGHTALIGN    0x0008
#define TPM_TOPALIGN      0x0000
#define TPM_VCENTERALIGN  0x0010
#define TPM_BOTTOMALIGN   0x0020
#define TPM_HORIZONTAL    0x0000
#define TPM_VERTICAL      0x0040
#define TPM_NONOTIFY      0x0080
#define TPM_RETURNCMD     0x0100

typedef struct 
{
    UINT   cbSize;
    RECT   rcExclude;
} TPMPARAMS, *LPTPMPARAMS;

/* FIXME: not sure this one is correct */
typedef struct {
  UINT    cbSize;
  UINT    fMask;
  UINT    fType;
  UINT    fState;
  UINT    wID;
  HMENU   hSubMenu;
  HBITMAP hbmpChecked;
  HBITMAP hbmpUnchecked;
  DWORD   dwItemData;
  LPSTR   dwTypeData;
  UINT    cch;
  HBITMAP hbmpItem;
} MENUITEMINFOA, *LPMENUITEMINFOA;

typedef struct {
  UINT    cbSize;
  UINT    fMask;
  UINT    fType;
  UINT    fState;
  UINT    wID;
  HMENU   hSubMenu;
  HBITMAP hbmpChecked;
  HBITMAP hbmpUnchecked;
  DWORD     dwItemData;
  LPWSTR    dwTypeData;
  UINT    cch;
  HBITMAP hbmpItem;
} MENUITEMINFOW, *LPMENUITEMINFOW;

DECL_WINELIB_TYPE_AW(MENUITEMINFO)
DECL_WINELIB_TYPE_AW(LPMENUITEMINFO)

typedef struct {
  DWORD   cbSize;
  DWORD   fMask;
  DWORD   dwStyle;
  UINT    cyMax;
  HBRUSH  hbrBack;
  DWORD   dwContextHelpID;
  DWORD   dwMenuData;
} MENUINFO, *LPMENUINFO;

typedef MENUINFO const * LPCMENUINFO;

#define MIM_MAXHEIGHT		0x00000001
#define MIM_BACKGROUND		0x00000002
#define MIM_HELPID		0x00000004
#define MIM_MENUDATA		0x00000008
#define MIM_STYLE		0x00000010
#define MIM_APPLYTOSUBMENUS	0x80000000

typedef struct {
  WORD versionNumber;
  WORD offset;
} MENUITEMTEMPLATEHEADER, *PMENUITEMTEMPLATEHEADER;


typedef struct {
  WORD mtOption;
  WORD mtID;
  WCHAR mtString[1];
} MENUITEMTEMPLATE, *PMENUITEMTEMPLATE;


typedef VOID   MENUTEMPLATE;
typedef PVOID *LPMENUTEMPLATE;

/* Field specifiers for MENUITEMINFO[AW] type.  */
#define MIIM_STATE       0x00000001
#define MIIM_ID          0x00000002
#define MIIM_SUBMENU     0x00000004
#define MIIM_CHECKMARKS  0x00000008
#define MIIM_TYPE        0x00000010
#define MIIM_DATA        0x00000020
#define MIIM_STRING      0x00000040
#define MIIM_BITMAP      0x00000080
#define MIIM_FTYPE       0x00000100

#define HBMMENU_CALLBACK	((HBITMAP) -1)
#define HBMMENU_SYSTEM		((HBITMAP)  1)
#define HBMMENU_MBAR_RESTORE	((HBITMAP)  2)
#define HBMMENU_MBAR_MINIMIZE	((HBITMAP)  3)
#define HBMMENU_MBAR_CLOSE	((HBITMAP)  5)
#define HBMMENU_MBAR_CLOSE_D	((HBITMAP)  6)
#define HBMMENU_MBAR_MINIMIZE_D	((HBITMAP)  7)
#define HBMMENU_POPUP_CLOSE	((HBITMAP)  8)
#define HBMMENU_POPUP_RESTORE	((HBITMAP)  9)
#define HBMMENU_POPUP_MAXIMIZE	((HBITMAP) 10)
#define HBMMENU_POPUP_MINIMIZE	((HBITMAP) 11)

/* DrawState defines ... */
typedef WIN_BOOL CALLBACK (*DRAWSTATEPROC)(HDC,LPARAM,WPARAM,INT,INT);

/* WM_H/VSCROLL commands */
#define SB_LINEUP           0
#define SB_LINELEFT         0
#define SB_LINEDOWN         1
#define SB_LINERIGHT        1
#define SB_PAGEUP           2
#define SB_PAGELEFT         2
#define SB_PAGEDOWN         3
#define SB_PAGERIGHT        3
#define SB_THUMBPOSITION    4
#define SB_THUMBTRACK       5
#define SB_TOP              6
#define SB_LEFT             6
#define SB_BOTTOM           7
#define SB_RIGHT            7
#define SB_ENDSCROLL        8

/* Scroll bar selection constants */
#define SB_HORZ             0
#define SB_VERT             1
#define SB_CTL              2
#define SB_BOTH             3

/* Scrollbar styles */
#define SBS_HORZ                    0x0000L
#define SBS_VERT                    0x0001L
#define SBS_TOPALIGN                0x0002L
#define SBS_LEFTALIGN               0x0002L
#define SBS_BOTTOMALIGN             0x0004L
#define SBS_RIGHTALIGN              0x0004L
#define SBS_SIZEBOXTOPLEFTALIGN     0x0002L
#define SBS_SIZEBOXBOTTOMRIGHTALIGN 0x0004L
#define SBS_SIZEBOX                 0x0008L
#define SBS_SIZEGRIP                0x0010L

/* EnableScrollBar() flags */
#define ESB_ENABLE_BOTH     0x0000
#define ESB_DISABLE_BOTH    0x0003

#define ESB_DISABLE_LEFT    0x0001
#define ESB_DISABLE_RIGHT   0x0002

#define ESB_DISABLE_UP      0x0001
#define ESB_DISABLE_DOWN    0x0002

#define ESB_DISABLE_LTUP    ESB_DISABLE_LEFT
#define ESB_DISABLE_RTDN    ESB_DISABLE_RIGHT

/* Win32 button control messages */
#define BM_GETCHECK          0x00f0
#define BM_SETCHECK          0x00f1
#define BM_GETSTATE          0x00f2
#define BM_SETSTATE          0x00f3
#define BM_SETSTYLE          0x00f4
#define BM_CLICK             0x00f5
#define BM_GETIMAGE          0x00f6
#define BM_SETIMAGE          0x00f7
/* Winelib button control messages */

/* Button notification codes */
#define BN_CLICKED             0
#define BN_PAINT               1
#define BN_HILITE              2
#define BN_UNHILITE            3
#define BN_DISABLE             4
#define BN_DOUBLECLICKED       5

/* Button states */
#define BST_UNCHECKED        0x0000
#define BST_CHECKED          0x0001
#define BST_INDETERMINATE    0x0002
#define BST_PUSHED           0x0004
#define BST_FOCUS            0x0008      

/* Static Control Styles */
#define SS_LEFT             0x00000000L
#define SS_CENTER           0x00000001L
#define SS_RIGHT            0x00000002L
#define SS_ICON             0x00000003L
#define SS_BLACKRECT        0x00000004L
#define SS_GRAYRECT         0x00000005L
#define SS_WHITERECT        0x00000006L
#define SS_BLACKFRAME       0x00000007L
#define SS_GRAYFRAME        0x00000008L
#define SS_WHITEFRAME       0x00000009L

#define SS_SIMPLE           0x0000000BL
#define SS_LEFTNOWORDWRAP   0x0000000CL

#define SS_OWNERDRAW        0x0000000DL
#define SS_BITMAP           0x0000000EL
#define SS_ENHMETAFILE      0x0000000FL

#define SS_ETCHEDHORZ       0x00000010L
#define SS_ETCHEDVERT       0x00000011L
#define SS_ETCHEDFRAME      0x00000012L
#define SS_TYPEMASK         0x0000001FL

#define SS_NOPREFIX         0x00000080L
#define SS_NOTIFY           0x00000100L
#define SS_CENTERIMAGE      0x00000200L
#define SS_RIGHTJUST        0x00000400L
#define SS_REALSIZEIMAGE    0x00000800L
#define SS_SUNKEN           0x00001000L

/* Static Control Messages */
#define STM_SETICON       0x0170
#define STM_GETICON       0x0171
#define STM_SETIMAGE        0x0172
#define STM_GETIMAGE        0x0173

/* Scrollbar messages */
#define SBM_SETPOS             0x00e0
#define SBM_GETPOS             0x00e1
#define SBM_SETRANGE           0x00e2
#define SBM_GETRANGE           0x00e3
#define SBM_ENABLE_ARROWS      0x00e4
#define SBM_SETRANGEREDRAW     0x00e6
#define SBM_SETSCROLLINFO      0x00e9
#define SBM_GETSCROLLINFO      0x00ea

/* Scrollbar info */
typedef struct
{
    UINT    cbSize;
    UINT    fMask;
    INT     nMin;
    INT     nMax;
    UINT    nPage;
    INT     nPos;
    INT     nTrackPos;
} SCROLLINFO, *LPSCROLLINFO;
 
/* GetScrollInfo() flags */ 
#define SIF_RANGE           0x0001
#define SIF_PAGE            0x0002
#define SIF_POS             0x0004
#define SIF_DISABLENOSCROLL 0x0008
#define SIF_TRACKPOS        0x0010
#define SIF_ALL             (SIF_RANGE | SIF_PAGE | SIF_POS | SIF_TRACKPOS)

/* Listbox styles */
#define LBS_NOTIFY               0x0001
#define LBS_SORT                 0x0002
#define LBS_NOREDRAW             0x0004
#define LBS_MULTIPLESEL          0x0008
#define LBS_OWNERDRAWFIXED       0x0010
#define LBS_OWNERDRAWVARIABLE    0x0020
#define LBS_HASSTRINGS           0x0040
#define LBS_USETABSTOPS          0x0080
#define LBS_NOINTEGRALHEIGHT     0x0100
#define LBS_MULTICOLUMN          0x0200
#define LBS_WANTKEYBOARDINPUT    0x0400
#define LBS_EXTENDEDSEL          0x0800
#define LBS_DISABLENOSCROLL      0x1000
#define LBS_NODATA               0x2000
#define LBS_NOSEL                0x4000
#define LBS_STANDARD  (LBS_NOTIFY | LBS_SORT | WS_VSCROLL | WS_BORDER)

/* Listbox messages */
#define LB_ADDSTRING           0x0180
#define LB_INSERTSTRING        0x0181
#define LB_DELETESTRING        0x0182
#define LB_SELITEMRANGEEX      0x0183
#define LB_RESETCONTENT        0x0184
#define LB_SETSEL              0x0185
#define LB_SETCURSEL           0x0186
#define LB_GETSEL              0x0187
#define LB_GETCURSEL           0x0188
#define LB_GETTEXT             0x0189
#define LB_GETTEXTLEN          0x018a
#define LB_GETCOUNT            0x018b
#define LB_SELECTSTRING        0x018c
#define LB_DIR                 0x018d
#define LB_GETTOPINDEX         0x018e
#define LB_FINDSTRING          0x018f
#define LB_GETSELCOUNT         0x0190
#define LB_GETSELITEMS         0x0191
#define LB_SETTABSTOPS         0x0192
#define LB_GETHORIZONTALEXTENT 0x0193
#define LB_SETHORIZONTALEXTENT 0x0194
#define LB_SETCOLUMNWIDTH      0x0195
#define LB_ADDFILE             0x0196
#define LB_SETTOPINDEX         0x0197
#define LB_GETITEMRECT         0x0198
#define LB_GETITEMDATA         0x0199
#define LB_SETITEMDATA         0x019a
#define LB_SELITEMRANGE        0x019b
#define LB_SETANCHORINDEX      0x019c
#define LB_GETANCHORINDEX      0x019d
#define LB_SETCARETINDEX       0x019e
#define LB_GETCARETINDEX       0x019f
#define LB_SETITEMHEIGHT       0x01a0
#define LB_GETITEMHEIGHT       0x01a1
#define LB_FINDSTRINGEXACT     0x01a2
#define LB_CARETON             0x01a3
#define LB_CARETOFF            0x01a4
#define LB_SETLOCALE           0x01a5
#define LB_GETLOCALE           0x01a6
#define LB_SETCOUNT            0x01a7
#define LB_INITSTORAGE         0x01a8
#define LB_ITEMFROMPOINT       0x01a9

/* Listbox notification codes */
#define LBN_ERRSPACE        (-2)
#define LBN_SELCHANGE       1
#define LBN_DBLCLK          2
#define LBN_SELCANCEL       3
#define LBN_SETFOCUS        4
#define LBN_KILLFOCUS       5

/* Listbox message return values */
#define LB_OKAY             0
#define LB_ERR              (-1)
#define LB_ERRSPACE         (-2)

#define LB_CTLCODE          0L

/* Combo box styles */
#define CBS_SIMPLE            0x0001L
#define CBS_DROPDOWN          0x0002L
#define CBS_DROPDOWNLIST      0x0003L
#define CBS_OWNERDRAWFIXED    0x0010L
#define CBS_OWNERDRAWVARIABLE 0x0020L
#define CBS_AUTOHSCROLL       0x0040L
#define CBS_OEMCONVERT        0x0080L
#define CBS_SORT              0x0100L
#define CBS_HASSTRINGS        0x0200L
#define CBS_NOINTEGRALHEIGHT  0x0400L
#define CBS_DISABLENOSCROLL   0x0800L

#define CBS_UPPERCASE	      0x2000L
#define CBS_LOWERCASE	      0x4000L


/* Combo box messages */
#define CB_GETEDITSEL            0x0140
#define CB_LIMITTEXT             0x0141
#define CB_SETEDITSEL            0x0142
#define CB_ADDSTRING             0x0143
#define CB_DELETESTRING          0x0144
#define CB_DIR                   0x0145
#define CB_GETCOUNT              0x0146
#define CB_GETCURSEL             0x0147
#define CB_GETLBTEXT             0x0148
#define CB_GETLBTEXTLEN          0x0149
#define CB_INSERTSTRING          0x014a
#define CB_RESETCONTENT          0x014b
#define CB_FINDSTRING            0x014c
#define CB_SELECTSTRING          0x014d
#define CB_SETCURSEL             0x014e
#define CB_SHOWDROPDOWN          0x014f
#define CB_GETITEMDATA           0x0150
#define CB_SETITEMDATA           0x0151
#define CB_GETDROPPEDCONTROLRECT 0x0152
#define CB_SETITEMHEIGHT         0x0153
#define CB_GETITEMHEIGHT         0x0154
#define CB_SETEXTENDEDUI         0x0155
#define CB_GETEXTENDEDUI         0x0156
#define CB_GETDROPPEDSTATE       0x0157
#define CB_FINDSTRINGEXACT       0x0158
#define CB_SETLOCALE             0x0159
#define CB_GETLOCALE             0x015a
#define CB_GETTOPINDEX           0x015b
#define CB_SETTOPINDEX           0x015c
#define CB_GETHORIZONTALEXTENT   0x015d
#define CB_SETHORIZONTALEXTENT   0x015e
#define CB_GETDROPPEDWIDTH       0x015f
#define CB_SETDROPPEDWIDTH       0x0160
#define CB_INITSTORAGE           0x0161

/* Combo box notification codes */
#define CBN_ERRSPACE        (-1)
#define CBN_SELCHANGE       1
#define CBN_DBLCLK          2
#define CBN_SETFOCUS        3
#define CBN_KILLFOCUS       4
#define CBN_EDITCHANGE      5
#define CBN_EDITUPDATE      6
#define CBN_DROPDOWN        7
#define CBN_CLOSEUP         8
#define CBN_SELENDOK        9
#define CBN_SELENDCANCEL    10

/* Combo box message return values */
#define CB_OKAY             0
#define CB_ERR              (-1)
#define CB_ERRSPACE         (-2)

#define MB_OK			0x00000000
#define MB_OKCANCEL		0x00000001
#define MB_ABORTRETRYIGNORE	0x00000002
#define MB_YESNOCANCEL		0x00000003
#define MB_YESNO		0x00000004
#define MB_RETRYCANCEL		0x00000005
#define MB_TYPEMASK		0x0000000F

#define MB_ICONHAND		0x00000010
#define MB_ICONQUESTION		0x00000020
#define MB_ICONEXCLAMATION	0x00000030
#define MB_ICONASTERISK		0x00000040
#define	MB_USERICON		0x00000080
#define MB_ICONMASK		0x000000F0

#define MB_ICONINFORMATION	MB_ICONASTERISK
#define MB_ICONSTOP		MB_ICONHAND
#define MB_ICONWARNING		MB_ICONEXCLAMATION
#define MB_ICONERROR		MB_ICONHAND

#define MB_DEFBUTTON1		0x00000000
#define MB_DEFBUTTON2		0x00000100
#define MB_DEFBUTTON3		0x00000200
#define MB_DEFBUTTON4		0x00000300
#define MB_DEFMASK		0x00000F00

#define MB_APPLMODAL		0x00000000
#define MB_SYSTEMMODAL		0x00001000
#define MB_TASKMODAL		0x00002000
#define MB_MODEMASK		0x00003000

#define MB_HELP			0x00004000
#define MB_NOFOCUS		0x00008000
#define MB_MISCMASK		0x0000C000

#define MB_SETFOREGROUND	0x00010000
#define MB_DEFAULT_DESKTOP_ONLY	0x00020000
#define MB_SERVICE_NOTIFICATION	0x00040000
#define MB_TOPMOST		0x00040000
#define MB_RIGHT		0x00080000
#define MB_RTLREADING		0x00100000

#define	HELPINFO_WINDOW		0x0001
#define	HELPINFO_MENUITEM	0x0002

/* Structure pointed to by lParam of WM_HELP */
typedef struct			
{
    UINT	cbSize;		/* Size in bytes of this struct  */
    INT	iContextType;	/* Either HELPINFO_WINDOW or HELPINFO_MENUITEM */
    INT	iCtrlId;	/* Control Id or a Menu item Id. */
    HANDLE	hItemHandle;	/* hWnd of control or hMenu.     */
    DWORD	dwContextId;	/* Context Id associated with this item */
    POINT	MousePos;	/* Mouse Position in screen co-ordinates */
}  HELPINFO,*LPHELPINFO;

typedef void CALLBACK (*MSGBOXCALLBACK)(LPHELPINFO lpHelpInfo);

typedef struct
{
    UINT	cbSize;
    HWND	hwndOwner;
    HINSTANCE	hInstance;
    LPCSTR	lpszText;
    LPCSTR	lpszCaption;
    DWORD	dwStyle;
    LPCSTR	lpszIcon;
    DWORD	dwContextHelpId;
    MSGBOXCALLBACK	lpfnMsgBoxCallback;
    DWORD	dwLanguageId;
} MSGBOXPARAMSA,*LPMSGBOXPARAMSA;

typedef struct
{
    UINT	cbSize;
    HWND	hwndOwner;
    HINSTANCE	hInstance;
    LPCWSTR	lpszText;
    LPCWSTR	lpszCaption;
    DWORD	dwStyle;
    LPCWSTR	lpszIcon;
    DWORD	dwContextHelpId;
    MSGBOXCALLBACK	lpfnMsgBoxCallback;
    DWORD	dwLanguageId;
} MSGBOXPARAMSW,*LPMSGBOXPARAMSW;

DECL_WINELIB_TYPE_AW(MSGBOXPARAMS)
DECL_WINELIB_TYPE_AW(LPMSGBOXPARAMS)

typedef struct _numberfmt32a {
    UINT NumDigits;
    UINT LeadingZero;
    UINT Grouping;
    LPCSTR lpDecimalSep;
    LPCSTR lpThousandSep;
    UINT NegativeOrder;
} NUMBERFMTA;

typedef struct _numberfmt32w {
    UINT NumDigits;
    UINT LeadingZero;
    UINT Grouping;
    LPCWSTR lpDecimalSep;
    LPCWSTR lpThousandSep;
    UINT NegativeOrder;
} NUMBERFMTW;

typedef struct _currencyfmt32a
{   
	UINT      NumDigits;   
	UINT      LeadingZero; 
	UINT      Grouping;   
	LPCSTR    lpDecimalSep;   
	LPCSTR    lpThousandSep; 
	UINT      NegativeOrder;   
	UINT      PositiveOrder; 
	LPCSTR    lpCurrencySymbol;
} CURRENCYFMTA; 

typedef struct _currencyfmt32w
{   
	UINT      NumDigits;   
	UINT      LeadingZero; 
	UINT      Grouping;   
	LPCWSTR   lpDecimalSep;   
	LPCWSTR   lpThousandSep; 
	UINT      NegativeOrder;   
	UINT      PositiveOrder; 
	LPCWSTR   lpCurrencySymbol;
} CURRENCYFMTW; 

#define MONITOR_DEFAULTTONULL       0x00000000
#define MONITOR_DEFAULTTOPRIMARY    0x00000001
#define MONITOR_DEFAULTTONEAREST    0x00000002

#define MONITORINFOF_PRIMARY        0x00000001

typedef struct tagMONITORINFO
{
    DWORD   cbSize;
    RECT  rcMonitor;
    RECT  rcWork;
    DWORD   dwFlags;
} MONITORINFO, *LPMONITORINFO;


typedef WIN_BOOL  CALLBACK (*MONITORENUMPROC)(HMONITOR,HDC,LPRECT,LPARAM);

/* FIXME: use this instead of LPCVOID for CreateDialogIndirectParam
   and DialogBoxIndirectParam */
typedef struct tagDLGTEMPLATE
{
    DWORD style;
    DWORD dwExtendedStyle;
    WORD cdit;
    short x;
    short y;
    short cx;
    short cy;
} DLGTEMPLATE;

typedef DLGTEMPLATE *LPDLGTEMPLATEA;
typedef DLGTEMPLATE *LPDLGTEMPLATEW;
#define LPDLGTEMPLATE WINELIB_NAME_AW(LPDLGTEMPLATE)
typedef const DLGTEMPLATE *LPCDLGTEMPLATEA;
typedef const DLGTEMPLATE *LPCDLGTEMPLATEW;
#define LPCDLGTEMPLATE WINELIB_NAME_AW(LPCDLGTEMPLATE)

typedef struct tagDLGITEMTEMPLATE
{
    DWORD style;
    DWORD dwExtendedStyle;
    short x;
    short y;
    short cx;
    short cy;
    WORD id;
} DLGITEMTEMPLATE;

typedef DLGITEMTEMPLATE *LPDLGITEMTEMPLATEA;
typedef DLGITEMTEMPLATE *LPDLGITEMTEMPLATEW;
#define LPDLGITEMTEMPLATE WINELIB_NAME_AW(LPDLGITEMTEMPLATE)
typedef const DLGITEMTEMPLATE *LPCDLGITEMTEMPLATEA;
typedef const DLGITEMTEMPLATE *LPCDLGITEMTEMPLATEW;
#define LPCDLGITEMTEMPLATE WINELIB_NAME_AW(LPCDLGITEMTEMPLATE)


  /* CBT hook values */
#define HCBT_MOVESIZE	    0
#define HCBT_MINMAX	    1
#define HCBT_QS 	    2
#define HCBT_CREATEWND	    3
#define HCBT_DESTROYWND	    4
#define HCBT_ACTIVATE	    5
#define HCBT_CLICKSKIPPED   6
#define HCBT_KEYSKIPPED     7
#define HCBT_SYSCOMMAND	    8
#define HCBT_SETFOCUS	    9

  /* CBT hook structures */

typedef struct
{
    CREATESTRUCTA *lpcs;
    HWND           hwndInsertAfter;
} CBT_CREATEWNDA, *LPCBT_CREATEWNDA;

typedef struct
{
    CREATESTRUCTW *lpcs;
    HWND           hwndInsertAfter;
} CBT_CREATEWNDW, *LPCBT_CREATEWNDW;

DECL_WINELIB_TYPE_AW(CBT_CREATEWND)
DECL_WINELIB_TYPE_AW(LPCBT_CREATEWND)

typedef struct
{
    WIN_BOOL    fMouse;
    HWND    hWndActive;
} CBTACTIVATESTRUCT, *LPCBTACTIVATESTRUCT;


/* modifiers for RegisterHotKey */
#define	MOD_ALT		0x0001
#define	MOD_CONTROL	0x0002
#define	MOD_SHIFT	0x0004
#define	MOD_WIN		0x0008

/* ids for RegisterHotKey */
#define	IDHOT_SNAPWINDOW	(-1)    /* SHIFT-PRINTSCRN  */
#define	IDHOT_SNAPDESKTOP	(-2)    /* PRINTSCRN        */

  /* keybd_event flags */
#define KEYEVENTF_EXTENDEDKEY        0x0001
#define KEYEVENTF_KEYUP              0x0002
#define KEYEVENTF_WINE_FORCEEXTENDED 0x8000

  /* mouse_event flags */
#define MOUSEEVENTF_MOVE        0x0001
#define MOUSEEVENTF_LEFTDOWN    0x0002
#define MOUSEEVENTF_LEFTUP      0x0004
#define MOUSEEVENTF_RIGHTDOWN   0x0008
#define MOUSEEVENTF_RIGHTUP     0x0010
#define MOUSEEVENTF_MIDDLEDOWN  0x0020
#define MOUSEEVENTF_MIDDLEUP    0x0040
#define MOUSEEVENTF_WHEEL       0x0800
#define MOUSEEVENTF_ABSOLUTE    0x8000

/* ExitWindows() flags */
#define EW_RESTARTWINDOWS   0x0042
#define EW_REBOOTSYSTEM     0x0043
#define EW_EXITANDEXECAPP   0x0044

/* ExitWindowsEx() flags */
#define EWX_LOGOFF           0
#define EWX_SHUTDOWN         1
#define EWX_REBOOT           2
#define EWX_FORCE            4
#define EWX_POWEROFF         8

/* SetLastErrorEx types */
#define	SLE_ERROR	0x00000001
#define	SLE_MINORERROR	0x00000002
#define	SLE_WARNING	0x00000003

/* Predefined resources */
#define IDI_APPLICATIONA MAKEINTRESOURCEA(32512)
#define IDI_APPLICATIONW MAKEINTRESOURCEW(32512)
#define IDI_APPLICATION    WINELIB_NAME_AW(IDI_APPLICATION)
#define IDI_HANDA        MAKEINTRESOURCEA(32513)
#define IDI_HANDW        MAKEINTRESOURCEW(32513)
#define IDI_HAND           WINELIB_NAME_AW(IDI_HAND)
#define IDI_QUESTIONA    MAKEINTRESOURCEA(32514)
#define IDI_QUESTIONW    MAKEINTRESOURCEW(32514)
#define IDI_QUESTION       WINELIB_NAME_AW(IDI_QUESTION)
#define IDI_EXCLAMATIONA MAKEINTRESOURCEA(32515)
#define IDI_EXCLAMATIONW MAKEINTRESOURCEW(32515)
#define IDI_EXCLAMATION    WINELIB_NAME_AW(IDI_EXCLAMATION)
#define IDI_ASTERISKA    MAKEINTRESOURCEA(32516)
#define IDI_ASTERISKW    MAKEINTRESOURCEW(32516)
#define IDI_ASTERISK       WINELIB_NAME_AW(IDI_ASTERISK)

#define IDC_BUMMERA      MAKEINTRESOURCEA(100)
#define IDC_BUMMERW      MAKEINTRESOURCEW(100)
#define IDC_BUMMER         WINELIB_NAME_AW(IDC_BUMMER)
#define IDC_ARROWA       MAKEINTRESOURCEA(32512)
#define IDC_ARROWW       MAKEINTRESOURCEW(32512)
#define IDC_ARROW          WINELIB_NAME_AW(IDC_ARROW)
#define IDC_IBEAMA       MAKEINTRESOURCEA(32513)
#define IDC_IBEAMW       MAKEINTRESOURCEW(32513)
#define IDC_IBEAM          WINELIB_NAME_AW(IDC_IBEAM)
#define IDC_WAITA        MAKEINTRESOURCEA(32514)
#define IDC_WAITW        MAKEINTRESOURCEW(32514)
#define IDC_WAIT           WINELIB_NAME_AW(IDC_WAIT)
#define IDC_CROSSA       MAKEINTRESOURCEA(32515)
#define IDC_CROSSW       MAKEINTRESOURCEW(32515)
#define IDC_CROSS          WINELIB_NAME_AW(IDC_CROSS)
#define IDC_UPARROWA     MAKEINTRESOURCEA(32516)
#define IDC_UPARROWW     MAKEINTRESOURCEW(32516)
#define IDC_UPARROW        WINELIB_NAME_AW(IDC_UPARROW)
#define IDC_SIZEA        MAKEINTRESOURCEA(32640)
#define IDC_SIZEW        MAKEINTRESOURCEW(32640)
#define IDC_SIZE           WINELIB_NAME_AW(IDC_SIZE)
#define IDC_ICONA        MAKEINTRESOURCEA(32641)
#define IDC_ICONW        MAKEINTRESOURCEW(32641)
#define IDC_ICON           WINELIB_NAME_AW(IDC_ICON)
#define IDC_SIZENWSEA    MAKEINTRESOURCEA(32642)
#define IDC_SIZENWSEW    MAKEINTRESOURCEW(32642)
#define IDC_SIZENWSE       WINELIB_NAME_AW(IDC_SIZENWSE)
#define IDC_SIZENESWA    MAKEINTRESOURCEA(32643)
#define IDC_SIZENESWW    MAKEINTRESOURCEW(32643)
#define IDC_SIZENESW       WINELIB_NAME_AW(IDC_SIZENESW)
#define IDC_SIZEWEA      MAKEINTRESOURCEA(32644)
#define IDC_SIZEWEW      MAKEINTRESOURCEW(32644)
#define IDC_SIZEWE         WINELIB_NAME_AW(IDC_SIZEWE)
#define IDC_SIZENSA      MAKEINTRESOURCEA(32645)
#define IDC_SIZENSW      MAKEINTRESOURCEW(32645)
#define IDC_SIZENS         WINELIB_NAME_AW(IDC_SIZENS)
#define IDC_SIZEALLA     MAKEINTRESOURCEA(32646)
#define IDC_SIZEALLW     MAKEINTRESOURCEW(32646)
#define IDC_SIZEALL        WINELIB_NAME_AW(IDC_SIZEALL)
#define IDC_NOA          MAKEINTRESOURCEA(32648)
#define IDC_NOW          MAKEINTRESOURCEW(32648)
#define IDC_NO             WINELIB_NAME_AW(IDC_NO)
#define IDC_APPSTARTINGA MAKEINTRESOURCEA(32650)
#define IDC_APPSTARTINGW MAKEINTRESOURCEW(32650)
#define IDC_APPSTARTING    WINELIB_NAME_AW(IDC_APPSTARTING)
#define IDC_HELPA        MAKEINTRESOURCEA(32651)
#define IDC_HELPW        MAKEINTRESOURCEW(32651)
#define IDC_HELP           WINELIB_NAME_AW(IDC_HELP)

#define MNC_IGNORE 0
#define MNC_CLOSE 1
#define MNC_EXECUTE 2
#define MNC_SELECT 3 

/* SystemParametersInfo */
/* defines below are for all win versions */
#define SPI_GETBEEP               1
#define SPI_SETBEEP               2
#define SPI_GETMOUSE              3
#define SPI_SETMOUSE              4
#define SPI_GETBORDER             5
#define SPI_SETBORDER             6
#define SPI_GETKEYBOARDSPEED      10
#define SPI_SETKEYBOARDSPEED      11
#define SPI_LANGDRIVER            12
#define SPI_ICONHORIZONTALSPACING 13
#define SPI_GETSCREENSAVETIMEOUT  14
#define SPI_SETSCREENSAVETIMEOUT  15
#define SPI_GETSCREENSAVEACTIVE   16
#define SPI_SETSCREENSAVEACTIVE   17
#define SPI_GETGRIDGRANULARITY    18
#define SPI_SETGRIDGRANULARITY    19
#define SPI_SETDESKWALLPAPER      20
#define SPI_SETDESKPATTERN        21
#define SPI_GETKEYBOARDDELAY      22
#define SPI_SETKEYBOARDDELAY      23
#define SPI_ICONVERTICALSPACING   24
#define SPI_GETICONTITLEWRAP      25
#define SPI_SETICONTITLEWRAP      26
#define SPI_GETMENUDROPALIGNMENT  27
#define SPI_SETMENUDROPALIGNMENT  28
#define SPI_SETDOUBLECLKWIDTH     29
#define SPI_SETDOUBLECLKHEIGHT    30
#define SPI_GETICONTITLELOGFONT   31
#define SPI_SETDOUBLECLICKTIME    32
#define SPI_SETMOUSEBUTTONSWAP    33
#define SPI_SETICONTITLELOGFONT   34
#define SPI_GETFASTTASKSWITCH     35
#define SPI_SETFASTTASKSWITCH     36
#define SPI_SETDRAGFULLWINDOWS    37
#define SPI_GETDRAGFULLWINDOWS	  38

#define SPI_GETFILTERKEYS         50
#define SPI_SETFILTERKEYS         51
#define SPI_GETTOGGLEKEYS         52
#define SPI_SETTOGGLEKEYS         53
#define SPI_GETMOUSEKEYS          54
#define SPI_SETMOUSEKEYS          55
#define SPI_GETSHOWSOUNDS         56
#define SPI_SETSHOWSOUNDS         57
#define SPI_GETSTICKYKEYS         58
#define SPI_SETSTICKYKEYS         59
#define SPI_GETACCESSTIMEOUT      60
#define SPI_SETACCESSTIMEOUT      61

#define SPI_GETSOUNDSENTRY        64
#define SPI_SETSOUNDSENTRY        65

/* defines below are for all win versions WINVER >= 0x0400 */
#define SPI_SETDRAGFULLWINDOWS    37
#define SPI_GETDRAGFULLWINDOWS    38
#define SPI_GETNONCLIENTMETRICS   41
#define SPI_SETNONCLIENTMETRICS   42
#define SPI_GETMINIMIZEDMETRICS   43
#define SPI_SETMINIMIZEDMETRICS   44
#define SPI_GETICONMETRICS        45
#define SPI_SETICONMETRICS        46
#define SPI_SETWORKAREA           47
#define SPI_GETWORKAREA           48
#define SPI_SETPENWINDOWS         49

#define SPI_GETSERIALKEYS         62
#define SPI_SETSERIALKEYS         63
#define SPI_GETHIGHCONTRAST       66
#define SPI_SETHIGHCONTRAST       67
#define SPI_GETKEYBOARDPREF       68
#define SPI_SETKEYBOARDPREF       69
#define SPI_GETSCREENREADER       70
#define SPI_SETSCREENREADER       71
#define SPI_GETANIMATION          72
#define SPI_SETANIMATION          73
#define SPI_GETFONTSMOOTHING      74
#define SPI_SETFONTSMOOTHING      75
#define SPI_SETDRAGWIDTH          76
#define SPI_SETDRAGHEIGHT         77
#define SPI_SETHANDHELD           78
#define SPI_GETLOWPOWERTIMEOUT    79
#define SPI_GETPOWEROFFTIMEOUT    80
#define SPI_SETLOWPOWERTIMEOUT    81
#define SPI_SETPOWEROFFTIMEOUT    82
#define SPI_GETLOWPOWERACTIVE     83
#define SPI_GETPOWEROFFACTIVE     84
#define SPI_SETLOWPOWERACTIVE     85
#define SPI_SETPOWEROFFACTIVE     86
#define SPI_SETCURSORS            87
#define SPI_SETICONS              88
#define SPI_GETDEFAULTINPUTLANG   89
#define SPI_SETDEFAULTINPUTLANG   90
#define SPI_SETLANGTOGGLE         91
#define SPI_GETWINDOWSEXTENSION   92
#define SPI_SETMOUSETRAILS        93
#define SPI_GETMOUSETRAILS        94
#define SPI_SETSCREENSAVERRUNNING 97
#define SPI_SCREENSAVERRUNNING    SPI_SETSCREENSAVERRUNNING

/* defines below are for all win versions (_WIN32_WINNT >= 0x0400) ||
 *                                        (_WIN32_WINDOWS > 0x0400) */
#define SPI_GETMOUSEHOVERWIDTH    98
#define SPI_SETMOUSEHOVERWIDTH    99
#define SPI_GETMOUSEHOVERHEIGHT   100
#define SPI_SETMOUSEHOVERHEIGHT   101
#define SPI_GETMOUSEHOVERTIME     102
#define SPI_SETMOUSEHOVERTIME     103
#define SPI_GETWHEELSCROLLLINES   104
#define SPI_SETWHEELSCROLLLINES   105

#define SPI_GETSHOWIMEUI          110
#define SPI_SETSHOWIMEUI          111

/* defines below are for all win versions WINVER >= 0x0500 */
#define SPI_GETMOUSESPEED         112
#define SPI_SETMOUSESPEED         113
#define SPI_GETSCREENSAVERRUNNING 114

#define SPI_GETACTIVEWINDOWTRACKING    0x1000
#define SPI_SETACTIVEWINDOWTRACKING    0x1001
#define SPI_GETMENUANIMATION           0x1002
#define SPI_SETMENUANIMATION           0x1003
#define SPI_GETCOMBOBOXANIMATION       0x1004
#define SPI_SETCOMBOBOXANIMATION       0x1005
#define SPI_GETLISTBOXSMOOTHSCROLLING  0x1006
#define SPI_SETLISTBOXSMOOTHSCROLLING  0x1007
#define SPI_GETGRADIENTCAPTIONS        0x1008
#define SPI_SETGRADIENTCAPTIONS        0x1009
#define SPI_GETMENUUNDERLINES          0x100A
#define SPI_SETMENUUNDERLINES          0x100B
#define SPI_GETACTIVEWNDTRKZORDER      0x100C
#define SPI_SETACTIVEWNDTRKZORDER      0x100D
#define SPI_GETHOTTRACKING             0x100E
#define SPI_SETHOTTRACKING             0x100F
#define SPI_GETFOREGROUNDLOCKTIMEOUT   0x2000
#define SPI_SETFOREGROUNDLOCKTIMEOUT   0x2001
#define SPI_GETACTIVEWNDTRKTIMEOUT     0x2002
#define SPI_SETACTIVEWNDTRKTIMEOUT     0x2003
#define SPI_GETFOREGROUNDFLASHCOUNT    0x2004
#define SPI_SETFOREGROUNDFLASHCOUNT    0x2005

/* SystemParametersInfo flags */

#define SPIF_UPDATEINIFILE              1
#define SPIF_SENDWININICHANGE           2
#define SPIF_SENDCHANGE                 SPIF_SENDWININICHANGE




/* Window Styles */
#define WS_OVERLAPPED    0x00000000L
#define WS_POPUP         0x80000000L
#define WS_CHILD         0x40000000L
#define WS_MINIMIZE      0x20000000L
#define WS_VISIBLE       0x10000000L
#define WS_DISABLED      0x08000000L
#define WS_CLIPSIBLINGS  0x04000000L
#define WS_CLIPCHILDREN  0x02000000L
#define WS_MAXIMIZE      0x01000000L
#define WS_CAPTION       0x00C00000L
#define WS_BORDER        0x00800000L
#define WS_DLGFRAME      0x00400000L
#define WS_VSCROLL       0x00200000L
#define WS_HSCROLL       0x00100000L
#define WS_SYSMENU       0x00080000L
#define WS_THICKFRAME    0x00040000L
#define WS_GROUP         0x00020000L
#define WS_TABSTOP       0x00010000L
#define WS_MINIMIZEBOX   0x00020000L
#define WS_MAXIMIZEBOX   0x00010000L
#define WS_TILED         WS_OVERLAPPED
#define WS_ICONIC        WS_MINIMIZE
#define WS_SIZEBOX       WS_THICKFRAME
#define WS_OVERLAPPEDWINDOW (WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_THICKFRAME| WS_MINIMIZEBOX | WS_MAXIMIZEBOX)
#define WS_POPUPWINDOW (WS_POPUP | WS_BORDER | WS_SYSMENU)
#define WS_CHILDWINDOW (WS_CHILD)
#define WS_TILEDWINDOW (WS_OVERLAPPEDWINDOW)

/* Window extended styles */
#define WS_EX_DLGMODALFRAME    0x00000001L
#define WS_EX_DRAGDETECT       0x00000002L
#define WS_EX_NOPARENTNOTIFY   0x00000004L
#define WS_EX_TOPMOST          0x00000008L
#define WS_EX_ACCEPTFILES      0x00000010L
#define WS_EX_TRANSPARENT      0x00000020L

/* New Win95/WinNT4 styles */
#define WS_EX_MDICHILD         0x00000040L
#define WS_EX_TOOLWINDOW       0x00000080L
#define WS_EX_WINDOWEDGE       0x00000100L
#define WS_EX_CLIENTEDGE       0x00000200L
#define WS_EX_CONTEXTHELP      0x00000400L
#define WS_EX_RIGHT            0x00001000L
#define WS_EX_LEFT             0x00000000L
#define WS_EX_RTLREADING       0x00002000L
#define WS_EX_LTRREADING       0x00000000L
#define WS_EX_LEFTSCROLLBAR    0x00004000L
#define WS_EX_RIGHTSCROLLBAR   0x00000000L
#define WS_EX_CONTROLPARENT    0x00010000L
#define WS_EX_STATICEDGE       0x00020000L
#define WS_EX_APPWINDOW        0x00040000L

#define WS_EX_OVERLAPPEDWINDOW (WS_EX_WINDOWEDGE|WS_EX_CLIENTEDGE)
#define WS_EX_PALETTEWINDOW    (WS_EX_WINDOWEDGE|WS_EX_TOOLWINDOW|WS_EX_TOPMOST)

/* WINE internal... */
#define WS_EX_TRAYWINDOW	0x80000000L

/* Window scrolling */
#define SW_SCROLLCHILDREN      0x0001
#define SW_INVALIDATE          0x0002
#define SW_ERASE               0x0004

/* CreateWindow() coordinates */
#define CW_USEDEFAULT ((INT)0x80000000)

/* ChildWindowFromPointEx Flags */
#define CWP_ALL                0x0000
#define CWP_SKIPINVISIBLE      0x0001
#define CWP_SKIPDISABLED       0x0002
#define CWP_SKIPTRANSPARENT    0x0004

  /* PeekMessage() options */
#define PM_NOREMOVE	0x0000
#define PM_REMOVE	0x0001
#define PM_NOYIELD	0x0002

/* WM_SHOWWINDOW wParam codes */
#define SW_PARENTCLOSING    1
#define SW_OTHERMAXIMIZED   2
#define SW_PARENTOPENING    3
#define SW_OTHERRESTORED    4

  /* ShowWindow() codes */
#define SW_HIDE             0
#define SW_SHOWNORMAL       1
#define SW_NORMAL           1
#define SW_SHOWMINIMIZED    2
#define SW_SHOWMAXIMIZED    3
#define SW_MAXIMIZE         3
#define SW_SHOWNOACTIVATE   4
#define SW_SHOW             5
#define SW_MINIMIZE         6
#define SW_SHOWMINNOACTIVE  7
#define SW_SHOWNA           8
#define SW_RESTORE          9
#define SW_SHOWDEFAULT	    10
#define SW_MAX		    10
#define SW_NORMALNA	    0xCC	/* undoc. flag in MinMaximize */

  /* WM_SIZE message wParam values */
#define SIZE_RESTORED        0
#define SIZE_MINIMIZED       1
#define SIZE_MAXIMIZED       2
#define SIZE_MAXSHOW         3
#define SIZE_MAXHIDE         4
#define SIZENORMAL           SIZE_RESTORED
#define SIZEICONIC           SIZE_MINIMIZED
#define SIZEFULLSCREEN       SIZE_MAXIMIZED
#define SIZEZOOMSHOW         SIZE_MAXSHOW
#define SIZEZOOMHIDE         SIZE_MAXHIDE

/* SetWindowPos() and WINDOWPOS flags */
#define SWP_NOSIZE          0x0001
#define SWP_NOMOVE          0x0002
#define SWP_NOZORDER        0x0004
#define SWP_NOREDRAW        0x0008
#define SWP_NOACTIVATE      0x0010
#define SWP_FRAMECHANGED    0x0020  /* The frame changed: send WM_NCCALCSIZE */
#define SWP_SHOWWINDOW      0x0040
#define SWP_HIDEWINDOW      0x0080
#define SWP_NOCOPYBITS      0x0100
#define SWP_NOOWNERZORDER   0x0200  /* Don't do owner Z ordering */

#define SWP_DRAWFRAME       SWP_FRAMECHANGED
#define SWP_NOREPOSITION    SWP_NOOWNERZORDER

#define SWP_NOSENDCHANGING  0x0400
#define SWP_DEFERERASE      0x2000
#define SWP_ASYNCWINDOWPOS  0x4000

#define HWND_DESKTOP        ((HWND)0)
#define HWND_BROADCAST      ((HWND)0xffff)

/* SetWindowPos() hwndInsertAfter field values */
#define HWND_TOP            ((HWND)0)
#define HWND_BOTTOM         ((HWND)1)
#define HWND_TOPMOST        ((HWND)-1)
#define HWND_NOTOPMOST      ((HWND)-2)

#define MF_INSERT          0x0000
#define MF_CHANGE          0x0080
#define MF_APPEND          0x0100
#define MF_DELETE          0x0200
#define MF_REMOVE          0x1000
#define MF_END             0x0080

#define MF_ENABLED         0x0000
#define MF_GRAYED          0x0001
#define MF_DISABLED        0x0002
#define MF_STRING          0x0000
#define MF_BITMAP          0x0004
#define MF_UNCHECKED       0x0000
#define MF_CHECKED         0x0008
#define MF_POPUP           0x0010
#define MF_MENUBARBREAK    0x0020
#define MF_MENUBREAK       0x0040
#define MF_UNHILITE        0x0000
#define MF_HILITE          0x0080
#define MF_OWNERDRAW       0x0100
#define MF_USECHECKBITMAPS 0x0200
#define MF_BYCOMMAND       0x0000
#define MF_BYPOSITION      0x0400
#define MF_SEPARATOR       0x0800
#define MF_DEFAULT         0x1000
#define MF_SYSMENU         0x2000
#define MF_HELP            0x4000
#define MF_RIGHTJUSTIFY    0x4000
#define MF_MOUSESELECT     0x8000

/* Flags for extended menu item types.  */
#define MFT_STRING         MF_STRING
#define MFT_BITMAP         MF_BITMAP
#define MFT_MENUBARBREAK   MF_MENUBARBREAK
#define MFT_MENUBREAK      MF_MENUBREAK
#define MFT_OWNERDRAW      MF_OWNERDRAW
#define MFT_RADIOCHECK     0x00000200L
#define MFT_SEPARATOR      MF_SEPARATOR
#define MFT_RIGHTORDER     0x00002000L
#define MFT_RIGHTJUSTIFY   MF_RIGHTJUSTIFY

/* Flags for extended menu item states.  */
#define MFS_GRAYED          0x00000003L
#define MFS_DISABLED        MFS_GRAYED
#define MFS_CHECKED         MF_CHECKED
#define MFS_HILITE          MF_HILITE
#define MFS_ENABLED         MF_ENABLED
#define MFS_UNCHECKED       MF_UNCHECKED
#define MFS_UNHILITE        MF_UNHILITE
#define MFS_DEFAULT         MF_DEFAULT
#define MFS_MASK            0x0000108BL
#define MFS_HOTTRACKDRAWN   0x10000000L
#define MFS_CACHEDBMP       0x20000000L
#define MFS_BOTTOMGAPDROP   0x40000000L
#define MFS_TOPGAPDROP      0x80000000L
#define MFS_GAPDROP         0xC0000000L

/* for GetMenuDefaultItem */
#define GMDI_USEDISABLED    0x0001L
#define GMDI_GOINTOPOPUPS   0x0002L

#define DT_TOP 0
#define DT_LEFT 0
#define DT_CENTER 1
#define DT_RIGHT 2
#define DT_VCENTER 4
#define DT_BOTTOM 8
#define DT_WORDBREAK 16
#define DT_SINGLELINE 32
#define DT_EXPANDTABS 64
#define DT_TABSTOP 128
#define DT_NOCLIP 256
#define DT_EXTERNALLEADING 512
#define DT_CALCRECT 1024
#define DT_NOPREFIX 2048
#define DT_INTERNAL 4096

/* DrawCaption()/DrawCaptionTemp() flags */
#define DC_ACTIVE		0x0001
#define DC_SMALLCAP		0x0002
#define DC_ICON			0x0004
#define DC_TEXT			0x0008
#define DC_INBUTTON		0x0010

/* DrawEdge() flags */
#define BDR_RAISEDOUTER    0x0001
#define BDR_SUNKENOUTER    0x0002
#define BDR_RAISEDINNER    0x0004
#define BDR_SUNKENINNER    0x0008

#define BDR_OUTER          0x0003
#define BDR_INNER          0x000c
#define BDR_RAISED         0x0005
#define BDR_SUNKEN         0x000a

#define EDGE_RAISED        (BDR_RAISEDOUTER | BDR_RAISEDINNER)
#define EDGE_SUNKEN        (BDR_SUNKENOUTER | BDR_SUNKENINNER)
#define EDGE_ETCHED        (BDR_SUNKENOUTER | BDR_RAISEDINNER)
#define EDGE_BUMP          (BDR_RAISEDOUTER | BDR_SUNKENINNER)

/* border flags */
#define BF_LEFT            0x0001
#define BF_TOP             0x0002
#define BF_RIGHT           0x0004
#define BF_BOTTOM          0x0008
#define BF_DIAGONAL        0x0010
#define BF_MIDDLE          0x0800  /* Fill in the middle */
#define BF_SOFT            0x1000  /* For softer buttons */
#define BF_ADJUST          0x2000  /* Calculate the space left over */
#define BF_FLAT            0x4000  /* For flat rather than 3D borders */
#define BF_MONO            0x8000  /* For monochrome borders */
#define BF_TOPLEFT         (BF_TOP | BF_LEFT)
#define BF_TOPRIGHT        (BF_TOP | BF_RIGHT)
#define BF_BOTTOMLEFT      (BF_BOTTOM | BF_LEFT)
#define BF_BOTTOMRIGHT     (BF_BOTTOM | BF_RIGHT)
#define BF_RECT            (BF_LEFT | BF_TOP | BF_RIGHT | BF_BOTTOM)
#define BF_DIAGONAL_ENDTOPRIGHT     (BF_DIAGONAL | BF_TOP | BF_RIGHT)
#define BF_DIAGONAL_ENDTOPLEFT      (BF_DIAGONAL | BF_TOP | BF_LEFT)
#define BF_DIAGONAL_ENDBOTTOMLEFT   (BF_DIAGONAL | BF_BOTTOM | BF_LEFT)
#define BF_DIAGONAL_ENDBOTTOMRIGHT  (BF_DIAGONAL | BF_BOTTOM | BF_RIGHT)

/* DrawFrameControl() uType's */

#define DFC_CAPTION             1
#define DFC_MENU                2
#define DFC_SCROLL              3
#define DFC_BUTTON              4

/* uState's */

#define DFCS_CAPTIONCLOSE       0x0000
#define DFCS_CAPTIONMIN         0x0001
#define DFCS_CAPTIONMAX         0x0002
#define DFCS_CAPTIONRESTORE     0x0003
#define DFCS_CAPTIONHELP        0x0004		/* Windows 95 only */

#define DFCS_MENUARROW          0x0000
#define DFCS_MENUCHECK          0x0001
#define DFCS_MENUBULLET         0x0002
#define DFCS_MENUARROWRIGHT     0x0004

#define DFCS_SCROLLUP            0x0000
#define DFCS_SCROLLDOWN          0x0001
#define DFCS_SCROLLLEFT          0x0002
#define DFCS_SCROLLRIGHT         0x0003
#define DFCS_SCROLLCOMBOBOX      0x0005
#define DFCS_SCROLLSIZEGRIP      0x0008
#define DFCS_SCROLLSIZEGRIPRIGHT 0x0010

#define DFCS_BUTTONCHECK        0x0000
#define DFCS_BUTTONRADIOIMAGE   0x0001
#define DFCS_BUTTONRADIOMASK    0x0002		/* to draw nonsquare button */
#define DFCS_BUTTONRADIO        0x0004
#define DFCS_BUTTON3STATE       0x0008
#define DFCS_BUTTONPUSH         0x0010

/* additional state of the control */

#define DFCS_INACTIVE           0x0100
#define DFCS_PUSHED             0x0200
#define DFCS_CHECKED            0x0400
#define DFCS_ADJUSTRECT         0x2000		/* exclude surrounding edge */
#define DFCS_FLAT               0x4000
#define DFCS_MONO               0x8000

/* Image type */
#define	DST_COMPLEX	0x0000
#define	DST_TEXT	0x0001
#define	DST_PREFIXTEXT	0x0002
#define	DST_ICON	0x0003
#define	DST_BITMAP	0x0004

/* State type */
#define	DSS_NORMAL	0x0000
#define	DSS_UNION	0x0010  /* Gray string appearance */
#define	DSS_DISABLED	0x0020
#define	DSS_DEFAULT	0x0040  /* Make it bold */
#define	DSS_MONO	0x0080
#define	DSS_RIGHT	0x8000

typedef struct
{
    UINT      CtlType;
    UINT      CtlID;
    UINT      itemID;
    UINT      itemAction;
    UINT      itemState;
    HWND      hwndItem;
    HDC       hDC;
    RECT      rcItem WINE_PACKED;
    DWORD       itemData WINE_PACKED;
} DRAWITEMSTRUCT, *PDRAWITEMSTRUCT, *LPDRAWITEMSTRUCT;


typedef struct
{
    UINT      CtlType;
    UINT      CtlID;
    UINT      itemID;
    UINT      itemWidth;
    UINT      itemHeight;
    DWORD       itemData;
} MEASUREITEMSTRUCT, *PMEASUREITEMSTRUCT, *LPMEASUREITEMSTRUCT;


typedef struct
{
    UINT     CtlType;
    UINT     CtlID;
    UINT     itemID;
    HWND     hwndItem;
    DWORD      itemData;
} DELETEITEMSTRUCT, *LPDELETEITEMSTRUCT;


typedef struct
{
    UINT      CtlType;
    UINT      CtlID;
    HWND      hwndItem;
    UINT      itemID1;
    DWORD       itemData1;
    UINT      itemID2;
    DWORD       itemData2;
    DWORD       dwLocaleId;
} COMPAREITEMSTRUCT, *PCOMPAREITEMSTRUCT, *LPCOMPAREITEMSTRUCT;


/* WM_KEYUP/DOWN/CHAR HIWORD(lParam) flags */
#define KF_EXTENDED         0x0100
#define KF_DLGMODE          0x0800
#define KF_MENUMODE         0x1000
#define KF_ALTDOWN          0x2000
#define KF_REPEAT           0x4000
#define KF_UP               0x8000

/* Virtual key codes */
#define VK_LBUTTON          0x01
#define VK_RBUTTON          0x02
#define VK_CANCEL           0x03
#define VK_MBUTTON          0x04
/*                          0x05-0x07  Undefined */
#define VK_BACK             0x08
#define VK_TAB              0x09
/*                          0x0A-0x0B  Undefined */
#define VK_CLEAR            0x0C
#define VK_RETURN           0x0D
/*                          0x0E-0x0F  Undefined */
#define VK_SHIFT            0x10
#define VK_CONTROL          0x11
#define VK_MENU             0x12
#define VK_PAUSE            0x13
#define VK_CAPITAL          0x14
/*                          0x15-0x19  Reserved for Kanji systems */
/*                          0x1A       Undefined */
#define VK_ESCAPE           0x1B
/*                          0x1C-0x1F  Reserved for Kanji systems */
#define VK_SPACE            0x20
#define VK_PRIOR            0x21
#define VK_NEXT             0x22
#define VK_END              0x23
#define VK_HOME             0x24
#define VK_LEFT             0x25
#define VK_UP               0x26
#define VK_RIGHT            0x27
#define VK_DOWN             0x28
#define VK_SELECT           0x29
#define VK_PRINT            0x2A /* OEM specific in Windows 3.1 SDK */
#define VK_EXECUTE          0x2B
#define VK_SNAPSHOT         0x2C
#define VK_INSERT           0x2D
#define VK_DELETE           0x2E
#define VK_HELP             0x2F
#define VK_0                0x30
#define VK_1                0x31
#define VK_2                0x32
#define VK_3                0x33
#define VK_4                0x34
#define VK_5                0x35
#define VK_6                0x36
#define VK_7                0x37
#define VK_8                0x38
#define VK_9                0x39
/*                          0x3A-0x40  Undefined */
#define VK_A                0x41
#define VK_B                0x42
#define VK_C                0x43
#define VK_D                0x44
#define VK_E                0x45
#define VK_F                0x46
#define VK_G                0x47
#define VK_H                0x48
#define VK_I                0x49
#define VK_J                0x4A
#define VK_K                0x4B
#define VK_L                0x4C
#define VK_M                0x4D
#define VK_N                0x4E
#define VK_O                0x4F
#define VK_P                0x50
#define VK_Q                0x51
#define VK_R                0x52
#define VK_S                0x53
#define VK_T                0x54
#define VK_U                0x55
#define VK_V                0x56
#define VK_W                0x57
#define VK_X                0x58
#define VK_Y                0x59
#define VK_Z                0x5A

#define VK_LWIN             0x5B
#define VK_RWIN             0x5C
#define VK_APPS             0x5D
/*                          0x5E-0x5F Unassigned */
#define VK_NUMPAD0          0x60
#define VK_NUMPAD1          0x61
#define VK_NUMPAD2          0x62
#define VK_NUMPAD3          0x63
#define VK_NUMPAD4          0x64
#define VK_NUMPAD5          0x65
#define VK_NUMPAD6          0x66
#define VK_NUMPAD7          0x67
#define VK_NUMPAD8          0x68
#define VK_NUMPAD9          0x69
#define VK_MULTIPLY         0x6A
#define VK_ADD              0x6B
#define VK_SEPARATOR        0x6C
#define VK_SUBTRACT         0x6D
#define VK_DECIMAL          0x6E
#define VK_DIVIDE           0x6F
#define VK_F1               0x70
#define VK_F2               0x71
#define VK_F3               0x72
#define VK_F4               0x73
#define VK_F5               0x74
#define VK_F6               0x75
#define VK_F7               0x76
#define VK_F8               0x77
#define VK_F9               0x78
#define VK_F10              0x79
#define VK_F11              0x7A
#define VK_F12              0x7B
#define VK_F13              0x7C
#define VK_F14              0x7D
#define VK_F15              0x7E
#define VK_F16              0x7F
#define VK_F17              0x80
#define VK_F18              0x81
#define VK_F19              0x82
#define VK_F20              0x83
#define VK_F21              0x84
#define VK_F22              0x85
#define VK_F23              0x86
#define VK_F24              0x87
/*                          0x88-0x8F  Unassigned */
#define VK_NUMLOCK          0x90
#define VK_SCROLL           0x91
/*                          0x92-0x9F  Unassigned */
/*
 * differencing between right and left shift/control/alt key.
 * Used only by GetAsyncKeyState() and GetKeyState().
 */
#define VK_LSHIFT           0xA0
#define VK_RSHIFT           0xA1
#define VK_LCONTROL         0xA2
#define VK_RCONTROL         0xA3
#define VK_LMENU            0xA4
#define VK_RMENU            0xA5
/*                          0xA6-0xB9  Unassigned */
#define VK_OEM_1            0xBA
#define VK_OEM_PLUS         0xBB
#define VK_OEM_COMMA        0xBC
#define VK_OEM_MINUS        0xBD
#define VK_OEM_PERIOD       0xBE
#define VK_OEM_2            0xBF
#define VK_OEM_3            0xC0
/*                          0xC1-0xDA  Unassigned */
#define VK_OEM_4            0xDB
#define VK_OEM_5            0xDC
#define VK_OEM_6            0xDD
#define VK_OEM_7            0xDE
/*                          0xDF-0xE4  OEM specific */

#define VK_PROCESSKEY       0xE5

/*                          0xE6       OEM specific */
/*                          0xE7-0xE8  Unassigned */
/*                          0xE9-0xF5  OEM specific */

#define VK_ATTN             0xF6
#define VK_CRSEL            0xF7
#define VK_EXSEL            0xF8
#define VK_EREOF            0xF9
#define VK_PLAY             0xFA
#define VK_ZOOM             0xFB
#define VK_NONAME           0xFC
#define VK_PA1              0xFD
#define VK_OEM_CLEAR        0xFE
  
  /* Key status flags for mouse events */
#define MK_LBUTTON	    0x0001
#define MK_RBUTTON	    0x0002
#define MK_SHIFT	    0x0004
#define MK_CONTROL	    0x0008
#define MK_MBUTTON	    0x0010

  /* Queue status flags */
#define QS_KEY		0x0001
#define QS_MOUSEMOVE	0x0002
#define QS_MOUSEBUTTON	0x0004
#define QS_MOUSE	(QS_MOUSEMOVE | QS_MOUSEBUTTON)
#define QS_POSTMESSAGE	0x0008
#define QS_TIMER	0x0010
#define QS_PAINT	0x0020
#define QS_SENDMESSAGE	0x0040
#define QS_HOTKEY	0x0080
#define QS_INPUT	(QS_MOUSE | QS_KEY)
#define QS_ALLEVENTS	(QS_INPUT | QS_POSTMESSAGE | QS_TIMER | QS_PAINT | QS_HOTKEY)
#define QS_ALLINPUT     (QS_ALLEVENTS | QS_SENDMESSAGE)

#define DDL_READWRITE	0x0000
#define DDL_READONLY	0x0001
#define DDL_HIDDEN	0x0002
#define DDL_SYSTEM	0x0004
#define DDL_DIRECTORY	0x0010
#define DDL_ARCHIVE	0x0020

#define DDL_POSTMSGS	0x2000
#define DDL_DRIVES	0x4000
#define DDL_EXCLUSIVE	0x8000

  /* Shell hook values */
#define HSHELL_WINDOWCREATED       1
#define HSHELL_WINDOWDESTROYED     2
#define HSHELL_ACTIVATESHELLWINDOW 3

/* Predefined Clipboard Formats */
#define CF_TEXT              1
#define CF_BITMAP            2
#define CF_METAFILEPICT      3
#define CF_SYLK              4
#define CF_DIF               5
#define CF_TIFF              6
#define CF_OEMTEXT           7
#define CF_DIB               8
#define CF_PALETTE           9
#define CF_PENDATA          10
#define CF_RIFF             11
#define CF_WAVE             12
#define CF_ENHMETAFILE      14
#define CF_HDROP            15
#define CF_LOCALE           16
#define CF_MAX              17

#define CF_OWNERDISPLAY     0x0080
#define CF_DSPTEXT          0x0081
#define CF_DSPBITMAP        0x0082
#define CF_DSPMETAFILEPICT  0x0083

/* "Private" formats don't get GlobalFree()'d */
#define CF_PRIVATEFIRST     0x0200
#define CF_PRIVATELAST      0x02FF

/* "GDIOBJ" formats do get DeleteObject()'d */
#define CF_GDIOBJFIRST      0x0300
#define CF_GDIOBJLAST       0x03FF


/* DragObject stuff */

typedef struct
{
    HWND16     hWnd;
    HANDLE16   hScope;
    WORD       wFlags;
    HANDLE16   hList;
    HANDLE16   hOfStruct;
    POINT16 pt WINE_PACKED;
    LONG       l WINE_PACKED;
} DRAGINFO, *LPDRAGINFO;

#define DRAGOBJ_PROGRAM		0x0001
#define DRAGOBJ_DATA		0x0002
#define DRAGOBJ_DIRECTORY	0x0004
#define DRAGOBJ_MULTIPLE	0x0008
#define DRAGOBJ_EXTERNAL	0x8000

#define DRAG_PRINT		0x544E5250
#define DRAG_FILE		0x454C4946

/* types of LoadImage */
#define IMAGE_BITMAP	0
#define IMAGE_ICON	1
#define IMAGE_CURSOR	2
#define IMAGE_ENHMETAFILE	3

/* loadflags to LoadImage */
#define LR_DEFAULTCOLOR		0x0000
#define LR_MONOCHROME		0x0001
#define LR_COLOR		0x0002
#define LR_COPYRETURNORG	0x0004
#define LR_COPYDELETEORG	0x0008
#define LR_LOADFROMFILE		0x0010
#define LR_LOADTRANSPARENT	0x0020
#define LR_DEFAULTSIZE		0x0040
#define LR_VGA_COLOR		0x0080
#define LR_LOADMAP3DCOLORS	0x1000
#define	LR_CREATEDIBSECTION	0x2000
#define LR_COPYFROMRESOURCE	0x4000
#define LR_SHARED		0x8000

/* Flags for DrawIconEx.  */
#define DI_MASK                 1
#define DI_IMAGE                2
#define DI_NORMAL               (DI_MASK | DI_IMAGE)
#define DI_COMPAT               4
#define DI_DEFAULTSIZE          8

  /* misc messages */
#define WM_CPL_LAUNCH       (WM_USER + 1000)
#define WM_CPL_LAUNCHED     (WM_USER + 1001)

/* WM_NOTIFYFORMAT commands and return values */
#define NFR_ANSI	    1
#define NFR_UNICODE	    2
#define NF_QUERY	    3
#define NF_REQUERY	    4

#include "poppack.h"
#define     EnumTaskWindows(handle,proc,lparam) \
            EnumThreadWindows(handle,proc,lparam)
#define     OemToAnsiA OemToCharA
#define     OemToAnsiW OemToCharW
#define     OemToAnsi WINELIB_NAME_AW(OemToAnsi)
#define     OemToAnsiBuffA OemToCharBuffA
#define     OemToAnsiBuffW OemToCharBuffW
#define     OemToAnsiBuff WINELIB_NAME_AW(OemToAnsiBuff)
#define     AnsiToOemA CharToOemA
#define     AnsiToOemW CharToOemW
#define     AnsiToOem WINELIB_NAME_AW(AnsiToOem)
#define     AnsiToOemBuffA CharToOemBuffA
#define     AnsiToOemBuffW CharToOemBuffW
#define     AnsiToOemBuff WINELIB_NAME_AW(AnsiToOemBuff)
/* NOTE: This is SYSTEM.3, not USER.182, which is also named KillSystemTimer */
WORD        WINAPI SYSTEM_KillSystemTimer( WORD );

/* Extra functions that don't exist in the Windows API */

HPEN      WINAPI GetSysColorPen(INT);
INT       WINAPI LoadMessageA(HMODULE,UINT,WORD,LPSTR,INT);
INT       WINAPI LoadMessageW(HMODULE,UINT,WORD,LPWSTR,INT);

VOID        WINAPI ScreenSwitchEnable16(WORD);

#define WC_DIALOG    (LPSTR)((DWORD)((WORD)( 0x8002)))

#ifdef __cplusplus
}
#endif

#endif /* _WINUSER_ */
