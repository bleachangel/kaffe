/* gc-mem.c
 * The heap manager.
 *
 * Copyright (c) 1996, 1997
 *	Transvirtual Technologies, Inc.  All rights reserved.
 *
 * See the file "license.terms" for information on usage and redistribution 
 * of this file. 
 */

#include "debug.h"
/* undefine this to revert to old tile scheme */
#define	PREDEFINED_NUMBER_OF_TILES

#include "config.h"
#include "config-std.h"
#include "config-mem.h"
#include "gtypes.h"
#include "locks.h"
#include "thread.h"
#include "gc.h"
#include "mem/gc-block.h"
#include "gc-mem.h"
#include "jni.h"
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif

extern iLock gc_lock;

static gc_block* gc_small_block(size_t);
static gc_block* gc_large_block(size_t);

static gc_block* gc_primitive_alloc(size_t);
static void gc_primitive_free(gc_block*);
static void* gc_system_alloc(size_t);

gc_freelist freelist[NR_FREELISTS+1]
#ifdef PREDEFINED_NUMBER_OF_TILES
	= {
#define	S(sz)	{ 0, sz }
	S(16),
	S(24),
	S(32),
	S(40),
	S(48),
	S(56),
	S(64),
	S(80),
	S(96),
	S(112),
	S(128),
	S(160),
	S(192),
	S(224),
	S(240),
	S(496),
	S(1000),
	S(2016),
	S(4040),
	{ (gc_block *)0xffffffff, 0 }
}
#endif /* PREDEFINED_NUMBER_OF_TILES */
;

static struct {
	uint16	list;
} sztable[MAX_SMALL_OBJECT_SIZE+1];
static int max_freelist;

gc_block* gc_prim_freelist;
static size_t max_small_object_size;

size_t gc_heap_total;
size_t gc_heap_allocation_size;
size_t gc_heap_limit;

#ifndef gc_pgsize
size_t gc_pgsize;
int gc_pgbits;
#endif

#ifdef DEBUG
int gc_system_alloc_cnt;
#endif

extern struct Hjava_lang_Thread* garbageman;

#ifdef DEBUG
/*
 * analyze the slack incurred by small objects
 */
static int totalslack;
static int totalsmallobjs;

static
void printslack(void)
{
	printf("allocated %d small objects, total slack %d, slack/per "
		"object %8.2f\n", 
		totalsmallobjs, totalslack, totalslack/(double)totalsmallobjs);
}
#endif /* DEBUG */

/*
 * Initialise allocator.
 */
static
void
gc_heap_initialise(void)
{
#ifndef gc_pgsize
	gc_pgsize = getpagesize();
	for (gc_pgbits = 0;
	     (1 << gc_pgbits) != gc_pgsize && gc_pgbits < 64;
	     gc_pgbits++)
		;
	assert(gc_pgbits < 64);
#endif

	gc_heap_allocation_size = Kaffe_JavaVMArgs[0].allocHeapSize;
	gc_heap_limit = Kaffe_JavaVMArgs[0].maxHeapSize;

	if (gc_heap_allocation_size > gc_heap_limit) {
		gc_heap_limit = gc_heap_allocation_size;
	}

#ifndef PREDEFINED_NUMBER_OF_TILES
    {
	int i;
	int l;
	int b;
	int t;

	/* old scheme, where number of tiles was approximated by a series
	 * of powers of two
	 */
#define	OBJSIZE(NR) \
	((gc_pgsize-GCBLOCK_OVH-ROUNDUPALIGN(1)-(NR*(2+sizeof(void*))))/NR)

	/* For a given number of tiles in a block, work out the size of
	 * the allocatable units which'll fit in them and build a translation
	 * table for the sizes.
	 */
	i = 1;
	max_small_object_size = ROUNDDOWNALIGN(OBJSIZE(i));
	l = max_small_object_size;
	for (;;) {
		b = ROUNDDOWNALIGN(OBJSIZE(i));
		if (b >= MIN_OBJECT_SIZE) {
			for (t = l; t > b; t--) {
				sztable[t].list = l;
			}
			l = t;
			i <<= 1;
		}
		else {
			for (t = l; t > MIN_OBJECT_SIZE; t--) {
				sztable[t].list = l;
			}
			for (t = 0; t <= MIN_OBJECT_SIZE; t++) {
				sztable[t].list = MIN_OBJECT_SIZE;
			}
			break;
		}
	}

	/* Translate table into list numbers */
	i = -1;
	b = -1;
	for (l = 0; l <= max_small_object_size; l++) {
		if (sztable[l].list != b) {
			b = sztable[l].list;
			i++;
			freelist[i].sz = b;
		}
		sztable[l].list = i;
	}
	max_freelist = i;
    }
#else
	/* PREDEFINED_NUMBER_OF_TILES */
	{
		/*
		 * Use the preinitialized freelist table to initialize
		 * the sztable.
		 */
		int sz = 0;
		uint16 flidx = 0;
		while (freelist[flidx].list == 0) {
			for (; sz <= freelist[flidx].sz; sz++)
				sztable[sz].list = flidx;
			flidx++;
		}
		max_small_object_size = sz - 1;
		max_freelist = flidx;
	}
#endif

DBG(SLACKANAL,
	atexit(printslack);
    )

#undef	OBJSIZE

	/* Round 'gc_heap_allocation_size' up to pagesize */
	gc_heap_allocation_size = ROUNDUPPAGESIZE(gc_heap_allocation_size);

	gc_block_init();
}

/*
 * gc_heap_malloc
 * Allocate a piece of memory.
 */
void*
gc_heap_malloc(size_t sz)
{
	static int gc_heap_init = 0;
	size_t lnr;
	gc_freeobj* mem;
	gc_block** mptr;
	gc_block* blk;
	size_t nsz;
	int times;

	/* Initialise GC heap first time in - we must assume single threaded
	 * operation here so we can do the lock initialising.
	 */
	if (gc_heap_init == 0) {
		gc_heap_init = 1;
		gc_heap_initialise();
	}

	times = 0;

DBG(SLACKANAL,
	if (GC_SMALL_OBJECT(sz)) {
		totalslack += (freelist[sztable[sz].list].sz - sz);
		totalsmallobjs++;
	}
    )

	rerun:;
	times++;

	if (GC_SMALL_OBJECT(sz)) {

		/* Translate size to object free list */
		lnr = sztable[sz].list;
		nsz = freelist[lnr].sz;

		/* No available objects? Allocate some more */
		mptr = &freelist[lnr].list;
		if (*mptr != 0) {
			blk = *mptr;
			assert(blk->free != 0);
DBG(GCALLOC,		dprintf("gc_heap_malloc: freelist %d at %p\n", sz, *mptr);)
		}
		else {
			blk = gc_small_block(nsz);
			if (blk == 0) {
				nsz = gc_pgsize;
				goto nospace;
			}
			blk->nfree = *mptr;
			*mptr = blk;

DBG(GCALLOC,		dprintf("gc_heap_malloc: small block %d at %p\n", sz, *mptr);)
		}

		/* Unlink free one and return it */
		mem = blk->free;

		DBG(GCDIAG,
		    assert(blk->magic == GC_MAGIC);
		    ASSERT_ONBLOCK(mem, blk);
		    if (mem->next) ASSERT_ONBLOCK(mem->next, blk));

		blk->free = mem->next;

		GC_SET_STATE(blk, GCMEM2IDX(blk, mem), GC_STATE_NORMAL);

		/* Once we use all the sub-blocks up, remove the whole block
		 * from the freelist.
		 */
		assert(blk->avail > 0);
		blk->avail--;
		if (blk->avail == 0) {
			*mptr = blk->nfree;
		}
	}
	else {
		nsz = sz;
		blk = gc_large_block(nsz);
		if (blk == 0) {
			nsz = nsz + GCBLOCK_OVH + sizeof(gcFuncs*) + ROUNDUPALIGN(1);
			nsz = ROUNDUPPAGESIZE(nsz);
			goto nospace;
		}
		mem = GCBLOCK2FREE(blk, 0);
		GC_SET_STATE(blk, 0, GC_STATE_NORMAL);
DBG(GCALLOC,	dprintf("gc_heap_malloc: large block %d at %p\n", sz, mem);	)
		blk->avail--;
		assert(blk->avail == 0);
	}

	/* Clear memory */
	memset(mem, 0, nsz);

	assert(GC_OBJECT_SIZE(mem) >= sz);

	return (mem);

	/* --------------------------------------------------------------- */
	nospace:;

	/* Failed to find space in any freelists. Must try to get the
	 * memory from somewhere.
	 */

	switch (times) {
	case 1:
		/* Try invoking GC if it is available */
		if (garbageman != 0) {
			/* The other caller of invokeGC,  Runtime.gc() can't 
			 * give up this lock on its own, since it does not 
			 * hold this lock.
			 */
			unlockStaticMutex(&gc_lock);
			invokeGC();
			lockStaticMutex(&gc_lock);
		}
		break;

	case 2:
		/* Get from the system */
		if (nsz < gc_heap_allocation_size) {
			nsz = gc_heap_allocation_size;
		}
		blk = gc_system_alloc(nsz);
		if (blk != 0) {
			/* Place block into the freelist for subsequent use */
			DBG(GCDIAG, blk->magic = GC_MAGIC);
			blk->size = nsz;

			/* Attach block to object hash */
			gc_block_add(blk);

			/* Free block into the system */
			gc_primitive_free(blk);
		}
		break;

	default:
		if (DBGEXPR(CATCHOUTOFMEM, true, false))
		{
			/*
			 * If we ran out of memory, a OutOfMemoryException is
			 * thrown.  If we fail to allocate memory for it, all
			 * is lost.
			 */
			static int ranout;
			assert (ranout++ == 0 || !!!"Ran out of memory!");
		}
		/* Guess we've really run out */
		return (0);
	}

	/* Try again */
	goto rerun;
}

/*
 * Free a piece of memory.
 */
void
gc_heap_free(void* mem)
{
	gc_block* info;
	gc_freeobj* obj;
	int lnr;
	int msz;
	int idx;


	info = GCMEM2BLOCK(mem);
	idx = GCMEM2IDX(info, mem);

	DBG(GCDIAG,
	    assert(info->magic == GC_MAGIC);
	    assert(GC_GET_COLOUR(info, idx) != GC_COLOUR_FREE));

	GC_SET_COLOUR(info, idx, GC_COLOUR_FREE);

DBG(GCFREE,
	dprintf("gc_heap_free: memory %p size %d\n", mem, info->size);	)

	if (GC_SMALL_OBJECT(info->size)) {
		lnr = sztable[info->size].list;
		/* If this block contains no free sub-blocks yet, attach
		 * it to freelist.
		 */
		if (info->avail == 0) {
			info->nfree = freelist[lnr].list;
			freelist[lnr].list = info;
		}
		info->avail++;
		DBG(GCDIAG,
		    /* write pattern in memory to see when live objects were
		     * freed - Note that (f4f4f4f4 == -185273100)
		     */
		    memset(mem, 0xf4, info->size));
		obj = GCMEM2FREE(mem);
		obj->next = info->free;
		info->free = obj;

		ASSERT_ONBLOCK(obj, info);

		/* If we free all sub-blocks, free the block */
		assert(info->avail <= info->nr);
		if (info->avail == info->nr) {
			gc_block** finfo = &freelist[lnr].list;
			for (;;) {
				if (*finfo == info) {
					(*finfo) = info->nfree;
					info->size = gc_pgsize;
					gc_primitive_free(info);
					break;
				}
				finfo = &(*finfo)->nfree;
				assert(*finfo != 0);
			}
		}
	}
	else {
		/* Calculate true size of block */
		msz = info->size + GCBLOCK_OVH + ROUNDUPALIGN(1);
		msz = ROUNDUPPAGESIZE(msz);
		info->size = msz;
		gc_primitive_free(info);
	}

}

/*
 * Allocate a new block of GC'ed memory.  The block will contain 'nr' objects
 * each of 'sz' bytes.
 */
static
gc_block*
gc_small_block(size_t sz)
{
	gc_block* info;
	int i;
	int nr;

	info = gc_primitive_alloc(gc_pgsize);
	if (info == 0) {
		return (0);
	}

	/* Calculate number of objects in this block */
	nr = (gc_pgsize-GCBLOCK_OVH-ROUNDUPALIGN(1))/(sz+2);

	/* Setup the meta-data for the block */
	DBG(GCDIAG, info->magic = GC_MAGIC);

	info->size = sz;
	info->nr = nr;
	info->avail = nr;
	info->funcs = (uint8*)GCBLOCK2BASE(info);
	info->state = (uint8*)(info->funcs + nr);
	info->data = (uint8*)ROUNDUPALIGN(info->state + nr);

	DBG(GCDIAG, memset(info->data, 0, sz * nr));

	/* Build the objects into a free list */
	for (i = nr-1; i >= 0; i--) {
		GCBLOCK2FREE(info, i)->next = GCBLOCK2FREE(info, i+1);
		GC_SET_COLOUR(info, i, GC_COLOUR_FREE);
		GC_SET_STATE(info, i, GC_STATE_NORMAL);
	}
	GCBLOCK2FREE(info, nr-1)->next = 0;
	info->free = GCBLOCK2FREE(info, 0);
DBG(SLACKANAL,
	int slack = ((void *)info) 
		+ gc_pgsize - (void *)(GCBLOCK2MEM(info, nr));
	totalslack += slack;
    )
	return (info);
}

/*
 * Allocate a new block of GC'ed memory.  The block will contain one object
 */
static
gc_block*
gc_large_block(size_t sz)
{
	gc_block* info;
	size_t msz;

	/* Add in management overhead */
	msz = sz+GCBLOCK_OVH+2+ROUNDUPALIGN(1);
	/* Round size up to a number of pages */
	msz = ROUNDUPPAGESIZE(msz);

	info = gc_primitive_alloc(msz);
	if (info == 0) {
		return (0);
	}

	/* Setup the meta-data for the block */
	DBG(GCDIAG, info->magic = GC_MAGIC);

	info->size = sz;
	info->nr = 1;
	info->avail = 1;
	info->funcs = (uint8*)GCBLOCK2BASE(info);
	info->state = (uint8*)(info->funcs + 1);
	info->data = (uint8*)ROUNDUPALIGN(info->state + 1);
	info->free = 0;

	DBG(GCDIAG, memset(info->data, 0, sz));

	GCBLOCK2FREE(info, 0)->next = 0;
	GC_SET_COLOUR(info, 0, GC_COLOUR_FREE);
	GC_SET_STATE(info, 0, GC_STATE_NORMAL);

	return (info);
}

/*
 * Allocate a block of memory from the free list or, failing that, the
 * system pool.
 */
static
gc_block*
gc_primitive_alloc(size_t sz)
{
	gc_block* ptr;
	gc_block** pptr;

	assert(sz % gc_pgsize == 0);

	for (pptr = &gc_prim_freelist; *pptr != 0; pptr = &ptr->next) {
		ptr = *pptr;
		/* First fit */
		if (sz <= ptr->size) {
			size_t left;
			/* If there's more than a page left, split it */
			left = ptr->size - sz;
			if (left >= gc_pgsize) {
				gc_block* nptr;

				ptr->size = sz;
				nptr = GCBLOCKEND(ptr);
				nptr->size = left;

				DBG(GCDIAG, nptr->magic = GC_MAGIC);

				nptr->next = ptr->next;
				ptr->next = nptr;
			}
			*pptr = ptr->next;
DBG(GCPRIM,		dprintf("gc_primitive_alloc: %d bytes from freelist @ %p\n", ptr->size, ptr); )
			gc_block_add(ptr);
			return (ptr);
		}
	}

	/* Nothing found on free list */
	return (0);
}

/*
 * Return a block of memory to the free list.
 */
static
void
gc_primitive_free(gc_block* mem)
{
	gc_block* lptr;
	gc_block* nptr;

	assert(mem->size % gc_pgsize == 0);

	/* Remove from object hash */
	gc_block_rm(mem);
	mem->next = 0;

	if(mem < gc_prim_freelist || gc_prim_freelist == 0) {
		/* If this block is directly before the first block on the
		 * freelist, merge it into that block.  Otherwise just
		 * attached it to the beginning.
		 */
		if (GCBLOCKEND(mem) == gc_prim_freelist) {
DBG(GCPRIM,	dprintf("gc_primitive_free: Merging (%d,%p) beginning of freelist\n", mem->size, mem); )
			mem->size += gc_prim_freelist->size;
			mem->next = gc_prim_freelist->next;
		}
		else {
DBG(GCPRIM,	dprintf("gc_primitive_free: Prepending (%d,%p) beginning of freelist\n", mem->size, mem); )
			mem->next = gc_prim_freelist;
		}
		gc_prim_freelist = mem;
		return;
	}

	/* Search the freelist for the logical place to put this block */
	lptr = gc_prim_freelist;
	while (lptr->next != 0) {
		nptr = lptr->next;
		if (mem > lptr && mem < nptr) {
			/* Block goes here in the logical scheme of things.
			 * Work out how to merge it with those which come
			 * before and after.
			 */
			if (GCBLOCKEND(lptr) == mem) {
				if (GCBLOCKEND(mem) == nptr) {
					/* Merge with last and next */
DBG(GCPRIM,				dprintf("gc_primitive_free: Merging (%d,%p) into list\n", mem->size, mem); )
					lptr->size += mem->size + nptr->size;
					lptr->next = nptr->next;
				}
				else {
					/* Merge with last but not next */
DBG(GCPRIM,				dprintf("gc_primitive_free: Merging (%d,%p) with last in list\n", mem->size, mem); )
					lptr->size += mem->size;
				}
			}
			else {
				if (GCBLOCKEND(mem) == nptr) {
					/* Merge with next but not last */
DBG(GCPRIM,				dprintf("gc_primitive_free: Merging (%d,%p) with next in list\n", mem->size, mem); )
					mem->size += nptr->size;
					mem->next = nptr->next;
					lptr->next = mem;
				}
				else {
					/* Wont merge with either */
DBG(GCPRIM,				dprintf("gc_primitive_free: Inserting (%d,%p) into list\n", mem->size, mem); )
					mem->next = nptr;
					lptr->next = mem;
				}
			}
			return;
		}
		lptr = nptr;
	}

	/* If 'mem' goes directly after the last block, merge it in.
	 * Otherwise, just add in onto the list at the end.
	 */
	if (GCBLOCKEND(lptr) == mem) {
DBG(GCPRIM,	dprintf("gc_primitive_free: Merge (%d,%p) onto last in list\n", mem->size, mem); )
		lptr->size += mem->size;
	}
	else {
DBG(GCPRIM,	dprintf("gc_primitive_free: Append (%d,%p) onto last in list\n", mem->size, mem); )
		lptr->next = mem;
	}
}

static
void*
gc_system_alloc(size_t sz)
{
	void* mem;

	assert(sz % gc_pgsize == 0);

	/* If we will pass the heap boundary, return 0 to indicate that
	 * we're run out.
	 */
	if (gc_heap_total <= gc_heap_limit && gc_heap_total + sz > gc_heap_limit) {
		gc_heap_total += sz;
		return (0);
	}
#ifdef DEBUG
	gc_system_alloc_cnt++;
#endif

	mem = gc_block_alloc(sz);
	gc_heap_total += sz;
	
DBG(GCSYSALLOC,
	dprintf("gc_system_alloc: %d byte at %p\n", sz, mem);		)

	return (mem);
}
