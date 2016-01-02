/****************** Start of $RCSfile: conf.h,v $  ****************
*
* $Source: /home/alb/afbackup/afbackup-3.5.8.12/RCS/conf.h,v $
* $Id: conf.h,v 1.3 2012/11/01 09:53:08 alb Exp alb $
* $Date: 2012/11/01 09:53:08 $
* $Author: alb $
*
*
******* description ***********************************************
*
*
*
*******************************************************************/

#ifndef	__CONF_H_
#define	__CONF_H_	__CONF_H_

/* enable 64 bit filesize API if possible */
#define	REQUEST_FILESIZE_64_BIT_API	1

#include <lconf.h>
#include "config.h"

/* Workaround for bug in IRIX headers */
#if	defined(IRIX_6) || defined(IRIX64_6)
#define	_KMEMUSER	1
#endif
/* sometimes not defined by SGI's compilers (?) */
#if	defined(IRIX_5) || defined(IRIX_6) || defined(IRIX64_6)
#ifndef	sgi
#define	sgi	sgi
#endif
#endif

#ifdef	HAVE_SYS_TIME_H
#include <sys/time.h>
#endif
#ifdef	HAVE_SYS_TIME_H
#ifdef	TIME_WITH_SYS_TIME
#include <time.h>
#endif
#endif

#ifdef	HAVE_SYS_PARAM_H
#include <sys/param.h>
#endif
#ifdef HAVE_LIMITS_H
#include <limits.h>
#endif

#ifdef  HAVE_SYS_VFS_H
#include <sys/vfs.h>
#endif
#ifdef  HAVE_SYS_STATFS_H
#include <sys/statfs.h>
#endif

/* cannot use pthread_create cause dumb IRIX will fail compiling */
#ifdef	HAVE_PTHREAD_JOIN
#define	HAVE_POSIX_THREADS	1
#endif

#ifdef	UnixWare_5
#define	REGEX_MALLOC	1
#endif

#ifdef	ENABLE_NLS
#ifdef	HAVE_INTL_LIBINTL_H
#include <intl/libintl.h>
#else
#include <libintl.h>
#endif
#define T_(String) gettext(String)
#ifdef gettext_noop
# define TN_(String) gettext_noop(String)
#else
# define TN_(String) (String)
#endif
#else /* !ENABLE_NLS */
#define T_(String) (String)
#define TN_(String) (String)
#endif /* ENABLE_NLS */

#endif	/* !defined(__CONF_H_) */
