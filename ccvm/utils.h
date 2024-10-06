#ifndef _UTILS_H_
#define _UTILS_H_

#ifdef INTELLISENSE
#define USING_GLOBALS
#include "tcc.h"
#endif

#include <stdlib.h>

#ifndef MIN
#define MIN(a,b) ((a) < (b) ? (a) : (b))
#endif
#ifndef MAX
#define MAX(a,b) ((a) > (b) ? (a) : (b))
#endif

#define VEC

#define vecFree(ptr) _vecFree(&ptr)
#define vecAlloc(ptr, initialSize) _vecAlloc((void**)&ptr, sizeof(ptr[0]), initialSize)
#define vecReallocate(ptr, newAllocated) _vecReallocate((void**)&ptr, newAllocated)
#define vecResize(ptr, newSize) _vecResize((void**)&ptr, newSize)
#define vecEnsure(ptr, index) ((typeof(ptr[0])*)_vecEnsure((void**)&ptr, index))
#define vecPush(ptr) ((typeof(ptr[0])*)_vecPush((void**)&ptr))
#define vecPushValue(ptr, value) ((typeof(ptr[0])*)_vecPushValue((void**)&ptr, &(value)))
#define vecReserve(ptr) ((typeof(ptr[0])*)_vecReserve((void**)&ptr))
#define vecPop(ptr) ((typeof(ptr[0])*)_vecPop((void**)&ptr))
#define vecEnd(ptr) ((typeof(ptr[0])*)(ptr) + ((Vector*)ptr - 1)->used)


typedef struct Vector {
    //size_t dummy[256];
    size_t itemSize;
    size_t used;
    size_t capacity;
    //size_t dummy2[256];
} Vector;


static void _vecFree(void** pptr)
{
    Vector* v = (Vector*)*pptr - 1;
    tcc_free(v);
    *pptr = NULL;
}

static void _vecAlloc(void** pptr, size_t itemSize, size_t initialCapacity)
{
    initialCapacity = MAX(initialCapacity, 1);
    Vector* v = (Vector*)tcc_mallocz(sizeof(Vector) + itemSize * initialCapacity);
    v->capacity = initialCapacity;
    v->used = 0;
    v->itemSize = itemSize;
    *pptr = v + 1;
}

static size_t vecSize(void* ptr)
{
    Vector* v = (Vector*)ptr - 1;
    return v->used;
}

static size_t vecCapacity(void* ptr)
{
    Vector* v = (Vector*)ptr - 1;
    return v->capacity;
}

static void _vecReallocate(void** pptr, size_t newCapacity)
{
    Vector* v = (Vector*)*pptr - 1;
    if (newCapacity == v->capacity) return;
    v->capacity = newCapacity;
    v = (Vector*)tcc_realloc(v, v->itemSize * newCapacity);
    *pptr = v + 1;
    if (v->used > v->capacity) {
        v->used = v->capacity;
    }
}

static void _vecResize(void** pptr, size_t newSize)
{
    Vector* v = (Vector*)*pptr - 1;
    if (newSize < v->used) {
        v->used = newSize;
        if (v->used < v->capacity / 8) {
            _vecReallocate(pptr, MAX(4, v->used * 2));
            v = (Vector*)*pptr - 1;
        }
    } else if (newSize > v->used) {
        if (newSize > v->capacity) {
            _vecReallocate(pptr, MAX(newSize + newSize / 8, 2 * v->capacity));
            v = (Vector*)*pptr - 1;
        }
        v->used = newSize;
    }
}

static void* _vecEnsure(void** pptr, size_t index)
{
    Vector* v = (Vector*)*pptr - 1;
    size_t newSize = index + 1;
    if (newSize > v->used) {
        _vecResize(pptr, newSize);
        v = (Vector*)*pptr - 1;
    }
    return (char*)(*pptr + (v->itemSize * index));
}

static void* _vecPush(void** pptr)
{
    Vector* v = (Vector*)*pptr - 1;
    return _vecEnsure(pptr, v->used);
}


static void* _vecPushValue(void** pptr, void* value)
{
    Vector* v = (Vector*)*pptr - 1;
    void* item = _vecPush(pptr);
    v = (Vector*)*pptr - 1;
    memcpy(item, value, v->itemSize);
}


static void* _vecReserve(void** pptr)
{
    Vector* v = (Vector*)*pptr - 1;
    size_t newSize = v->used + 1;
    if (newSize > v->capacity) {
        _vecReallocate(pptr, MAX(newSize + newSize / 8, 2 * v->capacity));
        v = (Vector*)*pptr - 1;
    }
    return (char*)(*pptr + (v->itemSize * v->used));
}

static void* _vecPop(void** pptr)
{
    Vector* v = (Vector*)*pptr - 1;
    if (v->used == 0) return NULL;
    _vecResize(pptr, v->used - 1);
    v = (Vector*)*pptr - 1;
    return (char*)(*pptr + (v->itemSize * v->used));
}

static void* _vecEnd(void* ptr)
{
    Vector* v = (Vector*)ptr - 1;
    return (char*)(ptr + (v->itemSize * v->used));
}


#endif // _UTILS_H_
