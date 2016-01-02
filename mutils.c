/****************** Start of $RCSfile: mutils.c,v $  ****************
*
* $Source: /home/alb/afbackup/afbackup-3.5.8.12/RCS/mutils.c,v $
* $Id: mutils.c,v 1.5 2006/12/12 20:21:11 alb Exp alb $
* $Date: 2006/12/12 20:21:11 $
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

  static char * fileversion = "$RCSfile: mutils.c,v $ $Source: /home/alb/afbackup/afbackup-3.5.8.12/RCS/mutils.c,v $ $Id: mutils.c,v 1.5 2006/12/12 20:21:11 alb Exp alb $ " PACKAGE " " VERSION_STRING;

#include <stdio.h>
#include <fcntl.h>
#include <stdlib.h>
#include <x_types.h>
#include <sys/types.h>
#ifdef  HAVE_SYS_TIME_H
#include <sys/time.h>
#endif
#ifdef  HAVE_SYS_TIME_H
#ifdef  TIME_WITH_SYS_TIME
#include <time.h>
#endif
#endif
#include <unistd.h>

#include <genutils.h>
#include <fileutil.h>

Real64
drandom()
{
  static Flag	initialized = NO;
  static Flag	workaround_initialized = NO;

  static Real64	maxval = 0.0;	/* on SunOS-4 drand48 seems not to work */
  Real		randval;
  Int32		the_seed[4];
  long int	*the_seed_ptr;
  int		i, fd;

  if(! initialized){

    the_seed_ptr = (long int *) (& the_seed[0]);

    *the_seed_ptr = (long int) time(NULL) * getpid();

#ifdef	HAVE_GETTIMEOFDAY
    {
	struct timeval	tv;

	gettimeofday(&tv, NULL);

	*the_seed_ptr *= (tv.tv_sec * 1000 + tv.tv_usec / 1000
			+ (tv.tv_usec % 1000) * 100000000);
    }
#endif

    fd = open("/dev/random", O_RDONLY);
    if(fd >= 0){
	fcntl(fd, F_SETFL, fcntl(fd, F_GETFL) | NONBLOCKING_FLAGS);
	i = read(fd, (char *) the_seed_ptr, sizeof(the_seed));
	close(fd);
	if(i < 0)
	  i = 0;

	if(i < sizeof(the_seed)){
	  fd = open("/dev/urandom", O_RDONLY);
	  if(fd >= 0){
	    read(fd, (char *) the_seed_ptr + i, sizeof(the_seed) - i);
	    close(fd);
	  }
	}
    }

#ifdef	HAVE_DRAND48
    srand48(*((long int *) the_seed));
#else
#ifdef	HAVE_SEED48
    seed48((unsigned short int *) the_seed_ptr);
#else
#ifdef	HAVE_RAND
    srand(*((unsigned int *) the_seed));
#endif
#endif
#endif

    initialized = YES;
  }

#ifdef	HAVE_DRAND48
    randval = drand48();
    forever{
	if(randval < 1.0 && randval > 0.0)
	  return(randval);

	if(randval > maxval){		/* workaround stupid SunOS-4 */
	  maxval = randval;
	}
	randval = drand48();
	if(randval == maxval){		/* drand48 does not work at all */

#ifdef	HAVE_RAND
#ifndef	RAND_MAX
#define	RAND_MAX	0x7fffffff
#endif

	  if(!workaround_initialized){
		*the_seed_ptr = (long int) (time(NULL) * getpid());
		srand(*the_seed_ptr);
		workaround_initialized = YES;
	  }

	  return((Real64) rand() / (Real64) (RAND_MAX));
	}

	if(randval < maxval){
	  return((Real64) randval / (Real64) maxval);
	}
#else
	fprintf(stderr, T_("Warning: drand48 does not work and no rand available.\n"));
	return(0.5);
#endif
    }

#else
#ifdef	HAVE_RAND
    return((Real64) rand() / (Real64) (RAND_MAX));
#endif
#endif

#if !defined(HAVE_DRAND48) && !defined(HAVE_RAND)
#error	"No random function found."
#endif
}


Real64
Real64_precision()
{
  static Int16	prec_calculated = 0;
  Real64		d, q;
  static Real64		p;

  if(prec_calculated)
    return(p);

  d = 1.0;
  q = 2.0;

  while(q != 1.0){
    while(1.0 + d != 1.0){
	p = d;
	d /= q;
    }
    d = p;
    q = 1.0 + ((q - 1.0) / 2.0);
  }

  prec_calculated = 1;

  return(p);
}

static int
permutate1(
  char		*arr,
  size_t	nmemb,
  size_t	size,
  int		pfunc(void *, int, void *),
  void		*data,
  size_t	orgn)
{
  int		i, r, j;
  char		*larr, *p;
  size_t	totalsz;

  totalsz = size * orgn;

  larr = malloc_forced(totalsz);
  if(!larr)
    return(-1);

  for(i = j = nmemb - 1; i >= 0; i--){
    if(nmemb < 2){
	r = (*pfunc)(arr, orgn, data);
	p = arr;
    }
    else{
	if(i != j){
	  memcpy(larr, arr, totalsz);
	  memcpy(larr + size * i, arr + size * j, size);
	  memcpy(larr + size * j, arr + size * i, size);
	  p = larr;
	}
	else{
	  p = arr;
	}

	r = permutate1(p, nmemb - 1, size, pfunc, data, orgn);
    }
    if(r){
	if(r == 2 && arr != p)
	  memcpy(arr, p, totalsz);

	free(larr);
	return(r);
    }
  }

  free(larr);
  return(0);
}

int
permutate(
  void		*base,
  size_t	nmemb,
  size_t	size,
  int		pfunc(void *, int, void *),
  void		*data)
{
  return(permutate1((char *) base, nmemb, size, pfunc, data, nmemb));
} 

double
r_int(double d)
{
  int	r;

  r = (d > 0.0 ? (int) (d + 0.5) : - (int) (-d + 0.5));

  return((double) r);
}

int
is_nan(float f)
{
  char	*u;

  u = (char *) (&f);

  if((u[3] & 127) == 127 && (u[2] & 128) == 128 &&
	((u[2] & 127) != 0 || u[1] != 0 || u[0] != 0))
    return(1);

  return(0);
}

Int32
align_n(Int32 s, Int32 n)
{
  Int32	k;

  k = (s - 1) / n + 1;

  return(k * n);
}

static	double	si_basevals[] = { 1e24, 1e21, 1e18, 1e15, 1e12, 1e9, 1e6, 1e3,
					1.0, 1e-3, 1e-6, 1e-9, 1e-12, 1e-15,
					1e-18, 1e-21, 1e-24 };
static	char	*si_symstrs[] = { "Y", "Z", "E", "P", "T", "G", "M", "k", "0",
				  "m", "u", "n", "p", "f", "a", "z", "y" };
static	double	si_bibasevals[] = {
		1208925819614629174706176.0, 1180591620717411303424.0,
		1152921504606846976.0, 1125899906842624.0,1099511627776.0,
		1073741824.0, 1048576.0, 1024.0, 1.0 };
static	char	*si_bisymstrs[] = { "Yi", "Zi", "Ei", "Pi", "Ti", "Gi",
				    "Mi", "Ki", "0" };

Int32
si_symvalstr(
  UChar		*str,
  double	value,
  Int32		prec,
  double	min_cut0,
  Flag		binary,
  Flag		space)
{
  double	*basevals, nval;
  char		**symstrs, dbuf[32];
  Int32		i, n, len = 0;
  Flag		neg = NO;

  if(prec > 20)
    prec = 20;
  if(prec < 1)
    prec = 1;

  basevals = (binary ? si_bibasevals : si_basevals);
  symstrs = (binary ? si_bisymstrs : si_symstrs);
  n = (binary ? sizeof(si_bibasevals) / sizeof(si_bibasevals[0])
		: sizeof(si_basevals) / sizeof(si_basevals[0])) - 1;
  if(value < 0.0){
    neg = YES;
    value = - value;
  }
  if(binary){
    value += 0.5;
    value -= fmod(value, 1.0);
  }
  if(value < min_cut0){
    sprintf(dbuf, "%g", 0.0);
    if(str)
	strcpy(str, dbuf);
    return(strlen(dbuf));
  }

  if(neg)
    if(str)
	(*str++) = '-';

  for(i = 0; i < n; i++)
    if(value >= basevals[i])
	break;

  nval = value / basevals[i];
  sprintf(dbuf, "%.*lg", prec, nval);
  if(space)
    strcat(dbuf, " ");

  if(symstrs[i][0] != '0')
    strcat(dbuf, symstrs[i]);

  len += strlen(dbuf);
  if(str)
    strcpy(str, dbuf);

  return(len);
}


/************ end of $RCSfile: mutils.c,v $ ******************/
