/****************** Start of $RCSfile: typeutil.c,v $  ****************
*
* $Source: /home/alb/afbackup/afbackup-3.5.8.12/RCS/typeutil.c,v $
* $Id: typeutil.c,v 1.4 2011/12/14 20:27:29 alb Exp alb $
* $Date: 2011/12/14 20:27:29 $
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

  static char * fileversion = "$RCSfile: typeutil.c,v $ $Source: /home/alb/afbackup/afbackup-3.5.8.12/RCS/typeutil.c,v $ $Id: typeutil.c,v 1.4 2011/12/14 20:27:29 alb Exp alb $ " PACKAGE " " VERSION_STRING;

#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#ifdef	HAVE_SYS_TIME_H
#include <sys/time.h>
#endif
#include <sys/types.h>
#include <sys/stat.h>
#ifdef	HAVE_SYS_TIME_H
#ifdef	TIME_WITH_SYS_TIME
#include <time.h>
#endif
#endif
#include <x_types.h>
#include <genutils.h>

UChar *
Real64_to_intstr(Real64 d, UChar * retstr)
{
  UChar		str[70];

  UChar		*cptr;
  Real64	p10, d_10;
  int		c;

  cptr = (retstr ? retstr : str);

  if(d >= 1.0e+40){
    sprintf(cptr, T_("%lg <integer representation not possible>"),
				(double) d);
    return(retstr ? retstr : (UChar *) strdup(str));
  }

  d += 0.5;
  if(d < 1.0){
    strcpy(cptr, "0");
    return(retstr ? retstr : (UChar *) strdup(str));
  }

  d_10 = d / 10.0;
  for(p10 = 1.0; p10 <= d_10; p10 *= 10.0);

  while(p10 >= 1.0){
    c = (int)(d / p10);
    *(cptr++) = '0' + c;
    d -= (Real64) c * p10;
    p10 /= 10.0;
  }

  *cptr = '\0';

  return(retstr ? retstr : (UChar *) strdup(str));
}

UChar *
time_t_to_intstr(time_t d, UChar * retstr)
{
  UChar		str[70];

  UChar		*cptr;
  time_t	p10, d_10;
  int		c, n;
  Flag		neg = NO;

  if(d < 0){
    d = -d;
    neg = YES;
  }

  p10 = (time_t) 1;
  d_10 = d / 10;
  for(n = 0; p10 <= d_10; n++)
    p10 = (p10 << 3) + (p10 << 1);

  cptr = (retstr ? retstr : str);
  if(neg)
    *(cptr++) = '-';

  while(p10 >= (time_t) 1){
    c = d / p10;
    *(cptr++) = '0' + c;
    d -= (time_t) c * p10;
    p10 /= 10;
  }

  *cptr = '\0';

  return(retstr ? retstr : (UChar *) strdup(str));
}

UChar *
size_t_to_intstr(size_t d, UChar * retstr)
{
  UChar		str[70];

  UChar		*cptr;
  size_t	p10, d_10;
  int		c, n;

  p10 = (size_t) 1;
  d_10 = d / 10;
  for(n = 0; p10 <= d_10; n++)
    p10 = (p10 << 3) + (p10 << 1);

  cptr = (retstr ? retstr : str);
  while(p10 >= (size_t) 1){
    c = d / p10;
    *(cptr++) = '0' + c;
    d -= (size_t) c * p10;
    p10 /= 10;
  }

  *cptr = '\0';

  return(retstr ? retstr : (UChar *) strdup(str));
}

UChar *
off_t_to_intstr(off_t d, UChar * retstr)
{
  UChar		str[70];

  UChar		*cptr;
  off_t		p10, d_10;
  int		c, n;

  p10 = (off_t) 1;
  d_10 = d / 10;
  for(n = 0; p10 <= d_10; n++)
    p10 = (p10 << 3) + (p10 << 1);

  cptr = (retstr ? retstr : str);
  while(p10 >= (off_t) 1){
    c = d / p10;
    *(cptr++) = '0' + c;
    d -= (off_t) c * p10;
    p10 /= 10;
  }

  *cptr = '\0';

  return(retstr ? retstr : (UChar *) strdup(str));
}

UChar *
ino_t_to_intstr(ino_t d, UChar * retstr)
{
  UChar		str[70];

  UChar		*cptr;
  ino_t		p10, d_10;
  int		c, n;

  p10 = (ino_t) 1;
  d_10 = d / 10;
  for(n = 0; p10 <= d_10; n++)
    p10 = (p10 << 3) + (p10 << 1);

  cptr = (retstr ? retstr : str);
  while(p10 >= (ino_t) 1){
    c = d / p10;
    *(cptr++) = '0' + c;
    d -= (ino_t) c * p10;
    p10 /= 10;
  }

  *cptr = '\0';

  return(retstr ? retstr : (UChar *) strdup(str));
}

Int32
sscanXValue(UChar * str, void * tvalue, X_Type type, Int32 * nchars)
{
  double	dbuf;
  long int	ibuf;
  long unsigned ubuf;
  Int32		i, dn;
  int		n;
  UChar		c;

  if(!str)
    return(ILLEGAL_ARGUMENT);

  if(!nchars)
    nchars = &dn;

  switch(type){
    case TypeReal64:
	i = sscanf(str, "%lg%n", &dbuf, &n);
	if(i > 0){
	  if(tvalue)
	    *((Real64 *) tvalue) = dbuf;
	  *nchars = n;
	}
	return(i);
	break;

    case TypeReal32:
	i = sscanf(str, "%lg%n", &dbuf, &n);
	if(i > 0){
	  if(tvalue)
	    *((Real32 *) tvalue) = dbuf;
	  *nchars = n;
	}
	return(i);
	break;

    case TypeInt32:
	i = sscanf(str, "%ld%n", &ibuf, &n);
	if(i > 0){
	  if(tvalue)
	    *((Int32 *) tvalue) = ibuf;
	  *nchars = n;
	}
	return(i);
	break;

    case TypeInt16:
	i = sscanf(str, "%ld%n", &ibuf, &n);
	if(i > 0){
	  if(tvalue)
	    *((Int16 *) tvalue) = ibuf;
	  *nchars = n;
	}
	return(i);
	break;

    case TypeUns32:
	i = sscanf(str, "%lu%n", &ubuf, &n);
	if(i > 0){
	  if(tvalue)
	    *((Uns32 *) tvalue) = ubuf;
	  *nchars = n;
	}
	return(i);
	break;

    case TypeUns16:
	i = sscanf(str, "%lu%n", &ubuf, &n);
	if(i > 0){
	  if(tvalue)
	    *((Uns16 *) tvalue) = ubuf;
	  *nchars = n;
	}
	return(i);
	break;

    case TypeUChar:
	i = sscanf(str, "%c%n", &c, &n);
	if(i > 0){
	  if(tvalue)
	    *((UChar *) tvalue) = c;
	  *nchars = n;
	}
	return(i);
	break;

    case TypeSChar:
	i = sscanf(str, "%c%n", &c, &n);
	if(i > 0){
	  if(tvalue)
	    *((SChar *) tvalue) = c;
	  *nchars = n;
	}
	return(i);
	break;

    default:
	return(-1);
  }

  return(0);
}

Int32
fscanXValue(FILE * fp, void * tvalue, X_Type type, Int32 * nchars)
{
  double	dbuf;
  long int	ibuf;
  long unsigned ubuf;
  Int32	i, dn;
  int		n;
  UChar		c;

  if(!fp)
    return(ILLEGAL_ARGUMENT);

  if(!nchars)
    nchars = &dn;

  switch(type){
    case TypeReal64:
	i = fscanf(fp, "%lg%n", &dbuf, &n);
	if(i > 0){
	  if(tvalue)
	    *((Real64 *) tvalue) = dbuf;
	  *nchars = n;
	}
	return(i);
	break;

    case TypeReal32:
	i = fscanf(fp, "%lg%n", &dbuf, &n);
	if(i > 0){
	  if(tvalue)
	    *((Real32 *) tvalue) = dbuf;
	  *nchars = n;
	}
	return(i);
	break;

    case TypeInt32:
	i = fscanf(fp, "%ld%n", &ibuf, &n);
	if(i > 0){
	  if(tvalue)
	    *((Int32 *) tvalue) = ibuf;
	  *nchars = n;
	}
	return(i);
	break;

    case TypeInt16:
	i = fscanf(fp, "%ld%n", &ibuf, &n);
	if(i > 0){
	  if(tvalue)
	    *((Int16 *) tvalue) = ibuf;
	  *nchars = n;
	}
	return(i);
	break;

    case TypeUns32:
	i = fscanf(fp, "%lu%n", &ubuf, &n);
	if(i > 0){
	  if(tvalue)
	    *((Uns32 *) tvalue) = ubuf;
	  *nchars = n;
	}
	return(i);
	break;

    case TypeUns16:
	i = fscanf(fp, "%lu%n", &ubuf, &n);
	if(i > 0){
	  if(tvalue)
	    *((Uns16 *) tvalue) = ubuf;
	  *nchars = n;
	}
	return(i);
	break;

    case TypeUChar:
	i = fscanf(fp, "%c%n", &c, &n);
	if(i > 0){
	  if(tvalue)
	    *((UChar *) tvalue) = c;
	  *nchars = n;
	}
	return(i);
	break;

    case TypeSChar:
	i = fscanf(fp, "%c%n", &c, &n);
	if(i > 0){
	  if(tvalue)
	    *((SChar *) tvalue) = c;
	  *nchars = n;
	}
	return(i);
	break;

    default:
	return(-1);
  }

  return(0);
}
