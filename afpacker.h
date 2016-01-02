/****************** Start of $RCSfile: afpacker.h,v $  ****************
*
* $Source: /home/alb/afbackup/afbackup-3.5.8.12/RCS/afpacker.h,v $
* $Id: afpacker.h,v 1.2 2012/11/01 09:52:53 alb Exp alb $
* $Date: 2012/11/01 09:52:53 $
* $Author: alb $
*
*
******* description ***********************************************
*
*
*
*******************************************************************/

#ifndef	__AFPACKER_H
#define	__AFPACKER_H	__AFPACKER_H

#include <stdio.h>
#include <sys/types.h>
#include <signal.h>

#include <x_types.h>
#include <sysutils.h>

#ifdef	USE_ZLIB
#include <zutils.h>
#endif

typedef struct aar_params {
  char		*zipcmd;
  char		*unzipcmd;
  FILE		*infp;
  FILE		*outfp;
  FILE		*errfp;
  Int32		blocksize;
  int		infd;
  int		outfd;
  Int32		(*outputfunc)(UChar *, Int32, struct aar_params *);
  Int32		(*inputfunc)(UChar *, Int32, struct aar_params *);
  Int64		(*writefunc)(int, UChar *, Int64);
  Int64		(*readfunc)(int, UChar *, Int64);
  Real64	maxmem;
  Int32		relative;
  UChar		unlink;
  UChar		recursive;
  Uns8		verbose;
  Flag		check;
  void		(*verbosefunc)(UChar *, struct aar_params *);
  void		(*pre_verbosefunc)(UChar *, struct aar_params *);
  UChar		mode;
  Flag		ignoreown;
  Flag		skip_garbage;
  Flag		dont_keep_atime;
  time_t	time_newer;
  time_t	time_older;
  uid_t		uid;
  gid_t		gid;
  int		ngids;
  gid_t		*gids;
  UChar		**dont_compress;
  Int16		bi_compr_level;
  Int32		bi_compr_maxblock;
  UChar		*program;
  sigset_t	blocked_signals;
  struct {
    Int32	errnum;
    Int32	actcart;
    Int32	actfile;
    Int32	firstcart;
    Int32	lastcart;
    Int32	firstfile;
    Int32	lastfile;
    Int32	num_fsentries;
    Real64	bytes_saved;
    Real64	sum_filesizes;		/* sum of sizes of ALL saved files */
    Real64	sum_compr_filesizes;	/* sum of compressed data */
    Real64      sum_uncompr_filesizes;	/* sum of sizes of only compressed */
    UChar	startdate[32];
    UChar	enddate[32];
    uid_t	uid;
    time_t	mtime;
    Uns32Range	*used_carts;
    Flag	no_acl_verify_warned;
#ifdef	USE_ZLIB
    ZFile	zfile;
#endif
  }		vars;
}	AarParams;


#define	AAR_DEFAULT_PARAMS		\
	{				\
		(char *) NULL,		\
		(char *) NULL,		\
		NULL,			\
		NULL,			\
		NULL,			\
		(Int32) 1024,		\
		0,			\
		1,			\
		pack_default_output,	\
		pack_default_input,	\
		write_forced,		\
		read_forced,		\
		(Real64) 0.0,		\
		(Int32) 0,		\
		(UChar) 0,		\
		(UChar) 0,		\
		(UChar) 0,		\
		NO,			\
		pack_default_verbose,	\
		NULL,			\
		MODE_NONE,		\
		NO,			\
		NO,			\
		NO,			\
		(time_t) 0,		\
		(time_t) 0,		\
		(uid_t) 0,		\
		(gid_t) 0,		\
		0,			\
		(gid_t *) 0,		\
		(UChar **) 0,		\
		(Int16) 0,		\
		(Int32) 0,		\
		NULL,			\
	}

#define	aarparams_init_base(ap)	{ AarParams api = AAR_DEFAULT_PARAMS;	\
				  api.infp = stdin; api.outfp = stdout;	\
				  api.errfp = stderr;			\
				  memcpy((ap), &api, sizeof(api));	\
				  memset(&((ap)->vars), 0, sizeof((ap)->vars)); }

#ifdef	USE_ZLIB
#define	aarparams_init(ap)	{ aarparams_init_base(ap);		\
				  zfile_init(&((ap)->vars.zfile)); }
#else
#define	aarparams_init(ap)	aarparams_init_base(ap)
#endif

#define	EGENFAULT		1
#define	EPACKWRITE		(EGENFAULT << 1)
#define	EPACKREAD		(EGENFAULT << 2)
#define	EPACKSYMLINK		(EGENFAULT << 3)
#define	EPACKCOMPR		(EGENFAULT << 4)
#define	EPACKTYPE		(EGENFAULT << 5)
#define	EPACKSIZECH		(EGENFAULT << 6)
#define	EPACKREADCMD		(EGENFAULT << 7)
#define	EPACKACLS		(EGENFAULT << 8)
#define	EPACKNOENT		(EGENFAULT << 9)
#define	EUNPACKNOCOMPR		(EGENFAULT << 16)
#define	EUNPACKUNCOMPR		(EGENFAULT << 17)
#define	EUNPACKREAD		(EGENFAULT << 18)
#define	EUNPACKFORMAT		(EGENFAULT << 19)
#define	EUNPACKMKDIR		(EGENFAULT << 20)
#define	EUNPACKSETPROPS		(EGENFAULT << 21)
#define	EUNPACKMKENT		(EGENFAULT << 22)
#define	EUNPACKACLS		(EGENFAULT << 23)
#define	EUNPACKEXISTS		(EGENFAULT << 24)
#define	EUNPACKWRITE		(EGENFAULT << 25)
#define	EUNPACKWRITECMD		(EGENFAULT << 26)
#define	EUNPACKNOENT		(EGENFAULT << 27)
#define	EUNPACKCKSUM		(EGENFAULT << 28)
#define	EUNPACKACCESS		(EGENFAULT << 29)
#define	EUNPACKACLIDS		(EGENFAULT << 30)

#define	MODE_CREATE		'c'
#define	MODE_EXTRACT		'x'
#define	MODE_CONTENTS		't'
#define	MODE_VERIFY		'd'
#define	MODE_INDEX		'I'
#define	MODE_NONE		'\0'

#define	ENDOFARCHIVE	-1

#define	REGFILE		1
#define	FIFO		2
#define	SYMLINK		3
#define	HARDLINK	4
#define	DIRECTORY	5
#define	BLOCKDEVICE	6
#define	CHARDEVICE	7
#define	REGFILECKS	8
#define	SOCKET		55
#define	INFORMATION	100
#define	FILECONTENTS	99
#define	FILECONTENTSCKS	98
#define	FILECONTENTS_O	111
#define	COMMANDINOUT	120
#define	COMMANDINOUTCKS	121
#define	SOLARIS2_ACL	80
#define	HPUX10_ACL	81
#define	POSIX_ACL	83
#define	NFS4_ACL	84

#define	PACK_NOVERIFY	(1 << 8)

#define	PACK_TYPEMASK	255
#define	PACK_ATTRMASK	(~(PACK_TYPEMASK))

#define	FILECONTPREFIX		"/../"
#define	FILECONTZPREFIX		"//../"
#define	LOCALDEVPREFIX		".//."
#define	NOVERIFYPREFIX		"//./../"
#define	CMDINOUTPREFIX		"||| "
#define	CMDINOUTSEP		"|||"
#define	CMDINOUTCOMM		"###"

#define	BUFFERSIZ		4096
#if	(BUFFERSIZ < 4 * MAXPATHLEN + 5)
#undef	BUFFERSIZ
#define	BUFFERSIZ	(4 * MAXPATHLEN + 5)
#endif

#ifdef	USE_POSIX_ACLS
extern	Int32	packer_posix_acl_types(Int32);
#endif

extern	Int32	pack_writeout(UChar *, AarParams *, Int16);
extern	Int32	pack_readin(UChar **, AarParams *, UChar *);
extern	Int32	pack_contents(AarParams *);
extern	Int32	pack_verify(UChar **, AarParams *, UChar *);

extern	Int32	pack_default_output(UChar *, Int32, AarParams *);
extern	Int32	pack_default_input(UChar *, Int32, AarParams *);
extern	void	pack_default_verbose(UChar *, AarParams *);
extern	UChar	**packer_strerrors(UChar **, Uns32);

#endif	/* defined(__AFPACKER_H) */
