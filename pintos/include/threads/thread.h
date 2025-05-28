#ifndef THREADS_THREAD_H
#define THREADS_THREAD_H

#include <debug.h>
#include <list.h>
#include <stdint.h>
#include "threads/interrupt.h"
#ifdef VM
#include "vm/vm.h"
#endif

#include "threads/synch.h"

/* States in a thread's life cycle. */
enum thread_status
{
	THREAD_RUNNING, /* Running thread. */
	THREAD_READY,	/* Not running but ready to run. */
	THREAD_BLOCKED, /* Waiting for an event to trigger. */
	THREAD_DYING	/* About to be destroyed. */
};

/* Thread identifier type.
   You can redefine this to whatever type you like. */
typedef int tid_t;
#define TID_ERROR ((tid_t) - 1) /* Error value for tid_t. */

/* Thread priorities. */
#define PRI_MIN 0	   /* Lowest priority. */
#define PRI_DEFAULT 31 /* Default priority. */
#define PRI_MAX 63	   /* Highest priority. */

/* mlfqs 를 위한 #define 추가*/
#define NICE_DEFAULT 0
#define RECENT_CPU_DEFAULT 0
#define LOAD_AVG_DEFAULT 0

/* fd를 위한 #define 추가*/
#define MAX_FD 60

/* A kernel thread or user process.
 *
 * Each thread structure is stored in its own 4 kB page.  The
 * thread structure itself sits at the very bottom of the page
 * (at offset 0).  The rest of the page is reserved for the
 * thread's kernel stack, which grows downward from the top of
 * the page (at offset 4 kB).  Here's an illustration:
 *
 *      4 kB +---------------------------------+
 *           |          kernel stack           |
 *           |                |                |
 *           |                |                |
 *           |                V                |
 *           |         grows downward          |
 *           |                                 |
 *           |                                 |
 *           |                                 |
 *           |                                 |
 *           |                                 |
 *           |                                 |
 *           |                                 |
 *           |                                 |
 *           +---------------------------------+
 *           |              magic              |
 *           |            intr_frame           |
 *           |                :                |
 *           |                :                |
 *           |               name              |
 *           |              status             |
 *      0 kB +---------------------------------+
 *
 * The upshot of this is twofold:
 *
 *    1. First, `struct thread' must not be allowed to grow too
 *       big.  If it does, then there will not be enough room for
 *       the kernel stack.  Our base `struct thread' is only a
 *       few bytes in size.  It probably should stay well under 1
 *       kB.
 *
 *    2. Second, kernel stacks must not be allowed to grow too
 *       large.  If a stack overflows, it will corrupt the thread
 *       state.  Thus, kernel functions should not allocate large
 *       structures or arrays as non-static local variables.  Use
 *       dynamic allocation with malloc() or palloc_get_page()
 *       instead.
 *
 * The first symptom of either of these problems will probably be
 * an assertion failure in thread_current(), which checks that
 * the `magic' member of the running thread's `struct thread' is
 * set to THREAD_MAGIC.  Stack overflow will normally change this
 * value, triggering the assertion. */
/* The `elem' member has a dual purpose.  It can be an element in
 * the run queue (thread.c), or it can be an element in a
 * semaphore wait list (synch.c).  It can be used these two ways
 * only because they are mutually exclusive: only a thread in the
 * ready state is on the run queue, whereas only a thread in the
 * blocked state is on a semaphore wait list. */
struct thread
{
	/* Owned by thread.c. */
	tid_t tid;				   /* Thread identifier. */
	enum thread_status status; /* Thread state. */
	char name[16];			   /* Name (for debugging purposes). */
	int priority;			   /* Priority. */

	/* Shared between thread.c and synch.c. */
	struct list_elem elem; /* List element. */

#ifdef USERPROG
	/* Owned by userprog/process.c. */
	uint64_t *pml4; /* Page map level 4 */
#endif
#ifdef VM
	/* Table for whole virtual memory owned by thread. */
	struct supplemental_page_table spt;
#endif

	/* Owned by thread.c. */
	struct intr_frame tf; /* Information for switching */
	unsigned magic;		  /* Detects stack overflow. */

	/* alarm을 위한 깨울 시각, 슬립 리스트 연결용 엘리먼트 추가*/
	int64_t wakeup_tick;
	struct list_elem sleep_elem;

	/* donate를 위한 변수들
		base_priority : 원래 우선순위
		donation_list : 나한테 기부된 우선순위들
		donation_elem : donation_list에 들어갈 list_elem
		wating_lock : 내가 얻으려고 기다리는 락
	*/
	int base_priority;
	struct list donation_list;
	struct list_elem donation_elem;
	struct lock *waiting_lock;

	/* mlfqs를 위한 변수 추가*/
	int nice;
	int recent_cpu;

	/* all_list의 리스트 요소*/
	struct list_elem allelem;

	/* exit()를 위한 종료 상태 변수 추가 */
	int exit_status;

	/* process의 부모 자식 관계를 위한 변수 추가*/
	struct list children; // struct child_status elem 들의 리스트
	tid_t parent_tid;	  // 나의 부모를 기록

	/* fd 테이블을 추가*/
	struct file *fd_table[MAX_FD];
	int next_fd;

	/* rox를 위한 자신이 실행한 프로그램을 가짐 */
	struct file *exec_prog;
};

/* 자식 프로세스 상태를 기록할 구조체 */
struct child_status
{
	tid_t tid;			   /* 자식 스레드/프로세스 id */
	int exit_status;	   /* 자식이 exit() 에서 넘긴 상태 코드 */
	bool has_exited;	   /* exit() 이 이미 호출되었는지 */
	struct semaphore sema; /* 부모가 대기(sema_down)할 세마포어 */
	struct list_elem elem; /* 부모의 children 리스트 항목 연결자 */
};

/* If false (default), use round-robin scheduler.
   If true, use multi-level feedback queue scheduler.
   Controlled by kernel command-line option "-o mlfqs". */
extern bool thread_mlfqs;

void thread_init(void);
void thread_start(void);

void thread_tick(void);
void thread_print_stats(void);

typedef void thread_func(void *aux);
tid_t thread_create(const char *name, int priority, thread_func *, void *);

void thread_block(void);
void thread_unblock(struct thread *);

struct thread *thread_current(void);
tid_t thread_tid(void);
const char *thread_name(void);

void thread_exit(void) NO_RETURN;
void thread_yield(void);

int thread_get_priority(void);
void thread_set_priority(int);

int thread_get_nice(void);
void thread_set_nice(int);
int thread_get_recent_cpu(void);
int thread_get_load_avg(void);
void thread_update_priority(void);
void do_iret(struct intr_frame *tf);

/*
 priority donate를 위한 함수 선언
 thread_donate_priority() : donation
 thread_remove_donations_for_lock() : lock 해제 시 해당 기부 제거
 thread_update_prioriy() : base + donation 중 최대값으로 priority 갱신
*/
void thread_donate_priority(void);
void thread_remove_donations_for_lock(struct lock *lock);

/* ready_list 비교함수 선언*/
bool thread_priority_greater(const struct list_elem *a, const struct list_elem *b, void *aux);

/* 양보 시 우선순위 선점 함수 선언*/
void thread_preempt(void);

/* mlfq를 위한 함수들 선언*/
void mlfqs_calculate_priority(struct thread *t);
void mlfqs_calculate_recent_cpu(struct thread *t);
void mlfqs_calculate_load_avg(void);
void mlfqs_increment_recent_cpu(void);
void mlfqs_recalculate_recent_cpu(void);
void mlfqs_recalculate_priority(void);

/* TID로 thread 구조체를 찾아서 반환, 없으면 NULL */
struct thread *thread_by_tid(tid_t tid);
#ifndef FIXED_POINT_H
#define FIXED_POINT_H

#include <stdint.h>

/* F는 1<<14 같은 고정 소수점 스케일 팩터 */
#define F (1 << 14)
#define INT_MAX ((1 << 31) - 1)
#define INT_MIN (-(1 << 31))

int int_to_fp(int n);
int fp_to_int(int x);
int fp_to_int_round(int x);
int add_fp(int x, int y);
int sub_fp(int x, int y);
int add_mixed(int x, int n);
int sub_mixed(int x, int n);
int mult_fp(int x, int y);
int mult_mixed(int x, int n);
int div_fp(int x, int y);
int div_mixed(int x, int n);

#endif /* FIXED_POINT_H */
#endif /* threads/thread.h */