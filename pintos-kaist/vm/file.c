/* file.c: Implementation of memory backed file object (mmaped object). */

#include "vm/vm.h"
#include <stdlib.h>  
#include <string.h> 

// ✅
#include "userprog/process.h"
#include "threads/mmu.h"

static bool file_backed_swap_in (struct page *page, void *kva);
static bool file_backed_swap_out (struct page *page);
static void file_backed_destroy (struct page *page);

extern bool lazy_load_segment(struct page *page, void *aux);

/* DO NOT MODIFY this struct */
static const struct page_operations file_ops = {
	.swap_in = file_backed_swap_in,
	.swap_out = file_backed_swap_out,
	.destroy = file_backed_destroy,
	.type = VM_FILE,
};

/* The initializer of file vm */
void
vm_file_init (void) {
}

/* Initialize the file backed page */
bool
file_backed_initializer (struct page *page, enum vm_type type, void *kva) {
	/* Set up the handler */
	page->operations = &file_ops;

	struct file_page *file_page = &page->file;

    return true; // ✅
}


/* Swap in the page by read contents from the file. */
// ✅
static bool
file_backed_swap_in (struct page *page, void *kva) {
	struct file_page *file_page UNUSED = &page->file;
	if (page == NULL)
        return false;

    struct segment_aux *segment_aux = (struct segment_aux *)page->uninit.aux;

    struct file *file = segment_aux->file;
	off_t offset = segment_aux->offset;
    size_t page_read_bytes = segment_aux->page_read_bytes;
    size_t page_zero_bytes = PGSIZE - page_read_bytes;

	file_seek (file, offset);

    if (file_read (file, kva, page_read_bytes) != (int) page_read_bytes) {
        // palloc_free_page (kva);
        return false;
    }

    memset ((uint8_t *)kva + page_read_bytes, 0, page_zero_bytes);

    return true;
}

/* Swap out the page by writeback contents to the file. */
// ✅
static bool
file_backed_swap_out (struct page *page) {
	struct file_page *file_page UNUSED = &page->file;
    if (page == NULL)
        return false;

    struct segment_aux * segment_aux = (struct segment_aux *) page->uninit.aux;

    // CHECK dirty page
    if(pml4_is_dirty(thread_current()->pml4, page->va)){
        file_write_at(segment_aux->file, page->va, segment_aux->page_read_bytes, segment_aux->offset);
        pml4_set_dirty (thread_current()->pml4, page->va, 0);
    }

    pml4_clear_page(thread_current()->pml4, page->va);

	return true;
}

/* Destory the file backed page. PAGE will be freed by the caller. */
// static void
// file_backed_destroy (struct page *page) {
// 	if (page->uninit.aux != NULL) {
// 		free(page->uninit.aux);
// 		page->uninit.aux = NULL;
// 	}
// }

static void
file_backed_destroy (struct page *page) {
	// mmap한 페이지가 메모리에 존재했다면 (frame이 있었다면)
	if (page->frame != NULL) {
		struct segment_aux *segment_aux = (struct segment_aux *) page->uninit.aux;

		// dirty 여부 확인 후 파일에 반영
		if (pml4_is_dirty(thread_current()->pml4, page->va)) {
			file_write_at(segment_aux->file, page->va, segment_aux->page_read_bytes, segment_aux->offset);
			pml4_set_dirty(thread_current()->pml4, page->va, 0);
		}
	}

	// aux 메모리 해제
	if (page->uninit.aux != NULL) {
		free(page->uninit.aux);
		page->uninit.aux = NULL;
	}
}


/* Do the mmap */
// ✅
void *
do_mmap (void *addr, size_t length, int writable,
		struct file *file, off_t offset) {
	struct file *re_file = file_reopen(file);

    void * mmap_addr = addr; 
    size_t read_bytes = length > file_length(file) ? file_length(file) : length; 
    size_t zero_bytes = PGSIZE - (read_bytes % PGSIZE); 

	while (read_bytes > 0 || zero_bytes > 0) {
		size_t page_read_bytes = read_bytes < PGSIZE ? read_bytes : PGSIZE;
		size_t page_zero_bytes = page_read_bytes == PGSIZE ? 0 : PGSIZE - page_read_bytes;

        struct segment_aux *segment_aux = (struct segment_aux*)malloc(sizeof(struct segment_aux));
        segment_aux->file = re_file;
        segment_aux->offset = offset;
        segment_aux->page_read_bytes = page_read_bytes;


		if (!vm_alloc_page_with_initializer (VM_FILE, mmap_addr, writable, lazy_load_segment, segment_aux)) {
			free(segment_aux);
			return NULL;
        }
		read_bytes -= page_read_bytes;
		zero_bytes -= page_zero_bytes;

		mmap_addr += PGSIZE;
		offset += page_read_bytes;
	}
	return addr;
}

/* Do the munmap */
// ✅
void
do_munmap (void *addr) {
	while (true) {
        struct page* page = spt_find_page(&thread_current()->spt, addr);

        if (page == NULL)
            break;

        struct segment_aux * segment_aux = (struct segment_aux *) page->uninit.aux;


        if(pml4_is_dirty(thread_current()->pml4, page->va)) {
            file_write_at(segment_aux->file, addr, segment_aux->page_read_bytes, segment_aux->offset); 
            pml4_set_dirty (thread_current()->pml4, page->va, 0);
        }

        pml4_clear_page(thread_current()->pml4, page->va);
        addr += PGSIZE;
    }
}