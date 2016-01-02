/****************** Start of $RCSfile: sysutils.c,v $  ****************
*
* $Source: /home/alb/afbackup/afbackup-3.5.8.12/RCS/sysutils.c,v $
* $Id: sysutils.c,v 1.16 2013/11/08 17:58:06 alb Exp alb $
* $Date: 2013/11/08 17:58:06 $
* $Author: alb $
*
*
******* description ***********************************************
*
*
*
*******************************************************************/

#ifdef	linux
#define	_XOPEN_SOURCE
#define _GNU_SOURCE
#endif

#include <conf.h>
#include <version.h>

  static char * fileversion = "$RCSfile: sysutils.c,v $ $Source: /home/alb/afbackup/afbackup-3.5.8.12/RCS/sysutils.c,v $ $Id: sysutils.c,v 1.16 2013/11/08 17:58:06 alb Exp alb $ " PACKAGE " " VERSION_STRING;

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdarg.h>
#include <x_defs.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <poll.h>
#ifdef  HAVE_SYS_WAIT_H
#include <sys/wait.h>
#endif
#ifdef	HAVE_SYS_TIME_H
#include <sys/time.h>
#endif
#ifdef  HAVE_SYS_VFS_H
#include <sys/vfs.h>
#endif
#ifdef	HAVE_SYS_STATVFS_H
#include <sys/statvfs.h>
#endif
#ifdef  HAVE_SYS_STATFS_H
#include <sys/statfs.h>
#endif
#ifdef	HAVE_SYS_FS_TYPES_H
#include <sys/fs_types.h>
#endif
#include <sys/stat.h>
#ifdef	HAVE_MNTENT_H
#include <mntent.h>
#endif
#ifdef	HAVE_TIME_H
#ifdef	TIME_WITH_SYS_TIME
#include <time.h>
#endif
#endif
#ifndef HAVE_TERMIOS_H
# include <termio.h>
#else
# include <termios.h>
#endif /* HAVE_TERMIOS_H */
#ifdef	HAVE_SYS_STROPTS_H
#include <sys/stropts.h>
#endif
#ifdef	HAVE_SYS_PTYIO_H
#include <sys/ptyio.h>
#endif
#ifdef	HAVE_SYS_STRTIO_H
#include <sys/strtio.h>
#endif
#ifdef	HAVE_SYS_PARAM_H
#include <sys/param.h>
#endif
#ifdef	HAVE_SYS_UCRED_H
#include <sys/ucred.h>
#endif
#ifdef	HAVE_SYS_MOUNT_H
#include <sys/mount.h>
#endif
#ifdef	HAVE_SYS_VMOUNT_H
#include <sys/vmount.h>
#endif
#ifdef	HAVE_SYS_MNTCTL_H
#include <sys/mntctl.h>
#endif
#ifdef	HAVE_SYS_MNTTAB_H
#include <sys/mnttab.h>
#endif
#include <syslog.h>
#include <signal.h>
#include <pwd.h>
#include <grp.h>

#include <sysutils.h>
#include <fileutil.h>

#ifndef	HAVE_STRERROR
#include <errno.h>

#ifndef	HAVE_SYS_ERRLIST_DEF
extern	char	*sys_errlist[];
extern	int	sys_nerr;
#endif

char *
strerror(int no)
{
  if(no < sys_nerr && no >= 0)
    return(sys_errlist[no] ? sys_errlist[no] : "Unknown error");

  return("Unknown error");

#if 0
  static char	*msg = "Unknown error (no sys_errlist available)";
  return(msg);
#endif
}

#endif

#ifndef	HAVE_SETITIMER
#include <errno.h>

static Timer	rtimer = -1;

int
setitimer(
    int			which,
    struct itimerval	*timeval,
    struct itimerval	*otimeval)
{
    struct Signal_Event	evp;
    struct itimerval	timer_value, old_timer_value, *timer_ptr, *old_timer_ptr;

    if(which != ITIMER_REAL)
	return(EINVAL);

    if(rtimer == -1){
	evp.sigev_signum = SIGALRM;

	rtimer = timer_make(CLOCK_TYPE_REALTIME, &evp);
	if(rtimer == -1)
	    return(-1);
    }

    timer_ptr = NULL;

    if(timeval){
	timer_value.it_value.tv_sec = timeval->it_value.tv_sec;
	timer_value.it_value.tv_usec = timeval->it_value.tv_usec;
	timer_value.it_interval.tv_sec = timeval->it_interval.tv_sec;
	timer_value.it_interval.tv_usec = timeval->it_interval.tv_usec;

	timer_ptr = &timer_value;
    }

    old_timer_ptr = (otimeval ? &old_timer_value : NULL);

    if(timer_set(rtimer, 0, timer_ptr, old_timer_ptr) != 0)
	return(-1);

    if(otimeval){
	otimeval->it_value.tv_sec = old_timer_value.it_value.tv_sec;
	otimeval->it_value.tv_usec = old_timer_value.it_value.tv_usec;
	otimeval->it_interval.tv_sec = old_timer_value.it_interval.tv_sec;
	otimeval->it_interval.tv_usec = old_timer_value.it_interval.tv_usec;
    }

    return(0);
}

int
getitimer(
    int			which,
    struct itimerval	*timeval)
{
    struct Signal_Event	evp;
    struct itimerval	timer_value;

    if(which != ITIMER_REAL){
	errno = EINVAL;
	return(-1);
    }

    if(!timeval)
	return(0);

    if(rtimer == -1){
	evp.sigev_signum = SIGALRM;

	rtimer = timer_make(CLOCK_REALTIME, &evp);
	if(rtimer != -1)
	    return(-1);
    }

    if(timer_get(rtimer, &timer_value) != 0)
	return(-1);

    if(timeval){
	timeval->it_value.tv_sec = timer_value.it_value.tv_sec;
	timeval->it_value.tv_usec = timer_value.it_value.tv_usec;
	timeval->it_interval.tv_sec = timer_value.it_interval.tv_sec;
	timeval->it_interval.tv_usec = timer_value.it_interval.tv_usec;
    }

    return(0);
}

#endif	/* !defined(HAVE_SETITIMER) */

#include <genutils.h>
#include <x_timer.h>
#include <errno.h>

#define	STOP_TIMERS	setitimer(ITIMER_REAL, &itimer_null, NULL)
#define	START_TIMERS	start_timers()

#define	ADD_TIME(dest, t1, t2)	{ (dest).tv_sec = (t1).tv_sec + (t2).tv_sec; \
				  (dest).tv_usec = \
					(t1).tv_usec + (t2).tv_usec; \
				  if((dest).tv_usec >= 1000000){ \
				    (dest).tv_usec -= 1000000; \
				    (dest).tv_sec++; } }
#define	SUB_TIME(dest, t1, t2)	{ if((t1).tv_usec < (t2).tv_usec){ \
				    (dest).tv_sec = \
					(t1).tv_sec - (t2).tv_sec - 1;\
				    (dest).tv_usec = (t1).tv_usec + 1000000 \
							- (t2).tv_usec; } \
				  else{ \
				    (dest).tv_sec = \
				    (t1).tv_sec - (t2).tv_sec; \
				    (dest).tv_usec = (t1).tv_usec - (t2).tv_usec;}}
#define	CP_TIME(dest, src)	{ (dest).tv_usec = (src).tv_usec; \
				  (dest).tv_sec = (src).tv_sec; }
#define	IS_LATER(now, timer)	((now).tv_sec > (timer).tv_sec ? 1 : \
				  ((now).tv_sec < (timer).tv_sec ? 0 : \
				    ((now).tv_usec >= (timer).tv_usec ? 1 : 0)))
#define	TIMERSIZE	sizeof(TimerDataStruct)

typedef struct _aix_posix_timer {
  int			timer_id;
  struct timeval	exp_time;
  struct timeval	interval;
  signal_t		signal;
  char			repeat;
}	TimerDataStruct;

static	void	(*org_handler)() = SIG_DFL;
static	char	handler_installed = 0;

static	char	timer_signal = 0;

static	int	num_active_timers = 0;
static	int	num_timers = 0;
static	int	max_num_timers = 0;

static	Timer	actid = 2783;

static	TimerDataStruct	*active_timers = NULL;
static	TimerDataStruct	*timers = NULL;
static	TimerDataStruct	tmp_timer;

static struct itimerval		tmp_itimer, loc_itimer;

static struct itimerval itimer_null = { { (int) 0, (int) 0 },
					{ (int) 0, (int) 0 } };

static void
exec_timer(int * act_timer_idx_ptr)
{
  int	i, j, act_timer_idx, sigfl;

  act_timer_idx = *act_timer_idx_ptr;

  sigfl = active_timers[act_timer_idx].signal;

  if(active_timers[act_timer_idx].repeat){
    ADD_TIME(active_timers[act_timer_idx].exp_time,
		active_timers[act_timer_idx].exp_time,
		active_timers[act_timer_idx].interval);

    for(i = act_timer_idx + 1; i < num_active_timers; i++)
	if(IS_LATER(active_timers[i].exp_time,
			active_timers[act_timer_idx].exp_time))
	  break;

    if(i > act_timer_idx + 1){
	memcpy(&tmp_timer, &active_timers[act_timer_idx], TIMERSIZE);
	for(j = act_timer_idx; j < i - 1; j++)
	  memcpy(&active_timers[j], &active_timers[j + 1], TIMERSIZE);
	memcpy(&active_timers[i - 1], &tmp_timer, TIMERSIZE);

	(*act_timer_idx_ptr)--;
    }
  }
  else{
    (*act_timer_idx_ptr)--;
    num_active_timers--;
    for(i = act_timer_idx; i < num_active_timers; i++)
	memcpy(active_timers + i, active_timers + i + 1, TIMERSIZE);
  }

  timer_signal = 1;
  kill(getpid(), sigfl);
  timer_signal = 0;
}

static	void
process_timers()
{
  int		i;
  struct timeval	acttime;
  struct timezone	actzone;


  if(num_active_timers == 0)
    return;

  gettimeofday(&acttime, &actzone);

  for(i = 0; i < num_active_timers; i++){
    if(IS_LATER(acttime, active_timers[i].exp_time)){
	exec_timer(&i);
    }
    else
	break;
  }
}

static int
start_timers()
{
  int		i;
  struct timeval	acttime;
  struct timezone	actzone;

  if(!num_active_timers)
    return(0);

  tmp_itimer.it_interval.tv_sec = 0;
  tmp_itimer.it_interval.tv_usec = 0;

  do{
    i = 0;

    gettimeofday(&acttime, &actzone);

    SUB_TIME(tmp_itimer.it_value, active_timers[0].exp_time, acttime);

    if((tmp_itimer.it_value.tv_sec == 0 && tmp_itimer.it_value.tv_usec == 0)
			|| tmp_itimer.it_value.tv_sec < 0){
	exec_timer(&i);
	i = 1;
    }
  } while(i);

  return(setitimer(ITIMER_REAL, &tmp_itimer, NULL));
}

static	char	handler_active = 0;

static void
handler(signal_t sig)
{
  if(timer_signal){
    if(org_handler != SIG_DFL)
	org_handler(sig);
  }
  else{
    while(handler_active);

    handler_active = 1;

    STOP_TIMERS;

    process_timers();

    START_TIMERS;

    handler_active = 0;
  }
}


Timer
timer_make(ClockID id, struct Signal_Event *sigev)
{
  struct sigaction	siga, osiga;

  if(id != CLOCK_TYPE_REALTIME){
    errno = EINVAL;
    return(-1);
  }

  STOP_TIMERS;

  num_timers++;

  if(num_timers > max_num_timers){
    max_num_timers = num_timers;

    if(timers)
	timers = RENEWP(timers, TimerDataStruct, max_num_timers);
    else
	timers = NEWP(TimerDataStruct, 1);

    if(active_timers)
	active_timers = RENEWP(active_timers, TimerDataStruct,
					max_num_timers);
    else
	active_timers = NEWP(TimerDataStruct, 1);

    if(!timers && ! active_timers){
	errno = ENOMEM;
	return(-1);
    }
  }

  process_timers();

  START_TIMERS;

  tmp_itimer.it_interval.tv_sec = 0;
  tmp_itimer.it_interval.tv_usec = 0;

  if(! handler_installed){
    SETZERO(siga);
    siga.sa_handler = handler;
    sigaction(SIGALRM, &siga, &osiga);

    org_handler = osiga.sa_handler;

    handler_installed = 1;
  }

  actid++;

  timers[num_timers - 1].timer_id = actid;
  timers[num_timers - 1].signal = sigev->sigev_signum;

  return(actid);
}

static void
clear_timer(int idx)
{
  num_active_timers--;

  for(; idx < num_active_timers; idx++)
    memcpy(&active_timers[idx], &active_timers[idx + 1], TIMERSIZE);
}

static int
remove_timer(Timer timerid)
{
  int	i;

  for(i = 0; i < num_timers; i++){
    if(timerid == timers[i].timer_id){
	num_timers--;
	for(; i < num_timers; i++)
	  memcpy(&timers[i], &timers[i + 1], TIMERSIZE);
	return(0);
    }
  }

  errno = EINVAL;

  return(-1);
}

int
timer_del(Timer timerid)
{
  int	i, res;

  res = -1;
  if(num_active_timers > 0){
    for(i = 0; i < num_active_timers; i++){
	if(active_timers[i].timer_id == timerid){
	  STOP_TIMERS;

	  clear_timer(i);

	  START_TIMERS;

	  res = remove_timer(timerid);
	}
    }
  }

  return(res);
}

int
timer_get(Timer timerid, struct itimerval *value)
{
  int	i;
  struct timeval	acttime;
  struct timezone	actzone;

  for(i = 0; i < num_active_timers; i++){
    if(timerid == active_timers[i].timer_id){
	if(active_timers[i].repeat){
	  value->it_interval.tv_usec =
			active_timers[i].interval.tv_usec;
	  value->it_interval.tv_sec =
			active_timers[i].interval.tv_sec;
	}
	else{
	  value->it_interval.tv_sec = 0;
	  value->it_interval.tv_usec = 0;
	}

	gettimeofday(&acttime, &actzone);
	SUB_TIME(tmp_itimer.it_value, active_timers[i].exp_time, acttime);
	if(tmp_itimer.it_value.tv_sec < 0){
	  tmp_itimer.it_value.tv_sec = 0;
	  tmp_itimer.it_value.tv_usec = 0;
	}
	value->it_value.tv_sec = tmp_itimer.it_value.tv_sec;
	value->it_value.tv_usec = tmp_itimer.it_value.tv_usec;

	return(0);
    }
  }

  for(i = 0; i < num_timers; i++)
    if(timerid == timers[i].timer_id)
	break;

  if(i < num_timers)
    return(0);

  return(-1);
}

int
timer_set(
  Timer		timerid,
  int		flags,
  struct itimerval	*value,
  struct itimerval	*ovalue)
{
  struct timeval	acttime;
  struct timezone	actzone;
  int	i, j, tnum;

  for(tnum = 0; tnum < num_timers; tnum++)
    if(timerid == timers[tnum].timer_id)
	break;

  if(tnum >= num_timers){
    errno = EINVAL;
    return(-1);
  };

  memset(&tmp_itimer, 0, sizeof(tmp_itimer));

  loc_itimer.it_interval.tv_sec = value->it_interval.tv_sec;
  loc_itimer.it_interval.tv_usec = value->it_interval.tv_usec;
  loc_itimer.it_value.tv_sec = value->it_value.tv_sec;
  loc_itimer.it_value.tv_usec = value->it_value.tv_usec;

  /*if(!loc_itimer.it_value.tv_sec && !loc_itimer.it_value.tv_usec &&
	(loc_itimer.it_interval.tv_sec || loc_itimer.it_interval.tv_usec)){
    loc_itimer.it_value.tv_sec = loc_itimer.it_interval.tv_sec;
    loc_itimer.it_value.tv_usec = loc_itimer.it_interval.tv_usec;
  }*/

  STOP_TIMERS;

  for(i = 0; i < num_active_timers; i++)
    if(active_timers[i].timer_id == timerid){
	gettimeofday(&acttime, &actzone);

	SUB_TIME(tmp_itimer.it_value, active_timers[i].exp_time, acttime);
	if(active_timers[i].repeat)
	  CP_TIME(tmp_itimer.it_interval, active_timers[i].interval);

	if(tmp_itimer.it_value.tv_sec < 0){
	  tmp_itimer.it_value.tv_sec = 0;
	  tmp_itimer.it_value.tv_usec = 0;
	}

	clear_timer(i);
	break;
    }

  gettimeofday(&acttime, &actzone);

  if(ovalue){
    ovalue->it_interval.tv_sec = tmp_itimer.it_interval.tv_sec;
    ovalue->it_interval.tv_usec = tmp_itimer.it_interval.tv_usec;
    ovalue->it_value.tv_sec = tmp_itimer.it_value.tv_sec;
    ovalue->it_value.tv_usec = tmp_itimer.it_value.tv_usec;
  }

  tmp_timer.timer_id = timerid;

  tmp_timer.repeat = !(loc_itimer.it_interval.tv_sec == 0
				&& loc_itimer.it_interval.tv_usec == 0);

  tmp_timer.signal = timers[tnum].signal;

  tmp_timer.interval.tv_sec = loc_itimer.it_interval.tv_sec;
  tmp_timer.interval.tv_usec = loc_itimer.it_interval.tv_usec;

  if(flags & TIMER_ADDREL){
    ADD_TIME(tmp_timer.exp_time, loc_itimer.it_value, tmp_itimer.it_value);
  }
  else{
    if(flags & TIMER_ABSTIME){
      CP_TIME(tmp_timer.exp_time, loc_itimer.it_value);
    }
    else{
      ADD_TIME(tmp_timer.exp_time, loc_itimer.it_value, acttime);
    }
  }

  if(loc_itimer.it_value.tv_sec || loc_itimer.it_value.tv_usec
	|| loc_itimer.it_interval.tv_sec || loc_itimer.it_interval.tv_usec){
    for(i = 0; i < num_active_timers; i++)
      if(IS_LATER(active_timers[i].exp_time, tmp_timer.exp_time))
	  break;

    for(j = num_active_timers; j > i; j--)
      memcpy(&active_timers[j], &active_timers[j - 1], TIMERSIZE);

    memcpy(&active_timers[i], &tmp_timer, TIMERSIZE);

    num_active_timers++;
  }

  process_timers();

  return(START_TIMERS);
}

int
clock_get(ClockID clock_id, struct timeval *tp)
{
  struct timeval	acttime;
  struct timezone	actzone;

  if(clock_id != CLOCK_TYPE_REALTIME){
    errno = EINVAL;
    return(-1);
  }

  gettimeofday(&acttime, &actzone);

  tp->tv_sec = acttime.tv_sec;
  tp->tv_usec = acttime.tv_usec;

  return(0);
}


#if !defined(WINDOWS_LIKE)

#include <signal.h>
#include <unistd.h>
#include <x_types.h>


#ifndef	NSIG
#ifdef	_SIGMAX
#define	NSIG	_SIGMAX
#else
#ifdef	SIGMAX
#define	NSIG	SIGMAX
#else
#define	NSIG	32
#endif
#endif
#endif

#ifndef HAVE_TERMIOS_H
static struct termio	org_ttyattr;
#else
static struct termios	org_ttyattr;
#endif
static void		(*org_sighandlers[NSIG])(int);
static UChar		signals_set = 0;

static void
reset_signals()
{
  Int32	i;

#ifndef HAVE_TERMIOS_H
  ioctl(0, TCSETA, &org_ttyattr);
#else
  tcsetattr(0, TCSANOW, &org_ttyattr);
#endif

  if(!signals_set)
    return;

  for(i = 0; i < NSIG; i++)
    signal(i, org_sighandlers[i]);

  signals_set = 0;
}

static void
ttsighandl(int sig)
{
  reset_signals();

  if(org_sighandlers[sig])
    org_sighandlers[sig](sig);
  else
    exit(1);
}

Int32
getinchr(UChar * c, Uns32 timeout)
{
#ifndef HAVE_TERMIOS_H
  struct termio		new_ttyattr;
#else
  struct termios	new_ttyattr;
#endif
  Int32		i;
  char		a;
  Uns32		ltime;
  char		stime;

  if(c == NULL)
    return(EINVAL);

  stime = (char)(timeout & 63);
  ltime = timeout / 64 + 1;

#ifndef HAVE_TERMIOS_H
  if(ioctl(0, TCGETA, &org_ttyattr))
#else
  if(tcgetattr(0, &org_ttyattr))
#endif
    return(-1);

  memcpy(&new_ttyattr, &org_ttyattr, sizeof(org_ttyattr));
  if(timeout){
    new_ttyattr.c_cc[VMIN] = '\0';
  }
  else{
    new_ttyattr.c_cc[VMIN] = '\001';
    new_ttyattr.c_cc[VTIME] = '\0';
  }
  new_ttyattr.c_lflag &= ~(ICANON | ECHO);

  for(i = 0; i < NSIG; i++)
    org_sighandlers[i] = signal(i, ttsighandl);
  signals_set = 1;


  do{
    if(timeout){
      if(ltime > 1){
	new_ttyattr.c_cc[VTIME] = (char) 64;
      }
      else{
	new_ttyattr.c_cc[VTIME] = stime;
      }
      ltime--;
    }

#ifndef HAVE_TERMIOS_H
    if(ioctl(0, TCSETA, &new_ttyattr)){
#else
    if(tcsetattr(0, TCSANOW, &new_ttyattr)){
#endif
      reset_signals();
      return(-1);
    }

    i = read(0, &a, 1);
  } while(timeout ? (ltime > 0 && i < 1) : (i < 1));

  reset_signals();

  if(i > 0){
    *c = a;
    return(1);
  }

  return(0);
}

Flag
is_a_tty(int fd)
{
#ifndef HAVE_TERMIOS_H
  struct termio		ttyattrs;
#else
  struct termios	ttyattrs;
#endif

#ifndef HAVE_TERMIOS_H
  if(! ioctl(0, TCGETA, &ttyattrs))
#else
  if(! tcgetattr(0, &ttyattrs))
#endif
    return(YES);

#if	0
  if(errno == EBADF || errno == EINVAL)
    return(DONT_KNOW);
#endif

  return(NO);
}

static char	*ptychars = "0123456789abcdefghijklmn";

static int
open_pty_master1(char * ttyname)
{
  int	master, i, nc;
  char	c, ptys[12], *cptr, slavename[64];

#if	defined(HAVE_GRANTPT) && defined(HAVE_UNLOCKPT)

  if((master = open("/dev/ptmx", O_RDWR)) >= 0){

#ifdef	HAVE_PTSNAME_R

    cptr = (ptsname_r(master, slavename, sizeof(slavename)) ? NULL : slavename);

#else

    cptr = ptsname(master);
    if(cptr)
	strcpy(slavename, cptr);

#endif

    if(!cptr)
	return(-2);

    if(ttyname)
	strcpy(ttyname, slavename);

    return(master);
  }

#else

  strcpy(ptys, "/dev/pty??");
  nc = strlen(ptychars);

  for(c = 'p'; c < 't'; c++){
    for(i = 0; i < nc; i++){
      ptys[8] = c;
      ptys[9] = ptychars[i];

      if((master = open(ptys, O_RDWR)) >= 0){
	if(ttyname){
	  strcpy(ttyname, ptys);
	  ttyname[5] = 't';
	}
 
	return(master);
      }
    }
  }

#endif

  return(-1);
}

int
open_pty_master(char * ttyname)
{
  int	master;

  master = open_pty_master1(ttyname);

#ifdef	I_PUSH
#if	0	/* Enabling packet features breaks TTY function on some OSes */
  if(master >= 0)
    ioctl(master, I_PUSH, "pckt");	/* why the hell must i do this myself */
#endif
#endif

  return(master);
}

int
open_pty_slave(int master, char * ttyname)
{
  int	slave;
  char	*slavename;

#if	defined(HAVE_GRANTPT) && defined(HAVE_UNLOCKPT)

  if(grantpt(master) || unlockpt(master))
    return(-2);

  if((slave = open(ttyname, O_RDWR)) < 0)
    return(-4);

#ifdef	I_PUSH
  ioctl(slave, I_PUSH, "ptem");		/* why the hell must i do this */
  ioctl(slave, I_PUSH, "ldterm");	/* explicitely ??? */
#endif

#else

  if(!ttyname)
    ttyname = "";
  if(!ttyname[0]){
    errno = EINVAL;
    return(-6);
  }

  if ((slave = open(ttyname, O_RDWR)) < 0)
    return(-5);

#endif

  return(slave);
}

Int32
free_fds()
{
  int	*fds = NULL, *newfds = NULL, fd;
  Int32	num_fds = 0;

  while((fd = open(NULLFILE, O_RDONLY)) >= 0){
    newfds = ZSRENEWP(fds, int, num_fds + 1, num_fds);

    if(!newfds){
	close(fd);
	break;
    }
    fds = newfds;

    fds[num_fds] = fd;

    num_fds++;
  }

  for(fd = 0; fd < num_fds; fd++)
    close(fds[fd]);

  free(fds);

  return(num_fds);
}


/* This is for stupid HP's HP-UX-10 header ioctl.h (Grrrrr HP !) */
#if	defined(__hpux) || defined(hpux)
#ifndef	TIOCNOTTY
#define	notdef	1
#undef	_SYS_IOCTL_INCLUDED
#include <sys/ioctl.h>
#ifndef	TIOCNOTTY
#warning Definition of macro TIOCNOTTY cannot be found !
#endif
#endif
#endif

int
detach_from_tty()
{
#ifdef	TIOCNOTTY

  int	fd, r;

  r = 1;

  if((fd = open("/dev/tty", O_RDWR)) >= 0){
    r = ioctl(fd, TIOCNOTTY);
    close(fd);
  }

  return(r);

#else

  return(0);

#endif
}

static void
the_believer()
{
  int	pst;

  /* in this program we buy anything */
  while(waitpid(-1, &pst, WNOHANG) > 0);

  /* Note:
     the man page of waitpid on Linux reads:
       POSIX.1-2001 specifies that if the disposition of SIGCHLD is set to
       SIG_IGN or the SA_NOCLDWAIT flag is set for SIGCHLD (see sigaction(2)),
       then children that terminate do not become zombies and a call to wait()
       or waitpid() will block until all children have terminated, and then
       fail with errno set to ECHILD. (The original POSIX standard left the
       behaviour of setting SIGCHLD to SIG_IGN unspecified.) Linux 2.6 con-
       forms to this specification.

     Basically this is true as long as one is not trying to switch back to the
     original behaviour in a subprocess. Experience shows, that this is not
     (reliably) possible. Sometimes it works, sometimes waitpid does not
     return the pid of a terminated subprocess :-( */
}

int
ign_childs(struct sigaction * rosiga)
{
  struct sigaction	siga, osiga;

  if(!rosiga)
    rosiga = &osiga;

  SETZERO(siga);
  siga.sa_flags = (0
#ifdef	SA_NOCLDWAIT
			| SA_NOCLDWAIT
#endif
#ifdef	SA_NOCLDSTOP
					| SA_NOCLDSTOP
#endif
							);
  siga.sa_handler = the_believer;
  return(sigaction(SIGCHLD, &siga, rosiga));
}

Int32
set_env(UChar * vname, UChar * value)
{
  UChar		*cptr;
  Int32		r;

#ifdef	HAVE_SETENV
  r = setenv(vname, value, 1);
#else
#ifdef	HAVE_ENVIRON
  static Flag	environ_malloced = NO;

  extern char	**environ;
  Int32		nl, n, vl, ol;
  char		**cpptr, **fptr = NULL;
  char		*tmp_newstr;

  r = 0;
  nl = strlen(vname);
  vl = strlen(value);
  for(n = 0, cpptr = environ; *cpptr; cpptr++, n++)
    if(!strncmp(*cpptr, vname, nl))
      if((*cpptr)[nl] == '=')
	fptr = cpptr;

  if(fptr){
	/* Here i do something, that is a tiny bit risky. The problem is
	 * to find out, if an environment entry has been obtained by malloc
	 * or is belonging to the process initialized segments. There are
	 * no reasonable assumptions to determine this from the pointers
	 * there. So i mark the malloced entries, that i make myself, with
	 * a trailing == sequence behind the null-byte terminating the
	 * string. == is a very unlike sequence appearing in any following
	 * environment variables (usually) on the stack. This would require
	 * a variable with the name = or an empty name, in any way a crazy
	 * thing to do, cause a variable like that is not queryable. On the
	 * other side, if i happen to access bytes, that are above the
	 * entire environment area, it is very unlike, that this leads to
	 * a segfault, because usually the memory above the environment is
	 * used for other stuff, so the accesses are valid. If they really
	 * contain ==, bad day. But i never expect this to happen */

    ol = strlen(*fptr);
    if(ol < nl + vl + 1){
      tmp_newstr = NEWP(UChar, nl + 1 + vl + 1 + 2);
      if(!tmp_newstr){
	r = -1;
      }
      else{
	strcpy(tmp_newstr, *fptr);
	strcpy(tmp_newstr + nl + 1, value);
	tmp_newstr[nl + 1 + vl + 1] = '=';
	tmp_newstr[nl + 1 + vl + 2] = '=';

	cptr = NULL;
	if((*fptr)[ol + 1] == '=' && (*fptr)[ol + 2] == '=')
	  cptr = *fptr;
	*fptr = tmp_newstr;
	ZFREE(cptr);
      }
    }
    else{
	strcpy(*fptr + nl + 1, value);
    }
  }
  else{
    cpptr = environ_malloced ? RENEWP(environ, char *, n + 2)
				: NEWP(char *, n + 2);
    if(!cpptr){
	r = -1;
    }
    else{
	if(!environ_malloced)
	  memcpy(cpptr, environ, n * sizeof(char *));

	cpptr[n + 1] = NULL;
	cpptr[n] = strchain(vname, "=", value, NULL);
	if(!*cpptr[n]){
	  free(cpptr);
	  r = -1;
	}
	else{
	  environ = cpptr;
	  environ_malloced = YES;
	}
    }
  }
#else
#error No way to set an environment variable. Failing
  r = -1;
#endif
#endif

  return(r);
}

Int32
unset_env(UChar * vname)
{
  Int32		r = 0;

#ifdef	HAVE_UNSETENV

  unsetenv(vname);

#endif

  /* do the following stuff in any case, even if unsetenv above has been
   * called: unsetenv does on some systems not remove from the environ array,
   * so we must do this ourselves :-( */

#ifdef	HAVE_ENVIRON
 {
  extern char	**environ;
  Int32		l, n;
  char		**cpptr, **fptr, *cptr, **env;

  env = environ;

  l = strlen(vname);

  fptr = NULL;
  for(n = 0, cpptr = env; *cpptr; cpptr++, n++)
    if(!strncmp(*cpptr, vname, l))
      if((*cpptr)[l] == '=')
	fptr = cpptr;

  if(fptr){
    cptr = *fptr;

    l = fptr - env;
    memmove(fptr, fptr + 1, (n - l) * sizeof(char *));

	/* see above in set_env: If after the string and the trailing
	 * null byte two equal signs == follow, this entry has been
	 * malloced by set_env. So we can free it */

    l = strlen(cptr);
    if(cptr[l + 1] == '=' && cptr[l + 2] == '=')
	free(cptr);
  }
 }
#endif

#ifndef	HAVE_ENVIRON
#ifndef	HAVE_UNSETENV
#error No way to set an environment variable. Failing
  r = -1;
#endif
#endif

  return(r);
}


void			/* the super-safe generalized vsyslog */
gvsyslog(
  UChar		*ident,
  int		options,
  int		facility,
  int		prio,
  UChar		*fmt,
  va_list	args)
{
  openlog(ident, options, facility);

#ifdef	HAVE_VSYSLOG

  vsyslog(prio, fmt, args);

#else
#ifdef	HAVE_VSNPRINTF
  {
    UChar	lbuf[10000];	/* we just hope, there is nothing important */
					/* after 9999 characters any more */
    vsnprintf(lbuf, 9998, fmt, args);
    lbuf[9999] = '\0';
    syslog(prio, "%s", lbuf);
  }
#else
  {
    UChar	*line;
    int		fd;
    FILE	*fp;

    fd = tmp_file(NULL);
    if(fd < 0)
	return;
    fp = fdopen(fd, "r+");
    if(!fp){
	close(fd);
	return;
    }
    vfprintf(fp, fmt, args);
    fseek(fp, 0, SEEK_SET);
    while(!feof(fp)){
	line = fget_alloc_str(fp);
	if(!line)
	  continue;
	syslog(prio, "%s", line);
	free(line);
    }
    fclose(fp);
  }
#endif
#endif

  closelog();
}

void
genlogmsg(UChar * ident, int prio, int fac, UChar * fmt, ...)
{
  va_list	args;

  va_start(args, fmt);

  gvsyslog(ident, LOG_PID, fac, prio, fmt, args);

  va_end(args);
}

#if	defined(unix) || defined(__unix) || defined(_AIX)

int
open_as_user(
  char		*file,
  int		flags,
  uid_t		uid,
  gid_t		gid,
  ...)
{
  mode_t	mode;
  UGIDS		orgugids;
  int		fd, org_errno;
  va_list	args;

  va_start(args, gid);

  mode = va_arg(args, mode_t);

  va_end(args);

  org_errno = errno;	/* ignore seteuid errors ... */
  to_other_user(uid, gid, &orgugids);
  errno = org_errno;

  fd = open(file, flags, mode);

  org_errno = errno;
  to_org_user(&orgugids);
  errno = org_errno;

  return(fd);
}

FILE *
fopen_as_user(char * file, char * mode, uid_t uid, gid_t gid)
{
  FILE		*fp;
  int		org_errno;
  UGIDS		orgugids;

  org_errno = errno;	/* ignore seteuid errors ... */
  to_other_user(uid, gid, &orgugids);
  errno = org_errno;

  fp = fopen(file, mode);

  org_errno = errno;
  to_org_user(&orgugids);
  errno = org_errno;

  return(fp);
}

FILE *
popen_as_user(char * cmd, char * mode, uid_t uid, gid_t gid)
{
  FILE		*fp;
  int		org_errno;
  UGIDS		orgugids;

  org_errno = errno;	/* ignore seteuid errors ... */
  to_other_user(uid, gid, &orgugids);
  errno = org_errno;

  fp = popen(cmd, mode);

  org_errno = errno;
  to_org_user(&orgugids);
  errno = org_errno;

  return(fp);
}

int
open_to_pipe(
  UChar		*prog,
  UChar		*filename,
  UChar		fds,
  int		*pidp,
  int		mode)
{
  int		fd = -1, pid, pp[2], i;
  char		**progargv;

  if(!filename && !prog)
    return -1;

  if(filename){
    fd = open(filename, O_WRONLY | O_APPEND | O_CREAT | O_BINARY, mode);
    if(fd < 0)
      return(-1);
  }

  if(prog){
    i = pipe(pp);

    if(i){
      if(filename)
	close(fd);
      return(-1);
    }

    pid = fork_forced();
    if(pid < 0){
      if(filename)
	close(fd);
      return(-1);
    }
    if(!pid){	/* child */
      if( (i = cmd2argvqf(&progargv, prog)) )
	exit(i);

      close(pp[1]);
      if(filename){
	if(fds & 1)
	  dup2(fd, 1);
	if(fds & 2)
	  dup2(fd, 2);
      }
      dup2(pp[0], 0);

      execvp(progargv[0], progargv + 1);
      exit(errno);
    }

    close(pp[0]);
    if(filename)
      close(fd);

    fd = pp[1];
    if(pidp)
	*pidp = pid;
  }

  return(fd);
}  

int
open_from_pipe(UChar * prog, UChar * filename, UChar fds, int * pidp)
{
  int		fd = -1, pid, pp[2], i;
  char		**progargv;

  if(!filename && !prog)
    return -1;

  if(filename){
    fd = open(filename, O_RDONLY | O_BINARY);
    if(fd < 0)
	return(-1);
  }

  if(prog){
    i = pipe(pp);

    if(i){
      if(filename)
	close(fd);
      return(-1);
    }

    pid = fork_forced();
    if(pid < 0){
      if(filename)
	close(fd);
      return(-1);
    }
    if(!pid){	/* child */
      close(pp[0]);

      if( (i = cmd2argvqf(&progargv, prog)) )
	exit(i);

      if(filename){
	dup2(fd, 0);
      }
      if(fds & 1)
	dup2(pp[1], 1);
      if(fds & 2)
	dup2(pp[1], 2);

      execvp(progargv[0], progargv + 1);
      exit(errno);
    }

    close(pp[1]);
    if(filename)
      close(fd);

    fd = pp[0];
    if(pidp)
	*pidp = pid;
  }

  return(fd);
}  

Int32
open_from_to_pipe(UChar * prog, int * pipes, UChar fds, int * pidp)
{
  int		pid, topp[2], frompp[2], i, j;
  char		**progargv;

  if(!prog)
    return -1;

  i = pipe(topp);
  j = pipe(frompp);

  if(i || j){
    if(i){
	close(topp[0]);
	close(topp[1]);
    }
    if(j){
	close(frompp[0]);
	close(frompp[1]);
    }
    return(-1);
  }

  pid = fork_forced();
  if(pid < 0){
    return(-1);
  }
  if(!pid){	/* child */
    close(frompp[0]);
    close(topp[1]);

    if( (i = cmd2argvqf(&progargv, prog)) )
	exit(i);

    dup2(topp[0], 0);
    if(fds & 1)
	dup2(frompp[1], 1);
    if(fds & 2)
	dup2(frompp[1], 2);

    execvp(progargv[0], progargv + 1);
    exit(errno);
  }

  close(frompp[1]);
  close(topp[0]);

  if(pidp)
    *pidp = pid;
  if(pipes){
    pipes[0] = frompp[0];
    pipes[1] = topp[1];
  }

  return(0);
}

int
open_to_pipe_sigblk(
  UChar		*prog,
  UChar		*filename,
  UChar		fds,
  int		*pidp,
  int		mode,
  sigset_t	*sigs)
{
  int		r;
  sigset_t	nsigs, osigs;

  if(prog){
    COPYVAL(nsigs, *sigs);	/* in case it's modified by sigprocmask */
    sigprocmask(SIG_SETMASK, &nsigs, &osigs);
  }

  r = open_to_pipe(prog, filename, fds, pidp, mode);

  if(prog)
    sigprocmask(SIG_SETMASK, &osigs, NULL);

  return(r);
}

int
open_from_pipe_sigblk(
  UChar		*prog,
  UChar		*filename,
  UChar		fds,
  int		*pidp,
  sigset_t	*sigs)
{
  int		r;
  sigset_t	nsigs, osigs;

  if(prog){
    COPYVAL(nsigs, *sigs);	/* in case it's modified by sigprocmask */
    sigprocmask(SIG_SETMASK, &nsigs, &osigs);
  }

  r = open_from_pipe(prog, filename, fds, pidp);

  if(prog)
    sigprocmask(SIG_SETMASK, &osigs, NULL);

  return(r);
}

Int32
open_from_to_pipe_sigblk(
  UChar		*prog,
  int		*pipes,
  UChar		fds,
  int		*pidp,
  sigset_t	*sigs)
{
  int		r;
  sigset_t	nsigs, osigs;

  if(prog){
    COPYVAL(nsigs, *sigs);	/* in case it's modified by sigprocmask */
    sigprocmask(SIG_SETMASK, &nsigs, &osigs);
  }

  r = open_from_to_pipe(prog, pipes, fds, pidp);

  if(prog)
    sigprocmask(SIG_SETMASK, &osigs, NULL);

  return(r);
}

int
fd_system_fork(char * prnam, int * fds)
{
  int	infd[2], outfd[2], errfd[2], i, flags;
  char	*the_shell;

  if(!fds || !prnam){
    errno = EINVAL;
    return(-1);
  }

  if(!(the_shell = getenv("SHELL")))
    the_shell = "/bin/sh";

  i = pipe(infd);
  if(i)
    return(-1);
  i = pipe(outfd);
  if(i)
    return(-1);
  i = pipe(errfd);
  if(i)
    return(-1);

  i = fork();
  if(i < 0)
    return(-1);

  if(i == 0){			/* child */
    if(fds[0])
      dup2(infd[0], 0);
    else
      close(infd[0]);
    if(fds[1])
      dup2(outfd[1], 1);
    else
      close(outfd[1]);
    if(fds[2])
      dup2(errfd[1], 2);
    else
      close(errfd[1]);

    close(infd[1]);
    close(outfd[0]);
    close(errfd[0]);

    flags = fcntl(1, F_GETFL);
    fcntl(1, F_SETFL, flags | O_SYNC);
    flags = fcntl(2, F_GETFL);
    fcntl(2, F_SETFL, flags | O_SYNC);

    execl(the_shell, the_shell, "-c", prnam, NULL);

    fprintf(stderr, T_("Error: could not start subprocess \"%s\".\n"),
			prnam);

    exit(1);
  }

  if(fds[0])
    fds[0] = infd[1];
  else
    close(infd[1]);
  if(fds[1])
    fds[1] = outfd[0];
  else
    close(outfd[0]);
  if(fds[2])
    fds[2] = errfd[0];
  else
    close(errfd[0]);

  close(infd[0]);
  close(outfd[1]);
  close(errfd[1]);

  return(i);
}

int
fp_system_fork(char * pnam, FILE ** fps)
{
  int	fds[3], pid;

  if(!fps || !pnam){
    errno = EINVAL;
    return(-1);
  }

  pid = fd_system_fork(pnam, fds);

  if(pid < 1)
    return(pid);

  fps[0] = fdopen(fds[0], "w");
  fps[1] = fdopen(fds[1], "r");
  fps[2] = fdopen(fds[2], "r");

  if(!fps[0] || !fps[1] || !fps[2]){
    if(fps[0])
	fclose(fps[0]);
    if(fps[1])
	fclose(fps[1]);
    if(fps[2])
	fclose(fps[2]);

    return(-1);
  }

  return(pid);
}

int
fdpopen(char * cmd, int mode, int * pidp)
{
  int	fds[3], pid, rfd;

  rfd = -1;

  switch(mode){
   case O_WRONLY:
    fds[0] = 1;
    fds[1] = 0;
    fds[2] = 0;

    pid = fd_system_fork(cmd, fds);
    if(pid < 0)
	return(-1);

    rfd = fds[0];
    break;

   case O_RDONLY:
    fds[0] = 0;
    fds[1] = 1;
    fds[2] = 0;

    pid = fd_system_fork(cmd, fds);
    if(pid < 0)
	return(-1);

    rfd = fds[1];
    break;

   default:
    errno = EINVAL;
    return(-1);
  }

  if(pidp)
    *pidp = pid;

  return(rfd);
}


/* execution wrapper exiting as soon as the immediate child has exited
 * forwarding file descriptors 0, 1 and 2 */
int
run_child1_io(char * cmd, char **argv)
{
  struct pollfd	pfds[6];
  char		ibuf[1024], obuf[1024], ebuf[1024];
  int		pid, pst, ip[2], op[2], ep[2], fds, ni, no, ne, maxfd, i, n, e;

  ip[0] = ip[1] = op[0] = op[1] = ep[0] = ep[1] = -1;

  if(pipe(ip) || pipe(op) || pipe(ep)){
    if(ip[0] >= 0)
	close(ip[0]);
    if(ip[1] >= 0)
	close(ip[1]);
    if(op[0] >= 0)
	close(op[0]);
    if(op[1] >= 0)
	close(op[1]);
    if(ep[0] >= 0)
	close(ep[0]);
    if(ep[1] >= 0)
	close(ep[1]);
    return(-1);
  }

  pid = fork_forced();
  if(pid < 0)
    return(-1);

  if(!pid){
    dup2(ip[0], 0);
    dup2(op[1], 1);
    dup2(ep[1], 2);
    close(ip[0]);
    close(ip[1]);
    close(op[0]);
    close(op[1]);
    close(ep[0]);
    close(ep[1]);

    execvp(cmd, argv);

    exit(126);
  }

  close(ip[0]);
  close(op[1]);
  close(ep[1]);
  maxfd = MAX(ip[1], op[0]);
  maxfd = MAX(maxfd, ep[0]);
  maxfd = MAX(maxfd, 2);

  fds = 077;
  ni = no = ne = 0;

/* INFO:
  pfds[0].fd = 0;
  pfds[1].fd = 1;
  pfds[2].fd = 2;
  pfds[3].fd = op[0];
  pfds[4].fd = ep[0];
  pfds[5].fd = ip[1];
*/

  while(fds){
    for(i = 0; i < 6; i++){
	pfds[i].fd = -1;
	pfds[i].events = 0;
    }

    if(!ni && !(fds & 01)){
	if(ip[1] >= 0)
	  close(ip[1]);
	ip[1] = -1;
    }
    if((fds & 01) && ni < 1024){
	pfds[0].fd = 0;
	pfds[0].events = POLLIN;
    }
    if((fds & 020) && no < 1024){
	pfds[3].fd = op[0];
	pfds[3].events = POLLIN;
    }
    if((fds & 040) && ne < 1024){
	pfds[4].fd = ep[0];
	pfds[4].events = POLLIN;
    }
    if((fds & 010) && ni > 0){
	pfds[5].fd = ip[1];
	pfds[5].events = POLLOUT;
    }
    if((fds & 02) && no > 0){
	pfds[1].fd = 1;
	pfds[1].events = POLLOUT;
    }
    if((fds & 04) && ne > 0){
	pfds[2].fd = 2;
	pfds[2].events = POLLOUT;
    }

    i = poll(pfds, 6, 70);

    if(i > 0){
      if(POLLINEVENT(pfds[0].revents)){
	n = read(0, ibuf + ni, 1024 - ni);
	if(n > 0)
	  ni += n;
	else
	  fds &= (~ 01);
      }

      if(ip[1] >= 0){
	if(POLLOUTEVENT(pfds[5].revents)){
	  n = write(ip[1], ibuf, ni);
	  if(n > 0){
	    ni -= n;
	    memmove(ibuf, ibuf + n, ni);
	  }
	  else{
	    fds &= (~ 010);
	  }
	}
      }

      if(POLLOUTEVENT(pfds[1].revents)){
	n = write(1, obuf, no);
	if(n > 0){
	  no -= n;
	  memmove(obuf, obuf + n, no);
	}
	else{
	  fds &= (~ 02);
	}
      }

      if(POLLOUTEVENT(pfds[2].revents)){
	n = write(2, ebuf, ne);
	if(n > 0){
	  ne -= n;
	  memmove(ebuf, ebuf + n, ne);
	}
	else{
	  fds &= (~ 04);
	}
      }

      if(POLLINEVENT(pfds[3].revents)){
	n = read(op[0], obuf + no, 1024 - no);
	if(n > 0){
	  no += n;
	}
	else{
	  fds &= (~ 020);
	  close(op[0]);
	}
      }

      if(POLLINEVENT(pfds[4].revents)){
	n = read(ep[0], ebuf + ne, 1024 - ne);
	if(n > 0){
	  ne += n;
	}
	else{
	  fds &= (~ 040);
	  close(ep[0]);
	}
      }
    }
    else{
      if(pid <= 0){
	close(op[0]);
	close(ep[0]);
	fds &= (~ (020 | 040));
	break;
      }
      else{
	n = waitpid(pid, &pst, WNOHANG);
	e = errno;
	if(n == pid || (n < 0 && e == ECHILD && kill(pid, 0) != 0)){
	  if(ip[1] >= 0)
	    close(ip[1]);
	  ip[1] = -1;
	  fds &= (~ (010 | 01));
	  pid = 0;
	  continue;
	}
      }
    }
  }

  if(pid > 0)
    waitpid_forced(pid, &pst, 0);

  while(no > 0 || ne > 0){
    if(no > 0){
	n = write(1, obuf, no);
	if(n > 0){
	  n -= no;
	  memmove(obuf, obuf + n, no);
	}
    }
    if(ne > 0){
	n = write(2, ebuf, ne);
	if(n > 0){
	  n -= ne;
	  memmove(ebuf, ebuf + n, ne);
	}
    }
  }

  return(WEXITSTATUS(pst));
}

#endif	/* defined(unix) || defined(__unix) || defined(_AIX) */

Int32
bytes_free_real_mem_pag(Int32 chunksize, Int32 maxsize)
{
  Real64	avg_duration, total_duration, timediff;
  Int32		nmeasured, newsize;
  Int32		allocated = 0;
  UChar		*mem = NULL, *newmem;
  time_t	start_time, end_time;
  struct timeval	start_tod, end_tod;

  avg_duration = total_duration = 0.0;
  nmeasured = 0;

  if(maxsize / chunksize < 8)
    maxsize = chunksize * 8;

  forever{
    do{
	start_time = time(NULL);
	gettimeofday(&start_tod, NULL);
    }while(start_time != time(NULL));

    if((newsize = allocated + chunksize) > maxsize)
	break;

    newmem = ZRENEWP(mem, UChar, newsize);
    if(!newmem)
	break;

    mem = newmem;
    memset(mem + allocated, 0x5a, chunksize);

    do{
	end_time = time(NULL);
	gettimeofday(&end_tod, NULL);
    }while(end_time != time(NULL));

    timediff = (Real64) (end_tod.tv_sec - start_tod.tv_sec) +
		(Real64) (end_tod.tv_usec - start_tod.tv_usec) / 1000000.0;
    if(timediff < 0.0)
	timediff += 86400.0;

    if(nmeasured == 0){
	avg_duration = total_duration =  timediff;
    }
    else{
	if(timediff > avg_duration * 5.0 && nmeasured >= 8)
	  break;
	total_duration += timediff;
	avg_duration = total_duration / (Real64) nmeasured;
    }
	
    nmeasured++;
    allocated = newsize;
  }

  ZFREE(mem);

  return(allocated);
}



#ifndef _WIN32	/* here we believe in the maximum thinkable BS occuring */

Int32
get_fs_space(UChar * path, Real64 * bsize)
{
  Real64	freeblocks, availblocks;
  Int32		blocksize, i;

  if(!bsize){
    errno = EINVAL;
    return(-1);
  }

  i = get_fs_status(path, &blocksize, NULL, &freeblocks, &availblocks);
  if(i)
    return(i);

  *bsize = (Real64) blocksize * (getuid() ? availblocks : freeblocks);

  return(0);
}

Int32
get_fs_status(
  UChar		*path,
  Int32		*blocksize,
  Real64	*bsize,
  Real64	*bfree,
  Real64	*bavail)
{
#ifndef	HAVE_STATVFS
  struct statfs	statfsb;
#else
  struct statvfs	statfsb;
#endif

#ifdef	HAVE_STATVFS		/* sees to be well-defined */
	  if(statvfs(path, &statfsb) == -1)
#else
#if defined(_AIX) || defined(linux) || defined(__hpux) || defined(hpux) || defined(__FreeBSD__) || ( defined(__GLIBC__) && defined(__FreeBSD_kernel__) ) || defined(__OpenBSD__) || defined(__NetBSD__)
	  if(statfs(path, &statfsb) == -1)
#else
#if defined(sun)
	  if(statfs(path, &statfsb, sizeof(statfsb), 0) == -1)
#else
#if defined(sgi)
	  if(statfs(path, &statfsb, sizeof(statfsb), 0) == -1)
#else
#if defined(__osf__)
	  if(statfs(path, &statfsb, sizeof(statfsb)) == -1)
#else
#if defined(UnixWare_5)
	  if(statfs(path, &statfsb) == -1)
#else
#error	undefined architecture
#endif
#endif
#endif
#endif
#endif
#endif
	    return(-1);

  if(bavail)
#if	defined(HAVE_STRUCT_STAT_BAVAIL) || defined(HAVE_STATVFS)
	*bavail = (Real64) statfsb.f_bavail;
#else
	*bavail = (Real64) statfsb.f_bfree;
#endif
  if(bfree)
	*bfree = (Real64) statfsb.f_bfree;
  if(bsize)
	*bsize = (Real64) statfsb.f_blocks;
  if(blocksize)
#ifndef	HAVE_STATVFS
	*blocksize = (Int32) statfsb.f_bsize;
#else
	*blocksize = (Int32) statfsb.f_frsize;
#endif

  return(0);
}

#else	/* defined(_WIN32) */

Int32
get_fs_space(UChar * path, Real64 * size)
{
  if(size)
    *size = 10000000.0;

  return(0);
}

#endif	/* ifelse defined(_WIN32) */

static int
set_eff_ugids1(uid_t uid, gid_t gid, int ngids, gid_t * gids, Flag ignerr)
{
  int		r = 0, ncgids = 0;
  uid_t		ceuid;
  gid_t		cegid, *cgids = NULL;

  ceuid = geteuid();
  cegid = getegid();

  if((ngids > 0 && gids) || (gid != (gid_t) -1)){
    if(ceuid){
#ifdef	HAVE_SETEUID	/* if that works, we can force group setting */
	seteuid(0);
#else
#ifdef	HAVE_SETREUID
	setreuid(-1, 0);
#else
#ifdef	HAVE_SETRESUID
	setresuid(-1, 0, -1);
#else
#error	No function to set the effective user id
#endif
#endif
#endif

	if(!(geteuid())){
	  if(uid == (uid_t) -1)
	    uid = ceuid;		/* to really go back later */
	  ceuid = 0;
	}
    }
  }

  if(ngids > 0 && gids){
    if(get_groups(&ncgids, &cgids) && !ignerr)
	GETOUTR(-1);

    r = setgroups(ngids, gids);
    if(r && !ignerr)
	GETOUTR(r);
  }

  if(gid != (gid_t) -1 && gid != cegid){
    r =

#ifdef	HAVE_SETEGID
	setegid(gid);
#else
#ifdef	HAVE_SETREGID
	setregid(-1, gid);
#else
#ifdef	HAVE_SETRESGID
	setresgid(-1, gid, -1);
#else
#error	No function to set the effective group id
#endif
#endif
#endif

    if(r && !ignerr)
	GETOUTR(r);
  }

  if(uid != (uid_t) -1 && uid != ceuid){

    r =

#ifdef	HAVE_SETEUID
	seteuid(uid);
#else
#ifdef	HAVE_SETREUID
	setreuid(-1, uid);
#else
#ifdef	HAVE_SETRESUID
	setresuid(-1, uid, -1);
#else
#error	No function to set the effective user id
#endif
#endif
#endif

    if(r && !ignerr)
	GETOUTR(r);
  }

 cleanup:
  ZFREE(cgids);

  return(r);

 getout:
  set_eff_ugids1(ceuid, cegid, ncgids, cgids, YES);

  CLEANUP;
}

int
set_eff_ugids(uid_t uid, gid_t gid, int ngids, gid_t * gids)
{
  return(set_eff_ugids1(uid, gid, ngids, gids, NO));
}

int
switch_to_user_str(UChar * username)
{
  char			*cptr, *groupname = NULL, *secgroups = NULL;
  int			r = 0;
  struct passwd		*pwdentry = NULL;
  struct group		*grentry;
  gid_t			*gr = NULL;
  int			uid, gid, n, ngr;
  Flag			have_uid = NO, have_gid = NO;

  username = strdup(username);
  if(!username)
    return(-1);

  forever{
    cptr = first_space(username);
    if(!cptr)
	break;
    if(!*cptr)
	break;
    memmove(cptr, cptr + 1, strlen(cptr /* + 1 */) /* + 1 */);
  }

  while( (cptr = strchr(username, '.')) )
    *cptr = ':';
  while( (cptr = strchr(username, ',')) )
    *cptr = ':';

  cptr = strchr(username, ':');
  if(cptr){
    *cptr = '\0';
    groupname = cptr + 1;
  }

  n = 0;
  sscanf(username, "%d%n", &uid, &n);
  if(n == strlen(username)){
    have_uid = YES;
    pwdentry = getpwuid(uid);
  }
  else{
    pwdentry = getpwnam(username);
    if(!pwdentry)
	CLEANUPR(-3);
    have_uid = YES;
    uid = pwdentry->pw_uid;	/* set the correct one, just in case the sscanf */
  }	/* above was able to read some number e.g. with an accoun name 90er */

  if(groupname){
    secgroups = strchr(groupname, ':');
    if(secgroups)
	*(secgroups++) = '\0';

    n = 0;
    sscanf(groupname, "%d%n", &gid, &n);
    if(n < strlen(groupname)){
      grentry = getgrnam(groupname);
      if(!grentry)
	CLEANUPR(-4);

      gid = grentry->gr_gid;
    }
    have_gid = YES;
  }

  if(! have_gid){
    if(!pwdentry){
	pwdentry = getpwnam(username);
	if(!pwdentry)
	  CLEANUPR(-3);
    }

    gid = pwdentry->pw_gid;
    have_gid = YES;
  }

  if(! have_uid)
    uid = pwdentry->pw_uid;

  ngr = 0;
  if(!(gr = NEWP(gid_t, 1)))
	CLEANUPR(-10);

  if(secgroups){
    do{
	cptr = strchr(secgroups, ':');
	if(cptr)
	  *(cptr++) = '\0';

	if(secgroups[0]){
	  if(!(gr = RENEWP(gr, gid_t, ngr + 1)))
	    CLEANUPR(-11);

	  sscanf(secgroups, "%d%n", gr + ngr, &n);
	  if(n < strlen(secgroups)){
	    grentry = getgrnam(secgroups);
	    if(!grentry)
		CLEANUPR(-4);

	    gr[ngr] = grentry->gr_gid;
	  }

	  ngr++;
	}

	secgroups = cptr;
    } while(cptr);
  }
 
  if(setgroups(ngr, gr))
    CLEANUPR(-6);

  if(setgid(gid))
    CLEANUPR(-7);

  if(set_eff_ugids(-1, gid, 0, NULL))
    CLEANUPR(-8);

  if(setuid(uid))
    CLEANUPR(-9);

 cleanup:
  if(r && r > -6)
    errno = EINVAL;

  free(username);
  ZFREE(gr);
  return(r);
}

int
get_groups(int * ngids, gid_t ** gids)
{
  int		ngs;
  gid_t		*gs, g;

  ngs = getgroups(0, &g);
  if(!gids){
    if(ngids)
	*ngids = ngs;
    return(0);
  }

  gs = NEWP(gid_t, ngs);
  if(!gs)
    return(-1);

  getgroups(ngs, gs);

  *gids = gs;
  if(ngids)
    *ngids = ngs;

  return(0);
}

static int
cmp_mnt_by_devno(void * ptr1, void * ptr2)
{
  dev_t		d1, d2;

  d1 = ((MntEnt *) ptr1)->dev;
  d2 = ((MntEnt *) ptr2)->dev;

  return(d1 > d2 ? 1 : (d1 < d2 ? -1 : 0));
}

UChar *
mount_tab_file()
{
#if	defined(linux) || defined(sgi) || ( defined(sun) && ! defined(HAVE_GETMNTENT_TWO_ARGS) ) || ( defined(__GLIBC__) && defined(__FreeBSD_kernel__) ) || defined(__CYGWIN32__) 
#define	MTABFILE	"/etc/mtab"
#else
#if	defined(sun) || defined(__hpux) || defined(hpux) || defined(UnixWare_5)
#define	MTABFILE	"/etc/mnttab"
#else
#if	defined(__osf__) || defined(__FreeBSD__) || defined(__OpenBSD__) || defined(__NetBSD__) || defined(_AIX) || defined(__APPLE__)
#define	MTABFILE	NULL
#endif
#endif
#endif

  return(MTABFILE);
}

#ifndef	HAVE_ENDMNTENT
#define	endmntent	fclose
#endif
#ifndef	HAVE_SETMNTENT
#define	setmntent	fopen
#endif

MntEnt *
get_all_mounts(Int32 * num_mounts)
{
  Int32		nmnt = 0, n, l, i;
  UChar		*dir, *typestr, *devstr, *cptr;
  MntEnt	*mnts = NULL;
  FILE		*mtabfp = NULL;
  struct stat	statb;
#ifdef	HAVE_GETMNTENT
#ifndef	HAVE_GETMNTENT_TWO_ARGS
  struct mntent	*mntentptr;
#else
  struct mnttab	mntent;
#endif
#endif
#ifdef	HAVE_GETFSSTAT
  struct statfs	*statfsptr, *statfsbuf = NULL;
#endif
  UChar		*mntentbuf = NULL, **vfslines = NULL;
  int		i1;
  Int32		nvfslines = 0, nmntent;		/* this stuff is for AIX */

#ifdef	HAVE_GETMNTENT
  mtabfp = setmntent(MTABFILE, "r");
  if(!mtabfp)
    return(NULL);
#else	/* defined(HAVE_GETMNTENT) */
#ifdef	HAVE_MNTCTL			/* the AIX stuff */
  n = 8;		/* die schiessen echt den Vogel ab */
  forever{
    mntentbuf = ZRENEWP(mntentbuf, UChar, n);
    if(!mntentbuf)
	GETOUT;

    nmntent = mntctl(MCTL_QUERY, n, mntentbuf);
    if(nmntent < 0)
	GETOUT;
    if(nmntent == 0){
	n = *((size_t *) &(mntentbuf[0]));
	continue;
    }

    break;
  }

  vfslines = read_asc_file("/etc/vfs", &nvfslines);
  if(!vfslines)
    GETOUT;
  for(i = 0; i < nvfslines; i++){
    massage_string(vfslines[i]);
    if( (cptr = first_space(vfslines[i])) )
	*cptr = '\0';
  }
#else
#ifdef	HAVE_GETFSSTAT
  n = getfsstat(NULL, 0, MNT_NOWAIT);
  statfsbuf = NEWP(struct statfs, n);
  if(!statfsbuf)
    GETOUT;
  mntentbuf = (UChar *) statfsbuf;	/* for free below */

  if((nmntent = getfsstat(statfsbuf, n * sizeof(struct statfs), MNT_NOWAIT)) < 0)
    GETOUT;

#else
#error	No way to get the (status of the) mounted filesystems
#endif
#endif
#endif

#ifdef	HAVE_GETMNTENT
#ifndef	HAVE_GETMNTENT_TWO_ARGS
  while( (mntentptr = getmntent(mtabfp)) ){
    dir = mntentptr->mnt_dir;
#else
  while(!getmntent(mtabfp, &mntent)){
    dir = mntent.mnt_mountp;
#endif
#else	/* defined(HAVE_GETMNTENT) */
#ifdef	HAVE_MNTCTL
  for(n = 0, cptr = mntentbuf; n < nmntent;
		cptr += ((struct vmount *) cptr)->vmt_length, n++){
    devstr = cptr + sizeof(struct vmount);
    dir = devstr + align_n(strlen(devstr) + 1, 32 / 8);
#else
#ifdef	HAVE_GETFSSTAT
  for(n = 0, statfsptr = statfsbuf; n < nmntent; n++, statfsptr++){
    dir = statfsptr->f_mntonname;
#endif
#endif
#endif

    if(lstat(dir, &statb) < 0)
	continue;		/* can't read this -> leave out */

#ifdef	HAVE_GETMNTENT
#ifndef	HAVE_GETMNTENT_TWO_ARGS
    typestr = mntentptr->mnt_type;
    devstr = mntentptr->mnt_fsname;
#else
    typestr = mntent.mnt_fstype;
    devstr = mntent.mnt_special;
#endif
#else	/* defined(HAVE_GETMNTENT) */
#ifdef	HAVE_MNTCTL		/* on AIX we must search in /etc/vfs */
    for(i = 0; i < nvfslines; i++){
	if(vfslines[i][0] == '#' || vfslines[i][0] == '%' || !vfslines[i][0])
	  continue;

	l = strlen(vfslines[i]);
	if(sscanf(vfslines[i] + l + 1, "%d", &i1) < 1)
	  continue;
	if(((struct vmount *) cptr)->vmt_gfstype == i1){
	  typestr = vfslines[i];
	  break;
	}
    }
    if(i >= nvfslines)
	continue;
#else
#ifdef	HAVE_GETFSSTAT
    devstr = statfsptr->f_mntfromname;
#ifdef	HAVE_MNT_NAMES
    typestr = mnt_names[statfsptr->f_type];
#else
    typestr = statfsptr->f_fstypename;
#endif
#endif
#endif
#endif

    mnts = ZRENEWP(mnts, MntEnt, nmnt + 2);
    if(!mnts)
	CLEANUP;
    memset(mnts + nmnt, 0, sizeof(MntEnt) * 2);

    mnts[nmnt].dev = statb.st_dev;
    if(!(mnts[nmnt].dir = strdup(dir)))
	GETOUT;
    if(typestr)
      if(!(mnts[nmnt].typestr = strdup(typestr)))
	GETOUT;
    if(devstr)
      if(!(mnts[nmnt].devstr = strdup(devstr)))
	GETOUT;
    i = stat(devstr, &statb);
    mnts[nmnt].have_devmode = (i < 0 ? NO : YES);
    mnts[nmnt].devmode = statb.st_mode;

    nmnt++;
  }

  if(num_mounts)
    *num_mounts = nmnt;

  q_sort(mnts, nmnt, sizeof(MntEnt), cmp_mnt_by_devno);

 cleanup:
  if(mtabfp){
    endmntent(mtabfp);
  }
  ZFREE(mntentbuf);
  free_asc_file(vfslines, 0);

  return(mnts);

 getout:
  free_mounts(mnts);
  CLEANUP;
}

static void
cry_out_loud_shit(UChar * msg, MntEnt * mnts, Int32 nmnts)
{
  fprintf(stderr, ">>>>> Please, if someone reads this, send a mail to af@muc.de\n"
		">>>>> containing the following output:\n");
  fprintf(stderr, ">>>>> %s\n", msg);
  while(nmnts > 0){
    fprintf(stderr, ">>>>> `%s' `%s' `%s' %ld\n",
		mnts->dir ? mnts->dir : (UChar *) "??",
		mnts->typestr ? mnts->typestr : (UChar *) "??",
		mnts->devstr ? mnts->devstr : (UChar *) "??",
		(long int) mnts->dev);
    mnts++;
    nmnts--;
  }
}

MntEnt *
find_mnt_by_devno_dir(MntEnt * mounts, Int32 nmnt, dev_t devno, UChar * dnam)
{
  MntEnt	cmpent, *mntptr;
  Int32		first, last, found, i, n, l;

  if(dnam)
    dnam = resolve_path__(dnam, NULL);

  if(nmnt < 1)
    for(nmnt = 0, mntptr = mounts; mntptr->dir; nmnt++, mntptr++);

  cmpent.dev = devno;

  mntptr = b_search(&cmpent, mounts, nmnt, sizeof(MntEnt), cmp_mnt_by_devno);

  if(mntptr){
    found = first = last = mntptr - mounts;	/* there can be several with */
    while(first > 0){				/* the same device number */
	if(mounts[first - 1].dev != devno)	/* when there are loopback */
	  break;				/* mounts. We have to find */
						/* the entry with a real */
	first--;				/* device as device, or */
    }						/* we are at the end of the */
    while(last < nmnt - 1){			/* loop searching for the */
	if(mounts[last + 1].dev != devno)	/* entry with the device */
	  break;				/* as directory */

	last++;
    }

    if(first < last){
#if 0	/* all that nice stuff does not work in practice */
	forever{
	  if(! mounts[found].have_devmode)
	    break;	/* may never happen */

	  if(S_ISBLK(mounts[found].devmode) || S_ISCHR(mounts[found].devmode))
	    break;				/* we have a device here */

	  for(i = first; i <= last; i++){
	    if(i == found)
		continue;
	    if(!strcmp(mounts[found].devstr, mounts[i].dir))
		break;	/* have a new entry with dir == dev of old entry */
	  }

	  if(i <= last)
	    found = i;		/* continue with the found entry */
	  else
	    break;	/* if none found, we are at the end, no clue left */
	}		/* stick with the entry found before (?) */
#else	/* instead simply look for a real device */
	n = 0;
	for(i = first; i <= last; i++){
	  if(!mounts[i].devstr || !mounts[i].typestr)
	    continue;

	  if(! mounts[found].have_devmode)
	    continue;

	  if(S_ISBLK(mounts[found].devmode) || S_ISCHR(mounts[found].devmode)){
	    if(dnam && mounts[i].dir){
		l = strlen(mounts[i].dir);
		if(!strcmp(dnam, mounts[i].dir)
			|| (!strncmp(dnam, mounts[i].dir, l)
					 && FN_ISDIRSEP(dnam[l]))){
		  n++;
		  found = i;
		}
	    }
	    else{
		n++;
		found = i;
	    }
	  }
	}
	if(n > 1)
		cry_out_loud_shit("several devices",
			mounts + first, last - first + 1);

	if(n == 0){	/* another perverse thing, hope it only happens on Sun */
	  n = 0;	/* a supervised automount point and the real mount */
	  for(i = first; i <= last; i++){	/* have the same device number */
	    if(!strstr(mounts[i].typestr, "auto")){	/* we prefer non-auto */
	      if(dnam && mounts[i].dir){	/* i *hate* such hardcoded stuff */
		l = strlen(mounts[i].dir);
		if(!strcmp(dnam, mounts[i].dir)
			|| (!strncmp(dnam, mounts[i].dir, l)
					 && FN_ISDIRSEP(dnam[l]))){
		  n++;
		  found = i;
		}
	      }
	      else{
		n++;
		found = i;
	      }
	    }
	  }

#ifdef	sun		/* ZFS does not show a real device in df. Humpf. So if */
	  if(n > 1){	/* there's one and only ZFS in the list, that's it */
	    n = 0;
	    for(i = first; i <= last; i++){
	      if(!strcmp(mounts[i].typestr, "zfs")){
		n++;
		found = i;
	      }
	    }
	  }
#endif

	  if(n > 1)
		cry_out_loud_shit("several non-automount directories",
			mounts + first, last - first + 1);
	}
#endif

	mntptr = mounts + found;
    }
  }

  ZFREE(dnam);

  return(mntptr);
}

UChar *
get_fstype_by_devno_dir(dev_t devno, UChar * dnam)	/* this is not MT-safe */
{
  static MntEnt	*mounts = NULL;
  static Int32	nmounts = 0;
  static UChar	*prev_type = NULL;
  static dev_t	prev_dev;

  MntEnt	*mntptr;

  if(prev_type && prev_dev == devno)
    return(prev_type);

  if(!mounts){
    mounts = get_all_mounts(&nmounts);
    if(!mounts)
	return(NULL);
  }

  mntptr = find_mnt_by_devno_dir(mounts, nmounts, devno, dnam);
  if(!mntptr){
    free_mounts(mounts);
    mounts = get_all_mounts(&nmounts);
    if(!mounts)
	return(NULL);

    mntptr = find_mnt_by_devno_dir(mounts, nmounts, devno, dnam);
  }

  prev_dev = devno;
  prev_type = (mntptr ? mntptr->typestr : NULL);
  return(prev_type);
}

void
free_mounts(MntEnt * mounts)
{
  MntEnt	*mntptr;

  if( (mntptr = mounts) ){
    while(mounts->dir){
	free(mounts->dir);
	ZFREE(mounts->typestr);
	ZFREE(mounts->devstr);

	mounts++;
    }

    free(mntptr);
  }
}

static struct _fac_names_ {
  char	*name;
  int	value;
} lfacilitynames[] = {
#ifdef	LOG_AUTH
    { "auth", LOG_AUTH },
#endif
#ifdef	LOG_AUTHPRIV
    { "authpriv", LOG_AUTHPRIV },
#endif
#ifdef	LOG_CRON
    { "cron", LOG_CRON },
#endif
#ifdef	LOG_DAEMON
    { "daemon", LOG_DAEMON },
#endif
#ifdef	LOG_FTP
    { "ftp", LOG_FTP },
#endif
#ifdef	LOG_KERN
    { "kern", LOG_KERN },
#endif
#ifdef	LOG_LPR
    { "lpr", LOG_LPR },
#endif
#ifdef	LOG_MAIL
    { "mail", LOG_MAIL },
#endif
#ifdef	LOG_NEWS
    { "news", LOG_NEWS },
#endif
#ifdef	LOG_AUTH
    { "security", LOG_AUTH },		/* DEPRECATED */
#endif
    { "syslog", LOG_SYSLOG },	/* always there !!! wow !!! */
#ifdef	LOG_USER
    { "user", LOG_USER },
#endif
#ifdef	LOG_UUCP
    { "uucp", LOG_UUCP },
#endif
#ifdef	LOG_LOCAL0
    { "local0", LOG_LOCAL0 },
#endif
#ifdef	LOG_LOCAL1
    { "local1", LOG_LOCAL1 },
#endif
#ifdef	LOG_LOCAL2
    { "local2", LOG_LOCAL2 },
#endif
#ifdef	LOG_LOCAL3
    { "local3", LOG_LOCAL3 },
#endif
#ifdef	LOG_LOCAL4
    { "local4", LOG_LOCAL4 },
#endif
#ifdef	LOG_LOCAL5
    { "local5", LOG_LOCAL5 },
#endif
#ifdef	LOG_LOCAL6
    { "local6", LOG_LOCAL6 },
#endif
#ifdef	LOG_LOCAL7
    { "local7", LOG_LOCAL7 },
#endif
    { NULL, -1 }
  };

Uns32
syslog_facility_from_string(UChar * str)
{
  struct _fac_names_  *fnamesptr;

  for(fnamesptr = lfacilitynames; fnamesptr->name; fnamesptr++)
    if(!strcmp(fnamesptr->name, str))
	break;

  return(fnamesptr->value);
}

Int32
to_other_user(
  uid_t		vuid,
  gid_t		vgid,
  UGIDS		*old_ugids)
{
  Int32		i;

  if(old_ugids){
    old_ugids->uid = geteuid();
    old_ugids->gid = getegid();

    ZFREE(old_ugids->gids);

    i = get_groups(&(old_ugids->ngids), &(old_ugids->gids));
    if(i)
	return(i);
  }

  return(set_eff_ugids(vuid, vgid, 0, &vgid));
}

Int32
to_org_user(UGIDS * old_ugids)
{
  Int32		i;

  i = set_eff_ugids(old_ugids->uid, old_ugids->gid, old_ugids->ngids, old_ugids->gids);
  if(!i){
    ZFREE(old_ugids->gids);
    SETZERO(*old_ugids);
  }

  return(i);
}

int
create_unix_socket(UChar * path)
{
  struct sockaddr_un	addr;
  struct stat	statb;
  int	usock, sock_optval;

  if(!stat(path, &statb)){
    usock = open_uxsock_conn(path);
    if(usock >= 0){
	close(usock);
	errno = EADDRINUSE;
	return(-2);
    }
    unlink(path);
  }

  if(mkbasedir(path, 0755, (uid_t) -1, (gid_t) -1) < 0)
    return(-2);

  usock = socket(AF_UNIX, SOCK_STREAM, 0);
  if(usock >= 0){
    SETZERO(addr);
    addr.sun_family = AF_UNIX;
    strcpy(addr.sun_path, path);

    sock_optval = 1;
    setsockopt(usock, SOL_SOCKET, SO_REUSEADDR,
				(char *) &sock_optval, sizeof(int));

    if(bind(usock, (struct sockaddr * ) (& addr), sizeof(addr))){
	close(usock);
	usock = -1;
    }
  }

  return(usock);
}

int
open_uxsock_conn(UChar * path)
{
  struct sockaddr_un	addr;
  int	usock;

  usock = socket(AF_UNIX, SOCK_STREAM, 0);
  if(usock >= 0){
    SETZERO(addr);
    addr.sun_family = AF_UNIX;
    strcpy(addr.sun_path, path);

    if(connect(usock, (struct sockaddr *) (&addr), sizeof(addr))){
	close(usock);
	usock = -1;
    }
  }

  return(usock);
}

#if 0
/* implemented as macro in sysutils.h */
Int32
set_closeonexec(int fd)
{
  int	fl;

  fl = fcntl(fd, F_GETFD) | FD_CLOEXEC;

  return(fcntl(fd, F_SETFD, fl));
}
#endif

#endif	/* any PC-compiler */

/************ end of $RCSfile: sysutils.c,v $ ******************/


