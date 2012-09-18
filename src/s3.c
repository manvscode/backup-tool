#include <string.h>
#include <assert.h>
#include <openssl/sha.h>
#include <openssl/hmac.h>
#include <openssl/evp.h>
#include <curl/curl.h>
#include <libxml/tree.h>
#include <libxml/parser.h>
#include <libxml/xpath.h>
#include <libxml/xpathInternals.h>
#include "base64.h"
#include "s3.h"

#define S3_HOSTNAME          "s3.amazonaws.com"
#define S3_USERAGENT         "Shrewd LLC/S3"

/* path sanity checks */
/* JoeM: Previously we allowed empty strings (i.e. "") */
#define s3_is_path_valid( s_path )                  ((s_path) && strlen((s_path)) > 0)



static uint s3_initialization_count = 0;

/* file size helper */
size_t file_size_from_pointer( FILE *p_file, boolean b_keep_open );
/* cURL Write Handlers */
size_t  _s3_list_buckets_handle_response  ( void *ptr, size_t size, size_t nmemb, void *data );
boolean _s3_list_buckets_process_response ( const S3 *p_s3, const MemoryBuffer *p_memory );
void    _print_s3_buckets( FILE *output, xmlDocPtr doc, xmlNodeSetPtr nodes );
void    _debug_dump_response( FILE *p_output, const MemoryBuffer *p_memory ); 


void s3_initialize( S3 *p_s3, const char *access_id, const char *secret_key, boolean verbose ) {
	// ---- CODE ----
	strncpy( p_s3->s_aws_access_id,  access_id,  sizeof(p_s3->s_aws_access_id) );
	strncpy( p_s3->s_aws_secret_key, secret_key, sizeof(p_s3->s_aws_secret_key) );
	p_s3->b_verbose = verbose;
	
	if( s3_initialization_count <= 0 ) {
		/* Init libxml */     
		xmlInitParser( );
		s3_initialization_count++;
	}
}

void s3_deinitialize( void ) {
	if( s3_initialization_count > 0 ) {
		/* Shutdown libxml */
		xmlCleanupParser();
		s3_initialization_count--;
	}
}

void s3_format_time( /*out*/ char *s_destination_string, size_t length ) {
	// ---- VAR ----
	time_t ts     = time( NULL );
	struct tm *tm = gmtime( &ts );

	// ---- CODE ----
	assert( s_destination_string );
	
	strftime( s_destination_string, length, "%a, %d %b %Y %H:%M:%S +0000", tm );
}

/* caller must free returned pointer */
const byte *s3_sign( const S3 *p_s3, const char* s_sign_string ) {
	// ---- VAR ----
	byte signature_plain[ EVP_MAX_MD_SIZE ];
	unsigned int ret_size 	= 0;
	HMAC_CTX hmac;

	// ---- CODE ----
	assert( p_s3 );
	assert( s_sign_string );

	/* create signature */
	HMAC_Init( &hmac, p_s3->s_aws_secret_key, strlen(p_s3->s_aws_secret_key), EVP_sha1() );
	HMAC_Update( &hmac, (byte *) s_sign_string, strlen(s_sign_string) );
	HMAC_Final( &hmac, signature_plain, (uint *) &ret_size );

	/* JoeM: Let's be cautious and watch out for buffer overruns. */
	assert( ret_size < EVP_MAX_MD_SIZE ); 

	/* base64 encode signature */
	const byte *signature_base64 = base64( signature_plain, (size_t) ret_size );
	
	/* cleanup */
	HMAC_cleanup( &hmac );

	return signature_base64; // this must be freed
}


int s3_response_code( const CURL *p_curl ) {
	// ---- VAR ----
	long status = 0L;

	// ---- CODE ----
	assert( p_curl );
	curl_easy_getinfo( (CURL *) p_curl, CURLINFO_RESPONSE_CODE, &status );
	return (int) status;
}

/* list buckets */
boolean s3_list_buckets( CURL *p_curl, const S3 *p_s3 ) {
	// ---- VAR ----
	char curl_err[ CURL_ERROR_SIZE ];
	char format_time[ 128 ];
	char buffer[ 1024 ];
	struct curl_slist *headerlist = NULL;
	CURLcode res                  = 0;
	boolean b_result              = TRUE;
	#ifndef _S3_CURL_COPIES_STRINGS
	char url[ 1024 ];
	#endif
	MemoryBuffer chunk;

	// ---- CODE ----
	assert( p_curl );
	assert( p_s3 );
	memset( &chunk, 0, sizeof(MemoryBuffer) );
	
	/* Prepare data */
	{
		s3_format_time( format_time, sizeof(format_time) );
	}

	if( b_result ) {

		curl_easy_setopt( p_curl, CURLOPT_ERRORBUFFER, curl_err );
		curl_easy_setopt( p_curl, CURLOPT_USERAGENT, S3_USERAGENT );
		curl_easy_setopt( p_curl, CURLOPT_FAILONERROR, 1 ); 
		curl_easy_setopt( p_curl, CURLOPT_WRITEFUNCTION, _s3_list_buckets_handle_response );
		curl_easy_setopt( p_curl, CURLOPT_WRITEDATA, (void *) &chunk );

		/* build URL */
		{
			#ifdef _S3_CURL_COPIES_STRINGS
			snprintf( buffer, sizeof(buffer), "https://%s/", S3_HOSTNAME );
			curl_easy_setopt( p_curl, CURLOPT_URL, buffer /* URL */ );
			#else
			snprintf( url, sizeof(url), "https://%s/", S3_HOSTNAME );
			curl_easy_setopt( p_curl, CURLOPT_URL, url );
			#endif
		}

		/* assemble headers and add headers to curl request */
		{
			/* build and add date header */
			{
				snprintf( buffer, sizeof(buffer), "Date: %s", format_time );
				headerlist = curl_slist_append( headerlist, buffer /* data header */ );
			}

			/* build and add authorization header */
			{
				snprintf( buffer, sizeof(buffer), "GET\n\n\n%s\n/", format_time );
				/* sign and base64 encode signature */
				const byte *signature_base64 = s3_sign( p_s3, buffer /* PUT string */ );
				snprintf( buffer, sizeof(buffer), "Authorization: AWS %s:%s", p_s3->s_aws_access_id, signature_base64 );
				headerlist = curl_slist_append( headerlist, buffer /* Authorization header */ );
				free( (byte *) signature_base64 ); /* signature cleanup */
			}

			curl_easy_setopt( p_curl, CURLOPT_HTTPHEADER, headerlist );
		}

		/* perform request */				
		res = curl_easy_perform( p_curl );

		if( res != 0 ) {
			if( s3_is_verbose(p_s3) ) fprintf( stderr, "%s:%d: Error performing curl request (res = %d, err = %.1024s).\n", __FUNCTION__, __LINE__, res, curl_err );			
			b_result = FALSE;
		}	

		/* check http status code */
		int i_response_code = s3_response_code( p_curl );
		if( i_response_code != 200 ) { /* did we not get 200? */
			if( s3_is_verbose(p_s3) ) fprintf( stderr, "%s:%d: Wrong HTTP response while talking to S3 host (res = %d).\n", __FUNCTION__, __LINE__, i_response_code );			
			b_result = FALSE;
		}

		/* cleanup */
		curl_slist_free_all( headerlist );
		#ifdef _DEBUG
		headerlist = NULL;
		#endif
	}

	if( b_result ) {
		#ifdef _DEBUG
		_debug_dump_response( stdout, &chunk );
		#endif
		b_result = _s3_list_buckets_process_response( p_s3, &chunk );
		free( chunk.buffer );
	}

	return b_result;
}

size_t _s3_list_buckets_handle_response( void *ptr, size_t size, size_t nmemb, void *data ) {
	register size_t realsize = size * nmemb;
	MemoryBuffer *mem        = (MemoryBuffer *) data;
	mem->buffer              = (byte *) realloc( mem->buffer, mem->size + realsize + 1 );

	if( mem->buffer ) {
		memcpy( &(mem->buffer[ mem->size ]), ptr, realsize );
		mem->size               += realsize;
		mem->buffer[ mem->size ] = 0;
	}

	return realsize;
}

boolean _s3_list_buckets_process_response ( const S3 *p_s3, const MemoryBuffer *p_memory ) {
	xmlDocPtr doc;
	xmlXPathContextPtr xpathCtx; 
	xmlXPathObjectPtr xpathObj; 

	assert( p_s3 );
	assert( p_memory );

	/* Load XML document */
	doc = xmlParseMemory( (char *) p_memory->buffer, p_memory->size );
	if( doc == NULL ) {
		if( s3_is_verbose(p_s3) ) fprintf( stderr, "Error: unable to parse memory buffer.\n" );
		return FALSE;
	}

	/* Create xpath evaluation context */
	xpathCtx = xmlXPathNewContext( doc );
	if( xpathCtx == NULL ) {
		if( s3_is_verbose(p_s3) ) fprintf( stderr,"Error: unable to create new XPath context\n" );
		xmlFreeDoc( doc );
		return FALSE;
	}

	/* do register namespace */
	const char *prefix = "aws";
	const char *href = "http://s3.amazonaws.com/doc/2006-03-01/";
	if( xmlXPathRegisterNs(xpathCtx, BAD_CAST prefix, BAD_CAST href) != 0 ) {
		if( s3_is_verbose(p_s3) ) fprintf( stderr,"Error: unable to register NS with prefix=\"%s\" and href=\"%s\"\n", prefix, href );
		xmlXPathFreeContext( xpathCtx );
		xmlFreeDoc( doc );
		return FALSE;
	}

	/* Evaluate xpath expression */
	xpathObj = xmlXPathEvalExpression( BAD_CAST "//aws:Bucket", xpathCtx );
	if( xpathObj == NULL ) {
		if( s3_is_verbose(p_s3) ) fprintf( stderr,"Error: unable to evaluate xpath expression.\n" );
		xmlXPathFreeContext( xpathCtx );
		xmlFreeDoc( doc );
		return FALSE;
	}

	/* Print results */
	_print_s3_buckets( stdout, doc, xpathObj->nodesetval );

	/* Cleanup */
	xmlXPathFreeObject( xpathObj );
	xmlXPathFreeContext( xpathCtx ); 
	xmlFreeDoc( doc ); 

	return TRUE;
}

void _print_s3_buckets( FILE *output, xmlDocPtr doc, xmlNodeSetPtr nodes ) {
	xmlNodePtr currentBucket;
	int size;
	int i;

	assert(output);
	size = (nodes) ? nodes->nodeNr : 0;

	if( size > 0 ) {
		fprintf( output, "%-20s %-20s\n", "Bucket Name", "Created On" );

		for(i = 0; i < size; ++i) {
			assert(nodes->nodeTab[i]);
			currentBucket = nodes->nodeTab[i];   	    
			xmlNodePtr child;

			for( child = currentBucket->children; child; child = child->next ) {
				if( child->type !=  XML_ELEMENT_NODE ) continue;

				if( xmlStrcasecmp(child->name, BAD_CAST "name") == 0 ) {
					xmlChar *name = xmlNodeListGetString(doc, child->children, 1);
					fprintf( output, "%-20s ", name );
					xmlFree( name );
				}
				else if( xmlStrcasecmp(child->name, BAD_CAST "creationdate") == 0 ) {
					xmlChar *date = xmlNodeListGetString(doc, child->children, 1);
					fprintf( output, "%-20s", date );
					xmlFree( date );
				}
			}
			printf( "\n" );
		}
	}
	else { /* no buckets */
		fprintf( output, "No buckets exist.\n" );
	}
}

void _debug_dump_response( FILE *p_output, const MemoryBuffer *p_memory ) {
	assert( p_output );
	assert( p_memory );

	fprintf( p_output, "----------------- S3 RESPONSE ------------------\n" );

	p_memory->buffer[ p_memory->size ] = '\0';
	fprintf( p_output, "%s\n", p_memory->buffer );

	fprintf( p_output, "------------------------------------------------\n" );
}

boolean s3_put_file( CURL *p_curl, const S3 *p_s3, const char *s_bucket, const char *s_key, const char *s_filename, const char *mime_type ) {
	// ---- VAR ----
	char curl_err[ CURL_ERROR_SIZE ];
    char format_time[ 128 ];
	char buffer[ 1024 ];
	#ifndef _S3_CURL_COPIES_STRINGS
	char url[ 1024 ];
	#endif
	struct curl_slist *headerlist = NULL;
	FILE *fd_tmp                  = NULL;
	size_t l_size                 = 0;
	CURLcode res                  = 0;
	boolean b_result              = TRUE;

	// ---- CODE ----
	assert( p_curl );
	assert( p_s3 );
	assert( s_bucket );
	assert( *s_bucket && *s_bucket != '/' );
	assert( s_key );
	assert( *s_key && *s_key != '/' );
	assert( s_filename );
	assert( mime_type );
	
	/* Prepare data */
	{
		s3_format_time( format_time, sizeof(format_time) );
		
		/* sanity checks */
		if( !s3_is_path_valid( s_key ) ) {
			if( s3_is_verbose(p_s3) ) fprintf( stderr, "Bad S3 key.\n" );
			b_result = FALSE;
		}	
	}

	/* open file */
	if( b_result )
	{
		fd_tmp = fopen( s_filename, "rb" );

		if( !fd_tmp ) {
			if( s3_is_verbose(p_s3) ) fprintf( stderr, "Cannot open file\n" );
			b_result = FALSE;
		}
	}

	if( b_result /*ec == 0*/ ) {
		#ifdef _NO_FILE_STDIO_STREAM
		curl_easy_setopt( p_curl, CURLOPT_READDATA, fd_tmp ); /* I had no stdio_stream in my FILE structure */
		#else
		curl_easy_setopt( p_curl, CURLOPT_READDATA, fd_tmp->stdio_stream ); /*** URGENT: this is for FCGI compatibility, normally is would be fd_tmp only !!! ***/		
		#endif
		curl_easy_setopt( p_curl, CURLOPT_UPLOAD, 1 );
		curl_easy_setopt( p_curl, CURLOPT_ERRORBUFFER, curl_err );
		curl_easy_setopt( p_curl, CURLOPT_USERAGENT, S3_USERAGENT );
		curl_easy_setopt( p_curl, CURLOPT_FAILONERROR, 1 ); 
	

		l_size = file_size_from_pointer( fd_tmp, TRUE ); /* determine filesize */

		curl_easy_setopt( p_curl, CURLOPT_INFILESIZE, l_size );

		const char *uri_encoded = NULL;
		/* URL encode resource URI */
		{
			snprintf( buffer, sizeof(buffer), "%s/%s", s_bucket, s_key );
			uri_encoded = curl_escape( buffer, 0 );
		}


		/* build URL */
		{
			#ifdef _S3_CURL_COPIES_STRINGS
			snprintf( buffer, sizeof(buffer), "https://%s/%s", S3_HOSTNAME, uri_encoded );
			curl_easy_setopt( p_curl, CURLOPT_URL, buffer /* URL */ );
			#else
			snprintf( url, sizeof(url), "https://%s/%s", S3_HOSTNAME, uri_encoded );
			curl_easy_setopt( p_curl, CURLOPT_URL, url /* URL */ );
			#endif
		}

		/* assemble headers and add headers to curl request */
		{
			/* build and add content type header */
			{
				snprintf( buffer, sizeof(buffer), "Content-Type: %s", mime_type );
				headerlist = curl_slist_append( headerlist, buffer /* content type header */ );
			}

			/* build and add date header */
			{
				snprintf( buffer, sizeof(buffer), "Date: %s", format_time );
				headerlist = curl_slist_append( headerlist, buffer /* data header */ );
			}

			/* build and add ACL header */
			{
				snprintf( buffer, sizeof(buffer), "x-amz-acl: public-read" );
				headerlist = curl_slist_append( headerlist, buffer /* ACL header */ );
			}

			/* build and add authorization header */
			{
				snprintf( buffer, sizeof(buffer), "PUT\n\n%s\n%s\nx-amz-acl:public-read\n/%s", mime_type, format_time, uri_encoded );
				/* sign and base64 encode signature */
				const byte *signature_base64 = s3_sign( p_s3, buffer /* PUT string */ );
				snprintf( buffer, sizeof(buffer), "Authorization: AWS %s:%s", p_s3->s_aws_access_id, signature_base64 );
				headerlist = curl_slist_append( headerlist, buffer /* Authorization header */ );
				free( (byte *) signature_base64 ); /* signature cleanup */
			}
		
			curl_easy_setopt( p_curl, CURLOPT_HTTPHEADER, headerlist );
		}
	
		/* free encoded URI */	
		curl_free( (char *) uri_encoded );

		/* perform request */				
		res = curl_easy_perform( p_curl );

		if( res != 0 ) {
			if( s3_is_verbose(p_s3) ) fprintf( stderr, "%s:%d: Error performing curl request (res = %d, err = %.1024s).\n", __FUNCTION__, __LINE__, res, curl_err );			
			b_result = FALSE;
		}	

		/* check http status code */
		int i_response_code = s3_response_code( p_curl );
		if( i_response_code != 200 ) { /* did we not get 200? */
			if( s3_is_verbose(p_s3) ) fprintf( stderr, "%s:%d: Wrong HTTP response while talking to S3 host (res = %d).\n", __FUNCTION__, __LINE__, i_response_code );			
			b_result = FALSE;
		}

		/* cleanup */
		fclose( fd_tmp );
		curl_slist_free_all( headerlist );
		#ifdef _DEBUG
		headerlist = NULL;
		#endif
	}

	return b_result;
}


boolean s3_delete_file( CURL *p_curl, const S3 *p_s3, const char *s_bucket, const char *s_key ) {
	// ---- VAR ----
	char curl_err[ CURL_ERROR_SIZE ];
	char format_time[ 128 ];
    char buffer[ 1024 ];
	#ifndef _S3_CURL_COPIES_STRINGS
	char url[ 1024 ];
	#endif
	struct curl_slist *headerlist = NULL;
	CURLcode res                  = 0;
	boolean b_result              = TRUE;

	// ---- CODE ----
	assert( p_curl );
	assert( p_s3 );
	assert( s_bucket );
	assert( *s_bucket && *s_bucket != '/' );
	assert( s_key );
	assert( *s_key && *s_key != '/' );
	
	/* Prepare data */
	{
		s3_format_time( format_time, sizeof(format_time) );
		
		/* sanity checks */
		if( !s3_is_path_valid( s_key ) ) {
			if( s3_is_verbose(p_s3) ) fprintf( stderr, "Bad S3 key.\n" );
			b_result = FALSE;
		}	
	}

	if( b_result /*ec == 0*/ ) {
		curl_easy_setopt( p_curl, CURLOPT_ERRORBUFFER, curl_err);								
		curl_easy_setopt( p_curl, CURLOPT_USERAGENT, S3_USERAGENT );
		curl_easy_setopt( p_curl, CURLOPT_CUSTOMREQUEST, "DELETE" );					
		curl_easy_setopt( p_curl, CURLOPT_FAILONERROR, 1 ); 

		const char *uri_encoded = NULL;
		/* URL encode resource URI */
		{
			snprintf( buffer, sizeof(buffer), "%s/%s", s_bucket, s_key );
			uri_encoded = curl_escape( buffer, 0 );
		}

		/* build URL */
		{
			#ifdef _S3_CURL_COPIES_STRINGS
			snprintf( buffer, sizeof(buffer), "https://%s/%s", S3_HOSTNAME, uri_encoded );
			curl_easy_setopt( p_curl, CURLOPT_URL, buffer /* URL */ );
			#else
			snprintf( url, sizeof(url), "https://%s/%s", S3_HOSTNAME, uri_encoded );
			curl_easy_setopt( p_curl, CURLOPT_URL, url );
			#endif
		}

		/* assemble headers and add headers to curl request */
		{
			/* build and add date header */
			{
				snprintf( buffer, sizeof(buffer), "Date: %s", format_time );
				headerlist = curl_slist_append( headerlist, buffer /* data header */ );
			}

			/* build and add authorization header */
			{
				snprintf( buffer, sizeof(buffer), "DELETE\n\n\n%s\n/%s", format_time, uri_encoded );
				/* sign and base64 encode signature */
				const byte *signature_base64 = s3_sign( p_s3, buffer /* DELETE string */ );
				snprintf( buffer, sizeof(buffer), "Authorization: AWS %s:%s", p_s3->s_aws_access_id, signature_base64 );
				headerlist = curl_slist_append( headerlist, buffer /* Authorization header */ );
				free( (byte *) signature_base64 ); /* signature cleanup */
			}
		
			curl_easy_setopt( p_curl, CURLOPT_HTTPHEADER, headerlist );
		}

		/* free encoded URI */	
		curl_free( (char *) uri_encoded );

		/* perform request */				
		res = curl_easy_perform( p_curl );

		/* wir ignorieren hier fehler 21, weil wenn file nicht existert, is das wurst. */
		if (res != 0 && res != 21) { 
			if( s3_is_verbose(p_s3) ) fprintf( stderr, "%s:%d: Error performing curl request (res = %d, err = %.1024s).\n", __FUNCTION__, __LINE__, res, curl_err );			
			b_result = FALSE;
		}

		/* check http status code */
		int i_response_code = s3_response_code( p_curl );
		if( i_response_code != 200 && i_response_code != 204 ) { /* did we not get 200 or 204? */
			if( s3_is_verbose(p_s3) ) fprintf( stderr, "%s:%d: Wrong HTTP response while talking to S3 host (res = %d).\n", __FUNCTION__, __LINE__, i_response_code );			
			b_result = FALSE;
		}

		/* cleanup */
		curl_slist_free_all( headerlist );
		#ifdef _DEBUG
		headerlist = NULL;
		#endif
	}

	return b_result;
}

size_t file_size_from_pointer( FILE *p_file, boolean b_keep_open ) {
	// ---- VAR ----
	size_t size = 0;

	// ---- CODE ----
	fseek( p_file, 0, SEEK_END );
	size = ftell( p_file );
	rewind( p_file );

	if( !b_keep_open ) {
		fclose( p_file );
	}

	return size;
}
