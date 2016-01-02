/****************** Start of $RCSfile: unpack.c,v $  ****************
*
* $Source: /home/alb/afbackup/afbackup-3.5.8.12/RCS/unpack.c,v $
* $Id: unpack.c,v 1.13 2012/11/01 09:53:01 alb Exp alb $
* $Date: 2012/11/01 09:53:01 $
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

  static char * fileversion = "$RCSfile: unpack.c,v $ $Source: /home/alb/afbackup/afbackup-3.5.8.12/RCS/unpack.c,v $ $Id: unpack.c,v 1.13 2012/11/01 09:53:01 alb Exp alb $ " PACKAGE " " VERSION_STRING;

#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <sys/types.h>
#ifdef	HAVE_SYS_PARAM_H
#include <sys/param.h>
#endif
#include <dirent.h>
#include <fcntl.h>
#ifdef	HAVE_SYS_TIME_H
#include <sys/time.h>
#endif
#ifdef	HAVE_SYS_WAIT_H
#include <sys/wait.h>
#endif
#include <signal.h>
#include <utime.h>
#include <unistd.h>
#include <poll.h>

#include <genutils.h>
#include <fileutil.h>
#include <afpacker.h>

#if defined(USE_SOLARIS2_ACLS) || defined(USE_HPUX10_ACLS) || defined(USE_POSIX_ACLS)
#include <sys/acl.h>
#endif

#ifdef	USE_ZLIB

#include <zutils.h>

#define	write_file(fd, cp, n, z, e, f)	write_file_unzip(z, cp, n, e, f)

#else	/* defined(USE_ZLIB) */

#define	write_file(fd, cp, n, z, e, f)	(*f)(fd, cp, n)

#endif	/* defined(USE_ZLIB) */

typedef	struct __solaris2_acl__ {
  int	acltype;
  int	nacls;
  void	*acls;
} Solaris2ACL;

typedef	Solaris2ACL	HPUX10ACL;	/* quite the same for us */
typedef	Solaris2ACL	NFS4ACL;	/* as well, nacls unused */

typedef struct __posix_acl__ {
  int	acltype;
  int	acl_types;
  void	*acl_arr[3];
} POSIXACL;

typedef struct __unknown_acl__ {
  int	acltype;
} UnknownACL;

#define	ZFREE_ACL_STRUCT(acls, params)	{ if(acls){	\
		free_acl_struct(acls, params); acls = NULL; }}

static void
free_acl_struct(void * acls, AarParams * params)
{
  if(!acls)
    return;

  switch(((UnknownACL *) acls)->acltype){
    case SOLARIS2_ACL:
	ZFREE(((Solaris2ACL *) acls)->acls);
	break;

    case NFS4_ACL:
	ZFREE(((NFS4ACL *) acls)->acls);
	break;

    case HPUX10_ACL:
	ZFREE(((HPUX10ACL *) acls)->acls);
	break;

    case POSIX_ACL:
#ifdef	USE_POSIX_ACLS
      {
	Int32	i, acl_types;
	acl_t	acl;

	acl_types = ((POSIXACL *) acls)->acl_types;
	for(i = 0; i < 3; i++){
	  if(acl_types & (1 << i)){
	    if( (acl = ((POSIXACL *) acls)->acl_arr[i]) ){
	      acl_free(acl);
	      ((POSIXACL *) acls)->acl_arr[i] = (acl_t) 0;
	    }
	  }
	}
      }
#endif
	break;

    default:
	fprintf(params->errfp,
		"Warning: Unkknown ACL type %d, possible memory"
		" leak - should never happen.\n",
		((UnknownACL *) acls)->acltype);
	return;
  }

  free(acls);
}

static Int32
get_int(AarParams * params, Int32 * rptr)
{
  Int32		i, s;
  UChar		c, n;
  Int32		r;

  n = 0;
  r = 0;
  s = 1;

  forever{
    i = params->inputfunc(&c, 1, params);
    if(i == 0)
	return(-1);

    if(c == ';')
	break;

    if(c == '-'){
	if(n || s == -1)
	  return(0);
	s = -1;
    }
    else if(isspace(c)){
	continue;
    }
    else if(c <= '9' && c >= '0'){
	r = (r << 3) + (r << 1) + (c - '0');
	n = 1;
    }
    else
	return(0);
  }

  if(!n)
    return(0);

  *rptr = r * s;

  return(1);
}
    
static Int32
get_uns(AarParams * params, Uns32 * rptr)
{
  Int32		i;
  UChar		c, n;
  Uns32		r;

  n = 0;
  r = 0;

  forever{
    i = params->inputfunc(&c, 1, params);
    if(i == 0)
	return(-1);

    if(c == ';')
	break;

    if(isspace(c)){
	continue;
    }
    else if(c <= '9' && c >= '0'){
	r = (r << 3) + (r << 1) + (c - '0');
	n = 1;
    }
    else
	return(0);
  }

  if(!n)
    return(0);

  *rptr = r;

  return(1);
}

static Int32
get_time_t(AarParams * params, time_t * rptr)
{
  Int32		i, s;
  UChar		c, n;
  time_t	r;

  n = 0;
  r = (time_t) 0;
  s = 1;		/* time_t can be fucking negative */

  forever{
    i = params->inputfunc(&c, 1, params);
    if(i == 0)
	return(-1);

    if(c == ';')
	break;

    if(c == '-'){
	if(n || s == -1)
	  return(0);
	s = -1;
    }
    else if(isspace(c)){
	continue;
    }
    else if(c <= '9' && c >= '0'){
	r = (r << 3) + (r << 1) + (c - '0');
	n = 1;
    }
    else
	return(0);
  }

  if(!n)
    return(0);

  *rptr = r * s;

  return(1);
}
    
static Int32
get_size_t(AarParams * params, size_t * rptr)
{
  Int32		i;
  UChar		c, n;
  size_t	r;

  n = 0;
  r = (size_t) 0;

  forever{
    i = params->inputfunc(&c, 1, params);
    if(i == 0)
	return(-1);

    if(c == ';')
	break;

    if(isspace(c)){
	continue;
    }
    else if(c <= '9' && c >= '0'){
	r = (r << 3) + (r << 1) + (c - '0');
	n = 1;
    }
    else
	return(0);
  }

  if(!n)
    return(0);

  *rptr = r;

  return(1);
}
    
static Int32
get_off_t(AarParams * params, off_t * rptr)
{
  Int32		i;
  UChar		c, n;
  off_t		r;

  n = 0;
  r = (off_t) 0;

  forever{
    i = params->inputfunc(&c, 1, params);
    if(i == 0)
	return(-1);

    if(c == ';')
	break;

    if(isspace(c)){
	continue;
    }
    else if(c <= '9' && c >= '0'){
	r = (r << 3) + (r << 1) + (c - '0');
	n = 1;
    }
    else
	return(0);
  }

  if(!n)
    return(0);

  *rptr = r;

  return(1);
}
    
static Int32
name_in_list(
  UChar		*name,
  UChar		**list,
  Int32		len,
  UChar		subdir,
  UChar		*found)
{
  UChar		**listptr;

  if(!list)
    return(1);

  if(len < 0)
    for(len = 0, listptr = list; *listptr; listptr++, len++);

  if(!subdir){
    listptr = (UChar **)
		b_search(&name, list, len, sizeof(UChar *), cmp_UCharPTR);
    if(listptr){
	found[listptr - list] = 1;
	return(1);
    }
    return(0);
  }	

  if(subdir)
    for(; *list; list++, found++){
      len = strlen(*list);
      if(!strcmp(*list, name) ||
		(!strncmp(*list, name, len) && (name[len] == FN_DIRSEPCHR
				|| name[len - 1] == FN_DIRSEPCHR))){
	*found = 1;
	return(1);
      }
    }

  return(0);
}

static Int32
formaterr(Int32 * type, AarParams * params, Flag started)
{
  Int32	i;
  Int32	li;

  if(!(!started && params->skip_garbage)){
    fprintf(params->errfp,
	T_("Error: format error in input, trying to skip to the next file ...\n"));

    params->vars.errnum |= EUNPACKFORMAT;
  }

  forever{
    if(params->pre_verbosefunc)
	(*params->pre_verbosefunc)(NULL, params);

    i = get_int(params, &li);

    if(i < 0)
	return(-1);	/* -1 */

    if(i == 1){
	*type = (Int32) li;
	return(1);
    }
  }
}

#define	read_int(val, typ)	{ rvr = get_int(params, &li); \
				  if(rvr < 1){ \
				    i = formaterr(&type, params, started); \
				    have_an_error = YES; \
				    goto tryagain; \
				  } \
				  val = (typ) li; }
#define	read_uns(val, typ)	{ rvr = get_uns(params, &lu); \
				  if(rvr < 1){ \
				    i = formaterr(&type, params, started); \
				    have_an_error = YES; \
				    goto tryagain; \
				  } \
				  val = (typ) lu; }
#define	read_time_t(val, typ)	{ rvr = get_time_t(params, &ltt); \
				  if(rvr < 1){ \
				    i = formaterr(&type, params, started); \
				    have_an_error = YES; \
				    goto tryagain; \
				  } \
				  val = (typ) ltt; }
#define	read_size_t(val, typ)	{ rvr = get_size_t(params, &stt); \
				  if(rvr < 1){ \
				    i = formaterr(&type, params, started); \
				    have_an_error = YES; \
				    goto tryagain; \
				  } \
				  val = (typ) stt; }
#define	read_off_t(val, typ)	{ rvr = get_off_t(params, &ott); \
				  if(rvr < 1){ \
				    i = formaterr(&type, params, started); \
				    have_an_error = YES; \
				    goto tryagain; \
				  } \
				  val = (typ) ott; }
#define	DP			"--> "	/* the DIFFERENCES_PREFIX */

static Flag
invalid_time_or_uid(time_t mtime, int uid, AarParams * params)
{
  return((params->uid && params->uid != uid)
		|| (params->time_older && params->time_older <= mtime)
		|| (params->time_newer && params->time_newer > mtime));
}

static Flag
not_allowed(UChar * name, AarParams * params)
{
  UChar		*lname;		/* uninitialized ok */
  UChar		llname[512], *cptr;
  uid_t		uid;
  Flag		r;		/* uninitialized ok */
  Flag		lname_all = NO;
  struct stat	statb;

  if(!(uid = params->uid))
    CLEANUPR(NO);
  if(!name[0])
    CLEANUPR(YES);

  lname = get_mem(NULL, NULL, strlen(name) + 1, llname, 512, &lname_all);
  if(!lname)
    CLEANUPR(YES);

  strcpy(lname, name);
  cleanpath(lname);

  forever{
    if(!stat(lname, &statb))	/* fs-entry exists, can read it */
	CLEANUPR(statb.st_uid != uid ? YES : NO);

    cptr = FN_LASTDIRDELIM(lname);	/* get last / */
    if(cptr)
	*cptr = '\0';
    if(!lname[0])
	CLEANUPR(YES);		/* was absolute path, at root now */

    if(!cptr){			/* name remaining in current dir */
	if(stat(FN_CURDIR, &statb))
	  CLEANUPR(YES);

	CLEANUPR(statb.st_uid != uid ? YES : NO);
    }
  }

 cleanup:
  if(r){
    fprintf(params->errfp,
		T_("Error: No write permission for `%s'.\n"), name);
    params->vars.errnum |= EUNPACKACCESS;
  }

  if(lname_all)
    free(lname);

  return(r);
}

static Int32
pack_contents_l(AarParams * params)
{
  UChar		lbuf[256], lbuf2[256];
  UChar		verbosestr[MAXPATHLEN * 2 + 100], c, *cptr;
  UChar		*buf = NULL, *buf2 = NULL;
  Int32		buf_nall = 0, buf2_nall = 0;
  Flag		buf_all = NO, buf2_all = NO;
  Flag		have_an_error, started, var_compr, fmterr;
  Flag		compressed, builtin_uncompress;
  Uns8		verbose;
  Int32		type, i, j, k, n, uid, gid, ifd, rvr, li, r = 0;
  Uns32		mode, lu;
  time_t	mtime, ltt;
  size_t	len, stt;
  off_t		filelen, ott;
  FILE		*errfp;
  int		pid, pst, pp[2], ucfd;
  dev_t		rdev;
  void		(*verbosefunc)(UChar *, struct aar_params *);
  Int64		(*writefunc)(int, UChar *, Int64);
  Flag		cks, biu_error;
  Uns32		crc32sum;	/* uninitialized OK */

  errfp = params->errfp;
  verbose = params->verbose;
  verbosefunc = params->verbosefunc;
  if(!verbosefunc)
    verbosefunc = pack_default_verbose;
  ifd = params->infd;
  writefunc = params->writefunc;

  started = have_an_error = NO;

  forever{
    pid = -1;

    if(params->pre_verbosefunc)
	(*params->pre_verbosefunc)(NULL, params);

    i = get_int(params, &li);
    type = (Int32) li;

   tryagain:

    if(i < 1){
	if(i < 0)
	  CLEANUPR(EOF);

	i = formaterr(&type, params, started);
	have_an_error = YES;
	goto tryagain;
    }

    type = (ABS(type) & PACK_TYPEMASK) * SIGN(type); 

    cks = NO;

    switch(type){
      case ENDOFARCHIVE:
	if(have_an_error){
	  i = formaterr(&type, params, started);
	  goto tryagain;
	}

	CLEANUPR(0);

      case INFORMATION:
	read_size_t(len, size_t);

	buf = get_mem(buf, &buf_nall, len + 1, lbuf, 256, &buf_all);
	if(!buf)
	  CLEANUPR(-ENOMEM);

	i = params->inputfunc(buf, len, params);
	if(i < len){
	  goto eoferr;
	}
	i = params->inputfunc(&c, 1, params);
	if(c != '.' || i < 1){
	  i = formaterr(&type, params, started);
	  have_an_error = YES;
	  goto tryagain;
	}
	buf[len] = '\0';
	verbosefunc(buf, params);

	started = YES;
	have_an_error = NO;

	break;
	
      case DIRECTORY:
	read_uns(mode, Uns32);
	read_time_t(mtime, time_t);
	read_int(uid, Int32);
	read_int(gid, Int32);
	read_size_t(len, size_t);

	buf = get_mem(buf, &buf_nall, len + 1, lbuf, 256, &buf_all);
	if(!buf)
	  CLEANUPR(-ENOMEM);

	i = params->inputfunc(buf, len, params);
	if(i < len)
	  goto eoferr;
	buf[len] = '\0';

	i = params->inputfunc(&c, 1, params);
	if(c != '.' || i < 1){
	  i = formaterr(&type, params, started);
	  have_an_error = YES;
	  goto tryagain;
	}

	mk_esc_seq(buf, ESCAPE_CHARACTER, verbosestr);
	if(verbose)
	  strcat(verbosestr, "/");
	strcat(verbosestr, "\n");
	params->vars.uid = uid;
	params->vars.mtime = mtime;
	verbosefunc(verbosestr, params);

	started = YES;
	have_an_error = NO;

	break;

      case FIFO:
      case SOCKET:
	read_uns(mode, Uns32);
	read_time_t(mtime, time_t);
	read_int(uid, Int32);
	read_int(gid, Int32);
	read_size_t(len, size_t);

	buf = get_mem(buf, &buf_nall, len + 1, lbuf, 256, &buf_all);
	if(!buf)
	  CLEANUPR(-ENOMEM);

	i = params->inputfunc(buf, len, params);
	if(i < 1)
	  goto eoferr;
	buf[len] = '\0';

	mk_esc_seq(buf, ESCAPE_CHARACTER, verbosestr);
	if(verbose)
	  strcat(verbosestr, (type == FIFO ? "|" : "["));
	strcat(verbosestr, "\n");
	params->vars.uid = uid;
	params->vars.mtime = mtime;
	verbosefunc(verbosestr, params);

	i = params->inputfunc(&c, 1, params);
	if(c != '.' || i < 1){
	  i = formaterr(&type, params, started);
	  have_an_error = YES;
	  goto tryagain;
	}

	started = YES;
	have_an_error = NO;

	break;

      case BLOCKDEVICE:
      case CHARDEVICE:
	read_uns(mode, Uns32);
	read_time_t(mtime, time_t);
	read_int(uid, Int32);
	read_int(gid, Int32);
	read_uns(rdev, dev_t);
	read_size_t(len, size_t);

	buf = get_mem(buf, &buf_nall, len + 1, lbuf, 256, &buf_all);
	if(!buf)
	  CLEANUPR(-ENOMEM);

	i = params->inputfunc(buf, len, params);
	if(i < 1)
	  goto eoferr;
	buf[len] = '\0';

	mk_esc_seq(buf, ESCAPE_CHARACTER, verbosestr);
	if(verbose)
	  strcat(verbosestr, (type == BLOCKDEVICE ? "#" : "*"));
	strcat(verbosestr, "\n");
	params->vars.uid = uid;
	params->vars.mtime = mtime;
	verbosefunc(verbosestr, params);

	i = params->inputfunc(&c, 1, params);
	if(c != '.' || i < 1){
	  i = formaterr(&type, params, started);
	  have_an_error = YES;
	  goto tryagain;
	}

	started = YES;
	have_an_error = NO;

	break;

      case HARDLINK:
      case SYMLINK:
	read_uns(mode, Uns32);
	read_time_t(mtime, time_t);
	read_int(uid, Int32);
	read_int(gid, Int32);
	read_size_t(len, size_t);

	buf = get_mem(buf, &buf_nall, len + 1, lbuf, 256, &buf_all);
	if(!buf)
	  CLEANUPR(-ENOMEM);

	i = params->inputfunc(buf, len, params);
	if(i < len)
	  goto eoferr;
	buf[len] = '\0';

	i = params->inputfunc(&c, 1, params);
	if(c != ';' || i < 1){
	  i = formaterr(&type, params, started);
	  have_an_error = YES;
	  goto tryagain;
	}

	mk_esc_seq(buf, ESCAPE_CHARACTER, verbosestr);
	if(verbose)
	  sprintf(verbosestr + strlen(verbosestr), T_("%s  (%slink to "),
			(type == SYMLINK ? "@" : "="),
			(type == SYMLINK ? T_("symbolic ") : ""));

	read_size_t(len, size_t);

	buf = get_mem(buf, &buf_nall, len + 1, lbuf, 256, &buf_all);
	if(!buf)
	  CLEANUPR(-ENOMEM);

	i = params->inputfunc(buf, len, params);
	if(i < len)
	  goto eoferr;
	buf[len] = '\0';

	i = params->inputfunc(&c, 1, params);
	if(c != '.' || i < 1){
	  i = formaterr(&type, params, started);
	  have_an_error = YES;
	  goto tryagain;
	}

	if(verbose){
	  mk_esc_seq(buf, ESCAPE_CHARACTER, verbosestr + strlen(verbosestr));
	  strcat(verbosestr, ")");
	}
	strcat(verbosestr, "\n");
	params->vars.uid = uid;
	params->vars.mtime = mtime;
	verbosefunc(verbosestr, params);

	have_an_error = NO;
	started = YES;

	break;

      case REGFILECKS:
	cks = YES;

#ifdef	USE_ZLIB

	if(params->check)
	  crc32sum = crc32(0L, NULL, 0);

#else

	fprintf(stderr, T_("Warning: CRC32 checksumming not available (requires zlib)\n"));

#endif

      case REGFILE:
	read_uns(mode, Uns32);
	read_time_t(mtime, time_t);
	read_int(uid, Int32);
	read_int(gid, Int32);
	read_size_t(len, size_t);

	buf = get_mem(buf, &buf_nall, len + 1, lbuf, 256, &buf_all);
	if(!buf)
	  CLEANUPR(-ENOMEM);

	i = params->inputfunc(buf, len, params);	/* read filename */
	if(i < len)
	  goto eoferr;
	buf[len] = '\0';

	i = params->inputfunc(&c, 1, params);
	if(c != ';' || i < 1){
	  i = formaterr(&type, params, started);
	  have_an_error = YES;
	  goto tryagain;
	}

	read_size_t(len, size_t);		/* read uncompresscmd */
	buf2 = get_mem(buf2, &buf2_nall, len + 1, lbuf2, 256, &buf2_all);
	if(!buf2)
	  CLEANUPR(-ENOMEM);

	i = params->inputfunc(buf2, len, params);
	if(i < len)
	  goto eoferr;
	buf2[len] = '\0';

	i = params->inputfunc(&c, 1, params);
	if(c != ';' || i < 1){
	  i = formaterr(&type, params, started);
	  have_an_error = YES;
	  goto tryagain;
	}

	read_off_t(filelen, off_t);			/* read filesize */

	compressed = (buf2[0] ? YES : NO);
	var_compr = (compressed && filelen == 0) ? YES : NO;
	builtin_uncompress = NO;

	mk_esc_seq(buf, ESCAPE_CHARACTER, verbosestr);
	if(!compressed || !verbose)
	  strcat(verbosestr, "\n");
	else
	  sprintf(verbosestr + strlen(verbosestr),
				T_("  (to be uncompressed by %s)\n"), buf2);
	params->vars.uid = uid;
	params->vars.mtime = mtime;
	verbosefunc(verbosestr, params);

	ucfd = -1;
	biu_error = NO;
	if(params->check){
	  if(compressed){		/* check integrity of compressed */
	    builtin_uncompress = NO;

	    if(buf2[0] == '.' && (! buf2[1] || buf2[1] == ' ')){
		if(buf2[1])
		  memmove(buf2, buf2 + 2, strlen(buf2) - 2 + 1);
		else
		  buf2[0] = '\0';

#ifndef	USE_ZLIB

		fprintf(errfp, T_("Error: Built-in (un)compression not available for file `%s'.\n"), buf);
		compressed = NO;
		buf2[0] = '\0';
		params->vars.errnum |= EUNPACKNOCOMPR;

#else

		builtin_uncompress = YES;

#endif
	    }

	    if(compressed){
	      if(buf2[0]){
		i = pipe(pp);

		if(i){
		  fprintf(errfp,
			T_("Error: Cannot check file `%s' for integrity.\n"),
				buf);
		  params->vars.errnum |= EGENFAULT;
		}
		else{
		  pid = fork_forced();

		  if(pid < 0){
		    fprintf(errfp,
			T_("Error: Cannot check file `%s' for integrity.\n"),
						buf);
		    params->vars.errnum |= EGENFAULT;
		  }
		  else if(! pid){		/* child */
		    char		**unzipargv;
		    int		fd;

		    clr_timer();

		    close(pp[1]);

		    if(cmd2argvq(&unzipargv, buf2)){
			close(pp[0]);
			exit(1);
		    }

		    fd = open(NULLFILE, O_WRONLY | O_BINARY, 0600);
		    if(fd < 0){
			fprintf(stderr, "Fatal: Cannot open " NULLFILE ".\n");
			exit(2);
		    }

		    dup2(pp[0], 0);
		    dup2(fd, 1);
		    dup2(fd, 2);

		    execvp(unzipargv[0], unzipargv + 1);

		    exit(3);
		  }

		  close(pp[0]);

		  ucfd = pp[1];
		}
	      }
	      else{
		ucfd = open(NULLFILE, O_WRONLY | O_BINARY, 0600);
		if(ucfd < 0){
		  fprintf(stderr, "Fatal: Cannot open " NULLFILE ".\n");
		  exit(2);
		}
	      }

#ifdef	USE_ZLIB

	      if(builtin_uncompress){
		i = open_file_unzip(&params->vars.zfile, ucfd);
		if(i){
		  fprintf(errfp, T_("Error: Cannot uncompress `%s'.\n"), buf);
		  close(ucfd);
		  ucfd = -1;
		  params->vars.errnum |= EUNPACKUNCOMPR;
		}
	      }
	      else
		params->vars.zfile.fd = ucfd;

#endif

	    }
	  }
	}

	buf = get_mem(buf, &buf_nall, 4096, lbuf, 256, &buf_all);
	if(!buf)
	  CLEANUPR(-ENOMEM);

	forever{
	  UChar		comprbuf[0x1000];

	  if(var_compr){
	    fmterr = NO;
	    i = params->inputfunc(lbuf, 2, params);
	    if(i < 2){
		fmterr = YES;
		params->vars.errnum |= EUNPACKREAD;
	    }
	    else{
		xref_to_UnsN(&len, lbuf, 16);
		if(len < 0 || len > 0xfff)
		  fmterr = YES;
	    }

	    if(!fmterr){
		i = params->inputfunc(comprbuf, len, params);
		if(i < len){
		  fmterr = YES;
		}
		else{
		  if(ucfd >= 0){
		    j = write_file(ucfd, comprbuf, len, &params->vars.zfile,
					len < 0xfff ? YES : NO, writefunc);
		    if(j < 0)
			biu_error = YES;
		  }

#ifdef	USE_ZLIB

		if(params->check && cks)
		  crc32sum = crc32(crc32sum, comprbuf, i);

#endif

		}
	    }
	    else{
		i = formaterr(&type, params, started);
		have_an_error = YES;
		close(ucfd);
		goto tryagain;
	    }

	    if(len < 0xfff)
		break;

	    continue;
	  }

	  if(filelen < 4096){
	    len = (Int32) filelen;
	    i = params->inputfunc(buf, len, params);
	    if(i < len){
		params->vars.errnum |= EUNPACKREAD;
		i = formaterr(&type, params, started);
		have_an_error = YES;
		close(ucfd);
		goto tryagain;
	    }

	    if(ucfd >= 0){
		j = write_file(ucfd, buf, i, &params->vars.zfile, YES, writefunc);
		if(j < 0)
		  biu_error = YES;
	    }

#ifdef	USE_ZLIB

	    if(params->check && cks)
		crc32sum = crc32(crc32sum, buf, i);

#endif

	    break;
	  }

	  i = params->inputfunc(buf, 4096, params);
	  if(i < 4096){
	    params->vars.errnum |= EUNPACKREAD;
	    i = formaterr(&type, params, started);
	    have_an_error = YES;
	    close(ucfd);
	    goto tryagain;
	  }

	  if(ucfd >= 0){
	    j = write_file(ucfd, buf, i, &params->vars.zfile, NO, writefunc);
	    if(j < 0)
		biu_error = YES;
	  }

#ifdef	USE_ZLIB

	  if(params->check && cks)
	    crc32sum = crc32(crc32sum, buf, i);

#endif

	  filelen -= 4096;
	}

	if(ucfd >= 0){
	  close(ucfd);

	  pst = 0;
	  if(pid > 0){
	    waitpid_forced(pid, &pst, 0);

	    pst = WEXITSTATUS(pst);
	  }

	  if(biu_error || pst){
	    fprintf(stderr,
		T_("Integrity check: This compressed file seems to be corrupted.\n"));
	    params->vars.errnum |= EUNPACKUNCOMPR;
	  }
	}

#ifdef	USE_ZLIB
	if(builtin_uncompress){
	  reset_zfile(&params->vars.zfile);
	}
#endif

	j = (cks ? 4 : 0);
	i = params->inputfunc(lbuf, 1 + j, params);
	if(lbuf[j] != '.' || i < 1 + j){
	  i = formaterr(&type, params, started);
	  have_an_error = YES;
	  goto tryagain;
	}

#ifdef	USE_ZLIB
	if(cks && params->check){
	  xref_to_Uns32(&lu, lbuf);

	  if(lu != crc32sum){
	    fprintf(stderr, T_("Integrity check: Wrong CRC32 checksum.\n"));
		params->vars.errnum |= EUNPACKUNCOMPR;
	  }
	}
#endif

	started = YES;
	have_an_error = NO;

	break;

      case FILECONTENTSCKS:
	cks = YES;

#ifdef	USE_ZLIB

	if(params->check)
	  crc32sum = crc32(0L, NULL, 0);

#else

	fprintf(stderr, T_("Warning: CRC32 checksumming not available (requires zlib)\n"));

#endif

      case FILECONTENTS:
	read_time_t(mtime, time_t);

      case FILECONTENTS_O:
	read_size_t(len, size_t);

	buf = get_mem(buf, &buf_nall, len + 1, lbuf, 256, &buf_all);
	if(!buf)
	  CLEANUPR(-ENOMEM);

	i = params->inputfunc(buf, len, params);	/* read filename */
	if(i < len)
	  goto eoferr;
	buf[len] = '\0';

	i = params->inputfunc(&c, 1, params);
	if(c != ';' || i < 1){
	  i = formaterr(&type, params, started);
	  have_an_error = YES;
	  goto tryagain;
	}

	read_size_t(len, size_t);		/* read uncompresscmd */

	buf2 = get_mem(buf2, &buf2_nall, len + 1, lbuf2, 256, &buf2_all);
	if(!buf2)
	  CLEANUPR(-ENOMEM);

	i = params->inputfunc(buf2, len, params);
	if(i < len)
	  goto eoferr;
	buf2[len] = '\0';

	i = params->inputfunc(&c, 1, params);
	if(c != ';' || i < 1){
	  i = formaterr(&type, params, started);
	  have_an_error = YES;
	  goto tryagain;
	}

	compressed = (buf2[0] ? YES : NO);
	builtin_uncompress = NO;

	mk_esc_seq(buf, ESCAPE_CHARACTER, verbosestr);
	if(verbose)
	  strcat(verbosestr, "<");

	if(!compressed || !verbose)
	  strcat(verbosestr, "\n");
	else
	  sprintf(verbosestr + strlen(verbosestr),
			T_("  (to be uncompressed by %s)\n"), buf2);
	verbosefunc(verbosestr, params);

	biu_error = NO;
	ucfd = -1;
	if(params->check && compressed){	/* check integrity of compressed */
	    builtin_uncompress = NO;

	    if(buf2[0] == '.' && (! buf2[1] || buf2[1] == ' ')){
		if(buf2[1])
		  memmove(buf2, buf2 + 2, strlen(buf2) - 2 + 1);
		else
		  buf2[0] = '\0';

#ifndef	USE_ZLIB

		fprintf(errfp, T_("Error: Built-in (un)compression not available for file `%s'.\n"), buf);
		compressed = NO;
		buf2[0] = '\0';
		params->vars.errnum |= EUNPACKNOCOMPR;

#else

		builtin_uncompress = YES;

#endif

	    }

	    if(buf2[0]){
	      i = pipe(pp);

	      if(i){
		fprintf(errfp,
			T_("Error: Cannot check file `%s' for integrity.\n"),
				buf);
		params->vars.errnum |= EGENFAULT;
	      }
	      else{
		pid = fork_forced();

		if(pid < 0){
		  fprintf(errfp,
			T_("Error: Cannot check file `%s' for integrity.\n"),
						buf);
		  params->vars.errnum |= EGENFAULT;
		}
		else if(! pid){		/* child */
		  char		**unzipargv;
		  int		fd;

		  clr_timer();

		  close(pp[1]);

		  if(cmd2argvq(&unzipargv, buf2)){
		    close(pp[0]);
		    exit(1);
		  }

		  fd = open(NULLFILE, O_WRONLY | O_BINARY, 0600);
		  if(fd < 0)
		    exit(2);

		  dup2(pp[0], 0);
		  dup2(fd, 1);
		  dup2(fd, 2);

		  execvp(unzipargv[0], unzipargv + 1);

		  exit(3);
		}

		close(pp[0]);

		ucfd = pp[1];
	      }
	    }
	    else{
		ucfd = open(NULLFILE, O_WRONLY | O_BINARY, 0600);
		if(ucfd < 0){
		  fprintf(stderr, "Fatal: Cannot open " NULLFILE ".\n");
		  exit(2);
		}
	    }

#ifdef	USE_ZLIB

	    if(builtin_uncompress){
		i = open_file_unzip(&params->vars.zfile, ucfd);
		if(i){
		  fprintf(errfp, T_("Error: Cannot uncompress `%s'.\n"), buf);
		  close(ucfd);
		  ucfd = -1;
		  params->vars.errnum |= EUNPACKUNCOMPR;
		}
	    }
	    else
		params->vars.zfile.fd = ucfd;

#endif

	}

	do{
	  i = params->inputfunc(lbuf2, 1, params);
	  if(i < 1)
	    goto eoferr;
	  cptr = lbuf2 + 1;
	  i = params->inputfunc(cptr, (Int32) lbuf2[0], params);
	  if(i < (Int32) lbuf2[0]){
	    params->vars.errnum |= EUNPACKFORMAT;
	    goto eoferr;
	  }

	  if(ucfd >= 0){
	    j = write_file(ucfd, cptr, i, &params->vars.zfile,
					i < 0xff ? YES : NO, writefunc);
	    if(j < 0)
		biu_error = YES;
	  }

#ifdef	USE_ZLIB

	  if(params->check && cks)
	    crc32sum = crc32(crc32sum, cptr, i);

#endif

	} while(lbuf2[0] == (UChar) 0xff);

	if(ucfd >= 0){
	  close(ucfd);

	  pst = 0;
	  if(pid > 0){
	    waitpid_forced(pid, &pst, 0);
	    pst = WEXITSTATUS(pst);
	  }

	  if(biu_error || pst){
	    fprintf(stderr,
		T_("Integrity check: This compressed file seems to be corrupted.\n"));
	    params->vars.errnum |= EUNPACKUNCOMPR;
	  }
	}

#ifdef	USE_ZLIB
	if(builtin_uncompress){
	  reset_zfile(&params->vars.zfile);
	}
#endif

	j = (cks ? 4 : 0);
	i = params->inputfunc(lbuf, 1 + j, params);
	if(lbuf[j] != '.' || i < 1 + j){
	  i = formaterr(&type, params, started);
	  have_an_error = YES;
	  goto tryagain;
	}

#ifdef	USE_ZLIB
	if(cks && params->check){
	  xref_to_Uns32(&lu, lbuf);

	  if(lu != crc32sum){
	    fprintf(stderr, T_("Integrity check: Wrong CRC32 checksum.\n"));
		params->vars.errnum |= EUNPACKUNCOMPR;
	  }
	}
#endif

	started = YES;
	have_an_error = NO;

	break;

      case SOLARIS2_ACL:		/* just parse */
      case HPUX10_ACL:		/* have the same structure */
	read_int(i, int);
	for(i *= 3; i > 0; i--)
	  read_int(li, Int32);
	
	i = params->inputfunc(&c, 1, params);
	if(c != '.' || i < 1){
	  i = formaterr(&type, params, started);
	  have_an_error = YES;
	  goto tryagain;
	}

	started = YES;
	have_an_error = NO;

	break;

      case NFS4_ACL:
	read_int(i, int);

	for(; i > 0; i -= j){
	  j = MIN(256, i);

	  if(params->inputfunc(lbuf, j, params) < j){
	    i = formaterr(&type, params, started);
	    have_an_error = YES;
	    goto tryagain;
	  }
	}

	i = params->inputfunc(&c, 1, params);
	if(c != '.' || i < 1){
	  i = formaterr(&type, params, started);
	  have_an_error = YES;
	  goto tryagain;
	}

	started = YES;
	have_an_error = NO;

	break;

      case POSIX_ACL:
	read_int(j, int);

	for(i = 0; i < 3; i++){
	  if(j & (1 << i)){
	    read_int(k, int);

	    for(; k > 0; k -= 256){
		n = (k > 256 ? 256 : k);
	    	if(params->inputfunc(lbuf, n, params) < n){
		  i = formaterr(&type, params, started);
		  have_an_error = YES;
		  goto tryagain;
		}

		n = params->inputfunc(&c, 1, params);
		if(c != ';' || n < 1){
		  i = formaterr(&type, params, started);
		  have_an_error = YES;
		  goto tryagain;
		}
	    }
	  }
	}
		  
	i = params->inputfunc(&c, 1, params);
	if(c != '.' || i < 1){
	  i = formaterr(&type, params, started);
	  have_an_error = YES;
	  goto tryagain;
	}

	started = YES;
	have_an_error = NO;

	break;

      case COMMANDINOUTCKS:
	cks = YES;

#ifdef	USE_ZLIB

	if(params->check)
	  crc32sum = crc32(0L, NULL, 0);

#else

	fprintf(stderr, T_("Warning: CRC32 checksumming not available (requires zlib)\n"));

#endif

      case COMMANDINOUT:
	read_int(i, int);

	buf2 = get_mem(buf2, &buf2_nall, i + 1, lbuf2, 256, &buf2_all);
	if(!buf2)
	  CLEANUPR(-ENOMEM);

	j = params->inputfunc(buf2, i, params);
	if(j < i){
	  params->vars.errnum |= EUNPACKREAD;
	  i = formaterr(&type, params, started);
	  have_an_error = YES;
	  goto tryagain;
	}
	buf2[i] = '\0';

	i = params->inputfunc(&c, 1, params);
	if(i < 1 || c != ';'){
	  i = formaterr(&type, params, started);
	  have_an_error = YES;
	  goto tryagain;
	}

	do{
	  UChar		nbuf[8196];

	  i = params->inputfunc(lbuf, 2, params);
	  if(i < 2){
	    params->vars.errnum |= EUNPACKREAD;
	    i = formaterr(&type, params, started);
	    have_an_error = YES;
	    goto tryagain;
	  }

	  xref_to_UnsN(&j, lbuf, 16);
	  if(j > 8192){
	    i = formaterr(&type, params, started);
	    have_an_error = YES;
	    goto tryagain;
	  }

	  i = params->inputfunc(nbuf, j, params);
	  if(i < j){
	    params->vars.errnum |= EUNPACKREAD;
	    i = formaterr(&type, params, started);
	    have_an_error = YES;
	    goto tryagain;
	  }

#ifdef	USE_ZLIB

	  if(params->check && cks)
	    crc32sum = crc32(crc32sum, nbuf, i);

#endif

	} while(j == 8192);

	j = (cks ? 4 : 0);
	i = params->inputfunc(lbuf, 1 + j, params);
	if(lbuf[j] != '.' || i < 1 + j){
	  i = formaterr(&type, params, started);
	  have_an_error = YES;
	  goto tryagain;
	}

#ifdef	USE_ZLIB
	if(cks && params->check){
	  xref_to_Uns32(&lu, lbuf);

	  if(lu != crc32sum){
	    fprintf(stderr, T_("Integrity check: Wrong CRC32 checksum.\n"));
		params->vars.errnum |= EUNPACKUNCOMPR;
	  }
	}
#endif

	strcpy(verbosestr, buf2);
	strcat(verbosestr, "\n");

	verbosefunc(verbosestr, params);

	started = YES;
	have_an_error = NO;

	break;

      default:
	i = formaterr(&type, params, started);
	have_an_error = YES;
	goto tryagain;
    }
  }

 cleanup:
  if(buf_all)
    free(buf);
  if(buf2_all)
    free(buf2);

  return(r);

 eoferr:

  fprintf(errfp, T_("Error: unexpected end of file.\n"));
  params->vars.errnum |= EUNPACKREAD;

  return(EOF);
}

static Int32
check_dir_writable(UChar * name, AarParams * params)
{
  uid_t		ceuid;
  gid_t		cegid, *cegids;
  int		negids;
  Int32		i;

  if(!params->uid && !params->gid && !params->ngids)	/* superuser */
    return(0);

  cegids = NULL;
  negids = 0;

  ceuid = geteuid();
  cegid = getegid();
  if(get_groups(&negids, &cegids)){
    params->vars.errnum |= EGENFAULT;
    return(-1);
  }

  i = set_eff_ugids(params->uid, params->gid,
				params->ngids, params->gids);
  if(!i)
    i = eaccess(name, W_OK);

  set_eff_ugids(ceuid, cegid, negids, cegids);	/* reset effective IDs */
  ZFREE(cegids);

  if(i){
    params->vars.errnum |= EUNPACKACCESS;
    return(-1);
  }

  return(0);
}

static Int32
make_check_basedir(UChar * name, Uns32 mode, Int32 uid, Int32 gid,
		AarParams * params)
{
  UChar		*cptr;
  struct stat	statb;
  Int32		res, i, statres;
  FILE		*errfp;

  errfp = params->errfp;
  res = 0;

  if(FN_ISROOTDIR(name)){
    if(params->check)
	return(check_dir_writable(name, params));

    return(0);
  }

  cptr = FN_LASTDIRDELIM(name);
  if(cptr){
    *cptr = '\0';

    if( (statres = stat(name, &statb)) ){
      statres = lstat(name, &statb);
    }

    if(!statres){
      if(IS_DIRECTORY(statb)){
	if(params->check){
	  i = check_dir_writable(name, params);
	  if(i)
	    return(i);
	}
      }
      else{
	if(params->unlink){
	  unlink(name);
	}
	else{
	  if(IS_SYMLINK(statb)){
	    char	linkb[BUFFERSIZ + 1];

	    i = readlink(name, linkb, BUFFERSIZ);
	    if(i >= 0)
		linkb[i] = '\0';
	    fprintf(errfp,
		T_("Warning: removing symlink `%s' pointing to unaccessible destination `%s'.\n"),
			name, i >= 0 ? linkb : T_("<symlink-unreadable>"));
	    unlink(name);
	  }
	}
      }
    }

    if(lstat(name, &statb) < 0){
	if( (res = make_check_basedir(name, mode, uid, gid, params)) ){
	  *cptr = FN_DIRSEPCHR;
	  return(-errno);
	}

	res = mkdir(name, mode);
	if(res){
	  fprintf(errfp, T_("Error: cannot create directory `%s'.\n"), name);
	  *cptr = FN_DIRSEPCHR;
	  params->vars.errnum |= EUNPACKMKDIR;
	  return(-errno);
	}

	if(!params->ignoreown){
	  res = chown(name, uid, gid);
	  if(res){
	    fprintf(errfp, T_("Error: cannot chown of directory `%s'.\n"), name);
	    *cptr = FN_DIRSEPCHR;
	    params->vars.errnum |= EUNPACKMKDIR;
	    return(-errno);
	  }
	}
    }

    *cptr = FN_DIRSEPCHR;

    return(res);
  }

  if(params->check)
    return(check_dir_writable(FN_CURDIR, params));

  return(0);
}

void
set_props(UChar * buf, Int32 uid, Int32 gid,
		time_t mtime, Uns32 mode, void * acls,
		AarParams * params)
{
  struct utimbuf	utim;
  Int32			r;

  if(!params->ignoreown){
    if(chown(buf, uid, gid)){
      fprintf(params->errfp, T_("Warning: cannot chown %s\n"), buf);
    }
  }
  /*else*/{
    SETZERO(utim);	/* on some architectures utim is more complex */
    utim.actime = utim.modtime = mtime;
    if(utime(buf, &utim)){
	fprintf(params->errfp,
		T_("Warning: cannot set modificaton time on %s.\n"), buf);
	params->vars.errnum |= EUNPACKSETPROPS;
    }
    /*else*/{
	if(chmod(buf, mode)){
	  fprintf(params->errfp,
		T_("Warning: cannot set permissions on %s.\n"), buf);
	  params->vars.errnum |= EUNPACKSETPROPS;
	}
	/*else*/ if(acls){
	  switch(((UnknownACL *) acls)->acltype){
	    case SOLARIS2_ACL:

#ifdef	USE_SOLARIS2_ACLS
		if(acl(buf, SETACL, ((Solaris2ACL *) acls)->nacls,
			(aclent_t *)(((Solaris2ACL *) acls)->acls)) < 0){
		  Int32		i, n, t, na, id;	/* probably NFS-4: */
		  aclent_t	*aclptr, tmpacl;	/* skip unresolvable IDs */

		  n = na = ((Solaris2ACL *) acls)->nacls;
		  aclptr = (aclent_t *)(((Solaris2ACL *) acls)->acls);

		  for(i = 0; i < n; i++){
		    t = aclptr[i].a_type;
		    id = aclptr[i].a_id;
		    r = 0;
		    if(t & (USER_OBJ | USER | DEF_USER | DEF_USER_OBJ))
			if(!getpwuid(id))
			  r = 1;
		    if(t & (GROUP_OBJ | GROUP | DEF_GROUP | DEF_GROUP_OBJ))
			if(!getgrgid(id))
			  r = 2;
		    if(r){
			memcpy(&tmpacl, aclptr + i, sizeof(aclent_t));
			memmove(aclptr + i, aclptr + i + 1,
					sizeof(aclent_t) * (na - i - 1));
			tmpacl.a_perm = r;	/* misuse a_perm for u/g */
			memcpy(aclptr + na - 1, &tmpacl, sizeof(aclent_t));
			n--;
			i--;
		    }
		  }

		  if(n < na && acl(buf, SETACL, n,
			(aclent_t *)(((Solaris2ACL *) acls)->acls)) >= 0){
		    fprintf(params->errfp, T_("Warning: Skipping ACL entry for `%s', IDs cannot be resolved to names:"),
				buf);
		    for(i = n; i < na; i++){
			fprintf(params->errfp, " %s %d",
				(aclptr[i].a_perm == 1 ? T_("User-ID") : T_("Group-ID")),
					(int) aclptr[i].a_id);
		    }
		    fprintf(stderr, "\n");
		    params->vars.errnum |= EUNPACKACLIDS;
		  }
		  else{
#endif

		    fprintf(params->errfp,
			T_("Warning: cannot set Solaris2 ACLs on %s.\n"), buf);
		    params->vars.errnum |= EUNPACKACLS;

#ifdef	USE_SOLARIS2_ACLS
		  }
		}
#endif

		break;

	    case NFS4_ACL:

#ifdef	USE_NFS4_ACLS
		if(acl_set(buf, (acl_t *)(((NFS4ACL *) acls)->acls)) < 0)

#endif
		{
		  fprintf(params->errfp,
			T_("Warning: cannot set NFS4 ACLs on %s.\n"), buf);
		  params->vars.errnum |= EUNPACKACLS;
		}

		break;

	    case HPUX10_ACL:

#ifdef	USE_HPUX10_ACLS
		if(setacl(buf, ((HPUX10ACL *) acls)->nacls,
			(struct acl_entry *)(((HPUX10ACL *) acls)->acls)) < 0)
#endif
		{
		  fprintf(params->errfp,
			T_("Warning: cannot set HPUX10 ACLs on %s.\n"), buf);
		  params->vars.errnum |= EUNPACKACLS;
		}
		break;

	    case POSIX_ACL:

#ifdef	USE_POSIX_ACLS
	      {
		Int32	i, acl_types;

		for(i = r = 0; i < 3; i++){
		  acl_types = ((POSIXACL *) acls)->acl_types;

		  if(acl_types & (1 << i)){
		    r = acl_set_file(buf, packer_posix_acl_types(i),
				(acl_t) ((POSIXACL *) acls)->acl_arr[i]);
		    if(r)
			break;
		  }
		}
	      }
		if(r)
#endif
		{
		  fprintf(params->errfp,
			T_("Warning: cannot set POSIX ACLs on %s.\n"), buf);
		  params->vars.errnum |= EUNPACKACLS;
		}
		break;

	    default:
		fprintf(params->errfp, T_("Warning: Unkknown ACL type %d.\n"),
			((UnknownACL *) acls)->acltype);
		params->vars.errnum |= EUNPACKACLS;
	  }
	}
    }
  }
}

static void
convtorel(UChar * buf, Int32 rel)
{
  cleanpath(buf);

  if(rel)
    mkrelpath(buf);
}

/* prepend absolute path with as many ../ as needed to get to the
 * root directory
 *
 * path is the path of a symolic link and target where it points to
 */
static UChar *
convtorelcwd(
  UChar		*target,
  UChar		*path,
  Int32		tgt_space,
  Int32		rel)
{
  UChar		*cptr, *abspath;
  Int32		nds, upspec_len;

  convtorel(path, 0);

  if(!FN_ISABSPATH(target) || !rel)
    return(target);

  abspath = path;

  if(!FN_ISABSPATH(path)){
    abspath = mkabspath(path, NULL);
    if(!abspath)
	return(NULL);
  }

  cleanpath(abspath);

  for(nds = -1, cptr = FN_FIRSTDIRSEP(abspath); *cptr; cptr++){
    if(FN_ISDIRSEP(*cptr))
	nds++;
  }

  upspec_len = nds * (strlen(FN_DIRSEPSTR) + strlen(FN_PARENTDIR));
  cptr = ZRENEWP((path == abspath ? NULL : abspath), UChar,
			upspec_len + strlen(target) + 1);
  if(!cptr){
    if(path != abspath)
	free(abspath);
    return(NULL);
  }

  for(strcpy(cptr, ""); nds > 0; nds--)
    strcat(cptr, FN_PARENTDIR FN_DIRSEPSTR);
  strcat(cptr, target);
  mkrelpath(cptr + upspec_len);

  if(strlen(cptr) + 1 > tgt_space)
    return(cptr);

  strcpy(target, cptr);
  free(cptr);

  return(target);
}

static Int32
pack_readin_l(UChar ** filelist, AarParams * params, UChar * files_found)
{
  UChar		lbuf[256], lbuf2[256];
  UChar		*buf = NULL, *buf2 = NULL;
  Int32		buf_nall = 0, buf2_nall = 0;
  Flag		buf_all = NO, buf2_all = NO;
  UChar		verbosestr[MAXPATHLEN * 4 + 100], c;
  Flag		started, have_an_error, var_compr, fmterr, try_cont;
  Flag		builtin_uncompress, compressed, skip, fault;
  Uns8		verbose;
  Int32		type, i, j, k, n, gid, uid, fd, r, rvr, ifd;
  Uns32		mode, lu;
  time_t	mtime = 0, ltt;
  size_t	len, stt;
  off_t		filelen, ott;
  FILE		*errfp;
  UChar		**files, *cptr, **fileidx;
  Int32		li, n_files;
  dev_t		rdev;
  int		pfd, pid, pst, pp[2];
  int		num_written = 0;
  void		(*verbosefunc)(UChar *, struct aar_params *);
  Int64		(*writefunc)(int, UChar *, Int64);
  struct stat	statb;
  void		*acls = NULL;
  Flag		cks;
  Uns32		crc32sum;	/* uninitialized OK */

  verbose = params->verbose;
  verbosefunc = params->verbosefunc;
  writefunc = params->writefunc;

  files = filelist;
  i = r = n_files = 0;

  ifd = params->infd;

  if(files){
    while(*files){
      cleanpath(*files);
      repl_esc_seq(*files, ESCAPE_CHARACTER);

      files++, i++;
    }

    files = NEWP(UChar *, i + 1);
    if(!files)
      return(-ENOMEM);
    memset(files, 0, (i + 1) * sizeof(UChar *));

    n_files = i;

    while(i > 0){
      i--;
      if(params->relative && FN_ISABSPATH(filelist[i])){
	cptr = filelist[i];
	cptr = FN_FIRSTDIRSEP(cptr);
	while(FN_ISDIRSEP(*cptr))
	  cptr++;
	files[i] = strdup(cptr);
      }
      else
	files[i] = strdup(filelist[i]);

      if(!files[i]){
	for(i++; i < n_files; i++)
	  free(files[i]);
	free(files);
	CLEANUPR(-ENOMEM);
      }
    }

    q_sort(files, n_files, sizeof(UChar *), cmp_UCharPTR);
  }

  errfp = params->errfp;

  started = have_an_error = NO;

  forever{
    pid = -1;

    if(filelist && !params->recursive){
	for(fileidx = filelist, cptr = files_found;
					*fileidx; fileidx++, cptr++){
	  if(! (*cptr))
	    break;
	}

	if(! (*fileidx))
	  goto cleanup;
    }

    if(params->pre_verbosefunc)
	(*params->pre_verbosefunc)(NULL, params);

    i = get_int(params, &li);
    type = (Int32) li;

   tryagain:

    ZFREE_ACL_STRUCT(acls, params);

    if(i < 1){
	if(i < 0){
	  r = EOF;
	  goto cleanup;
	}

	i = formaterr(&type, params, started);
	have_an_error = YES;
	goto tryagain;
    }

    skip = NO;

    type = (ABS(type) & PACK_TYPEMASK) * SIGN(type); 

    if(type == SOLARIS2_ACL){
	Int32	t, id, p;

	read_int(j, Int32);

	acls = NEWP(Solaris2ACL, 1);
	if(!acls){
	  r = -ENOMEM;
	  goto cleanup;
	}

	((Solaris2ACL *) acls)->acls = NULL;

#ifdef	USE_SOLARIS2_ACLS
	if(type == SOLARIS2_ACL){
	  ((Solaris2ACL *) acls)->acls = NEWP(aclent_t, j);
	}
#endif

	((Solaris2ACL *) acls)->acltype = SOLARIS2_ACL;

	for(i = 0; i < j; i++){
	  read_int(t, Int32);
	  read_int(id, Int32);
	  read_int(p, Int32);

#ifdef	USE_SOLARIS2_ACLS
	  if(type == SOLARIS2_ACL){
	    ((aclent_t *)(((Solaris2ACL *) acls)->acls))[i].a_type = t;
	    ((aclent_t *)(((Solaris2ACL *) acls)->acls))[i].a_id = id;
	    ((aclent_t *)(((Solaris2ACL *) acls)->acls))[i].a_perm = p;
	  }
#endif
	}

#ifdef	USE_SOLARIS2_ACLS
	((Solaris2ACL *) acls)->nacls = j;
#else
	((Solaris2ACL *) acls)->nacls = 0;
#endif

	i = params->inputfunc(&c, 1, params);
	if(c != '.' || i < 1){
	  i = formaterr(&type, params, started);
	  have_an_error = YES;
	  goto tryagain;
	}

	i = get_int(params, &li);
	type = (Int32) li;

	started = YES;
	have_an_error = NO;
    }
    if(type == NFS4_ACL){
	read_int(k, Int32);

	acls = NEWP(NFS4ACL, 1);
	if(!acls){
	  r = -ENOMEM;
	  goto cleanup;
	}

	((Solaris2ACL *) acls)->acls = NULL;
	buf = get_mem(buf, &buf_nall, k + 1, lbuf, 256, &buf_all);
	if(!buf)
	  CLEANUPR(-ENOMEM);

	j = params->inputfunc(buf, k, params);
	if(j < k){
	  i = formaterr(&type, params, started);
	  have_an_error = YES;
	  goto tryagain;
	}
	buf[k] = '\0';

#ifdef	USE_NFS4_ACLS

	if(acl_fromtext(buf, (acl_t **)(&(((NFS4ACL *) acls)->acls))))
	  CLEANUPR(-ENOMEM);

#endif

	((NFS4ACL *) acls)->acltype = NFS4_ACL;

	i = params->inputfunc(&c, 1, params);
	if(c != '.' || i < 1){
	  i = formaterr(&type, params, started);
	  have_an_error = YES;
	  goto tryagain;
	}

	i = get_int(params, &li);
	type = (Int32) li;

	started = YES;
	have_an_error = NO;
    }
    else if(type == HPUX10_ACL){
	Int32	u, g, m;

	read_int(j, Int32);

	acls = NEWP(HPUX10ACL, 1);
	if(!acls){
	  r = -ENOMEM;
	  goto cleanup;
	}

#ifdef	USE_HPUX10_ACLS
	((HPUX10ACL *) acls)->nacls = j;
	((HPUX10ACL *) acls)->acls = NEWP(struct acl_entry, j);
#else
	((HPUX10ACL *) acls)->nacls = 0;
	((HPUX10ACL *) acls)->acls = NULL;
#endif

	((HPUX10ACL *) acls)->acltype = HPUX10_ACL;

	for(i = 0; i < j; i++){
	  read_int(u, Int32);
	  read_int(g, Int32);
	  read_int(m, Int32);

#ifdef	USE_HPUX10_ACLS
	  ((struct acl_entry *)(((HPUX10ACL *) acls)->acls))[i].uid = u;
	  ((struct acl_entry *)(((HPUX10ACL *) acls)->acls))[i].gid = g;
	  ((struct acl_entry *)(((HPUX10ACL *) acls)->acls))[i].mode = m;
#endif
	}
	
	i = params->inputfunc(&c, 1, params);
	if(c != '.' || i < 1){
	  i = formaterr(&type, params, started);
	  have_an_error = YES;
	  goto tryagain;
	}

	i = get_int(params, &li);
	type = (Int32) li;

	started = YES;
	have_an_error = NO;
    }
    else if(type == POSIX_ACL){
	Int32	acl_types;

	read_int(j, Int32);

	acls = NEWP(POSIXACL, 1);
	if(!acls){
	  r = -ENOMEM;
	  goto cleanup;
	}

	((POSIXACL *) acls)->acltype = POSIX_ACL;

	((POSIXACL *) acls)->acl_types = acl_types = j;

	for(i = 0; i < 3; i++){
	  if(acl_types & (1 << i)){
	    read_int(k, Int32);

	    buf = get_mem(buf, &buf_nall, k + 1, lbuf, 256, &buf_all);
	    if(!buf)
		CLEANUPR(-ENOMEM);

	    j = params->inputfunc(buf, k, params);
	    if(j < k){
		i = formaterr(&type, params, started);
		have_an_error = YES;
		goto tryagain;
	    }

#ifdef	USE_POSIX_ACLS
	    buf[k] = '\0';
	    if(!(((POSIXACL *) acls)->acl_arr[i] = acl_from_text(buf))){
		i = formaterr(&type, params, started);
		have_an_error = YES;
		goto tryagain;
	    }
#else
	    ((POSIXACL *) acls)->acl_arr[i] = NULL;
#endif

	    j = params->inputfunc(buf, 1, params);
	    if(j < 1 || buf[0] != ';'){
		i = formaterr(&type, params, started);
		have_an_error = YES;
		goto tryagain;
	    }
	  }
	}

	j = params->inputfunc(lbuf, 1, params);
	if(j < 1 || lbuf[0] != '.'){
	  i = formaterr(&type, params, started);
	  have_an_error = YES;
	  goto tryagain;
	}

	i = get_int(params, &li);
	type = (Int32) li;

	started = YES;
	have_an_error = NO;
    }

    cks = NO;

    switch(type){
      case ENDOFARCHIVE:
#if 0
	if(filelist){
	  for(fileidx = filelist, cptr = files_found;
					*fileidx; fileidx++, cptr++){
	    if(! (*cptr)){
		fprintf(params->errfp,
			T_("%s not found in archive.\n"), *fileidx);
	    }
	  }
	}
#endif
	if(have_an_error){
	  i = formaterr(&type, params, started);
	  goto tryagain;
	}

	goto cleanup;

      case INFORMATION:
	read_size_t(len, size_t);

	buf = get_mem(buf, &buf_nall, len + 1, lbuf, 256, &buf_all);
	if(!buf)
	    CLEANUPR(-ENOMEM);

	i = params->inputfunc(buf, len, params);
	if(i < len)
	  goto eoferr;

	i = params->inputfunc(&c, 1, params);
	if(c != '.' || i < 1){
	  i = formaterr(&type, params, started);
	  have_an_error = YES;
	  goto tryagain;
	}
	buf[len] = '\0';
	if(verbose && verbosefunc)
	  verbosefunc(buf, params);

	started = YES;
	have_an_error = NO;

	break;
	
      case DIRECTORY:
	read_uns(mode, Uns32);
	read_time_t(mtime, time_t);
	read_int(uid, Int32);
	read_int(gid, Int32);
	read_size_t(len, size_t);

	buf = get_mem(buf, &buf_nall, len + 1, lbuf, 256, &buf_all);
	if(!buf)
	    CLEANUPR(-ENOMEM);

	i = params->inputfunc(buf, len, params);
	if(i < len)
	  goto eoferr;
	buf[len] = '\0';
	convtorel(buf, params->relative);

	i = params->inputfunc(&c, 1, params);
	if(c != '.' || i < 1){
	  i = formaterr(&type, params, started);
	  have_an_error = YES;
	  goto tryagain;
	}

	started = YES;
	have_an_error = NO;

	if(! name_in_list(buf, files, n_files, params->recursive, files_found)
			|| invalid_time_or_uid(mtime, uid, params)
			|| (params->uid && not_allowed(buf, params)))
	  skip = YES;

	name_in_list(buf, files, n_files, 0, files_found);

	if(!skip){
	  j = make_check_basedir(buf, 0700, uid, gid, params);

	  if(! lstat(buf, &statb)){
	    if(!IS_DIRECTORY(statb) && params->unlink){
		unlink(buf);
	    }
	  }

	  i = mkdir(buf, mode);
	  if(j || (i && errno != EEXIST)){
	    params->vars.errnum |= EUNPACKMKDIR;
	    fprintf(errfp, T_("Error: cannot create directory `%s'\n"), buf);
	  }
	  else{
	    set_props(buf, uid, gid, mtime, mode, acls, params);
	    if(verbose && verbosefunc){
		mk_esc_seq(buf, ESCAPE_CHARACTER, verbosestr);
		strcat(verbosestr, "\n");
		params->vars.uid = uid;
		params->vars.mtime = mtime;
		verbosefunc(verbosestr, params);
	    }

	  }
	}

	break;

#ifndef _WIN32
      case FIFO:
      case SOCKET:
	read_uns(mode, Uns32);
	read_time_t(mtime, time_t);
	read_int(uid, Int32);
	read_int(gid, Int32);
	read_size_t(len, size_t);

	buf = get_mem(buf, &buf_nall, len + 1, lbuf, 256, &buf_all);
	if(!buf)
	    CLEANUPR(-ENOMEM);

	i = params->inputfunc(buf, len, params);
	if(i < len)
	  goto eoferr;
	buf[len] = '\0';
	convtorel(buf, params->relative);

	i = params->inputfunc(&c, 1, params);
	if(c != '.' || i < 1){
	  i = formaterr(&type, params, started);
	  have_an_error = YES;
	  goto tryagain;
	}

	started = YES;
	have_an_error = NO;

	if(! name_in_list(buf, files, n_files, params->recursive, files_found)
			|| invalid_time_or_uid(mtime, uid, params)
			|| (params->uid && not_allowed(buf, params)))
	  skip = YES;

	name_in_list(buf, files, n_files, 0, files_found);

	if(! skip){
	  if(make_check_basedir(buf, 0700, uid, gid, params)){
	    params->vars.errnum |= EUNPACKMKDIR;
	    fprintf(errfp, T_("Error: cannot create directory `%s'\n"), buf);
	  }
	  else{
	    if(params->unlink)
		unlink(buf);

	    if(type == FIFO){
		i = mkfifo(buf, mode);
	    }
	    else{
		if( (i = create_unix_socket(buf)) >= 0){
		  close(i);
		  i = 0;
		}
	    }
	    if(i){
		params->vars.errnum |= EUNPACKMKENT;
		fprintf(errfp, T_("Error: cannot create %s `%s'\n"),
			(type == FIFO ? "named pipe" : "socket"), buf);
	    }
	    else{
	      set_props(buf, uid, gid, mtime, mode, acls, params);
	      if(verbose && verbosefunc){
		mk_esc_seq(buf, ESCAPE_CHARACTER, verbosestr);
		strcat(verbosestr, "\n");
		verbosefunc(verbosestr, params);
		params->vars.uid = uid;
		params->vars.mtime = mtime;
	      }
	    }
	  }
	}

	break;

      case BLOCKDEVICE:
      case CHARDEVICE:
	read_uns(mode, Uns32);
	read_time_t(mtime, time_t);
	read_int(uid, Int32);
	read_int(gid, Int32);
	read_uns(rdev, dev_t);
	read_size_t(len, size_t);

	buf = get_mem(buf, &buf_nall, len + 1, lbuf, 256, &buf_all);
	if(!buf)
	    CLEANUPR(-ENOMEM);

	i = params->inputfunc(buf, len, params);
	if(i < len)
	  goto eoferr;
	buf[len] = '\0';
	convtorel(buf, params->relative);

	i = params->inputfunc(&c, 1, params);
	if(c != '.' || i < 1){
	  i = formaterr(&type, params, started);
	  have_an_error = YES;
	  goto tryagain;
	}

	started = YES;
	have_an_error = NO;

	if(! name_in_list(buf, files, n_files, params->recursive, files_found)
			|| invalid_time_or_uid(mtime, uid, params)
			|| (params->uid && not_allowed(buf, params)))
	  skip = YES;

	name_in_list(buf, files, n_files, 0, files_found);

	if(! skip){
	  if(make_check_basedir(buf, 0700, uid, gid, params)){
	    params->vars.errnum |= EUNPACKMKDIR;
	    fprintf(errfp, T_("Error: cannot create directory `%s'\n"), buf);
	  }
	  else{
	    if(params->unlink)
		unlink(buf);

	    if(mknod(buf, mode, rdev)){
		params->vars.errnum |= EUNPACKMKENT;
		fprintf(errfp, T_("Error: cannot create device `%s'\n"), buf);
	    }
	    else{
	      set_props(buf, uid, gid, mtime, mode, acls, params);
	      if(verbose && verbosefunc){
		mk_esc_seq(buf, ESCAPE_CHARACTER, verbosestr);
		strcat(verbosestr, "\n");
		params->vars.uid = uid;
		params->vars.mtime = mtime;
		verbosefunc(verbosestr, params);
	      }
	    }
	  }
	}

	break;
#endif

      case HARDLINK:
      case SYMLINK:
	read_uns(mode, Uns32);
	read_time_t(mtime, time_t);
	read_int(uid, Int32);
	read_int(gid, Int32);
	read_size_t(len, size_t);

	buf = get_mem(buf, &buf_nall, len + 1, lbuf, 256, &buf_all);
	if(!buf)
	    CLEANUPR(-ENOMEM);

	i = params->inputfunc(buf, len, params);
	if(i < 1)
	  goto eoferr;
	buf[len] = '\0';

	i = params->inputfunc(&c, 1, params);
	if(c != ';' || i < 1){
	  i = formaterr(&type, params, started);
	  have_an_error = YES;
	  goto tryagain;
	}
	if(params->relative >= 3)
	  convtorel(buf, params->relative);

	read_int(len, Int32);

	buf2 = get_mem(buf2, &buf2_nall, len + 1, lbuf2, 256, &buf2_all);
	if(!buf2)
	  CLEANUPR(-ENOMEM);

	i = params->inputfunc(buf2, len, params);
	if(i < len)
	  goto eoferr;
	buf2[len] = '\0';
	if(type == SYMLINK){
	  if(params->relative > 1){
	    cptr = convtorelcwd(buf2, buf,
			(buf2 == lbuf2 ? 256 : buf2_nall), params->relative);
	    if(!cptr)
		CLEANUPR(-ENOMEM);
	    if(cptr != buf2){
		if(buf2 != lbuf2)
		  free(buf2);
		buf2 = cptr;
		buf2_nall = strlen(buf2) + 1;
		buf2_all = YES;
	    }
	  }
	}
	else{
	  convtorel(buf2, params->relative);
	}
	if(params->relative < 3)
	  convtorel(buf, params->relative);

	i = params->inputfunc(&c, 1, params);
	if(c != '.' || i < 1){
	  i = formaterr(&type, params, started);
	  have_an_error = YES;
	  goto tryagain;
	}

	started = YES;
	have_an_error = NO;

	if(! name_in_list(buf, files, n_files, params->recursive, files_found)
			|| invalid_time_or_uid(mtime, uid, params)
			|| (params->uid && not_allowed(buf, params)))
	  skip = YES;

	name_in_list(buf, files, n_files, 0, files_found);

	if(! skip){
	  if(params->unlink)
	    unlink(buf);

	  if(make_check_basedir(buf, 0700, uid, gid, params)){
	    params->vars.errnum |= EUNPACKMKDIR;
	    fprintf(errfp, T_("Error: cannot create directory `%s'\n"), buf);
	  }
	  else{
	    if(type == HARDLINK){
	      if(link(buf2, buf)){
		params->vars.errnum |= EUNPACKMKENT;
		fprintf(errfp, T_("Error: cannot create link `%s' to `%s'.\n"),
				buf, buf2);
	      }
	      else{
		set_props(buf, uid, gid, mtime, mode, acls, params);
		if(verbose && verbosefunc){
		  mk_esc_seq(buf, ESCAPE_CHARACTER, verbosestr);
		  strcat(verbosestr, "\n");
		  params->vars.uid = uid;
		  params->vars.mtime = mtime;
		  verbosefunc(verbosestr, params);
		}
	      }
	    }
	    if(type == SYMLINK){
	      if(symlink(buf2, buf)){
		params->vars.errnum |= EUNPACKMKENT;
		fprintf(errfp, T_("Error: cannot create symlink `%s' to `%s'.\n"),
				buf, buf2);
	      }
	      else{
		/*set_props(buf, uid, gid, mtime, mode, acls, params);*/
#ifdef	HAVE_LCHOWN
		if(!params->ignoreown){
		  if(lchown(buf, uid, gid)){
		    params->vars.errnum |= EUNPACKSETPROPS;
		    fprintf(params->errfp, T_("Warning: cannot lchown %s\n"), buf);
		  }
		}
#endif
		if(verbose && verbosefunc){
		  mk_esc_seq(buf, ESCAPE_CHARACTER, verbosestr);
		  strcat(verbosestr, "\n");
		  params->vars.uid = uid;
		  params->vars.mtime = mtime;
		  verbosefunc(verbosestr, params);
		}
	      }
	    }
	  }
	}

	break;

      case REGFILECKS:
	cks = YES;

#ifdef	USE_ZLIB

	crc32sum = crc32(0L, NULL, 0);

#else

	fprintf(stderr, T_("Warning: CRC32 checksumming not available (requires zlib)\n"));

#endif

      case REGFILE:
	read_uns(mode, Uns32);
	read_time_t(mtime, time_t);
	read_int(uid, Int32);
	read_int(gid, Int32);
	read_size_t(len, size_t);

	buf = get_mem(buf, &buf_nall, len + 1, lbuf, 256, &buf_all);
	if(!buf)
	    CLEANUPR(-ENOMEM);

	i = params->inputfunc(buf, len, params);	/* read filename */
	if(i < len)
	  goto eoferr;
	buf[len] = '\0';
	convtorel(buf, params->relative);

	i = params->inputfunc(&c, 1, params);
	if(c != ';' || i < 1){
	  i = formaterr(&type, params, started);
	  have_an_error = YES;
	  goto tryagain;
	}

	read_size_t(len, size_t);		/* read uncompresscmd */

	buf2 = get_mem(buf2, &buf2_nall, len + 1, lbuf2, 256, &buf2_all);
	if(!buf2)
	  CLEANUPR(-ENOMEM);

	i = params->inputfunc(buf2, len, params);
	if(i < len)
	  goto eoferr;
	buf2[len] = '\0';

	i = params->inputfunc(&c, 1, params);
	if(c != ';' || i < 1){
	  i = formaterr(&type, params, started);
	  have_an_error = YES;
	  goto tryagain;
	}

#ifdef	USE_ZLIB

	reset_zfile(&params->vars.zfile);

#endif

	read_off_t(filelen, off_t);		/* read filesize */

	compressed = (buf2[0] ? YES : NO);
	var_compr = (compressed && filelen == 0) ? YES : NO;
	builtin_uncompress = NO;

	if(! name_in_list(buf, files, n_files, params->recursive, files_found)
			|| invalid_time_or_uid(mtime, uid, params)
			|| (params->uid && not_allowed(buf, params)))
	  skip = YES;

	name_in_list(buf, files, n_files, 0, files_found);

	if(!skip){
	  if(make_check_basedir(buf, 0700, uid, gid, params)){
	    params->vars.errnum |= EUNPACKMKDIR;
	    fprintf(errfp, T_("Error: cannot create directory `%s'\n"), buf);
	    skip = YES;
	  }
	}

	fault = NO;
	fd = -10;

	if(skip)
	  fault = YES;

	if(! params->unlink && ! fault){
	  if(! stat(buf, &statb)){
	    params->vars.errnum |= EUNPACKEXISTS;
	    fprintf(errfp, T_("Error: must not delete existing `%s'.\n"), buf);
	    fault = YES;
	  }
	}

	if(! compressed){		/* not compressed */
	  if(!fault){
	    unlink(buf);

	    fd = open(buf, O_WRONLY | O_CREAT | O_BINARY, 0600);
	    if(fd < 0){
	      fd = -10;
	      fault = YES;
	      fprintf(errfp, T_("Error: Cannot create file `%s'.\n"), buf);
	      params->vars.errnum |= EUNPACKMKENT;
	    }
	  }

	  forever{
	    if(filelen < 4096){
	      len = (Int32) filelen;

	      buf2 = get_mem(buf2, &buf2_nall, len, lbuf2, 256, &buf2_all);
	      if(!buf2)
		CLEANUPR(-ENOMEM);

	      i = params->inputfunc(buf2, len, params);
	      if(i < len){
		if(fd != -10)
		  close(fd);
		goto eoferr;
	      }

#ifdef	USE_ZLIB

	      if(cks)
		crc32sum = crc32(crc32sum, buf2, i);

#endif

	      if(!fault){
	        if(len != writefunc(fd, buf2, len)){
		  params->vars.errnum |= EUNPACKWRITE;
		  fprintf(errfp, T_("Error: writing to `%s' failed.\n"), buf);
	        }
	      }
	      break;
	    }

	    buf2 = get_mem(buf2, &buf2_nall, 4096, lbuf2, 256, &buf2_all);
	    if(!buf2)
		CLEANUPR(-ENOMEM);

	    i = params->inputfunc(buf2, 4096, params);
	    if(i < 4096){
	      if(fd != -10)
	        close(fd);
	      goto eoferr;
	    }
	    if(!fault){
	      if(writefunc(fd, buf2, 4096) != 4096){
		params->vars.errnum |= EUNPACKWRITE;
	        fprintf(errfp, T_("Error: writing to `%s' failed.\n"), buf);
		if(fd != -10)
	          close(fd);
	        fault = YES;
		fd = -10;
	      }
	    }


#ifdef	USE_ZLIB

	    if(cks)
		crc32sum = crc32(crc32sum, buf2, i);

#endif

	    filelen -= 4096;
	  }

	  if(fd != -10)
	    close(fd);
	}
	else{			/* compressed */
	  if(!fault){
	    unlink(buf);

	    builtin_uncompress = NO;

	    if(buf2[0] == '.' && (! buf2[1] || buf2[1] == ' ')){
		if(buf2[1])
		  memmove(buf2, buf2 + 2, strlen(buf2) - 2 + 1);
		else
		  buf2[0] = '\0';

#ifndef	USE_ZLIB

		fprintf(errfp, T_("Error: Built-in (un)compression not available for file `%s'.\n"), buf);
		fault = YES;
		compressed = NO;
		buf2[0] = '\0';
		params->vars.errnum |= EUNPACKNOCOMPR;

#else

		builtin_uncompress = YES;

#endif
	    }

	    fd = open(buf, O_WRONLY | O_CREAT | O_BINARY, 0600);
	    if(fd < 0){
		fault = YES;
		fprintf(errfp, T_("Error: Cannot create file `%s'.\n"), buf);
		params->vars.errnum |= EUNPACKMKENT;
	    }
	    else{
	      if(buf2[0]){
		i = pipe(pp);

		if(i){
		  fault = YES;
		  params->vars.errnum |= EUNPACKUNCOMPR;
		  fprintf(errfp,
			T_("Error: Cannot create pipe for uncompressing `%s'.\n"),
				buf);
		  close(fd);
		  fd = -1;
	        }
		else{
		  pid = fork_forced();

		  if(pid < 0){
		    fault = YES;
		    fprintf(errfp, T_("Error: Cannot create file `%s'.\n"), buf);
		    params->vars.errnum |= EUNPACKMKENT;

		    close(fd);
		    fd = -1;
		  }
		  else if(! pid){		/* child */
		    char	**unzipargv;

		    clr_timer();

		    close(pp[1]);

		    if(cmd2argvq(&unzipargv, buf2)){
			close(pp[0]);
			exit(1);
		    }

		    dup2(pp[0], 0);
		    dup2(fd, 1);

		    execvp(unzipargv[0], unzipargv + 1);

		    exit(3);
		  }

		  close(pp[0]);
		  close(fd);

		  fd = pp[1];
		}
	      }		/* if there is an uncompress program */

#ifdef	USE_ZLIB

	      if(builtin_uncompress){
		i = open_file_unzip(&params->vars.zfile, fd);
		if(i){
		  fprintf(errfp, T_("Error: Cannot uncompress `%s'.\n"), buf);
		  params->vars.errnum |= EUNPACKUNCOMPR;
		  close(fd);
		  fd = -1;
		  fault = YES;
		}
	      }
	      else
		params->vars.zfile.fd = fd;

#endif

	    }		/* if file could be opened for write */
	  }		/* if there was no fault */

	  buf2 = get_mem(buf2, &buf2_nall, 4096, lbuf2, 256, &buf2_all);
	  if(!buf2)
	    CLEANUPR(-ENOMEM);

	  forever{
	    UChar	comprbuf[0x1000];

	    if(var_compr){
		fmterr = try_cont = NO;

		i = params->inputfunc(lbuf2, 2, params);
		if(i < 2){
		  fmterr = YES;
		}
		else{
		  xref_to_UnsN(&len, lbuf2, 16);
		  if(len > 0xfff || len < 0)
		    fmterr = try_cont = YES;
		}

		if(!fmterr){
		  i = params->inputfunc(comprbuf, len, params);
		  if(i < len){
		    params->vars.errnum |= EUNPACKREAD;
		    fmterr = YES;
		  }
		  else{
		    if(!fault){
	   	      if(write_file(fd, comprbuf, len, &params->vars.zfile,
				len < 0xfff ? YES : NO, writefunc) != len){
			fprintf(errfp, T_("Error: writing to `%s' failed.\n"), buf);
			params->vars.errnum |= EUNPACKWRITE;
			fault = YES;
		      }
		    }

#ifdef	USE_ZLIB

		    if(cks)
			crc32sum = crc32(crc32sum, comprbuf, len);

#endif

		  }
	        }
		else{
		  if(fd >= 0){
		    if(pid > 0){
			kill(pid, SIGTERM);
			waitpid_forced(pid, &pst, 0);
		    }
		    close(fd);
		  }

#ifdef	USE_ZLIB

		  if(builtin_uncompress)
		    reset_zfile(&params->vars.zfile);

#endif

		  if(try_cont){
		    i = formaterr(&type, params, started);
		    have_an_error = YES;
		    goto tryagain;
		  }
		  else
		    goto eoferr;
	 	}

		if(len < 0xfff)
		  break;

		continue;
	    }

	    if(filelen < 4096){
	      len = (Int32) filelen;

	      i = params->inputfunc(buf2, len, params);
	      if(i < len){
		if(fd >= 0){
		  if(pid > 0){
		    kill(pid, SIGTERM);
		    waitpid_forced(pid, &pst, 0);
		  }
		  close(fd);
		}
		goto eoferr;
	      }
	      if(!fault){
	        if(write_file(fd, buf2, len, &params->vars.zfile, YES, writefunc) != len){
		  params->vars.errnum |= EUNPACKWRITE;
		  fprintf(errfp, T_("Error: Writing to `%s' failed.\n"), buf);
	        }
	      }

#ifdef	USE_ZLIB

	      if(cks)
		crc32sum = crc32(crc32sum, buf2, len);

#endif

	      break;
	    }

	    i = params->inputfunc(buf2, 4096, params);
	    if(i < 4096){
	      if(fd >= 0){
		if(pid >= 0){
		  kill(pid, SIGTERM);
		  waitpid_forced(pid, &pst, 0);
		}
		close(fd);
	      }
	      goto eoferr;
	    }
	    if(!fault){
	      if(write_file(fd, buf2, 4096, &params->vars.zfile,
				len < 0xfff ? YES : NO, writefunc) != len){
	        fprintf(errfp, T_("Error: Writing to `%s' failed.\n"), buf);
		params->vars.errnum |= EUNPACKWRITE;
	        if(fd >= 0){
		  if(pid >= 0){
		    kill(pid, SIGTERM);
		    waitpid_forced(pid, &pst, 0);
		  }
		  close(fd);
		}
	        fd = -1;
		fault = YES;
	      }
	    }

#ifdef	USE_ZLIB

	    if(cks)
		crc32sum = crc32(crc32sum, buf2, len);

#endif

	    filelen -= 4096;
	  }

	  if(fd >= 0){
	    close(fd);

	    if(pid >= 0){
		waitpid_forced(pid, &pst, 0);
		if(WEXITSTATUS(pst)){
		  params->vars.errnum |= EUNPACKUNCOMPR;
		  fprintf(errfp, T_("Warning: Uncompress program returned bad status for file `%s'.\n"),
				buf);
		}
	    }
	  }

#ifdef	USE_ZLIB
	  if(builtin_uncompress){
	    reset_zfile(&params->vars.zfile);
	  }
#endif
	}

	if(!skip && !fault){
	  set_props(buf, uid, gid, mtime, mode, acls, params);

	  if(verbose && verbosefunc){
	    mk_esc_seq(buf, ESCAPE_CHARACTER, verbosestr);
	    strcat(verbosestr, "\n");
	    params->vars.uid = uid;
	    params->vars.mtime = mtime;
	    verbosefunc(verbosestr, params);
	  }
	}

	j = (cks ? 4 : 0);

	i = params->inputfunc(lbuf2, j + 1, params);
	if(lbuf2[j] != '.' || i < j + 1){
	  i = formaterr(&type, params, started);
	  have_an_error = YES;
	  goto tryagain;
	}

#ifdef	USE_ZLIB

	if(cks){
	  xref_to_Uns32(&lu, lbuf2);

	  if(lu != crc32sum){
	    fprintf(errfp, T_("Error: Wrong checksum unpacking file `%s'.\n"), buf);
	    params->vars.errnum |= EUNPACKCKSUM;
	  }
	}

#endif

	started = YES;
	have_an_error = NO;

	break;

      case FILECONTENTSCKS:
	cks = YES;

#ifdef	USE_ZLIB

	crc32sum = crc32(0L, NULL, 0);

#else

	fprintf(stderr, T_("Warning: CRC32 checksumming not available (requires zlib)\n"));

#endif

      case FILECONTENTS:
	read_time_t(mtime, time_t);

      case FILECONTENTS_O:
	read_size_t(len, size_t);

	buf = get_mem(buf, &buf_nall, len + 1, lbuf, 256, &buf_all);
	if(!buf)
	    CLEANUPR(-ENOMEM);

	i = params->inputfunc(buf, len, params);	/* read filename */
	if(i < len)
	  goto eoferr;
	buf[len] = '\0';
	convtorel(buf, params->relative);

	i = params->inputfunc(&c, 1, params);
	if(c != ';' || i < 1){
	  i = formaterr(&type, params, started);
	  have_an_error = YES;
	  goto tryagain;
	}

	read_size_t(len, size_t);		/* read uncompresscmd */

	buf2 = get_mem(buf2, &buf2_nall, len + 1, lbuf2, 256, &buf2_all);
	if(!buf2)
	  CLEANUPR(-ENOMEM);

	i = params->inputfunc(buf2, len, params);
	if(i < len)
	  goto eoferr;
	buf2[len] = '\0';

	i = params->inputfunc(&c, 1, params);
	if(c != ';' || i < 1){
	  i = formaterr(&type, params, started);
	  have_an_error = YES;
	  goto tryagain;
	}

	compressed = (buf2[0] ? YES : NO);
	builtin_uncompress = NO;

	i = stat(buf, &statb);
	uid = statb.st_uid;
	if(i){
	  fprintf(errfp, T_("Error: Cannot write to file `%s'.\n"), buf);
	  skip = YES;
	}

	if(! name_in_list(buf, files, n_files, params->recursive, files_found)
			|| invalid_time_or_uid(mtime, uid, params)
			|| (params->uid && not_allowed(buf, params)))
	  skip = YES;

	name_in_list(buf, files, n_files, 0, files_found);

	fd = -10;
	fault = NO;

	if(! skip){
	  fd = open(buf, O_WRONLY | O_BINARY);
	  if(fd < 0){
	    fd = -10;
	    fault = YES;
	    fprintf(errfp, T_("Error: Cannot write to file `%s'.\n"), buf);
	    params->vars.errnum |= EUNPACKWRITE;
  	  }

	  if(compressed && !fault){	/* have to uncompress */
	    builtin_uncompress = NO;

	    if(buf2[0] == '.' && (! buf2[1] || buf2[1] == ' ')){
		if(buf2[1])
		  memmove(buf2, buf2 + 2, strlen(buf2) - 2 + 1);
		else
		  buf2[0] = '\0';

#ifndef	USE_ZLIB

		fprintf(errfp, T_("Error: Built-in (un)compression not available for file `%s'.\n"), buf);
		fault = YES;
		compressed = NO;
		buf2[0] = '\0';
		params->vars.errnum |= EUNPACKNOCOMPR;

#else

		builtin_uncompress = YES;

#endif
	    }

	    if(buf2[0]){
	      i = pipe(pp);
	      if(i){
		fprintf(errfp,
			T_("Error: cannot uncompress contents of file `%s'.\n"),
			buf);
		params->vars.errnum |= EUNPACKUNCOMPR;
		skip = YES;
	      }
	      else{
		pid = fork_forced();

		if(pid < 0){
		  fault = YES;
		  fprintf(errfp,
			T_("Error: Cannot uncompress to file `%s'.\n"), buf);
		  params->vars.errnum |= EUNPACKUNCOMPR;
		}
		else if(! pid){		/* child */
		  char		**unzipargv;

		  clr_timer();

		  close(pp[1]);

		  if(cmd2argvq(&unzipargv, buf2)){
		    close(pp[0]);
		    exit(1);
		  }

		  dup2(pp[0], 0);
		  dup2(fd, 1);

		  execvp(unzipargv[0], unzipargv + 1);

		  exit(3);
		}

		close(pp[0]);
		close(fd);
		fd = pp[1];
	      }
	    }

#ifdef	USE_ZLIB

	    if(builtin_uncompress){
		i = open_file_unzip(&params->vars.zfile, fd);
		if(i){
		  params->vars.errnum |= EUNPACKUNCOMPR;
		  fprintf(errfp, T_("Error: Cannot uncompress `%s'.\n"), buf);
		  close(fd);
		  fd = -1;
		  fault = YES;
		}
	    }
	    else
		params->vars.zfile.fd = fd;

#endif

	  }

#ifdef	USE_ZLIB

	  else
	    params->vars.zfile.fd = fd;

#endif

	}

	if(skip)
	  fault = YES;

	do{
	  i = params->inputfunc(lbuf2, 1, params);
	  if(i < 1)
	    goto eoferr;

	  cptr = lbuf2 + 1;
	  i = params->inputfunc(cptr, (Int32) lbuf2[0], params);
	  if(i > 0 && ! fault){
	    j = write_file(fd, cptr, i, &params->vars.zfile,
					i < 0xff ? YES : NO, writefunc);
	    if(j > 0)
		num_written += j;

	    if(j < i){
		params->vars.errnum |= EUNPACKWRITE;
		fprintf(errfp, T_("Error: writing to `%s' failed.\n"), buf);
		fault = YES;
		close(fd);
		fd = -10;
		i = j;
	    }
	  }

	  if(i < (Int32) lbuf2[0]){
	    if(fd >= 0)
		close(fd);

#ifdef	USE_ZLIB

	    if(builtin_uncompress)
		reset_zfile(&params->vars.zfile);

#endif

	    goto eoferr;
	  }

#ifdef	USE_ZLIB

	  if(cks)
	    crc32sum = crc32(crc32sum, cptr, i);

#endif

	} while(lbuf2[0] == (UChar) 0xff);

	if(fd >= 0)
	  close(fd);

	if(pid >= 0){
	  waitpid_forced(pid, &pst, 0);
	  if(WEXITSTATUS(pst)){
	    params->vars.errnum |= EUNPACKUNCOMPR;
	    fprintf(errfp, T_("Error: uncompressing to `%s' failed.\n"), buf);
	    fault = YES;
	  }
	}

#ifdef	USE_ZLIB
	if(builtin_uncompress){
	  reset_zfile(&params->vars.zfile);
	}
#endif

	if(verbose && verbosefunc && !skip && !fault){
	  mk_esc_seq(buf, ESCAPE_CHARACTER, verbosestr);
	  strcat(verbosestr, "\n");
	  params->vars.uid = uid;
	  params->vars.mtime = mtime;
	  verbosefunc(verbosestr, params);
	}

	j = (cks ? 4 : 0);

	i = params->inputfunc(lbuf2, j + 1, params);
	if(lbuf2[j] != '.' || i < j + 1){
	  i = formaterr(&type, params, started);
	  have_an_error = YES;
	  goto tryagain;
	}

#ifdef	USE_ZLIB

	if(cks){
	  xref_to_Uns32(&lu, lbuf2);

	  if(lu != crc32sum){
	    fprintf(errfp, T_("Error: Wrong checksum unpacking file `%s'.\n"), buf);
	    params->vars.errnum |= EUNPACKCKSUM;
	  }
	}

#endif

	started = YES;
	have_an_error = NO;

	break;

      case COMMANDINOUTCKS:
	cks = YES;

#ifdef	USE_ZLIB

	crc32sum = crc32(0L, NULL, 0);

#else

	fprintf(stderr, T_("Warning: CRC32 checksumming not available (requires zlib)\n"));

#endif

      case COMMANDINOUT:
	read_int(i, int);

	buf2 = get_mem(buf2, &buf2_nall, i + 1, lbuf2, 256, &buf2_all);
	if(!buf2)
	  CLEANUPR(-ENOMEM);

	j = params->inputfunc(buf2, i, params);
	if(j < i){
	  i = formaterr(&type, params, started);
	  have_an_error = YES;
	  goto tryagain;
	}
	buf2[i] = '\0';

	i = params->inputfunc(&c, 1, params);
	if(i < 1 || c != ';'){
	  i = formaterr(&type, params, started);
	  have_an_error = YES;
	  goto tryagain;
	}

	cptr = NULL;
	fault = NO;

	mk_esc_seq(buf2, ESCAPE_CHARACTER, verbosestr);

	if(! name_in_list(buf2, files, n_files, params->recursive, files_found))
	  skip = YES;

	name_in_list(buf2, files, n_files, 0, files_found);
	if(skip)
	  fault = YES;

	if(params->uid && !fault){
	  fprintf(errfp, T_("Error: unpacking `%s' is only allowed for root.\n"),
				verbosestr);
	  fault = YES;
	}

	pid = pfd = -1;

	if(!fault){
	  UChar		*cptr2;

	  cptr = buf2 + strlen(CMDINOUTPREFIX);
	  cptr = strstr(cptr, CMDINOUTSEP) + strlen(CMDINOUTSEP);
	  if( (cptr2 = strstr(cptr, CMDINOUTCOMM)) ){
	    c = *cptr2;
	    *cptr2 = '\0';
	  }

	  pfd = fdpopen(cptr, O_WRONLY, &pid);
	  if(cptr2)
	    *cptr2 = c;

	  if(pfd < 0){
	    fault = YES;
	    params->vars.errnum |= EUNPACKWRITECMD;
	    fprintf(errfp, T_("Error: Cannot start program to unpack `%s'\n"),
				verbosestr);
	  }
	}

	do{
	  UChar		nbuf[8196];

	  i = params->inputfunc(nbuf, 2, params);
	  xref_to_UnsN(&n, nbuf, 16);
	  if(i < 2 || n > 8192){
	    if(pfd >= 0){
		close(pfd);
		if(pid > 0)
		  waitpid_forced(pid, &pst, 0);
		pid = pfd = -1;
	    }
	    i = formaterr(&type, params, started);
	    have_an_error = YES;
	    goto tryagain;
	  }

	  i = params->inputfunc(nbuf, n, params);
	  if(i < n){
	    if(pfd >= 0){
		close(pfd);
		if(pid > 0)
		  waitpid_forced(pid, &pst, 0);
		pid = pfd = -1;
	    }
	    params->vars.errnum |= EUNPACKREAD;
	    i = formaterr(&type, params, started);
	    have_an_error = YES;
	    goto tryagain;
	  }

	  if(!fault){
	    for(cptr = nbuf, j = n; j > 0; cptr += i, j -= i){
		i = writefunc(pfd, cptr, j);
		POSZ(i);
		if(i < 1){
		  fault = YES;
	 	  params->vars.errnum |= EUNPACKWRITE;
		  fprintf(errfp, T_("Error: Writing to unpack `%s` failed.\n"),
					verbosestr);
		  close(pfd);
		  if(pid > 0)
		    waitpid_forced(pid, &pst, 0);
		  pid = pfd = -1;
		}
	    }
	  }

#ifdef	USE_ZLIB

	  if(cks)
	    crc32sum = crc32(crc32sum, nbuf, i);

#endif

	} while(n == 8192);

	if(pfd >= 0){
	  close(pfd);
	  if(pid > 0){
	    waitpid_forced(pid, &pst, 0);
	    pst = WEXITSTATUS(pst);
	    if(pst){
		params->vars.errnum |= EUNPACKWRITECMD;
		fprintf(errfp, T_("Error: Command to unpack `%s' returned bad status %d.\n"),
				verbosestr, pst);
		fault = YES;
	    }
	  }

	  pfd = pid = -1;
	}

	if(verbose && verbosefunc && !skip && !fault){
	  strcat(verbosestr, "\n");
	  verbosefunc(verbosestr, params);
	}

	j = (cks ? 4 : 0);

	i = params->inputfunc(lbuf2, j + 1, params);
	if(lbuf2[j] != '.' || i < j + 1){
	  i = formaterr(&type, params, started);
	  have_an_error = YES;
	  goto tryagain;
	}

#ifdef	USE_ZLIB

	if(cks){
	  xref_to_Uns32(&lu, lbuf2);

	  if(lu != crc32sum){
	    fprintf(errfp, T_("Error: Wrong checksum unpacking file `%s'.\n"), verbosestr);
	    params->vars.errnum |= EUNPACKCKSUM;
	  }
	}

#endif

	started = YES;
	have_an_error = NO;

	break;

      default:
	i = formaterr(&type, params, started);
	goto tryagain;
    }
  }

 cleanup:

  if(buf_all)
    free(buf);
  if(buf2_all)
    free(buf2);

  if(files){
    for(fileidx = files; *fileidx; fileidx++)
	free(*fileidx);

    free(files);
  }

  ZFREE_ACL_STRUCT(acls, params);

  return(r);

 eoferr:
  r = EOF;

  fprintf(errfp, T_("Error: unexpected end of file.\n"));
  params->vars.errnum |= EUNPACKREAD;

  goto cleanup;
}

static void
print_if_false(
  UChar		*name,
  UChar		*type_n,
  Flag		*false,
  AarParams	*params)
{
  UChar	lv[256], *v;
  Flag	all = NO;

  if(! *false){
    v = get_mem(NULL, NULL, strlen(name) + strlen(type_n) + 2,
			lv, 256, &all);
    if(!v){
	fprintf(params->errfp, T_("<Error: No memory>\n"));
	return;
    }

    sprintf(v, "%s%s\n", name, type_n);
    params->verbosefunc(v, params);
    *false = 1;

    if(all)
	free(v);
  }
}

static void
print_if_unequal_u(
  Uns32		is,
  Uns32		should,
  UChar		*paramname,
  UChar		*name,
  Flag		oct,
  UChar		*tname,
  Flag		*false,
  AarParams	*params)
{
  UChar	lv[256], *v, *t;
  Flag	all = NO;

  if(is == should)
    return;

  t = (oct ? T_("%s%s is 0%lo, should be 0%lo.\n")
		: T_("%s%s is %lu, should be %lu.\n"));

  v = get_mem(NULL, NULL, strlen(paramname) + 32
		+ strlen(t) + strlen(DP), lv, 256, &all);
  if(!v){
    fprintf(params->errfp, T_("<Error: No memory>\n"));
    return;
  }

  print_if_false(name, tname, false, params);
  sprintf(v, t, DP, paramname,
		(unsigned long) is, (unsigned long) should);
  params->verbosefunc(v, params);

  if(all)
    free(v);
}

static void
print_if_unequal_i(
  Int32		is,
  Int32		should,
  UChar		*paramname,
  UChar		*name,
  UChar		*tname,
  Flag		*false,
  AarParams	*params)
{
  UChar	lv[256], *v, *t;
  Flag	all = NO;

  if(is == should)
    return;

  t = T_("%s%s is %ld, should be %ld.\n");

  v = get_mem(NULL, NULL, strlen(paramname) + 32
		+ strlen(t) + strlen(DP), lv, 256, &all);
  if(!v){
    fprintf(params->errfp, T_("<Error: No memory>\n"));
    return;
  }

  print_if_false(name, tname, false, params);
  sprintf(v, t, DP, paramname,
		(long int) is, (long int) should);
  params->verbosefunc(v, params);

  if(all)
    free(v);
}

static void
print_if_unequal_t(
  time_t	is,
  time_t	should,
  UChar		*paramname,
  UChar		*name,
  UChar		*tname,
  Flag		*false,
  AarParams	*params)
{
  UChar	lv[256], *v, *t, timestr1[50], timestr2[50];
  Flag	all = NO;

  if(is == should)
    return;

  t = T_("%s%s is %s, should be %s.\n");

  v = get_mem(NULL, NULL, strlen(paramname) + 128
		+ strlen(t) + strlen(DP), lv, 256, &all);
  if(!v){
    fprintf(params->errfp, T_("<Error: No memory>\n"));
    return;
  }

  print_if_false(name, tname, false, params);
  sprintf(v, t, DP, paramname,
			(char *) time_t_to_intstr(is, timestr1),
			(char *) time_t_to_intstr(should, timestr2));
  params->verbosefunc(v, params);

  if(all)
    free(v);
}

static void
print_if_unequal_s(
  UChar		*is,
  UChar		*should,
  UChar		*paramname,
  UChar		*name,
  UChar		*tname,
  Flag		*false,
  AarParams	*params)
{
  UChar	lv[256], *v, *t;
  Flag	all = NO;

  if(!strcmp(is, should))
    return;

  t = T_("%s%s is %s, should be %s.\n");

  v = get_mem(NULL, NULL, strlen(paramname) + strlen(t)
			+ strlen(is) + strlen(should) + strlen(DP),
		lv, 256, &all);
  if(!v){
    fprintf(params->errfp, T_("<Error: No memory>\n"));
    return;
  }

  print_if_false(name, tname, false, params);
  sprintf(v, t, DP, paramname,
		(char *) is, (char *) should);
  params->verbosefunc(v, params);

  if(all)
    free(v);
}

#if	BUFFERSIZ < 4096
#define	INBUFSIZ	4096
#else
#define INBUFSIZ	BUFFERSIZ
#endif

#define	bytes_from_stream	filesize

static Int32
pack_verify_l(UChar ** filelist, AarParams * params, UChar * files_found)
{
  UChar		lbuf[256], lbuf2[256], lbuf3[256];
  UChar		verbosestr[MAXPATHLEN * 4 + 100];
  UChar		buf4[BUFFERSIZ], esc_name[BUFFERSIZ], *readptr;
  UChar		*buf = NULL, *buf2 = NULL, *buf3 = NULL;
  Int32		buf_nall = 0, buf2_nall = 0, buf3_nall = 0;
  Flag		buf_all = NO, buf2_all = NO, buf3_all = NO;
  Flag		started, have_an_error, compressed, builtin_uncompress;
  UChar		*typename, *typetag, vbuf[BUFFERSIZ], c;
  UChar		*wptr, *cmpptr;	/* uninitialized OK */
  Flag		fault, subprfault, bi_uncompr_fault, uncompress_program, skip;
  Flag		pipe_exh, file_exh, bustream_exh, up_exh, biu_end, biu_exh;
  Uns8		verbose;
  UChar		**files, *cptr, *cptr2, **fileidx;
  Int32		li, type, type_attrs, i, j, k, n, gid, uid, fd, r, rvr, ifd;
  Int32		to_read, to_compare, written_bytes, left_to_write, n_files;
  off_t		compared_bytes, read_bytes, filesize, flen, filelen, ott;
  UChar		sizestr1[30], sizestr2[30];
  Flag		var_compr, var_compr_avail, name_printed;
  Int32		wb, rb, avail_out_before;
  Uns32		mode, lu;
  time_t	mtime = 0, ltt;
  size_t	len, stt;
  FILE		*errfp;
  dev_t		rdev;
  int		pfd, pid, pst, inpp[2], outpp[2], fl;
  int		tofd, fromfd;	/* uninitialized OK */
  void		(*verbosefunc)(UChar *, struct aar_params *);
  Int64		(*writefunc)(int, UChar *, Int64);
  Int64		(*readfunc)(int, UChar *, Int64);
  struct pollfd	fds[2];
  struct stat	statb, statb2, statb3;
  Flag		cks, noverify;
  Uns32		crc32sum;	/* uninitialized OK */

  verbose = params->verbose;
  verbosefunc = params->verbosefunc;
  writefunc = params->writefunc;
  readfunc = params->readfunc;

  files = filelist;
  i = r = n_files = 0;

  ifd = params->infd;

  if(files){
    while(*files){
      cleanpath(*files);
      repl_esc_seq(*files, ESCAPE_CHARACTER);

      files++, i++;
    }

    files = NEWP(UChar *, i + 1);
    if(!files)
      return(-ENOMEM);
    memset(files, 0, (i + 1) * sizeof(UChar *));

    n_files = i;
    while(i > 0){
      i--;
      if(params->relative && FN_ISABSPATH(filelist[i])){
	cptr = filelist[i];
	cptr = FN_FIRSTDIRSEP(cptr);
	while(FN_ISDIRSEP(*cptr))
	  cptr++;
	files[i] = strdup(cptr);
      }
      else
	files[i] = strdup(filelist[i]);

      if(!files[i]){
	for(i++; i < n_files; i++)
	  free(files[i]);
	free(files);
	return(-ENOMEM);
      }
    }

    q_sort(files, n_files, sizeof(UChar *), cmp_UCharPTR);
  }

  errfp = params->errfp;

  started = have_an_error = NO;

  forever{
    flen = 0;
    pid = pfd = -1;

    if(filelist && !params->recursive){
	for(fileidx = filelist, cptr = files_found;
					*fileidx; fileidx++, cptr++){
	  if(! (*cptr))
	    break;
	}

	if(! (*fileidx))
	  goto cleanup;
    }

    if(params->pre_verbosefunc)
	(*params->pre_verbosefunc)(NULL, params);

    i = get_int(params, &li);
    type = (Int32) li;

   tryagain:

    if(i < 1){
	if(i < 0){
	  r = EOF;
	  goto cleanup;
	}

	i = formaterr(&type, params, started);
	have_an_error = YES;
	goto tryagain;
    }

    type_attrs = (ABS(type) & PACK_ATTRMASK);
    type = (ABS(type) & PACK_TYPEMASK) * SIGN(type); 

    skip = NO;
    name_printed = NO;
    cks = NO;
    noverify = (type_attrs & PACK_NOVERIFY ? YES : NO);

    switch(type){
      case ENDOFARCHIVE:
#if 0
	if(filelist){
	  for(fileidx = filelist, cptr = files_found;
					*fileidx; fileidx++, cptr++){
	    if(! (*cptr)){
		fprintf(params->errfp,
			DP "%s not found in archive.\n", *fileidx);
	    }
	  }
	}
#endif
	if(have_an_error){
	  i = formaterr(&type, params, started);
	  goto tryagain;
	}

	goto cleanup;

      case INFORMATION:
	read_size_t(len, size_t);

	buf = get_mem(buf, &buf_nall, len + 1, lbuf, 256, &buf_all);
	if(!buf)
	  CLEANUPR(-ENOMEM);

	i = params->inputfunc(buf, len, params);
	if(i < len){
	  goto eoferr;
	}
	i = params->inputfunc(&c, 1, params);
	if(c != '.' || i < 1){
	  i = formaterr(&type, params, started);
	  have_an_error = YES;
	  goto tryagain;
	}
	buf[len] = '\0';
	if(verbose && verbosefunc)
	  verbosefunc(buf, params);

	started = YES;
	have_an_error = NO;

	break;
	
      case DIRECTORY:
	read_uns(mode, Uns32);
	read_time_t(mtime, time_t);
	read_int(uid, Int32);
	read_int(gid, Int32);
	read_size_t(len, size_t);

	buf = get_mem(buf, &buf_nall, len + 1, lbuf, 256, &buf_all);
	if(!buf)
	  CLEANUPR(-ENOMEM);

	i = params->inputfunc(buf, len, params);
	if(i < len)
	  goto eoferr;
	buf[len] = '\0';
	convtorel(buf, params->relative);
	mk_esc_seq(buf, ESCAPE_CHARACTER, esc_name);

	i = params->inputfunc(&c, 1, params);
	if(c != '.' || i < 1){
	  i = formaterr(&type, params, started);
	  have_an_error = YES;
	  goto tryagain;
	}

	started = YES;
	have_an_error = NO;

	if(! name_in_list(buf, files, n_files, params->recursive, files_found)
			|| invalid_time_or_uid(mtime, uid, params)
			|| (params->uid && not_allowed(buf, params))
			|| noverify)
	  skip = YES;

	name_in_list(buf, files, n_files, 0, files_found);

	if(!skip){
	  typename = "directory";
	  typetag = "/";

	  if(verbose){
	    print_if_false(esc_name, typetag, &name_printed, params);
	  }
	  if(stat(buf, &statb)){
	    print_if_false(esc_name, typetag, &name_printed, params);
	    sprintf(verbosestr, T_("Cannot stat `%s'.\n"), esc_name);
	    verbosefunc(verbosestr, params);
	  }
	  else{
	    if(!IS_DIRECTORY(statb)){
		print_if_false(esc_name, typetag, &name_printed, params);
		sprintf(verbosestr, DP "Is not a %s.\n", typename);
		verbosefunc(verbosestr, params);
	    }
	    else{
		print_if_unequal_i(statb.st_uid, uid, T_("User-ID"), esc_name,
			typetag, &name_printed, params);
		print_if_unequal_i(statb.st_gid, gid, T_("Group-ID"), esc_name,
			typetag, &name_printed, params);
		print_if_unequal_u(statb.st_mode, mode, T_("Mode"), esc_name,
			YES, typetag, &name_printed, params);
		print_if_unequal_t(statb.st_mtime, mtime,
			T_("Modification-Time"), esc_name,
			typetag, &name_printed, params);
	    }
	  }
	}

	break;

#ifndef _WIN32
      case FIFO:
      case SOCKET:
	read_uns(mode, Uns32);
	read_time_t(mtime, time_t);
	read_int(uid, Int32);
	read_int(gid, Int32);
	read_size_t(len, size_t);

	buf = get_mem(buf, &buf_nall, len + 1, lbuf, 256, &buf_all);
	if(!buf)
	  CLEANUPR(-ENOMEM);

	i = params->inputfunc(buf, len, params);
	if(i < len)
	  goto eoferr;
	buf[len] = '\0';
	convtorel(buf, params->relative);
	mk_esc_seq(buf, ESCAPE_CHARACTER, esc_name);

	i = params->inputfunc(&c, 1, params);
	if(c != '.' || i < 1){
	  i = formaterr(&type, params, started);
	  have_an_error = YES;
	  goto tryagain;
	}

	started = YES;
	have_an_error = NO;

	if(! name_in_list(buf, files, n_files, params->recursive, files_found)
			|| invalid_time_or_uid(mtime, uid, params)
			|| (params->uid && not_allowed(buf, params))
			|| noverify)
	  skip = YES;

	name_in_list(buf, files, n_files, 0, files_found);

	if(! skip){
	  typename = (type == FIFO ? "Named Pipe" : "Socket");
	  typetag = (type == FIFO ? "|" : "[");

	  if(verbose){
	    print_if_false(esc_name, typetag, &name_printed, params);
	  }
	  if(stat(buf, &statb)){
	    print_if_false(esc_name, typetag, &name_printed, params);
	    sprintf(verbosestr, T_("Cannot stat `%s'.\n"), esc_name);
	    verbosefunc(verbosestr, params);
	  }
	  else{
	    if(!(type == FIFO ? IS_FIFO(statb) : IS_SOCKET(statb))){
		print_if_false(esc_name, typetag, &name_printed, params);
		sprintf(verbosestr, DP "Is not a %s.\n", typename);
		verbosefunc(verbosestr, params);
	    }
	    else{
		print_if_unequal_i(statb.st_uid, uid, T_("User-ID"), esc_name,
			typetag, &name_printed, params);
		print_if_unequal_i(statb.st_gid, gid, T_("Group-ID"), esc_name,
			typetag, &name_printed, params);
		print_if_unequal_u(statb.st_mode, mode, T_("Mode"), esc_name,
			YES, typetag, &name_printed, params);
		print_if_unequal_t(statb.st_mtime, mtime,
			T_("Modification-Time"), esc_name,
			typetag, &name_printed, params);
	    }
	  }
	}

	break;

      case BLOCKDEVICE:
      case CHARDEVICE:
	read_uns(mode, Uns32);
	read_time_t(mtime, time_t);
	read_int(uid, Int32);
	read_int(gid, Int32);
	read_uns(rdev, dev_t);
	read_size_t(len, size_t);

	buf = get_mem(buf, &buf_nall, len + 1, lbuf, 256, &buf_all);
	if(!buf)
	  CLEANUPR(-ENOMEM);

	i = params->inputfunc(buf, len, params);
	if(i < len)
	  goto eoferr;
	buf[len] = '\0';
	convtorel(buf, params->relative);
	mk_esc_seq(buf, ESCAPE_CHARACTER, esc_name);

	i = params->inputfunc(&c, 1, params);
	if(c != '.' || i < 1){
	  i = formaterr(&type, params, started);
	  have_an_error = YES;
	  goto tryagain;
	}

	started = YES;
	have_an_error = NO;

	if(! name_in_list(buf, files, n_files, params->recursive, files_found)
			|| invalid_time_or_uid(mtime, uid, params)
			|| (params->uid && not_allowed(buf, params))
			|| noverify)
	  skip = YES;

	name_in_list(buf, files, n_files, 0, files_found);

	if(! skip){
	  typename = (type == CHARDEVICE ? "Character Device" : "Block Device");
	  typetag = (type == CHARDEVICE ? "*" : "#");

	  if(verbose){
	    print_if_false(esc_name, typetag, &name_printed, params);
	  }
	  if(stat(buf, &statb)){
	    print_if_false(esc_name, typetag, &name_printed, params);
	    sprintf(verbosestr, T_("Cannot stat `%s'.\n"), esc_name);
	    verbosefunc(verbosestr, params);
	  }
	  else{
	    if(!(type == CHARDEVICE ? IS_CHARDEV(statb)
					: IS_BLOCKDEV(statb))){
		print_if_false(esc_name, typetag, &name_printed, params);
		sprintf(verbosestr, DP "Is not a %s.\n", typename);
		verbosefunc(verbosestr, params);
	    }
	    else{
		print_if_unequal_i(statb.st_uid, uid, T_("User-ID"), esc_name,
			typetag, &name_printed, params);
		print_if_unequal_i(statb.st_gid, gid, T_("Group-ID"), esc_name,
			typetag, &name_printed, params);
		print_if_unequal_u(statb.st_mode, mode, T_("Mode"), esc_name,
			YES, typetag, &name_printed, params);
		print_if_unequal_t(statb.st_mtime, mtime,
			T_("Modification-Time"), esc_name,
			typetag, &name_printed, params);
		print_if_unequal_u(statb.st_rdev, rdev,
			T_("Major number"), esc_name,
			NO, typetag, &name_printed, params);
	    }
	  }
	}

	break;
#endif

      case SYMLINK:
      case HARDLINK:
	read_uns(mode, Uns32);
	read_time_t(mtime, time_t);
	read_int(uid, Int32);
	read_int(gid, Int32);
	read_size_t(len, size_t);

	buf = get_mem(buf, &buf_nall, len + 1, lbuf, 256, &buf_all);
	if(!buf)
	  CLEANUPR(-ENOMEM);

	i = params->inputfunc(buf, len, params);
	if(i < 1)
	  goto eoferr;
	buf[len] = '\0';

	i = params->inputfunc(&c, 1, params);
	if(c != ';' || i < 1){
	  i = formaterr(&type, params, started);
	  have_an_error = YES;
	  goto tryagain;
	}
	if(params->relative >= 3)
	  convtorel(buf, params->relative);

	read_size_t(len, size_t);

	buf2 = get_mem(buf2, &buf2_nall, len + 1, lbuf2, 256, &buf2_all);
	if(!buf2)
	  CLEANUPR(-ENOMEM);

	i = params->inputfunc(buf2, len, params);
	if(i < len)
	  goto eoferr;
	buf2[len] = '\0';
	if(type == SYMLINK){
	  if(params->relative > 1){
	    cptr = convtorelcwd(buf2, buf,
			(buf2 == lbuf2 ? 256 : buf2_nall), params->relative);
	    if(!cptr)
		CLEANUPR(-ENOMEM);
	    if(cptr != buf2){
		if(buf2 != lbuf2)
		  free(buf2);
		buf2 = cptr;
		buf2_nall = strlen(buf2) + 1;
		buf2_all = YES;
	    }
	  }
	}
	else{
	  convtorel(buf2, params->relative);
	}
	if(params->relative < 3)
	  convtorel(buf, params->relative);

	mk_esc_seq(buf, ESCAPE_CHARACTER, esc_name);

	i = params->inputfunc(&c, 1, params);
	if(c != '.' || i < 1){
	  i = formaterr(&type, params, started);
	  have_an_error = YES;
	  goto tryagain;
	}

	started = YES;
	have_an_error = NO;

	if(! name_in_list(buf, files, n_files, params->recursive, files_found)
			|| invalid_time_or_uid(mtime, uid, params)
			|| (params->uid && not_allowed(buf, params))
			|| noverify)
	  skip = YES;

	name_in_list(buf, files, n_files, 0, files_found);

	if(! skip){
	  typename = (type == SYMLINK ? "Symlink" : "Hardlink");
	  typetag = (type == SYMLINK ? "@" : "=");

	  if(verbose){
	    print_if_false(esc_name, typetag, &name_printed, params);
	  }
	  if(lstat(buf, &statb)){
	    print_if_false(esc_name, typetag, &name_printed, params);
	    sprintf(verbosestr, T_("Cannot stat `%s'.\n"), esc_name);
	    verbosefunc(verbosestr, params);
	  }
	  else{
	    if(!(type == SYMLINK ? IS_SYMLINK(statb) : IS_HARDLINK(statb))){
		print_if_false(esc_name, typetag, &name_printed, params);
		sprintf(verbosestr, DP "Is not a %s.\n", typename);
		verbosefunc(verbosestr, params);
	    }
	    else{
		if(type == SYMLINK){
		  for(n = i = 192; i >= n; n += 64){
		    buf3 = get_mem(buf3, &buf3_nall, n + 1, lbuf3, 256, &buf3_all);
		    if(!buf3)
			CLEANUPR(-errno);

		    i = readlink(buf, buf3, n);
		    if(i < 0){
			fprintf(errfp, T_("Error: cannot read symlink `%s'.\n"), buf);
			CLEANUPR(-errno);
		    }
		  }
		  buf3[i] = '\0';

		  if(!FN_ISABSPATH(buf3) && FN_ISPATH(buf)){
		    strcpy(vbuf, buf);
		    strcpy(FN_LASTDIRDELIM(vbuf) + 1, buf3);

		    buf3 = get_mem(buf3, &buf3_nall, strlen(vbuf),
					lbuf3, 256, &buf3_all);
		    if(!buf3)
			CLEANUPR(-ENOMEM);

		    strcpy(buf3, vbuf);
		  }
		  if(!FN_ISABSPATH(buf2) && FN_ISPATH(buf)){
		    strcpy(vbuf, buf);
		    strcpy(FN_LASTDIRDELIM(vbuf) + 1, buf2);

		    buf2 = get_mem(buf2, &buf2_nall, strlen(vbuf),
					lbuf2, 256, &buf2_all);
		    if(!buf2)
			CLEANUPR(-ENOMEM);

		    strcpy(buf2, vbuf);
		  }
		}
		else{
		  buf3 = get_mem(buf3, &buf3_nall, strlen(buf2),
					lbuf3, 256, &buf3_all);
		  if(!buf3)
		    CLEANUPR(-ENOMEM);

		  strcpy(buf3, buf2);
		}

		i = stat(buf3, &statb3);
		j = stat(buf2, &statb2);
		if(!i && !j){
		  if(statb2.st_ino != statb3.st_ino
			|| statb2.st_rdev != statb3.st_rdev){
		    print_if_unequal_s(buf3, buf2, T_("File pointed to"),
			esc_name, typetag, &name_printed, params);
		  }
		}
		if(i || j){
		  cleanpath(buf2);
		  cleanpath(buf3);
		  print_if_unequal_s(buf3, buf2, T_("File pointed to"),
			esc_name, typetag, &name_printed, params);
		}

#ifdef	HAVE_LCHOWN
		print_if_unequal_i(statb.st_uid, uid, T_("User-ID"), esc_name,
			typetag, &name_printed, params);
		print_if_unequal_i(statb.st_gid, gid, T_("Group-ID"), esc_name,
			typetag, &name_printed, params);
#endif
		if(type != SYMLINK){
		  print_if_unequal_u(statb.st_mode, mode, T_("Mode"), esc_name,
			YES, typetag, &name_printed, params);
		  print_if_unequal_t(statb.st_mtime, mtime,
			T_("Modification-Time"), esc_name,
			typetag, &name_printed, params);
		}
	    }
	  }
	}

	break;

      case REGFILECKS:
	cks = YES;

#ifdef	USE_ZLIB

	crc32sum = crc32(0L, NULL, 0);

#else

	fprintf(stderr, T_("Warning: CRC32 checksumming not available (requires zlib)\n"));

#endif

      case REGFILE:
	read_uns(mode, Uns32);
	read_time_t(mtime, time_t);
	read_int(uid, Int32);
	read_int(gid, Int32);
	read_size_t(len, size_t);

	buf = get_mem(buf, &buf_nall, len + 1, lbuf, 256, &buf_all);
	if(!buf)
	  CLEANUPR(-ENOMEM);

	i = params->inputfunc(buf, len, params);	/* read filename */
	if(i < len)
	  goto eoferr;
	buf[len] = '\0';
	convtorel(buf, params->relative);
	mk_esc_seq(buf, ESCAPE_CHARACTER, esc_name);

	i = params->inputfunc(&c, 1, params);
	if(c != ';' || i < 1){
	  i = formaterr(&type, params, started);
	  have_an_error = YES;
	  goto tryagain;
	}

	read_size_t(len, size_t);		/* read uncompresscmd */

	buf2 = get_mem(buf2, &buf2_nall, MAX(4096, len + 1),
				lbuf2, 256, &buf2_all);
	if(!buf2)
	  CLEANUPR(-ENOMEM);

	i = params->inputfunc(buf2, len, params);
	if(i < len)
	  goto eoferr;
	buf2[len] = '\0';

	i = params->inputfunc(&c, 1, params);
	if(c != ';' || i < 1){
	  i = formaterr(&type, params, started);
	  have_an_error = YES;
	  goto tryagain;
	}

	read_off_t(filelen, off_t);			/* read filesize */

	compressed = (buf2[0] ? YES : NO);
	var_compr = var_compr_avail = (compressed && filelen == 0) ? YES : NO;

	if(! name_in_list(buf, files, n_files, params->recursive, files_found)
			|| invalid_time_or_uid(mtime, uid, params)
			|| (params->uid && not_allowed(buf, params))
			|| noverify)
	  skip = YES;

	name_in_list(buf, files, n_files, 0, files_found);

	typename = "Regular File";
	typetag = "";

	if(!skip){
	  if(verbose){
	    print_if_false(esc_name, typetag, &name_printed, params);
	  }
	  if(stat(buf, &statb)){
	    print_if_false(esc_name, typetag, &name_printed, params);
	    sprintf(verbosestr, DP "Cannot stat `%s'\n", esc_name);
	    verbosefunc(verbosestr, params);
	    skip = YES;
	  }
	  else{
	    if(!IS_REGFILE(statb)){
		print_if_false(esc_name, typetag, &name_printed, params);
		sprintf(verbosestr, DP "Is not a %s.\n", typename);
		verbosefunc(verbosestr, params);
		skip = YES;
	    }
	    else{
		flen = statb.st_size;
		print_if_unequal_i(statb.st_uid, uid, T_("User-ID"), esc_name,
			typetag, &name_printed, params);
		print_if_unequal_i(statb.st_gid, gid, T_("Group-ID"), esc_name,
			typetag, &name_printed, params);
		print_if_unequal_u(statb.st_mode, mode, T_("Mode"), esc_name,
			YES, typetag, &name_printed, params);
		print_if_unequal_t(statb.st_mtime, mtime,
			T_("Modification-Time"), esc_name,
			typetag, &name_printed, params);
	    }
	  }
	}

	fault = subprfault = NO;
	fd = -1;

	if(skip)
	  fault = YES;

	if(!fault){
	  fd = open(buf, O_RDONLY | O_BINARY);
	  if(fd < 0){
	    fault = YES;

	    sprintf(verbosestr, DP "Cannot open `%s'\n", esc_name);
	    verbosefunc(verbosestr, params);
	  }
	}

	builtin_uncompress = NO;
	if(buf2[0] == '.' && (! buf2[1] || buf2[1] == ' ')){
	  if(buf2[1])
	    memmove(buf2, buf2 + 2, strlen(buf2) - 2 + 1);
	  else
	    buf2[0] = '\0';

#ifndef	USE_ZLIB

		fprintf(errfp, T_("Error: Built-in (un)compression not available for file `%s'.\n"), esc_name);
		fault = YES;
		uncompress_program = NO;
		compressed = NO;
		buf2[0] = '\0';
		params->vars.errnum |= EUNPACKNOCOMPR;

#else

	  builtin_uncompress = YES;

#endif

	}
	uncompress_program = (buf2[0] ? YES : NO);

	buf3 = get_mem(buf3, &buf3_nall, 4096,
				lbuf3, 256, &buf3_all);
	if(!buf3)
	  CLEANUPR(-ENOMEM);

	bustream_exh = file_exh = pipe_exh = NO;
	tofd = fromfd = -1;

	if(compressed && uncompress_program){
	  i = pipe(inpp);
	  j = pipe(outpp);

	  tofd = inpp[1];
	  fromfd = outpp[0];

	  if(i || j){
	    subprfault = YES;
	    params->vars.errnum |= EUNPACKUNCOMPR;
	    fprintf(errfp,
		"Error: Cannot create pipe for uncompression of `%s'.\n",
				esc_name);
	    if(!i){
	      close(inpp[0]);
	      close(tofd);
	    }
	  }

	  if(!subprfault){
	    pid = fork_forced();
	    if(pid < 0){
	      fprintf(errfp, T_("Error: Cannot start uncompression of `%s'.\n"), esc_name);
	      params->vars.errnum |= EUNPACKUNCOMPR;
	      subprfault = YES;
	    }
	    if(!pid){		/* child */
	      char	**unzipargv;

	      close(tofd);
	      close(fromfd);

	      if(cmd2argvq(&unzipargv, buf2)){
		close(inpp[0]);
		close(outpp[1]);
		exit(1);
	      }

	      dup2(inpp[0], 0);
	      dup2(outpp[1], 1);

	      execvp(unzipargv[0], unzipargv + 1);
	      exit(3);
	    }
	  }
	  close(inpp[0]);
	  close(outpp[1]);

	  if(subprfault){
	    while(var_compr || filelen > 0){
		if(var_compr){
		  i = params->inputfunc(lbuf3, 2, params);
		  if(i < 2)
		    break;
		  xref_to_UnsN(&n, lbuf3, 16);
		  if(n < 0 || n > 0xfff)
		    break;
		}
		else{
		  n = (filelen > 4096 ? 4096 : filelen);
		}
		if(n > 0){
		  i = params->inputfunc(buf3, n, params);
		  if(i < 0)
		    break;
		}
		else{
		  i = 0;
		}
		if(var_compr){
		  if(n != 0xfff)
		    var_compr = NO;
		}
		else{
		  filelen -= (i > 0 ? i : 0);
		}
	    }
	    bustream_exh = YES;
	    filelen = 0;
	  }

	  fl = fcntl(tofd, F_GETFL);
	  fcntl(tofd, F_SETFL, fl | NONBLOCKING_FLAGS);
	}

#ifdef	USE_ZLIB

	if(builtin_uncompress){
	  reset_zfile(&params->vars.zfile);
	  if(inflateInit(&(params->vars.zfile.z_stream)) != Z_OK)
	    bi_uncompr_fault = YES;
	  params->vars.zfile.z_stream.next_out = buf3;
	  params->vars.zfile.z_stream.avail_out = 4096;
	  params->vars.zfile.z_stream.next_in = buf2;
	  params->vars.zfile.z_stream.avail_in = 0;
	}

#endif

	read_bytes = compared_bytes = written_bytes = left_to_write = 0;
	up_exh = biu_exh = biu_end = NO;

	while(!pipe_exh){
	  forever{
	    if(uncompress_program && !subprfault){
	      rb = 0;
	      if(!up_exh){
		fds->fd = fromfd;
		fds->events = POLLIN;
		i = poll(fds, 1, tofd < 0 ? -1 : 0);
	 	if(i > 0 && POLLINEVENT(fds->revents)){
		  rb = read(fromfd, buf4, BUFFERSIZ);
		  POSZ(rb);
		  if(!rb && tofd < 0)
		    up_exh = YES;
		}
	      }

	      if(up_exh || rb > 0){
		to_compare = rb;
		cmpptr = buf4;

		pipe_exh = up_exh;

		if(to_compare > 0 || up_exh)
		  break;
	      }

	      if((builtin_uncompress ? biu_exh : bustream_exh)
					&& !left_to_write && tofd >= 0){
		close(tofd);
		tofd = -1;
	      }

	      if(left_to_write && tofd >= 0){
/*
		fds->fd = tofd;
		fds->events = POLLOUT;
		i = poll(fds, 1, 0);
		wb = 0;
		if(i > 0 && POLLOUTEVENT(fds->revents)){
*/
		  wb = writefunc(tofd, wptr, left_to_write);
		  POSZ(wb);
		  wptr += wb;
		  left_to_write -= wb;
/*
		}
*/
		if(!wb){
		  if(fromfd >= 0){
		    fds[0].fd = fromfd;
		    fds[0].events = POLLIN;
		  }
		  else{
		    fds[0].fd = -1;
		    fds[0].events = 0;
		  }
		  if(tofd >= 0){
		    fds[1].fd = tofd;
		    fds[1].events = POLLOUT;
		  }
		  else{
		    fds[1].fd = -1;
		    fds[1].events = 0;
		  }

		  i = poll(fds, 2, 100);	/* wait max 1/10 s */
		  if(i > 0){		/* this is suspect: no write or read */
		    i = waitpid(pid, &pst, WNOHANG);	/* was possible, but */
		    if(i == pid){		/* select indicated possible */
			close(tofd);		/* read or write. This might */
			close(fromfd);		/* mean: The subprocess died */
			tofd = fromfd = -1;
			subprfault = YES;
			params->vars.errnum |= EUNPACKUNCOMPR;
			fprintf(errfp,
				T_("Error: Unprocess program unexpectedly died while processing `%s'.\n"),
					esc_name);
		    }
		  }
		}

		continue;
	      }
	    }

#ifdef	USE_ZLIB

	    if(builtin_uncompress && !subprfault){
	      to_compare = params->vars.zfile.z_stream.next_out - buf3;
	      if(to_compare > 0)
		cmpptr = buf3;
	      else if(biu_end)
		biu_exh = YES;

	      if(biu_end || to_compare > 0){
		params->vars.zfile.z_stream.next_out = buf3;
		params->vars.zfile.z_stream.avail_out = 4096;

		if(uncompress_program){
		  left_to_write = to_compare;
		  wptr = buf3;
		  continue;
		}
		else{
		  break;
		}
	      }

	      if(!biu_end){
		avail_out_before = params->vars.zfile.z_stream.avail_out;

		i = inflate(&(params->vars.zfile.z_stream), /* bustream_exh */ Z_NO_FLUSH);
		if(i == Z_STREAM_END){
		  biu_end = YES;

		  if(!uncompress_program)
		    pipe_exh = YES;
		}
		else if(i == Z_BUF_ERROR
			&& avail_out_before == params->vars.zfile.z_stream.avail_out){
		  if(params->vars.zfile.z_stream.avail_in){
		    sprintf(verbosestr, T_("Strange: inflate() in the zlib complains: can't go on, though input data available.\n"));
		    verbosefunc(verbosestr, params);
		  }

		  pipe_exh = biu_exh = biu_end = bustream_exh;

		  if(uncompress_program)
		    up_exh = bustream_exh;
		}
		else if(i != Z_OK){
		  goto formaterr_verify_regf;
		}

		if(params->vars.zfile.z_stream.next_out > buf3
				|| params->vars.zfile.z_stream.avail_in > 0)
		  continue;
	      }
	    }

#endif	/* defined(USE_ZLIB) */

	    if(subprfault){
		to_compare = BUFFERSIZ;	/* avoid infinite loop */
		pipe_exh = file_exh;
		cmpptr = NULL;
		if(bustream_exh)
		  break;
	    }

	    if(var_compr && ! bustream_exh){
	      i = params->inputfunc(buf2, 2, params);
	      if(i < 2)
		goto formaterr_verify_regf;

	      xref_to_UnsN(&to_read, buf2, 16);

	      if(to_read < 0 || to_read > 0xfff)
		goto formaterr_verify_regf;

	      if(to_read != 0xfff){
		var_compr_avail = NO;
		bustream_exh = YES;
	      }
	    }
	    else{
	      if(filelen > 4096){
		to_read = 4096;
	      }
	      else{
		to_read = filelen;
		bustream_exh = YES;
	      }

	      filelen -= to_read;
	    }

	    readptr = (builtin_uncompress ? buf2 : buf3);

	    if(to_read > 0){
	      i = params->inputfunc(readptr, to_read, params);
	      if(i < to_read)
		goto formaterr_verify_regf;
	    }
	    else{
		i = 0;
	    }

#ifdef	USE_ZLIB

	    if(cks)
		crc32sum = crc32(crc32sum, readptr, i);

#endif

	    if(!compressed){
		cmpptr = readptr;
		to_compare = to_read;
		pipe_exh = bustream_exh;

		break;
	    }

	    if(uncompress_program){

#ifdef	USE_ZLIB

		if(builtin_uncompress){
		  if(params->vars.zfile.z_stream.avail_in == 0){
		    params->vars.zfile.z_stream.avail_in = to_read;
		    params->vars.zfile.z_stream.next_in = buf2;
		  }

		  to_read = params->vars.zfile.z_stream.next_out - buf3;
		}

#endif	/* defined(USE_ZLIB) */

		left_to_write = to_read;
		wptr = buf3;
	    }
	    else{

#ifdef	USE_ZLIB

		if(builtin_uncompress){
		  params->vars.zfile.z_stream.avail_in = to_read;
		  params->vars.zfile.z_stream.next_in = buf2;
		}

#endif	/* defined(USE_ZLIB) */

	    }
	  }	/* while trying to get any more input */

	  if(to_compare == 0)
	    continue;

	  read_bytes += to_compare;

	  if(!file_exh && !fault){	/* read a bufferfull from the file to compare */
	    n = readfunc(fd, vbuf, to_compare);
	    POSZ(n);
	    if(n < to_compare)
		file_exh = YES;

	    if(cmpptr){
	      i = memcmp(vbuf, cmpptr, n);	/* compare buffers */
	      if(i){
		for(i = 0, cptr = vbuf, cptr2 = cmpptr;
				*cptr == *cptr2 && i < n;
					i++, cptr++, cptr2++);

		print_if_false(esc_name, typetag, &name_printed, params);
		sprintf(verbosestr, DP "File differs from position %s.\n",
			off_t_to_intstr(compared_bytes + i + 1, sizestr1));
		verbosefunc(verbosestr, params);

		file_exh = YES;
	      }
	    }

	    compared_bytes += n;

	    flen -= n;
	  }
	}	/* while there is sth. to read from uncompr. chain */

	if(fd >= 0)
	  close(fd);

	if(compressed){
	  if(tofd >= 0)
	    close(tofd);
	  if(fromfd >= 0)
	    close(fromfd);

	  if(pid >= 0){
	    waitpid_forced(pid, &pst, 0);
	    pst = WEXITSTATUS(pst);
	    if(pst){
		print_if_false(esc_name, typetag, &name_printed, params);
		sprintf(verbosestr, T_(" Error: Uncompressing the stored file failed.\n"));
	    }
	  }

#ifdef	USE_ZLIB

	  if(builtin_uncompress){
	    if(inflateEnd(&(params->vars.zfile.z_stream)) != Z_OK){
		print_if_false(esc_name, typetag, &name_printed, params);
		sprintf(verbosestr, T_(" Error: built-in uncompress of the stored file failed.\n"));
	    }
	    reset_zfile(&params->vars.zfile);
	  }

#endif

	}

	if(subprfault)
	  fault = YES;

	if(var_compr && !bustream_exh && pipe_exh){	/* special case */
	  i = params->inputfunc(buf2, 2, params);	/* trailing 0 0 */
	  if(i < 2)			/* not yet read, but end of  */
	    goto formaterr_verify_regf;	/* compressed data detected */

	  xref_to_UnsN(&i, buf2, 16);
	  if(i != 0)
	    goto formaterr_verify_regf;
	}

	if(statb.st_size > read_bytes && !fault){
	  print_if_false(esc_name, typetag, &name_printed, params);
	  sprintf(verbosestr,
			DP "File is longer (%s) than stored version (%s).\n",
			off_t_to_intstr(statb.st_size, sizestr1),
			off_t_to_intstr(read_bytes, sizestr2));
	  verbosefunc(verbosestr, params);
	}

	if(statb.st_size < read_bytes && !fault){
	  print_if_false(esc_name, typetag, &name_printed, params);
	  sprintf(verbosestr,
			DP "File is shorter (%s) than stored version (%s).\n",
			off_t_to_intstr(statb.st_size, sizestr1),
			off_t_to_intstr(read_bytes, sizestr2));
	  verbosefunc(verbosestr, params);
	}

	j = (cks ? 4 : 0);
	i = params->inputfunc(buf2, 1 + j, params);
	if(buf2[j] != '.' || i < 1 + j){
	  i = formaterr(&type, params, started);
	  have_an_error = YES;
	  goto tryagain;
	}

#ifdef	USE_ZLIB

	if(cks){
	  xref_to_Uns32(&lu, buf2);

	  if(lu != crc32sum){
	    fprintf(errfp, T_("Error: Wrong checksum verifying file `%s'.\n"), buf);
	    params->vars.errnum |= EUNPACKCKSUM;
	  }
	}

#endif

	started = YES;
	have_an_error = NO;

	break;

       formaterr_verify_regf:
	if(compressed){
	  if(tofd >= 0)
	    close(tofd);
	  close(fromfd);
	  if(pid >= 0){
	    kill(pid, SIGTERM);
	    waitpid(pid, &pst, 0);
	  }

#ifdef	USE_ZLIB

	  if(builtin_uncompress)
	    reset_zfile(&params->vars.zfile);

#endif

	}
	close(fd);

	i = formaterr(&type, params, started);
	have_an_error = YES;
	goto tryagain;

	break;

      case FILECONTENTSCKS:
	cks = YES;

#ifdef	USE_ZLIB

	crc32sum = crc32(0L, NULL, 0);

#else

	fprintf(stderr, T_("Warning: CRC32 checksumming not available (requires zlib)\n"));

#endif

      case FILECONTENTS:
	read_time_t(mtime, time_t);

      case FILECONTENTS_O:
	read_size_t(len, size_t);

	buf = get_mem(buf, &buf_nall, len + 1, lbuf, 256, &buf_all);
	if(!buf)
	  CLEANUPR(-ENOMEM);

	i = params->inputfunc(buf, len, params);	/* read filename */
	if(i < len)
	  goto eoferr;
	buf[len] = '\0';
	convtorel(buf, params->relative);
	mk_esc_seq(buf, ESCAPE_CHARACTER, esc_name);

	i = params->inputfunc(&c, 1, params);
	if(c != ';' || i < 1){
	  i = formaterr(&type, params, started);
	  have_an_error = YES;
	  goto tryagain;
	}

	read_size_t(len, size_t);		/* read uncompresscmd */

	buf2 = get_mem(buf2, &buf2_nall, MAX(256, len + 1),
				lbuf2, 256, &buf2_all);
	buf3 = get_mem(buf3, &buf3_nall, 4096,
				lbuf3, 256, &buf3_all);
	if(!buf2 || !buf3)
	  CLEANUPR(-ENOMEM);

	i = params->inputfunc(buf2, len, params);
	if(i < len)
	  goto eoferr;
	buf2[len] = '\0';

	i = params->inputfunc(&c, 1, params);
	if(c != ';' || i < 1){
	  i = formaterr(&type, params, started);
	  have_an_error = YES;
	  goto tryagain;
	}

	compressed = (buf2[0] ? YES : NO);

	i = stat(buf, &statb);
	uid = statb.st_uid;
	if(i){
	  params->vars.errnum |= EUNPACKNOENT;
	  fprintf(errfp, T_("Error: Cannot read file `%s'.\n"), esc_name);
	  skip = YES;
	}

	if(! name_in_list(buf, files, n_files, params->recursive, files_found)
			|| invalid_time_or_uid(mtime, uid, params)
			|| (params->uid && not_allowed(buf, params))
			|| noverify)
	  skip = YES;

	name_in_list(buf, files, n_files, 0, files_found);

	typename = "Filecontents";
	typetag = "<";

	if(!skip){
	  if(verbose){
	    print_if_false(esc_name, typetag, &name_printed, params);
	  }
	}

	fault = subprfault = NO;
	fd = -1;

	if(skip)
	  fault = YES;

	if(!fault){
	  fd = open(buf, O_RDONLY | O_BINARY);
	  if(fd < 0){
	    fault = YES;

	    sprintf(verbosestr, DP "Cannot open %s\n", esc_name);
	    verbosefunc(verbosestr, params);
	  }
	}

	builtin_uncompress = NO;
	if(buf2[0] == '.' && (! buf2[1] || buf2[1] == ' ')){
	  if(buf2[1])
	    memmove(buf2, buf2 + 2, strlen(buf2) - 2 + 1);
	  else
	    buf2[0] = '\0';

#ifndef	USE_ZLIB

		fprintf(errfp, T_("Error: Built-in (un)compression not available for file `%s'.\n"), esc_name);
		fault = YES;
		uncompress_program = NO;
		compressed = NO;
		buf2[0] = '\0';
		params->vars.errnum |= EUNPACKNOCOMPR;

#else

	  builtin_uncompress = YES;

#endif

	}
	uncompress_program = (buf2[0] ? YES : NO);

	bustream_exh = file_exh = pipe_exh = NO;
	tofd = fromfd = -1;

	if(compressed && uncompress_program){
	  i = pipe(inpp);
	  j = pipe(outpp);

	  tofd = inpp[1];
	  fromfd = outpp[0];

	  if(i || j){
	    subprfault = YES;
	    params->vars.errnum |= EUNPACKUNCOMPR;
	    fprintf(errfp,
		"Error: Cannot create pipe for uncompression of `%s'.\n",
				esc_name);
	    if(!i){
	      close(inpp[0]);
	      close(tofd);
	    }
	  }

	  if(!subprfault){
	    pid = fork_forced();
	    if(pid < 0){
	      fprintf(errfp, T_("Error: Cannot start uncompression of `%s'.\n"), esc_name);
	      params->vars.errnum |= EUNPACKUNCOMPR;
	      subprfault = YES;
	    }
	    if(!pid){		/* child */
	      char	**unzipargv;

	      close(tofd);
	      close(fromfd);

	      if(cmd2argvq(&unzipargv, buf2)){
		close(inpp[0]);
		close(outpp[1]);
		exit(1);
	      }

	      dup2(inpp[0], 0);
	      dup2(outpp[1], 1);

	      execvp(unzipargv[0], unzipargv + 1);
	      exit(3);
	    }
	  }
	  close(inpp[0]);
	  close(outpp[1]);

	  if(subprfault){
	    forever{
		i = params->inputfunc(lbuf3, 1, params);
		if(i < 1)
		  break;
		n = (Int32) lbuf3[0];
		i = params->inputfunc(lbuf3, n, params);
		if(i < 0)
		  break;
		if(n != 0xff)
		  break;
	    }
	    bustream_exh = YES;
	    filelen = 0;
	  }

	  fl = fcntl(tofd, F_GETFL);
	  fcntl(tofd, F_SETFL, fl | NONBLOCKING_FLAGS);
	}

#ifdef	USE_ZLIB

	if(builtin_uncompress){
	  reset_zfile(&params->vars.zfile);
	  if(inflateInit(&(params->vars.zfile.z_stream)) != Z_OK)
	    bi_uncompr_fault = YES;
	  params->vars.zfile.z_stream.next_out = buf3;
	  params->vars.zfile.z_stream.avail_out = 4096;
	  params->vars.zfile.z_stream.next_in = buf2;
	  params->vars.zfile.z_stream.avail_in = 0;
	}

#endif

	read_bytes = compared_bytes = written_bytes = left_to_write
					= filesize = 0;
	up_exh = biu_exh = NO;

	while(!pipe_exh){
	  forever{
	    if(uncompress_program){
	      rb = 0;
	      if(!up_exh){
		fds->fd = fromfd;
		fds->events = POLLIN;
		i = poll(fds, 1, tofd < 0 ? -1 : 0);
	 	if(i > 0 && POLLINEVENT(fds->revents)){
		  rb = read(fromfd, buf4, BUFFERSIZ);
		  POSZ(rb);
		  if(!rb && tofd < 0)
		    up_exh = YES;
		}
	      }

	      if(up_exh || rb > 0){
		to_compare = rb;
		cmpptr = buf4;

		pipe_exh = up_exh;

		if(to_compare > 0 || up_exh)
		  break;
	      }
	      if(left_to_write && tofd >= 0){
/*
		fds->fd = tofd;
		fds->events = POLLOUT;
		i = poll(fds, 1, 0);
		wb = 0;
		if(i > 0 && POLLOUTEVENT(fds->revents)){
*/
		  wb = writefunc(tofd, wptr, left_to_write);
		  POSZ(wb);
		  wptr += wb;
		  left_to_write -= wb;
/*
		}
*/

		if(!wb){
		  if(fromfd >= 0){
		    fds[0].fd = fromfd;
		    fds[0].events = POLLIN;
		  }
		  else{
		    fds[0].fd = -1;
		    fds[0].events = 0;
		  }
		  if(tofd >= 0){
		    fds[1].fd = tofd;
		    fds[1].events = POLLOUT;
		  }
		  else{
		    fds[1].fd = -1;
		    fds[1].events = 0;
		  }

		  poll(fds, 2, 100);	/* wait max 1/10 s */
		}

		if((builtin_uncompress ? biu_exh : bustream_exh)
					&& !left_to_write && tofd >= 0){
		  close(tofd);
		  tofd = -1;
		}

		continue;
	      }
	    }

#ifdef	USE_ZLIB

	    if(builtin_uncompress){
	      to_compare = params->vars.zfile.z_stream.next_out - buf3;
	      if(to_compare > 0)
		cmpptr = buf3;

	      if(biu_exh || to_compare > 0){
		params->vars.zfile.z_stream.next_out = buf3;
		params->vars.zfile.z_stream.avail_out = 4096;

		if(uncompress_program){
		  left_to_write = to_compare;
		  wptr = buf3;
		  continue;
		}
		else{
		  break;
		}
	      }

	      if(!biu_exh){
		avail_out_before = params->vars.zfile.z_stream.avail_out;

		i = inflate(&(params->vars.zfile.z_stream), /* bustream_exh */ Z_NO_FLUSH);
		if(i == Z_STREAM_END){
		  biu_exh = YES;

		  if(!uncompress_program)
		    pipe_exh = YES;
		}
		else if(i == Z_BUF_ERROR
			&& avail_out_before == params->vars.zfile.z_stream.avail_out){
		  if(params->vars.zfile.z_stream.avail_in){
		    sprintf(verbosestr, T_("Strange: inflate() in the zlib complains: can't go on, though input data available.\n"));
		    verbosefunc(verbosestr, params);
		  }

		  pipe_exh = biu_exh = bustream_exh;

		  if(uncompress_program)
		    up_exh = bustream_exh;
		}
		else if(i != Z_OK){
		  goto formaterr_verify_regf;
		}

		if(params->vars.zfile.z_stream.next_out > buf3
				|| params->vars.zfile.z_stream.avail_in > 0)
		  continue;
	      }
	    }

#endif	/* defined(USE_ZLIB) */

	    i = params->inputfunc(buf2, 1, params);
	    if(i < 1)
		goto formaterr_verify_fcont;

	    to_read = (Int32) buf2[0];

	    if(to_read != 0xff)
		bustream_exh = YES;

	    readptr = (builtin_uncompress ? buf2 : buf3);

	    i = params->inputfunc(readptr, to_read, params);
	    if(i < to_read)
		goto formaterr_verify_fcont;

#ifdef	USE_ZLIB

	    if(cks)
		crc32sum = crc32(crc32sum, readptr, i);

#endif

	    if(!compressed){
		cmpptr = readptr;
		to_compare = to_read;
		pipe_exh = bustream_exh;

		break;
	    }

	    if(uncompress_program){

#ifdef	USE_ZLIB

		if(builtin_uncompress){
		  if(params->vars.zfile.z_stream.avail_in == 0){
		    params->vars.zfile.z_stream.avail_in = to_read;
		    params->vars.zfile.z_stream.next_in = buf2;
		  }

		  to_read = params->vars.zfile.z_stream.next_out - buf3;
		}

#endif	/* defined(USE_ZLIB) */

		left_to_write = to_read;
		wptr = buf3;
	    }
	    else{

#ifdef	USE_ZLIB

		if(builtin_uncompress){
		  params->vars.zfile.z_stream.avail_in = to_read;
		  params->vars.zfile.z_stream.next_in = buf2;
		}

#endif	/* defined(USE_ZLIB) */

	    }
	  }

	  if(to_compare == 0)
	    continue;

	  read_bytes += to_compare;

	  if(!file_exh && !fault){	/* read a bufferfull from the file to compare */
	    n = readfunc(fd, vbuf, to_compare);
	    POSZ(n);
	    filesize += n;
	    if(n < to_compare)
		file_exh = YES;

	    i = memcmp(vbuf, cmpptr, n);	/* compare buffers */
	    if(i){
		for(i = 0, cptr = vbuf, cptr2 = cmpptr;
				*cptr == *cptr2 && i < n;
					i++, cptr++, cptr2++);

		print_if_false(esc_name, typetag, &name_printed, params);
		sprintf(verbosestr, DP "File differs from position %s.\n",
			off_t_to_intstr(compared_bytes + i + 1, sizestr1));
		verbosefunc(verbosestr, params);

		file_exh = YES;
	    }

	    compared_bytes += n;

	    flen -= n;
	  }
	}

	while((i = read(fd, vbuf, BUFFERSIZ)) > 0)
	  filesize += i;

	if(fd >= 0)
	  close(fd);

	if(compressed){
	  if(tofd >= 0)
	    close(tofd);
	  if(fromfd >= 0)
	    close(fromfd);

	  if(pid >= 0){
	    waitpid_forced(pid, &pst, 0);
	    pst = WEXITSTATUS(pst);
	    if(pst){
		print_if_false(esc_name, typetag, &name_printed, params);
		sprintf(verbosestr, T_(" Error: Uncompressing the stored file failed.\n"));
	    }
	  }

#ifdef	USE_ZLIB

	  if(builtin_uncompress){
	    if(inflateEnd(&(params->vars.zfile.z_stream)) != Z_OK){
		print_if_false(esc_name, typetag, &name_printed, params);
		sprintf(verbosestr, T_(" Error: built-in uncompress of the stored file failed.\n"));
	    }
	    reset_zfile(&params->vars.zfile);
	  }

#endif

	}

	if(filesize != read_bytes && !fault){
	  print_if_false(esc_name, typetag, &name_printed, params);
	  sprintf(verbosestr,
		DP "File contains %s bytes (%s) than stored version (%s).\n",
			filesize > read_bytes ? "more" : "less",
			off_t_to_intstr(filesize, sizestr1),
			off_t_to_intstr(read_bytes, sizestr2));
	  verbosefunc(verbosestr, params);
	}

	j = (cks ? 4 : 0);
	i = params->inputfunc(lbuf2, 1 + j, params);
	if(lbuf2[j] != '.' || i < 1 + j){
	  i = formaterr(&type, params, started);
	  have_an_error = YES;
	  goto tryagain;
	}

#ifdef	USE_ZLIB

	if(cks){
	  xref_to_Uns32(&lu, lbuf2);

	  if(lu != crc32sum){
	    fprintf(errfp, T_("Error: Wrong checksum verifying file `%s'.\n"), buf);
	    params->vars.errnum |= EUNPACKCKSUM;
	  }
	}

#endif

	started = YES;
	have_an_error = NO;

	break;

       formaterr_verify_fcont:
	if(compressed){
	  if(tofd >= 0)
	    close(tofd);
	  close(fromfd);
	  if(pid >= 0){
	    kill(pid, SIGTERM);
	    waitpid(pid, &pst, 0);
	  }

#ifdef	USE_ZLIB

	  if(builtin_uncompress)
	    reset_zfile(&params->vars.zfile);

#endif

	}
	close(fd);

	i = formaterr(&type, params, started);
	have_an_error = YES;
	goto tryagain;

	break;


	started = YES;
	have_an_error = NO;

	break;

      case SOLARIS2_ACL:		/* just parse */
      case HPUX10_ACL:
	read_int(i, int);

	for(i *= 3; i > 0; i--)
	  read_int(li, Int32);
	
	i = params->inputfunc(&c, 1, params);
	if(c != '.' || i < 1){
	  i = formaterr(&type, params, started);
	  have_an_error = YES;
	  goto tryagain;
	}

	if(!skip){
	  if(!params->vars.no_acl_verify_warned){
	    verbosefunc("Warning: ACL verification not yet implemented.\n",
				params);
	    params->vars.no_acl_verify_warned = YES;
	  }
	}

	started = YES;
	have_an_error = NO;

	break;

      case NFS4_ACL:
	read_int(i, int);

	for(; i > 0; i -= j){
	  j = MIN(256, i);

	  if(params->inputfunc(lbuf, j, params) < j){
	    i = formaterr(&type, params, started);
	    have_an_error = YES;
	    goto tryagain;
	  }
	}

	i = params->inputfunc(&c, 1, params);
	if(c != '.' || i < 1){
	  i = formaterr(&type, params, started);
	  have_an_error = YES;
	  goto tryagain;
	}

	if(!skip){
	  if(!params->vars.no_acl_verify_warned){
	    verbosefunc("Warning: ACL verification not yet implemented.\n",
				params);
	    params->vars.no_acl_verify_warned = YES;
	  }
	}

	started = YES;
	have_an_error = NO;

	break;

      case POSIX_ACL:			/* just parse */
	read_int(j, int);

	for(i = 0; i < 3; i++){
	  if(j & (1 << i)){
	    read_int(k, int);

	    for(; k > 0; k -= 256){
		n = (k > 256 ? 256 : k);
	    	if(params->inputfunc(lbuf, n, params) < n){
		  i = formaterr(&type, params, started);
		  have_an_error = YES;
		  goto tryagain;
		}

		n = params->inputfunc(&c, 1, params);
		if(c != ';' || n < 1){
		  i = formaterr(&type, params, started);
		  have_an_error = YES;
		  goto tryagain;
		}
	    }
	  }
	}
		  
	i = params->inputfunc(&c, 1, params);
	if(c != '.' || i < 1){
	  i = formaterr(&type, params, started);
	  have_an_error = YES;
	  goto tryagain;
	}

	if(!skip){
	  if(!params->vars.no_acl_verify_warned){
	    verbosefunc("Warning: ACL verification not yet implemented.\n",
				params);
	    params->vars.no_acl_verify_warned = YES;
	  }
	}

	started = YES;
	have_an_error = NO;

	break;

      case COMMANDINOUTCKS:
	cks = YES;

#ifdef	USE_ZLIB

	crc32sum = crc32(0L, NULL, 0);

#else

	fprintf(stderr, T_("Warning: CRC32 checksumming not available (requires zlib)\n"));

#endif

      case COMMANDINOUT:
	read_int(i, int);

	buf2 = get_mem(buf2, &buf2_nall, i + 1,
				lbuf2, 256, &buf2_all);
	if(!buf2)
	  CLEANUPR(-ENOMEM);

	j = params->inputfunc(buf2, i, params);
	if(j < i){
	  i = formaterr(&type, params, started);
	  have_an_error = YES;
	  goto tryagain;
	}
	buf2[i] = '\0';

	i = params->inputfunc(&c, 1, params);
	if(i < 1 || c != ';'){
	  i = formaterr(&type, params, started);
	  have_an_error = YES;
	  goto tryagain;
	}

	cptr = NULL;
	fault = NO;
	typetag = "";

	mk_esc_seq(buf2, ESCAPE_CHARACTER, verbosestr);

	if(! name_in_list(buf2, files, n_files, params->recursive, files_found)
			|| noverify)
	  skip = YES;

	if(!skip){
	  if(verbose){
	    print_if_false(verbosestr, typetag, &name_printed, params);
	  }
	}

	name_in_list(buf2, files, n_files, 0, files_found);
	if(skip)
	  fault = YES;

	if(!fault){
	  cptr = buf2 + strlen(CMDINOUTPREFIX);
	  cptr2 = strstr(cptr, CMDINOUTSEP);
	  *cptr2 = '\0';

	  pfd = fdpopen(cptr, O_RDONLY, &pid);

	  if(pfd < 0){
	    fault = YES;
	    params->vars.errnum |= EUNPACKWRITECMD;
	    fprintf(errfp, T_("Error: Cannot start program to pack `%s'\n"),
				verbosestr);
	  }
	}

	read_bytes = compared_bytes = bytes_from_stream = 0;

	do{
	  UChar		nbuf[8196], cmpbuf[8196];

	  i = params->inputfunc(nbuf, 2, params);
	  xref_to_UnsN(&n, nbuf, 16);
	  if(i < 2 || n > 8192){
	    if(pfd >= 0){
		close(pfd);
		if(pid > 0)
		  waitpid_forced(pid, &pst, 0);
	    }
	    pfd = pid = -1;
	    i = formaterr(&type, params, started);
	    have_an_error = YES;
	    goto tryagain;
	  }

	  i = params->inputfunc(nbuf, n, params);
	  if(i < n){
	    if(pfd >= 0){
		close(pfd);
		if(pid > 0)
		  waitpid_forced(pid, &pst, 0);
	    }
	    pfd = pid = -1;
	    i = formaterr(&type, params, started);
	    have_an_error = YES;
	    goto tryagain;
	  }

#ifdef	USE_ZLIB

	  if(cks)
	    crc32sum = crc32(crc32sum, nbuf, i);

#endif

	  bytes_from_stream += n;

	  for(cptr = cmpbuf, j = n; j > 0; cptr += i, j -= i){
	    i = readfunc(pfd, cptr, j);
	    POSZ(i);
	    if(i < 1){
		fault = YES;
		break;
	    }
	  }
	  j = n - j;

	  if(!fault){
	    rb = memcmp(cmpbuf, nbuf, j);
	    if(rb){
		for(rb = 0, cptr = nbuf, cptr2 = cmpbuf;
				*cptr == *cptr2 && rb < j;
					rb++, cptr++, cptr2++);

		print_if_false(verbosestr, typetag, &name_printed, params);
		sprintf(verbosestr, DP "Stream differs from position %s.\n",
			off_t_to_intstr(read_bytes + rb + 1, sizestr1));
		verbosefunc(verbosestr, params);
		fault = YES;
	    }
	  }

	  read_bytes += j;

	} while(n == 8192);

	if(pfd > 0){
	  for(i = 1; i > 0;){
	    i = readfunc(pfd, lbuf, 256);
	    if(i > 0)
		read_bytes += i;
	  }

	  close(pfd);
	  if(pid > 0){
	    waitpid_forced(pid, &pst, 0);
	    pst = WEXITSTATUS(pst);
	    if(pst){
		params->vars.errnum |= EUNPACKWRITECMD;
		fprintf(errfp, T_("Error: Command to pack `%s' returned bad status %d.\n"),
				verbosestr, pst);
		fault = YES;
	    }
	  }
	}

	if(bytes_from_stream != read_bytes){
	  print_if_false(verbosestr, typetag, &name_printed, params);
	  sprintf(verbosestr,
		DP "Command output contains %s bytes (%s) than stored version (%s).\n",
			read_bytes > bytes_from_stream ? "more" : "less",
			off_t_to_intstr(read_bytes, sizestr1),
			off_t_to_intstr(bytes_from_stream, sizestr2));
	  verbosefunc(verbosestr, params);
	}

	j = (cks ? 4 : 0);
	i = params->inputfunc(lbuf2, 1 + j, params);
	if(lbuf2[j] != '.' || i < 1 + j){
	  i = formaterr(&type, params, started);
	  have_an_error = YES;
	  goto tryagain;
	}

#ifdef	USE_ZLIB

	if(cks){
	  xref_to_Uns32(&lu, lbuf2);

	  if(lu != crc32sum){	/* FIXME: verbosestr is garbage here */
	    fprintf(errfp, T_("Error: Wrong checksum verifying stream `%s'.\n"), verbosestr);
	    params->vars.errnum |= EUNPACKCKSUM;
	  }
	}

#endif

	started = YES;
	have_an_error = NO;

	break;

      default:
	i = formaterr(&type, params, started);
	goto tryagain;
    }
  }

 cleanup:

  if(files){
    for(fileidx = files; *fileidx; fileidx++)
	free(*fileidx);

    free(files);
  }

  if(buf_all)
    free(buf);
  if(buf2_all)
    free(buf2);
  if(buf3_all)
    free(buf3);

  return(r);

 eoferr:

  if(files){
    for(fileidx = files; *fileidx; fileidx++)
	free(*fileidx);

    free(files);
  }

  fprintf(errfp, T_("Error: unexpected end of file.\n"));
  params->vars.errnum |= EUNPACKREAD;

  return(EOF);
}

#define	CALL_WITH_SIGPIPE_SUPPRESSED(func)		\
  Int32		r;					\
  sigset_t	nsigs, osigs;				\
							\
  sigemptyset(&nsigs);					\
  sigaddset(&nsigs, SIGPIPE);				\
  sigprocmask(SIG_BLOCK, &nsigs, &osigs);		\
							\
  r = (func);	\
							\
  sigprocmask(SIG_SETMASK, &osigs, NULL);		\
							\
  return(r);

Int32
pack_readin(UChar ** filelist, AarParams * params, UChar * files_found)
{
  CALL_WITH_SIGPIPE_SUPPRESSED(pack_readin_l(filelist, params, files_found))
}

Int32
pack_contents(AarParams * params)
{
  CALL_WITH_SIGPIPE_SUPPRESSED(pack_contents_l(params))
}

Int32
pack_verify(UChar ** filelist, AarParams * params, UChar * files_found)
{
  CALL_WITH_SIGPIPE_SUPPRESSED(pack_verify_l(filelist, params, files_found))
}

Int32
pack_default_input(UChar * buffer, Int32 num, AarParams * params)
{
  return(read_forced(params->infd, buffer, num));
}

void
pack_default_verbose(UChar * str, AarParams * params)
{
  if(params->mode == MODE_CONTENTS)
     fprintf(params->outfp, "%s", str);
  else
     fprintf(params->errfp, "%s", str);
}
