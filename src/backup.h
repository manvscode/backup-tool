#ifndef _BACKUP_H_
#define _BACKUP_H_

#include <curl/curl.h>
#include "types.h"
#include "s3.h"
#include "mime.h"


#define BACKUP_CONFIGURATION_FILE         "/etc/backup_tool.conf"
#define BACKUP_S3_GROUP_NAME              "S3"

typedef enum tagBackupOperation {
	OP_NOTHING = 0,
	OP_S3_PUT,
	OP_S3_DELETE,
	OP_S3_LIST,
} BackupOperation;

typedef struct tagBackupTool {
	boolean b_verbose;
	boolean b_quiet;
	BackupOperation operation;
	char s_s3_access_id[ 64 ];
	char s_s3_secret_key[ 64 ];
	char s_s3_bucket[ S3_MAX_BUCKET_NAME ];
	char s_key[ 512 ];
	char s_filename[ 512 ];
	uint retries;
	CURL *p_curl;
	MimeTable mime_table;
} BackupTool;


#define backup_show_messages( p_tool, messages ) \
	if( !(p_tool)->b_quiet ) { \
		messages \
	}

#define backup_show_messages_if_verbose( p_tool, messages ) \
	if( !(p_tool)->b_quiet && (p_tool)->b_verbose ) {\
		messages \
	}

boolean backup_initialize( BackupTool *p_tool );
boolean backup_deinitialize( BackupTool *p_tool );
boolean backup_read_configuration( BackupTool *p_tool, const char *configuration_file );
void    backup_set_s3_bucket( BackupTool *p_tool, const char *bucket );
void    backup_set_s3_key( BackupTool *p_tool, const char *key );
void    backup_set_file( BackupTool *p_tool, const char *filename );
#define backup_set_op( p_tool, op )  ( (p_tool)->operation = (op) )
void    backup_set_verbose( BackupTool *p_tool, boolean verbose );
void    backup_set_quiet( BackupTool *p_tool, boolean quiet );
void    backup_set_retries( BackupTool *p_tool, uint retries );
int     backup_help( const char *program );
boolean backup_s3_put_file( BackupTool *p_tool, const S3 *p_s3 );
boolean backup_s3_delete_file( BackupTool *p_tool, const S3 *p_s3 );
boolean backup_s3_list_buckets( BackupTool *p_tool, const S3 *p_s3 );



#endif /* _BACKUP_H_ */
