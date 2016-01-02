/****************** Start of $RCSfile: crptauth.c,v $  ****************
*
* $Source: /home/alb/afbackup/afbackup-3.5.8.12/RCS/crptauth.c,v $
* $Id: crptauth.c,v 1.4 2008/11/17 20:09:39 alb Exp alb $
* $Date: 2008/11/17 20:09:39 $
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

  static char * fileversion = "$RCSfile: crptauth.c,v $ $Source: /home/alb/afbackup/afbackup-3.5.8.12/RCS/crptauth.c,v $ $Id: crptauth.c,v 1.4 2008/11/17 20:09:39 alb Exp alb $ " PACKAGE " " VERSION_STRING;

#include <unistd.h>
#include <fcntl.h>
#include <math.h>
#include <errno.h>
#include <sys/types.h>
#ifdef	HAVE_SYS_TIME_H
#include <sys/time.h>
#endif
#include <sys/stat.h>
#include <genutils.h>
#include <fileutil.h>
#include <x_types.h>
#include <x_errno.h>
#include <crptauth.h>

#ifdef	HAVE_DES_ENCRYPTION
#include <des_aux.h>
#endif

#ifndef XOR
#define XOR(a,b)	(((a) & ~(b)) | (~(a) & (b)))
#endif

#ifndef	ETIMEDOUT
#define	ETIMEDOUT	EAGAIN
#endif

#define	LOGON_TIMEOUT	90


Int32
logon_to_server_k(
  int	fromfd,
  int	tofd,
  UChar	*greeting_message,
  UChar	*version_msg_key,
  UChar	**versionstr,
  Uns32	flags,
  void	*key)
{
  UChar		cbuf[30], buf[1000], *cptr;
  Int32		i;
  Flag		usedes, confirm;

  usedes = (flags & AUTH_USE_DES ? YES : NO);
  confirm = (flags & AUTH_NOCONFIRM ? NO : YES);

  memset(buf, 0, 1000 * sizeof(UChar));

  if(greeting_message){
    for(i = 0; i < 1000; i++){
      if(wait_for_input(fromfd, LOGON_TIMEOUT) < 1)
	return(NO_GREETING_MESSAGE);

      if(read_forced(fromfd, buf + i, 1) != 1){
	return(NO_GREETING_MESSAGE);
      }

      if(strstr(buf, greeting_message))
	break;
    }
    if(i >= 1000)
	return(NO_GREETING_MESSAGE);
  }

  if(versionstr && version_msg_key){
    *versionstr = NULL;

    cptr = strstr(buf, version_msg_key);
    if(cptr){
	cptr += strlen(version_msg_key);
	*versionstr = strword(cptr, 0);
    }
  }

  if(usedes){
#ifdef	HAVE_DES_ENCRYPTION
    read_forced(fromfd, cbuf, 16);

    encrpt_k(cbuf, usedes, key);

    write_forced(tofd, cbuf, 16);
#else

    fprintf(stderr, T_("Error: DES encryption not available.\n"));

    return( (errno = EINVAL) );
#endif
  }
  else{
    read_forced(fromfd, cbuf, 4);

    encrpt_k(cbuf, usedes, key);

    write_forced(tofd, cbuf, 4);
  }

  if(confirm){
    read_forced(fromfd, cbuf, 1);

    return((Int32) cbuf[0]);
  }

  return(0);
}

Int32
authenticate_client_k(
  int		tofd,
  int		fromfd,
  Flag		prot_only,
  UChar		*initmsg,
  UChar 	fresult,
  UChar		errb,
  Uns32		flags,
  Int32		timeout_sec,
  void		*key)
{
  UChar		*cptr, buf[100], c = 0;
  Int32		i;
  Uns32		code;
  Flag		auth_ok, usedes, confirm;

  usedes = (flags & AUTH_USE_DES ? YES : NO);
  confirm = (flags & AUTH_NOCONFIRM ? NO : YES);

  cptr = initmsg;
  i = strlen(cptr);

  write_forced(tofd, cptr, i);

  if(usedes){
#ifdef	HAVE_DES_ENCRYPTION
    for(i = 0; i < 4; i++){
      code = (Uns32) (drandom() * 4294967296.0);
      memcpy(buf + 4 * i, &code, sizeof(Uns32));
    }
    write_forced(tofd, buf, 16);

    encrpt_k(buf, usedes, key);

    if(read_with_timeout(fromfd, buf + 16, 16, timeout_sec) < 0)
	return(errno = ETIMEDOUT);

    auth_ok = ! memcmp(buf, buf + 16, 16);
#else
    fprintf(stderr, T_("Error: DES encryption not available.\n"));

    return( (errno = EINVAL) );
#endif
  }
  else{
    code = (Uns32) (drandom() * 4294967296.0);

    Uns32_to_xref(buf, code);

    encrpt_k(buf, usedes, key);

    write_forced(tofd, buf, 4);

    encrpt_k(buf, usedes, key);

    if(read_with_timeout(fromfd, buf + 4, 4, timeout_sec) < 0)
	return(errno = ETIMEDOUT);

    auth_ok = ! memcmp(buf, buf + 4, 4);
  }

  if(!auth_ok && !prot_only)
    c = errb;

  if(fresult)
    c = fresult;

  if(confirm)
    write_forced(tofd, &c, 1);

  return((Int32) c);
}

Int32
check_cryptfile(UChar * filename)
{
  Int32		i;
  struct stat	statb;

  if(!filename)
    return(2);

  i = eaccess(filename, R_OK);
  if(i)
    return(3);

  i = stat(filename, &statb);
  if(i)
    return(4);

  if(statb.st_mode & 0044)
    return(-1);

  if(statb.st_size < 5)
    return(1);

  return(0);
}

static UChar	*crmsg_warnings[] = {
		TN_ ("Key in file is too short"),
		TN_ ("No filename supplied"),
		TN_ ("File not readable"),
		TN_ ("Cannot get file information"),
		};
static UChar	*crmsg_errors[] = {
		TN_ ("File is group/world readable"),
		};

UChar *
check_cryptfile_msg(Int32 errn)
{
  if(errn > 0)
    return(T_(crmsg_warnings[errn - 1]));
  if(errn < 0)
    return(T_(crmsg_errors[- errn - 1]));

  return(NULL);
}


#ifdef	HAVE_DES_ENCRYPTION

static	UChar	*initstring = "Albi's encryption initialization string";

static	des_key_schedule	accesskeys[2];

#else

static	void			*accesskeys;

#endif

static Int32
make_cryptkey_des(void * tgt, UChar * filename, UChar * keystr)
{
#ifdef	HAVE_DES_ENCRYPTION
  struct stat	statb;
  Int32		i, fd, ret = 0;
  Uns32		factor, prevval, newval;
  UChar		buf[100], *kptr, *endptr, *bufptr;
  des_cblock	rawkeys[2];
  des_key_schedule	keys[2];

  buf[0] = '\0';

  if(filename){
    i = stat(filename, &statb);
    if(i)
      ret = ENOENT;
    else{
      fd = open(filename, O_RDONLY);
      if(fd < 0){
	ret = EACCES;
      }
      else{
	i = read(fd, buf, 99);
	close(fd);

	if(i < 0)
	  i = 0;
	buf[i] = '\0';
      }
    }
  }

  if(ret || !filename)
    strncpy(buf, keystr, 99);
  if(!buf[0])
    memset(buf, 0x5a, 99);
  buf[99] = '\0';

  memset(rawkeys, 0x5a, 2 * sizeof(des_cblock));
  kptr = (UChar *) &(rawkeys[0]);
  endptr = kptr + 2 * sizeof(des_cblock);
  bufptr = buf + 1;

  prevval = buf[0] % 0x40;
  factor = 0x40;
  forever{
    if(! *bufptr)
	break;

    newval = prevval + factor * ((*bufptr) % 0x40);
    factor *= 0x40;

    if((newval & 0xff) == (prevval & 0xff) && factor >= 0x0100){
	*kptr = (prevval & 0xff);
	kptr++;
	if(kptr >= endptr)
	  break;

	factor /= 0x0100;
	newval /= 0x0100;
    }

    prevval = newval;
    bufptr++;
  }
  while(kptr < endptr && prevval){
    *(kptr++) = prevval & 0xff;
    prevval /= 0x0100;
  }

  if(des_set_key(rawkeys, keys[0])
			|| des_set_key(rawkeys + 1, keys[1]))
    ret = -1;

  memcpy(tgt, keys, 2 * sizeof(des_key_schedule));

  return(ret);

#else

  fprintf(stderr, T_("Error: DES encryption not available.\n"));

  return( (errno = EINVAL) );

#endif	/* defined(HAVE_DES_ENCRYPTION) */
}

static Int32
set_cryptkey_des(UChar * filename, UChar * keystr)
{
  return(make_cryptkey_des(accesskeys, filename, keystr));
}

static Uns32
encrpt_des_k(UChar * buf, void * key)
{
#ifdef	HAVE_DES_ENCRYPTION
  UChar		lbuf[20], mask, *cptr;
  des_cblock	ivec[2];
  Int32		i;
  des_key_schedule	*keys;

  keys = (des_key_schedule *) key;
  cptr = (UChar *) ivec;
  mask = 0xdf;

  for(i = 0; i < sizeof(des_cblock) * 2; i++, cptr++){
    *cptr = XOR(mask, initstring[i]);
    mask = XOR(mask, initstring[i]);
  }

#ifndef	LIBDESBUG_COMPATIBILITY
  des_ede2_cbc_encrypt((des_cblock *) buf, (des_cblock *) lbuf,
			16 * sizeof(UChar),
			keys[0], keys[1], ivec, 1);
#else
  des_3cbc_encrypt((des_cblock *) buf, (des_cblock *) lbuf,
			16 * sizeof(UChar),
			keys[0], keys[1], ivec, ivec + 1, 1);
#endif

  memcpy(buf, lbuf, 16 * sizeof(UChar));

  return(0);

#else

  fprintf(stderr, T_("Error: DES encryption not available.\n"));

  return( (errno = EINVAL) );

#endif	/* defined(HAVE_DES_ENCRYPTION) */
}

static Uns32
encrpt_des(UChar * buf)
{
  return(encrpt_des_k(buf, accesskeys));
}

static Int32
cryptn_des_k(UChar * buf, Int32 len, Flag en, void * key)
{
#ifdef	HAVE_DES_ENCRYPTION
  UChar		sbuf[4096], *tbuf = NULL, *cbuf, mask, *cptr;
  des_cblock	ivec[2];
  Int32		i, l;
  des_key_schedule	*keys;

  keys = (des_key_schedule *) key;

  cbuf = sbuf;
  l = (((len - 1) >> 4) + 1) << 4;

  if(l > 4096){
    tbuf = NEWP(UChar, l);
    if(!tbuf)
	return(-1);

    cbuf = tbuf;
  }

  cptr = (UChar *) ivec;
  mask = 0xdf;

  for(i = 0; i < sizeof(des_cblock) * 2; i++, cptr++){
    *cptr = XOR(mask, initstring[i]);
    mask = XOR(mask, initstring[i]);
  }

#ifndef	LIBDESBUG_COMPATIBILITY
  des_ede2_cbc_encrypt((des_cblock *) buf, (des_cblock *) cbuf,
			l * sizeof(UChar),
			keys[0], keys[1],
			ivec, en ? 1 : 0);
#else
  des_3cbc_encrypt((des_cblock *) buf, (des_cblock *) cbuf,
			l * sizeof(UChar),
			keys[0], keys[1],
			ivec, ivec + 1, en ? 1 : 0);
#endif

  memcpy(buf, cbuf, l * sizeof(UChar));

  if(tbuf)
    free(tbuf);

  return(0);


#else

  fprintf(stderr, T_("Error: DES encryption not available.\n"));

  return( (errno = EINVAL) );

#endif
}

static Int32
cryptn_des(UChar * buf, Int32 len, Flag en)
{
  return(cryptn_des_k(buf, len, en, accesskeys));
}

#define	PI_1_8		(M_PI / 8.0)
#define	PI_3_8		(M_PI / 8.0 * 3.0)
#define	S_PI_1_8	sin(PI_1_8)
#define	S_PI_3_8	sin(PI_3_8)

#define	LOG_8		log(8.0)
#define	LOG_12		log(12.0)

static Uns32	accesskey = 0;

static Int32
make_cryptkey_simple(void * tgt, UChar * filename, UChar * accesskeystr)
{
  struct stat	statb;
  Int32		i, fd;
  Uns32		n;
  UChar		buf[10];

  if(!accesskeystr)
    return(EINVAL);

  if(filename){
    i = stat(filename, &statb);
    if(i)
	return(ENOENT);

    fd = open(filename, O_RDONLY);
    if(fd < 0)
	return(EACCES);

    i = read(fd, buf, 5);
    close(fd);

    if(i < 5)
	return(EIO);
  }
  else
    memcpy(buf, accesskeystr, 5);

  n = 0;
  for(i = 0; i < 5; i++)
    n = (n * 96 + ((buf[i] & 0x7f) - 32)) & 0xffffffff;

  memcpy(tgt, &n, sizeof(n));

  return(0);
}

static Int32
set_cryptkey_simple(UChar * filename, UChar * accesskeystr)
{
  return(make_cryptkey_simple(&accesskey, filename, accesskeystr));
}

static Uns32
encrpt_simple_k(UChar * inptr, Uns32 key)
{
  Uns32		inp, a, b, in;

  xref_to_Uns32(&in, inptr);

  inp = XOR(in, key);

  a = inp >> 5;
  a &= (1 << 13) - 1;

  a = (Uns32) ((sin((double)a / 8192.0 * (PI_3_8 - PI_1_8) + PI_1_8)
			- S_PI_1_8) / (S_PI_3_8 - S_PI_1_8) * 8192.0);

  b = a;

  a = inp & ((1 << 5) - 1);
  a = a * a * a;

  a &= ((1 << 5) - 1);

  b |= a << 13;

  a = (inp >> 18);
  a &= (1 << 9) - 1;

  a = (Uns32) ((log((double)a / 512.0 * 4.0 + 8.0) - LOG_8)
			/ (LOG_12 - LOG_8) * 512.0);

  b |= a << 23;

  a = (inp >> 27);
  a &= (1 << 5) - 1;

  a = (Uns32) ((4.0 - sqrt((double)a / 32.0 * 7.0 + 9.0)) * 32.0);

  b |= a << 18;

  Uns32_to_xref(inptr, b);

  return(b);
}

static Uns32
encrpt_simple(UChar * inptr)
{
  return(encrpt_simple_k(inptr, accesskey));
}

static Int32
cryptn_simple_k(UChar * buf, Int32 n, Uns32 key)
{
  for(; n > 0; buf += 4, n -= 4)
    encrpt_simple_k(buf, key);
   
  return(0); 
}

static Int32
cryptn_simple(UChar * buf, Int32 n)
{
  return(cryptn_simple_k(buf, n, accesskey)); 
}

Int32
nencrypt(UChar * buf, Int32 n, Flag usedes)
{
  if(usedes)
    return(cryptn_des(buf, n, YES));

  return(cryptn_simple(buf, n));
}

Int32
ndecrypt(UChar * buf, Int32 n, Flag usedes)
{
  if(usedes)
    return(cryptn_des(buf, n, NO));

  fprintf(stderr, T_("Error: not yet implemented.\n"));
  return(1);
}

Int32
nencrypt_k(UChar * buf, Int32 n, Flag usedes, void * key)
{
  if(usedes)
    return(cryptn_des_k(buf, n, YES, key));

  return(cryptn_simple_k(buf, n, *((UChar *) key)));
}

Int32
ndecrypt_k(UChar * buf, Int32 n, Flag usedes, void * key)
{
  if(usedes)
    return(cryptn_des_k(buf, n, NO, key));

  fprintf(stderr, T_("Error: not yet implemented.\n"));
  return(1);
}

Int32
set_cryptkey(UChar * filename, UChar * defkey, Flag usedes)
{
  if(usedes)
    return(set_cryptkey_des(filename, defkey));

  return(set_cryptkey_simple(filename, defkey));
}

Int32
make_cryptkey(void * tgt, UChar * filename, UChar * defkey, Flag usedes)
{
  if(usedes)
    return(make_cryptkey_des(tgt, filename, defkey));

  return(make_cryptkey_simple(tgt, filename, defkey));
}

Uns32
encrpt(UChar * data, Flag usedes)
{
  if(usedes)
    return(encrpt_des(data));

  return(encrpt_simple(data));
}

Uns32
encrpt_k(UChar * data, Flag usedes, void * key)
{
  if(usedes)
    return(encrpt_des_k(data, key));

  return(encrpt_simple_k(data, *((Uns32 *) key)));
}

Int32
sizeof_cryptkey(Flag usedes)
{
  if(usedes)

#ifdef	HAVE_DES_ENCRYPTION

    return(sizeof(des_key_schedule) * 2);

#else

    return(0);

#endif

  return(sizeof(Uns32));
}


Int32
logon_to_server(
  int	fromfd,
  int	tofd,
  UChar	*greeting_message,
  UChar	*version_msg_key,
  UChar	**versionstr,
  Uns32	flags)
{
  return(logon_to_server_k(fromfd, tofd, greeting_message, version_msg_key,
		versionstr, flags,
		((flags & AUTH_USE_DES) ? accesskeys : (void *)(&accesskey))));
}

Int32
authenticate_client(
  int	tofd,
  int	fromfd,
  Flag	prot_only,
  UChar	*initmsg,
  UChar fresult,
  UChar	errb,
  Uns32	flags,
  Int32	timeout_sec)
{
  return(authenticate_client_k(tofd, fromfd, prot_only, initmsg, fresult,
		errb, flags, timeout_sec,
		((flags & AUTH_USE_DES) ? accesskeys : (void *)(&accesskey))));
}
