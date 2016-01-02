/****************** Start of $RCSfile: x_types.h,v $  ****************
*
* $Source: /home/alb/afbackup/afbackup-3.5.8.12/RCS/x_types.h,v $
* $Id: x_types.h,v 1.5 2012/11/01 09:53:09 alb Exp alb $
* $Date: 2012/11/01 09:53:09 $
* $Author: alb $
*
*
******* description ***********************************************
*
*
*
*******************************************************************/

#ifndef	_X_TYPES
#define	_X_TYPES	_X_TYPES

#include <lconf.h>

#include <sys/types.h>

#ifdef	INC_STDINT_H
#include <stdint.h>
#endif

#include <x_defs.h>

#ifdef	USE_DEFINE_FOR_X_TYPES

#define	Real64		double
#define	Real32		float

#ifdef	TYPE_INT64_T
#define	Int64		int64_t
#else
#define	Int64		long long int
#endif

#ifdef	TYPE_UINT64_T
#define	Uns64		uint64_t
#else
#ifdef	TYPE_U_INT64_T
#define	Uns64		u_int64_t
#else
#define	Uns64		unsigned long long
#endif
#endif

#ifdef	TYPE_INT32_T
#define	Int32		int32_t
#else
#if (!defined(__osf__) || !defined(__alpha)) && (!defined(__linux__) || !defined(__alpha__))
#define	Int32		long int
#else
#define	Int32		int
#endif
#endif

#ifdef	TYPE_UINT32_T
#define	Uns32		uint32_t
#else
#ifdef	TYPE_U_INT32_T
#define	Uns32		u_int32_t
#else
#if (!defined(__osf__) || !defined(__alpha)) && (!defined(__linux__) || !defined(__alpha__))
#define	Uns32		unsigned long
#else
#define	Uns32		unsigned
#endif
#endif
#endif

#ifdef	TYPE_INT16_T
#define	Int16		int16_t
#else
#define	Int16		short int
#endif

#ifdef	TYPE_UINT16_T
#define	Uns16		uint16_t
#else
#ifdef	TYPE_U_INT16_T
#define	Uns16		u_int16_t
#else
#define	Uns16		unsigned short
#endif
#endif

#ifdef	TYPE_INT8_T
#define	Int8		int8_t
#else
#define	Int8		signed char
#endif

#ifdef	TYPE_UINT8_T
#define	Uns8		uint8_t
#else
#ifdef	TYPE_U_INT8_T
#define	Uns8		u_int8_t
#else
#define	Uns8		unsigned char
#endif
#endif

#define	UChar		unsigned char
#define	SChar		signed char

#define	Flag		Uns8

#else

typedef	double		Real64;		/* 64 Bit float value */
typedef	float		Real32;		/* 32 Bit float value */

#ifdef	TYPE_INT64_T
typedef	int64_t		Int64;
#else
typedef	long long int	Int64;		/* 64 Bit signed integer */
#endif

#ifdef	TYPE_INT32_T
typedef	int32_t		Int32;
#else
#if (!defined(__osf__) || !defined(__alpha)) && (!defined(__linux__) || !defined(__alpha__))
typedef	long int	Int32;		/* 32 Bit signed integer */
#else
typedef	int		Int32;
#endif
#endif

#ifdef	TYPE_UINT64_T
typedef	uint64_t	Uns64;
#else
#ifdef	TYPE_U_INT64_T
typedef	u_int64_t	Uns64;
#else
typedef	unsigned long long	Uns64;		/* 64 Bit unsigned integer */
#endif
#endif

#ifdef	TYPE_UINT32_T
typedef	uint32_t	Uns32;
#else
#ifdef	TYPE_U_INT32_T
typedef	u_int32_t	Uns32;
#else
#if (!defined(__osf__) || !defined(__alpha)) && (!defined(__linux__) || !defined(__alpha__))
typedef	unsigned long	Uns32;		/* 32 Bit unsigned integer */
#else
typedef	unsigned	Uns32;
#endif
#endif
#endif

#ifdef	TYPE_INT16_T
typedef	int16_t		Int16;
#else
typedef	short int	Int16;		/* 16 Bit signed integer */
#endif

#ifdef	TYPE_UINT16_T
typedef	uint16_t	Uns16;
#else
#ifdef	TYPE_U_INT16_T
typedef	u_int16_t	Uns16;
#else
typedef	short unsigned	Uns16;		/* 16 Bit unsigned integer */
#endif
#endif

#ifdef	TYPE_INT8_T
typedef	int8_t		Int8;
#else
typedef	signed char	Int8;		/* 8 Bit signed integer */
#endif

#ifdef	TYPE_UINT8_T
typedef	uint8_t		Uns8;
#else
#ifdef	TYPE_U_INT8_T
typedef	u_int8_t	Uns8;
#else
typedef	unsigned char	Uns8;		/* 8 Bit unsigned integer */
#endif
#endif

typedef	unsigned char	UChar;		/* character (always 8 Bit) */
typedef	signed char	SChar;

typedef	Uns8		Flag;

#endif	/* defined(USE_DEFINE_FOR_X_TYPES) */

typedef	struct	__Uns32_range	{
  Int32		first;
  Int32		last;
} Uns32Range;

typedef struct _string_chain_ {
  UChar		*chain;
  Int32		len;
} StringChain;

typedef struct _key_value_pair_ {
  UChar		*key;
  UChar		*value;
} KeyValuePair;
#define	KeyValue	KeyValuePair

#include <x_errno.h>

#ifdef	CALC_WITH_Real64
#ifdef	USE_DEFINE_FOR_X_TYPES
#define	Real	Real64
#else
typedef	Real64	Real;
#endif
#define MAXREAL	MAXReal64
#define MINREAL MINReal64
#define REAL_DEFINED
#else
#ifdef	USE_DEFINE_FOR_X_TYPES
#define	Real	Real32
#else
typedef	Real32	Real;
#endif
#define MAXREAL	MAXReal32
#define	MINREAL	MINReal32
#define REAL_DEFINED
#endif

typedef	enum	_x_type	{
  TypeReal64, TypeReal32, TypeInt32, TypeUns32,
  TypeInt16, TypeUns16, TypeUChar, TypeSChar, TypeFlag,
  TypeReal64PTR, TypeReal32PTR, TypeInt32PTR, TypeUns32PTR,
  TypeInt16PTR, TypeUns16PTR, TypeUCharPTR, TypeSCharPTR
}	X_Type;

typedef	struct __dir_repl__ {
  UChar	*token;
  UChar	*repl;
  UChar	**replptr;
} ReplSpec;

#endif	/* !_X_TYPES */

/************ end of $RCSfile: x_types.h,v $ ******************/
