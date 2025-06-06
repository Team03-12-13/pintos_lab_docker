#include "userprog/syscall.h"
#include <stdio.h>
#include <syscall-nr.h>
#include "threads/interrupt.h"
#include "threads/thread.h"
#include "threads/loader.h"
#include "userprog/gdt.h"
#include "threads/flags.h"
#include "intrinsic.h"

#include <string.h>

// ✅
#include "filesys/filesys.h" 	// filesys_* func
#include "filesys/file.h"		// file_* func
#include "threads/vaddr.h"		// is_user_vaddr
// #include "lib/user/syscall.h" 	// pid_t
#include "threads/palloc.h" 	// palloc_get_page
#include "lib/stdio.h" 			// predefined fd
#include "threads/synch.h" 		// lock
#include "vm/vm.h" 

#include "userprog/process.h" 

// ✅
// pid_t는 프로세스 ID를 표현할 때 사용하는 타입
typedef int pid_t;   

void syscall_entry (void);                  // 어셈블리 레벨에서 syscall 명령어가 실행되면 진입하는 함수
void syscall_handler (struct intr_frame *); // 시스템 콜 번호를 분석하고 실제 함수를 호출하는 로직


struct lock filesys_lock;   
// 파일 시스템 접근 시 동기화를 보장하기 위한 전역 락 변수
// 파일을 열거나 읽고 쓸 때 여러 프로세스가 동시에 접근하면 안되기 때문에 이 락을 걸어야 한다.
static void check_writable(void *addr);
static void check_address(void *addr);
static void check_buffer(void *buffer, unsigned size, bool writable);

// ✅✅
void get_argument(void *rsp, int argc, void *argv[]);
void halt (void);
void exit (int status);
pid_t fork (const char *thread_name, struct intr_frame *f);
int exec (const char *file);
int wait (tid_t pid);
bool create (const char *file, unsigned initial_size);
bool remove (const char *file);
int open (const char *file);
int filesize (int fd);
int read (int fd, void *buffer, unsigned length);
int write (int fd, const void *buffer, unsigned length);
void seek (int fd, unsigned position);
unsigned tell (int fd);
void close (int fd);

/* System call.
 *
 * Previously system call services was handled by the interrupt handler
 * (e.g. int 0x80 in linux). However, in x86-64, the manufacturer supplies
 * efficient path for requesting the system call, the `syscall` instruction.
 *
 * The syscall instruction works by reading the values from the the Model
 * Specific Register (MSR). For the details, see the manual. */

void *mmap (void *addr, size_t length, int writable, int fd, off_t offset);
void munmap (void *addr);


// ✅
static int fdt_add_fd(struct file *f); 
static struct file *fdt_get_file(int fd); 
static void fdt_remove_fd(int fd);
static void check_string(const char* str);

#ifndef VM
static void check_address(void *addr);
#endif

// ✅
#ifndef VM
static void check_address(void *addr);
static void check_buffer(void *buffer, unsigned size, bool writable); 
#endif


#define MSR_STAR 0xc0000081         /* Segment selector msr */
#define MSR_LSTAR 0xc0000082        /* Long mode SYSCALL target */
#define MSR_SYSCALL_MASK 0xc0000084 /* Mask for the eflags */

// PintOS가 사용자 프로그램으로부터 시스템 콜 요청을 받을 준비를 하는 초기화 함수
// x86-64 CPU에서 시스템 콜을 처리하려면 몇 가지 MSR을 설정해야 한다.
void
syscall_init (void) {
	write_msr(MSR_STAR, 
			((uint64_t)SEL_UCSEG - 0x10) << 48  |
			((uint64_t)SEL_KCSEG) << 32);
	write_msr(MSR_LSTAR, (uint64_t) syscall_entry);

	/* The interrupt service rountine should not serve any interrupts
	 * until the syscall_entry swaps the userland stack to the kernel
	 * mode stack. Therefore, we masked the FLAG_FL. */
	write_msr(MSR_SYSCALL_MASK,
			FLAG_IF | FLAG_TF | FLAG_DF | FLAG_IOPL | FLAG_AC | FLAG_NT);

	lock_init(&filesys_lock); // ✅
}

/* The main system call interface */
// ✅
void
syscall_handler (struct intr_frame *f UNUSED) {
	// TODO: Your implementation goes here.
	struct thread *curr = thread_current();
	switch (f->R.rax) // 시스템 콜 번호에 따라 분기
	{
	case SYS_HALT:
		halt ();
		break;
	case SYS_EXIT:
		exit (f->R.rdi);
		break;
	case SYS_FORK:
		f->R.rax = fork (f->R.rdi, f);
		break;
	case SYS_EXEC:
		if (exec (f->R.rdi) == -1)
			exit (-1);
		break;
	case SYS_WAIT:
		f->R.rax = wait (f->R.rdi);
		break;
	case SYS_CREATE:
		f->R.rax = create (f->R.rdi, f->R.rsi);
		break;
	case SYS_REMOVE:
		f->R.rax = remove (f->R.rdi);
		break;
	case SYS_OPEN:
		f->R.rax = open (f->R.rdi);
		break;
	case SYS_FILESIZE:
		f->R.rax = filesize (f->R.rdi);
		break;
	case SYS_READ:
		check_buffer(f->R.rsi, f->R.rdx, 1);
		f->R.rax = read (f->R.rdi, f->R.rsi, f->R.rdx);
		break;
	case SYS_WRITE:
		check_buffer(f->R.rsi, f->R.rdx, 0);
		f->R.rax = write (f->R.rdi, f->R.rsi, f->R.rdx);
		break;
	case SYS_SEEK:
		seek (f->R.rdi, f->R.rsi);
		break;
	case SYS_TELL:
		f->R.rax = tell (f->R.rdi);
		break;
	case SYS_CLOSE:
		close (f->R.rdi);
		break;
	case SYS_MMAP:
		f->R.rax = mmap(f->R.rdi, f->R.rsi, f->R.rdx, f->R.r10, f->R.r8);
		break;
	case SYS_MUNMAP:
		munmap(f->R.rdi);
		break;
	default:
		exit (-1);
		break;
	}
	// printf ("system call!\n");
	// thread_exit ();
}

// ✅
void 
halt(void) {
	power_off();
}

// ✅
void 
exit(int status) {
	struct thread *curr = thread_current();
	curr->exit_status = status;
	printf("%s: exit(%d)\n", curr->name, status);
	thread_exit();
}

// ✅
pid_t fork (const char *thread_name, struct intr_frame *f) {
	check_string(thread_name);
	//check_address(thread_name);
	return process_fork(thread_name, f);
}

// ✅
int exec (const char *file){
	check_string(file);
	//check_address(file);

	int size = strlen(file) + 1; // 파일 사이즈(NULL 포함하기 위해 +1)
	char *fn_copy = palloc_get_page(PAL_ZERO);

	if (fn_copy == NULL)// 메모리 할당 불가 시
		exit(-1);
	strlcpy(fn_copy, file, size);

	if (process_exec(fn_copy) == -1) // [process_exec] 'load (file_name, &_if);' -> load 실패 시
		return -1;
	
	return 0;
}

// ✅
int wait(tid_t pid){
    return process_wait(pid);
}

// ✅
bool 
create(const char *file, unsigned initial_size){
    check_string(file);
    lock_acquire(&filesys_lock);
    bool result = filesys_create(file, initial_size);
    lock_release(&filesys_lock);
    return result;
}


// ✅
bool remove(const char *file){
    check_string(file);
    lock_acquire(&filesys_lock);
    bool res = filesys_remove(file);
    lock_release(&filesys_lock);
    return res;
}

// ✅
int 
open(const char *file) {
    check_string(file);
    lock_acquire(&filesys_lock);

    struct file *target_file = filesys_open(file);
    if (target_file == NULL) {
        lock_release(&filesys_lock);
        return -1;
    }

    int fd = fdt_add_fd(target_file);
    if (fd == -1) {
        file_close(target_file);
    }

    lock_release(&filesys_lock);
    return fd;
}


// ✅
int 
filesize (int fd){
    struct file *target_file = fdt_get_file(fd);
    if (target_file == NULL)
        return -1;
    lock_acquire(&filesys_lock);      // 추가
    int size = file_length(target_file);
    lock_release(&filesys_lock);      // 추가
    return size;
}


// ✅
int 
read(int fd, void *buffer, unsigned size) {
    // 1. buffer 주소 유효성 체크 (항상 가장 먼저!)
    check_writable(buffer);

    // 2. STDIN (키보드 입력)
    if (fd == STDIN_FILENO) {
        unsigned char *buf = buffer;
        for (unsigned i = 0; i < size; i++)
            buf[i] = input_getc();
        return size;
    }

    // 3. STDOUT에 read 요청 or fd < 0 or fd < 2는 무효
    if (fd < 2)
        return -1;

    // 4. 파일 객체 가져오기
    struct file *file = fdt_get_file(fd);
    if (file == NULL)
        return -1;

    // 5. 실제 파일 읽기
    lock_acquire(&filesys_lock);
    int read_bytes = file_read(file, buffer, size);
    lock_release(&filesys_lock);
    return read_bytes;
}

// ✅
int 
write(int fd, const void *buffer, unsigned size) {
    // 1. buffer 주소 유효성 체크
    check_address((void *)buffer);

    // 2. STDOUT
    if (fd == STDOUT_FILENO) {
        putbuf(buffer, size);
        return size;
    }

    // 3. STDIN에 write 요청 or fd < 0 or fd < 2는 무효
    if (fd < 2)
        return -1;

    // 4. 파일 객체 가져오기
    struct file *file = fdt_get_file(fd);
    if (file == NULL)
        return -1;

    // 5. 실제 파일 쓰기
    lock_acquire(&filesys_lock);
    int write_bytes = file_write(file, buffer, size);
    lock_release(&filesys_lock);
    return write_bytes;
}





// ✅
void 
seek (int fd, unsigned position){
    struct file *target_file = fdt_get_file(fd);
    if (fd <= STDOUT_FILENO || target_file == NULL)
        return;
    lock_acquire(&filesys_lock);      // 추가
    file_seek(target_file, position);
    lock_release(&filesys_lock);      // 추가
}

// ✅
unsigned
tell(int fd) {
    struct file *target_file = fdt_get_file(fd);
    if (fd <= STDOUT_FILENO || target_file == NULL)
        return 0;   // 실패 시 0 리턴 (안전한 기본값)
    lock_acquire(&filesys_lock);
    unsigned pos = file_tell(target_file);
    lock_release(&filesys_lock);
    return pos;
}

void *mmap (void *addr, size_t length, int writable, int fd, off_t offset){

    if (addr == NULL || is_kernel_vaddr(addr) || is_kernel_vaddr(pg_round_up(addr)) || pg_round_down(addr) != addr || spt_find_page(&thread_current()->spt, addr) \
		|| offset > PGSIZE \
		|| (long) length <= 0) 
        return NULL;


    struct file *file = fdt_get_file(fd);


    if (fd <= STDOUT_FILENO || file == NULL || file_length(file) == 0)
        return NULL;

    return do_mmap(addr, length, writable, file, offset);
}

void munmap (void *addr){
	do_munmap(addr);
}



// ✅
void
close(int fd) {
    struct file *target_file = fdt_get_file(fd);
    if (fd <= STDOUT_FILENO || target_file == NULL)
        return;
    fdt_remove_fd(fd);
    lock_acquire(&filesys_lock);
    file_close(target_file);
    lock_release(&filesys_lock);
}

static void
check_address(void *addr) {
    // struct thread *curr = thread_current();
    // if (addr == NULL 
	// 	|| !is_user_vaddr(addr)
    //     || pml4_get_page(curr->pml4, addr) == NULL)
    //     exit(-1);

	if (addr == NULL || !is_user_vaddr(addr))
        exit(-1);
    if (spt_find_page(&thread_current()->spt, addr) == NULL)
        exit(-1);
}

static void
check_buffer(void *buffer, unsigned size, bool writable) {
    for (unsigned i = 0; i < size; i += 8) {
        struct page *page = spt_find_page(&thread_current()->spt, (uint8_t *)buffer + i);
        if (!page || (writable && !page->writable)) {
            exit(-1);
        }
    }
}



static void
check_writable(void *addr){
    struct thread *curr = thread_current();
    if (addr == NULL || !is_user_vaddr(addr))
        exit(-1);
    struct page *page = spt_find_page(&curr->spt, addr);
    if (page == NULL)
        exit(-1);
    if (!page->writable)
        exit(-1);
}



// ✅
static int 
fdt_add_fd(struct file *f) {
	struct thread *curr = thread_current();
	struct file **fdt = curr->fdt;

	// fd가 제한 범위를 넘지 않고 fdt의 인덱스 위치와 일치 시
	while (curr->next_fd < FDCOUNT_LIMIT && fdt[curr->next_fd]) {
		curr->next_fd++;
	}

	// fdt가 가득 찼을 때 return -1
	if (curr->next_fd >= FDCOUNT_LIMIT)
		return -1;

	fdt[curr->next_fd] = f; // fdt에 해당 fd 새로 넣어줌
	return curr->next_fd;
}



// ✅
static struct file *
fdt_get_file(int fd) {
	struct thread *curr = thread_current();
	if (fd < STDIN_FILENO || fd >= FDCOUNT_LIMIT) { // 실패
		return NULL;
	}
	return curr->fdt[fd]; // 성공
}

// ✅
static void 
fdt_remove_fd(int fd) {
	struct thread *curr = thread_current();

	if (fd < STDIN_FILENO || fd >= FDCOUNT_LIMIT) // 실패
		return;
	
	curr->fdt[fd] = NULL; // 성공
}


// ✅
static void check_string(const char* str) {
    check_address((void*)str);
    while (1) {
        check_address((void*)str);
        if (*str == '\0') break;
        str++;
    }
}