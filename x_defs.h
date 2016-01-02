/****************** Start of $RCSfile: x_defs.h,v $  ****************
*
* $Source: /home/alb/afbackup/afbackup-3.5.8.12/RCS/x_defs.h,v $
* $Id: x_defs.h,v 1.4 2006/12/12 20:21:19 alb Exp alb $
* $Date: 2006/12/12 20:21:19 $
* $Author: alb $
*
*
******* description ***********************************************
*
*
*
*******************************************************************/

#ifndef	_X_DEFS
#define	_X_DEFS	_X_DEFS

#ifdef	_AIX
#ifndef	SYSV
#define	SYSV	SYSV
#endif
#ifndef	unix
#define	unix	unix
#endif
#ifndef	__unix
#define	__unix	__unix
#endif
#endif	/* defined(_AIX) */
#if	defined (__OpenBSD__) || defined (__NetBSD__) || defined(__APPLE__)
#ifndef	unix
#define	unix	unix
#endif
#ifndef	__unix
#define	__unix	__unix
#endif
#endif

#ifdef	unix

#define	far		/**/
#define	_far		/**/
#define	__far		/**/
#define	pascal		/**/
#define	_pascal		/**/
#define	__pascal	/**/
#define	PASCAL		/**/

#ifdef sun
#define	CALC_WITH_Real64	1
#endif
#ifdef __WATCOMC__
#define	CALC_WITH_Real64	1
#endif
#ifdef sgi
#define	CALC_WITH_Real64	1
#endif
#ifdef	_AIX
#define	CALC_WITH_Real64	1
#endif
#if	defined(__hpux) || defined(hpux)
#define	CALC_WITH_Real64	1
#endif
#ifdef linux
#define	CALC_WITH_Real64	1
#endif
#if	defined (__OpenBSD__) || defined (__NetBSD__)
#define	CALC_WITH_Real64	1
#endif
#ifdef UnixWare_5
#define	CALC_WITH_Real64	1
#endif
#ifdef __APPLE__
#define	CALC_WITH_Real64	1
#endif

#endif	/* defined(unix) */

#if defined(_WIN32) && !defined(__WIN32__)
#define	__WIN32__	1
#endif

#if defined(__WINDOWS__) || defined(__MSDOS__) || defined(__DOS__) || defined(__NT__) || defined(__WIN32__)
#define	WINDOWS_LIKE	1
#endif

#if defined(unix) && defined(WINDOWS_LIKE)
#error	FUCK_MICROSOFT
#endif

#endif	/* !_X_DEFS */

/************ end of $RCSfile: x_defs.h,v $ ******************/
