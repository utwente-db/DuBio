/* 
 * This file is part of the Dubio distribution (https://github.com/utwente-db/DuBio).
 * Copyright (c) 2020 Jan Flokstra & Maurice van Keulen
 * 
 * This program is free software: you can redistribute it and/or modify  
 * it under the terms of the GNU General Public License as published by  
 * the Free Software Foundation, version 3.
 *
 * This program is distributed in the hope that it will be useful, but 
 * WITHOUT ANY WARRANTY; without even the implied warranty of 
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU 
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License 
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 */
#ifndef VECTOR_H
#define VECTOR_H

#define VECTOR_MAGIC 1234321

#define VECTOR_ASSERT(V)  if ((V->magic!=VECTOR_MAGIC)||(V->size<0)||(V->size>V->capacity)||(!V->items)) vector_error("VECTOR_ASSERT FAILS")

#define VECTOR_INIT_CAPACITY 4

#define DefVectorH(type) \
typedef struct V_##type { \
    int   magic; \
    int   size, capacity; \
    type *items; \
    type *dynamic; \
    type  fixed[0]; \
} V_##type; \
typedef int (*V_##type##_cmpfun)(type*,type*); \
V_##type *V_##type##_init(V_##type*); \
V_##type *V_##type##_relocate(V_##type*); \
void V_##type##_free(V_##type*); \
void V_##type##_reset(V_##type*); \
int V_##type##_size(V_##type*);\
int V_##type##_bytesize(V_##type*); \
int V_##type##_add(V_##type*, type); \
type V_##type##_get(V_##type*, int); \
type* V_##type##_getp(V_##type*, int); \
void V_##type##_set(V_##type*, int, type); \
void V_##type##_delete(V_##type*, int); \
V_##type *V_##type##_serialize(void*, V_##type*); \
int V_##type##_find(V_##type*, V_##type##_cmpfun, type*); \
int V_##type##_bsearch(V_##type*, V_##type##_cmpfun, int, int, type*); \
void V_##type##_quicksort(V_##type*,int,int,V_##type##_cmpfun); \
void V_##type##_shrink2size(V_##type*); \
int V_##type##_is_serialized(V_##type*); \
V_##type *V_##type##_copy2(V_##type*, V_##type*); \
void V_##type##_resize(V_##type*, int); \

/*
 *
 */

#define DefVectorC(type) \
V_##type *V_##type##_init(V_##type *v) \
{ \
    v->magic   = VECTOR_MAGIC; \
    v->capacity = VECTOR_INIT_CAPACITY; \
    v->size = 0; \
    v->items   = v->dynamic = MALLOC(sizeof(type) * v->capacity); \
    return v; \
} \
\
V_##type *V_##type##_relocate(V_##type *v) \
{ \
    v->magic   = VECTOR_MAGIC; \
    /* size and capacity are already assigned in this case */ \
    v->dynamic = NULL; \
    v->items   = v->fixed; \
    return v; \
} \
\
void V_##type##_reset(V_##type *v) { \
    VECTOR_ASSERT(v); \
    v->size = 0; \
} \
 \
void V_##type##_shrink2size(V_##type *v) { \
    VECTOR_ASSERT(v); \
    v->capacity = v->size; \
    /* do not realloc, could be an option */ \
} \
 \
int V_##type##_bytesize(V_##type *v) { \
    VECTOR_ASSERT(v); \
    return sizeof(V_##type)+v->capacity*sizeof(type); \
} \
 \
int V_##type##_is_serialized(V_##type *v) { \
    VECTOR_ASSERT(v); \
    return !v->dynamic; \
} \
 \
V_##type *V_##type##_serialize(void *void_dst, V_##type *src) { \
    VECTOR_ASSERT(src); \
    V_##type *dst = (V_##type *)void_dst; \
    *dst = *src; \
    memcpy(dst->fixed,src->items,src->size*sizeof(type)); \
    return V_##type##_relocate(dst); \
} \
\
V_##type *V_##type##_copy2(V_##type *dst, V_##type *src) { \
    VECTOR_ASSERT(src); \
    *dst = *src; \
    dst->items   = dst->dynamic = MALLOC(sizeof(type) * src->capacity); \
    memcpy(dst->items,src->items,src->size*sizeof(type)); \
    VECTOR_ASSERT(dst); \
    return dst; \
} \
\
int V_##type##_size(V_##type *v) \
{ \
    VECTOR_ASSERT(v); \
    return v->size; \
} \
\
void V_##type##_resize(V_##type *v, int capacity) \
{ \
    VECTOR_ASSERT(v); \
    if ( !v->dynamic ) { \
        /* vector is serialized, special case */ \
        if ( capacity <= v-> capacity ) { \
            v->capacity = capacity; /* do not realloc, could be an option */ \
            return; \
        } else { \
            /* malloc a new array but do not free old */ \
            if ( !(v->dynamic = MALLOC(sizeof(type) * capacity))) \
                vector_error("malloc return NULL"); \
            memcpy(v->dynamic,v->items,v->size*sizeof(type)); \
        } \
    } else { \
        if ( !(v->dynamic = REALLOC(v->dynamic, sizeof(type) * capacity))) \
            vector_error("realloc return NULL"); \
    } \
    v->capacity = capacity; \
    v->items   = v->dynamic; \
} \
\
int V_##type##_add(V_##type *v, type item) \
{ \
    VECTOR_ASSERT(v); \
    if (v->capacity == v->size) \
        V_##type##_resize(v, v->capacity * 2); \
    v->items[v->size++] = item; \
    return v->size-1; \
} \
\
void V_##type##_set(V_##type *v, int index, type item) \
{ \
    VECTOR_ASSERT(v); \
    if (index >= 0 && index < v->size) \
        v->items[index] = item; \
} \
\
type V_##type##_get(V_##type *v, int index) \
{ \
    VECTOR_ASSERT(v); \
    if (index < 0 || index >= v->size) \
        vector_error("vector_get index out of range %d / [%d,%d]",index,0,v->size); \
    return v->items[index]; \
} \
\
type* V_##type##_getp(V_##type *v, int index) \
{ \
    VECTOR_ASSERT(v); \
    if (index < 0 || index >= v->size) \
        vector_error("vector_getp index out of range %d / [%d,%d]",index,0,v->size); \
    return &(v->items[index]); \
} \
\
void V_##type##_delete(V_##type *v, int index) \
{ \
    VECTOR_ASSERT(v); \
    if (index < 0 || index >= v->size) \
        vector_error("delete index out of range %d / [%d,%d]",index,0,v->size); \
    for (int i = index; i < v->size - 1; i++) \
        v->items[i] = v->items[i + 1]; \
    v->size--; \
    if (v->size > 0 && v->size == (v->capacity/4)) \
        V_##type##_resize(v, v->capacity / 2); \
} \
\
int V_##type##_find(V_##type *v, V_##type##_cmpfun f, type* val) { \
    VECTOR_ASSERT(v); \
    for(int i=0; i<v->size; i++) { \
        if ( f(&v->items[i],val)==0) \
            return i; \
    } \
    return -1; \
} \
\
int V_##type##_bsearch(V_##type *v, V_##type##_cmpfun f, int l, int r, type *x) \
{ \
    VECTOR_ASSERT(v); \
    if (r >= l) { \
        int mid = l + (r - l) / 2; \
        int cmp = f(&v->items[mid],x);\
        if (cmp==0) \
            return mid; \
        else if (cmp>0) \
            return V_##type##_bsearch(v, f, l, mid - 1, x); \
        return V_##type##_bsearch(v, f, mid + 1, r, x); \
    } \
    return -1; \
} \
\
void V_##type##_quicksort(V_##type *v,int first,int last, V_##type##_cmpfun f){ \
   int i, j, pivot; \
   type temp; \
   \
   VECTOR_ASSERT(v); \
   if(first<last){ \
      pivot=first; \
      i=first; \
      j=last; \
      \
      while(i<j){ \
         while((f(&v->items[i],&v->items[pivot])<=0) && i<last) \
            i++; \
         while((f(&v->items[j],&v->items[pivot])>0)) \
            j--; \
         if(i<j){ \
            temp=v->items[i]; \
            v->items[i]=v->items[j]; \
            v->items[j]=temp; \
         } \
      } \
      temp=v->items[pivot]; \
      v->items[pivot]=v->items[j]; \
      v->items[j]=temp; \
      V_##type##_quicksort(v,first,j-1,f); \
      V_##type##_quicksort(v,j+1,last,f); \
   } \
} \
\
void V_##type##_free(V_##type *v) \
{ \
    VECTOR_ASSERT(v); \
    if ( v->dynamic ) { \
        FREE(v->dynamic); \
        v->dynamic = NULL; \
    }  \
    v->items = NULL; \
}


int vector_error(const char*,...);

#endif
