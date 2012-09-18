#ifndef _S3_H_
#define _S3_H_

#include <stdio.h>
#include <curl/curl.h>
#include "types.h"

typedef struct sS3 {
	char s_aws_access_id[ 64 ];
	char s_aws_secret_key[ 64 ];
	boolean b_verbose;
} S3;

typedef struct sMemoryBuffer {
	byte *buffer;
	size_t size;
} MemoryBuffer;

#define S3_MAX_BUCKET_NAME   (255)

#define s3_is_verbose( p_s3 ) ( (p_s3)->b_verbose )
void    s3_initialize     ( S3 *p_s3, const char *access_id, const char *secret_key, boolean verbose );
void    s3_deinitialize   ( void );
void    s3_format_time    ( /* out */ char *s_destination_string, size_t length );
const   byte *s3_sign     ( const S3 *p_s3, const char* s_sign_string );
int     s3_response_code  ( const CURL *p_curl );
boolean s3_list_buckets   ( CURL *p_curl, const S3 *p_s3 );
boolean s3_put_file       ( CURL *p_curl, const S3 *p_s3, const char *s_bucket, const char *s_key, const char *s_filename, const char *mime_type );
boolean s3_delete_file    ( CURL *p_curl, const S3 *p_s3, const char *s_bucket, const char *s_key );
#define s3_verify_response_code( p_curl, i_code )   (s3_response_code( (p_curl) ) == ((int) i_code))
#define s3_response_ok( p_curl )                    (s3_verify_response_code( (p_curl), 200 ))

#endif // _S3_H_
