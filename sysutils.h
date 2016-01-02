/****************** Start of $RCSfile: sysutils.h,v $  ****************
*
* $Source: /home/alb/afbackup/afbackup-3.5.8.12/RCS/sysutils.h,v $
* $Id: sysutils.h,v 1.9 2013/11/08 17:58:05 alb Exp alb $
* $Date: 2013/11/08 17:58:05 $
* $Author: alb $
*
*
******* description ***********************************************
*
*
*
*******************************************************************/

#ifndef __SYSUTILS_H
#define	__SYSUTILS_H	__SYSUTILS_H

#include <signal.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdarg.h>
#include <syslog.h>
#include <netdb.h>

#include <genutils.h>

#ifdef	__FreeBSD__
#ifndef O_SYNC
#define O_SYNC O_FSYNC
#endif
#endif

#ifndef	MAXHOSTNAMELEN
#define	MAXHOSTNAMELEN	200	/* should be almost sufficient */
#endif

typedef struct _complete_user_spec {
  uid_t		uid;
  gid_t		gid;
  int		ngids;
  gid_t		*gids;
} UGIDS;

typedef	struct	__mnt_ent_ {
  dev_t		dev;
  UChar		*dir;
  UChar		*typestr;
  UChar		*devstr;
  Int32		devmode;
  Flag		have_devmode;
} MntEnt;

#ifdef __QNX__

#define	ITIMER_REAL		0		/* Real time */
#define	ITIMER_VIRTUAL		1		/* Per-process time */
#define	ITIMER_PROF		2		/* Per-process user time */

#if 0
/* seems to exist already */
struct timeval {
        int		tv_sec;			/* seconds */
        int		tv_usec;		/* microseconds */
};

struct	itimerval {
	struct		timeval it_interval;	/* timer interval */
	struct		timeval it_value;	/* current value */
};
#endif	/* 0 */

#ifdef	__cplusplus
extern	"C"	{
#endif

extern	int	setitimer(int, struct itimerval *, struct itimerval *);
extern	int	getitimer(int, struct itimerval *);

#ifdef	__cplusplus
}
#endif

#endif	/* defined(__QNX__) */

#ifdef	__cplusplus
extern	"C"	{
#endif

extern	Int32	get_fs_space(UChar *, Real64 *);
extern	Int32	get_fs_status(UChar *, Int32 *, Real64 *, Real64 *, Real64 *);
extern	MntEnt	*get_all_mounts(Int32 *);
extern	MntEnt	*find_mnt_by_devno_dir(MntEnt *, Int32, dev_t, UChar *);
#define	find_mnt_by_devno(mnts, nmnts, devno)	\
			find_mnt_by_devno_dir(mnts, nmnts, devno, NULL)
extern	UChar	*get_fstype_by_devno_dir(dev_t, UChar *);
#define	get_fstype_by_devno(devno)	\
			get_fstype_by_devno_dir(devno, NULL)
extern	void	free_mounts(MntEnt *);
extern	Int32	free_fds();
extern	Flag	is_a_tty(int);
extern	int	open_pty_master(char *);
extern	int	open_pty_slave(int, char *);
extern	void	gvsyslog(UChar *, int, int, int, UChar *, va_list);
extern	void	genlogmsg(UChar *, int, int, UChar *, ...);
extern	int	fd_system_fork(char *, int *);
extern	int	fp_system_fork(char *, FILE **);
extern	int	fdpopen(char *, int, int *);
extern	Int32	bytes_free_real_mem_pag(Int32, Int32);
extern	int	open_to_pipe(UChar *, UChar *, UChar, int *, int);
extern	int	open_from_pipe(UChar *, UChar *, UChar, int *);
extern	Int32	open_from_to_pipe(UChar *, int *, UChar, int *);
extern	int	open_to_pipe_sigblk(UChar *, UChar *, UChar,
					int *, int, sigset_t *);
extern	int	open_from_pipe_sigblk(UChar *, UChar *, UChar,
					int *, sigset_t *);
extern	Int32	open_from_to_pipe_sigblk(UChar *, int *, UChar,
					int *, sigset_t *);
extern	int	open_as_user(char *, int, uid_t, gid_t, ...);
extern	FILE	*fopen_as_user(char *, char *, uid_t, gid_t);
extern	FILE	*popen_as_user(char *, char *, uid_t, gid_t);
extern	int	detach_from_tty();
extern	void	ms_sleep(Int32);
extern	int	set_eff_ugids(uid_t, gid_t, int, gid_t *);
extern	int	switch_to_user_str(UChar *);
extern	int	get_groups(int *, gid_t **);
extern	Int32	set_env(UChar *, UChar *);
extern	Int32	unset_env(UChar *);
extern	int	ign_childs(struct sigaction *);

extern	Int32	to_other_user(uid_t, gid_t, UGIDS *);
extern	Int32	to_org_user(UGIDS *);
extern	int	create_unix_socket(UChar *);
extern	int	open_uxsock_conn(UChar *);
extern	int	run_child1_io(char *, char **);

extern	Uns32	syslog_facility_from_string(UChar *);

#ifdef	__cplusplus
}
#endif

/* according to experience and man-pages, FD_CLOEXEC is
 * always the only bit that can be modified with F_SETFD
 * and it always seems to be defined to 1 */
#ifndef	FD_CLOEXEC
#define	FD_CLOEXEC	1
#endif
#define	set_closeonexec(fd) fcntl(fd, F_SETFD, fcntl(fd, F_GETFD) | FD_CLOEXEC)


#endif	/* ! defined(__SYSUTILS_H) */

/************ end of $RCSfile: sysutils.h,v $ ******************/

