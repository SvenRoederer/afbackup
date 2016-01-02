/****************** Start of $RCSfile: safewrap.c,v $  ****************
*
* $Source: /home/alb/afbackup/afbackup-3.5.8.12/RCS/safewrap.c,v $
* $Id: safewrap.c,v 1.5 2012/11/01 09:52:53 alb Exp alb $
* $Date: 2012/11/01 09:52:53 $
* $Author: alb $
*
*
******* description ***********************************************
*
*
*
*******************************************************************/

#include <conf.h>
#include <version.h>

  static char * fileversion = "$RCSfile: safewrap.c,v $ $Source: /home/alb/afbackup/afbackup-3.5.8.12/RCS/safewrap.c,v $ $Id: safewrap.c,v 1.5 2012/11/01 09:52:53 alb Exp alb $ " PACKAGE " " VERSION_STRING;

#include <stdio.h>
#include <string.h>
#include <fcntl.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#ifdef	HAVE_SYS_TIME_H
#include <sys/time.h>
#endif
#ifdef	HAVE_SYS_WAIT_H
#include <sys/wait.h>
#endif
#include <sys/types.h>
#include <signal.h>
#ifdef	HAVE_SYS_TIME_H
#ifdef	TIME_WITH_SYS_TIME
#include <time.h>
#endif
#endif
#include <ctype.h>

#include <x_types.h>
#include <genutils.h>
#include <sysutils.h>
#include <netutils.h>

#define	PROC_MAX_TRIES			20
#define	MEM_MAX_TRIES			60
#define	DEFAULT_TCPIP_OPEN_TIMEOUT	1800


Int64
read_forced(int fd, UChar * data, Int64 num)
{
  Int64		i, n = 0;

  if(num < 1)
    return(0);

  while(num > 0){
    i = read(fd, data, num);

    if(i < 1){
	return(n > 0 ? n : i);
	break;
    }

    n += i;
    num -= i;
    data += i;
  }

  return(n);
}


Int64
write_forced(int fd, UChar * data, Int64 num)
{
  Int64		i, n = 0;

  if(num < 1)
    return(0);

  while(num > 0){
    i = write(fd, data, num);

    if(i < 1){
	return(n > 0 ? n : i);
	break;
    }

    n += i;
    num -= i;
    data += i;
  }

  return(n);
}


static int		/* try 10 seconds to obtain a lock to a file */
set_lock_forced(UChar * name, int filemode, int lockmode)
{
  int		fd;
  struct flock	lockb;
  time_t	startt;

  fd = open(name, filemode, 0600);
  if(fd < 0)
    return(-1);

  startt = time(NULL);

  forever{
    SETZERO(lockb);
    lockb.l_type = lockmode;

    if(!fcntl(fd, F_SETLK, &lockb))
	return(fd);

    if(startt + 10 > time(NULL))
	break;

    ms_sleep((Int32)(drandom() * 200.0) + 100);
  }

  return(-1);
}

int
set_wlock_forced(UChar * name)
{
  return(set_lock_forced(name, O_WRONLY | O_CREAT, F_WRLCK));
}

int
set_rlock_forced(UChar * name)
{
  return(set_lock_forced(name, O_RDONLY, F_RDLCK));
}

int			/* try n milliseconds to obtain a lock to a file */
set_wlock_timeout(UChar * name, Int32 timeout_ms)
{
  int		fd;
  Int32		remaining_ms;
  struct flock		lockb;
  struct timeval	tv, ntv, etv;
  Flag		last_try = NO;

  gettimeofday(&tv, NULL);
  etv.tv_usec = tv.tv_usec + 1000 * (timeout_ms % 1000);
  etv.tv_sec = tv.tv_sec + (timeout_ms / 1000);
  if(etv.tv_usec > 1000000){
    etv.tv_sec++;
    etv.tv_usec -= 1000000;
  }

  fd = open(name, O_WRONLY | O_CREAT, 0600);
  if(fd < 0)
    return(-1);

  forever{
    SETZERO(lockb);
    lockb.l_type = F_WRLCK;

    if(!fcntl(fd, F_SETLK, &lockb))
	return(fd);

    if(last_try)
	return(-1);

    remaining_ms = 1000 * (etv.tv_sec - tv.tv_sec) +
			(etv.tv_usec - tv.tv_usec) / 1000;
    if(remaining_ms > 300)
	remaining_ms = (Int32)(drandom() * 200.0) + 100;
    else
	last_try = YES;
	
    ms_sleep(remaining_ms);

    gettimeofday(&ntv, NULL);
    if(ntv.tv_sec < tv.tv_sec)
	ntv.tv_sec += 24 * 60 * 60;
    tv.tv_sec = ntv.tv_sec;
    tv.tv_usec = ntv.tv_usec;
  }

  return(-1);
}

pid_t
waitpid_forced(pid_t pid, int * status, int options)
{
  int	p, e;

  e = errno;
  forever{
    p = waitpid(pid, status, options);

    if(p == pid || !p)
	break;

    if(errno != EINTR
#ifdef	ERESTARTSYS
				 && errno != ERESTARTSYS
#endif
							)
	break;

    errno = e;
  }

  return(p);
}

int
fork_forced()
{
  int		pid, proc_tries;

  proc_tries = 0;
  do{
    pid = fork();

    if(pid < 0){
	if(errno == EAGAIN){
	  errno = 0;

	  proc_tries++;
	  if(proc_tries > PROC_MAX_TRIES){
	    return(pid);
	  }

	  ms_sleep(1000 * 1);
	}
	else{
	  return(pid);
	}
    }

    if(!pid)
	clr_timer();

  } while(pid < 0);

  return(pid);
}

static Int32
fget_char_forced(FILE * fp, UChar * c)
{
  Int32		r;

  forever{
    r = fread(c, sizeof(UChar), 1, fp);
    if(r < 1){
      if(errno != EINTR){
	fclose(fp);
	return((Int32) EOF);
      }
      errno = 0;
      continue;
    }

    break;
  }

  return(1);
}

Int32
fscanwordq_forced(
   FILE         *fp,
   UChar         *string)
{
   Int32	i = 0, quote = 0;
   UChar 	a = '\0', prev;

   do{
	prev = a;
	if(fget_char_forced(fp, &a) < 1){
            fclose(fp);
            return((Int32) EOF);
        }
   } while(isspace(a));

   string[0] = a;

   if(a == '\\')
	i--;
   if(a == '\"'){
	quote = 1;
	i--;
   }

   do{
        i++;
	prev = a;
        if(fget_char_forced(fp, &a) < 1){
            fclose(fp);
	    string[i] = '\0';
            return(NO_ERROR);
        }
	if(a == '\\'){
	    if(prev != a){
		prev = a;
		if(fget_char_forced(fp, &a) < 1){
		    fclose(fp);
		    string[i] = '\0';
		    return(NO_ERROR);
		}
	    }
	}
	else{
	    do{
		if(a == '\"' && prev != '\\'){
		    prev = a;
		    quote = !quote;
        	    if(fget_char_forced(fp, &a) < 1){
			fclose(fp);
			string[i] = '\0';
			return(NO_ERROR);
        	    }
		}
	    } while(a == '\"' && prev != '\\');
	}
        string[i] = a;
	if(a == '\\')
	    a = '\0';
   } while(!isspace(a) || quote);

   string[i] = '\0';

   return(NO_ERROR);
}

/*
 * Try to allocate a region, up to MEM_MAX_TRIES times.  (This routine
 * can still return NULL, though.)
 */

void *
malloc_forced(size_t size)
{
  void	*newm;	/* uninitialized OK */
  Int32	i;

  for(i = MEM_MAX_TRIES; i >= 0; i--){
    newm = malloc(size);
    if(newm)
	break;

    ms_sleep(50 + (Int32)(200.0 * drandom()));
  }

  return(newm);
}

void *
realloc_forced(void * old, size_t size)
{
  void	*newm;	/* uninitialized OK */
  Int32	i;

  if(!old)
    return(malloc_forced(size));

  for(i = MEM_MAX_TRIES; i >= 0; i--){
    newm = realloc(old, size);
    if(newm)
	break;

    ms_sleep(50 + (Int32)(200.0 * drandom()));
  }

  return(newm);
}

int
open_tcpip_conn_forced(
  UChar		*hostname,
  UChar		*servname,
  Int32		fallback_port,
  Int32		timeout)
{
  int		fd, avg_sleeptime_ms, sleeptime_ms;
  time_t	endtime, time_now;

  avg_sleeptime_ms = 200;
  if(timeout < 0)
    timeout = DEFAULT_TCPIP_OPEN_TIMEOUT;

  endtime = time(NULL) + timeout;

  do{
    fd = open_tcpip_conn(hostname, servname, fallback_port);
    if(fd >= 0)
	break;

    if(fd > -128)	/* 0 > return-value > -128: we can never connect */
	break;		/* due to wrong data e.g. host or service name */

    sleeptime_ms = (avg_sleeptime_ms / 5) +
		(Int32)(drandom() * avg_sleeptime_ms * 8 / 5);

    time_now = time(NULL);

    if(timeout){
      if(time_now + (sleeptime_ms / 1000) >= endtime){
	sleeptime_ms = (endtime - 1 - time_now) * 1000;
	if(sleeptime_ms < 500)
	  sleeptime_ms = 500;
      }
    }

    ms_sleep(sleeptime_ms);

    avg_sleeptime_ms = avg_sleeptime_ms * 3 / 2;
    if(avg_sleeptime_ms > 100000)
	avg_sleeptime_ms = 100000;
  } while(!timeout || time(NULL) <= endtime);

  return(fd);
}
