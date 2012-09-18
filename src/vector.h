#ifndef _VECTOR_H_
#define _VECTOR_H_

#include <stddef.h>

#define INITIAL_ARRAY_SIZE    250

typedef int (*vector_element_function)( void *element );


typedef struct struct_vector {
	size_t element_size;
	void *array;
	size_t array_size;
	size_t size;
	vector_element_function element_destroy_callback;
} vector;

int   vector_create     ( vector *p_vector, size_t element_size, vector_element_function element_destroy_callback );
void  vector_destroy    ( vector *p_vector );
int   vector_push       ( vector *p_vector, void *element );
int   vector_pop        ( vector *p_vector );
void* vector_element_at ( vector *p_vector, size_t index );

#define vector_element_size( p_vector )      ((p_vector)->element_size)
#define vector_array(p_vector)               ((p_vector)->array)
#define vector_array_size( p_vector )        ((p_vector)->array_size)
#define vector_size( p_vector )              ((p_vector)->size)
#define vector_is_empty( p_vector )          ((p_vector)->size <= 0)
#define vector_element_at( p_vector, index ) (vector_array(p_vector) + vector_element_size(p_vector) * (index))

#endif /* _VECTOR_H_ */
