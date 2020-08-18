#ifndef PS_REFCNT_H
#define PS_REFCNT_H

#include <ps_plat.h>

struct ps_refcnt {
	unsigned long cnt;
};

static inline unsigned long
ps_refcnt_get(struct ps_refcnt *rc)
{
	return rc->cnt;
}

static inline void
ps_refcnt_take(struct ps_refcnt *rc)
{
	ps_faa(&rc->cnt, 1);
}

static inline int
ps_refcnt_release(struct ps_refcnt *rc)
{
	ps_faa(&rc->cnt, -1);

	return rc->cnt == 0;
}

#endif	/* PS_REFCNT_H */
