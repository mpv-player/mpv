#include <Carbon/Carbon.h>
#include <ApplicationServices/ApplicationServices.h>
#include "stream/url.h"
#include "mp_msg.h"
#include "m_option.h"
#include "m_config.h"
#include "playtree.h"

static play_tree_t *files=NULL;

static inline void add_entry(play_tree_t **last_parentp, play_tree_t **last_entryp, play_tree_t *entry) {

	if(*last_entryp==NULL)
		play_tree_set_child(*last_parentp, entry);		      
	else 
		play_tree_append_entry(*last_entryp, entry);

	*last_entryp=entry;
}

static pascal OSErr AppleEventHandlerProc(const AppleEvent *theAppleEvent, AppleEvent* reply, SInt32 handlerRefcon) {
OSErr err=errAEEventNotHandled, res=noErr;
AEDescList docList;
long itemsInList;

	AERemoveEventHandler(kCoreEventClass, kAEOpenDocuments, NULL, FALSE);
	if((res=AEGetParamDesc(theAppleEvent, keyDirectObject, typeAEList, &docList))==noErr) {
		if((res=AECountItems(&docList, &itemsInList))==noErr) {
		Size currentSize=0;
		int valid=0,i;
		char *parm=NULL;
		play_tree_t *last_entry=NULL;

			files=play_tree_new();
			for(i=1;i<=itemsInList;++i) {

				for(;;) {
				OSErr e;
				Size actualSize=0;
				AEKeyword keywd;
				DescType returnedType;

					if((e=AEGetNthPtr(&docList, i, typeFileURL, &keywd, &returnedType, (Ptr)parm, currentSize, &actualSize))==noErr) {
						if(actualSize>=currentSize) {
							currentSize=actualSize+1;
							parm=realloc(parm, currentSize);
						}
						else {
							parm[actualSize]=0;
							valid=1;
							break;
						}
					}
					else {
						valid=0;
						break;
					}
				}

				if(valid) {
				URL_t *url=url_new(parm);

					if(url && !strcmp(url->protocol,"file") && !strcmp(url->hostname,"localhost")) {
					play_tree_t *entry=play_tree_new();

						url_unescape_string(url->file, url->file);
						play_tree_add_file(entry, url->file);
						add_entry(&files, &last_entry, entry);
					}

					url_free(url);
				}
			}

			if(parm)
				free(parm);

			err=noErr;
		}
		else
			mp_msg(MSGT_CFGPARSER, MSGL_ERR, "AECountItems() error %d\n", res);
		
		AEDisposeDesc(&docList);
	}
	else
		mp_msg(MSGT_CFGPARSER, MSGL_ERR, "AEGetParamDesc() error %d\n", res);

	QuitApplicationEventLoop();
	return err;
}

play_tree_t *macosx_finder_args(m_config_t *config, int argc, char **argv) {
ProcessSerialNumber myPsn;
char myPsnStr[5+10+1+10+1];

	GetCurrentProcess(&myPsn);
	snprintf(myPsnStr, 5+10+1+10+1, "-psn_%u_%u", myPsn.highLongOfPSN, myPsn.lowLongOfPSN);
	myPsnStr[5+10+1+10]=0;

	if((argc==2) && !strcmp(myPsnStr, argv[1])) {
		m_config_set_option(config, "quiet", NULL);
		InitCursor();
		AEInstallEventHandler(kCoreEventClass, kAEOpenDocuments, NewAEEventHandlerUPP(AppleEventHandlerProc), 0, FALSE);
		RunApplicationEventLoop();
	}

	return files;
}
