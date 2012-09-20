#include <string.h>
#include <openssl/sha.h>
#include <openssl/hmac.h>
#include <openssl/evp.h>
#include <openssl/bio.h>
#include <openssl/buffer.h>
#include <assert.h>
#include "base64.h"

/* caller is responsible for freeing the memory. */
byte *base64( const byte *input, size_t length )
{
	BIO *bmem, *b64;
	BUF_MEM *bptr;

	b64 = BIO_new(BIO_f_base64());

	bmem = BIO_new(BIO_s_mem());
	b64 = BIO_push(b64, bmem);
	BIO_write(b64, input, length);
	BIO_flush(b64);
	BIO_get_mem_ptr(b64, &bptr);

	byte *buff = (byte *)malloc(bptr->length);
	memcpy(buff, bptr->data, bptr->length - 1 );
	buff[ bptr->length - 1 ] = 0;

	BIO_free_all(b64);

	return buff;
}


/* caller is responsible for freeing the memory. */
byte *unbase64( byte *input, size_t length )
{
	BIO *b64, *bmem;

	byte *buffer = (byte *)malloc(length);
	memset(buffer, 0, length);

	b64 = BIO_new(BIO_f_base64());
	BIO_set_flags(b64, BIO_FLAGS_BASE64_NO_NL);

	bmem = BIO_new_mem_buf(input, length);
	bmem = BIO_push(b64, bmem);

	BIO_read(bmem, buffer, length);

	BIO_free_all(bmem);

	assert( buffer );
	return buffer;
}

#ifdef _TEST_BASE64
void test_base64( const char *text, size_t length )
{
	byte *output;
	byte *original;

	output = base64( (byte *) text, length );
	printf( " Base64: %s\n", output );
	original = unbase64( output, strlen(output) );
	printf( "Orginal: %s\n", original );
	
	free(output);
	free(original);
}
#endif
