
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../../../config.h"
#include "../../../help_mp.h"

#include "../../events.h"

#include "menu.h"
#include "../widgets.h"

void ActivateMenuItem( int Item )
{
// fprintf( stderr,"[menu] item: %d.%d\n",Item&0xffff,Item>>16 );
 gtkPopupMenu=Item & 0x0000ffff;
 gtkPopupMenuParam=Item >> 16;
 mplEventHandling( Item & 0x0000ffff,Item >> 16 );
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

typedef struct
{
 int id;
 char * name;
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

static char * ChannelTypes[] =
	{ "Dolby Digital","","Mpeg1","Mpeg2","PCM","","Digital Theatre System" };
static char * ChannelNumbers[] =
	{ "","Stereo","","","","5.1" };

char * GetLanguage( int language )
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

GtkWidget * VCDSubMenu;
GtkWidget * VCDTitleMenu;

GtkWidget * create_PopUpMenu( void )
{
 GtkWidget * Menu = NULL;
 GtkWidget * SubMenu = NULL;
 GtkWidget * MenuItem = NULL;

 Menu=gtk_menu_new();

  AddMenuItem( Menu,MSGTR_MENU_AboutMPlayer"      ", evAbout );
  AddSeparator( Menu );
   SubMenu=AddSubMenu( Menu,MSGTR_MENU_Open );
    AddMenuItem( SubMenu,MSGTR_MENU_PlayFile"    ", evLoadPlay );
#ifdef HAVE_VCD
    AddMenuItem( SubMenu,MSGTR_MENU_PlayVCD, evPlayVCD );
#endif
#ifdef USE_DVDREAD
    AddMenuItem( SubMenu,MSGTR_MENU_PlayDVD, evPlayDVD );
#endif
    AddMenuItem( SubMenu,MSGTR_MENU_PlayURL, evSetURL );
    AddMenuItem( SubMenu,MSGTR_MENU_LoadSubtitle"   ", evLoadSubtitle );
    AddMenuItem( SubMenu,MSGTR_MENU_LoadExternAudioFile, evLoadAudioFile );
   SubMenu=AddSubMenu( Menu,MSGTR_MENU_Playing );
    AddMenuItem( SubMenu,MSGTR_MENU_Play"        ", evPlay );
    AddMenuItem( SubMenu,MSGTR_MENU_Pause, evPause );
    AddMenuItem( SubMenu,MSGTR_MENU_Stop, evStop );
    AddMenuItem( SubMenu,MSGTR_MENU_NextStream, evPrev );
    AddMenuItem( SubMenu,MSGTR_MENU_PrevStream, evNext );
//    AddSeparator( SubMenu );
//    AddMenuItem( SubMenu,"Back 10 sec", evBackward10sec );
//    AddMenuItem( SubMenu,"Fwd 10 sec", evForward10sec );
//    AddMenuItem( SubMenu,"Back 1 min", evBackward1min );
//    AddMenuItem( SubMenu,"Fwd 1 min", evForward1min );
   SubMenu=AddSubMenu( Menu,MSGTR_MENU_Size );
    AddMenuItem( SubMenu,MSGTR_MENU_NormalSize"      ", evNormalSize );
    AddMenuItem( SubMenu,MSGTR_MENU_DoubleSize, evDoubleSize );
    AddMenuItem( SubMenu,MSGTR_MENU_FullScreen, evFullScreen );
#ifdef HAVE_VCD
   VCDSubMenu=AddSubMenu( Menu,MSGTR_MENU_VCD );
    AddMenuItem( VCDSubMenu,MSGTR_MENU_PlayDisc,evPlayVCD );
    AddSeparator( VCDSubMenu );
    VCDTitleMenu=AddSubMenu( VCDSubMenu,MSGTR_MENU_Titles );
    if ( guiIntfStruct.VCDTracks ) 
     {
      char tmp[32]; int i;
      for ( i=0;i < guiIntfStruct.VCDTracks;i++ )
       {
        sprintf( tmp,MSGTR_MENU_Title,i+1 );
	AddMenuItem( VCDTitleMenu,tmp,( (i+1) << 16 ) + evSetVCDTrack );
       }
     }
     else
      {
       MenuItem=AddMenuItem( VCDTitleMenu,MSGTR_MENU_None,evNone );
       gtk_widget_set_sensitive( MenuItem,FALSE );
      }
#endif
#ifdef USE_DVDREAD
   DVDSubMenu=AddSubMenu( Menu,MSGTR_MENU_DVD );
    AddMenuItem( DVDSubMenu,MSGTR_MENU_PlayDisc"    ", evPlayDVD );
    AddMenuItem( DVDSubMenu,MSGTR_MENU_ShowDVDMenu, evNone );
    AddSeparator( DVDSubMenu );
    DVDTitleMenu=AddSubMenu( DVDSubMenu,MSGTR_MENU_Titles );
     if ( guiIntfStruct.DVD.titles )
      {
       char tmp[32]; int i;
       for ( i=0;i < guiIntfStruct.DVD.titles;i++ )
        {
         sprintf( tmp,MSGTR_MENU_Title,i+1 );
         AddMenuItem( DVDTitleMenu,tmp,( (i+1) << 16 ) + evSetDVDTitle );
        }
      }
      else
       {
        MenuItem=AddMenuItem( DVDTitleMenu,MSGTR_MENU_None,evNone );
        gtk_widget_set_sensitive( MenuItem,FALSE );
       }
    DVDChapterMenu=AddSubMenu( DVDSubMenu,MSGTR_MENU_Chapters );
     if ( guiIntfStruct.DVD.chapters )
      {
       char tmp[32]; int i;
       for ( i=0;i < guiIntfStruct.DVD.chapters;i++ )
        {
         sprintf( tmp,MSGTR_MENU_Chapter,i+1 );
         AddMenuItem( DVDChapterMenu,tmp,( (i+1) << 16 ) + evSetDVDChapter );
        }
      }
      else
       {
        MenuItem=AddMenuItem( DVDChapterMenu,MSGTR_MENU_None,evNone );
        gtk_widget_set_sensitive( MenuItem,FALSE );
       }
    DVDAudioLanguageMenu=AddSubMenu( DVDSubMenu,MSGTR_MENU_AudioLanguages );
     if ( guiIntfStruct.DVD.nr_of_audio_channels )
      {
       char tmp[64]; int i;
       for ( i=0;i < guiIntfStruct.DVD.nr_of_audio_channels;i++ )
        {
	 snprintf( tmp,64,"%s - %s %s",GetLanguage( guiIntfStruct.DVD.audio_streams[i].language ),
	   ChannelTypes[ guiIntfStruct.DVD.audio_streams[i].type ],
	   ChannelNumbers[ guiIntfStruct.DVD.audio_streams[i].channels ] );
         AddMenuItem( DVDAudioLanguageMenu,tmp,( guiIntfStruct.DVD.audio_streams[i].id << 16 ) + evSetDVDAudio );
        }
      }
      else
       {
        MenuItem=AddMenuItem( DVDAudioLanguageMenu,MSGTR_MENU_None,evNone );
        gtk_widget_set_sensitive( MenuItem,FALSE );
       }
    DVDSubtitleLanguageMenu=AddSubMenu( DVDSubMenu,MSGTR_MENU_SubtitleLanguages );
     if ( guiIntfStruct.DVD.nr_of_subtitles )
      {
       char tmp[64]; int i;
       AddMenuItem( DVDSubtitleLanguageMenu,"None",( (unsigned short)-1 << 16 ) + evSetDVDSubtitle );
       for ( i=0;i < guiIntfStruct.DVD.nr_of_subtitles;i++ )
        {
         strcpy( tmp,GetLanguage( guiIntfStruct.DVD.subtitles[i].language ) );
         AddMenuItem( DVDSubtitleLanguageMenu,tmp,( guiIntfStruct.DVD.subtitles[i].id << 16 ) + evSetDVDSubtitle );
        }
      }
      else
       {
        MenuItem=AddMenuItem( DVDSubtitleLanguageMenu,MSGTR_MENU_None,evNone );
        gtk_widget_set_sensitive( MenuItem,FALSE );
       }
#endif
  AddSeparator( Menu );
  AddMenuItem( Menu,"Mute", evMute );
  AddMenuItem( Menu,MSGTR_MENU_PlayList, evPlayList );
  AddMenuItem( Menu,MSGTR_MENU_SkinBrowser, evSkinBrowser );
  AddMenuItem( Menu,MSGTR_MENU_Preferences, evPreferences );
  AddMenuItem( Menu,MSGTR_Equalizer, evEqualizer );
  AddSeparator( Menu );
  AddMenuItem( Menu,MSGTR_MENU_Exit, evExit );

 return Menu;
}
