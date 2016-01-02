/****************** Start of $RCSfile: crptauth.h,v $  ****************
*
* $Source: /home/alb/afbackup/afbackup-3.5.8.12/RCS/crptauth.h,v $
* $Id: crptauth.h,v 1.3 2004/07/08 20:34:44 alb Exp alb $
* $Date: 2004/07/08 20:34:44 $
* $Author: alb $
*
*
******* description ***********************************************
*
*
*
*******************************************************************/

#ifndef	__CRPTAUTH_H
#define	__CRPTAUTH_H	__CRPTAUTH_H

#include <x_types.h>

#define	AUTH_USE_DES	1
#define	AUTH_NOCONFIRM	(1 << 1)

extern	Int32	set_cryptkey(UChar *, UChar *, Flag);
extern	Int32	make_cryptkey(void *, UChar *, UChar *, Flag);
extern	Uns32	encrpt(UChar *, Flag);
extern	Uns32	encrpt_k(UChar *, Flag, void *);
extern	Int32	logon_to_server(int, int, UChar *, UChar *, UChar **, Uns32);
extern	Int32	authenticate_client(int, int, Flag, UChar *,
						UChar, UChar, Uns32, Int32);
extern	Int32	logon_to_server_k(int, int, UChar *, UChar *,
						UChar **, Uns32, void *);
extern	Int32	authenticate_client_k(int, int, Flag, UChar *,
					UChar, UChar, Uns32, Int32, void *);
extern	Int32	check_cryptfile(UChar *);
extern	UChar	*check_cryptfile_msg(Int32);

extern	Int32	nencrypt(UChar *, Int32, Flag);
extern	Int32	ndecrypt(UChar *, Int32, Flag);
extern	Int32	nencrypt_k(UChar *, Int32, Flag, void *);
extern	Int32	ndecrypt_k(UChar *, Int32, Flag, void *);

extern	Int32	sizeof_cryptkey(Flag);

#endif	/* !defined(__CRPTAUTH_H) */
