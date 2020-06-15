/***
 * Copyright 2015 by Gabriel Parmer.  All rights reserved.
 * Redistribution of this file is permitted under the BSD 2 clause license.
 *
 * Authors: Gabriel Parmer, gparmer@gwu.edu, 2015
 */

/*
 * TODO: most of this file should simply use the concurrency kit
 * versions.
 */

#ifndef PS_ARCH_H
#define PS_ARCH_H

#include <ps_config.h>

typedef unsigned short int u16_t;
typedef unsigned int u32_t;
typedef unsigned long long u64_t;
typedef u64_t ps_tsc_t; 	/* our time-stamp counter representation */
typedef u16_t coreid_t;
typedef u16_t localityid_t;

#ifndef likely
#define likely(x)      __builtin_expect(!!(x), 1)
#endif
#ifndef unlikely
#define unlikely(x)    __builtin_expect(!!(x), 0)
#endif

#define PS_CACHE_LINE  64
#define PS_CACHE_PAD   (PS_CACHE_LINE*2)
#define PS_WORD        sizeof(long)
#define PS_PACKED      __attribute__((packed))
#define PS_ALIGNED     __attribute__((aligned(PS_CACHE_LINE)))
#define PS_WORDALIGNED __attribute__((aligned(PS_WORD)))
#define PS_PAGE_SIZE   4096
#define PS_RNDUP(v, a) (-(-(v) & -(a))) /* from blogs.oracle.com/jwadams/entry/macros_and_powers_of_two */

#ifndef PS_WORDSIZE
#define PS_WORDSIZE 32
#endif
#if PS_WORDSIZE == 32  /* x86-32 */
#define PS_PLAT_SHIFTR32(v)
#define PS_ATOMIC_POSTFIX "l"
#else /* x86-64 */
#define PS_PLAT_SHIFTR32(v) (v |= v >> 32)
#define PS_ATOMIC_POSTFIX "q"
#endif

#define PS_CAS_INSTRUCTION "cmpxchg"
#define PS_FAA_INSTRUCTION "xadd"
#define PS_CAS_STR PS_CAS_INSTRUCTION PS_ATOMIC_POSTFIX " %2, %0; setz %1"
#define PS_FAA_STR PS_FAA_INSTRUCTION PS_ATOMIC_POSTFIX " %1, %0"

#ifndef ps_cc_barrier
#define ps_cc_barrier() __asm__ __volatile__ ("" : : : "memory")
#endif

/* Basic assembly for Cortex-A */
static inline unsigned long
ps_ldrexw(volatile unsigned long *addr)
{
	unsigned long result;
	asm volatile ("ldrex %0, %1" : "=r" (result) : "Q" (*addr) );
	return(result);
}

static inline unsigned long
ps_strexw(unsigned long value, volatile unsigned long *addr)
{
	unsigned long result;
	asm volatile ("strex %0, %2, %1" : "=&r" (result), "=Q" (*addr) : "r" (value) );
	return(result);
}


static inline void
ps_clrex(void)
{
	asm volatile ("clrex" ::: "memory");
}

/*
 * Return values:
 * 0 on failure due to contention (*target != old)
 * 1 otherwise (*target == old -> *target = updated)
 */
static inline int 
ps_cas(unsigned long *target, unsigned long old, unsigned long updated)
{
	unsigned long oldval, res;

	do {
		oldval=ps_ldrexw(target);

		if(oldval==old) /* 0=succeeded, 1=failed */
			res=ps_strexw(updated, target);
		else {
			ps_clrex();
			return 0;
		}
	}
	while(res);

	return 1;
}

/*
 * Fetch-and-add implementation on Cortex-A. Returns the original value.
 */
static inline int 
ps_faa(int *var, int value)
{
	unsigned int res;
	int oldval;

	do {
		oldval=(int)ps_ldrexw((volatile unsigned long*)var);
		res=ps_strexw((unsigned long)(oldval+value), (volatile unsigned long*)var);
	}
	while(res);

	return oldval;
}

static inline void
ps_mem_fence(void)
{ __asm__ __volatile__("dsb" ::: "memory"); }

#define ps_load(addr) (*(volatile __typeof__(*addr) *)(addr))

/*
 * Only atomic on a uni-processor, so not for cross-core coordination.
 * Faster on a multiprocessor when used to synchronize between threads
 * on a single core by avoiding locking.
 */
static inline int
ps_upcas(unsigned long *target, unsigned long old, unsigned long updated)
{
	unsigned long oldval, res;

	do {
		oldval=ps_ldrexw(target);

		if(oldval==old) /* 0=succeeded, 1=failed */
			res=ps_strexw(updated, target);
		else {
			ps_clrex();
			return 0;
		}
	}
	while(res);

	return 1;
}

static inline long
ps_upfaa(unsigned long *var, long value)
{
	unsigned int res;
	int oldval;

	do {
		oldval=(int)ps_ldrexw((volatile unsigned long*)var);
		res=ps_strexw((unsigned long)(oldval+value), (volatile unsigned long*)var);
	}
	while(res);

	return oldval;
}


/*
 * FIXME: this is truly an affront to humanity for now, but it is a
 * simple lock for testing -- naive spin *without* backoff, gulp
 *
 * This is a great example where we should be using CK.
 */
struct ps_lock {
	unsigned long o;
};

static inline void
ps_lock_take(struct ps_lock *l)
{ while (!ps_cas(&l->o, 0, 1)) ; }

static inline void
ps_lock_release(struct ps_lock *l)
{ l->o = 0; }

static inline void
ps_lock_init(struct ps_lock *l)
{ l->o = 0; }

static inline ps_tsc_t
ps_tsc(void)
{
	unsigned long a, d, c;

	/* __asm__ __volatile__("rdtsc" : "=a" (a), "=d" (d), "=c" (c) : : ); */

	return ((u64_t)d << 32) | (u64_t)a;
}

#endif /* PS_ARCH_H */
