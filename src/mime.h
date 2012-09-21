#ifndef _MIME_H_
#define _MIME_H_

#include "types.h"

#ifdef _DEBUG_MIME
#define _DEBUG_VECTOR
#endif
#include "vector.h"

typedef vector mime_table; /* table of mime records */

boolean     mime_create           ( mime_table *p_table );
boolean     mime_create_from_file ( mime_table *p_table, const char *s_mime_file );
void        mime_destroy          ( mime_table *p_table );
void        mime_debug_table      ( const mime_table *p_table );
const char* mime_type             ( const mime_table *p_table, const char *extension );



#endif
