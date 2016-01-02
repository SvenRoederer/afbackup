/****************** Start of $RCSfile: __descrpt.c,v $  ****************
*
* $Source: /home/alb/afbackup/afbackup-3.5.8.12/RCS/__descrpt.c,v $
* $Id: __descrpt.c,v 1.3 2011/12/14 20:27:23 alb Exp alb $
* $Date: 2011/12/14 20:27:23 $
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

  static char * fileversion = "$RCSfile: __descrpt.c,v $ $Source: /home/alb/afbackup/afbackup-3.5.8.12/RCS/__descrpt.c,v $ $Id: __descrpt.c,v 1.3 2011/12/14 20:27:23 alb Exp alb $ " PACKAGE " " VERSION_STRING;

#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <genutils.h>
#include <fileutil.h>
#include <x_types.h>
#include <crptauth.h>
#include "cryptkey.h"
#include "afbackup.h"

#include "des_aux.h"

#ifndef XOR
#define XOR(a,b)	(((a) & ~(b)) | (~(a) & (b)))
#endif

#ifndef	ENCRYPT				/* should be defined in des.h */
#define	ENCRYPT			1
#endif
#define	WEAK_ENCRYPTION		(ENCRYPT << 1)

static	UChar	*initstring = "Albi's encryption initialization string";

static	des_key_schedule	accesskeys[2];

static Int32
set_crptkey(UChar * filename)
{
  struct stat	statb;
  Int32		i, fd, ret = 0;
  Uns32		factor, prevval, newval;
  UChar		buf[100], *kptr, *endptr, *bufptr;
  des_cblock	rawkeys[2];

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
    strncpy(buf, ACCESSKEYSTRING, 99);
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

  if(des_set_key(rawkeys, accesskeys[0])
			|| des_set_key(rawkeys + 1, accesskeys[1]))
    ret = -1;

  return(ret);
}

static Uns32
crpt(UChar * outbuf, UChar * buf, Int32 len, Int8 flags, Int8 in)
{
  static Int8		initialized = 0;
  static des_cblock	ivec[2];

  UChar		mask, *cptr;
  Int32		i;
  Int8		encrypt, weak;

  encrypt = (flags & ENCRYPT) ? 1 : 0;
  weak = (flags & WEAK_ENCRYPTION) ? 1 : 0;

  if(in)
    initialized = 0;

  if(! initialized){
    cptr = (UChar *) ivec;
    mask = 0xdf;

    for(i = 0; i < sizeof(des_cblock) * 2; i++, cptr++){
      *cptr = XOR(mask, initstring[i]);
      mask = XOR(mask, initstring[i]);
    }

    initialized = 1;
  }

  if(weak)
    des_cbc_encrypt((des_cblock *) buf, (des_cblock *) outbuf,
			len * sizeof(UChar),
			accesskeys[0], ivec, encrypt);
  else
#ifndef	LIBDESBUG_COMPATIBILITY
    des_ede2_cbc_encrypt((des_cblock *) buf, (des_cblock *) outbuf,
			len * sizeof(UChar),
			accesskeys[0], accesskeys[1], ivec, encrypt);
#else
    des_3cbc_encrypt((des_cblock *) buf, (des_cblock *) outbuf,
			len * sizeof(UChar),
			accesskeys[0], accesskeys[1],
			ivec, ivec + 1, encrypt);
#endif
}

void
usage(UChar * pname)
{
  fprintf(stderr, "Usage: %s [ -dw ] [ -k <cryptkeyfile> ]\n",
		FN_BASENAME(pname));
  exit(2);
}

main(int argc, char ** argv)
{
  UChar		ibuf[256], obuf[256];
  UChar		decrypt = 0, encrypt = 0, *cryptfile = NULL, weak = 0;
  Int32		i, nsh = 4, n;

  i = goptions(-argc, (UChar **) argv, "b:e;b:d;s:k;b:w",
			&encrypt, &decrypt, &cryptfile, &weak);
  if(i || (encrypt && decrypt))
    usage(argv[0]);

  if(weak)
    weak = WEAK_ENCRYPTION;

  set_crptkey(cryptfile);

  if(! decrypt){
    while((i = read_forced(0, ibuf + 1, 255)) > 0){
      ibuf[0] = (UChar) i;

      n = ((((i + 1) - 1) >> nsh) + 1) << nsh;

      crpt(obuf, ibuf, n, ENCRYPT | weak, 0);

      write_forced(1, obuf, n);
    }
  }
  else{
    while((i = read_forced(0, ibuf, 256)) > 0){
      n = (((i - 1) >> nsh) + 1) << nsh;

      crpt(obuf, ibuf, n, weak, 0);

      write_forced(1, obuf + 1, obuf[0]);
    }
  }

  exit(0);
}
