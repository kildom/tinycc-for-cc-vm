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

#define vecFree(ptr) _vecFree((void**)&ptr)
#define vecAlloc(ptr, initialSize) _vecAlloc((void**)&ptr, sizeof(ptr[0]), initialSize)
#define vecReallocate(ptr, newAllocated) _vecReallocate((void**)&ptr, newAllocated)
#define vecResize(ptr, newSize) _vecResize((void**)&ptr, newSize)
#define vecEnsure(ptr, index) ((typeof(ptr[0])*)_vecEnsure((void**)&ptr, index))
#define vecPush(ptr) ((typeof(ptr[0])*)_vecPush((void**)&ptr))
#define vecPushValue(ptr, value) ((typeof(ptr[0])*)_vecPushValue((void**)&ptr, &(value)))
#define vecPushMulti(ptr, count) ((typeof(ptr[0])*)_vecPushMulti((void**)&ptr, (count)))
#define vecPushMultiValue(ptr, src, count) ((typeof(ptr[0])*)_vecPushMultiValue((void**)&ptr, (void*)(src), (count)))
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
    TRACE("");
    Vector* v = (Vector*)*pptr - 1;
    tcc_free(v);
    *pptr = NULL;
}

static void _vecAlloc(void** pptr, size_t itemSize, size_t initialCapacity)
{
    TRACE("");
    initialCapacity = MAX(initialCapacity, 1);
    Vector* v = (Vector*)tcc_malloc(sizeof(Vector) + itemSize * initialCapacity);
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
    TRACE("");
    Vector* v = (Vector*)*pptr - 1;
    if (newCapacity == v->capacity) return;
    v->capacity = newCapacity;
    v = (Vector*)tcc_realloc(v, sizeof(Vector) + v->itemSize * newCapacity);
    *pptr = v + 1;
    if (v->used > v->capacity) {
        v->used = v->capacity;
    }
}

static void _vecResize(void** pptr, size_t newSize)
{
    TRACE("");
    Vector* v = (Vector*)*pptr - 1;
    size_t oldSize = v->used;
    if (oldSize == 0 && newSize == 6) {
        newSize = 6;
    }
    if (newSize < oldSize) {
        if (newSize < v->capacity / 8) {
            _vecReallocate(pptr, MAX(4, newSize * 2));
            v = (Vector*)*pptr - 1;
        }
        v->used = newSize;
    } else if (newSize > oldSize) {
        if (newSize > v->capacity) {
            _vecReallocate(pptr, MAX(newSize + newSize / 8, 2 * v->capacity));
            v = (Vector*)*pptr - 1;
        }
        memset((char*)(*pptr) + (v->itemSize * oldSize), 0, v->itemSize * (newSize - oldSize));
        v->used = newSize;
    }
}

static void* _vecEnsure(void** pptr, size_t index)
{
    TRACE("");
    Vector* v = (Vector*)*pptr - 1;
    size_t oldSize = v->used;
    size_t newSize = index + 1;
    if (newSize > oldSize) {
        _vecResize(pptr, newSize);
        v = (Vector*)*pptr - 1;
    }
    return (char*)(*pptr) + (v->itemSize * index);
}

static void* _vecPush(void** pptr)
{
    TRACE("");
    Vector* v = (Vector*)*pptr - 1;
    return _vecEnsure(pptr, v->used);
}


static void* _vecPushValue(void** pptr, void* value)
{
    TRACE("");
    Vector* v = (Vector*)*pptr - 1;
    void* item = _vecPush(pptr);
    v = (Vector*)*pptr - 1;
    memcpy(item, value, v->itemSize);
}


static void* _vecPushMulti(void** pptr, size_t count)
{
    TRACE("");
    Vector* v = (Vector*)*pptr - 1;
    size_t offset = v->itemSize * v->used;
    _vecEnsure(pptr, v->used + count - 1);
    return (char*)(*pptr) + offset;
}


static void* _vecPushMultiValue(void** pptr, void* src, size_t count)
{
    TRACE("");
    void* dst = _vecPushMulti(pptr, count);
    Vector* v = (Vector*)*pptr - 1;
    memcpy(dst, src, v->itemSize * count);
    return dst;
}


static void* _vecReserve(void** pptr)
{
    TRACE("");
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
    TRACE("");
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

static const char* text_to_compare;

static void textToCompare(const char* text) {
    text_to_compare = text;
}

static bool startsWith(const char* prefix) {
    int len_text = strlen(text_to_compare);
    int len_prefix = strlen(prefix);
    if (len_text < len_prefix) return false;
    return memcmp(text_to_compare, prefix, len_prefix) == 0;
}

static bool endsWith(const char* postfix) {
    int len_text = strlen(text_to_compare);
    int len_postfix = strlen(postfix);
    if (len_text < len_postfix) return false;
    return memcmp(text_to_compare - len_postfix, postfix, len_postfix) == 0;
}

static bool theSame(const char* b) {
    return strcmp(text_to_compare, b) == 0;
}


#endif // _UTILS_H_
