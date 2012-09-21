#ifndef _BACKUP_H_
#define _BACKUP_H_

#include <curl/curl.h>
#include "s3.h"


typedef enum {
	OP_NOTHING = 0,
	OP_S3_PUT,
	OP_S3_DELETE,
	OP_S3_LIST,
} backup_operation;

struct tag_backup_tool;
typedef struct tag_backup_tool backup_tool;


backup_tool* backup_create             ( void );
boolean      backup_destroy            ( backup_tool** p_tool );
boolean      backup_read_configuration ( backup_tool *p_tool, const char *configuration_file );
void         backup_set_s3_bucket      ( backup_tool *p_tool, const char *bucket );
void         backup_set_s3_key         ( backup_tool *p_tool, const char *key );
void         backup_set_file           ( backup_tool *p_tool, const char *filename );
void         backup_set_op             ( backup_tool *p_tool, backup_operation op );
void         backup_set_retries        ( backup_tool *p_tool, uint retries );
int          backup_help               ( const char *program );
boolean      backup_s3_put_file        ( backup_tool *p_tool, const S3 *p_s3 );
boolean      backup_s3_delete_file     ( backup_tool *p_tool, const S3 *p_s3 );
boolean      backup_s3_list_buckets    ( backup_tool *p_tool, const S3 *p_s3 );



#endif /* _BACKUP_H_ */
