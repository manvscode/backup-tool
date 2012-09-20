#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include "ftp.h"

#define FTP_USERAGENT   "Shrewd LLC/FTP"

boolean ftp_upload( CURL *p_curl, const char *s_hostname, const char *s_username, const char *s_password, const char *s_path, const char *s_filename )
{
	char curl_err[ CURL_ERROR_SIZE ];
	char buffer[ 1024 ];
	struct curl_slist *headerlist = NULL;
	FILE *fd_tmp                  = NULL;
	CURLcode res                  = 0;
	boolean b_result              = TRUE;

	assert( p_curl );
	assert( s_hostname );
	assert( s_filename );

	/* open file */
	fd_tmp = fopen( s_filename, "rb" );

	if( !fd_tmp )
	{
		b_result = FALSE;
		fprintf( stderr, "Cannot open file\n" );
	}

	/* prepare url */

	if( b_result )
	{
		#ifdef _NO_FILE_STDIO_STREAM
			curl_easy_setopt( p_curl, CURLOPT_READDATA, fd_tmp ); /* I had no stdio_stream in my FILE structure */
		#else
			curl_easy_setopt( p_curl, CURLOPT_READDATA, fd_tmp->stdio_stream ); /*** URGENT: this is for FCGI compatibility, normally is would be fd_tmp only !!! ***/		
		#endif
		curl_easy_setopt( p_curl, CURLOPT_ERRORBUFFER, curl_err );
		curl_easy_setopt( p_curl, CURLOPT_UPLOAD, 1 );
		curl_easy_setopt( p_curl, CURLOPT_USERAGENT, FTP_USERAGENT );
		curl_easy_setopt( p_curl, CURLOPT_FTP_FILEMETHOD,	CURLFTPMETHOD_NOCWD );

		/* build URL */
		{
			char *s_filename_basename = strrchr( s_filename, '/' ) + 1;//basename( s_filename ); //use the gnu version

			if( s_username )
			{
				assert( s_password );

				if( s_path )
				{
					snprintf( buffer, sizeof(buffer), "ftp://%.64s:%.64s@%.128s/%s/%s", s_username, s_password, s_hostname, s_path, s_filename_basename );
				}
				else
				{
					snprintf( buffer, sizeof(buffer), "ftp://%.64s:%.64s@%.128s/%s", s_username, s_password, s_hostname, s_filename_basename );
				}
			}
			else
			{
				if( s_path )
				{
					snprintf( buffer, sizeof(buffer), "ftp://%.128s/%s/%s", s_hostname, s_path, s_filename_basename );
				}
				else
				{
					snprintf( buffer, sizeof(buffer), "ftp://%.128s/%s", s_hostname, s_filename_basename );
				}
			}

			curl_easy_setopt( p_curl, CURLOPT_URL, buffer /* URL */ );
		}

		/* assemble headers and add headers to curl request */
		{
			/* Add CWD */
			{
				headerlist = curl_slist_append( headerlist, "CWD /" );
			}

			curl_easy_setopt( p_curl, CURLOPT_PREQUOTE, headerlist );
		}
								
		/* perform request */				
		res = curl_easy_perform( p_curl );

		/* cleanup */
		fclose( fd_tmp );
		curl_slist_free_all( headerlist );
		#ifdef _DEBUG
		headerlist = NULL;
		#endif

		if( res != 0 )
		{
			b_result = FALSE;
			fprintf( stderr, "%s:%d: Error performing curl request (host = %.128s res = %d, err = %.1024s).\n", __FUNCTION__, __LINE__, (s_hostname ? s_hostname : "<null>"), res, curl_err );			
		}	
	}

	return b_result;
}

boolean ftp_delete( CURL *p_curl, const char *s_hostname, const char *s_username, const char *s_password, const char *s_filepath )
{
	char curl_err[ CURL_ERROR_SIZE ];
	char buffer[ 1024 ];
	struct curl_slist *headerlist = NULL;
	CURLcode res                  = 0;
	FILE *fd_tmp                  = NULL;
	boolean b_result              = TRUE;

	if( b_result /*ec == 0*/ )
	{
		/* open dummy file */
		fd_tmp = fopen("/dev/null", "wb");		/* do dummy download/list into devnull */

		#ifdef _NO_FILE_STDIO_STREAM
			curl_easy_setopt( p_curl, CURLOPT_WRITEDATA, fd_tmp ); /* I had no stdio_stream in my FILE structure */
		#else
			curl_easy_setopt( p_curl, CURLOPT_WRITEDATA, fd_tmp->stdio_stream);		/*** URGENT: this is for FCGI compatibility, normally is would be fd_tmp only !!! ***/						
		#endif

		curl_easy_setopt( p_curl, CURLOPT_ERRORBUFFER, curl_err);								
		curl_easy_setopt( p_curl, CURLOPT_USERAGENT, FTP_USERAGENT );
		curl_easy_setopt( p_curl, CURLOPT_UPLOAD, 0 );
		curl_easy_setopt( p_curl, CURLOPT_FTP_FILEMETHOD, CURLFTPMETHOD_NOCWD );

		/* build URL */
		{
			if( s_username )
			{
				assert( s_password );
				snprintf( buffer, sizeof(buffer), "ftp://%.64s:%.64s@%.128s/", s_username, s_password, s_hostname );
			}
			else
			{
				snprintf( buffer, sizeof(buffer), "ftp://%.128s/", s_hostname );
			}

			curl_easy_setopt( p_curl, CURLOPT_URL, buffer /* URL */ );
		}

		/* assemble headers and add headers to curl request */
		{
			/* build and add delete header */
			{
				if( *s_filepath == '/' )
				{
					snprintf( buffer,	sizeof(buffer), "DELE /%s", s_filepath );
				}
				else
				{
					snprintf( buffer,	sizeof(buffer), "DELE %s", s_filepath );
				}

				headerlist = curl_slist_append( headerlist, buffer /* delete header */ );
			}

			curl_easy_setopt( p_curl, CURLOPT_POSTQUOTE, headerlist );
		}
							
		/* perform request */				
		res = curl_easy_perform( p_curl );

		/* cleanup */
		curl_slist_free_all( headerlist );
		#ifdef _DEBUG
		headerlist = NULL;
		#endif

		if( res != 0 && res != 21 ) /* wir ignorieren hier fehler 21, weil wenn file nicht existert, is das wurst. */
		{
			b_result = FALSE;
			fprintf( stderr, "%s:%d: Error performing curl request (host = %.128s res = %d, err = %.1024s).\n", __FUNCTION__, __LINE__, (s_hostname ? s_hostname : "<null>"), res, curl_err );			
		}
	}

	fclose( fd_tmp );
	return TRUE;
}
