#ifndef _BASE64_H_
#define _BASE64_H_

#include "types.h"
/* Requires OpenSSL (-lssl) */



/* caller is responsible for freeing the memory. */
byte *base64(const byte *input, size_t length);
/* caller is responsible for freeing the memory. */
byte *unbase64(byte *input, size_t length);

#ifdef _TEST_BASE64
void test_base64( const char *text, size_t length );
#endif

#endif // _BASE64_H_
