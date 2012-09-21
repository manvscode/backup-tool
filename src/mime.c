#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <assert.h>
#include "mime.h"


typedef struct tagMimeRecord {
	char *mime_type;
	char *extension;
} MimeRecord;

int mime_record_compare( const void *a, const void *b );

/*
 * String helpers
 */
static char *strtrim_left( char *string );
static char *strtrim_right( char *string );


int mime_record_destroy( void *element );



boolean mime_create( mime_table *p_table )
{
	assert( p_table );
	return mime_create_from_file( p_table, "/etc/mime.types" );
}

boolean mime_create_from_file( mime_table *p_table, const char *s_mime_file )
{
	char buffer[ 128 ];
	FILE *f = NULL;

	assert( p_table );

	vector_create( p_table, sizeof(MimeRecord), mime_record_destroy );

	f = fopen( s_mime_file, "rb" );

	if( !f )
	{
		return FALSE;
	}

	while( !feof(f) )
	{
		fgets( buffer, sizeof(buffer), f );	

		char *trimmed = strtrim_right( strtrim_left( buffer ) );
		if( *trimmed == '#' ) continue;


		int token_count = 0;
		char *mime_type = NULL;	
		char *token     = strtok( buffer, "\t\n\r " );

		while( token )
		{
			if( token_count == 0 )
			{
				mime_type = token;	
			}
			else
			{
				MimeRecord record;

				record.mime_type = strdup( mime_type );
				record.extension = strdup( token );

				vector_push( p_table, &record ); 
			}
		
			token_count += 1;
			token        = strtok( NULL, "\t\n\r " );
		}
	}

	/* sort the table */
	qsort( vector_array(p_table), vector_size(p_table), sizeof(MimeRecord), mime_record_compare );

	return TRUE;	
}

void mime_destroy( mime_table *p_table )
{
	vector_destroy( p_table );
}

int mime_record_destroy( void *element )
{
	assert( element );

	MimeRecord *p_record = (MimeRecord *) element;
	free( p_record->extension );
	free( p_record->mime_type );
	return 1;
}

void mime_debug_table( const mime_table *p_table )
{
	int i;
	assert( p_table );

	for( i = 0; i < vector_size(p_table); i++ )
	{
		MimeRecord *p_record = (MimeRecord *) vector_element_at( p_table, i );
		printf( "%20s --> %s\n", p_record->extension, p_record->mime_type );
	}

	printf( "     ===============================================\n" );
	printf( "                   # of records: %lu\n", vector_size(p_table) );
}

const char *mime_type( const mime_table *p_table, const char *extension )
{
	MimeRecord key;
	MimeRecord *p_record = NULL;

	assert( p_table );
	key.extension = (char *) extension;

	p_record = (MimeRecord *) bsearch( &key, vector_array(p_table), vector_size(p_table), sizeof(MimeRecord), mime_record_compare );

	return p_record ? (const char *) p_record->mime_type : NULL;
}

int mime_record_compare( const void *a, const void *b )
{
	assert( a && b );
	return strcasecmp( ((MimeRecord*) a)->extension, ((MimeRecord*) b)->extension );
}


/*
 * String helpers
 */
char *strtrim_left( char *string )
{
	char *first_char = string;

	while( *first_char )
	{
		if( isspace( *first_char) )
		{
			first_char++;
		}
		else
		{
			break;
		}
	}

	return first_char;
} 

char *strtrim_right( char *string )
{
    int len = strlen( string );
    char *end;

    while ( *string && len)
	{
        end = string + len - 1;

        if( isspace(*end) )
		{
            *end = 0;
		}
        else
		{
            break;
		}

        len = strlen(string);
    }

    return string;
}


