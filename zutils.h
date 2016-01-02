/****************** Start of $RCSfile: zutils.h,v $  ****************
*
* $Source: /home/alb/afbackup/afbackup-3.5.8.12/RCS/zutils.h,v $
* $Id: zutils.h,v 1.3 2012/11/01 09:52:56 alb Exp alb $
* $Date: 2012/11/01 09:52:56 $
* $Author: alb $
*
*
******* description ***********************************************
*
*
*
*******************************************************************/

#ifndef	__ZUTILS_H
#define	__ZUTILS_H	__ZUTILS_H

#ifdef	USE_ZLIB

#include <zlib.h>

#define	Z_UNINITIALIZED		0
#define	Z_INIT_FOR_INFLATE	1
#define	Z_INIT_FOR_DEFLATE	2

typedef	struct ZFile_s	{
  Int8		initialized;
  Int32		level;
  Int32		maxblock;
  int		fd;
  Flag		compr_done;
  Flag		file_exh;
  Int32		numblock;
  z_stream	z_stream;
  UChar		filebuf[0x2000];
} ZFile;

#define	ZFILE_INIT	{			\
			Z_UNINITIALIZED,	\
			(Int32) 0,		\
			(Int32) 0,		\
			(int) -1,		\
			NO,			\
			NO,			\
			(Int32) 0,		\
			}

#define	zfile_init(zf)	{ ZFile zfile_i = ZFILE_INIT;	\
			  memset(&(zfile_i.z_stream), 0, sizeof(z_stream)); \
			  memset(&(zfile_i.filebuf), 0, sizeof(zfile_i.filebuf)); \
			  memcpy((zf), &zfile_i, sizeof(zfile_i)); }

extern	Int32	reset_zfile(ZFile *);
extern	Int32	open_file_zip(ZFile *, int, Int32, Int32);
extern	Int32	open_file_unzip(ZFile *, int);
extern	Int64	read_file_zip(ZFile *, UChar *, Int64,
					Int64 (*)(int, UChar *, Int64));
extern	Int64	write_file_unzip(ZFile *, UChar *, Int64, Flag,
					Int64 (*)(int, UChar *, Int64));
extern	void	close_file_zip(ZFile *);

#endif	/* defined(USE_ZLIB) */

#endif	/* !defined(__ZUTILS_H) */
