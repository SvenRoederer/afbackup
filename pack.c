/****************** Start of $RCSfile: pack.c,v $  ****************
*
* $Source: /home/alb/afbackup/afbackup-3.5.8.12/RCS/pack.c,v $
* $Id: pack.c,v 1.12 2012/11/01 09:52:51 alb Exp alb $
* $Date: 2012/11/01 09:52:51 $
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

  static char * fileversion = "$RCSfile: pack.c,v $ $Source: /home/alb/afbackup/afbackup-3.5.8.12/RCS/pack.c,v $ $Id: pack.c,v 1.12 2012/11/01 09:52:51 alb Exp alb $ " PACKAGE " " VERSION_STRING;

#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <sys/types.h>
#include <dirent.h>
#include <fcntl.h>
#ifdef	HAVE_SYS_TIME_H
#include <sys/time.h>
#endif
#ifdef	HAVE_SYS_RESOURCE_H
#include <sys/resource.h>
#endif
#ifdef	HAVE_SYS_PARAM_H
#include <sys/param.h>
#endif
#include <utime.h>
#ifdef	HAVE_SYS_MOUNT_H
#include <sys/mount.h>
#endif
#ifdef	HAVE_SYS_WAIT_H
#include <sys/wait.h>
#endif
#include <signal.h>
#include <unistd.h>

#include <genutils.h>
#include <sysutils.h>
#include <fileutil.h>
#include <mvals.h>
#include <afpacker.h>
#if defined(USE_SOLARIS2_ACLS) || defined(USE_HPUX10_ACLS) || defined(USE_POSIX_ACLS)
#include <sys/acl.h>
#endif
#ifdef	USE_ZLIB
#include <zutils.h>
#endif

#define	ER__(func, i)	{ if( (i = func) ) return(i); }

static Int32	write_filecont(UChar *, struct stat *, AarParams *, Flag, int);

struct hl_spec {
  ino_t 	ino;
  dev_t		dev;
  UChar		*name;
};

static struct hl_spec	*hardlinks = NULL;
static Int32		num_hardlinks = 0;

#ifdef	USE_ZLIB

#define	read_file(fd, cp, n, z, f)	read_file_zip(z, cp, n, f)

#else	/* defined(USE_ZLIB) */

#define	read_file(fd, cp, n, z, f) (*f)(fd, cp, n)

#endif	/* defined(USE_ZLIB) */


static Int32
add_link(UChar * name, ino_t ino, dev_t dev)
{
  struct hl_spec	*newlinks;

  newlinks = (struct hl_spec *) realloc_forced(hardlinks,
			(num_hardlinks + 1) * sizeof(struct hl_spec));
  if(! newlinks)
	return(-ENOMEM);

  hardlinks = newlinks;
  hardlinks[num_hardlinks].ino = ino;
  hardlinks[num_hardlinks].dev = dev;
  hardlinks[num_hardlinks].name = strdup(name);
  if(!hardlinks[num_hardlinks].name)
    return(-ENOMEM);

  num_hardlinks++;

  return(0);
}

static Int32
link_in_list(ino_t ino, dev_t dev, UChar ** real_name)
{
  Int32	i;

  for(i = 0; i < num_hardlinks; i++){
    if(hardlinks[i].ino == ino && hardlinks[i].dev == dev){
	*real_name = hardlinks[i].name;
	return(1);
    }
  }

  return(0);
}

static void
write_dummy_bytes(
  off_t		num,
  AarParams	*params)
{
  Int32	i;
  UChar		buf[BUFFERSIZ];

  memset(buf, 0, BUFFERSIZ * sizeof(UChar));

  do{
    i = (Int32) (num < BUFFERSIZ ? num : BUFFERSIZ);

    params->outputfunc(buf, i, params);

    num -= i;
  } while(num > 0);
}

#ifdef	S_ENFMT

/* check for l-bit on platforms supporting that:
 * if l-bit set (== SetGID and no execute permission for group):
 * try to get lock. If that succeeds, backup file as normal
 * holding the lock. If lock cannot be obtained: error message
 */

static int
open_check_rlock(char * filen, int flags)
{
  struct stat	statb;
  struct flock  lockb;
  int		i, fd;

  if((i = stat(filen, &statb)) < 0)
    return(i);

  fd = open(filen, flags);
  if(fd < 0)
    return(fd);

  if((statb.st_mode & S_ENFMT) && !(statb.st_mode & S_IXGRP)){
    /* the pathological case */
    SETZERO(lockb);
    lockb.l_type = F_RDLCK;

    i = fcntl(fd, F_SETLK, &lockb);
    if(i < 0){
	close(fd);
	return((int) i);
    }

    /* lock is released on close, so we don't
     * have to care about that further */
  }

  return(fd);
}

#define	open_for_pack	open_check_rlock
#else
#define	open_for_pack	open
#endif

static Int32
open_for_compress(
  int		*fd,
  UChar		*name,
  UChar		*zipcmd,
  int		*pid,
  AarParams	*params)
{
  int		i, pp[2], lpid;

  *pid = -1;
  *fd = -1;

#ifdef	USE_ZLIB

  SETZERO(params->vars.zfile);

  if(!zipcmd){
    *fd = open_for_pack(name, O_RDONLY | O_BINARY);

    if(params->bi_compr_level){
	i = open_file_zip(&params->vars.zfile, *fd, params->bi_compr_level,
					params->bi_compr_maxblock);
	if(i)
	  return(i);
    }

    return(!(*fd >= 0));
  }

#endif    

  i = pipe(pp);
  if(i){
    return(1);
  }

  lpid = fork_forced();
  if(lpid < 0){
    close(pp[0]);
    close(pp[1]);
    return(2);
  }

  if(lpid){	/* parent */
    close(pp[1]);

    *fd = pp[0];
    *pid = lpid;

#ifdef	USE_ZLIB

    i = open_file_zip(&params->vars.zfile, *fd, params->bi_compr_level,
					params->bi_compr_maxblock);
    if(i)
	return(i);

#endif

    return(0);
  }
  else{		/* child */
    int		ifd;
    char	**zipargv;

    clr_timer();

    close(pp[0]);

    if(cmd2argvq(&zipargv, zipcmd)){
	close(pp[1]);
	exit(1);
    }

    ifd = open_for_pack(name, O_RDONLY | O_BINARY);
    if(ifd < 0){
	exit(2);
    }

    dup2(ifd, 0);
    dup2(pp[1], 1);

    execvp(zipargv[0], zipargv + 1);

    close(pp[1]);
    exit(3);
  }
}

static Int32
write_cmdio(UChar * cmd, AarParams * params, int tattrs)
{
  Int32		r = NO_ERROR, i, n;
  FILE		*errfp;
  int		pfd, pid, pst, ftype;
  UChar		lbuf[8196], *buf, *cmd1, *cmd2, c, *cptr;
  Int32		(*ofunc)(UChar *, Int32, AarParams *);
  Int64		(*readfunc)(int, UChar *, Int64);
  Flag		cks, buf_malloced = NO;
  Uns32		crc32sum;	/* uninitialized OK */

  cks = NO;
  ftype = COMMANDINOUT;
  pfd = -1;

#ifdef	USE_ZLIB

  cks = params->check;
  if(cks){
    ftype = COMMANDINOUTCKS;
    crc32sum = crc32(0L, Z_NULL, 0);
  }

#endif  

  ftype |= tattrs;

  cmd1 = cmd + strlen(CMDINOUTPREFIX);
  cmd2 = strstr(cmd1, CMDINOUTSEP);
  c = *cmd2;
  *cmd2 = '\0';

  errfp = params->errfp;
  readfunc = params->readfunc;

  n = strlen(cmd1) + 1;
  i = strlen(cmd) + 20;
  n = MAX(n, i);
  buf = get_mem(NULL, NULL, n, lbuf, 8196, &buf_malloced);
  if(!buf)
    CLEANUPR(-ENOMEM);

  strcpy(buf, cmd1);
  repl_esc_seq(buf, ESCAPE_CHARACTER);

  pid = -1;
  pfd = fdpopen(buf, O_RDONLY, &pid);
  if(pfd < 0){
    fprintf(errfp, T_("Error: Cannot run command `%s' to read output.\n"), cmd1);
    params->vars.errnum |= EPACKREADCMD;
    CLEANUPR(-EINVAL);
  }

  *cmd2 = c;
  ofunc = params->outputfunc;

  if(params->pre_verbosefunc)
    (*params->pre_verbosefunc)(NULL, params);

  sprintf(buf, "%d;%d;%s;", ftype, (int) strlen(cmd), cmd);
  if( (r = ofunc(buf, strlen(buf), params)) )
    CLEANUP;

  do{
    for(n = 8192, cptr = buf + 2; n >= 0; cptr += i, n -= i){
	i = readfunc(pfd, cptr, n);
	if(i < 1)
	  break;

#ifdef	USE_ZLIB

	if(cks)
	  crc32sum = crc32(crc32sum, cptr, i);

#endif

    }
    n = 8192 - n;
    UnsN_to_xref(buf, n, 16);

    if( (r = ofunc(buf, n + 2, params)) )
	CLEANUP;
  } while(n == 8192);

  if(cks)
    Uns32_to_xref(buf, crc32sum);

  buf[4] = '.';
  i = (cks ? 0 : 4);
  if( (i = ofunc(buf + i, 1 + 4 - i, params)) )
    if(!r)
	r = i;

  if(params->verbose && params->verbosefunc){
    sprintf(buf, "%s\n", cmd);
    params->verbosefunc(buf, params);
  }

 cleanup:
  if(buf_malloced)
    free(buf);
  if(pfd >= 0){
    close(pfd);

    if(pid > 0){
	waitpid_forced(pid, &pst, 0);
	pst = WEXITSTATUS(pst);
	if(pst)
	  r = - pst;
    }
  }

  return(r);
}

#define	time_cond(statb, params)	\
	((! params->time_newer || params->time_newer <= statb.st_mtime)	\
	&& (! params->time_older || params->time_older > statb.st_mtime))
#define	uid_cond(statb, params)		\
	(! params->uid || (statb.st_uid == params->uid))

static Int32
write_file_uncompressed(
  UChar		*name,
  struct stat	*statb,
  AarParams	*params,
  int		type_attrs)
{
  int		fd, ftype;
  Int32		i, r = 0;
  off_t		rd_bytes;
  off_t		bytes;
  UChar		buf[BUFFERSIZ];
  UChar		mtimestr[50], sizestr[50];
  FILE		*errfp;
  Int32		(*ofunc)(UChar *, Int32, AarParams *);
  Int64		(*readfunc)(int, UChar *, Int64);
  Flag		cks;
  Uns32		crc32sum;	/* uninitialized OK */

  cks = NO;
  ftype = REGFILE;

#ifdef	USE_ZLIB

  cks = params->check;
  if(cks){
    ftype = REGFILECKS;
    crc32sum = crc32(0L, Z_NULL, 0);
  }

#endif  

  ftype |= type_attrs;

  ofunc = params->outputfunc;
  errfp = params->errfp;
  readfunc = params->readfunc;

  fd = open_for_pack(name, O_RDONLY | O_BINARY);
  if(fd < 0){
    fprintf(errfp, T_("Error: cannot open file `%s'.\n"), name);
    params->vars.errnum |= EPACKNOENT;
    return(-ENOENT);
  }

  sprintf(buf, "%d;%lu;%s;%d;%d;%d;", ftype,
		(unsigned long) statb->st_mode,
		(char *) time_t_to_intstr(statb->st_mtime, mtimestr),
		(int) statb->st_uid, (int) statb->st_gid,
		(int) strlen(name));
  i = ofunc(buf, strlen(buf), params);
  if(!i)
    i = ofunc(name, strlen(name), params);
  sprintf(buf, ";%d;%s;%s;", 0, "",
		(char *) off_t_to_intstr(statb->st_size, sizestr));
  if(!i)
    i = ofunc(buf, strlen(buf), params);
  if(i){
    close(fd);
    params->vars.errnum |= EPACKWRITE;
    CLEANUPR(i);
  }

  rd_bytes = 0;

  if(statb->st_size > 0){
    do{
      i = readfunc(fd, buf, BUFFERSIZ);
      if(i < 0)
	i = 0;

#ifdef	USE_ZLIB

      if(cks && i > 0)
	crc32sum = crc32(crc32sum, buf, i);

#endif

      bytes = i;

      if(rd_bytes + bytes > statb->st_size){
	fprintf(errfp,
		T_("Error: File `%s' changed size while reading, trailing bytes not saved.\n"),
		name);
	bytes = statb->st_size - rd_bytes;
	params->vars.errnum |= EPACKSIZECH;
      }

      rd_bytes += bytes;

      if( (i = ofunc(buf, bytes, params)) ){
	close(fd);
	params->vars.errnum |= EPACKWRITE;
	CLEANUPR(i);
      }
    } while(rd_bytes < statb->st_size && bytes > 0);
  }

  close(fd);

  if(rd_bytes < statb->st_size){
    fprintf(errfp, T_("Error: Unexpected fault reading `%s'.\n"), name);
    params->vars.errnum |= EPACKREAD;

    write_dummy_bytes(statb->st_size - rd_bytes, params);
  }
  else{
    if(cks){
	Uns32_to_xref(buf, crc32sum);
	if( (i = ofunc(buf, 4, params)) ){
	  params->vars.errnum |= EPACKWRITE;
	  CLEANUPR(i);
	}
    }
  }

  params->vars.sum_uncompr_filesizes += (Real64) statb->st_size;

 cleanup:
  return(r);
}

#ifdef	USE_POSIX_ACLS
#ifdef	ACL_TYPE_DEFAULT_DIR
#define	PACKER_ACL_NUM_TYPES	3
#else
#define	PACKER_ACL_NUM_TYPES	2
#endif

#ifdef	NACLBASE
#define	PACKER_MIN_ACL_ENTRIES	NACLBASE
#else
#ifdef	ACL_MIN_ENTRIES
#define	PACKER_MIN_ACL_ENTRIES	ACL_MIN_ENTRIES
#else
#define	PACKER_MIN_ACL_ENTRIES	4
#endif
#endif

acl_type_t packer_posix_acl_types_arr[3] = {
		ACL_TYPE_ACCESS,
		ACL_TYPE_DEFAULT,
#if PACKER_ACL_NUM_TYPES >= 3
		ACL_TYPE_DEFAULT_DIR,
#endif
};

Int32
packer_posix_acl_types(Int32 idx)
{
  return(packer_posix_acl_types_arr[idx]);
}
#endif

static Int32
write_acls(UChar * name, struct stat * statb, AarParams * params)
{
  Int32		i, k, n;
  UChar		sbuf[128];	/* space for > 3 x 64 Bit unsigned */

#ifdef	USE_POSIX_ACLS
{
  Uns32		acl_types;
  acl_t		acl;
  acl_entry_t	acl_entry;
  UChar		*acl_strs[3];
  ssize_t	aclstrlens[3];

  memset(acl_strs, 0, 3 * sizeof(acl_strs[0]));
  acl_types = 0;

  n = IS_DIRECTORY((*statb)) ? PACKER_ACL_NUM_TYPES : 1;
  for(i = 0; i < n; i++){
    acl = acl_get_file(name, packer_posix_acl_types(i));
    if(acl){
#ifndef	sgi
#ifndef	__osf__

	for(k = 0; acl_get_entry(acl, k ? ACL_NEXT_ENTRY : ACL_FIRST_ENTRY, &acl_entry); k++);

#else
	acl_first_entry(acl);			/* this is for OSF1 */
	for(k = 0; acl_get_entry(acl); k++);
	acl_first_entry(acl);
#endif
#else
	k = acl->acl_cnt;			/* and this is for IRIX */
#endif

	if(k > PACKER_MIN_ACL_ENTRIES){
	  acl_strs[i] =
#ifdef	HAVE_ACL_TO_SHORT_TEXT
			acl_to_short_text
#else
			acl_to_text
#endif
					(acl, &(aclstrlens[i]));
	  if(!acl_strs[i])
	    return(-ENOMEM);

	  acl_types |= (1 << i);
	}
        acl_free(acl);
    }
  }

  if(acl_types){
    sprintf(sbuf, "%d;%d", (int) POSIX_ACL, (int) acl_types);
    if( (k = params->outputfunc(sbuf, strlen(sbuf), params)) )
	return(k);

    for(i = 0; i < n; i++){
      if(acl_strs[i]){
	sprintf(sbuf, ";%d;", (int) aclstrlens[i]);
	if( (k = params->outputfunc(sbuf, strlen(sbuf), params)) )
	  return(k);
	if( (k = params->outputfunc(acl_strs[i], aclstrlens[i], params)) )
	  return(k);
#ifdef	HAVE_ACL_FREE_TEXT
	acl_free_text(acl_strs[i]);
#else
	acl_free(acl_strs[i]);
#endif
      }
    }
    if( (k = params->outputfunc(";.", 2, params)) )
	return(k);
  }
}
#else	/* defined(USE_POSIX_ACLS) */

#ifdef	USE_SOLARIS2_ACLS
#ifdef	USE_NFS4_ACLS

/*if(!acl_trivial(name))*/
/* buggy, return value is inconsistent with acl_get(..., ACL_NO_TRIVIAL, ...) */
#endif
{
  aclent_t	*pacls;
  int		nacls;

  nacls = acl(name, GETACLCNT, 0, NULL);
  if(nacls > MIN_ACL_ENTRIES){
    pacls = NEWP(aclent_t, nacls);
    if(!pacls)
	return(-ENOMEM);

    nacls = acl(name, GETACL, nacls, pacls);

    sprintf(sbuf, "%d;%d;", (int) SOLARIS2_ACL, (int) nacls);
    if( (k = params->outputfunc(sbuf, strlen(sbuf), params)) )
	return(k);

    for(i = 0; i < nacls; i++){
	sprintf(sbuf, "%d;%d;%d;", (int) pacls[i].a_type,
			(int) pacls[i].a_id, (int) pacls[i].a_perm);
	if( (k = params->outputfunc(sbuf, strlen(sbuf), params)) ){
	  free(pacls);
	  return(k);
	}
    }

    free(pacls);
    if( (k = params->outputfunc(".", 1, params)) )
	return(k);
  }
#ifdef	USE_NFS4_ACLS
  else{
    acl_t	*acls = NULL;
    char	*aclstr = NULL;

    if(!acl_get(name, ACL_NO_TRIVIAL, &acls)){
      if(acls){
	aclstr = acl_totext(acls, ACL_COMPACT_FMT | ACL_APPEND_ID);
	if(!aclstr)
	  return(-ENOMEM);

	sprintf(sbuf, "%d;%d;", (int) NFS4_ACL, strlen(aclstr));
	if( (k = params->outputfunc(sbuf, strlen(sbuf), params)) ){
	  free(aclstr);
	  return(k);
	}
	if( (k = params->outputfunc(aclstr, strlen(aclstr), params)) ){
	  free(aclstr);
	  return(k);
	}

	free(aclstr);
	if( (k = params->outputfunc(".", 1, params)) )
	  return(k);
      }
    }
  }
#endif
}
#endif

#ifdef	USE_HPUX10_ACLS
{
  struct acl_entry	*acls;
  int			nacls;

  nacls = getacl(name, 0, NULL);
  if(nacls > NBASEENTRIES){
    acls = NEWP(struct acl_entry, nacls);
    if(!acls)
	return(-ENOMEM);

    nacls = getacl(name, nacls, acls);

    sprintf(sbuf, "%d;%d;", (int) HPUX10_ACL, (int) nacls);
    if( (k = params->outputfunc(sbuf, strlen(sbuf), params)) )
	return(k);

    for(i = 0; i < nacls; i++){
	sprintf(sbuf, "%d;%d;%d;", (int) acls[i].uid,
			(int) acls[i].gid, (int) acls[i].mode);
	if( (k = params->outputfunc(sbuf, strlen(sbuf), params)) ){
	  free(acls);
	  return(k);
	}
    }

    free(acls);
    if( (k = params->outputfunc(".", 1, params)) )
	return(k);
  }
}
#endif
#endif /* defined(USE_POSIX_ACLS) */

  return(0);
}

static Int32
l_writeout(UChar * name, AarParams * params, Flag dont_save_as_hardlink)
{
  struct stat	statb, pdirstatb;
  Int32		i, n, r = 0;
  Flag		compress;
  UChar		*buf, lbuf[256], sbuf[128], mtimestr[48];
  UChar		*verbosestr = NULL, lverbosestr[256];
  UChar		*esc_name = NULL, lesc_name[256];
  Flag		esc_name_all = NO, verbosestr_all = NO, buf_all = NO;
  Int32		esc_name_nall = 0, verbosestr_nall = 0, buf_nall = 0;
  int		fd, pid, pst, ftype, ftype_attrs;
  UChar		*hlinkname;
  DIR		*dir;
  struct dirent	*entry;
  UChar		*cptr;
  FILE		*errfp;
  UChar		*unzipcmd;
  Int32		(*ofunc)(UChar *, Int32, AarParams *);
  Int64		(*readfunc)(int, UChar *, Int64);
  Uns8		filecontents = 0;
  struct utimbuf utim;
  Flag		cks;
  Uns32		crc32sum;	/* uninitialized OK */
  Flag		no_hardlink;

  buf = lbuf;

  cks = NO;

#ifdef	USE_ZLIB

  cks = params->check;

#else

  if(params->check){
    fprintf(stderr, T_("Warning: CRC32 checksumming not available (requires zlib).\n"));
    params->check = NO;
  }

#endif  

  ftype_attrs = 0;
  if(! strncmp(name, NOVERIFYPREFIX, i = strlen(NOVERIFYPREFIX))){
    ftype_attrs |= PACK_NOVERIFY;
    name += i;
  }

  if(! strncmp(name, CMDINOUTPREFIX, i = strlen(CMDINOUTPREFIX)))
    if(strstr(name + i, CMDINOUTSEP))
	CLEANUPR(write_cmdio(name, params, ftype_attrs));

  ofunc = params->outputfunc;
  readfunc = params->readfunc;

  unzipcmd = "";
  if(params->zipcmd && params->unzipcmd)
    if(*params->zipcmd && *params->unzipcmd)
      unzipcmd = params->unzipcmd;

  errfp = params->errfp;

  if(! strncmp(name, FILECONTPREFIX, i = strlen(FILECONTPREFIX))){
    filecontents = 1;
    name += i;
  }
  if(! strncmp(name, FILECONTZPREFIX, i = strlen(FILECONTZPREFIX))){
    filecontents = 1 | ((unzipcmd[0]
#ifdef	USE_ZLIB
			|| params->bi_compr_level > 0
#endif
				) ? (1 << 1) : 0);
    name += i;
  }

  esc_name = get_mem(esc_name, &esc_name_nall, strlen(name) + 1,
			lesc_name, 256, &esc_name_all);
  if(!esc_name)
    CLEANUPR(-errno);

  strcpy(esc_name, name);

  repl_esc_seq(name, ESCAPE_CHARACTER);

  i = lstat(name, &statb);

  if(i < 0){
    fprintf(errfp, T_("Error: cannot lstat `%s'.\n"), name);
    params->vars.errnum |= EPACKNOENT;
    CLEANUPR(-ENOENT);
  }

  if((! IS_DIRECTORY(statb) || ! params->recursive)
			&& (! time_cond(statb, params)
				|| ! uid_cond(statb, params)))
    CLEANUPR(0);

  params->vars.num_fsentries++;

  SETZERO(utim);	/* on some architectures utim is more complex */
  utim.modtime = statb.st_mtime;
  utim.actime = statb.st_atime;

  params->vars.uid = statb.st_uid;
  params->vars.mtime = statb.st_mtime;

#ifndef	HAVE_LCHOWN

  if(IS_SYMLINK(statb)){	/* if the symlink is owned by root, print */
    if(statb.st_uid == 0 && statb.st_gid == 0){	/* UID of the parent dir */
	verbosestr = get_mem(verbosestr, &verbosestr_nall,
			strlen(name) + 1, lverbosestr, 256,
			&verbosestr_all);
	if(!verbosestr)
	  CLEANUPR(-errno);
	strcpy(verbosestr, name);
	cleanpath(verbosestr);
	cptr = FN_LASTDIRDELIM(verbosestr);
	if(cptr){
	  *(cptr + 1) = '\0';
	  if(!FN_ISROOTDIR(verbosestr))
	    *cptr = '\0';
	  i = stat(verbosestr, &pdirstatb);
	}
	else{
	  i = stat(FN_CURDIR, &pdirstatb);
	}

	if(!i)
	  params->vars.uid = pdirstatb.st_uid;
    }
  }

#endif

  if(params->pre_verbosefunc && ! IS_DIRECTORY(statb))
    (*params->pre_verbosefunc)(NULL, params);

  if(IS_SYMLINK(statb)){
    for(n = i = 192; i >= n; n += 64){
	buf = get_mem(buf, &buf_nall, n + 1, lbuf, 256, &buf_all);
	if(!buf)
	  CLEANUPR(-errno);

	i = readlink(name, buf, n);

	if(i < 0){
	  fprintf(errfp, T_("Error: cannot read symlink `%s'.\n"), name);
	  params->vars.errnum |= EPACKSYMLINK;
	  CLEANUPR(-errno);
	}
    }

    buf[i] = '\0';

    sprintf(sbuf, "%d;%lu;%s;%d;%d;%d;", SYMLINK | ftype_attrs,
			(unsigned long) statb.st_mode,
			(char *) time_t_to_intstr(statb.st_mtime, mtimestr),
			(int) statb.st_uid, (int) statb.st_gid,
			(int) strlen(name));
    i = ofunc(sbuf, strlen(sbuf), params);
    if(!i)
	i = ofunc(name, strlen(name), params);
    sprintf(sbuf, ";%d;", (int) strlen(buf));
    if(!i)
	i = ofunc(sbuf, strlen(sbuf), params);
    if(!i)
	i = ofunc(buf, strlen(buf), params);
    if(!i)
	i = ofunc(".", 1 /* strlen(".") */, params);
    if(i){
	params->vars.errnum |= EPACKWRITE;
	CLEANUPR(i);
    }

    /* if(!params->dont_keep_atime)
	utime(name, &utim); */ /* Yes, this was a bug */

    if(params->verbose && params->verbosefunc){
	verbosestr = get_mem(verbosestr, &verbosestr_nall,
			strlen(esc_name) + 2, lverbosestr, 256,
			&verbosestr_all);
	if(!verbosestr)
	  CLEANUPR(-errno);

	sprintf(verbosestr, "%s\n", esc_name);
	params->verbosefunc(verbosestr, params);
    }

    CLEANUPR(0);
  }

  i = stat(name, &statb);

  if(i == -1){
    fprintf(errfp, T_("Error: cannot stat `%s'.\n"), name);
    params->vars.errnum |= EPACKNOENT;

    CLEANUPR(-ENOENT);
  }

  no_hardlink = (!IS_HARDLINK(statb) || dont_save_as_hardlink);

  if(!IS_DIRECTORY(statb) && no_hardlink){
    i = write_acls(name, &statb, params);
    if(i){
	params->vars.errnum |= EPACKACLS;
	CLEANUPR(i);
    }
  }

  utim.modtime = statb.st_mtime;
  utim.actime = statb.st_atime;

  if(filecontents){
    ER__(write_filecont(name, &statb, params,
			filecontents & (1 << 1), ftype_attrs), i);

    if(params->verbose && params->verbosefunc){
	verbosestr = get_mem(verbosestr, &verbosestr_nall,
			strlen(esc_name) + 2, lverbosestr, 256,
			&verbosestr_all);
	if(!verbosestr)
	  CLEANUPR(-errno);

	sprintf(verbosestr, "%s\n", esc_name);
	params->verbosefunc(verbosestr, params);
    }
  } else if(IS_DIRECTORY(statb)){
    if(params->recursive){
      dir = opendir(name);

      if(!dir){
	fprintf(errfp, T_("Error: cannot read directory `%s'.\n"), name);
	params->vars.errnum |= EPACKNOENT;

	CLEANUPR(-ENOENT);
      }

      forever{
	entry = readdir(dir);
	if(! entry)
	  break;

	cptr = (UChar *) &(entry->d_name[0]);

	if(!strcmp(cptr, FN_CURDIR) || !strcmp(cptr, FN_PARENTDIR))
	  continue;

	i = strlen(esc_name);

	buf = get_mem(buf, &buf_nall,
			i + ((FN_DIRSEPLEN + strlen(cptr)) << 2) + 1,
			lbuf, 256, &buf_all);
	if(!buf)
	  CLEANUPR(-errno);

	strcpy(buf, esc_name);
	if(i < FN_DIRSEPLEN || !FN_ISDIRSEP(buf[i - FN_DIRSEPLEN])){
	  mk_esc_seq(FN_DIRSEPSTR, ESCAPE_CHARACTER, buf + i);
	  i += strlen(buf + i);
	}
	mk_esc_seq(cptr, ESCAPE_CHARACTER, buf + i);

	l_writeout(buf, params, NO);
      }

      closedir(dir);
    }

    if(time_cond(statb, params)){	/* only, if time condition satisf */
      if(params->pre_verbosefunc)
	(*params->pre_verbosefunc)(NULL, params);

      i = write_acls(name, &statb, params);
      if(i){
	params->vars.errnum |= EPACKACLS;
	CLEANUPR(i);
      }

      sprintf(sbuf, "%d;%lu;%s;%d;%d;%d;", DIRECTORY | ftype_attrs,
			(unsigned long) statb.st_mode,
			(char *) time_t_to_intstr(statb.st_mtime, mtimestr),
			(int) statb.st_uid, (int) statb.st_gid,
			(int) strlen(name));
      i = ofunc(sbuf, strlen(sbuf), params);
      if(!i)
	i = ofunc(name, strlen(name), params);
      if(!i)
	i = ofunc(".", 1 /* strlen(".") */, params);
      if(i){
	params->vars.errnum |= EPACKWRITE;
	CLEANUPR(i);
      }

      if(params->verbose && params->verbosefunc){
	verbosestr = get_mem(verbosestr, &verbosestr_nall,
			strlen(esc_name) + 2, lverbosestr, 256,
			&verbosestr_all);
	if(!verbosestr)
	  CLEANUPR(-errno);

	sprintf(verbosestr, "%s\n", esc_name);
	params->verbosefunc(verbosestr, params);
      }
    }

    if(!params->dont_keep_atime)
	utime(name, &utim);
  }
  else if(! no_hardlink){
    if(link_in_list(statb.st_ino, statb.st_dev, &hlinkname)){
      sprintf(sbuf, "%d;%lu;%s;%d;%d;%d;", HARDLINK | ftype_attrs,
			(unsigned long) statb.st_mode,
			(char *) time_t_to_intstr(statb.st_mtime, mtimestr),
			(int) statb.st_uid, (int) statb.st_gid,
			(int) strlen(name));
      i = ofunc(sbuf, strlen(sbuf), params);
      if(!i)
	i = ofunc(name, strlen(name), params);
      sprintf(sbuf, ";%d;", (int) strlen(hlinkname));
      if(!i)
	i = ofunc(sbuf, strlen(sbuf), params);
      if(!i)
	i = ofunc(hlinkname, strlen(hlinkname), params);
      if(!i)
	i = ofunc(".", 1 /* strlen(".") */, params);
      if(i){
	params->vars.errnum |= EPACKWRITE;
	CLEANUPR(i);
      }

      if(params->verbose && params->verbosefunc){
	verbosestr = get_mem(verbosestr, &verbosestr_nall,
			strlen(esc_name) + 2, lverbosestr, 256,
			&verbosestr_all);
	if(!verbosestr)
	  CLEANUPR(-errno);

	sprintf(verbosestr, "%s\n", esc_name);
	params->verbosefunc(verbosestr, params);
      }
    }
    else{
	l_writeout(esc_name, params, YES);

	add_link(name, statb.st_ino, statb.st_dev);
    }

    if(!params->dont_keep_atime)
	utime(name, &utim);
  }
  else if(IS_FIFO(statb)){
    sprintf(sbuf, "%d;%lu;%s;%d;%d;%d;", FIFO | ftype_attrs,
			(unsigned long) statb.st_mode,
			(char *) time_t_to_intstr(statb.st_mtime, mtimestr),
			(int) statb.st_uid, (int) statb.st_gid,
			(int) strlen(name));
    i = ofunc(sbuf, strlen(sbuf), params);
    if(!i)
	i = ofunc(name, strlen(name), params);
    if(!i)
	i = ofunc(".", 1 /* strlen(".") */, params);
    if(i){
	params->vars.errnum |= EPACKWRITE;
	CLEANUPR(i);
    }

    if(!params->dont_keep_atime)
	utime(name, &utim);

    if(params->verbose && params->verbosefunc){
	verbosestr = get_mem(verbosestr, &verbosestr_nall,
			strlen(esc_name) + 2, lverbosestr, 256,
			&verbosestr_all);
	if(!verbosestr)
	  CLEANUPR(-errno);

	sprintf(verbosestr, "%s\n", esc_name);
	params->verbosefunc(verbosestr, params);
    }
  }
  else if(IS_SOCKET(statb)){
    sprintf(sbuf, "%d;%lu;%s;%d;%d;%d;", SOCKET | ftype_attrs,
			(unsigned long) statb.st_mode,
			(char *) time_t_to_intstr(statb.st_mtime, mtimestr),
			(int) statb.st_uid, (int) statb.st_gid,
			(int) strlen(name));
    i = ofunc(sbuf, strlen(sbuf), params);
    if(!i)
	i = ofunc(name, strlen(name), params);
    if(!i)
	i = ofunc(".", 1 /* strlen(".") */, params);
    if(i){
	params->vars.errnum |= EPACKWRITE;
	CLEANUPR(i);
    }

    if(!params->dont_keep_atime)
	utime(name, &utim);

    if(params->verbose && params->verbosefunc){
	verbosestr = get_mem(verbosestr, &verbosestr_nall,
			strlen(esc_name) + 2, lverbosestr, 256,
			&verbosestr_all);
	if(!verbosestr)
	  CLEANUPR(-errno);

	sprintf(verbosestr, "%s\n", esc_name);
	params->verbosefunc(verbosestr, params);
    }
  }
  else if(IS_BLOCKDEV(statb)){
    sprintf(sbuf, "%d;%lu;%s;%d;%d;%lu;%d;", BLOCKDEVICE | ftype_attrs,
			(unsigned long) statb.st_mode,
			(char *) time_t_to_intstr(statb.st_mtime, mtimestr),
			(int) statb.st_uid, (int) statb.st_gid,
			(unsigned long) statb.st_rdev,
			(int) strlen(name));
    i = ofunc(sbuf, strlen(sbuf), params);
    if(!i)
	i = ofunc(name, strlen(name), params);
    if(!i)
	i = ofunc(".", 1 /* strlen(".") */, params);
    if(i){
	params->vars.errnum |= EPACKWRITE;
	CLEANUPR(i);
    }

    if(!params->dont_keep_atime)
	utime(name, &utim);

    if(params->verbose && params->verbosefunc){
	verbosestr = get_mem(verbosestr, &verbosestr_nall,
			strlen(esc_name) + 2, lverbosestr, 256,
			&verbosestr_all);
	if(!verbosestr)
	  CLEANUPR(-errno);

	sprintf(verbosestr, "%s\n", esc_name);
	params->verbosefunc(verbosestr, params);
    }
  }
  else if(IS_CHARDEV(statb)){
    sprintf(sbuf, "%d;%lu;%s;%d;%d;%lu;%d;", CHARDEVICE | ftype_attrs,
			(unsigned long) statb.st_mode,
			(char *) time_t_to_intstr(statb.st_mtime, mtimestr),
			(int) statb.st_uid, (int) statb.st_gid,
			(unsigned long) statb.st_rdev,
			(int) strlen(name));
    i = ofunc(sbuf, strlen(sbuf), params);
    if(!i)
	i = ofunc(name, strlen(name), params);
    if(!i)
	i = ofunc(".", 1 /* strlen(".") */, params);
    if(i){
	params->vars.errnum |= EPACKWRITE;
	CLEANUPR(i);
    }

    if(!params->dont_keep_atime)
	utime(name, &utim);

    if(params->verbose && params->verbosefunc){
	verbosestr = get_mem(verbosestr, &verbosestr_nall,
			strlen(esc_name) + 2, lverbosestr, 256,
			&verbosestr_all);
	if(!verbosestr)
	  CLEANUPR(-errno);

	sprintf(verbosestr, "%s\n", esc_name);
	params->verbosefunc(verbosestr, params);
    }
  }
  else if(IS_REGFILE(statb)){
    Flag	compression_done, dont_compress;

    dont_compress = NO;
    if(params->dont_compress){
      UChar	**cpptr;

      for(cpptr = params->dont_compress; *cpptr; cpptr++){
	if(!fn_match(*cpptr, FN_BASENAME(name), 0)){
	  dont_compress = YES;
	  break;
	}
      }
    }

    params->vars.sum_filesizes += (Real64) statb.st_size;

    compress = ((unzipcmd[0]
#ifdef	USE_ZLIB
			|| params->bi_compr_level > 0
#endif
			) && statb.st_size > 0 && (!dont_compress));

    if(! compress){
	ER__(write_file_uncompressed(name, &statb, params, ftype_attrs), i);
    }
    else{
      UChar	compressbuf[0x1000 + 2];

      compression_done = NO;
      fd = pid = -1;

      i = open_for_compress(&fd, name, params->zipcmd, &pid, params);
      if(!i){
	n = strlen(unzipcmd);

	cptr = "";

	if(params->bi_compr_level > 0){
	  if(n > 0){
	    n++;
	    cptr = ". ";
	  }
	  else{
	    cptr = ".";
	  }
	  n++;

	  cks = NO;	/* built-in compression already does checksumming */
	}
	
	ftype = REGFILE;

#ifdef	USE_ZLIB

	if(cks){
	  ftype = REGFILECKS;
	  crc32sum = crc32(0L, Z_NULL, 0);
	}

#endif

	sprintf(sbuf, "%d;%lu;%s;%d;%d;%d;", ftype | ftype_attrs,
			(unsigned long) statb.st_mode,
			(char *) time_t_to_intstr(statb.st_mtime, mtimestr),
			(int) statb.st_uid, (int) statb.st_gid,
			(int) strlen(name));
	i = ofunc(sbuf, strlen(sbuf), params);
	if(!i)
	  i = ofunc(name, strlen(name), params);
	sprintf(sbuf, ";%d;%s", (int) n, cptr);
	if(!i)
	  i = ofunc(sbuf, strlen(sbuf), params);
	if(!i)
	  i = ofunc(unzipcmd, strlen(unzipcmd), params);
	sprintf(sbuf, ";%d;", 0);
	if(!i)
	  i = ofunc(sbuf, strlen(sbuf), params);
	if(i){
	  params->vars.errnum |= EPACKWRITE;
	  CLEANUPR(i);
	}

	compression_done = YES;

	do{
	  n = read_file(fd, compressbuf + 2, 0xfff, &params->vars.zfile, readfunc);

	  if(n < 0){
	    fprintf(params->errfp,
			T_("Error: Fault during read of compressed file `%s'.\n"), name);
	    params->vars.errnum |= EPACKCOMPR;
	    n = 0;
	    compression_done = NO;
	  }

#ifdef	USE_ZLIB

	  if(cks && n > 0)
	    crc32sum = crc32(crc32sum, compressbuf + 2, n);

#endif

	  UnsN_to_xref(compressbuf, n, 16);
	  ER__(ofunc(compressbuf, n + 2, params), i);

	  params->vars.sum_compr_filesizes += (Real64) (n + 2);
	} while(n == 0xfff);

	pst = 0;
	if(pid > 0){
	  waitpid_forced(pid, &pst, 0);
	  if( (i = WEXITSTATUS(pst)) ){
	    fprintf(params->errfp,
		T_("Error: Compression program returned bad status %d for `%s'.\n"),
				(int) i, name);

	    params->vars.errnum |= EPACKCOMPR;

	    compression_done = NO;

	    buf[0] = '.';	/* terminate the data stream correctly for */
	    ER__(ofunc(buf, 1, params), i);	/* the attempt to write */
	  }					/* not compressed */
	}

	close(fd);

#ifdef	USE_ZLIB

	close_file_zip(&params->vars.zfile);

	if(cks){
	  Uns32_to_xref(compressbuf, crc32sum);
	  ER__(ofunc(compressbuf, 4, params), i);
	}

#endif

      }
      else{
	if(fd >= 0){
	  close(fd);
	}
	if(pid > 0){
	  kill(pid, SIGTERM);
	  waitpid_forced(pid, &pst, 0);
	}
      }

      if(!compression_done){
	ER__(write_file_uncompressed(name, &statb, params, ftype_attrs), i);
      }
    }

    buf[0] = '.';

    ER__(ofunc(buf, 1, params), i);

    if(!params->dont_keep_atime)
	utime(name, &utim);

    if(params->verbose && params->verbosefunc){
	verbosestr = get_mem(verbosestr, &verbosestr_nall,
			strlen(esc_name) + 2, lverbosestr, 256,
			&verbosestr_all);
	if(!verbosestr)
	  CLEANUPR(-errno);

	sprintf(verbosestr, "%s\n", esc_name);
	params->verbosefunc(verbosestr, params);
    }
  }
#ifdef IS_DOOR
  else if(IS_DOOR(statb)) {
    /* 
     * This is a file special to the OS which will be created on demand.
     * Don't try to archive it, and don't throw any errors/warnings.
     */
    ;
  }  
#endif
  else{
    fprintf(errfp, T_("Error: `%s' is an illegal type of file.\n"), name);
    params->vars.errnum |= EPACKTYPE;

    CLEANUPR(0 /* WARNING */);
  }

 cleanup:
  if(esc_name_all)
    free(esc_name);
  if(verbosestr_all)
    free(verbosestr);
  if(buf_all)
    free(buf);

  return(r);
}

Int32
pack_writeout(UChar * name, AarParams * params, Int16 endofrecord)
{
  Int32		i;
  UChar		buf[30];

  i = 0;

  if(name)
    i = l_writeout(name, params, NO);

  if(endofrecord){
    sprintf(buf, "%d;", ENDOFARCHIVE);
    i = params->outputfunc(buf, strlen(buf), params);
  }

  return(i);
}

Int32
pack_default_output(UChar * buffer, Int32 num, AarParams * params)
{
  return(write_forced(params->outfd, buffer, num));
}

static Int32
write_filecont(
  UChar		*name,
  struct stat	*statb,
  AarParams	*params,
  Flag		compress,
  int		ftype_attrs)
{
  int		fd, pid, pst;
  Int32		i, num_read, n, ret = 0;
  UChar		sbuf[64], mtimestr[50];
  UChar		buf[256];
  UChar		*unzipcmd, *cptr;
  FILE		*errfp;
  Uns32		readerrno;
  Int32		(*ofunc)(UChar *, Int32, AarParams *);
  Int64		(*readfunc)(int, UChar *, Int64);
  int		ftype;
  Flag		cks;
  Uns32		crc32sum;	/* uninitialized OK */

  cks = NO;
  ftype = FILECONTENTS;

#ifdef	USE_ZLIB

  cks = params->check;
  if(params->bi_compr_level > 0)
    cks = NO;		/* built-in compression already does checksumming */

  if(cks){
    ftype = FILECONTENTSCKS;
    crc32sum = crc32(0L, Z_NULL, 0);
  }

#endif  

  ftype |= ftype_attrs;

  unzipcmd = "";
  ofunc = params->outputfunc;
  errfp = params->errfp;
  readfunc = params->readfunc;
  fd = -1;
  pid = -1;
  readerrno = EPACKREAD;

  if(params->zipcmd && params->unzipcmd)
    if(*params->zipcmd && *params->unzipcmd)
      unzipcmd = params->unzipcmd;

  compress = (compress && (unzipcmd[0]
#ifdef	USE_ZLIB
			|| params->bi_compr_level > 0
#endif
			));

  if(compress){
    i = open_for_compress(&fd, name, params->zipcmd, &pid, params);

    readerrno |= EPACKCOMPR;
  }
  else{
    fd = open_for_pack(name, O_RDONLY | O_BINARY);
    i = (fd < 0 ? -1 : 0);
    unzipcmd = "";
  }

#ifdef	USE_ZLIB

  params->vars.zfile.fd = fd;

#endif

  if(i){
    fprintf(errfp, T_("Error: cannot open file `%s'.\n"), name);
    params->vars.errnum |= readerrno;

    return(-ENOENT);
  }

  n = strlen(unzipcmd);

  cptr = "";
  if(compress && params->bi_compr_level > 0){
    if(n > 0){
	n++;
	cptr = ". ";
    }
    else{
      cptr = ".";
    }
    n++;
  }

  sprintf(sbuf, "%d;%s;%d;", ftype,
			(char *) time_t_to_intstr(statb->st_mtime, mtimestr),
			(int) strlen(name));
  i = ofunc(sbuf, strlen(sbuf), params);
  if(!i)
    i = ofunc(name, strlen(name), params);
  sprintf(sbuf, ";%d;%s", (int) n, cptr);
  if(!i)
    i = ofunc(sbuf, strlen(sbuf), params);
  if(!i)
    i = ofunc(unzipcmd, strlen(unzipcmd), params);
  if(!i)
    i = ofunc(";", 1 /* strlen(";") */, params);
  if(i){
    ret = i;
    params->vars.errnum |= EPACKWRITE;
    GETOUT;
  }

  do{
    num_read = 0;
    forever{
      i = read_file(fd, buf + 1 + num_read, 255 - num_read, &params->vars.zfile, readfunc);

      if(i > 0){

#ifdef	USE_ZLIB

	if(cks)
	  crc32sum = crc32(crc32sum, buf + 1 + num_read, i);

#endif

	num_read += i;
      }

      if(i < 0){
	fprintf(params->errfp, T_("Error: Unexpected error reading contents of `%s'.\n"), name);
	params->vars.errnum |= readerrno;
      }

      if(i <= 0)
	break;
    }

    buf[0] = (UChar) num_read;

    /*if(num_read < 255)
	memset(buf + 1 + num_read, 0, sizeof(UChar) * (255 - num_read));*/

    if( (i = ofunc(buf, num_read + 1, params)) ){
      ret = i;
      params->vars.errnum |= EPACKWRITE;
      GETOUT;
    }
  } while(num_read == 255);

  if(cks)
    Uns32_to_xref(buf, crc32sum);

  buf[4] = '.';
  i = (cks ? 0 : 4);
  if( (i = ofunc(buf + i, 1 + 4 - i, params)) ){
    ret = i;
    params->vars.errnum |= EPACKWRITE;
    GETOUT;
  }

 cleanup:
  if(fd >= 0)
    close(fd);

  if(compress && pid >= 0){
    waitpid_forced(pid, &pst, 0);
    ret = WEXITSTATUS(pst);

    if(ret)
	params->vars.errnum |= EPACKCOMPR;
  }

#ifdef	USE_ZLIB

  close_file_zip(&params->vars.zfile);

#endif

  return(ret);

 getout:
  if(pid >= 0)
    kill(pid, SIGTERM);

  CLEANUP;
}

static UChar	*packer_errstrs[] = {
		TN_ ("General error"),
		TN_ ("Error writing packed data stream"),
		TN_ ("Error reading data while packing"),
		TN_ ("Error reading symbolic link while packing"),
		TN_ ("Error compressing data while packing"),
		TN_ ("Unknown filesystem entry type found while packing"),
		TN_ ("Size of file changed while packing it"),
		TN_ ("Error reading command output while packing"),
		TN_ ("Error while packing ACLs"),
		TN_ ("Given filesystem entry not found while packing"),
		NULL,
		NULL,
		NULL,
		NULL,
		NULL,
		NULL,
		TN_ ("Required built-in uncompress not available"),
		TN_ ("Error uncompressing data during extract"),
		TN_ ("Error reading packed data stream during extract"),
		TN_ ("Format error in packed data stream"),
		TN_ ("Error creating base directory during extract"),
		TN_ ("Error setting filesystem entry properties"),
		TN_ ("Error creating filesystem entry during extract"),
		TN_ ("Error setting ACLs during extract"),
		TN_ ("Filesystem entry already exists, must not remove"),
		TN_ ("Error writing data to file during extract"),
		TN_ ("Error writing data to command input"),
		TN_ ("File to write contents to does not exist"),
		TN_ ("Wrong checksum reading packed data stream"),
		TN_ ("No writing permission"),
		TN_ ("ACL entry skipped because of unresolvable numerical ID"),
		NULL,
		};

UChar **
packer_strerrors(UChar ** estrs, Uns32 errnum)
{
  Int32		i, j, n;

  n = sizeof(packer_errstrs) / sizeof(packer_errstrs[0]);

  if(!estrs){
    estrs = NEWP(UChar *, n + 1);
    if(!estrs)
	return(NULL);
  }

  for(i = j = 0; i < n; i++){
    if(errnum & (1 << i)){
	if( (estrs[j] = T_(packer_errstrs[i])) )
	  j++;
    }
  }

  estrs[j] = NULL;

  return(estrs);
}
