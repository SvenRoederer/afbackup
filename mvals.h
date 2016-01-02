/****************** Start of $RCSfile: mvals.h,v $  ****************
*
* $Source: /home/alb/afbackup/afbackup-3.5.8.12/RCS/mvals.h,v $
* $Id: mvals.h,v 1.4 2006/12/12 20:21:11 alb Exp alb $
* $Date: 2006/12/12 20:21:11 $
* $Author: alb $
*
*
******* description ***********************************************
*
*
*
*******************************************************************/



#ifndef	_MVALS_H
#define	_MVALS_H

#include <lconf.h>

#if !defined(__WATCOMC__) && !defined(__QNX__) && !defined(__MSDOS__) && !defined(__WIN32__) && !defined(_MSC_VER)
# ifdef INC_LIMITS_H
#  include <limits.h>
#  if defined(__OpenBSD__) || defined(__NetBSD__) || defined(__FreeBSD__)
#   ifdef INT_MAX
#    ifndef MAXINT
#     define MAXINT	INT_MAX
#    endif
#   endif
#  endif
# endif
# ifdef	INC_VALUES_H
#  include <values.h>
# endif
#else

#pragma ident	"@(#)values.h	1.9	93/02/04 SMI"	/* SVr4.0 1.33	*/

#ifdef	__cplusplus
extern "C" {
#endif

/* These values work with any binary representation of integers
 * where the high-order bit contains the sign. */

/* a number used normally for size of a shift */
#define	BITSPERBYTE	8

#define	BITS(type)	(BITSPERBYTE * (int)sizeof (type))

/* short, regular and long ints with only the high-order bit turned on */
#define	HIBITS	((short)(1 << BITS(short) - 1))

#if defined(__STDC__)

#define	HIBITI	(1U << BITS(int) - 1)
#define	HIBITL	(1UL << BITS(long) - 1)

#else

#define	HIBITI	((unsigned)1 << BITS(int) - 1)
#define	HIBITL	(1L << BITS(long) - 1)

#endif

/* largest short, regular and long int */
#define	MAXSHORT	((short)~HIBITS)
#define	MAXINT	((int)(~HIBITI))
#define	MAXLONG	((long)(~HIBITL))

/* various values that describe the binary floating-point representation
 * _EXPBASE	- the exponent base
 * DMAXEXP	- the maximum exponent of a double (as returned by frexp())
 * FMAXEXP	- the maximum exponent of a float  (as returned by frexp())
 * DMINEXP	- the minimum exponent of a double (as returned by frexp())
 * FMINEXP	- the minimum exponent of a float  (as returned by frexp())
 * MAXDOUBLE	- the largest double
 *			((_EXPBASE ** DMAXEXP) * (1 - (_EXPBASE ** -DSIGNIF)))
 * MAXFLOAT	- the largest float
 *			((_EXPBASE ** FMAXEXP) * (1 - (_EXPBASE ** -FSIGNIF)))
 * MINDOUBLE	- the smallest double (_EXPBASE ** (DMINEXP - 1))
 * MINFLOAT	- the smallest float (_EXPBASE ** (FMINEXP - 1))
 * DSIGNIF	- the number of significant bits in a double
 * FSIGNIF	- the number of significant bits in a float
 * DMAXPOWTWO	- the largest power of two exactly representable as a double
 * FMAXPOWTWO	- the largest power of two exactly representable as a float
 * _IEEE	- 1 if IEEE standard representation is used
 * _DEXPLEN	- the number of bits for the exponent of a double
 * _FEXPLEN	- the number of bits for the exponent of a float
 * _HIDDENBIT	- 1 if high-significance bit of mantissa is implicit
 * LN_MAXDOUBLE	- the natural log of the largest double  -- log(MAXDOUBLE)
 * LN_MINDOUBLE	- the natural log of the smallest double -- log(MINDOUBLE)
 * LN_MAXFLOAT	- the natural log of the largest float  -- log(MAXFLOAT)
 * LN_MINFLOAT	- the natural log of the smallest float -- log(MINFLOAT)
 */

#include <float.h>

#if defined(__STDC__)

#define	MAXDOUBLE	DBL_MAX
#define	MAXFLOAT	((float)FLT_MAX)
#define	MINDOUBLE	DBL_MIN
#define	MINFLOAT	((float)FLT_MIN)
#define	_IEEE		1
#define	_DEXPLEN	11
#define	_HIDDENBIT	1
#define	DMINEXP		DBL_MIN_EXP
#define	FMINEXP		FLT_MIN_EXP

#else

#define	MAXDOUBLE	DBL_MAX
#define	MAXFLOAT	((float)FLT_MAX)
#define	MINDOUBLE	DBL_MIN
#define	MINFLOAT	((float)FLT_MIN)
#define	_IEEE		1
#define	_DEXPLEN	11
#define	_HIDDENBIT	1
#define	DMINEXP		DBL_MIN_EXP
#define	FMINEXP		FLT_MIN_EXP

#endif	/* __STDC__ */

#define	_EXPBASE	(1 << _LENBASE)
#define	_FEXPLEN	8
#define	DSIGNIF	(BITS(double) - _DEXPLEN + _HIDDENBIT - 1)
#define	FSIGNIF	(BITS(float)  - _FEXPLEN + _HIDDENBIT - 1)
#define	DMAXPOWTWO	((double)(1L << BITS(long) - 2) * \
				(1L << DSIGNIF - BITS(long) + 1))
#define	FMAXPOWTWO	((float)(1L << FSIGNIF - 1))
#define	DMAXEXP	((1 << _DEXPLEN - 1) - 1 + _IEEE)
#define	FMAXEXP	((1 << _FEXPLEN - 1) - 1 + _IEEE)
#define	LN_MAXDOUBLE	(M_LN2 * DMAXEXP)
#define	LN_MAXFLOAT	(float)(M_LN2 * FMAXEXP)
#define	LN_MINDOUBLE	(M_LN2 * (DMINEXP - 1))
#define	LN_MINFLOAT	(float)(M_LN2 * (FMINEXP - 1))
#define	H_PREC	(DSIGNIF % 2 ? (1L << DSIGNIF/2) * M_SQRT2 : 1L << DSIGNIF/2)
#define	FH_PREC \
	(float)(FSIGNIF % 2 ? (1L << FSIGNIF/2) * M_SQRT2 : 1L << FSIGNIF/2)
#define	X_EPS	(1.0/H_PREC)
#define	FX_EPS	(float)((float)1.0/FH_PREC)
#define	X_PLOSS	((double)(long)(M_PI * H_PREC))
#define	FX_PLOSS ((float)(long)(M_PI * FH_PREC))
#define	X_TLOSS	(M_PI * DMAXPOWTWO)
#define	FX_TLOSS (float)(M_PI * FMAXPOWTWO)
#define	M_LN2	0.69314718055994530942
#define	M_PI	3.14159265358979323846
#define	M_SQRT2	1.41421356237309504880
#define	MAXBEXP	DMAXEXP /* for backward compatibility */
#define	MINBEXP	DMINEXP /* for backward compatibility */
#define	MAXPOWTWO	DMAXPOWTWO /* for backward compatibility */

#ifdef	__cplusplus
}
#endif

#endif	/* !defined(__WATCOMC__) && !defined(__QNX__) */

#define	ONE_MEGA	1048576

#define	MAXReal64	MAXDOUBLE
#define	MAXReal32	MAXFLOAT
#define	MINReal64	MINDOUBLE
#define	MINReal32	MINFLOAT

/* finally hand crafted avoiding integer overflow or similar warnings */
#ifndef	MAXINT
#define	MAXINT	((((1 << (sizeof(int) * 8 - 2)) - 1) << 1) | 1)
#endif

#endif	/* _MVALS_H */
