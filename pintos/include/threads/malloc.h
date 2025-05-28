#ifndef THREADS_MALLOC_H  // 헤더 파일 중복 포함을 방지하는 include guard 시작
#define THREADS_MALLOC_H  // 헤더 파일의 실제 정의가 시작되는 위치

#include <debug.h>        // ASSERT 매크로 등 디버깅 관련 기능 포함
#include <stddef.h>       // size_t 타입 정의를 포함하는 표준 헤더

void malloc_init (void); // malloc 초기화 함수 | malloc을 사용할때마다 반드시 호출하여 malloc을 초기화 한 뒤 사용해야함.
void *malloc (size_t) __attribute__ ((malloc)); // malloc 함수 | 메모리를 할당하는 함수. size만큼의 바이트를 할당하고 그 포인터를 반환. 메모리 할당에 실패하면 NULL을 반환한다.
void *calloc (size_t, size_t) __attribute__ ((malloc)); // calloc 함수 | 메모리를 할당하고 0으로 초기화하는 함수. a * b 만큼의 바이트를 할당하고 그 포인터를 반환. 메모리 할당에 실패하면 NULL을 반환한다.
void *realloc (void *, size_t); // realloc 함수 | 기존에 할당된 메모리 블록의 크기를 변경하는 함수. old_block이 NULL이면 malloc과 동일하게 동작하고, new_size가 0이면 free와 동일하게 동작한다. 새로 할당된 메모리 블록의 포인터를 반환하며, 메모리 할당에 실패하면 NULL을 반환한다.
void free (void *); // free 함수 | 할당된 메모리 블록을 해제하는 함수. p가 NULL이면 아무 동작도 하지 않는다. p가 할당된 메모리 블록의 포인터이면 해당 블록을 해제한다.

#endif /* threads/malloc.h */ // include guard의 끝