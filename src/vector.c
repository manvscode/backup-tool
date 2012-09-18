#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include "vector.h"

int vector_create( vector *p_vector, size_t element_size, vector_element_function element_destroy_callback )
{
	assert( p_vector );

	p_vector->element_size             = element_size;
	p_vector->array_size               = INITIAL_ARRAY_SIZE;
	p_vector->size                     = 0;
	p_vector->array                    = malloc( vector_element_size(p_vector) * INITIAL_ARRAY_SIZE );
	p_vector->element_destroy_callback = element_destroy_callback;

	#ifdef _DEBUG_VECTOR
	memset( p_vector->array, 0, vector_element_size(p_vector) * vector_array_size(p_vector) );
	#endif

	assert( p_vector->array );

	return p_vector->array != NULL;
}

void vector_destroy( vector *p_vector )
{
	assert( p_vector );

	while( !vector_is_empty(p_vector) )
	{
		vector_pop( p_vector );
	}

	free( p_vector->array );

	#ifdef _DEBUG_VECTOR
	p_vector->element_size            = 0;
	p_vector->array                   = NULL;
	p_vector->array_size              = 0;
	p_vector->size                    = 0;
	p_vector-element_destroy_callback = NULL;
	#endif
}

int vector_push( vector *p_vector, void *element )
{
	assert( p_vector );

	/* grow the array if needed */
	if( vector_size(p_vector) >= vector_array_size(p_vector) )
	{
		size_t new_size      = 2 * p_vector->array_size + 1;
		p_vector->array_size = new_size;
		p_vector->array      = realloc( p_vector->array, p_vector->element_size * vector_array_size(p_vector) );
		assert( p_vector->array );
	}

	#ifdef _DEBUG_VECTOR
	memset( vector_array(p_vector) + vector_size(p_vector) * vector_element_size(p_vector), 0, vector_element_size(p_vector) );
	#endif

	memcpy( vector_array(p_vector) + (vector_size(p_vector) * vector_element_size(p_vector)), element, vector_element_size(p_vector) );
	p_vector->size++;

	return p_vector->array != NULL;
}

int vector_pop( vector *p_vector )
{
	assert( p_vector );

	void *element = vector_array(p_vector) + vector_size(p_vector) * vector_element_size(p_vector);
	int result    = p_vector->element_destroy_callback( element );

	#ifdef _DEBUG_VECTOR
	memset( vector_array(p_vector) + vector_size(p_vector) * vector_element_size(p_vector), 0, vector_element_size(p_vector) );
	#endif

	p_vector->size--;
	return result;
}
