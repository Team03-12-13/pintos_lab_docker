/* vm.c: Generic interface for virtual memory objects. */

#include "threads/malloc.h"
#include "vm/vm.h"
#include "vm/inspect.h"

#include "threads/vaddr.h"
#include "threads/mmu.h"
#include "userprog/process.h"
#include <stdlib.h>  
#include <string.h> 


static struct list frame_table;


static unsigned hash_func (const struct hash_elem *e, void *aux UNUSED); 
static unsigned less_func(const struct hash_elem *a, const struct hash_elem *b, void *aux); 
static bool insert_page(struct hash *h, struct page *p);
static bool delete_page(struct hash *h, struct page *p);
static void spt_destroy(struct hash_elem *e, void* aux);


void
vm_init (void) {
	vm_anon_init ();
	vm_file_init ();
#ifdef EFILESYS 
	pagecache_init ();
#endif
	register_inspect_intr ();
	/* DO NOT MODIFY UPPER LINES. */
	/* TODO: Your code goes here. */
	list_init(&frame_table); 
}

/* Get the type of the page. This function is useful if you want to know the
 * type of the page after it will be initialized.
 * This function is fully implemented now. */
enum vm_type
page_get_type (struct page *page) {
	int ty = VM_TYPE (page->operations->type);
	switch (ty) {
		case VM_UNINIT:
			return VM_TYPE (page->uninit.type);
		default:
			return ty;
	}
}

/* Helpers */
static struct frame *vm_get_victim (void);
static bool vm_do_claim_page (struct page *page);
static struct frame *vm_evict_frame (void);


bool
vm_alloc_page_with_initializer (enum vm_type type, void *upage, bool writable, vm_initializer *init, void *aux) 
{
	ASSERT (VM_TYPE(type) != VM_UNINIT) 

	struct supplemental_page_table *spt = &thread_current ()->spt;

	if (spt_find_page (spt, upage) == NULL) {

		struct page* pg = calloc(1, sizeof(struct page)); 

		typedef bool (*initializer_by_type)(struct page *, enum vm_type, void *);
        initializer_by_type initializer = NULL;

		if(VM_TYPE(type) == VM_ANON)
			initializer = anon_initializer;
		else if(VM_TYPE(type) == VM_FILE)
			initializer = file_backed_initializer;
		
		uninit_new(pg, upage, init, type, aux, initializer); 

		pg->writable = writable;
		spt_insert_page(spt, pg);
		return true;
	}
err:
	return false;
}

/* Find VA from spt and return page. On error, return NULL. */
struct page *
spt_find_page (struct supplemental_page_table *spt UNUSED, void *va UNUSED) {
	
	struct page *page = (struct page*)malloc(sizeof(struct page));	

	struct hash_elem *e;
	
	page->va = pg_round_down(va);
	e = hash_find(&spt->spt_hash, &page->hash_elem);
	free(page);

	if (e == NULL)
		return NULL;
	else
		return hash_entry(e, struct page, hash_elem); 

}


bool
spt_insert_page (struct supplemental_page_table *spt UNUSED,
		struct page *page UNUSED) {
	return insert_page(&spt -> spt_hash, page);
}

void
spt_remove_page (struct supplemental_page_table *spt, struct page *page) {
	vm_dealloc_page (page);
	return true;
}

/* Get the struct frame, that will be evicted. */
static struct frame *
vm_get_victim (void) {
	struct frame *victim = NULL;
	struct thread *curr = thread_current();
    struct list_elem *frame_e;

	for (frame_e = list_begin(&frame_table); frame_e != list_end(&frame_table); frame_e = list_next(frame_e)) {
        victim = list_entry(frame_e, struct frame, frame_elem);
        if (pml4_is_accessed(curr->pml4, victim->page->va))
            pml4_set_accessed (curr->pml4, victim->page->va, 0); 
        else
            return victim;
    }

	return victim;
}


static struct frame *
vm_evict_frame (void) {
	struct frame *victim UNUSED = vm_get_victim ();
	/* TODO: swap out the victim and return the evicted frame. */

	if(victim->page != NULL){
		swap_out(victim -> page);
		return victim;
	}
	return NULL;
}


static struct frame *
vm_get_frame (void) {
	/* TODO: Fill this function. */

	struct frame *frame = (struct frame*)malloc(sizeof(struct frame)); 

	frame->kva = palloc_get_page(PAL_USER); 
    if(frame->kva == NULL) { 
        frame = vm_evict_frame(); 
        frame->page = NULL;

        return frame; 
    }
    list_push_back (&frame_table, &frame->frame_elem); 
    frame->page = NULL;
	ASSERT (frame != NULL);
	ASSERT (frame->page == NULL);
	return frame;
}

/* Growing the stack. */
static void
vm_stack_growth (void *addr UNUSED) {

	if(vm_alloc_page(VM_ANON | VM_MARKER_0, addr, 1)) { 
		vm_claim_page(addr); 
		thread_current()->stack_bottom -= PGSIZE; 
	}
}

/* Handle the fault on write_protected page */
static bool
vm_handle_wp (struct page *page UNUSED) {
}

/* Return true on success */
bool
vm_try_handle_fault (struct intr_frame *f UNUSED, void *addr UNUSED,
		bool user UNUSED, bool write UNUSED, bool not_present UNUSED) {

	struct supplemental_page_table *spt UNUSED = &thread_current ()->spt;
	static void *STACK_MINIMUM_ADDR = USER_STACK - (1 << 20); 

	/* TODO: Validate the fault */
	/* TODO: Your code goes here */
	if (is_kernel_vaddr (addr) && user) // real fault
		return false;

    void *rsp_stack = f->rsp;
    if (not_present){
        if (!vm_claim_page(addr)){ 
			if (rsp_stack - sizeof(void*) == addr && STACK_MINIMUM_ADDR <= addr && addr <= USER_STACK) {
				vm_stack_growth(thread_current()->stack_bottom - PGSIZE);
				return true;
			}
			return false;
		}
		else
			return true;
    }
	return false;
}

/* Free the page.
 * DO NOT MODIFY THIS FUNCTION. */
void
vm_dealloc_page (struct page *page) {
	destroy (page);
	free (page);
}

/* Claim the page that allocate on VA. */
bool
vm_claim_page (void *va UNUSED) {
    struct thread *curr = thread_current();
	/* TODO: Fill this function */
	struct page *page = spt_find_page(&curr -> spt, va); 
	if (page == NULL)
		return false;

	return vm_do_claim_page (page); 
}

/* Claim the PAGE and set up the mmu. */
static bool
vm_do_claim_page (struct page *page) {
	struct frame *frame = vm_get_frame (); 

	/* Set links */
	frame->page = page; 
	page->frame = frame; 

	/* TODO: Insert page table entry to map page's VA to frame's PA. */
    struct thread *curr = thread_current();
	bool writable = page -> writable; 
	pml4_set_page(curr->pml4, page->va, frame->kva, writable); 

	return swap_in (page, frame->kva);
}


/* Initialize new supplemental page table */ 

void
supplemental_page_table_init (struct supplemental_page_table *spt UNUSED) {
	hash_init(&spt->spt_hash, hash_func, less_func, NULL);
}

/* Copy supplemental page table from src to dst */
bool
supplemental_page_table_copy (struct supplemental_page_table *dst UNUSED,
		struct supplemental_page_table *src UNUSED) {
	struct thread *curr = thread_current(); 

	struct hash_iterator i; 
    hash_first (&i, &src->spt_hash);
    while (hash_next (&i)) {
        struct page *parent_page = hash_entry (hash_cur (&i), struct page, hash_elem); 
        enum vm_type parent_type = parent_page->operations->type; 
        if(parent_type == VM_UNINIT){
            if(!vm_alloc_page_with_initializer(parent_page->uninit.type, parent_page->va, \
				parent_page->writable, parent_page->uninit.init, parent_page->uninit.aux))
                return false;
		}
        else { 

			if (parent_type & VM_MARKER_0)
				setup_stack(&thread_current()->tf); 

			else
				if(!vm_alloc_page(parent_type, parent_page->va, parent_page->writable)) 
					return false;
				if(!vm_claim_page(parent_page->va)) 
					return false;
			

            struct page* child_page = spt_find_page(dst, parent_page->va);
            memcpy(child_page->frame->kva, parent_page->frame->kva, PGSIZE); 
		}
    }
    return true;
}	

/* Free the resource hold by the supplemental page table */
void
supplemental_page_table_kill (struct supplemental_page_table *spt UNUSED) {
	/* TODO: Destroy all the supplemental_page_table hold by thread and
	 * TODO: writeback all the modified contents to the storage. */
	struct hash_iterator i;

	if (&spt->spt_hash == NULL)
		return;

    hash_first (&i, &spt->spt_hash);
	while (hash_next (&i)) {
        struct page *page = hash_entry (hash_cur (&i), struct page, hash_elem);

        if (page_get_type(page) == VM_FILE)
            do_munmap(page->va);
			
    }
    hash_destroy(&spt->spt_hash, spt_destroy);

}


static unsigned 
hash_func (const struct hash_elem *e, void *aux UNUSED) {
	const struct page *p = hash_entry(e, struct page, hash_elem); 
	return hash_bytes(&p->va, sizeof(p->va)); 
}


static unsigned 
less_func(const struct hash_elem *a, const struct hash_elem *b, void *aux) {
	const struct page *a_p = hash_entry(a, struct page, hash_elem);
	const struct page *b_p = hash_entry(b, struct page, hash_elem);
	return a_p->va < b_p->va; 
}

static bool 
insert_page(struct hash *h, struct page *p) {
    if(!hash_insert(h, &p->hash_elem))
		return true;
	else
		return false;
}

static bool 
delete_page(struct hash *h, struct page *p) {
	if(!hash_delete(h, &p->hash_elem))
		return true;
	else
		return false;
}

static void
spt_destroy(struct hash_elem *e, void* aux) {
    const struct page *p = hash_entry(e, struct page, hash_elem);
    free(p);
}
