/*
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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "config.h"
#include "help_mp.h"
#include "access_mpcontext.h"
#include "mixer.h"

#include "menu.h"
#include "gui/mplayer/widgets.h"
#include "gui/mplayer/gmplayer.h"
#include "gui/app.h"

#include "stream/stream.h"
#include "libmpdemux/demuxer.h"
#include "libmpdemux/stheader.h"

#include "gui/mplayer/pixmaps/ab.xpm"
#include "gui/mplayer/pixmaps/half.xpm"
#include "gui/mplayer/pixmaps/normal.xpm"
#include "gui/mplayer/pixmaps/double.xpm"
#include "gui/mplayer/pixmaps/fs.xpm"
#include "gui/mplayer/pixmaps/exit.xpm"
#include "gui/mplayer/pixmaps/prefs.xpm"
#include "gui/mplayer/pixmaps/eq.xpm"
#include "gui/mplayer/pixmaps/pl.xpm"
#include "gui/mplayer/pixmaps/skin.xpm"
#include "gui/mplayer/pixmaps/sound.xpm"
#include "gui/mplayer/pixmaps/open.xpm"
#include "gui/mplayer/pixmaps/play.xpm"
#include "gui/mplayer/pixmaps/stop2.xpm"
#include "gui/mplayer/pixmaps/pause.xpm"
#include "gui/mplayer/pixmaps/prev.xpm"
#include "gui/mplayer/pixmaps/next.xpm"
#include "gui/mplayer/pixmaps/aspect.xpm"
#include "gui/mplayer/pixmaps/a11.xpm"
#include "gui/mplayer/pixmaps/a169.xpm"
#include "gui/mplayer/pixmaps/a235.xpm"
#include "gui/mplayer/pixmaps/a43.xpm"
#include "gui/mplayer/pixmaps/file2.xpm"
#include "gui/mplayer/pixmaps/url.xpm"
#include "gui/mplayer/pixmaps/sub.xpm"
#include "gui/mplayer/pixmaps/delsub.xpm"
#include "gui/mplayer/pixmaps/empty.xpm"
#include "gui/mplayer/pixmaps/loadeaf.xpm"
#include "gui/mplayer/pixmaps/title.xpm"
#ifdef CONFIG_DVDREAD
#include "gui/mplayer/pixmaps/dvd.xpm"
#include "gui/mplayer/pixmaps/playdvd.xpm"
#include "gui/mplayer/pixmaps/chapter.xpm"
#include "gui/mplayer/pixmaps/dolby.xpm"
#include "gui/mplayer/pixmaps/tongue.xpm"
#include "gui/mplayer/pixmaps/tonguebla.xpm"
#include "gui/mplayer/pixmaps/empty1px.xpm"
#endif
#ifdef CONFIG_VCD
#include "gui/mplayer/pixmaps/vcd.xpm"
#include "gui/mplayer/pixmaps/playvcd.xpm"
#endif

void ActivateMenuItem( int Item )
{
// fprintf( stderr,"[menu] item: %d.%d\n",Item&0xffff,Item>>16 );
 gtkPopupMenu=Item & 0x0000ffff;
 gtkPopupMenuParam=Item >> 16;
 mplEventHandling( Item & 0x0000ffff,Item >> 16 );
}

static GtkWidget * AddMenuCheckItem(GtkWidget *window1, const char * immagine_xpm, GtkWidget* Menu,const char* label, gboolean state, int Number)
{
 GtkWidget * Label = NULL;
 GtkWidget * Pixmap = NULL;
 GtkWidget * hbox = NULL;
 GtkWidget * Item = NULL;

 GdkPixmap *PixmapIcon = NULL;
 GdkColor transparent;
 GdkBitmap *MaskIcon = NULL;

 PixmapIcon = gdk_pixmap_create_from_xpm_d (window1->window, &MaskIcon, &transparent,(gchar **)immagine_xpm );
 Pixmap = gtk_pixmap_new (PixmapIcon, MaskIcon);
 gdk_pixmap_unref (PixmapIcon);

 Item=gtk_check_menu_item_new();
 Label = gtk_label_new (label);
 
 hbox = gtk_hbox_new (FALSE, 8);
 gtk_box_pack_start (GTK_BOX (hbox), Pixmap, FALSE, FALSE, 0);
 gtk_box_pack_start (GTK_BOX (hbox), Label, FALSE, FALSE, 0);
 gtk_container_add (GTK_CONTAINER (Item), hbox);
 
 gtk_menu_append( GTK_MENU( Menu ),Item );
 
 gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(Item),state);
 gtk_signal_connect_object( GTK_OBJECT(Item),"activate",
   GTK_SIGNAL_FUNC(ActivateMenuItem),(gpointer)Number );
 gtk_menu_item_right_justify (GTK_MENU_ITEM (Item));
 gtk_widget_show_all(Item);
   
 return Item;
}
GtkWidget * AddMenuItem( GtkWidget *window1, const char * immagine_xpm,  GtkWidget * SubMenu,const char * label,int Number )
{
 GtkWidget * Label = NULL;
 GtkWidget * Pixmap = NULL;
 GtkWidget * hbox = NULL;
 GtkWidget * Item = NULL;
 GdkPixmap * PixmapIcon = NULL;
 GdkColor transparent;
 GdkBitmap * MaskIcon = NULL;

 PixmapIcon = gdk_pixmap_create_from_xpm_d (window1->window, &MaskIcon, &transparent,(gchar **)immagine_xpm );
 Pixmap = gtk_pixmap_new (PixmapIcon, MaskIcon);
 gdk_pixmap_unref (PixmapIcon);

 Item=gtk_menu_item_new();
 Label = gtk_label_new (label);

 hbox = gtk_hbox_new (FALSE, 8);
 gtk_box_pack_start (GTK_BOX (hbox), Pixmap, FALSE, FALSE, 0);
 gtk_box_pack_start (GTK_BOX (hbox), Label, FALSE, FALSE, 0);
 gtk_container_add (GTK_CONTAINER (Item), hbox);


 gtk_menu_append( GTK_MENU( SubMenu ),Item );
 gtk_signal_connect_object( GTK_OBJECT(Item),"activate",
   GTK_SIGNAL_FUNC(ActivateMenuItem),(gpointer)Number );

 gtk_menu_item_right_justify (GTK_MENU_ITEM (Item));
 gtk_widget_show_all(Item);
 return Item;
}


GtkWidget * AddSubMenu( GtkWidget *window1, const char * immagine_xpm, GtkWidget * Menu,const char * label )
{
 GtkWidget * Label = NULL;
 GtkWidget * Pixmap = NULL;
 GtkWidget * hbox = NULL;
 GtkWidget * Item = NULL;
 GtkWidget * SubItem = NULL;
 GdkPixmap * PixmapIcon = NULL;
 GdkColor transparent;
 GdkBitmap * MaskIcon = NULL;

 PixmapIcon = gdk_pixmap_create_from_xpm_d (window1->window, &MaskIcon, &transparent,(gchar **)immagine_xpm);
 Pixmap = gtk_pixmap_new (PixmapIcon, MaskIcon);
 gdk_pixmap_unref (PixmapIcon);

 SubItem=gtk_menu_item_new();
 Item=gtk_menu_new();
 Label = gtk_label_new (label);

 hbox = gtk_hbox_new (FALSE, 8);
 gtk_box_pack_start (GTK_BOX (hbox), Pixmap, FALSE, FALSE, 0);
 gtk_box_pack_start (GTK_BOX (hbox), Label, FALSE, FALSE, 0);
 gtk_container_add (GTK_CONTAINER (SubItem), hbox);
 
 gtk_menu_append( GTK_MENU( Menu ),SubItem );
 gtk_menu_item_set_submenu( GTK_MENU_ITEM( SubItem ),Item );
 
 gtk_widget_show_all( SubItem );
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

typedef struct
{
 int id;
 const char * name;
} Languages_t;

#define lng( a,b ) ( (int)(a) * 256 + b )
static Languages_t Languages[] =
         {
           { lng( 'a','b' ), "Abkhazian"                  },
           { lng( 'a','a' ), "Afar"                       },
           { lng( 'a','f' ), "Afrikaans"                  },
           { lng( 's','q' ), "Albanian"                   },
           { lng( 'a','m' ), "Amharic"                    },
           { lng( 'a','r' ), "Arabic"                     },
           { lng( 'h','y' ), "Armenian"                   },
           { lng( 'a','s' ), "Assamese"                   },
           { lng( 'a','e' ), "Avestan"                    },
           { lng( 'a','y' ), "Aymara"                     },
           { lng( 'a','z' ), "Azerbaijani"                },
           { lng( 'b','a' ), "Bashkir"                    },
           { lng( 'e','u' ), "Basque"                     },
           { lng( 'b','e' ), "Belarusian"                 },
           { lng( 'b','n' ), "Bengali"                    },
           { lng( 'b','h' ), "Bihari"                     },
           { lng( 'b','i' ), "Bislama"                    },
           { lng( 'b','s' ), "Bosnian"                    },
           { lng( 'b','r' ), "Breton"                     },
           { lng( 'b','g' ), "Bulgarian"                  },
           { lng( 'm','y' ), "Burmese"                    },
           { lng( 'c','a' ), "Catalan"                    },
           { lng( 'c','h' ), "Chamorro"                   },
           { lng( 'c','e' ), "Chechen"                    },
           { lng( 'n','y' ), "Chichewa;Nyanja"            },
           { lng( 'z','h' ), "Chinese"                    },
           { lng( 'c','u' ), "ChurchSlavic"               },
           { lng( 'c','v' ), "Chuvash"                    },
           { lng( 'k','w' ), "Cornish"                    },
           { lng( 'c','o' ), "Corsican"                   },
           { lng( 'h','r' ), "Croatian"                   },
           { lng( 'c','s' ), "Czech"                      },
           { lng( 'd','a' ), "Danish"                     },
           { lng( 'n','l' ), "Dutch"                      },
           { lng( 'd','z' ), "Dzongkha"                   },
           { lng( 'e','n' ), "English"                    },
           { lng( 'e','o' ), "Esperanto"                  },
           { lng( 'e','t' ), "Estonian"                   },
           { lng( 'f','o' ), "Faroese"                    },
           { lng( 'f','j' ), "Fijian"                     },
           { lng( 'f','i' ), "Finnish"                    },
           { lng( 'f','r' ), "French"                     },
           { lng( 'f','y' ), "Frisian"                    },
           { lng( 'g','d' ), "Gaelic(Scots"               },
           { lng( 'g','l' ), "Gallegan"                   },
           { lng( 'k','a' ), "Georgian"                   },
           { lng( 'd','e' ), "German"                     },
           { lng( 'e','l' ), "Greek"                      },
           { lng( 'g','n' ), "Guarani"                    },
           { lng( 'g','u' ), "Gujarati"                   },
           { lng( 'h','a' ), "Hausa"                      },
           { lng( 'h','e' ), "Hebrew"                     },
           { lng( 'i','w' ), "Hebrew"                     },
           { lng( 'h','z' ), "Herero"                     },
           { lng( 'h','i' ), "Hindi"                      },
           { lng( 'h','o' ), "HiriMotu"                   },
           { lng( 'h','u' ), "Hungarian"                  },
           { lng( 'i','s' ), "Icelandic"                  },
           { lng( 'i','d' ), "Indonesian"                 },
           { lng( 'i','n' ), "Indonesian"                 },
           { lng( 'i','a' ), "Interlingua"                },
           { lng( 'i','e' ), "Interlingue"                },
           { lng( 'i','u' ), "Inuktitut"                  },
           { lng( 'i','k' ), "Inupiaq"                    },
           { lng( 'g','a' ), "Irish"                      },
           { lng( 'i','t' ), "Italian"                    },
           { lng( 'j','a' ), "Japanese"                   },
           { lng( 'j','v' ), "Javanese"                   },
           { lng( 'j','w' ), "Javanese"                   },
           { lng( 'k','l' ), "Kalaallisut"                },
           { lng( 'k','n' ), "Kannada"                    },
           { lng( 'k','s' ), "Kashmiri"                   },
           { lng( 'k','k' ), "Kazakh"                     },
           { lng( 'k','m' ), "Khmer"                      },
           { lng( 'k','i' ), "Kikuyu"                     },
           { lng( 'r','w' ), "Kinyarwanda"                },
           { lng( 'k','y' ), "Kirghiz"                    },
           { lng( 'k','v' ), "Komi"                       },
           { lng( 'k','o' ), "Korean"                     },
           { lng( 'k','j' ), "Kuanyama"                   },
           { lng( 'k','u' ), "Kurdish"                    },
           { lng( 'l','o' ), "Lao"                        },
           { lng( 'l','a' ), "Latin"                      },
           { lng( 'l','v' ), "Latvian"                    },
           { lng( 'l','b' ), "Letzeburgesch"              },
           { lng( 'l','n' ), "Lingala"                    },
           { lng( 'l','t' ), "Lithuanian"                 },
           { lng( 'm','k' ), "Macedonian"                 },
           { lng( 'm','g' ), "Malagasy"                   },
           { lng( 'm','s' ), "Malay"                      },
           { lng( 'm','l' ), "Malayalam"                  },
           { lng( 'm','t' ), "Maltese"                    },
           { lng( 'g','v' ), "Manx"                       },
           { lng( 'm','i' ), "Maori"                      },
           { lng( 'm','r' ), "Marathi"                    },
           { lng( 'm','h' ), "Marshall"                   },
           { lng( 'm','o' ), "Moldavian"                  },
           { lng( 'm','n' ), "Mongolian"                  },
           { lng( 'n','a' ), "Nauru"                      },
           { lng( 'n','v' ), "Navajo"                     },
           { lng( 'n','d' ), "North Ndebele"              },
           { lng( 'n','r' ), "South Ndebele"              },
           { lng( 'n','g' ), "Ndonga"                     },
           { lng( 'n','e' ), "Nepali"                     },
           { lng( 's','e' ), "NorthernSami"               },
           { lng( 'n','o' ), "Norwegian"                  },
           { lng( 'n','b' ), "NorwegianBokmål"            },
           { lng( 'n','n' ), "NorwegianNynorsk"           },
           { lng( 'n','y' ), "Nyanja;Chichewa"            },
           { lng( 'o','c' ), "Occitan(post1500;Provençal" },
           { lng( 'o','r' ), "Oriya"                      },
           { lng( 'o','m' ), "Oromo"                      },
           { lng( 'o','s' ), "Ossetian;Ossetic"           },
           { lng( 'p','i' ), "Pali"                       },
           { lng( 'p','a' ), "Panjabi"                    },
           { lng( 'f','a' ), "Persian"                    },
           { lng( 'p','l' ), "Polish"                     },
           { lng( 'p','t' ), "Portuguese"                 },
           { lng( 'o','c' ), "Provençal;Occitan(post1500" },
           { lng( 'p','s' ), "Pushto"                     },
           { lng( 'q','u' ), "Quechua"                    },
           { lng( 'r','m' ), "Raeto-Romance"              },
           { lng( 'r','o' ), "Romanian"                   },
           { lng( 'r','n' ), "Rundi"                      },
           { lng( 'r','u' ), "Russian"                    },
           { lng( 's','m' ), "Samoan"                     },
           { lng( 's','g' ), "Sango"                      },
           { lng( 's','a' ), "Sanskrit"                   },
           { lng( 's','c' ), "Sardinian"                  },
           { lng( 's','r' ), "Serbian"                    },
           { lng( 's','n' ), "Shona"                      },
           { lng( 's','d' ), "Sindhi"                     },
           { lng( 's','i' ), "Sinhalese"                  },
           { lng( 's','k' ), "Slovak"                     },
           { lng( 's','l' ), "Slovenian"                  },
           { lng( 's','o' ), "Somali"                     },
           { lng( 's','t' ), "Sotho"                      },
           { lng( 'e','s' ), "Spanish"                    },
           { lng( 's','u' ), "Sundanese"                  },
           { lng( 's','w' ), "Swahili"                    },
           { lng( 's','s' ), "Swati"                      },
           { lng( 's','v' ), "Swedish"                    },
           { lng( 't','l' ), "Tagalog"                    },
           { lng( 't','y' ), "Tahitian"                   },
           { lng( 't','g' ), "Tajik"                      },
           { lng( 't','a' ), "Tamil"                      },
           { lng( 't','t' ), "Tatar"                      },
           { lng( 't','e' ), "Telugu"                     },
           { lng( 't','h' ), "Thai"                       },
           { lng( 'b','o' ), "Tibetan"                    },
           { lng( 't','i' ), "Tigrinya"                   },
           { lng( 't','o' ), "Tonga"                      },
           { lng( 't','s' ), "Tsonga"                     },
           { lng( 't','n' ), "Tswana"                     },
           { lng( 't','r' ), "Turkish"                    },
           { lng( 't','k' ), "Turkmen"                    },
           { lng( 't','w' ), "Twi"                        },
           { lng( 'u','g' ), "Uighur"                     },
           { lng( 'u','k' ), "Ukrainian"                  },
           { lng( 'u','r' ), "Urdu"                       },
           { lng( 'u','z' ), "Uzbek"                      },
           { lng( 'v','i' ), "Vietnamese"                 },
           { lng( 'v','o' ), "Volapük"                    },
           { lng( 'c','y' ), "Welsh"                      },
           { lng( 'w','o' ), "Wolof"                      },
           { lng( 'x','h' ), "Xhosa"                      },
           { lng( 'y','i' ), "Yiddish"                    },
           { lng( 'j','i' ), "Yiddish"                    },
           { lng( 'y','o' ), "Yoruba"                     },
           { lng( 'z','a' ), "Zhuang"                     },
           { lng( 'z','u' ), "Zulu"                       },
         };
#undef lng

#ifdef CONFIG_DVDREAD
static char * ChannelTypes[] =
	{ "Dolby Digital","","Mpeg1","Mpeg2","PCM","","Digital Theatre System" };
static char * ChannelNumbers[] =
	{ "","Stereo","","","","5.1" };
#endif

const char * GetLanguage( int language )
{
 unsigned int i;
 for ( i=0;i<sizeof( Languages ) / sizeof( Languages_t );i++ )
  if ( Languages[i].id == language ) return Languages[i].name;
 return NULL;
}


GtkWidget * DVDSubMenu;
GtkWidget * DVDTitleMenu;
GtkWidget * DVDChapterMenu;
GtkWidget * DVDAudioLanguageMenu;
GtkWidget * DVDSubtitleLanguageMenu;
GtkWidget * AspectMenu;
GtkWidget * VCDSubMenu;
GtkWidget * VCDTitleMenu;

GtkWidget * create_PopUpMenu( void )
{
 GtkWidget * window1;
 GtkWidget * Menu = NULL;
 GtkWidget * SubMenu = NULL;
 GtkWidget * MenuItem = NULL;
 GtkWidget * H, * N, * D, * F;
 mixer_t *mixer = mpctx_get_mixer(guiIntfStruct.mpcontext);
 int global_sub_size = mpctx_get_global_sub_size(guiIntfStruct.mpcontext);

 Menu=gtk_menu_new();
 gtk_widget_realize (Menu);
 window1 = gtk_widget_get_toplevel(Menu);


  AddMenuItem( window1, (const char*)ab_xpm, Menu,MSGTR_MENU_AboutMPlayer"     ", evAbout );
  AddSeparator( Menu );
   SubMenu=AddSubMenu( window1, (const char*)open_xpm, Menu,MSGTR_MENU_Open );
    AddMenuItem( window1, (const char*)file2_xpm, SubMenu,MSGTR_MENU_PlayFile"    ", evLoadPlay );
#ifdef CONFIG_VCD
    AddMenuItem( window1, (const char*)playvcd_xpm, SubMenu,MSGTR_MENU_PlayVCD, evPlayVCD );
#endif
#ifdef CONFIG_DVDREAD
    AddMenuItem( window1, (const char*)playdvd_xpm, SubMenu,MSGTR_MENU_PlayDVD, evPlayDVD );
#endif
    AddMenuItem( window1, (const char*)url_xpm, SubMenu,MSGTR_MENU_PlayURL, evSetURL );
    AddMenuItem( window1, (const char*)sub_xpm, SubMenu,MSGTR_MENU_LoadSubtitle"   ", evLoadSubtitle );
    AddMenuItem( window1, (const char*)delsub_xpm, SubMenu,MSGTR_MENU_DropSubtitle,evDropSubtitle );
    AddMenuItem( window1, (const char*)loadeaf_xpm, SubMenu,MSGTR_MENU_LoadExternAudioFile, evLoadAudioFile );
   SubMenu=AddSubMenu(window1, (const char*)play_xpm, Menu,MSGTR_MENU_Playing );
    AddMenuItem( window1, (const char*)play_xpm, SubMenu,MSGTR_MENU_Play"        ", evPlay );
    AddMenuItem( window1, (const char*)pause_xpm, SubMenu,MSGTR_MENU_Pause, evPause );
    AddMenuItem( window1, (const char*)stop2_xpm, SubMenu,MSGTR_MENU_Stop, evStop );
    AddMenuItem( window1, (const char*)next_xpm, SubMenu,MSGTR_MENU_NextStream, evNext );
    AddMenuItem( window1, (const char*)prev_xpm, SubMenu,MSGTR_MENU_PrevStream, evPrev );
//    AddSeparator( SubMenu );
//    AddMenuItem( SubMenu,"Back 10 sec", evBackward10sec );
//    AddMenuItem( SubMenu,"Fwd 10 sec", evForward10sec );
//    AddMenuItem( SubMenu,"Back 1 min", evBackward1min );
//    AddMenuItem( SubMenu,"Fwd 1 min", evForward1min );
//   SubMenu=AddSubMenu( Menu,MSGTR_MENU_Size );
//    AddMenuItem( SubMenu,MSGTR_MENU_NormalSize"      ", evNormalSize );
//    AddMenuItem( SubMenu,MSGTR_MENU_DoubleSize, evDoubleSize );
//    AddMenuItem( SubMenu,MSGTR_MENU_FullScreen, evFullScreen );
#ifdef CONFIG_VCD
   VCDSubMenu=AddSubMenu( window1, (const char*)vcd_xpm, Menu,MSGTR_MENU_VCD );
    AddMenuItem( window1, (const char*)playvcd_xpm, VCDSubMenu,MSGTR_MENU_PlayDisc,evPlayVCD );
    AddSeparator( VCDSubMenu );
    VCDTitleMenu=AddSubMenu( window1, (const char*)title_xpm, VCDSubMenu,MSGTR_MENU_Titles );
    if ( guiIntfStruct.VCDTracks ) 
     {
      char tmp[32]; int i;
      for ( i=0;i < guiIntfStruct.VCDTracks;i++ )
       {
        snprintf( tmp,32,MSGTR_MENU_Title,i+1 );
    //AddMenuItem( VCDTitleMenu,tmp,( (i+1) << 16 ) + evSetVCDTrack );
        AddMenuItem(window1, (const char*)empty_xpm, VCDTitleMenu,tmp,( (i+1) << 16 ) + evSetVCDTrack );
       }
     }
     else
      {
       MenuItem=AddMenuItem( window1, (const char*)empty_xpm, VCDTitleMenu,MSGTR_MENU_None,evNone );
       gtk_widget_set_sensitive( MenuItem,FALSE );
      }
#endif
#ifdef CONFIG_DVDREAD
   DVDSubMenu=AddSubMenu( window1, (const char*)dvd_xpm, Menu,MSGTR_MENU_DVD );
    AddMenuItem( window1, (const char*)playdvd_xpm, DVDSubMenu,MSGTR_MENU_PlayDisc"    ", evPlayDVD );
//    AddMenuItem( DVDSubMenu,MSGTR_MENU_ShowDVDMenu, evNone );
    AddSeparator( DVDSubMenu );
    DVDTitleMenu=AddSubMenu( window1, (const char*)title_xpm, DVDSubMenu,MSGTR_MENU_Titles );
     if ( guiIntfStruct.DVD.titles )
      {
       char tmp[32]; int i;
       for ( i=1 ; i<= guiIntfStruct.DVD.titles;i++ )
        {
         snprintf( tmp,32,MSGTR_MENU_Title,i);
         AddMenuCheckItem( window1, (const char*)empty1px_xpm, DVDTitleMenu,tmp,
			   guiIntfStruct.DVD.current_title == i,
			   (i << 16) + evSetDVDTitle );
        }
      }
      else
       {
        MenuItem=AddMenuItem( window1, (const char*)empty_xpm, DVDTitleMenu,MSGTR_MENU_None,evNone );
        gtk_widget_set_sensitive( MenuItem,FALSE );
       }
    DVDChapterMenu=AddSubMenu( window1, (const char*)chapter_xpm, DVDSubMenu,MSGTR_MENU_Chapters );
     if ( guiIntfStruct.DVD.chapters )
      {
       char tmp[32]; int i;
       for ( i=1;i <= guiIntfStruct.DVD.chapters;i++ )
        {
         snprintf( tmp,32,MSGTR_MENU_Chapter,i );
         AddMenuCheckItem( window1, (const char*)empty1px_xpm, DVDChapterMenu,tmp,guiIntfStruct.DVD.current_chapter == i,
			   ( i << 16 ) + evSetDVDChapter );
        }
      }
      else
       {
        MenuItem=AddMenuItem( window1, (const char*)empty_xpm, DVDChapterMenu,MSGTR_MENU_None,evNone );
        gtk_widget_set_sensitive( MenuItem,FALSE );
       }
    DVDAudioLanguageMenu=AddSubMenu( window1, (const char*)tongue_xpm, DVDSubMenu,MSGTR_MENU_AudioLanguages );
     if ( guiIntfStruct.DVD.nr_of_audio_channels )
      {
       char tmp[64]; int i, id = guiIntfStruct.demuxer ? ((demuxer_t *)guiIntfStruct.demuxer)->audio->id : audio_id;
       for ( i=0;i < guiIntfStruct.DVD.nr_of_audio_channels;i++ )
        {
	 snprintf( tmp,64,"%s - %s %s",GetLanguage( guiIntfStruct.DVD.audio_streams[i].language ),
	   ChannelTypes[ guiIntfStruct.DVD.audio_streams[i].type ],
	   ChannelNumbers[ guiIntfStruct.DVD.audio_streams[i].channels ] );
//	 if ( id == -1 ) id=audio_id; //guiIntfStruct.DVD.audio_streams[i].id;
         AddMenuCheckItem( window1, (const char*)dolby_xpm, DVDAudioLanguageMenu,tmp,
			   id == guiIntfStruct.DVD.audio_streams[i].id,
			   ( guiIntfStruct.DVD.audio_streams[i].id << 16 ) + evSetDVDAudio );
        }
      }
      else
       {
        MenuItem=AddMenuItem( window1, (const char*)empty_xpm, DVDAudioLanguageMenu,MSGTR_MENU_None,evNone );
        gtk_widget_set_sensitive( MenuItem,FALSE );
       }
    DVDSubtitleLanguageMenu=AddSubMenu( window1, (const char*)tonguebla_xpm, DVDSubMenu,MSGTR_MENU_SubtitleLanguages );
     if ( guiIntfStruct.DVD.nr_of_subtitles )
      {
       char tmp[64]; int i;
       AddMenuItem( window1, (const char*)empty1px_xpm, DVDSubtitleLanguageMenu,MSGTR_MENU_None,( (unsigned short)-1 << 16 ) + evSetDVDSubtitle );
       for ( i=0;i < guiIntfStruct.DVD.nr_of_subtitles;i++ )
        {
	 snprintf( tmp,64,"%s",GetLanguage( guiIntfStruct.DVD.subtitles[i].language ) );
         AddMenuCheckItem( window1, (const char*)empty1px_xpm, DVDSubtitleLanguageMenu,tmp,
			   dvdsub_id == guiIntfStruct.DVD.subtitles[i].id,
			   ( guiIntfStruct.DVD.subtitles[i].id << 16 ) + evSetDVDSubtitle );
        }
      }
      else
       {
        MenuItem=AddMenuItem( window1, (const char*)empty_xpm, DVDSubtitleLanguageMenu,MSGTR_MENU_None,evNone );
        gtk_widget_set_sensitive( MenuItem,FALSE );
       }
#endif

//  if ( guiIntfStruct.Playing )
   {
    AspectMenu=AddSubMenu( window1, (const char*)aspect_xpm, Menu,MSGTR_MENU_AspectRatio );
    AddMenuItem( window1, (const char*)a11_xpm, AspectMenu,MSGTR_MENU_Original,( 1 << 16 ) + evSetAspect );
    AddMenuItem( window1, (const char*)a169_xpm, AspectMenu,"16:9",( 2 << 16 ) + evSetAspect );
    AddMenuItem( window1, (const char*)a43_xpm, AspectMenu,"4:3",( 3 << 16 ) + evSetAspect );
    AddMenuItem( window1, (const char*)a235_xpm, AspectMenu,"2.35",( 4 << 16 ) + evSetAspect );
   }

  if ( guiIntfStruct.Playing && guiIntfStruct.demuxer && guiIntfStruct.StreamType != STREAMTYPE_DVD )
   {
    int i,c = 0;

    for ( i=0;i < MAX_A_STREAMS;i++ )
     if ( ((demuxer_t *)guiIntfStruct.demuxer)->a_streams[i] ) c++;
    
    if ( c > 1 )
     {
      SubMenu=AddSubMenu( window1, (const char*)empty_xpm, Menu,MSGTR_MENU_AudioTrack );
      for ( i=0;i < MAX_A_STREAMS;i++ )
       if ( ((demuxer_t *)guiIntfStruct.demuxer)->a_streams[i] )
        {
         int aid = ((sh_audio_t *)((demuxer_t *)guiIntfStruct.demuxer)->a_streams[i])->aid;
         char tmp[32];
         snprintf( tmp,32,MSGTR_MENU_Track,aid );
         AddMenuItem( window1, (const char*)empty_xpm, SubMenu,tmp,( aid << 16 ) + evSetAudio );
        }
     }

    for ( c=0,i=0;i < MAX_V_STREAMS;i++ )
     if ( ((demuxer_t *)guiIntfStruct.demuxer)->v_streams[i] ) c++;
    
    if ( c > 1 )
     {
      SubMenu=AddSubMenu( window1, (const char*)empty_xpm, Menu,MSGTR_MENU_VideoTrack );
      for ( i=0;i < MAX_V_STREAMS;i++ )
       if ( ((demuxer_t *)guiIntfStruct.demuxer)->v_streams[i] )
        {
         int vid = ((sh_video_t *)((demuxer_t *)guiIntfStruct.demuxer)->v_streams[i])->vid;
         char tmp[32];
         snprintf( tmp,32,MSGTR_MENU_Track,vid );
         AddMenuItem( window1, (const char*)empty_xpm, SubMenu,tmp,( vid << 16 ) + evSetVideo );
        }
     }
   }
  
  /* cheap subtitle switching for non-DVD streams */
  if ( global_sub_size && guiIntfStruct.StreamType != STREAMTYPE_DVD )
   {
    int i;
    SubMenu=AddSubMenu( window1, (const char*)empty_xpm, Menu, MSGTR_MENU_Subtitles );
    AddMenuItem( window1, (const char*)empty_xpm, SubMenu, MSGTR_MENU_None, (-1 << 16) + evSetSubtitle );
    for ( i=0;i < global_sub_size;i++ )
     {
      char tmp[32];
      snprintf( tmp, 32, MSGTR_MENU_Track, i );
      AddMenuItem( window1,(const char*)empty_xpm,SubMenu,tmp,( i << 16 ) + evSetSubtitle );
     }
   }

  AddSeparator( Menu );
  MenuItem=AddMenuCheckItem( window1, (const char*)sound_xpm, Menu,MSGTR_MENU_Mute,mixer->muted,evMute );
  if ( !guiIntfStruct.AudioType ) gtk_widget_set_sensitive( MenuItem,FALSE );
  AddMenuItem( window1, (const char*)pl_xpm, Menu,MSGTR_MENU_PlayList, evPlayList );
  AddMenuItem( window1, (const char*)skin_xpm, Menu,MSGTR_MENU_SkinBrowser, evSkinBrowser );
  AddMenuItem( window1, (const char*)prefs_xpm, Menu,MSGTR_MENU_Preferences, evPreferences );
  AddMenuItem( window1, (const char*)eq_xpm, Menu,MSGTR_Equalizer, evEqualizer );

  if ( guiIntfStruct.NoWindow == False )
   {
    int b1 = 0, b2 = 0, b_half = 0;
    AddSeparator( Menu );
    if ( !appMPlayer.subWindow.isFullScreen && guiIntfStruct.Playing )
     {
      if ( ( appMPlayer.subWindow.Width == guiIntfStruct.MovieWidth * 2 )&& 
           ( appMPlayer.subWindow.Height == guiIntfStruct.MovieHeight * 2 ) ) b2=1;
      else if ( ( appMPlayer.subWindow.Width == guiIntfStruct.MovieWidth / 2 ) && 
                ( appMPlayer.subWindow.Height == guiIntfStruct.MovieHeight / 2 ) ) b_half=1;
      else b1=1;
     } else b1=!appMPlayer.subWindow.isFullScreen;
    H=AddMenuCheckItem( window1, (const char*)half_xpm, Menu,MSGTR_MENU_HalfSize,b_half,evHalfSize );
    N=AddMenuCheckItem( window1, (const char*)normal_xpm, Menu,MSGTR_MENU_NormalSize"      ",b1,evNormalSize );
    D=AddMenuCheckItem( window1, (const char*)double_xpm, Menu,MSGTR_MENU_DoubleSize,b2,evDoubleSize );
    F=AddMenuCheckItem( window1, (const char*)fs_xpm, Menu,MSGTR_MENU_FullScreen,appMPlayer.subWindow.isFullScreen,evFullScreen );
  if ( !gtkShowVideoWindow && !guiIntfStruct.Playing )
   {
    gtk_widget_set_sensitive( H,FALSE );
    gtk_widget_set_sensitive( N,FALSE );
    gtk_widget_set_sensitive( D,FALSE );
    gtk_widget_set_sensitive( F,FALSE );
   }
   }

  AddSeparator( Menu );
  AddMenuItem( window1, (const char*)exit_xpm, Menu,MSGTR_MENU_Exit, evExit );

 return Menu;
}
