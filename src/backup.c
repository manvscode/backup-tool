#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <unistd.h>
#include <getopt.h>
#include <curl/curl.h>
#include <glib.h>
#include "s3.h"
#include "backup.h"
#include "types.h"
#include "mime.h"
#include "types.h"

#define BACKUP_CONFIGURATION_FILE         "/etc/backup_tool.conf"
#define BACKUP_S3_GROUP_NAME              "S3"

static struct option long_options[] = {
	{ "help",    no_argument,       NULL, 'h' }, // 0
	{ "verbose", no_argument,       NULL, 'v' },
	{ "quiet",   no_argument,       NULL, 'q' },
	{ "config",  required_argument, NULL, 'c' }, // 3
	{ "retries", required_argument, NULL, 'r' }, 
	{ "bucket",  required_argument, NULL, 'b' },
	{ "key",     required_argument, NULL, 'k' }, // 6
	{ "list",    no_argument,       NULL, 'l' }, 
	{ "put",     required_argument, NULL, 'p' },
	{ "delete",  no_argument,       NULL, 'd' }, // 9
	{ NULL, 0, NULL, 0 }
};

static char* option_descriptions[] = {
	"This message that you see now.", // 0
	"To enable the printing of extra messages.",
	"To disable the printing of all messages.",
	"Specify a configuration file.",  // 3
	"The number of times to retry in case of an error.", 
	"The S3 bucket to use.",
	"The S3 key to use.", // 6
	"To list all of the buckets.",
	"To put a file in the S3 bucket.",
	"To delete a file from the S3 Bucket.",  // 9
	NULL
};

struct tag_backup_tool {
	boolean b_verbose;
	boolean b_quiet;
	backup_operation operation;
	char s_s3_access_id[ 64 ];
	char s_s3_secret_key[ 64 ];
	char s_s3_bucket[ S3_MAX_BUCKET_NAME ];
	char s_key[ 512 ];
	char s_filename[ 512 ];
	uint retries;
	CURL* p_curl;
	mime_table mime_table;
	S3 s3;
};

boolean backup_initialize            ( backup_tool *p_tool );
boolean backup_deinitialize          ( backup_tool *p_tool );

#define backup_show_messages( p_tool, messages ) \
	if( !(p_tool)->b_quiet ) { \
		messages \
	}

#define backup_show_messages_if_verbose( p_tool, messages ) \
	if( !(p_tool)->b_quiet && (p_tool)->b_verbose ) {\
		messages \
	}


//////////////////////////////////////////////////////
////////////////// ENTRY POINT ///////////////////////
//////////////////////////////////////////////////////
int main( int argc, char *argv[] )
{
	int option_index  = 0;
	int option        = 0;
	backup_tool* p_bt = NULL;
	char configuration_file[ 255 ];

	/* Copy default value for configuration filename */
	strncpy( configuration_file, BACKUP_CONFIGURATION_FILE, sizeof(configuration_file) );

	p_bt = backup_create( );

	if( !p_bt ) return 1;

	/* get all of the command line options */
	while( (option = getopt_long( argc, argv, "b:k:p:c:r:dlvqh", long_options, &option_index )) >= 0 )
	{
		switch( option )
		{
			case 0: /* long option */
				break;
			case 'b': /* S3 bucket */
				backup_set_s3_bucket( p_bt, optarg );
				break;
			case 'k': /* S3 key */
				backup_set_s3_key( p_bt, optarg );
				break;
			case 'p': /* S3 put */
				backup_set_op( p_bt, OP_S3_PUT );
				backup_set_file( p_bt, optarg );
				break;
			case 'c': /* configuration file */
				strncpy( configuration_file, optarg, sizeof(configuration_file) );
				break;
			case 'r':
				backup_set_retries( p_bt, atoi(optarg) );
				break;
			case 'd': /* S3 delete */
				backup_set_op( p_bt, OP_S3_DELETE );
				break;
			case 'l': /* S3 list */
				backup_set_op( p_bt, OP_S3_LIST );
				break;
			case 'v': /* Verbose */
				backup_set_verbose( p_bt, TRUE );
				break;
			case 'q': /* Quiet */
				backup_set_quiet( p_bt, TRUE );
				break;
			case 'h': /* Help */
				backup_deinitialize( p_bt );
				return backup_help( argv[ 0 ] );
				break;
			default:
				break;
		}
	}
	
	backup_show_messages_if_verbose( p_bt, 
		printf( "Using %s...\n", configuration_file );
	);
	boolean b_result = FALSE;

	/* Read in configuration from file */
	if( backup_read_configuration( p_bt, configuration_file ) )
	{
		switch( bt.operation )
		{
			case OP_S3_PUT:
				b_result = backup_s3_put_file( p_bt, &s3 );
				break;
			case OP_S3_DELETE:
				b_result = backup_s3_delete_file( p_bt, &s3 );
				break;
			case OP_S3_LIST:
				b_result = backup_s3_list_buckets( p_bt, &s3 );
				break;
			case OP_NOTHING:
			default:
				break;
		}
	}

	backup_destroy( &p_bt );

	return b_result ? 0 : 2;
}

backup_tool* backup_create( void )
{
	backup_tool* p_bt = (backup_tool*) malloc( sizeof(backup_tool) );

	if( p_bt )
	{
		if( !backup_initialize( p_bt ) )
		{
			free( p_bt );
			p_bt = NULL;
		}
	}

	return p_bt;
}

boolean backup_destroy( backup_tool** p_tool )
{
	boolean result = FALSE;

	if( p_tool && *p_tool )
	{
		backup_deinitialize( *p_tool );
		p_tool = NULL;
		result = TRUE;
	}

	return result;
}

boolean backup_initialize( backup_tool *p_tool )
{
	assert( p_tool );
	curl_global_init( CURL_GLOBAL_ALL );

	p_tool->b_verbose        = FALSE;
	p_tool->b_quiet          = FALSE;
	p_tool->operation        = OP_NOTHING;
	p_tool->s_s3_bucket[ 0 ] = '\0';
	p_tool->s_key[ 0 ]       = '\0';
	p_tool->retries          = 1;
	p_tool->p_curl           = curl_easy_init( );
	
	if( !p_tool->p_curl )
	{
		backup_show_messages( p_tool,
			fprintf( stderr, "Unable to create cURL handle.\n" );
		);
		return FALSE;
	}				

	/* Set common options */
	#ifdef _CURL_VERBOSE					
		curl_easy_setopt( p_tool->p_curl, CURLOPT_VERBOSE, 1 );						
	#endif						

	/* this allocates memory */
	mime_create( &p_tool->mime_table );
		
	s3_initialize( &s3, bt.s_s3_access_id, bt.s_s3_secret_key, bt.b_verbose );

	return TRUE;
}

boolean backup_deinitialize( backup_tool *p_tool )
{
	assert( p_tool );
	curl_easy_cleanup( p_tool->p_curl );
	curl_global_cleanup( );

	#ifdef _DEBUG					
	p_tool->b_verbose        = FALSE;
	p_tool->b_quiet          = FALSE;
	p_tool->operation        = OP_NOTHING;
	p_tool->s_s3_bucket[ 0 ] = '\0';
	p_tool->s_key[ 0 ]       = '\0';
	p_tool->p_curl           = NULL;
	#endif						

	mime_destroy( &p_tool->mime_table );
		
	s3_deinitialize( );	
	
	return TRUE;
}

boolean backup_read_configuration( backup_tool *p_tool, const char *configuration_file )
{
	boolean b_result = TRUE;
	GKeyFile *p_configuration_file = g_key_file_new( );

	b_result = g_key_file_load_from_file( p_configuration_file, configuration_file, G_KEY_FILE_NONE, NULL );
	
	if( !b_result )
	{
		backup_show_messages( p_tool,
			fprintf( stderr, "Unable to load configuration file (%s).\n", configuration_file );
		);
		//b_result = FALSE;
	}

	if( b_result )
	{
		gchar *aws_access_id  = g_key_file_get_value( p_configuration_file, BACKUP_S3_GROUP_NAME, "AccessId", NULL );
		gchar *aws_secret_key = g_key_file_get_value( p_configuration_file, BACKUP_S3_GROUP_NAME, "SecretKey", NULL );

		if( !aws_access_id || *aws_access_id == '\0' )
		{
			backup_show_messages( p_tool,
				fprintf( stderr, "AWS access ID is required in configuration file (%s).\n", configuration_file );
			);
			b_result = FALSE;
		}
		else if( !aws_secret_key || *aws_secret_key == '\0' )
		{
			backup_show_messages( p_tool,
				fprintf( stderr, "AWS secret key is required in configuration file (%s).\n", configuration_file );
			);
			b_result = FALSE;
		}

		if( b_result )
		{
			strncpy( p_tool->s_s3_access_id, aws_access_id, sizeof(p_tool->s_s3_access_id) );
			strncpy( p_tool->s_s3_secret_key, aws_secret_key, sizeof(p_tool->s_s3_secret_key) );
			p_tool->s_s3_access_id[ sizeof(p_tool->s_s3_access_id) - 1 ]   = '\0';
			p_tool->s_s3_secret_key[ sizeof(p_tool->s_s3_secret_key) - 1 ] = '\0';
		}

		g_free( aws_access_id );
		g_free( aws_secret_key );
	}

	g_key_file_free( p_configuration_file );
	
	return b_result;
}

void backup_set_s3_bucket( backup_tool *p_tool, const char *bucket )
{
	assert( p_tool );
	assert( bucket );
	strncpy( p_tool->s_s3_bucket, bucket, sizeof(p_tool->s_s3_bucket) );	
	p_tool->s_s3_bucket[ S3_MAX_BUCKET_NAME - 1 ] = '\0';
}

void backup_set_s3_key( backup_tool *p_tool, const char *key )
{
	assert( p_tool );
	assert( key && *key );
	strncpy( p_tool->s_key, key, sizeof(p_tool->s_key) );	
	p_tool->s_key[ sizeof(p_tool->s_key) - 1 ] = '\0';
}

void backup_set_file( backup_tool *p_tool, const char *filename )
{
	assert( p_tool );
	assert( filename && *filename );
	strncpy( p_tool->s_filename, filename, sizeof(p_tool->s_filename) );	
	p_tool->s_filename[ sizeof(p_tool->s_filename) - 1 ] = '\0';
}

void backup_set_op( backup_tool *p_tool, backup_operation op )
{
	assert( p_tool );
	p_tool->operation = op;	
}

void backup_set_verbose( backup_tool *p_tool, boolean verbose )
{
	assert( p_tool );
	p_tool->b_verbose = verbose;
}

void backup_set_quiet( backup_tool *p_tool, boolean quiet )
{
	assert( p_tool );

	if( quiet )
	{
		p_tool->b_verbose = FALSE;
	}

	p_tool->b_quiet = quiet;
}

void backup_set_retries( backup_tool *p_tool, uint retries )
{
	assert( p_tool );
	assert( retries >= 0 );

	p_tool->retries = retries;
}

int backup_help( const char *program )
{
	int i;

	printf( "S3 Backup Tool. Copyright 2009, FAP Services LLC.\n" );
	printf( "Usage: %s [OPTION]... FILE\n\n", program );

	printf( "Possible options include:\n" );
	for( i = 0; long_options[ i ].name != NULL; i++ )
	{
		printf( "  -%c, --%-10s %s\n", (char) long_options[ i ].val, long_options[ i ].name, option_descriptions[ i ] );
	}

	return 1;
}

boolean backup_s3_put_file( backup_tool *p_tool )
{
	boolean b_result        = FALSE;
	const char *p_dot       = strrchr( p_tool->s_filename, '.' );
	const char *extension   = p_dot ? p_dot + 1 : "txt";
	const char *s_mime_type = mime_type( &p_tool->mime_table, extension );
	uint retry_attempts     = p_tool->retries + 1;

	assert( extension );
	assert( s_mime_type );
	assert( retry_attempts > 0 );

	while( !b_result && retry_attempts > 0 )
	{
		backup_show_messages( p_tool,
			printf( "Uploading: %-12.12s   %40.40s --> ", s_mime_type, p_tool->s_filename );
		);

		b_result = s3_put_file( p_tool->p_curl, &p_tool->s3, p_tool->s_s3_bucket, p_tool->s_key, p_tool->s_filename, s_mime_type );

		backup_show_messages( p_tool,
			if( retry_attempts > 1 )
			{
				printf( "%s\n", b_result ? "SUCCESS" : "FAILED (but will retry)" );
			}
			else
			{
				printf( "%s\n", b_result ? "SUCCESS" : "FAILED" );
			}
		);

		retry_attempts--;
	}

	return b_result;
}

boolean backup_s3_delete_file( backup_tool *p_tool )
{
	boolean b_result    = FALSE;
	uint retry_attempts = p_tool->retries + 1;

	while( !b_result && retry_attempts > 0 )
	{
		backup_show_messages( p_tool,
			char s_bucket_and_key[ 52 ];
			snprintf( s_bucket_and_key, sizeof(s_bucket_and_key), "%s/%s", p_tool->s_s3_bucket, p_tool->s_key );
			printf( " Deleting:    %52.52s --> ", s_bucket_and_key );
		);

		b_result = s3_delete_file( p_tool->p_curl, &p_tool->s3, p_tool->s_s3_bucket, p_tool->s_key );

		backup_show_messages( p_tool,
			if( retry_attempts > 1 )
			{
				printf( "%s\n", b_result ? "SUCCESS" : "FAILED (but will retry)" );
			}
			else
			{
				printf( "%s\n", b_result ? "SUCCESS" : "FAILED" );
			}
		);
		
		retry_attempts--;
	}

	return b_result;
}

boolean backup_s3_list_buckets( backup_tool *p_tool )
{
	boolean b_result = FALSE;
	b_result = s3_list_buckets( p_tool->p_curl, &p_tool->s3 );

	return b_result;
}
