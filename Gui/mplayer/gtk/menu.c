
#include <stdio.h>
#include <stdlib.h>

#include "../../events.h"
#include "menu.h"
#include "../widgets.h"

void ActivateMenuItem( int Item )
{
// fprintf( stderr,"[menu] item: %d\n",Item );
 gtkShMem->popupmenu=Item;
 gtkSendMessage( evShowPopUpMenu );
}

GtkWidget * AddMenuItem( GtkWidget * Menu,char * label,int Number )
{
 GtkWidget * Item = NULL;
 Item=gtk_menu_item_new_with_label( label );
 gtk_menu_append( GTK_MENU( Menu ),Item );
 gtk_signal_connect_object( GTK_OBJECT(Item),"activate",
   GTK_SIGNAL_FUNC(ActivateMenuItem),(gpointer)Number );
 gtk_widget_show( Item );
 return Item;
}

GtkWidget * AddSubMenu( GtkWidget * Menu,char * label )
{
 GtkWidget * Item = NULL;
 GtkWidget * SubItem = NULL;

 SubItem=gtk_menu_item_new_with_label( label );
 gtk_menu_append( GTK_MENU( Menu ),SubItem );
 gtk_widget_show( SubItem );

 Item=gtk_menu_new();
 gtk_widget_show( Item );
 gtk_menu_item_set_submenu( GTK_MENU_ITEM( SubItem ),Item );
 return Item;
}

GtkWidget * AddSeparator( GtkWidget * Menu )
{
 GtkWidget * Item = NULL;

 Item=gtk_menu_item_new ();
 gtk_widget_show( Item );
 gtk_container_add( GTK_CONTAINER( Menu ),Item );
 gtk_widget_set_sensitive( Item,FALSE );

 return Item;
}

GtkWidget * DVDSubMenu;
GtkWidget * DVDAudioLanguageMenu;
GtkWidget * DVDSubtitleLanguageMenu;

GtkWidget * create_PopUpMenu( void )
{
 GtkWidget * Menu = NULL;
 GtkWidget * SubMenu = NULL;
 GtkWidget * SubMenuItem = NULL;

 Menu=gtk_menu_new();

  AddMenuItem( Menu,"About MPlayer""      ", evAbout );
  AddSeparator( Menu );
   SubMenu=AddSubMenu( Menu,"Open ..." );
    AddMenuItem( SubMenu,"Play file ...""    ", evLoadPlay );
    AddMenuItem( SubMenu,"Play VCD ...", evNone );
    AddMenuItem( SubMenu,"Play DVD ...", evNone );
    AddMenuItem( SubMenu,"Play URL ...", evNone );
    AddMenuItem( SubMenu,"Load subtitle ...   ", evLoadSubtitle );
   SubMenu=AddSubMenu( Menu,"Playing" );
    AddMenuItem( SubMenu,"Play""        ", evPlay );
    AddMenuItem( SubMenu,"Pause", evPause );
    AddMenuItem( SubMenu,"Stop", evStop );
    AddMenuItem( SubMenu,"Prev stream", evPrev );
    AddMenuItem( SubMenu,"Next stream", evNext );
    AddSeparator( SubMenu );
    AddMenuItem( SubMenu,"Back 10 sec", evBackward10sec );
    AddMenuItem( SubMenu,"Fwd 10 sec", evForward10sec );
    AddMenuItem( SubMenu,"Back 1 min", evBackward1min );
    AddMenuItem( SubMenu,"Fwd 1 min", evForward1min );
    AddMenuItem( SubMenu,"Back 10 min", evBackward10min );
    AddMenuItem( SubMenu,"Fwk 10 min", evForward10min );
   SubMenu=AddSubMenu( Menu,"Size" );
    AddMenuItem( SubMenu,"Normal size""      ", evNormalSize );
    AddMenuItem( SubMenu,"Double size", evDoubleSize );
    AddMenuItem( SubMenu,"Fullscreen", evFullScreen );
   DVDSubMenu=AddSubMenu( Menu,"DVD" );
    AddMenuItem( DVDSubMenu,"Play disc ...""    ", evNone );
    AddMenuItem( DVDSubMenu,"Show DVD Menu", evNone );
    AddSeparator( DVDSubMenu );
    DVDAudioLanguageMenu=AddSubMenu( DVDSubMenu,"Audio language" );
    DVDSubtitleLanguageMenu=AddSubMenu( DVDSubMenu,"Subtitle language" );
  AddSeparator( Menu );
  AddMenuItem( Menu,"Playlist", evPlayList );
  AddMenuItem( Menu,"Skin browser", evSkinBrowser );
  AddMenuItem( Menu,"Preferences", evPreferences );
  AddSeparator( Menu );
  AddMenuItem( Menu,"Exit ...", evExit );

 return Menu;
}
