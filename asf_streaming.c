#include "asf.h"

#include <string.h>

static ASF_StreamType_e stream_type;

void asf_streaming(char *data, int length) {
	ASF_stream_chunck_t *stream_chunck=(ASF_stream_chunck_t*)data;
	printf("ASF stream chunck size=%d\n", stream_chunck->length);

	switch(stream_chunck->type) {
		case 0x4324:	// Clear ASF configuration
			printf("    --> Clearing ASF stream configuration!\n");
			break;
		case 0x4424:    // Data follows
			printf("    --> Data follows\n");
			break;
		case 0x4524:    // Transfer complete
			printf("    --> Transfer complete\n");
			break;
		case 0x4824:    // ASF header chunk follows
			printf("    --> ASF header chunk follows\n");
			break;
		default:
			printf("======> Unknown stream type %d\n", stream_chunck->type );
	}
}

void asf_steam_type(char *content_type, char *features) {
	stream_type = ASF_Unknown_e;
	if( !strcasecmp(content_type, "application/octet-stream") ) {
		if( strstr(features, "broadcast")) {
			printf("-----> Live stream <-------\n");
			stream_type = ASF_Live_e;
		} else {
			printf("-----> Prerecorded <-------\n");
			stream_type = ASF_Prerecorded_e;
		}
	} else {
		if(     (!strcasecmp(content_type, "audio/x-ms-wax")) ||
			(!strcasecmp(content_type, "audio/x-ms-wma")) ||
			(!strcasecmp(content_type, "video/x-ms-asf")) ||
			(!strcasecmp(content_type, "video/x-ms-afs")) ||
			(!strcasecmp(content_type, "video/x-ms-wvx")) ||
			(!strcasecmp(content_type, "video/x-ms-wmv")) ||
			(!strcasecmp(content_type, "video/x-ms-wma")) ) {
			printf("-----> Redirector <-------\n");
			stream_type = ASF_Redirector_e;
		} else {
			printf("-----> unknown content-type: %s\n", content_type );
			stream_type = ASF_Unknown_e;
		}
	}
}
