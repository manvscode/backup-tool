#include <string.h>
#include <assert.h>
#include <unistd.h>
#include <magick/api.h>
#include "simple_image.h"

#define MAX_PATH 255


boolean simple_image_initialize( const char *s_client_path, size_t length ) {
	// ---- VAR ----
	char s_cwd[ MAX_PATH ];

	// ---- CODE ----
	
	if( !s_client_path ) {
		if( getcwd( s_cwd, sizeof(s_cwd) ) < 0 ) {
			fprintf( stderr, "Unable to determine current working directory.\n" );
			return FALSE;
		}
	}
	else { 
		strncpy( s_cwd, s_client_path, length );
		s_cwd[ length ] = '\0';
	}

	if( !IsMagickInstantiated( ) ) {
		MagickCoreGenesis( s_cwd, MagickTrue );
	}

	return TRUE;
}

boolean simple_image_deinitialize( ) {
	// ---- CODE ----
	if( IsMagickInstantiated( ) ) {
		/* Destroy the Image Magick environment */
		MagickCoreTerminus( );
	}

	return TRUE;
}

boolean simple_image_load( const char *s_filename, size_t length, Image **p_p_image ) {
	// ---- VAR ----
	boolean b_result = TRUE;
	ImageInfo *p_info = NULL;
	ExceptionInfo *p_exception = AcquireExceptionInfo( );

	// ---- CODE ----
    p_info = CloneImageInfo( (ImageInfo *) NULL );
    strncpy( p_info->filename, s_filename, length );
	p_info->filename[ length ] = '\0';
    *p_p_image = ReadImage( p_info, p_exception );

    if( p_exception->severity != UndefinedException ) {
		CatchException(p_exception);
		DestroyImageInfo( p_info );
		b_result = FALSE;
	}

	/* make sure everything is kosher */
	assert( b_result && p_info );
	assert( b_result && (*p_p_image) );
	assert( b_result && (*p_p_image)->signature == MagickSignature );

	DestroyImageInfo( p_info );
	DestroyExceptionInfo( p_exception );

	return b_result;
}

boolean simple_image_write( const char *s_filename, size_t length, Image *p_image ) {
	// ---- VAR ----
	boolean b_result = TRUE;
	ImageInfo *p_info = NULL;
	ExceptionInfo *p_exception = AcquireExceptionInfo( );

	// ---- CODE ----
    p_info = CloneImageInfo( (ImageInfo *) NULL );


    strncpy( p_image->filename, s_filename, length );
	p_image->filename[ length ] = '\0';

	//p_info->quality = 90;
	p_image->quality = 90;

	if( simple_image_is_jpeg( p_image ) ) {	
		//p_info->compression = JPEG2000Compression;
		p_image->compression = JPEG2000Compression;
	}

	WriteImage( p_info, p_image );

    if( p_exception->severity != UndefinedException ) {
		CatchException( p_exception );
		b_result = FALSE;
	}

	DestroyImageInfo( p_info );
	DestroyExceptionInfo( p_exception );
	return b_result;
}

void simple_image_destroy( Image *p_image ) {
	// ---- CODE ----
	DestroyImage( p_image );
}

const char* simple_image_format_description( const Image *data ) {
	// ---- VAR ----
	ExceptionInfo *p_exception = AcquireExceptionInfo( );
	const MagickExport MagickInfo *magick_info = GetMagickInfo( data->magick, p_exception );
	// ---- CODE ----
	DestroyExceptionInfo( p_exception );
	return GetMagickDescription( magick_info );
}

boolean simple_image_resize( const Image *p_image, uint width, uint height, /*out*/ Image **p_p_new_image ) {
	// ---- VAR ----
	boolean b_result = TRUE;
	ExceptionInfo *p_exception = AcquireExceptionInfo( );

	// ---- CODE ----

	/* This can be expanded later to accept different resize filters */
	*p_p_new_image = ResizeImage( p_image, width, height, LanczosFilter, 1.0, p_exception );

    if( p_exception->severity != UndefinedException ) {
		CatchException( p_exception );
		b_result = FALSE;
	}

	b_result &= *p_p_new_image != NULL;

	DestroyExceptionInfo( p_exception );
	return b_result;
}

boolean simple_image_rotate( const Image *p_image, double angle_in_degrees, /*out*/ Image **p_p_new_image ) {
	// ---- VAR ----
	boolean b_result = TRUE;
	ExceptionInfo *p_exception = AcquireExceptionInfo( );

	// ---- CODE ----
	
	*p_p_new_image = RotateImage( p_image, angle_in_degrees, p_exception );

    if( p_exception->severity != UndefinedException ) {
		CatchException( p_exception );
		b_result = FALSE;
	}

	b_result &= *p_p_new_image != NULL;

	DestroyExceptionInfo( p_exception );
	return b_result;
}

boolean simple_image_crop( const Image *p_image, uint x, uint y, uint width, uint height, /*out*/ Image **p_p_new_image ) {
	// ---- VAR ----
	boolean b_result = TRUE;
	ExceptionInfo *p_exception = AcquireExceptionInfo( );
	RectangleInfo geometry;

	// ---- CODE ----
	geometry.x      = x;
	geometry.y      = y;
	geometry.width  = width;
	geometry.height = height;
	
	*p_p_new_image = CropImage( p_image, &geometry, p_exception );

    if( p_exception->severity != UndefinedException ) {
		CatchException( p_exception );
		b_result = FALSE;
	}

	b_result &= *p_p_new_image != NULL;

	DestroyExceptionInfo( p_exception );
	return b_result;
}

boolean simple_image_composite( Image **p_p_image, const Image *p_top_image, long x, long y ) {
	// ---- CODE ----
	return CompositeImage( *p_p_image, OverCompositeOp, p_top_image, x, y ) == MagickTrue;
}

boolean simple_image_annotate( Image **p_p_image, const char *text ) {
	// ---- VAR ----
    ImageInfo *p_info;
	DrawInfo *p_draw_info;
	const char *geometry            = "+3+3"; /* push off to (3,3) from bottom left corner */
	const double font_size          = 10.0;
	const double font_kerning       = 0.83;
	const unsigned long font_weight = 10;

	// ---- CODE ----
	p_info = CloneImageInfo( (ImageInfo *) NULL );

	PixelPacket foregroundColor;
	foregroundColor.red     = MaxRGB * (((float)0x15) / 0xff);
	foregroundColor.green   = MaxRGB * (((float)0x15) / 0xff);
	foregroundColor.blue    = MaxRGB * (((float)0x15) / 0xff);
	foregroundColor.opacity = 0;

	PixelPacket backgroundColor;  // #c9b2a6
	backgroundColor.red     = MaxRGB * (((float)0xca) / 0xff);
	backgroundColor.green   = MaxRGB * (((float)0xb4) / 0xff);
	backgroundColor.blue    = MaxRGB * (((float)0xa8) / 0xff);
	backgroundColor.opacity = 0;


	/* write out the background text */
	{
		p_draw_info                   = CloneDrawInfo( p_info, (DrawInfo *) NULL );
		p_draw_info->gravity          = SouthWestGravity; /* bottom-left corner */
		p_draw_info->compose          = OverCompositeOp; 
		p_draw_info->pointsize        = font_size;
		p_draw_info->kerning          = font_kerning;
		p_draw_info->weight           = font_weight;
		p_draw_info->stroke_width     = 4.2;
		p_draw_info->stroke           = backgroundColor;
		p_draw_info->fill             = backgroundColor;
		p_draw_info->stroke_antialias = MagickTrue;
		p_draw_info->text_antialias   = MagickTrue;
		p_draw_info->text             = AcquireString(text);
		p_draw_info->geometry         = AcquireString(geometry);

		strcpy( p_draw_info->geometry, geometry );
		strcpy( p_draw_info->text, text );

		AnnotateImage( *p_p_image, p_draw_info );

		/* clean up */
		DestroyString( p_draw_info->text );
		DestroyString( p_draw_info->geometry );
		p_draw_info->text     = NULL;
		p_draw_info->geometry = NULL;
		DestroyDrawInfo( p_draw_info ); 
	}

	/* write out the foregroundColor text */
	{	
		p_draw_info                   = CloneDrawInfo( p_info, (DrawInfo *) NULL );
		p_draw_info->gravity          = SouthWestGravity; /* bottom-left corner */
		p_draw_info->compose          = OverCompositeOp; 
		p_draw_info->pointsize        = font_size;
		p_draw_info->kerning          = font_kerning;
		p_draw_info->weight           = font_weight;
		p_draw_info->fill             = foregroundColor;
		p_draw_info->text_antialias   = MagickTrue;
		p_draw_info->text             = AcquireString(text);
		p_draw_info->geometry         = AcquireString(geometry);

		strcpy( p_draw_info->geometry, geometry );
		strcpy( p_draw_info->text, text );

		AnnotateImage( *p_p_image, p_draw_info );

		/* clean up */
		DestroyString( p_draw_info->text );
		DestroyString( p_draw_info->geometry );
		p_draw_info->text     = NULL;
		p_draw_info->geometry = NULL;
		DestroyDrawInfo( p_draw_info ); 
	}

	DestroyImageInfo( p_info );
	return TRUE;
}

boolean simple_image_is_jpeg( const Image *data ) {
	// ---- VAR ----
	const char *format = simple_image_format( data );
	// ---- CODE ----
	return strncmp( format, SIMPLE_IMAGE_JPEG, SIMPLE_IMAGE_JPEG_LENGTH ) == 0;
}

boolean simple_image_is_file_jpeg( const char *s_filename, size_t length ) {
	// ---- VAR ----
	boolean b_is_jpeg        = TRUE;
	const char *ext          = strrchr( s_filename, '.' ) + 1;

	// ---- CODE ----
	if( strncasecmp(ext, "jpg", 3) != 0 &&
	    strncasecmp(ext, "jpe", 3) != 0 &&
	    strncasecmp(ext, "jpeg", 4) != 0 ) {
		b_is_jpeg = FALSE;
	}

	if( b_is_jpeg ) {
		ExceptionInfo *p_exception = AcquireExceptionInfo( );
		ImageInfo *p_image_info = NULL;
		Image *p_image          = NULL;

		p_image_info = CloneImageInfo( NULL );
		strncpy( p_image_info->filename, s_filename, length );
		p_image_info->filename[ length ] = '\0';
		p_image = PingImage( p_image_info, p_exception );

		if( p_exception->severity != UndefinedException ) {
			CatchException( p_exception );
			DestroyImageInfo( p_image_info );
			b_is_jpeg = FALSE;
		}

		assert( p_image_info );
		assert( p_image );
		assert( p_image->signature == MagickSignature );

		b_is_jpeg = simple_image_is_jpeg( p_image );
	
		/* clean up */	
		DestroyImageInfo( p_image_info );
		DestroyImage( p_image );
		DestroyExceptionInfo( p_exception );
	}

	return b_is_jpeg;	
}


boolean simple_image_exif_clear( Image *p_image ) {
	assert( p_image );
	assert( p_image->signature == MagickSignature );
	DestroyImageProfiles( p_image );
	return TRUE;
}

/* this will be removed later */
void simple_image_print_exif( FILE *p_output, Image *p_image ) {
	const char *s_key = NULL;
	int i; 

	assert( p_output );
	assert( p_image );
	assert( p_image->signature == MagickSignature );


	ResetImageProfileIterator( p_image );

	/* for each EXIF key ... */
	while( (s_key = (const char*) GetNextImageProfile( p_image )) ) {
		const StringInfo *p_profile  = GetImageProfile( p_image, s_key );
		const char *s_value          = StringInfoToString( p_profile );

		fprintf( p_output, "[" );
		for(i = 0; i < p_profile->length; i++ )
		{
			char c = p_profile->datum[ i ];
			fprintf( p_output, "%c", c == '\0' ? '#' : c );
		}
		fprintf( p_output, "]\n" );

		fprintf( p_output, "%20s: %-32s\n", s_key, s_value );
		DestroyString( (char*)s_value );
	}
}

boolean simple_image_strip_image( Image *p_image ) {
	/* removes EXIF and comments */
	return StripImage((p_image)) == MagickTrue;
}
