/****************** Start of $RCSfile: zutils.c,v $  ****************
*
* $Source: /home/alb/afbackup/afbackup-3.5.8.12/RCS/zutils.c,v $
* $Id: zutils.c,v 1.3 2012/11/01 09:53:03 alb Exp alb $
* $Date: 2012/11/01 09:53:03 $
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

  static char * fileversion = "$RCSfile: zutils.c,v $ $Source: /home/alb/afbackup/afbackup-3.5.8.12/RCS/zutils.c,v $ $Id: zutils.c,v 1.3 2012/11/01 09:53:03 alb Exp alb $ " PACKAGE " " VERSION_STRING;

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>

#include <x_types.h>
#include <genutils.h>

#ifdef	USE_ZLIB

#include <zutils.h>


Int32
reset_zfile(ZFile * zfile)
{
  Int8	inited;

  inited = zfile->initialized;

  if(inited != Z_UNINITIALIZED){
    if(inited == Z_INIT_FOR_INFLATE){
	inflateEnd(& (zfile->z_stream));
    }
    else if(inited == Z_INIT_FOR_DEFLATE){
	deflateEnd(& (zfile->z_stream));
    }
    else{
	fprintf(stderr, "Internal Error: Unknown zstream status.\n");
    }
  }

  memset(zfile, 0, (char *) zfile->filebuf - (char *) zfile);

  zfile->initialized = Z_UNINITIALIZED;
  zfile->compr_done = NO;
  zfile->file_exh = NO;

  return(0);
}

Int32
open_file_zip(
  ZFile		*zfile,
  int		fd,
  Int32		level,
  Int32		maxblock)
{
  Int32		r;

  memset(zfile, 0, sizeof(ZFile));

  zfile->fd = fd;

  if((r = deflateInit(&(zfile->z_stream), level)) != Z_OK){
    return(r);
  }

  zfile->initialized = Z_INIT_FOR_DEFLATE;
  zfile->level = level;
  zfile->maxblock = maxblock;

  return(0);
}

Int32
open_file_unzip(
  ZFile		*zfile,
  int		fd)
{
  memset(zfile, 0, sizeof(ZFile));

  zfile->fd = fd;

  if(inflateInit(&(zfile->z_stream)) != Z_OK){
    return(2);
  }

  zfile->initialized = Z_INIT_FOR_INFLATE;
  zfile->level = 1;

  return(0);
}

Int64
read_file_zip(
  ZFile		*zfile,
  UChar		*cp,
  Int64		n,
  Int64		(*readfunc)(int, UChar *, Int64))
{
  Int64		i;
  Int32		d;

  if(zfile->level <= 0)
    return(readfunc(zfile->fd, cp, n));

  if(n <= 0)
    return(n);

  if(zfile->compr_done)
    return(0);

  zfile->z_stream.avail_out = n;
  zfile->z_stream.next_out = cp;

  while(zfile->z_stream.avail_out > 0){
    if(zfile->z_stream.avail_in == 0 && !zfile->file_exh){
	i = readfunc(zfile->fd, zfile->filebuf, 0x2000);
	if(i >= 0){
	  if(i < 0x2000){
	    zfile->file_exh = YES;
	  }

	  zfile->z_stream.next_in = zfile->filebuf;
	  zfile->z_stream.avail_in = i;
	}
	else{
	  reset_zfile(zfile);
	  return(i);
	}
    }

    d = deflate(&(zfile->z_stream), zfile->file_exh ? Z_FINISH : Z_NO_FLUSH);
    if(d == Z_STREAM_END){
	zfile->compr_done = YES;
	break;
    }
    else if(d != Z_OK){
	reset_zfile(zfile);
	return(- (ABS(d) | (d > 0 ? 128 : 0)));
    }
  }

  return(zfile->z_stream.next_out - cp);
}

Int64
write_file_unzip(
  ZFile		*zfile,
  UChar		*cp,
  Int64		n,
  Flag		end,
  Int64		(*writefunc)(int, UChar *, Int64))
{
  Int64		i, d, len;
  UChar		*oldptr;

  if(zfile->level <= 0)
    return(writefunc(zfile->fd, cp, n));

  if(n <= 0)
    return(n);

  if(zfile->compr_done)
    return(0);

  zfile->z_stream.avail_in = n;
  zfile->z_stream.next_in = cp;

  while(zfile->z_stream.avail_in > 0){
    if(zfile->z_stream.avail_out == 0){
	zfile->z_stream.avail_out = 0x2000;
	zfile->z_stream.next_out = zfile->filebuf;
    }

    oldptr = zfile->z_stream.next_out;
    d = inflate(&(zfile->z_stream), /* end ? Z_FINISH : */ Z_NO_FLUSH);
    len = zfile->z_stream.next_out - oldptr;
    if(zfile->z_stream.next_out > oldptr){
	i = writefunc(zfile->fd, oldptr, len);
	if(i < len){
	  if(i < 0)
	    return(i);

	  d = n * i / len;
	  return(d < n ? d : (d ? d - 1 : 0));
	}
    }

    if(d == Z_STREAM_END)
	break;
    else if(d != Z_OK){
	return(- (ABS(d) | (d > 0 ? 128 : 0)));
    }
  }

  return(zfile->z_stream.next_in - cp);
}

void
close_file_zip(ZFile * zfile)
{
  reset_zfile(zfile);
}

#endif	/* defined(USE_ZLIB) */
