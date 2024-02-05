#ifndef _BITARCH_H_
#define _BITARCH_H_
/*                                                                    -*-C-*-
 * (C) Copyright IBM Corp. 2001, 2012  All Rights Reserved.
 */

/*
 * Set this to 1 to have 64 bit SPU
 */
#ifndef SPU_64BIT
#define SPU_64BIT 1
#endif

/*
 * Set this to 1 for 64 bit HOST.
 * It's turned off yet but once the host is 64 bit we can turn it on again
 */
#ifndef HOST_64BIT
#define HOST_64BIT 1
#endif

/* explicitly defined VARHDRSZ */

/*
 * Check if following items are in sync:
 * class NzVarlena in nde/misc/geninl.h
 * class NzVarlena in nde/expr/pgwrap.h
 * struct varlena in pg/include/c.h
 * struct varattrib in pg/include/postgres.h
 */

#define VARLENA_HDR_64BIT 1
#define VARHDRSZ ((uint32)16)

#endif /* _BITARCH_H_ */
