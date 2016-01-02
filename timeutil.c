/****************** Start of $RCSfile: timeutil.c,v $  ****************
*
* $Source: /home/alb/afbackup/afbackup-3.5.8.12/RCS/timeutil.c,v $
* $Id: timeutil.c,v 1.5 2012/11/01 09:53:11 alb Exp alb $
* $Date: 2012/11/01 09:53:11 $
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

  static char * fileversion = "$RCSfile: timeutil.c,v $ $Source: /home/alb/afbackup/afbackup-3.5.8.12/RCS/timeutil.c,v $ $Id: timeutil.c,v 1.5 2012/11/01 09:53:11 alb Exp alb $ " PACKAGE " " VERSION_STRING;

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#ifdef	TIME_WITH_SYS_TIME
#ifdef	HAVE_SYS_TIME_H
#include <sys/time.h>
#endif
#endif
#include <sys/types.h>
#include <signal.h>
#ifdef	HAVE_SYS_TIME_H
#include <time.h>
#endif
#include <ctype.h>

#include <x_types.h>
#include <genutils.h>
#include <x_regex.h>

static	UChar		buf[48];

UChar *
timestr(time_t given_time)
{
  strcpy(buf, ctime(&given_time));

  while(buf[strlen(buf) - 1] == '\n')
    buf[strlen(buf) - 1] = '\0';

  return(buf);
}

UChar *
actimestr()
{
  time_t	actime;

  actime = time(NULL);

  return(timestr(actime));
}

void
clr_timer()
{
  struct itimerval	itv;

  memset(&itv, 0, sizeof(itv));
  setitimer(ITIMER_REAL, &itv, NULL);

  signal(SIGALRM, SIG_DFL);
}

static Flag	timerdone;

static void sig_handler(int sig)
{
  timerdone = YES;
}

void
ms_sleep(Int32 msecs)
{
#ifdef	HAVE_SELECT

  struct timeval	tv;

  tv.tv_sec = msecs / 1000;
  tv.tv_usec = (msecs % 1000) * 1000;

  select(1, NULL, NULL, NULL, &tv);

#else	/* defined(HAVE_SELECT) */

  struct itimerval	timerval, otimerval;
  int			res;
  struct sigaction	siga, osiga;
  sigset_t		sigm, osigm;

  timerval.it_value.tv_sec = msecs / 1000;
  timerval.it_value.tv_usec = (msecs % 1000) * 1000;
  timerval.it_interval.tv_sec = 0;
  timerval.it_interval.tv_usec = 0;

  timerdone = NO;

  sigemptyset(&sigm);		/* get mask of blocked signals */
  sigaddset(&sigm, SIGALRM);		/* and unblock SIGALRM */
  sigprocmask(SIG_UNBLOCK, &sigm, &osigm);

  SETZERO(siga);		/* set signal handler, get old */
  siga.sa_handler = sig_handler;
  sigaction(SIGALRM, &siga, &osiga);

				/* set timer, get old value */
  res = setitimer(ITIMER_REAL, &timerval, &otimerval);

#if defined(HAVE_SIGHOLD) && defined(HAVE_SIGRELSE)

  while(1) {
    sighold(SIGALRM);
    if(timerdone)
	break;
    else
	sigpause(SIGALRM);
  }

  sigrelse(SIGALRM);

#else

  do{
    sigpause(SIGALRM);
  } while(!timerdone);

#endif

				/* restore old timer value */
  res = setitimer(ITIMER_REAL, &otimerval, NULL);
  sigaction(SIGALRM, &osiga, NULL);	/* signal handler */
  sigprocmask(SIG_SETMASK, &osigm, NULL);	/* and mask */

#endif	/* defined(HAVE_SELECT) */

}

#define	WS	"[ \t]+"
#define	OWS	"[ \t]*"
#define	MNAM	"[A-Za-z][A-Za-z][A-Za-z][A-Za-z]*"
#define	MNUM	"[01]?[0-9]"
#define	MDAY	"[0-3]?[0-9]"
#define	YEAR	"[0-9][0-9]*"
#define	AMDATE	MNUM "[/|\\]" MDAY "[/|\\]" YEAR
#define	EUDATE	MDAY OWS "\\([.]" OWS MNUM OWS "[.]?\\|[.]?" OWS		\
				MNAM OWS "[.]?\\)\\(" OWS YEAR "\\)"
#define	TIME	"\\([0-2]?[0-9]" OWS ":" OWS "[0-5]?[0-9]" OWS		\
		"\\(:" OWS "[0-5]?[0-9]\\)?\\|T[0-9][0-9]:?[0-9][0-9]:?[0-9][0-9]\\)"

static UChar	*monthnames[] = {
		"[Jj][Aa][Nn]", "[Ff][Ee][Bb]", "[Mm][Aa][Ee]?[Rr]",
		"[Aa][Pp][Rr]", "[Mm][Aa][YyIi]", "[Jj][Uu][Nn]",
		"[Jj][Uu][Ll]", "[Aa][Uu][Gg]", "[Ss][Ee][Pp]",
		"[Oo][CcKk][Tt]", "[Nn][Oo][Vv]", "[Dd][Ee][CcZz]",
	};
static UChar	*dateformats[] = {
		"^" OWS MNAM WS MNAM OWS MDAY WS TIME
			"\\(\\(" WS MNAM "\\)?" WS YEAR "\\)?" OWS "$",
		"^" OWS MNAM OWS MDAY WS TIME "\\(" WS YEAR "\\)?" OWS "$",
		"^" OWS MNAM OWS MDAY "\\(" WS YEAR "\\)?" OWS "$",
		"^" OWS MDAY OWS "\\([.]?" OWS MNAM OWS
			"\\|[.]" OWS MNUM OWS "\\)[.]?"
			"\\(" WS TIME "\\)?" OWS "$",
		"^" OWS TIME WS MDAY OWS "\\([.]?" OWS MNAM OWS
			"[.]?\\|[.]" OWS MNUM OWS "[.]?\\)" OWS "$",
		"^" OWS AMDATE "\\(" WS TIME "\\)?" OWS "$",
		"^" OWS EUDATE "\\(" WS TIME "\\)?" OWS "$",
		"^" OWS TIME WS EUDATE OWS "$",
		"^" OWS TIME WS AMDATE OWS "$",
		"^" OWS MNAM OWS MDAY WS YEAR WS TIME OWS "$",
		"^" OWS TIME OWS "$",
		/* ISO 8601: */
		"^" OWS "[0-9][0-9]\\([0-9][0-9]-?\\|-\\)\\([0-9][0-9]?-?\\([0-9][0-9]\\)?\\|W[0-9][0-9]-?[0-9]?\\)?\\(T[0-9][0-9]:?\\([0-9][0-9]:?\\([0-9][0-9]\\([.,][0-9]*\\)?\\)?\\)?\\)?" OWS "$",
	};

static void
read_time(UChar ** str, int * hour, int * min, int * sec)
{
  int		j;
  UChar		*cp;
  Flag		iso = NO;

  *str = first_nospace(*str);
  if(!*str)
    return;

  if(**str == 'T'){
    (*str)++;
    iso = YES;
  }
  sscanf(*str, "%2d%n", hour, &j);
  if(iso){
    *str += j;
    if(**str == ':')
	(*str)++;
  }
  else{
    *str = strchr(*str, ':') + 1;
  }
  sscanf(*str, "%2d%n", min, &j);
  (*str) += j;
  if(iso){
    if(**str == ':')
	(*str)++;
    cp = *str;
    cp--;
  }
  else
    cp = strchr(*str, ':');
  if(cp){
    cp++;
    sscanf(cp, "%d%n", sec, &j);
    *str = cp + j;
  }
  *str = first_nospace(*str);
}

static Int32
read_month(UChar ** str, int * mon)
{
  int		i, n;
  Int32		end;

  if(isdigit(**str)){
    sscanf(*str, "%d%n", mon, &i);
    *str += i;
  }
  else{
    n = sizeof(monthnames) / sizeof(monthnames[0]);
    for(i = 0; i < n; i++){
	if(re_find_match_once(monthnames[i], *str, NULL, &end) == 0)
	  break;
    }
    if(i < n){
	*mon = (i % 12) + 1;
    }
    else{
	return(-1);
    }
    while(isalpha(**str))
	(*str)++;
  }

  return(0);
}

static struct tm *
actim()
{
  static struct tm	actm;
  time_t		actime;

  actime = time(NULL);
  memcpy(&actm, localtime(&actime), sizeof(actm));
  return(&actm);
}

static Int32
read_date(UChar ** str, int * day, int * mon, int * year, UChar am)
{
  int		j;

  sscanf(*str, "%d%n", day, &j);
  *str += j;
  while(!isalnum(**str))
    (*str)++;
  if(read_month(str, mon)){
    *day = *year = *mon = 0;
    return(-1);
  }
  if(am)
    memswap(day, mon, sizeof(int));

  while(!isdigit(**str))
    (*str)++;
  sscanf(*str, "%d%n", year, &j);
  *str = first_nospace(*str + j);
  if(*year < 100)
    *year += 1900 + (*year >= 70 ? 0 : 100);

  return(0);
}

time_t
time_from_datestr(UChar * datestr)
{
  Int32		nfmts;
  int		i, j, year, mon, day, hour, min, sec, week, yday, fmt;
  struct tm	td, *atd, testtd;
  time_t	testres;

  year = mon = day = hour = min = sec = week = yday = 0;
  fmt = -1;

  nfmts = sizeof(dateformats) / sizeof(dateformats[0]);
  for(i = 0; i < nfmts; i++){
    if(re_find_match_once(dateformats[i], datestr, NULL, NULL) == 0){
	fmt = i;
	break;
    }
  }
  if(fmt == -1)
    return(UNSPECIFIED_TIME);

  datestr = first_nospace(datestr);

  SETZERO(td);

  switch(fmt){
    case 0:
	while(!isspace(*datestr))
	  datestr++;
	datestr = first_nospace(datestr);

    case 1:
    case 2:
	if(read_month(&datestr, &mon))
	  return(UNSPECIFIED_TIME);

	sscanf(datestr, "%d%n", &day, &j);
	datestr = first_nospace(datestr + j);
	if(fmt == 1 || fmt == 0){
	  read_time(&datestr, &hour, &min, &sec);
	}

	if(*datestr){
	  if(fmt == 0){
	    while(!isdigit(*datestr) && *datestr)
		datestr++;
	  }

	  sscanf(datestr, "%d", &year);
	  if(year < 100)
	    year += 1900 + (year >= 70 ? 0 : 100);
	}
	else{
	  atd = actim();
	  year = atd->tm_year + 1900;
	}
	break;

    case 5:
    case 6:
	if(read_date(&datestr, &day, &mon, &year, fmt == 5))
	  return(UNSPECIFIED_TIME);

	if(! *datestr)
	  break;

    case 7:
    case 8:
	read_time(&datestr, &hour, &min, &sec);

	if(fmt == 7 || fmt == 8)
	  if(read_date(&datestr, &day, &mon, &year, fmt == 8))
	    return(UNSPECIFIED_TIME);

	break;

    case 3:
	sscanf(datestr, "%d%n", &day, &i);
	datestr += i;
	while(!isalnum(*datestr))
	  datestr++;

	if(read_month(&datestr, &mon))
	  return(UNSPECIFIED_TIME);

	datestr = first_nospace(datestr);

	if(*datestr){
	  while(!isdigit(*datestr) && *datestr != 'T' && *datestr)
	    datestr++;
	  if(*datestr)
	    read_time(&datestr, &hour, &min, &sec);
	}

    case 4:
    case 10:
	atd = actim();
	year = atd->tm_year + 1900;

	if(fmt == 3)
	  break;

	read_time(&datestr, &hour, &min, &sec);

	if(fmt == 10){
	  mon = atd->tm_mon + 1;
	  day = atd->tm_mday;
	  break;
	}

	sscanf(datestr, "%d%n", &day, &i);
	datestr += i;
	while(!isalnum(*datestr))
	  datestr++;

    case 9:
	if(read_month(&datestr, &mon))
	  return(UNSPECIFIED_TIME);

	if(fmt == 4)
	  break;

	sscanf(datestr, "%d%n", &day, &i);
	datestr += i;

	sscanf(datestr, "%d%n", &year, &i);
	datestr += i;
	if(year < 100)
	  year += 1900 + (year >= 70 ? 0 : 100);

	read_time(&datestr, &hour, &min, &sec);
	break;

    case 11:	/* complete ISO-8601 */
	try_start{
	  i = 0;
	  sscanf(datestr, "%d%n", &year, &i);
	  if(i == 6){
	    day = year % 100;
	    mon = (year / 100) % 100;
	    year /= 10000;
	    year += 1900 + (year >= 70 ? 0 : 100);
	    datestr += i;
	    break;
	  }
	  i = year = 0;
	  sscanf(datestr, "%4d%n", &year, &i);
	  if(i < 1){
	    year = 0;
	    break;
	  }
	  if(i == 2)
	    year += 1900 + (year >= 70 ? 0 : 100);
	  datestr += i;
	  if(year < 100)
	    year += 1900 + (year >= 70 ? 0 : 100);
	  if(*datestr == '-')
	    datestr++;
	  if(*datestr == 'W'){
	    day = 1;
	    datestr++;
	    if(strlen(datestr) < 2)
	      break;
	    i = 0;
	    sscanf(datestr, "%2d%n", &week, &i);
	    if(i < 1){
	      week = 1;
	      break;
	    }
	    datestr += i;
	    if(*datestr == '-')
	      datestr++;
	    if(strlen(datestr) < 1)
	      break;
	    i = 0;
	    sscanf(datestr, "%1d%n", &day, &i);
	    if(i < 1){
	      day = 1;
	      break;
	    }
	    datestr += i;
	  }
	  else{
	    day = mon = 1;
	    if(strlen(datestr) < 2)
	      break;
	    i = 0;
	    sscanf(datestr, "%d%n", &yday, &i);
	    if(i == 3){
		datestr += i;
		break;
	    }
	    yday = i = 0;
	    sscanf(datestr, "%2d%n", &mon, &i);
	    if(i < 1){
	      mon = 1;
	      break;
	    }
	    datestr += i;
	    if(*datestr == '-')
	      datestr++;
	    if(strlen(datestr) < 2)
	      break;
	    i = 0;
	    sscanf(datestr, "%2d%n", &day, &i);
	    if(i < 1){
	      day = 1;
	      break;
	    }
	    datestr += i;
	  }
	}try_end;
	if(*datestr == 'T'){
	  try_start{
	    datestr++;
	    if(strlen(datestr) < 2)
	      break;
	    i = 0;
	    sscanf(datestr, "%2d%n", &hour, &i);
	    if(i < 1){
	      hour = 0;
	      break;
	    }
	    datestr += i;
	    if(*datestr == ':')
	      datestr++;
	    if(strlen(datestr) < 2)
	      break;
	    i = 0;
	    sscanf(datestr, "%2d%n", &min, &i);
	    if(i < 1){
	      min = 0;
	      break;
	    }
	    datestr += i;
	    if(*datestr == ':')
	      datestr++;
	    if(strlen(datestr) < 2)
	      break;
	    i = 0;
	    sscanf(datestr, "%2d%n", &sec, &i);
	    if(i < 1){
	      sec = 0;
	      break;
	    }
	  }try_end;
	}

  }

  td.tm_hour = hour;
  td.tm_min = min;
  td.tm_sec = sec;
  td.tm_year = year - 1900;
  td.tm_isdst = -1;
  td.tm_mday = 1;

  if(week || yday){
    testres = mktime(&td);
    atd = localtime(&testres);
    COPYVAL(testtd, *atd);
    if(week){
      if(testtd.tm_wday == 0)
	testtd.tm_wday = 7;
      testres += 86400 * (7 * (week - 1) + day - 1);
      if(testtd.tm_wday >= 5)
	testres += 86400 * (7 - testtd.tm_wday + 1);
      else
	testres -= 86400 * (testtd.tm_wday - 1);
    }
    else{
	testres += 86400 * (yday - 1);
    }
    return(testres);
  }

  td.tm_mday = day;
  td.tm_mon = mon - 1;

  return(mktime(&td));
}

/*
 * Takes the string <str> and returns the time_t number corresponding
 * to that string.
 */

time_t
strint2time(UChar * str)
{
  time_t	t;

  t = (time_t) 0;

  str = first_nospace(str);
  if(!isdigit(*str))
    return(UNSPECIFIED_TIME);

  while(*str && isdigit(*str))
    t = (t << 3) + (t << 1) + (*(str++) - '0');

  return(t);
}
