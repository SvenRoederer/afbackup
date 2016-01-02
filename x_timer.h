/****************** Start of $RCSfile: x_timer.h,v $  ****************
*
* $Source: /home/alb/afbackup/afbackup-3.5.8.12/RCS/x_timer.h,v $
* $Id: x_timer.h,v 1.2 2004/07/08 20:34:47 alb Exp alb $
* $Date: 2004/07/08 20:34:47 $
* $Author: alb $
*
*
******* description ***********************************************
*
*		Posix Timer replacement functions
*
*******************************************************************/

#ifndef __X_TIMERS
#define __X_TIMERS __X_TIMERS

#include <lconf.h>
#include <signal.h>
#ifdef	INC_SYS_TIME_H
#include <sys/time.h>
#endif
#ifdef	INC_TIME_H
#include <time.h>
#endif
#ifdef	INC_SYS_TIMERS_H
#include <sys/timers.h>
#endif

#ifdef	TIMER_ABSTIME
#undef	TIMER_ABSTIME
#endif
#ifdef	TIMER_ADDREL
#undef	TIMER_ADDREL
#endif
#ifdef	_TIMER_PRESERVE_EXEC
#undef	_TIMER_PRESERVE_EXEC
#endif
#define	TIMER_ABSTIME		1
#define	TIMER_ADDREL		(TIMER_ABSTIME << 1)
#define	_TIMER_PRESERVE_EXEC	(TIMER_ABSTIME << 2)

#define	clock_getres(id, res)	restimer((id), (res), &_local_timer_ptr_)

#define	CLOCK_TYPE_REALTIME	1

struct Signal_Event {
  signal_t	sigev_signum;
};

typedef	int	ClockID;
typedef	int	Timer;

#ifdef	__cplusplus
extern	"C"	{
#endif

extern	Timer	timer_make(ClockID, struct Signal_Event *);
extern	int	timer_del(Timer);
extern	int	timer_get(Timer, struct itimerval *);
extern	int	timer_set(Timer, int, struct itimerval *,
						struct itimerval *);
extern	int	clock_get(ClockID, struct timeval *);

#ifdef	__cplusplus
}
#endif


#endif	/* __X_TIMERS */

/************ end of %M% **************************************/
