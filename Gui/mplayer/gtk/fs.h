#ifndef __GUI_FS_H
#define __GUI_FS_H

#include <gtk/gtk.h>

#define fsVideoSelector    0
#define fsSubtitleSelector 1
#define fsOtherSelector    2

extern GtkWidget   * fsFileSelect;

extern void HideFileSelect( void );
extern void ShowFileSelect( int type, int modal );

extern GtkWidget * create_FileSelect( void );

#endif
