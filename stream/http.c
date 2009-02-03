/* 
 * HTTP Helper
 * by Bertrand Baudet <bertrand_baudet@yahoo.com>
 * (C) 2001, MPlayer team.
 */

#include "config.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#if !HAVE_WINSOCK2_H
#else
#include <winsock2.h>
#include <ws2tcpip.h>
#endif

#include "http.h"
#include "url.h"
#include "mp_msg.h"

#include "stream.h"
#include "libmpdemux/demuxer.h"
#include "network.h"
#include "help_mp.h"


extern const mime_struct_t mime_type_table[];
extern int stream_cache_size;
extern int network_bandwidth;

int http_seek(stream_t *stream, off_t pos);

typedef struct {
  unsigned metaint;
  unsigned metapos;
  int is_ultravox;
} scast_data_t;

/**
 * \brief first read any data from sc->buffer then from fd
 * \param fd file descriptor to read data from
 * \param buffer buffer to read into
 * \param len how many bytes to read
 * \param sc streaming control containing buffer to read from first
 * \return len unless there is a read error or eof
 */
static unsigned my_read(int fd, char *buffer, int len, streaming_ctrl_t *sc) {
  unsigned pos = 0;
  unsigned cp_len = sc->buffer_size - sc->buffer_pos;
  if (cp_len > len)
    cp_len = len;
  memcpy(buffer, &sc->buffer[sc->buffer_pos], cp_len);
  sc->buffer_pos += cp_len;
  pos += cp_len;
  while (pos < len) {
    int ret = recv(fd, &buffer[pos], len - pos, 0);
    if (ret <= 0)
      break;
    pos += ret;
  }
  return pos;
}

/**
 * \brief read and process (i.e. discard *g*) a block of ultravox metadata
 * \param fd file descriptor to read from
 * \param sc streaming_ctrl_t whose buffer is consumed before reading from fd
 * \return number of real data before next metadata block starts or 0 on error
 */
static unsigned uvox_meta_read(int fd, streaming_ctrl_t *sc) {
  unsigned metaint;
  unsigned char info[6] = {0, 0, 0, 0, 0, 0};
  int info_read;
  do {
    info_read = my_read(fd, info, 1, sc);
    if (info[0] == 0x00)
      info_read = my_read(fd, info, 6, sc);
    else
      info_read += my_read(fd, &info[1], 5, sc);
    if (info_read != 6) // read error or eof
      return 0;
    // sync byte and reserved flags
    if (info[0] != 0x5a || (info[1] & 0xfc) != 0x00) {
      mp_msg(MSGT_DEMUXER, MSGL_ERR, "Invalid or unknown uvox metadata\n");
      return 0;
    }
    if (info[1] & 0x01)
      mp_msg(MSGT_DEMUXER, MSGL_WARN, "Encrypted ultravox data\n");
    metaint = info[4] << 8 | info[5];
    if ((info[3] & 0xf) < 0x07) { // discard any metadata nonsense
      char *metabuf = malloc(metaint);
      my_read(fd, metabuf, metaint, sc);
      free(metabuf);
    }
  } while ((info[3] & 0xf) < 0x07);
  return metaint;
}

/**
 * \brief read one scast meta data entry and print it
 * \param fd file descriptor to read from
 * \param sc streaming_ctrl_t whose buffer is consumed before reading from fd
 */
static void scast_meta_read(int fd, streaming_ctrl_t *sc) {
  unsigned char tmp = 0;
  unsigned metalen;
  my_read(fd, &tmp, 1, sc);
  metalen = tmp * 16;
  if (metalen > 0) {
    char *info = malloc(metalen + 1);
    unsigned nlen = my_read(fd, info, metalen, sc);
    info[nlen] = 0;
    mp_msg(MSGT_DEMUXER, MSGL_INFO, "\nICY Info: %s\n", info);
    free(info);
  }
}

/**
 * \brief read data from scast/ultravox stream without any metadata
 * \param fd file descriptor to read from
 * \param buffer buffer to read data into
 * \param size number of bytes to read
 * \param sc streaming_ctrl_t whose buffer is consumed before reading from fd
 */
static int scast_streaming_read(int fd, char *buffer, int size,
                                streaming_ctrl_t *sc) {
  scast_data_t *sd = (scast_data_t *)sc->data;
  unsigned block, ret;
  unsigned done = 0;

  // first read remaining data up to next metadata
  block = sd->metaint - sd->metapos;
  if (block > size)
    block = size;
  ret = my_read(fd, buffer, block, sc);
  sd->metapos += ret;
  done += ret;
  if (ret != block) // read problems or eof
    size = done;

  while (done < size) { // now comes the metadata
    if (sd->is_ultravox)
    {
      sd->metaint = uvox_meta_read(fd, sc);
      if (!sd->metaint)
        size = done;
    }
    else
      scast_meta_read(fd, sc); // read and display metadata
    sd->metapos = 0;
    block = size - done;
    if (block > sd->metaint)
      block = sd->metaint;
    ret = my_read(fd, &buffer[done], block, sc);
    sd->metapos += ret;
    done += ret;
    if (ret != block) // read problems or eof
      size = done;
  }
  return done;
}

static int scast_streaming_start(stream_t *stream) {
  int metaint;
  scast_data_t *scast_data;
  HTTP_header_t *http_hdr = stream->streaming_ctrl->data;
  int is_ultravox = strcasecmp(stream->streaming_ctrl->url->protocol, "unsv") == 0;
  if (!stream || stream->fd < 0 || !http_hdr)
    return -1;
  if (is_ultravox)
    metaint = 0;
  else {
    metaint = atoi(http_get_field(http_hdr, "Icy-MetaInt"));
    if (metaint <= 0)
      return -1;
  }
  stream->streaming_ctrl->buffer = malloc(http_hdr->body_size);
  stream->streaming_ctrl->buffer_size = http_hdr->body_size;
  stream->streaming_ctrl->buffer_pos = 0;
  memcpy(stream->streaming_ctrl->buffer, http_hdr->body, http_hdr->body_size);
  scast_data = malloc(sizeof(scast_data_t));
  scast_data->metaint = metaint;
  scast_data->metapos = 0;
  scast_data->is_ultravox = is_ultravox;
  http_free(http_hdr);
  stream->streaming_ctrl->data = scast_data;
  stream->streaming_ctrl->streaming_read = scast_streaming_read;
  stream->streaming_ctrl->streaming_seek = NULL;
  stream->streaming_ctrl->prebuffer_size = 64 * 1024; // 64 KBytes
  stream->streaming_ctrl->buffering = 1;
  stream->streaming_ctrl->status = streaming_playing_e;
  return 0;
}

static int nop_streaming_start( stream_t *stream ) {
	HTTP_header_t *http_hdr = NULL;
	char *next_url=NULL;
	URL_t *rd_url=NULL;
	int fd,ret;
	if( stream==NULL ) return -1;

	fd = stream->fd;
	if( fd<0 ) {
		fd = http_send_request( stream->streaming_ctrl->url, 0 ); 
		if( fd<0 ) return -1;
		http_hdr = http_read_response( fd );
		if( http_hdr==NULL ) return -1;

		switch( http_hdr->status_code ) {
			case 200: // OK
				mp_msg(MSGT_NETWORK,MSGL_V,"Content-Type: [%s]\n", http_get_field(http_hdr, "Content-Type") );
				mp_msg(MSGT_NETWORK,MSGL_V,"Content-Length: [%s]\n", http_get_field(http_hdr, "Content-Length") );
				if( http_hdr->body_size>0 ) {
					if( streaming_bufferize( stream->streaming_ctrl, http_hdr->body, http_hdr->body_size )<0 ) {
						http_free( http_hdr );
						return -1;
					}
				}
				break;
			// Redirect
			case 301: // Permanently
			case 302: // Temporarily
			case 303: // See Other
				ret=-1;
				next_url = http_get_field( http_hdr, "Location" );

				if (next_url != NULL)
					rd_url=url_new(next_url);

				if (next_url != NULL && rd_url != NULL) {
					mp_msg(MSGT_NETWORK,MSGL_STATUS,"Redirected: Using this url instead %s\n",next_url);
							stream->streaming_ctrl->url=check4proxies(rd_url);
					ret=nop_streaming_start(stream); //recursively get streaming started 
				} else {
					mp_msg(MSGT_NETWORK,MSGL_ERR,"Redirection failed\n");
					closesocket( fd );
					fd = -1;
				}
				return ret;
				break;
			case 401: //Authorization required
			case 403: //Forbidden
			case 404: //Not found
			case 500: //Server Error
			default:
				mp_msg(MSGT_NETWORK,MSGL_ERR,"Server returned code %d: %s\n", http_hdr->status_code, http_hdr->reason_phrase );
				closesocket( fd );
				fd = -1;
				return -1;
				break;
		}
		stream->fd = fd;
	} else {
		http_hdr = (HTTP_header_t*)stream->streaming_ctrl->data;
		if( http_hdr->body_size>0 ) {
			if( streaming_bufferize( stream->streaming_ctrl, http_hdr->body, http_hdr->body_size )<0 ) {
				http_free( http_hdr );
				stream->streaming_ctrl->data = NULL;
				return -1;
			}
		}
	}

	if( http_hdr ) {
		http_free( http_hdr );
		stream->streaming_ctrl->data = NULL;
	}

	stream->streaming_ctrl->streaming_read = nop_streaming_read;
	stream->streaming_ctrl->streaming_seek = nop_streaming_seek;
	stream->streaming_ctrl->prebuffer_size = 64*1024; // 64 KBytes
	stream->streaming_ctrl->buffering = 1;
	stream->streaming_ctrl->status = streaming_playing_e;
	return 0;
}

HTTP_header_t *
http_new_header(void) {
	HTTP_header_t *http_hdr;

	http_hdr = malloc(sizeof(HTTP_header_t));
	if( http_hdr==NULL ) return NULL;
	memset( http_hdr, 0, sizeof(HTTP_header_t) );

	return http_hdr;
}

void
http_free( HTTP_header_t *http_hdr ) {
	HTTP_field_t *field, *field2free;
	if( http_hdr==NULL ) return;
	if( http_hdr->protocol!=NULL ) free( http_hdr->protocol );
	if( http_hdr->uri!=NULL ) free( http_hdr->uri );
	if( http_hdr->reason_phrase!=NULL ) free( http_hdr->reason_phrase );
	if( http_hdr->field_search!=NULL ) free( http_hdr->field_search );
	if( http_hdr->method!=NULL ) free( http_hdr->method );
	if( http_hdr->buffer!=NULL ) free( http_hdr->buffer );
	field = http_hdr->first_field;
	while( field!=NULL ) {
		field2free = field;
		if (field->field_name)
		  free(field->field_name);
		field = field->next;
		free( field2free );
	}
	free( http_hdr );
	http_hdr = NULL;
}

int
http_response_append( HTTP_header_t *http_hdr, char *response, int length ) {
	if( http_hdr==NULL || response==NULL || length<0 ) return -1;

	if( (unsigned)length > SIZE_MAX - http_hdr->buffer_size - 1) {
		mp_msg(MSGT_NETWORK,MSGL_FATAL,"Bad size in memory (re)allocation\n");
		return -1;
	}
	http_hdr->buffer = (char*)realloc( http_hdr->buffer, http_hdr->buffer_size+length+1 );
	if( http_hdr->buffer==NULL ) {
		mp_msg(MSGT_NETWORK,MSGL_FATAL,"Memory (re)allocation failed\n");
		return -1;
	}
	memcpy( http_hdr->buffer+http_hdr->buffer_size, response, length );
	http_hdr->buffer_size += length;
	http_hdr->buffer[http_hdr->buffer_size]=0; // close the string!
	return http_hdr->buffer_size;
}

int
http_is_header_entire( HTTP_header_t *http_hdr ) {
	if( http_hdr==NULL ) return -1;
	if( http_hdr->buffer==NULL ) return 0; // empty
	
	if( strstr(http_hdr->buffer, "\r\n\r\n")==NULL &&
	    strstr(http_hdr->buffer, "\n\n")==NULL ) return 0;
	return 1;
}

int
http_response_parse( HTTP_header_t *http_hdr ) {
	char *hdr_ptr, *ptr;
	char *field=NULL;
	int pos_hdr_sep, hdr_sep_len;
	size_t len;
	if( http_hdr==NULL ) return -1;
	if( http_hdr->is_parsed ) return 0;

	// Get the protocol
	hdr_ptr = strstr( http_hdr->buffer, " " );
	if( hdr_ptr==NULL ) {
		mp_msg(MSGT_NETWORK,MSGL_ERR,"Malformed answer. No space separator found.\n");
		return -1;
	}
	len = hdr_ptr-http_hdr->buffer;
	http_hdr->protocol = malloc(len+1);
	if( http_hdr->protocol==NULL ) {
		mp_msg(MSGT_NETWORK,MSGL_FATAL,"Memory allocation failed\n");
		return -1;
	}
	strncpy( http_hdr->protocol, http_hdr->buffer, len );
	http_hdr->protocol[len]='\0';
	if( !strncasecmp( http_hdr->protocol, "HTTP", 4) ) {
		if( sscanf( http_hdr->protocol+5,"1.%d", &(http_hdr->http_minor_version) )!=1 ) {
			mp_msg(MSGT_NETWORK,MSGL_ERR,"Malformed answer. Unable to get HTTP minor version.\n");
			return -1;
		}
	}

	// Get the status code
	if( sscanf( ++hdr_ptr, "%d", &(http_hdr->status_code) )!=1 ) {
		mp_msg(MSGT_NETWORK,MSGL_ERR,"Malformed answer. Unable to get status code.\n");
		return -1;
	}
	hdr_ptr += 4;

	// Get the reason phrase
	ptr = strstr( hdr_ptr, "\n" );
	if( hdr_ptr==NULL ) {
		mp_msg(MSGT_NETWORK,MSGL_ERR,"Malformed answer. Unable to get the reason phrase.\n");
		return -1;
	}
	len = ptr-hdr_ptr;
	http_hdr->reason_phrase = malloc(len+1);
	if( http_hdr->reason_phrase==NULL ) {
		mp_msg(MSGT_NETWORK,MSGL_FATAL,"Memory allocation failed\n");
		return -1;
	}
	strncpy( http_hdr->reason_phrase, hdr_ptr, len );
	if( http_hdr->reason_phrase[len-1]=='\r' ) {
		len--;
	}
	http_hdr->reason_phrase[len]='\0';

	// Set the position of the header separator: \r\n\r\n
	hdr_sep_len = 4;
	ptr = strstr( http_hdr->buffer, "\r\n\r\n" );
	if( ptr==NULL ) {
		ptr = strstr( http_hdr->buffer, "\n\n" );
		if( ptr==NULL ) {
			mp_msg(MSGT_NETWORK,MSGL_ERR,"Header may be incomplete. No CRLF CRLF found.\n");
			return -1;
		}
		hdr_sep_len = 2;
	}
	pos_hdr_sep = ptr-http_hdr->buffer;

	// Point to the first line after the method line.
	hdr_ptr = strstr( http_hdr->buffer, "\n" )+1;
	do {
		ptr = hdr_ptr;
		while( *ptr!='\r' && *ptr!='\n' ) ptr++;
		len = ptr-hdr_ptr;
		if( len==0 ) break;
		field = (char*)realloc(field, len+1);
		if( field==NULL ) {
			mp_msg(MSGT_NETWORK,MSGL_ERR,"Memory allocation failed\n");
			return -1;
		}
		strncpy( field, hdr_ptr, len );
		field[len]='\0';
		http_set_field( http_hdr, field );
		hdr_ptr = ptr+((*ptr=='\r')?2:1);
	} while( hdr_ptr<(http_hdr->buffer+pos_hdr_sep) );
	
	if( field!=NULL ) free( field );

	if( pos_hdr_sep+hdr_sep_len<http_hdr->buffer_size ) {
		// Response has data!
		http_hdr->body = http_hdr->buffer+pos_hdr_sep+hdr_sep_len;
		http_hdr->body_size = http_hdr->buffer_size-(pos_hdr_sep+hdr_sep_len);
	}

	http_hdr->is_parsed = 1;
	return 0;
}

char *
http_build_request( HTTP_header_t *http_hdr ) {
	char *ptr, *uri=NULL;
	int len;
	HTTP_field_t *field;
	if( http_hdr==NULL ) return NULL;

	if( http_hdr->method==NULL ) http_set_method( http_hdr, "GET");
	if( http_hdr->uri==NULL ) http_set_uri( http_hdr, "/");
	else {
		uri = malloc(strlen(http_hdr->uri) + 1);
		if( uri==NULL ) {
			mp_msg(MSGT_NETWORK,MSGL_ERR,"Memory allocation failed\n");
			return NULL;
		}
		strcpy(uri,http_hdr->uri);
	}

	//**** Compute the request length
	// Add the Method line
	len = strlen(http_hdr->method)+strlen(uri)+12;
	// Add the fields
	field = http_hdr->first_field; 
	while( field!=NULL ) {
		len += strlen(field->field_name)+2;
		field = field->next;
	}
	// Add the CRLF
	len += 2;
	// Add the body
	if( http_hdr->body!=NULL ) {
		len += http_hdr->body_size;
	}
	// Free the buffer if it was previously used
	if( http_hdr->buffer!=NULL ) {
		free( http_hdr->buffer );
		http_hdr->buffer = NULL;
	}
	http_hdr->buffer = malloc(len+1);
	if( http_hdr->buffer==NULL ) {
		mp_msg(MSGT_NETWORK,MSGL_ERR,"Memory allocation failed\n");
		return NULL;
	}
	http_hdr->buffer_size = len;

	//*** Building the request
	ptr = http_hdr->buffer;
	// Add the method line
	ptr += sprintf( ptr, "%s %s HTTP/1.%d\r\n", http_hdr->method, uri, http_hdr->http_minor_version );
	field = http_hdr->first_field;
	// Add the field
	while( field!=NULL ) {
		ptr += sprintf( ptr, "%s\r\n", field->field_name );
		field = field->next;
	}
	ptr += sprintf( ptr, "\r\n" );
	// Add the body
	if( http_hdr->body!=NULL ) {
		memcpy( ptr, http_hdr->body, http_hdr->body_size );
	}

	if( uri ) free( uri );
	return http_hdr->buffer;	
}

char *
http_get_field( HTTP_header_t *http_hdr, const char *field_name ) {
	if( http_hdr==NULL || field_name==NULL ) return NULL;
	http_hdr->field_search_pos = http_hdr->first_field;
	http_hdr->field_search = (char*)realloc( http_hdr->field_search, strlen(field_name)+1 );
	if( http_hdr->field_search==NULL ) {
		mp_msg(MSGT_NETWORK,MSGL_FATAL,"Memory allocation failed\n");
		return NULL;
	}
	strcpy( http_hdr->field_search, field_name );
	return http_get_next_field( http_hdr );
}

char *
http_get_next_field( HTTP_header_t *http_hdr ) {
	char *ptr;
	HTTP_field_t *field;
	if( http_hdr==NULL ) return NULL;

	field = http_hdr->field_search_pos;
	while( field!=NULL ) { 
		ptr = strstr( field->field_name, ":" );
		if( ptr==NULL ) return NULL;
		if( !strncasecmp( field->field_name, http_hdr->field_search, ptr-(field->field_name) ) ) {
			ptr++;	// Skip the column
			while( ptr[0]==' ' ) ptr++; // Skip the spaces if there is some
			http_hdr->field_search_pos = field->next;
			return ptr;	// return the value without the field name
		}
		field = field->next;
	}
	return NULL;
}

void
http_set_field( HTTP_header_t *http_hdr, const char *field_name ) {
	HTTP_field_t *new_field;
	if( http_hdr==NULL || field_name==NULL ) return;

	new_field = malloc(sizeof(HTTP_field_t));
	if( new_field==NULL ) {
		mp_msg(MSGT_NETWORK,MSGL_FATAL,"Memory allocation failed\n");
		return;
	}
	new_field->next = NULL;
	new_field->field_name = malloc(strlen(field_name)+1);
	if( new_field->field_name==NULL ) {
		mp_msg(MSGT_NETWORK,MSGL_FATAL,"Memory allocation failed\n");
		free(new_field);
		return;
	}
	strcpy( new_field->field_name, field_name );

	if( http_hdr->last_field==NULL ) {
		http_hdr->first_field = new_field;
	} else {
		http_hdr->last_field->next = new_field;
	}
	http_hdr->last_field = new_field;
	http_hdr->field_nb++;
}

void
http_set_method( HTTP_header_t *http_hdr, const char *method ) {
	if( http_hdr==NULL || method==NULL ) return;

	http_hdr->method = malloc(strlen(method)+1);
	if( http_hdr->method==NULL ) {
		mp_msg(MSGT_NETWORK,MSGL_FATAL,"Memory allocation failed\n");
		return;
	}
	strcpy( http_hdr->method, method );
}

void
http_set_uri( HTTP_header_t *http_hdr, const char *uri ) {
	if( http_hdr==NULL || uri==NULL ) return;

	http_hdr->uri = malloc(strlen(uri)+1);
	if( http_hdr->uri==NULL ) {
		mp_msg(MSGT_NETWORK,MSGL_FATAL,"Memory allocation failed\n");
		return;
	}
	strcpy( http_hdr->uri, uri );
}

int
http_add_basic_authentication( HTTP_header_t *http_hdr, const char *username, const char *password ) {
	char *auth = NULL, *usr_pass = NULL, *b64_usr_pass = NULL;
	int encoded_len, pass_len=0, out_len;
	int res = -1;
	if( http_hdr==NULL || username==NULL ) return -1;

	if( password!=NULL ) {
		pass_len = strlen(password);
	}
	
	usr_pass = malloc(strlen(username)+pass_len+2);
	if( usr_pass==NULL ) {
		mp_msg(MSGT_NETWORK,MSGL_FATAL,"Memory allocation failed\n");
		goto out;
	}

	sprintf( usr_pass, "%s:%s", username, (password==NULL)?"":password );

	// Base 64 encode with at least 33% more data than the original size
	encoded_len = strlen(usr_pass)*2;
	b64_usr_pass = malloc(encoded_len);
	if( b64_usr_pass==NULL ) {
		mp_msg(MSGT_NETWORK,MSGL_FATAL,"Memory allocation failed\n");
		goto out;
	}

	out_len = base64_encode( usr_pass, strlen(usr_pass), b64_usr_pass, encoded_len);
	if( out_len<0 ) {
		mp_msg(MSGT_NETWORK,MSGL_FATAL,"Base64 out overflow\n");
		goto out;
	}

	b64_usr_pass[out_len]='\0';
	
	auth = malloc(encoded_len+22);
	if( auth==NULL ) {
		mp_msg(MSGT_NETWORK,MSGL_FATAL,"Memory allocation failed\n");
		goto out;
	}
	
	sprintf( auth, "Authorization: Basic %s", b64_usr_pass);
	http_set_field( http_hdr, auth );
	res = 0;
	
out:
	free( usr_pass );
	free( b64_usr_pass );
	free( auth );
	
	return res;
}

void
http_debug_hdr( HTTP_header_t *http_hdr ) {
	HTTP_field_t *field;
	int i = 0;
	if( http_hdr==NULL ) return;

	mp_msg(MSGT_NETWORK,MSGL_V,"--- HTTP DEBUG HEADER --- START ---\n");
	mp_msg(MSGT_NETWORK,MSGL_V,"protocol:           [%s]\n", http_hdr->protocol );
	mp_msg(MSGT_NETWORK,MSGL_V,"http minor version: [%d]\n", http_hdr->http_minor_version );
	mp_msg(MSGT_NETWORK,MSGL_V,"uri:                [%s]\n", http_hdr->uri );
	mp_msg(MSGT_NETWORK,MSGL_V,"method:             [%s]\n", http_hdr->method );
	mp_msg(MSGT_NETWORK,MSGL_V,"status code:        [%d]\n", http_hdr->status_code );
	mp_msg(MSGT_NETWORK,MSGL_V,"reason phrase:      [%s]\n", http_hdr->reason_phrase );
	mp_msg(MSGT_NETWORK,MSGL_V,"body size:          [%d]\n", http_hdr->body_size );

	mp_msg(MSGT_NETWORK,MSGL_V,"Fields:\n");
	field = http_hdr->first_field;
	while( field!=NULL ) {
		mp_msg(MSGT_NETWORK,MSGL_V," %d - %s\n", i++, field->field_name );
		field = field->next;
	}
	mp_msg(MSGT_NETWORK,MSGL_V,"--- HTTP DEBUG HEADER --- END ---\n");
}

int 
base64_encode(const void *enc, int encLen, char *out, int outMax) {
	static const char	b64[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

	unsigned char		*encBuf;
	int			outLen;
	unsigned int		bits;
	unsigned int		shift;

	encBuf = (unsigned char*)enc;
	outLen = 0;
	bits = 0;
	shift = 0;
	outMax &= ~3;

	while(1) {
		if( encLen>0 ) {
			// Shift in byte
			bits <<= 8;
			bits |= *encBuf;
			shift += 8;
			// Next byte
			encBuf++;
			encLen--;
		} else if( shift>0 ) {
			// Pad last bits to 6 bits - will end next loop
			bits <<= 6 - shift;
			shift = 6;
		} else {
			// As per RFC 2045, section 6.8,
			// pad output as necessary: 0 to 2 '=' chars.
			while( outLen & 3 ){
				*out++ = '=';
				outLen++;
			}

			return outLen;
		}

		// Encode 6 bit segments
		while( shift>=6 ) {
			if (outLen >= outMax)
				return -1;
			shift -= 6;
			*out = b64[ (bits >> shift) & 0x3F ];
			out++;
			outLen++;
		}
	}
}

static void print_icy_metadata(HTTP_header_t *http_hdr) {
	const char *field_data;
	// note: I skip icy-notice1 and 2, as they contain html <BR>
	// and are IMHO useless info ::atmos
	if( (field_data = http_get_field(http_hdr, "icy-name")) != NULL )
		mp_msg(MSGT_NETWORK,MSGL_INFO,"Name   : %s\n", field_data);
	if( (field_data = http_get_field(http_hdr, "icy-genre")) != NULL )
		mp_msg(MSGT_NETWORK,MSGL_INFO,"Genre  : %s\n", field_data);
	if( (field_data = http_get_field(http_hdr, "icy-url")) != NULL )
		mp_msg(MSGT_NETWORK,MSGL_INFO,"Website: %s\n", field_data);
	// XXX: does this really mean public server? ::atmos
	if( (field_data = http_get_field(http_hdr, "icy-pub")) != NULL )
		mp_msg(MSGT_NETWORK,MSGL_INFO,"Public : %s\n", atoi(field_data)?"yes":"no");
	if( (field_data = http_get_field(http_hdr, "icy-br")) != NULL )
		mp_msg(MSGT_NETWORK,MSGL_INFO,"Bitrate: %skbit/s\n", field_data);
}

//! If this function succeeds you must closesocket stream->fd
static int http_streaming_start(stream_t *stream, int* file_format) {
	HTTP_header_t *http_hdr = NULL;
	unsigned int i;
	int fd = stream->fd;
	int res = STREAM_UNSUPPORTED;
	int redirect = 0;
	int auth_retry=0;
	int seekable=0;
	char *content_type;
	char *next_url;
	URL_t *url = stream->streaming_ctrl->url;

	do
	{
		redirect = 0;
		if (fd > 0) closesocket(fd);
		fd = http_send_request( url, 0 );
		if( fd<0 ) {
			goto err_out;
		}

		http_free(http_hdr);
		http_hdr = http_read_response( fd );
		if( http_hdr==NULL ) {
			goto err_out;
		}

		if( mp_msg_test(MSGT_NETWORK,MSGL_V) ) {
			http_debug_hdr( http_hdr );
		}
		
		// Check if we can make partial content requests and thus seek in http-streams
		if( http_hdr!=NULL && http_hdr->status_code==200 ) {
		    char *accept_ranges;
		    if( (accept_ranges = http_get_field(http_hdr,"Accept-Ranges")) != NULL )
			seekable = strncmp(accept_ranges,"bytes",5)==0;
		}

		print_icy_metadata(http_hdr);

		// Check if the response is an ICY status_code reason_phrase
		if( !strcasecmp(http_hdr->protocol, "ICY") ||
		     http_get_field(http_hdr, "Icy-MetaInt") ) {
			switch( http_hdr->status_code ) {
				case 200: { // OK
					char *field_data;
					// If content-type == video/nsv we most likely have a winamp video stream 
					// otherwise it should be mp3. if there are more types consider adding mime type 
					// handling like later
					if ( (field_data = http_get_field(http_hdr, "content-type")) != NULL && (!strcmp(field_data, "video/nsv") || !strcmp(field_data, "misc/ultravox")))
						*file_format = DEMUXER_TYPE_NSV;
					else if ( (field_data = http_get_field(http_hdr, "content-type")) != NULL && (!strcmp(field_data, "audio/aacp") || !strcmp(field_data, "audio/aac")))
						*file_format = DEMUXER_TYPE_AAC;
					else
						*file_format = DEMUXER_TYPE_AUDIO;
					res = STREAM_ERROR;
					goto out;
				}
				case 400: // Server Full
					mp_msg(MSGT_NETWORK,MSGL_ERR,"Error: ICY-Server is full, skipping!\n");
					goto err_out;
				case 401: // Service Unavailable
					mp_msg(MSGT_NETWORK,MSGL_ERR,"Error: ICY-Server return service unavailable, skipping!\n");
					goto err_out;
				case 403: // Service Forbidden
					mp_msg(MSGT_NETWORK,MSGL_ERR,"Error: ICY-Server return 'Service Forbidden'\n");
					goto err_out;
				case 404: // Resource Not Found
					mp_msg(MSGT_NETWORK,MSGL_ERR,"Error: ICY-Server couldn't find requested stream, skipping!\n");
					goto err_out;
				default:
					mp_msg(MSGT_NETWORK,MSGL_ERR,"Error: unhandled ICY-Errorcode, contact MPlayer developers!\n");
					goto err_out;
			}
		}

		// Assume standard http if not ICY			
		switch( http_hdr->status_code ) {
			case 200: // OK
				// Look if we can use the Content-Type
				content_type = http_get_field( http_hdr, "Content-Type" );
				if( content_type!=NULL ) {
					char *content_length = NULL;
					mp_msg(MSGT_NETWORK,MSGL_V,"Content-Type: [%s]\n", content_type );
					if( (content_length = http_get_field(http_hdr, "Content-Length")) != NULL) {
						mp_msg(MSGT_NETWORK,MSGL_V,"Content-Length: [%s]\n", http_get_field(http_hdr, "Content-Length"));
						stream->end_pos = atoi(content_length);
					}
					// Check in the mime type table for a demuxer type
					i = 0;
					while(mime_type_table[i].mime_type != NULL) {
						if( !strcasecmp( content_type, mime_type_table[i].mime_type ) ) {
							*file_format = mime_type_table[i].demuxer_type;
							res = seekable;
							goto out;
						}
						i++;
					}
				}
				// Not found in the mime type table, don't fail,
				// we should try raw HTTP
				res = seekable;
				goto out;
			// Redirect
			case 301: // Permanently
			case 302: // Temporarily
			case 303: // See Other
				// TODO: RFC 2616, recommand to detect infinite redirection loops
				next_url = http_get_field( http_hdr, "Location" );
				if( next_url!=NULL ) {
					int is_ultravox = strcasecmp(stream->streaming_ctrl->url->protocol, "unsv") == 0;
					stream->streaming_ctrl->url = url_redirect( &url, next_url );
					if (!strcasecmp(url->protocol, "mms")) {
						res = STREAM_REDIRECTED;
						goto err_out;
					}
					if (strcasecmp(url->protocol, "http")) {
						mp_msg(MSGT_NETWORK,MSGL_ERR,"Unsupported http %d redirect to %s protocol\n", http_hdr->status_code, url->protocol);
						goto err_out;
					}
					if (is_ultravox)  {
						free(url->protocol);
						url->protocol = strdup("unsv");
					}
					redirect = 1;	
				}
				break;
			case 401: // Authentication required
				if( http_authenticate(http_hdr, url, &auth_retry)<0 )
					goto err_out;
				redirect = 1;
				break;
			default:
				mp_msg(MSGT_NETWORK,MSGL_ERR,"Server returned %d: %s\n", http_hdr->status_code, http_hdr->reason_phrase );
				goto err_out;
		}
	} while( redirect );

err_out:
	if (fd > 0) closesocket( fd );
	fd = -1;
	http_free( http_hdr );
	http_hdr = NULL;
out:
	stream->streaming_ctrl->data = (void*)http_hdr;
	stream->fd = fd;
	return res;
}

static int fixup_open(stream_t *stream,int seekable) {
	HTTP_header_t *http_hdr = stream->streaming_ctrl->data;
	int is_icy = http_hdr && http_get_field(http_hdr, "Icy-MetaInt");
	int is_ultravox = strcasecmp(stream->streaming_ctrl->url->protocol, "unsv") == 0;

	stream->type = STREAMTYPE_STREAM;
	if(!is_icy && !is_ultravox && seekable)
	{
		stream->flags |= STREAM_SEEK;
		stream->seek = http_seek;
	}
	stream->streaming_ctrl->bandwidth = network_bandwidth;
	if ((!is_icy && !is_ultravox) || scast_streaming_start(stream))
	if(nop_streaming_start( stream )) {
		mp_msg(MSGT_NETWORK,MSGL_ERR,"nop_streaming_start failed\n");
		if (stream->fd >= 0)
			closesocket(stream->fd);
		stream->fd = -1;
		streaming_ctrl_free(stream->streaming_ctrl);
		stream->streaming_ctrl = NULL;
		return STREAM_UNSUPPORTED;
	}

	fixup_network_stream_cache(stream);
	return STREAM_OK;
}

static int open_s1(stream_t *stream,int mode, void* opts, int* file_format) {
	int seekable=0;
	URL_t *url;

	stream->streaming_ctrl = streaming_ctrl_new();
	if( stream->streaming_ctrl==NULL ) {
		return STREAM_ERROR;
	}
	stream->streaming_ctrl->bandwidth = network_bandwidth;
	url = url_new(stream->url);
	stream->streaming_ctrl->url = check4proxies(url);
	url_free(url);
	
	mp_msg(MSGT_OPEN, MSGL_V, "STREAM_HTTP(1), URL: %s\n", stream->url);
	seekable = http_streaming_start(stream, file_format);
	if((seekable < 0) || (*file_format == DEMUXER_TYPE_ASF)) {
		if (stream->fd >= 0)
			closesocket(stream->fd);
		stream->fd = -1;
		if (seekable == STREAM_REDIRECTED)
			return seekable;
		streaming_ctrl_free(stream->streaming_ctrl);
		stream->streaming_ctrl = NULL;
		return STREAM_UNSUPPORTED;
	}

	return fixup_open(stream, seekable);
}

static int open_s2(stream_t *stream,int mode, void* opts, int* file_format) {
	int seekable=0;
	URL_t *url;

	stream->streaming_ctrl = streaming_ctrl_new();
	if( stream->streaming_ctrl==NULL ) {
		return STREAM_ERROR;
	}
	stream->streaming_ctrl->bandwidth = network_bandwidth;
	url = url_new(stream->url);
	stream->streaming_ctrl->url = check4proxies(url);
	url_free(url);
	
	mp_msg(MSGT_OPEN, MSGL_V, "STREAM_HTTP(2), URL: %s\n", stream->url);
	seekable = http_streaming_start(stream, file_format);
	if(seekable < 0) {
		if (stream->fd >= 0)
			closesocket(stream->fd);
		stream->fd = -1;
		streaming_ctrl_free(stream->streaming_ctrl);
		stream->streaming_ctrl = NULL;
		return STREAM_UNSUPPORTED;
	}

	return fixup_open(stream, seekable);
}


const stream_info_t stream_info_http1 = {
  "http streaming",
  "null",
  "Bertrand, Albeau, Reimar Doeffinger, Arpi?",
  "plain http",
  open_s1,
  {"http", "http_proxy", "unsv", "icyx", "noicyx", NULL},
  NULL,
  0 // Urls are an option string
};

const stream_info_t stream_info_http2 = {
  "http streaming",
  "null",
  "Bertrand, Albeu, Arpi? who?",
  "plain http, also used as fallback for many other protocols",
  open_s2,
  {"http", "http_proxy", "pnm", "mms", "mmsu", "mmst", "rtsp", NULL},	//all the others as fallback
  NULL,
  0 // Urls are an option string
};
