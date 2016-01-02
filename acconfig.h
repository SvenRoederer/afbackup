/****************** Start of $RCSfile: acconfig.h,v $  ****************
*
* $Source: /home/alb/afbackup/afbackup-3.5.8.12/RCS/acconfig.h,v $
* $Id: acconfig.h,v 1.4 2004/07/08 20:34:42 alb Exp alb $
* $Date: 2004/07/08 20:34:42 $
* $Author: alb $
*
*
******* description ***********************************************
*
*
*
*******************************************************************/

/* Define if sys_errlist is defined.  */
#undef HAVE_SYS_ERRLIST_DEF

/* Define to `int' if not defined.  */
#undef timer_t

/* Define to `int' if not defined.  */
#undef signal_t

/* Define for 64 Bit file offsets i.e. sizes on Solaris */
#undef _64_BIT_FILESIZE_

/* Posix thread functions */
#undef HAVE_PTHREAD_JOIN

/* Posix(?) ACL functions */
#undef HAVE_ACL_FREE_TEXT
#undef HAVE_ACL_TO_SHORT_TEXT

#undef ENABLE_NLS
#undef HAVE_CATGETS
#undef HAVE_GETTEXT
#undef HAVE_STPCPY
#undef PACKAGE
#undef VERSION

#undef HAVE_LC_MESSAGES

/* Define if libintl.h is in intl/ */
#undef HAVE_INTL_LIBINTL_H

/* Define if getmntent takes two arguments */
#undef HAVE_GETMNTENT_TWO_ARGS

/* Define if f_bavail is available in struct statfs */
#undef HAVE_STRUCT_STAT_BAVAIL

/* Define if IP 6 implemented on this architecture */
#undef HAVE_IP6

/* Define if environ array abailable */
#undef HAVE_ENVIRON

/* Dummies, dont use. These types should not be defined to some default */
/* #undef int32_t */
/* #undef int16_t */
/* #undef int8_t */
/* #undef uint32_t */
/* #undef uint16_t */
/* #undef uint8_t */
/* #undef u_int32_t */
/* #undef u_int16_t */
/* #undef u_int8_t */
