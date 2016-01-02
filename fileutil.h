/****************** Start of $RCSfile: fileutil.h,v $  ****************
*
* $Source: /home/alb/afbackup/afbackup-3.5.8.12/RCS/fileutil.h,v $
* $Id: fileutil.h,v 1.12 2013/11/08 17:58:06 alb Exp alb $
* $Date: 2013/11/08 17:58:06 $
* $Author: alb $
*
*
******* description ***********************************************
*
*
*
*******************************************************************/

#include <lconf.h>

#ifndef	__FILE_UTIL_H
#define	__FILE_UTIL_H	__FILE_UTIL_H

#include <stdio.h>
#include <time.h>
#include <ctype.h>
#include <x_types.h>
#include <string.h>
#include <unistd.h>
#include <stdarg.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <poll.h>

#include <genutils.h>

/* auxiliary macros */
#ifdef	O_NONBLOCK
#define	NONBLOCKING_FLAGS O_NONBLOCK
#endif
#if	!defined(__hpux) && !defined(hpux)	/* f... HP */
#ifdef	O_NDELAY
#ifdef	O_NONBLOCK
#undef	NONBLOCKING_FLAGS
#define	NONBLOCKING_FLAGS	( O_NONBLOCK | O_NDELAY )
#else
#undef	NONBLOCKING_FLAGS
#define	NONBLOCKING_FLAGS	O_NDELAY
#endif
#endif	/* O_NDELAY */
#endif	/* HP Grrr */
#ifndef	NONBLOCKING_FLAGS
#error	Cannot build. Neither O_NONBLOCK nor O_NDELAY defined.
#endif
#define	INV_NONBLOCKING_FLAGS	( ~ NONBLOCKING_FLAGS )
#if	defined(__hpux) || defined(hpux)	/* f... HP */
#ifdef	O_NDELAY
#undef	INV_NONBLOCKING_FLAGS
#define	INV_NONBLOCKING_FLAGS	( ~ (NONBLOCKING_FLAGS | O_NDELAY))
#endif	/* O_NDELAY */
#endif	/* O_NDELAY */

#define	POLL_ERR_FLAGS	(POLLERR | POLLHUP | POLLNVAL)
#define	POLLINEVENT(e)	((e) & (POLLIN | POLL_ERR_FLAGS))
#define	POLLINERR(e)	((e) & POLL_ERR_FLAGS)
#define	POLLOUTEVENT(e)	((e) & (POLLOUT | POLL_ERR_FLAGS))
#define	POLLOUTERR(e)	((e) & POLL_ERR_FLAGS)

#ifndef	O_BINARY
#define	O_BINARY	0
#endif

typedef	struct _param_file_entry {
  void		*entry_ptr;
  Uns32		*num_entries;  
  UChar		*pattern;
  X_Type	type;
} ParamFileEntry;

typedef struct _find_params {
  UChar		**excl_dirs;
  UChar		**names;
  UChar		**excl_names;
  Uns32		options;
  time_t	newer_than;
  time_t	older_than;
  Int32		(*errfunc)(UChar *, void *);
  void		*errfunc_param;
  UChar		*excl_filename;
  UChar		**fstypes_to_search;
  UChar		**fstypes_to_skip;
  UChar		**fstypes_to_prune;
  int		*interrupted;
} FindParams;

#define	FIND_NO_DIRS		1
#define	FIND_NO_FILES		(FIND_NO_DIRS << 1)
#define	FIND_NO_SLINKS		(FIND_NO_DIRS << 2)
#define	FIND_NO_CDEVS		(FIND_NO_DIRS << 3)
#define	FIND_NO_BDEVS		(FIND_NO_DIRS << 4)
#define	FIND_NO_FIFOS		(FIND_NO_DIRS << 5)
#define	FIND_NO_SOCKS		(FIND_NO_DIRS << 6)
#define	FIND_FOLLOW_SLINKS	(FIND_NO_DIRS << 7)
#define	FIND_LOCAL_DEV		(FIND_NO_DIRS << 8)
#define	FIND_DEPTH		(FIND_NO_DIRS << 9)
#define	FIND_CMP_CTIME		(FIND_NO_DIRS << 10)

#define	FIND_ONLY_DIRS		(FIND_NO_FILES | FIND_NO_SLINKS	\
				| FIND_NO_CDEVS | FIND_NO_BDEVS \
				| FIND_NO_FIFOS | FIND_NO_SOCKS)

#define	KFILE_SORT	1
#define	KFILE_SORTN	(1 << 1)
#define	KFILE_LOCKED	(1 << 10)
#define	KFILE_OPTLOCK	((1 << 11) | KFILE_LOCKED)	/* optional lock */

#ifdef	__cplusplus
extern	"C"	{
#endif

extern	Int32	save_insert(UChar *, UChar *,
				Int32 (*)(FILE *, void *), void *);
extern	Int32	read_param_file(UChar *, ParamFileEntry *, Int32,
				UChar *, UChar *);
extern	Int32	sscanXValue(UChar *, void *, X_Type, Int32 *);
extern	Int32	fscanXValue(FILE *, void *, X_Type, Int32 *);
extern	UChar	*find_program(UChar *);
extern	Int32	cleanpath(UChar *);
extern	Int32	cleanpath__(UChar *);
extern	UChar	*mkabspath(UChar *, UChar *);
extern	UChar	*get_cwd(UChar *, Int32);
extern	Int32	mkrelpath(UChar *);
extern	UChar	*resolve_path(UChar *, UChar *);
extern	UChar	*resolve_path__(UChar *, UChar *);
extern	Int32	find1(UChar *, FindParams *,
			Int32 (*)(UChar *, void *, struct stat *), void *);
extern	Int32	find(UChar **, FindParams *,
			Int32 (*)(UChar *, void *, struct stat *), void *);
extern	UChar	**fnglob1(UChar *);
extern	UChar	**fnglob(UChar *);
extern	Int32	copy_file(UChar *, UChar *);
extern	UChar	*hidden_filename(UChar *);
extern	Int32	make_dirpath(UChar *, Uns32, uid_t, gid_t);
extern	Int32	kfile_insert(UChar *, UChar *, UChar *, Uns32);
extern	Int32	kfile_delete(UChar *, UChar *, Uns32);
extern	UChar	*kfile_get(UChar *, UChar *, Uns32);
extern	KeyValuePair	*kfile_getall(UChar *, Int32 *, Uns32);
extern	void	kfile_freeall(KeyValuePair *, Int32);
extern	Int32	add_to_int_list_file(UChar *, Int32);
extern	Int32	*get_list_from_int_list_file(UChar *, Int32 *);
extern	UChar	*tmp_name(UChar *);
extern	int	tmp_file(UChar *);
extern	FILE	*tmp_fp(UChar *);
extern	UChar	*check_commands_executable(Flag, UChar *, ...);
extern	Int32	mkbasedir(UChar *, mode_t, uid_t, gid_t);
extern	Int32	mkdirpath(UChar *, mode_t, uid_t, gid_t);
extern	Int32	perms_from_string(UChar *, mode_t *);
extern	void	permstr_from_mode(char *, mode_t);
extern	void	close_fd_ranges(Uns32Range *);
extern	Int32	pipethrough(int, int);
extern	UChar	*parentdir(UChar *, Flag);
extern	Flag	fsentry_access(UChar *, int);
extern	Int32	compare_files(UChar *, UChar *);
extern	UChar	*read_file_x(UChar *, Int64, Int64, Int64 *, Int64 *);
extern	UChar	*read_file(UChar *, Int64 *);
extern	int	read_Uns64_file(UChar *, Uns64 *);
extern	int	write_Uns64_file(UChar *, Uns64, int);

#ifdef	__cplusplus
}
#endif

#define	IS_REGFILE(statbuf)	S_ISREG((statbuf).st_mode)
#define	IS_FIFO(statbuf)	S_ISFIFO((statbuf).st_mode)
#define	IS_SYMLINK(statbuf)	S_ISLNK((statbuf).st_mode)
#define	IS_HARDLINK(statbuf)	((statbuf).st_nlink > 1)
#define	IS_DIRECTORY(statbuf)	S_ISDIR((statbuf).st_mode)
#define	IS_BLOCKDEV(statbuf)	S_ISBLK((statbuf).st_mode)
#define	IS_CHARDEV(statbuf)	S_ISCHR((statbuf).st_mode)
#define	IS_SOCKET(statbuf)	S_ISSOCK((statbuf).st_mode)
#ifdef	HAVE_DOOR_CREATE
#define	IS_DOOR(statbuf)	S_ISDOOR((statbuf).st_mode)
#endif

#if	defined(unix) || defined(__unix)

#define	FN_DIRSEPCHR		'/'
#define	FN_DIRSEPSTR		"/"
#define	FN_DIRSEPLEN		1
#define	FN_CURDIR		"."
#define	FN_PARENTDIR		".."
#define	FN_ISPATH(s)		(strchr((s), FN_DIRSEPCHR))
#define	FN_ISABSPATH(s)		(*(s) == FN_DIRSEPCHR)
#define	FN_LASTDIRDELIM(s)	(strrchr((s), FN_DIRSEPCHR))
#define	FN_BASENAME(s)		(strrchr((s), FN_DIRSEPCHR) ? 		\
				(UChar *) strrchr((s), FN_DIRSEPCHR) + 1 : (s))
#define	FN_STRDBLDIRSEP(s)	(strstr((s), "//"))
#define	FN_STREMPTYDIRSEP(s)	(strstr((s), "/./"))
#define	FN_ISDIRSEP(c)		((c) == FN_DIRSEPCHR)
#define	FN_ISROOTDIR(s)		(FN_ISDIRSEP(*(s)) && ! *((s) + 1))
#define	FN_LEADINGDUMMY(s)	(*(s) == '.' && FN_ISDIRSEP(*((s) + 1)))
#define	FN_TRAILINGDUMMY(s)	(strlen(s) >= 2 &&	\
				*((s) + strlen(s) - 1) == '.' &&	\
				FN_ISDIRSEP(*((s) + strlen(s) - 2)))
#define	FN_FIRSTDIRSEP(s)	(strchr((s), FN_DIRSEPCHR))

#define	ENV_PATHSEPCHR		':'

#define	ESCAPE_CHARACTER	'\\'

#define	NULLFILE		"/dev/null"
#ifndef	TMPDIR
#define	FN_TMPDIR		"/tmp"
#else
#define	FN_TMPDIR		TMPDIR
#endif

#define	DEFAULT_LOCKDIRS	{			\
		"/var/locks", "/var/lock", "/var/run",	\
		"/var/spool/locks", "/var/spool/lock", 	\
		"/var/tmp", FN_TMPDIR, NULL,		\
}

#endif	/* defined(unix) || defined(__unix) */

#ifdef	WINDOWS_LIKE

#define	FN_DIRSEPCHR		'\\'
#define	FN_DIRSEPSTR		"\\"
#define	FN_DIRSEPLEN		1
#define	FN_CURDIR		"."
#define	FN_PARENTDIR		".."
#define	FN_ISDIRSEP(c)	((c) == '/' || (c) == '\\')
#define	FN_ISPATH(s)	(strchr((s), '\\') || strchr((s), '/') ||	\
				(isalpha(*(s)) && *((s) + 1) == ':'))
#define	FN_ISABSPATH(s)	(FN_ISDIRSEP(*(s)) ||				\
				(isalpha(*(s)) && *((s) + 1) == ':' &&	\
				FN_ISDIRSEP(*((s) + 2))))
#define	FN_LASTDIRDELIM(s)	(strrchr(s, '/') ? strrchr(s, '/') :	\
				(strrchr(s, '\\') ? strrchr(s, '\\') :	\
				(isalpha(*(s)) && *((s) + 1) == ':') ?	\
				s + 2 : NULL))
#define	FN_BASENAME(s)	(strrchr(s, '/') ? strrchr(s, '/') + 1 :	\
			(strrchr(s, '\\') ? strrchr(s, '\\') + 1 :	\
			(isalpha(*(s)) && *((s) + 1) == ':') ? s + 2 : (s)))
#define	FN_STRDBLDIRSEP(s)						\
		(strstr((s), "//") ? strstr((s), "//") : strstr((s), "\\\\"))
#define	FN_STREMPTYDIRSEP(s)	(strstr((s), "/./") ? strstr((s), "/./") : \
				(strstr((s), "/.\\") ? strstr((s), "/.\\") : \
				(strstr((s), "\\./") ? strstr((s), "\\./") : \
				strstr((s), "\\.\\"))))
#define	FN_ISROOTDIR(s)	((FN_ISDIRSEP(*(s)) && ! *((s) + 1)) ||		\
				(isalpha(*(s)) && *((s) + 1) == ':'	\
				&& FN_ISDIRSEP(*((s) + 2)) && ! *((s) + 3)))
#define	FN_LEADINGDUMMY(s)	(*(s) == '.' && FN_ISDIRSEP(*((s) + 1)))
#define	FN_TRAILINGDUMMY(s)	(strlen(s) >= 2 &&	\
				*((s) + strlen(s) - 1) == '.' &&	\
				FN_ISDIRSEP(*((s) + strlen(s) - 2)))
#define	FN_FIRSTDIRSEP(s)	(strchr((s), '/') ? strchr((s), '/') :	\
					strchr((s), '\\'))

#define	ENV_PATHSEPCHR		';'

#define	ESCAPE_CHARACTER	'\t'	/* don't know, what to do here */

#define	NULLFILE		"nul"
#ifndef	TMPDIR
#define	FN_TMPDIR		"c:\\temp"
#else
#define	FN_TMPDIR		TMPDIR
#endif

#endif	/* defined(WINDOWS_LIKE) */

#endif	/* ! __FILE_UTIL_H */

#ifndef	_FN_MATCH_H

#define	_FN_MATCH_H	_FN_MATCH_H

/* Bits set in the FLAGS argument to `fn_match'.  */
#define	GFNM_PATHNAME	(1 << 0) /* No wildcard can ever match `/'.  */
#define	GFNM_NOESCAPE	(1 << 1) /* Backslashes don't quote special chars.  */
#define	GFNM_PERIOD	(1 << 2) /* Leading `.' is matched only explicitly.  */

#define	GFNM_FILE_NAME	GFNM_PATHNAME /* Preferred GNU name.  */
#define	GFNM_LEADING_DIR	(1 << 3) /* Ignore `/...' after a match.  */
#define	GFNM_CASEFOLD	(1 << 4) /* Compare without regard to case.  */

/* Value returned by `fn_match' if STRING does not match PATTERN.  */
#define	GFNM_NOMATCH	1

#ifdef	__cplusplus
extern "C" {
#endif

/* Match STRING against the filename pattern PATTERN,
   returning zero if it matches, GFNM_NOMATCH if not.  */
extern Int32 fn_match(UChar *, UChar *, Int32);

#ifdef	__cplusplus
}
#endif

#ifdef	DEF_EACCESS
#define	eaccess	e_access
#endif

#if !defined(GFNM_CASEFOLD) && defined(GFNM_IGNORECASE)
#define GFNM_CASEFOLD    GFNM_IGNORECASE
#endif

#endif /* defined(_FN_MATCH_H) */

/************ end of $RCSfile: fileutil.h,v $ ******************/
