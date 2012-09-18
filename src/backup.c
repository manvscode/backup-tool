#include <stdio.h>
#include <string.h>
#include <assert.h>
#include <unistd.h>
#include <getopt.h>
#include <glib.h>
#include "backup.h"
#include "types.h"

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



//////////////////////////////////////////////////////
////////////////// ENTRY POINT ///////////////////////
//////////////////////////////////////////////////////
int main( int argc, char *argv[] ) {
	// ---- VAR ----
	int option_index = 0;
	int option       = 0;
	BackupTool backup_tool;
	char configuration_file[ 255 ];

	/* Copy default value for configuration filename */
	strncpy( configuration_file, BACKUP_CONFIGURATION_FILE, sizeof(configuration_file) );

	// ---- CODE ----
	backup_initialize( &backup_tool );

	/* get all of the command line options */
	while( (option = getopt_long( argc, argv, "b:k:p:c:r:dlvqh", long_options, &option_index )) >= 0 ) {
		switch( option ) {
			case 0: /* long option */
				break;
			case 'b': /* S3 bucket */
				backup_set_s3_bucket( &backup_tool, optarg );
				break;
			case 'k': /* S3 key */
				backup_set_s3_key( &backup_tool, optarg );
				break;
			case 'p': /* S3 put */
				backup_set_op( &backup_tool, OP_S3_PUT );
				backup_set_file( &backup_tool, optarg );
				break;
			case 'c': /* configuration file */
				strncpy( configuration_file, optarg, sizeof(configuration_file) );
				break;
			case 'r':
				backup_set_retries( &backup_tool, atoi(optarg) );
				break;
			case 'd': /* S3 delete */
				backup_set_op( &backup_tool, OP_S3_DELETE );
				break;
			case 'l': /* S3 list */
				backup_set_op( &backup_tool, OP_S3_LIST );
				break;
			case 'v': /* Verbose */
				backup_set_verbose( &backup_tool, TRUE );
				break;
			case 'q': /* Quiet */
				backup_set_quiet( &backup_tool, TRUE );
				break;
			case 'h': /* Help */
				backup_deinitialize( &backup_tool );
				return backup_help( argv[ 0 ] );
				break;
			default:
				break;
		}
	}
	
	backup_show_messages_if_verbose( &backup_tool, 
		printf( "Using %s...\n", configuration_file );
	);
	boolean b_result = FALSE;

	/* Read in configuration from file */
	if( backup_read_configuration( &backup_tool, configuration_file ) ) {
		S3 s3;
		s3_initialize( &s3, backup_tool.s_s3_access_id, backup_tool.s_s3_secret_key, backup_tool.b_verbose );

		switch( backup_tool.operation ) {
			case OP_S3_PUT:
				b_result = backup_s3_put_file( &backup_tool, &s3 );
				break;
			case OP_S3_DELETE:
				b_result = backup_s3_delete_file( &backup_tool, &s3 );
				break;
			case OP_S3_LIST:
				b_result = backup_s3_list_buckets( &backup_tool, &s3 );
				break;
			case OP_NOTHING:
			default:
				break;
		}

		s3_deinitialize( );	
	}

	backup_deinitialize( &backup_tool );

	return b_result ? 0 : 2;
}

boolean backup_initialize( BackupTool *p_tool ) {
	// ---- CODE ----
	assert( p_tool );
	curl_global_init( CURL_GLOBAL_ALL );

	p_tool->b_verbose        = FALSE;
	p_tool->b_quiet          = FALSE;
	p_tool->operation        = OP_NOTHING;
	p_tool->s_s3_bucket[ 0 ] = '\0';
	p_tool->s_key[ 0 ]       = '\0';
	p_tool->retries          = 1;
	p_tool->p_curl           = curl_easy_init( );
	
	if( !p_tool->p_curl ) {
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

	return TRUE;
}

boolean backup_deinitialize( BackupTool *p_tool ) {
	// ---- CODE ----
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
	
	return TRUE;
}

boolean backup_read_configuration( BackupTool *p_tool, const char *configuration_file ) {
	// ---- VAR ----
	boolean b_result = TRUE;
	GKeyFile *p_configuration_file = g_key_file_new( );

	// ---- CODE ----
	b_result = g_key_file_load_from_file( p_configuration_file, configuration_file, G_KEY_FILE_NONE, NULL );
	
	if( !b_result ) {
		backup_show_messages( p_tool,
			fprintf( stderr, "Unable to load configuration file (%s).\n", configuration_file );
		);
		//b_result = FALSE;
	}

	if( b_result ) {
		gchar *aws_access_id  = g_key_file_get_value( p_configuration_file, BACKUP_S3_GROUP_NAME, "AccessId", NULL );
		gchar *aws_secret_key = g_key_file_get_value( p_configuration_file, BACKUP_S3_GROUP_NAME, "SecretKey", NULL );

		if( !aws_access_id || *aws_access_id == '\0' ) {
			backup_show_messages( p_tool,
				fprintf( stderr, "AWS access ID is required in configuration file (%s).\n", configuration_file );
			);
			b_result = FALSE;
		}
		else if( !aws_secret_key || *aws_secret_key == '\0' ) {
			backup_show_messages( p_tool,
				fprintf( stderr, "AWS secret key is required in configuration file (%s).\n", configuration_file );
			);
			b_result = FALSE;
		}

		if( b_result ) {
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

void backup_set_s3_bucket( BackupTool *p_tool, const char *bucket ) {
	// ---- CODE ----
	assert( p_tool );
	assert( bucket );
	strncpy( p_tool->s_s3_bucket, bucket, sizeof(p_tool->s_s3_bucket) );	
	p_tool->s_s3_bucket[ S3_MAX_BUCKET_NAME - 1 ] = '\0';
}

void backup_set_s3_key( BackupTool *p_tool, const char *key ) {
	// ---- CODE ----
	assert( p_tool );
	assert( key && *key );
	strncpy( p_tool->s_key, key, sizeof(p_tool->s_key) );	
	p_tool->s_key[ sizeof(p_tool->s_key) - 1 ] = '\0';
}

void backup_set_file( BackupTool *p_tool, const char *filename ) {
	// ---- CODE ----
	assert( p_tool );
	assert( filename && *filename );
	strncpy( p_tool->s_filename, filename, sizeof(p_tool->s_filename) );	
	p_tool->s_filename[ sizeof(p_tool->s_filename) - 1 ] = '\0';
}

void backup_set_verbose( BackupTool *p_tool, boolean verbose ) {
	// ---- CODE ----
	assert( p_tool );
	p_tool->b_verbose = verbose;
}

void backup_set_quiet( BackupTool *p_tool, boolean quiet ) {
	// ---- CODE ----
	assert( p_tool );

	if( quiet ) {
		p_tool->b_verbose = FALSE;
	}

	p_tool->b_quiet = quiet;
}

void backup_set_retries( BackupTool *p_tool, uint retries ) {
	assert( p_tool );
	assert( retries >= 0 );

	p_tool->retries = retries;
}

int backup_help( const char *program ) {
	// ---- VAR ----
	int i;
	// ---- CODE ----
	printf( "S3 Backup Tool. Copyright 2009, FAP Services LLC.\n" );
	printf( "Usage: %s [OPTION]... FILE\n\n", program );

	printf( "Possible options include:\n" );
	for( i = 0; long_options[ i ].name != NULL; i++ ) {
		printf( "  -%c, --%-10s %s\n", (char) long_options[ i ].val, long_options[ i ].name, option_descriptions[ i ] );
	}

	return 1;
}

boolean backup_s3_put_file( BackupTool *p_tool, const S3 *p_s3 ) {
	// ---- VAR ----
	boolean b_result        = FALSE;
	const char *extension   = strrchr( p_tool->s_filename, '.' ) + 1;
	const char *s_mime_type = mime_type( &p_tool->mime_table, extension );
	uint retry_attempts     = p_tool->retries + 1;

	assert( extension );
	assert( s_mime_type );
	assert( retry_attempts > 0 );

	// ---- CODE ----
	while( !b_result && retry_attempts > 0 ) {
		backup_show_messages( p_tool,
			printf( "Uploading: %-12.12s   %40.40s --> ", s_mime_type, p_tool->s_filename );
		);

		b_result = s3_put_file( p_tool->p_curl, p_s3, p_tool->s_s3_bucket, p_tool->s_key, p_tool->s_filename, s_mime_type );

		backup_show_messages( p_tool,
			if( retry_attempts > 1 ) {
				printf( "%s\n", b_result ? "SUCCESS" : "FAILED (but will retry)" );
			}
			else {
				printf( "%s\n", b_result ? "SUCCESS" : "FAILED" );
			}
		);

		retry_attempts--;
	}

	return b_result;
}

boolean backup_s3_delete_file( BackupTool *p_tool, const S3 *p_s3 ) {
	// ---- VAR ----
	boolean b_result    = FALSE;
	uint retry_attempts = p_tool->retries + 1;

	// ---- CODE ----
	while( !b_result && retry_attempts > 0 ) {
		backup_show_messages( p_tool,
			char s_bucket_and_key[ 52 ];
			snprintf( s_bucket_and_key, sizeof(s_bucket_and_key), "%s/%s", p_tool->s_s3_bucket, p_tool->s_key );
			printf( " Deleting:    %52.52s --> ", s_bucket_and_key );
		);

		b_result = s3_delete_file( p_tool->p_curl, p_s3, p_tool->s_s3_bucket, p_tool->s_key );

		backup_show_messages( p_tool,
			if( retry_attempts > 1 ) {
				printf( "%s\n", b_result ? "SUCCESS" : "FAILED (but will retry)" );
			}
			else {
				printf( "%s\n", b_result ? "SUCCESS" : "FAILED" );
			}
		);
		
		retry_attempts--;
	}

	return b_result;
}

boolean backup_s3_list_buckets( BackupTool *p_tool, const S3 *p_s3 ) {
	// ---- VAR ----
	boolean b_result = FALSE;

	// ---- CODE ----

	b_result = s3_list_buckets( p_tool->p_curl, p_s3 );

	return b_result;
}
