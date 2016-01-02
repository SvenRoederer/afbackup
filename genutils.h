/****************** Start of $RCSfile: genutils.h,v $  ****************
*
* $Source: /home/alb/afbackup/afbackup-3.5.8.12/RCS/genutils.h,v $
* $Id: genutils.h,v 1.10 2012/11/01 09:53:02 alb Exp alb $
* $Date: 2012/11/01 09:53:02 $
* $Author: alb $
*
*
******* description ***********************************************
*
*
*
*******************************************************************/

#ifndef	__GENUTILS_H
#define	__GENUTILS_H __GENUTILS_H

#include <lconf.h>

#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <string.h>
#include <math.h>
#include <sys/types.h>

#include <x_types.h>

#define	NO_ERROR	0

#define RET_ON_ERROR(function) \
	{	Int32 errfl; \
		errfl = (Int32)(function); \
		if(errfl != NO_ERROR) \
			return(errfl); \
	}

#define	ISSPACE(chr)	((chr) == '\0' || isspace(chr))
#define ishnchr(c)	(isalnum(c) || (c) == '-')
#define	isfqhnchr(c)	(ishnchr(c) || (c) == '.')

#define	NEWP(type, num)		(type *) malloc_forced((num) * sizeof(type))
#define	RENEWP(old, type, num)	(type *) realloc_forced((old), (num) * sizeof(type))
#define	SNEWP(type, num)	(type *) seg_malloc((num) * sizeof(type))
#define	SRENEWP(old, type, num, onum)	(type *) seg_realloc(old,	\
						sizeof(type) * (num),	\
						sizeof(type) * (onum))

#define	ZRENEWP	RENEWP		/* realloc_forced already handles NULL */
#define	ZSRENEWP	SRENEWP			/* seg_realloc as well */
#define	ZFREE(ptr)	{ if(ptr) { free(ptr); ptr = NULL; } }

#define	forever	for(;;)
#define	try_start	do
#define	try_end		while(0)
#define	SETZERO(var)	memset(&(var), 0, sizeof(var))
#define	COPYVAL(dest, src)	memcpy(&(dest), &(src), sizeof(src))

#ifndef XOR
#define XOR(a,b)	(((a) & ~(b)) | (~(a) & (b)))
#endif

#define	GETOUT		{ goto getout; }
#define	CLEANUP		{ goto cleanup; }
#define	GETOUTR(ret)	{ r = (ret); goto getout; }
#define	CLEANUPR(ret)	{ r = (ret); goto cleanup; }

#ifdef	__cplusplus
extern	"C" {
#endif

extern	void *	MALLOC_SAFE(Int32);
extern	void *	REALLOC_SAFE(void  *, Int32);
extern	UChar *	STRDUP_SAFE(UChar *);
extern	Int32	FSEEK_SAFE(FILE *, Int32, Int32);
extern	FILE *	FOPEN_SAFE(UChar *, UChar *);
extern	void	nomemmsgexit(void *, FILE *);
extern	Flag	is_yes(UChar *, Int32);
extern	UChar *	strapp(UChar *, UChar *);
extern	UChar *	strchain(UChar *, ...);
#ifdef	DEF_STRRSTR
extern	UChar *	strrstr(UChar *, UChar *);
#endif
#ifdef	DEF_STRCASESTR
extern	UChar *	strcasestr(UChar *, UChar *);
#endif
extern	UChar *	strword(UChar *, Int32);
extern	UChar *	strwordq(UChar *, Int32);
extern	Int32	strint(UChar *, Int32 *);
extern	int	trailingint(UChar *);
extern	Flag	chop(UChar *);
extern	UChar *	word_skip(UChar *, Int32, Int32);
#define	word_start(str, num)	word_skip((str), (num), 1)
#define	word_end(str, num)	word_skip((str), (num), -1)
extern	Int32	str2words(UChar ***, UChar *);
extern	Int32	str2wordsq(UChar ***, UChar *);
extern	Int32	cmd2argv(char ***, char *);
extern	Int32	cmd2argvq(char ***, char *);
extern	Int32	cmd2argvf(char ***, char *);
extern	Int32	cmd2argvqf(char ***, char *);
#define	REPLESC_NOOCTALS	1
extern	void	repl_esc_seq_x(UChar *, UChar, Uns8);
#define	repl_esc_seq(str, ec)	repl_esc_seq_x((str), (ec), 0)
extern	Int32	rm_backspace(UChar *);
extern	UChar * mk_esc_seq(UChar *, UChar, UChar *);
extern	UChar *	repl_substring(UChar *, UChar *, UChar *);
extern	Int32	repl_substrings(UChar **, ReplSpec *, Int32);
extern	Int32	existfile(UChar *);
extern	Int32	memswap(void *, void *, Int32);
extern	UChar *	memfind(UChar *, Int32, UChar *, Int32);
extern	Int32	memrepl(UChar *, Int32,
				UChar *, Int32, UChar *, Int32);
extern	void *	mem_move(void *, void *, size_t);
extern	void *	mem_move2(void *, void *, size_t);
extern	void *	mem_move3(void *, void *, size_t);
extern	void *	mem_move4(void *, void *, size_t);
extern	void *	memdup(void *, size_t);
extern	Int32	fscanword(FILE *, UChar *);
extern	Int32	fscanwordq(FILE *, UChar *);
extern	UChar *	sscanword(UChar *, UChar *);
extern	UChar *	sscanwordq(UChar *, UChar *);
extern	Int32	fprintwordq(FILE *, UChar *);
extern	Int32	sprintwordq(UChar *, UChar *);
extern	Int32	ffindword(FILE *, UChar *);
extern	Int32	ffindwordb(FILE *, UChar *);
extern	Int8	escaped(UChar *, UChar *, UChar);
extern	Uns8	parity_byte(UChar *, Int32);
extern	Int32	ishn(UChar *);
extern	Int32	isfqhn(UChar *);
extern	Int32	minmax(Real64 *, Int32, Real64 *, Real64 *);
extern	Int32	ffindchr(FILE *, UChar);
extern	Int32	getinchr(UChar *, Uns32);
extern 	Int32	word_count(UChar *);
extern 	Int32	word_countq(UChar *);
extern	Int32	empty_string(UChar *);
extern	UChar *	first_space(UChar *);
extern	UChar *	first_nospace(UChar *);
extern	UChar *	sscancstr(UChar *, UChar *);
extern	void	sscancchars(UChar *, UChar *);
extern	void	massage_string(UChar *);
extern	UChar **read_asc_file(UChar *, Int32 *);
extern	UChar **empty_asc_file();
extern	void	free_asc_file(UChar **, Int32);
extern	Int32	write_asc_file(UChar *, UChar **, Int32);
extern	Int32	write_asc_file_safe(UChar *, UChar **, Int32);
extern	UChar	*read_command_output(UChar *, int, int *);
#define	free_array	free_asc_file
extern	Uns32Range	*empty_Uns32Ranges();
extern	Flag	in_Uns32Ranges(Uns32Range *, Int32);
extern	Flag	overlap_Uns32Ranges(Uns32Range *, Uns32Range *);
extern	Int32	next_in_Uns32Ranges(Uns32Range *, Int32, Flag);
extern	Int32	prev_in_Uns32Ranges(Uns32Range *, Int32, Flag);
extern	Uns32Range	*sscan_Uns32Ranges(UChar *, Int32, Int32, Int32 *, UChar **);
extern	Uns32Range	*sscan_Uns32Ranges_(UChar *, Int32, Int32, Int32 *, UChar **);
extern	Uns32Range	*sscan_Uns32Ranges__(UChar *, Int32, Int32, Int32 *, UChar **);
extern	Int32	fprint_Uns32Ranges(FILE *, Uns32Range *, Int32);
extern	Int32	pack_Uns32Ranges(Uns32Range *, Int32 *);
extern	Int32	len_Uns32Ranges(Uns32Range *);
extern	Int32	num_Uns32Ranges(Uns32Range *);
extern	Uns32Range	*add_to_Uns32Ranges(Uns32Range *, Int32, Int32);
extern	Int32	merge_Uns32Ranges(Uns32Range **, Uns32Range *);
extern	UChar	*str_Uns32Ranges(Uns32Range *, Int32);
extern	Uns32Range	*del_one_from_Uns32Ranges(Uns32Range *, Int32);
extern	Uns32Range	*del_range_from_Uns32Ranges(Uns32Range *, Uns32Range *);
extern	Uns32Range	*common_Uns32Ranges(Uns32Range *, Uns32Range *);
extern	Uns32Range	*dup_Uns32Ranges(Uns32Range *);
extern	Int32	foreach_Uns32Ranges(Uns32Range *,
					Int32 (*)(Int32, void *), void *);
extern	void	q_sort2(void *, Uns32, Uns32, int (*)(void *, void *, void *), void *);
extern	void	q_sort(void *, Uns32, Uns32, int (*)(void *, void *));
extern	void	*b_search(void *, void *, Uns32, Uns32,
					int (*)(void *, void *));
extern	void	*b_locate(void *, void *, Uns32, Uns32,
					int (*)(void *, void *));
extern	void	*ba_search(void *, void *, Uns32, Uns32,
					int (*)(void *, void *));
extern	void	*l_search(void *, void *, Uns32 *, Uns32,
					int (*)(void *, void *));
extern	void	*l_find(void *, void *, Uns32 *, Uns32,
					int (*)(void *, void *));
extern	UChar	char64(Int32);
extern	Int32	base64_decode(UChar *, UChar **, Int32, Uns32 *);
extern	Int32	base64_encode(UChar *, UChar **, Int32, Flag, Uns32 *);
extern	int	cmp_Int32(void *, void *);
extern	int	cmp_Int16(void *, void *);
extern	int	cmp_Int8(void *, void *);
extern	int	cmp_SChar(void *, void *);
extern	int	cmp_Uns32(void *, void *);
extern	int	cmp_Uns16(void *, void *);
extern	int	cmp_Uns8(void *, void *);
extern	int	cmp_UChar(void *, void *);
extern	int	cmp_UCharPTR(void *, void *);
extern	int	cmp_KeyValue_bykey(void *, void *);
extern	int	cmp_KeyValue_bykeyn(void *, void *);
#define	cmp_SCharPTR	cmp_UCharPTR
extern	int	cmp_Uns32Ranges(void *, void *);
extern	int	compare_version_strings(UChar *, UChar *);
extern	int	version_string_corr(UChar *, UChar *);
extern	int	closer_version(UChar *, UChar *, UChar *);
extern	void	*seg_malloc(Int32);
extern	void	*seg_realloc(void *, Int32, Int32);
extern	Int32	__internal_sm_import(void ***, Int8 **, Int32 *,
					Int8, void *);
extern	void	*__internal_sm_malloc(void ***, Int8 **, Int32 *,
					Int8, size_t);
extern	void	*__internal_sm_realloc(void ***, Int8 **, Int32 *,
					void *, Int8, size_t);
extern	void	__internal_sm_freeall(void **, Int8 *, Int32, Int8);
extern	void	__internal_sm_free(void **, Int8 *, Int32 *, void *);
extern	UChar	*__internal_sm_strdup(void ***, Int8 **, Int32 *,
					Int8, UChar *);
extern	UChar	*__internal_sm_strapp(void ***, Int8 **, Int32 *,
					Int8, UChar *, UChar *);
extern	UChar	*__internal_sm_strchain(void ***, Int8 **, Int32 *,
					Int8, ...);
extern	void	*get_mem(void *, Int32 *, Int32, void *, Int32, Flag *);
extern	void	*get_mem_cpy(void *, Int32 *, Int32, void *, Int32, Flag *);
extern	UChar	*fget_alloc_str(FILE *);
extern	Int32	fget_realloc_str(FILE *, UChar **, Int32 *);
extern	Int32	wait_for_input(int, Int32);
extern	Int32	read_with_timeout(int, UChar *, Int32, Int32);
extern	Int32	wait_until_writable(int, Int32);
extern	Int32	find_zero(Real64 (*)(Real64), Real64, Real64, Real64,
					Real64, Real64 *);
extern	Real64	num_integ(Real64 (*)(Real64), Real64, Real64, Real64);
extern	Real64	Real64_precision();

extern	Real64	drandom();

extern	int	permutate(void *, size_t, size_t,
			int (*)(void *, int, void *), void *);

extern	Int32	goptions(Int32, UChar **, UChar *, ...);

extern	double	r_int(double);
extern	int	is_nan(float);
extern	Int32	align_n(Int32, Int32);
extern	Int32	si_symvalstr(UChar *, double, Int32, double, Flag, Flag);

extern	void	clr_timer();
extern	UChar	*timestr();
extern	UChar	*actimestr();
extern	time_t	time_from_datestr(UChar *);
extern	time_t	strint2time(UChar *);

extern	int	fork_forced();
extern	void *	malloc_forced(size_t);
extern	void *	realloc_forced(void *, size_t);

extern	Int64	write_forced(int, UChar *, Int64);
extern	Int64	read_forced(int, UChar *, Int64);
extern	Int32	fscanwordq_forced(FILE *, UChar *);

extern	pid_t	waitpid_forced(pid_t, int *, int);
extern	int	set_wlock(UChar *);
extern	int	set_rlock(UChar *);
extern	int	set_wlock_forced(UChar *);
extern	int	set_rlock_forced(UChar *);
extern	int	set_wlock_timeout(UChar *, Int32);

extern	UChar	*Real64_to_intstr(Real64, UChar *);
extern	UChar	*time_t_to_intstr(time_t, UChar *);
extern	UChar	*size_t_to_intstr(size_t, UChar *);
extern	UChar	*off_t_to_intstr(off_t, UChar *);
extern	UChar	*ino_t_to_intstr(ino_t, UChar *);

extern	Int32	stringlist_combine(UChar ***, UChar **);
extern	Int32	stringlist_common(UChar ***, UChar **);
extern	Int32	stringlist_remove(UChar ***, UChar **);
extern	Int32	stringlist_num(UChar **);
extern	Int32	stringlist_totallen(UChar **);
#define	stringlist_free	free_asc_file
extern	UChar	*stringlist_cat(UChar **, Int32, UChar *);
#define	stringlist_catall(arr, sep)	stringlist_cat((arr), 0, (sep))
extern	Int32	stringlist_contlines(UChar **, UChar);
extern	Int32	stringlist_uniq(UChar **, Flag, Flag);

extern	UChar	*str_chain_from_array(UChar **, Int32 *);
extern	UChar	**str_array_from_chain(UChar *, Int32);
extern	Int32	str_chain_num(UChar *, Int32);
extern	Int32	str_chain_uniq(UChar *, Int32);

extern	Int32	ugids_from_str(UChar *, uid_t *, gid_t *, int *, gid_t **);
extern	UChar	*str_from_ugids(uid_t, gid_t, int, gid_t *);
extern	UChar	*create_uuid(UChar *);

extern	Int32	set_var(KeyValue **, UChar *, UChar *);
extern	Int32	unset_var(KeyValue *, UChar *);
extern	UChar	*get_var(KeyValue *, UChar *, Flag);
extern	Int32	num_vars(KeyValue *);
extern	UChar	*repl_vars(UChar *, KeyValue *);

extern	Int32	set_named_data_r(KeyValue **, UChar *, void *, size_t, void **);
#define	set_named_data(kv, name, data, size)	set_named_data_r((kv), (name), (data), (size), NULL)
extern	void	*get_named_data(KeyValue *, UChar *, Flag, size_t *);
#define	unset_named_data	unset_var
#define	num_named_data	num_vars

#ifdef	__cplusplus
}
#endif

#ifdef	DEF_MEMMOVE
#define	memmove	mem_move
#endif
#ifdef	DEF_QSORT
#define	qsort	q_sort
#endif
#ifdef	DEF_BSEARCH
#define	bsearch	b_search
#endif
#ifdef	DEF_LSEARCH
#define	lsearch	l_search
#endif
#ifdef	DEF_LFIND
#define	lfind	l_find
#endif
#if	defined(DEF_ISNAN) && !defined(isnan)
#define	isnan	is_nan
#endif
#ifdef	DEF_RINT
#define	rint	r_int
#endif
#ifdef	DEF_ISATTY
#define	isatty	is_a_tty
#endif

#define	sm_list	void ** __local_memlist = NULL; 		\
				Int8 * __local_smflags = NULL;	\
				Int32 __num_memptrs = 0
#define	sm_import(ptr)	__internal_sm_import(& __local_memlist,		\
		& __local_smflags, & __num_memptrs, 0, ptr)
#define	sm_import_tmp(ptr)	__internal_sm_import(& __local_memlist,	\
		& __local_smflags, & __num_memptrs, 1, ptr)
#define	sm_malloc(size)	__internal_sm_malloc(& __local_memlist,		\
		& __local_smflags, & __num_memptrs, 0, (size))
#define	sm_malloc_tmp(size)	__internal_sm_malloc(& __local_memlist,	\
		& __local_smflags, & __num_memptrs, 1, (size))
#define	sm_realloc(ptr, size)	__internal_sm_realloc(			\
		& __local_memlist,& __local_smflags, & __num_memptrs, 	\
		(ptr), 0, (size))
#define	sm_realloc_tmp(ptr, size)	__internal_sm_realloc(	\
		& __local_memlist, & __local_smflags, & __num_memptrs, 	\
		(ptr), 1, (size))
#define	sm_return(arg)	{ __internal_sm_freeall(__local_memlist,	\
		__local_smflags, __num_memptrs, 1); return(arg); }
#define	sm_return_err(arg)	{ __internal_sm_freeall(__local_memlist, \
		__local_smflags, __num_memptrs, 0); return(arg); }
#define	sm_return_void	{ __internal_sm_freeall(__local_memlist,	\
		__local_smflags, __num_memptrs, 1); return; }
#define	sm_return_err_void	{ __internal_sm_freeall(__local_memlist, \
		__local_smflags, __num_memptrs, 0); return; }
#define	sm_free(ptr)	__internal_sm_free(__local_memlist,	\
		__local_smflags, & __num_memptrs, ptr)
#define	sm_strdup(str)	__internal_sm_strdup(& __local_memlist,		\
		& __local_smflags, & __num_memptrs, 0, (str))
#define	sm_strdup_tmp(str)	__internal_sm_strdup(& __local_memlist,	\
		& __local_smflags, & __num_memptrs, 1, (str))
#define	sm_strapp(s1, s2)	__internal_sm_strapp(& __local_memlist,	\
		& __local_smflags, & __num_memptrs, 0, (s1), (s2))
#define	sm_strapp_tmp(s1, s2)	__internal_sm_strapp(& __local_memlist,	\
		& __local_smflags, & __num_memptrs, 1, (s1), (s2))

#define	NEWSP(type, num)	(type *) sm_malloc((num) * sizeof(type))
#define	NEWSTP(type, num)	(type *) sm_malloc_tmp((num) * sizeof(type))
#define	RENEWSP(old, type, num)	(type *) sm_realloc(old, (num) * sizeof(type))
#define	RENEWSTP(old, type, num)	(type *)		\
				sm_realloc_tmp(old, (num) * sizeof(type))
#define	ZRENEWSP(old, type, num)	\
		((old) ? RENEWSP(old, type, num) : NEWSP(type, num))
#define	ZRENEWSTP(old, type, num)	\
		((old) ? RENEWSTP(old, type, num) : NEWSTP(type, num))

#define	free_string_array	free_asc_file

#ifndef	bzero
#define	bzero(mem, size)	memset((mem), 0, (size))
#endif
#ifndef	bcopy
#define	bcopy(mem1, mem2, size)	memcpy((mem2), (mem1), (size))
#endif
#ifndef	bcmp
#define	bcmp(mem1, mem2, size)	memcmp((mem1), (mem2), (size))
#endif
#define	bswap(mem1, mem2, size)	memswap((mem1), (mem2), (size))
#ifndef	MAX
#define	MAX(a, b)	((a) > (b) ? (a) : (b))
#endif
#ifndef	MIN
#define	MIN(a, b)	((a) < (b) ? (a) : (b))
#endif
#ifndef	ABS
#define	ABS(a)		((a) < 0 ? - (a) : (a))
#endif
#ifndef	SIGN
#define	SIGN(a)		((a) > 0 ? 1 : ((a) < 0 ? -1 : 0))
#endif
#ifdef	YES
#undef	YES
#endif
#ifdef	NO
#undef	NO
#endif
#ifdef	TRUE
#undef	TRUE
#endif
#ifdef	FALSE
#undef	FALSE
#endif
#define	TRUE		1
#define	FALSE		0
#define	YES		TRUE
#define	NO		FALSE
#define	DONT_KNOW	127
#ifndef	POSZ
#define	POSZ(var)	{if((var) < 0) var = 0;}
#endif

#define	UNSPECIFIED_TIME	((time_t) -1)

#define	endof(staticarr)	((staticarr) + sizeof(staticarr))

#define	UnsN_to_xref(p, v, n)	{ Uns64 va_; UChar *buf_;	\
				buf_ = (p) + (((n) - 1) >> 3);	\
				va_ = (v); while(buf_ >= (p)){		\
				*(buf_--) = (va_ & 0xff); va_ >>= 8;}}
#define	xref_to_UnsN(v, p, n)	{ UChar *endb_, *buf_; buf_ = (p);	\
				endb_ = buf_ + (((n) - 1) >> 3) + 1;	\
				*(v) = 0; while(buf_ < endb_)	\
				*(v) = (*(v) << 8) | *(buf_++); }
#define	Uns32_to_xref(p, v)	UnsN_to_xref(p, v, 32)
#define	xref_to_Uns32(v, p)	xref_to_UnsN(v, p, 32)

#define	str_to_intN(type, result, str)	{		\
		type	r = 0; char * cp;		\
		cp = (str);				\
		while(*cp && isspace(*cp)) cp++;	\
		while(isdigit(*cp))			\
			r = r * 10 + (*(cp++)) - '0';	\
		*(result) = r; }

#endif	/* !__GENUTILS_H */

