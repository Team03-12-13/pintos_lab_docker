/* This file is derived from source code for the Nachos
   instructional operating system.  The Nachos copyright notice
   is reproduced in full below. */

/* Copyright (c) 1992-1996 The Regents of the University of California.
   All rights reserved.

   Permission to use, copy, modify, and distribute this software
   and its documentation for any purpose, without fee, and
   without written agreement is hereby granted, provided that the
   above copyright notice and the following two paragraphs appear
   in all copies of this software.

   IN NO EVENT SHALL THE UNIVERSITY OF CALIFORNIA BE LIABLE TO
   ANY PARTY FOR DIRECT, INDIRECT, SPECIAL, INCIDENTAL, OR
   CONSEQUENTIAL DAMAGES ARISING OUT OF THE USE OF THIS SOFTWARE
   AND ITS DOCUMENTATION, EVEN IF THE UNIVERSITY OF CALIFORNIA
   HAS BEEN ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

   THE UNIVERSITY OF CALIFORNIA SPECIFICALLY DISCLAIMS ANY
   WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
   WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
   PURPOSE.  THE SOFTWARE PROVIDED HEREUNDER IS ON AN "AS IS"
   BASIS, AND THE UNIVERSITY OF CALIFORNIA HAS NO OBLIGATION TO
   PROVIDE MAINTENANCE, SUPPORT, UPDATES, ENHANCEMENTS, OR
   MODIFICATIONS.
   */

#include "threads/synch.h"
#include <stdio.h>
#include <string.h>
#include "threads/interrupt.h"
#include "threads/thread.h"

//✅
void donate_priority(void);
void remove_with_lock(struct lock *);
void refresh_priority(void);
static bool cmp_d_priority (const struct list_elem *, const struct list_elem *, void *aux);
static bool cmp_sem_priority (const struct list_elem *a, const struct list_elem *b, void *aux UNUSED);


/* Initializes semaphore SEMA to VALUE.  A semaphore is a
   nonnegative integer along with two atomic operators for
   manipulating it:

   - down or "P": wait for the value to become positive, then
   decrement it.

   - up or "V": increment the value (and wake up one waiting
   thread, if any). */
void
sema_init (struct semaphore *sema, unsigned value) {
	ASSERT (sema != NULL);

	sema->value = value;          // 자원 수 초기화
	list_init (&sema->waiters);   // 대기 큐 초기화
}

/* Down or "P" operation on a semaphore.  Waits for SEMA's value
   to become positive and then atomically decrements it.

   This function may sleep, so it must not be called within an
   interrupt handler.  This function may be called with
   interrupts disabled, but if it sleeps then the next scheduled
   thread will probably turn interrupts back on. This is
   sema_down function. */
// void
// sema_down (struct semaphore *sema) {
// 	enum intr_level old_level;

// 	ASSERT (sema != NULL);
// 	ASSERT (!intr_context ());

// 	old_level = intr_disable ();
// 	while (sema->value == 0) {
// 		// ✅
// 		list_insert_ordered(&sema->waiters, &thread_current ()->elem, cmp_priority, NULL);
// 		//list_push_back (&sema->waiters, &thread_current ()->elem);
// 		thread_block ();
// 	}
// 	sema->value--;
// 	intr_set_level (old_level);
// }

// 세마포어 Down: 자원 획득 또는 대기
void
sema_down(struct semaphore *sema) {
    enum intr_level old_level;

    ASSERT(sema != NULL);
    ASSERT(!intr_context());

    old_level = intr_disable();

    // if (sema->value == 0) {
    //     list_insert_ordered(&sema->waiters, &thread_current()->elem, cmp_priority, NULL); 
        
    //     do {
    //         thread_block();         // BLOCK 상태로 전환
    //     } while (sema->value == 0); // 깨어나더라도 다시 확인 필요
    // }

	while (sema->value == 0) { 
		list_insert_ordered(&sema->waiters, &thread_current ()->elem, cmp_priority, NULL);
		// list_push_back (&sema->waiters, &thread_current ()->elem);
		thread_block ();
	}

    sema->value--;                 
    intr_set_level(old_level);
}



/* Down or "P" operation on a semaphore, but only if the
   semaphore is not already 0.  Returns true if the semaphore is
   decremented, false otherwise.

   This function may be called from an interrupt handler. */
bool
sema_try_down (struct semaphore *sema) {
	enum intr_level old_level;
	bool success;

	ASSERT (sema != NULL);

	old_level = intr_disable ();
	if (sema->value > 0)
	{
		sema->value--;
		success = true;
	}
	else
		success = false;
	intr_set_level (old_level);

	return success;
}

/* Up or "V" operation on a semaphore.  Increments SEMA's value
   and wakes up one thread of those waiting for SEMA, if any.

   This function may be called from an interrupt handler. */
// void
// sema_up (struct semaphore *sema) {
// 	enum intr_level old_level;

// 	ASSERT (sema != NULL);

// 	old_level = intr_disable ();
	
// 	// if (!list_empty (&sema->waiters))
// 	// 	thread_unblock (list_entry (list_pop_front (&sema->waiters),
// 	// 				struct thread, elem));
// 	// sema->value++;

// 	// ✅✅
// 	if (!list_empty (&sema->waiters)){
// 		list_sort(&sema->waiters, cmp_priority, NULL); // waiter_list 정렬
// 		struct thread *new_lockholder = list_entry(list_pop_front (&sema->waiters), struct thread, elem);
// 		thread_unblock (new_lockholder); // 세마포어를 해제하고 레디 상태로 만들어준다.
// 		// if (thread_get_priority() < new_lockholder->priority ){
// 		// 	if (intr_context())
// 		// 		intr_yield_on_return();
// 		// 	else
// 		// 		thread_yield();
// 		// }
// 	}
// 	sema->value++;
// 	intr_set_level (old_level);
// }

//✅
// 세마포어 Up: 자원 반납 및 대기 스레드 깨움 
void
sema_up (struct semaphore *sema) {
	enum intr_level old_level;

	ASSERT (sema != NULL);

	old_level = intr_disable ();
	sema->value++;
	if (!list_empty (&sema->waiters)){
		list_sort(&sema->waiters, cmp_priority, NULL); // waiter_list 정렬
		struct thread *new_lockholder = list_entry(list_pop_front (&sema->waiters), struct thread, elem);
		thread_unblock (new_lockholder); // 세마포어를 해제하고 레디 상태로 만들어준다.
		if (thread_get_priority() < new_lockholder->priority ){
			if (intr_context())
				intr_yield_on_return();
			else
				thread_yield();
		}
	}
	intr_set_level (old_level);
}



static void sema_test_helper (void *sema_);

/* Self-test for semaphores that makes control "ping-pong"
   between a pair of threads.  Insert calls to printf() to see
   what's going on. */
void
sema_self_test (void) {
	struct semaphore sema[2];
	int i;

	printf ("Testing semaphores...");
	sema_init (&sema[0], 0);
	sema_init (&sema[1], 0);
	thread_create ("sema-test", PRI_DEFAULT, sema_test_helper, &sema);
	for (i = 0; i < 10; i++)
	{
		sema_up (&sema[0]);
		sema_down (&sema[1]);
	}
	printf ("done.\n");
}

/* Thread function used by sema_self_test(). */
static void
sema_test_helper (void *sema_) {
	struct semaphore *sema = sema_;
	int i;

	for (i = 0; i < 10; i++)
	{
		sema_down (&sema[0]);
		sema_up (&sema[1]);
	}
}

/* Initializes LOCK.  A lock can be held by at most a single
   thread at any given time.  Our locks are not "recursive", that
   is, it is an error for the thread currently holding a lock to
   try to acquire that lock.

   A lock is a specialization of a semaphore with an initial
   value of 1.  The difference between a lock and such a
   semaphore is twofold.  First, a semaphore can have a value
   greater than 1, but a lock can only be owned by a single
   thread at a time.  Second, a semaphore does not have an owner,
   meaning that one thread can "down" the semaphore and then
   another one "up" it, but with a lock the same thread must both
   acquire and release it.  When these restrictions prove
   onerous, it's a good sign that a semaphore should be used,
   instead of a lock. */

// 락 초기화: 내부적으로 세마포어 1로 초기화
void
lock_init (struct lock *lock) {
	ASSERT (lock != NULL);

	lock->holder = NULL;
	sema_init (&lock->semaphore, 1);
}

/* Acquires LOCK, sleeping until it becomes available if
   necessary.  The lock must not already be held by the current
   thread.

   This function may sleep, so it must not be called within an
   interrupt handler.  This function may be called with
   interrupts disabled, but interrupts will be turned back on if
   we need to sleep. */

//✅
// 락 획득: 다른 스레드가 소유 중이면 세마포어 down 대기
void
lock_acquire (struct lock *lock) {
	ASSERT (lock != NULL);
	ASSERT (!intr_context ());
	ASSERT (!lock_held_by_current_thread (lock));

   // priority donation을 위해 락 소유자가 있다면 현재 스레드 정보 전달
	if (lock->holder != NULL) {               // 해당 lock의 holder가 존재한다면
		thread_current()->wait_on_lock = lock; // 해당 락을 wait_on_lock에 추가한다.
		list_insert_ordered(&lock->holder->donations, &thread_current()->d_elem, cmp_d_priority, NULL);
      donate_priority();
   }

	sema_down (&lock->semaphore);          // 자원 대기 or 획득
   thread_current()->wait_on_lock = NULL; // 해당 스레드의 wait_on_lcok을 NULL로 만든다.
	lock->holder = thread_current ();      // 락 소유자 갱신
}

/* Tries to acquires LOCK and returns true if successful or false
   on failure.  The lock must not already be held by the current
   thread.

   This function will not sleep, so it may be called within an
   interrupt handler. */
bool
lock_try_acquire (struct lock *lock) {
	bool success;

	ASSERT (lock != NULL);
	ASSERT (!lock_held_by_current_thread (lock));

	success = sema_try_down (&lock->semaphore);
	if (success)
		lock->holder = thread_current ();
	return success;
}

/* Releases LOCK, which must be owned by the current thread.
   This is lock_release function.

   An interrupt handler cannot acquire a lock, so it does not
   make sense to try to release a lock within an interrupt
   handler. */

//✅
// 락 해제: 세마포어 up, priority 복원
void
lock_release (struct lock *lock) {
	ASSERT (lock != NULL);
	ASSERT (lock_held_by_current_thread (lock));

   remove_with_lock(lock); // 기부받은 priority 제거
   refresh_priority();     // priority 재계산

	lock->holder = NULL;
	sema_up (&lock->semaphore);   // 대기 중인 스레드 깨움
}

/* Returns true if the current thread holds LOCK, false
   otherwise.  (Note that testing whether some other thread holds
   a lock would be racy.) */
bool
lock_held_by_current_thread (const struct lock *lock) {
	ASSERT (lock != NULL);

	return lock->holder == thread_current ();
}

//✅
// priority donation을 수행하는 함수
// 현재 스레드가 기다리고 있는 락의 holder에게 자신의 priority를 기부한다.
// nested donation도 지원: 최대 8단계까지 중첩 기부를 수행한다.
void 
donate_priority(void) {
	ASSERT(thread_current()->wait_on_lock != NULL);                            // 현재 스레드는 반드시 어떤 락을 기다리고 있어야 한다.
	struct thread *curr_lock_holder = thread_current()->wait_on_lock->holder;  // 현재 스레드가 기다리는 락의 holder를 가져온다.
	int nested_depth = 8;                                                      // 최대 8단계까지 nested donation 허용
	
	// 현재 스레드의 priority가 holder보다 높은 동안 반복
	while (thread_get_priority() > curr_lock_holder->priority){

		if (nested_depth-- <= 0) break;                     // 기부 depth 제한 초과 시 중단 (무한 루프 방지)
		curr_lock_holder->priority = thread_get_priority(); // holder의 priority를 현재 스레드의 priority로 갱신
		
		// holder가 또 다른 락을 기다리고 있다면 다음 holder로 이동 (nested donation)
		if (curr_lock_holder->wait_on_lock != NULL)
			curr_lock_holder = curr_lock_holder->wait_on_lock->holder;
		else
			break; // 더 이상 nested되지 않으면 종료
	}
}


//✅
// lock을 해제했을 때 해당 락을 기다리며 기부한 스레드를 donations 리스트에서 제거
void 
remove_with_lock(struct lock *lock) {
	struct list_elem *curr = list_begin(&thread_current()->donations);
	struct thread *curr_thread;

   // donations 리스트 전체를 순회하며 해당 락과 관련된 스레드를 제거
	while (curr != list_end(&thread_current()->donations)){ 
		curr_thread = list_entry(curr, struct thread, d_elem);

      // 이 스레드가 이 락을 기다리고 있다면 삭제
		if (curr_thread -> wait_on_lock == lock)
			curr = list_remove(curr); // 삭제 후 다음 노드는 자동으로 curr에 저장됨
		else
			curr = curr -> next;
   }
}


//✅
// 현재 스레드의 priority를 재계산하는 함수
// priority_base를 기준으로 donation이 있으면 가장 높은 값을 사용한다.
void
refresh_priority(void){
	struct list_elem *donation_elem;
	struct list *curr_donations = &thread_current()->donations;
	int donations_priority;

	thread_current()->priority = thread_current()->priority_base; // priority를 기부 받기 전의 원래 값으로 되돌림

   // donation 리스트가 비어있지 않으면 우선순위 재계산
	if (!list_empty(curr_donations)){ 
		list_sort(curr_donations, cmp_d_priority, NULL); // donations 리스트를 우선순위 기준으로 정렬

      // 가장 높은 priority를 가진 donation 값을 확인
		donation_elem = list_begin(curr_donations);
		donations_priority = list_entry(donation_elem, struct thread, d_elem)->priority;

      // donation 우선순위가 현재 스레드보다 높으면 갱신
		if (thread_get_priority() < donations_priority)
			thread_current()->priority = donations_priority;
   }
}


//✅
// donation elem 으로 정렬 기준 재지정
static bool
cmp_d_priority (const struct list_elem *a, const struct list_elem *b, void *aux UNUSED) {
	return list_entry(a, struct thread, d_elem) -> priority \
		      > list_entry(b, struct thread, d_elem) -> priority ? \
            1:0;
};
      
/* One semaphore in a list. */
struct semaphore_elem {
	struct list_elem elem;              /* List element. */
	struct semaphore semaphore;         /* This semaphore. */
};

//✅
// Condition variables
static bool
cmp_sem_priority (const struct list_elem *a, const struct list_elem *b, void *aux UNUSED) {
	return list_entry (list_begin (&list_entry (a, struct semaphore_elem, elem)->semaphore.waiters), struct thread, elem)->priority \
		> list_entry (list_begin (&list_entry (b, struct semaphore_elem, elem)->semaphore.waiters), struct thread, elem)->priority;
}

/* Initializes condition variable COND.  A condition variable
   allows one piece of code to signal a condition and cooperating
   code to receive the signal and act upon it. */
void
cond_init (struct condition *cond) {
	ASSERT (cond != NULL);

	list_init (&cond->waiters);
}

/* Atomically releases LOCK and waits for COND to be signaled by
   some other piece of code.  After COND is signaled, LOCK is
   reacquired before returning.  LOCK must be held before calling
   this function.

   The monitor implemented by this function is "Mesa" style, not
   "Hoare" style, that is, sending and receiving a signal are not
   an atomic operation.  Thus, typically the caller must recheck
   the condition after the wait completes and, if necessary, wait
   again.

   A given condition variable is associated with only a single
   lock, but one lock may be associated with any number of
   condition variables.  That is, there is a one-to-many mapping
   from locks to condition variables.

   This function may sleep, so it must not be called within an
   interrupt handler.  This function may be called with
   interrupts disabled, but interrupts will be turned back on if
   we need to sleep. */

// 조건 대기: 락을 잠깐 내려놓고 sema_down으로 대기, signal 받을 때 다시 획득
void
cond_wait (struct condition *cond, struct lock *lock) {
	struct semaphore_elem waiter;

	ASSERT (cond != NULL);
	ASSERT (lock != NULL);
	ASSERT (!intr_context ());
	ASSERT (lock_held_by_current_thread (lock));

	sema_init (&waiter.semaphore, 0);
	//list_push_back (&cond->waiters, &waiter.elem);
   list_insert_ordered (&cond->waiters, &waiter.elem, cmp_sem_priority, NULL); //✅
	lock_release (lock);
	sema_down (&waiter.semaphore);
	lock_acquire (lock);
}

/* If any threads are waiting on COND (protected by LOCK), then
   this function signals one of them to wake up from its wait.
   LOCK must be held before calling this function.

   An interrupt handler cannot acquire a lock, so it does not
   make sense to try to signal a condition variable within an
   interrupt handler. */

// 조건 하나 깨우기 (waiters 중 priority 가장 높은 스레드 깨움)
void
cond_signal (struct condition *cond, struct lock *lock UNUSED) {
	ASSERT (cond != NULL);
	ASSERT (lock != NULL);
	ASSERT (!intr_context ());
	ASSERT (lock_held_by_current_thread (lock));

	if (!list_empty (&cond->waiters)){
      list_sort (&cond->waiters, cmp_sem_priority, NULL); //✅
		sema_up (&list_entry (list_pop_front (&cond->waiters),
					struct semaphore_elem, elem)->semaphore);
   }
}

/* Wakes up all threads, if any, waiting on COND (protected by
   LOCK).  LOCK must be held before calling this function.

   An interrupt handler cannot acquire a lock, so it does not
   make sense to try to signal a condition variable within an
   interrupt handler. */

// 조건 전체 깨우기 (모든 waiters 스레드 깨움)
void
cond_broadcast (struct condition *cond, struct lock *lock) {
	ASSERT (cond != NULL);
	ASSERT (lock != NULL);

	while (!list_empty (&cond->waiters))
		cond_signal (cond, lock);
}
