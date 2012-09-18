#ifndef _FTP_H_
#define _FTP_H_

#include <curl/curl.h>
#include "types.h"

boolean ftp_upload( CURL *p_curl, const char *s_hostname, const char *s_username, const char *s_password, const char *s_path, const char *s_filename );
boolean ftp_delete( CURL *p_curl, const char *s_hostname, const char *s_username, const char *s_password, const char *s_filepath );

#endif /* _FTP_H_ */
