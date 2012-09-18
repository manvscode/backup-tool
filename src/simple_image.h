#ifndef _IMAGE_RESIZER_H_
#define _IMAGE_RESIZER_H_

#include <stdlib.h>
#include <magick/api.h>
#include "types.h"

#define SIMPLE_IMAGE_JPEG          ("JPEG")
#define SIMPLE_IMAGE_JPEG_LENGTH   (4)



boolean     simple_image_initialize( const char *s_client_path, size_t length );
boolean     simple_image_deinitialize( );
boolean     simple_image_load( const char *s_filename, size_t length, /*out*/ Image **p_p_image );
boolean     simple_image_write( const char *s_filename, size_t length, Image *p_image );
void        simple_image_destroy( Image *p_image );
#define     simple_image_format( p_image_data ) ((const char*)(p_image_data)->magick)
const char* simple_image_format_description( const Image *data );
boolean     simple_image_resize( const Image *p_image, uint width, uint height, /*out*/ Image **p_p_new_image );
boolean     simple_image_rotate( const Image *p_image, double angle_in_degrees, /*out*/ Image **p_p_new_image );
boolean     simple_image_crop( const Image *p_image, uint x, uint y, uint width, uint height, /*out*/ Image **p_p_new_image );
boolean     simple_image_composite( /*in/out*/ Image **p_p_image, const Image *p_top_image, long x, long y );
boolean     simple_image_annotate( /*in/out*/ Image **p_p_image, const char *text );
boolean     simple_image_is_jpeg( const Image *data );
boolean     simple_image_is_file_jpeg( const char *s_filename, size_t length );
boolean     simple_image_exif_clear( Image *p_image ); /* Only deletes EXIF data */
void        simple_image_print_exif( FILE *p_output, Image *p_image );
boolean     simple_image_strip_image( Image *p_image ); /* removes EXIF and comments */

#endif /* _IMAGE_RESIZER_H_ */
