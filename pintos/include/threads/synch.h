#ifndef THREADS_SYNCH_H  // 헤더 파일 중복 포함 방지
#define THREADS_SYNCH_H  // 헤더 파일 정의 시작

#include <list.h>        // 대기 중인 스레드들을 관리하기 위한 리스트 자료구조
#include <stdbool.h>     // bool, true, false 정의를 위한 헤더

/* A counting semaphore. */
/* 카운팅 세마포어 구조체: 지정된 자원 수(value)를 기준으로 동기화 처리 */
struct semaphore {
	unsigned value;             /* 현재 자원 개수 또는 허용된 접근 횟수 */
	struct list waiters;        /* 이 세마포어를 기다리는 스레드 리스트 */
};

/* 세마포어 관련 함수 선언 */
void sema_init (struct semaphore *, unsigned value);       // 세마포어 초기화 함수
void sema_down (struct semaphore *);                       // P 연산 (자원 요청 및 대기)
bool sema_try_down (struct semaphore *);                   // P 연산 시도 (바로 실패 여부 반환)
void sema_up (struct semaphore *);                         // V 연산 (자원 반환 및 대기 스레드 깨움)
void sema_self_test (void);                                // 세마포어 자체 테스트 함수 (디버깅용)

/* Lock. */
/* 락 구조체: binary 세마포어 기반으로 구현된 상호 배제(mutex) 락 */
struct lock {
	struct thread *holder;      /* 락을 보유한 스레드 (디버깅 목적) */
	struct semaphore semaphore; /* 락을 제어하는 이진 세마포어 */
};

/* 락 관련 함수 선언 */
void lock_init (struct lock *);                            // 락 초기화 함수
void lock_acquire (struct lock *);                         // 락 획득 (자원 독점)
bool lock_try_acquire (struct lock *);                     // 락 획득 시도 (실패 시 바로 반환)
void lock_release (struct lock *);                         // 락 해제
bool lock_held_by_current_thread (const struct lock *);    // 현재 스레드가 락을 보유 중인지 확인

/* Condition variable. */
/* 조건 변수 구조체: 특정 조건이 충족될 때까지 대기하는 스레드를 관리 */
struct condition {
	struct list waiters;        /* 조건이 충족되기를 기다리는 스레드 리스트 */
};

/* 조건 변수 관련 함수 선언 */
void cond_init (struct condition *);                       // 조건 변수 초기화
void cond_wait (struct condition *, struct lock *);        // 조건 대기 (락을 놓고 조건 대기)
void cond_signal (struct condition *, struct lock *);      // 대기 중인 스레드 하나를 깨움
void cond_broadcast (struct condition *, struct lock *);   // 모든 대기 스레드를 깨움

/* Optimization barrier.
 *
 * 컴파일러가 최적화를 위해 명령어 순서를 재배치하지 못하도록 막는 명령어.
 * 메모리 접근 순서가 중요한 동기화 코드에서 사용됨. */
#define barrier() asm volatile ("" : : : "memory")  // 최적화 방지용 어셈블리 명령

#endif /* threads/synch.h */  // include guard 끝
