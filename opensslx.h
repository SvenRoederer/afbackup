/****************** Start of $RCSfile: opensslx.h,v $  ****************
*
* $Source: /home/alb/afbackup/afbackup-3.5.8.12/RCS/opensslx.h,v $
* $Id: opensslx.h,v 1.4 2013/08/04 16:18:06 alb Exp alb $
* $Date: 2013/08/04 16:18:06 $
* $Author: alb $
*
*
******* description ***********************************************
*
*
*
*******************************************************************/

#ifndef	__OPENSSLX_H
#define	__OPENSSLX_H	__OPENSSLX_H

#include <lconf.h>

#ifdef	INC_OPENSSL_SSL_H

#include <openssl/ssl.h>
#include <stdio.h>
#include <x_types.h>
#include <genutils.h>

#define	SSL_MTU_SIZE	1344

extern	int	SSL_read_forced_with_timeout(SSL *, char *, int, int);
extern	int	SSL_write_forced_with_timeout(SSL *, char *, int, int);
#define	SSL_write_forced(ssl, buf, n)	SSL_write_forced_with_timeout(ssl, buf, n, 0)
#define	SSL_read_forced(ssl, buf, n)	SSL_read_forced_with_timeout(ssl, buf, n, 0)
extern	int	SSL_connect_forced_with_timeout(SSL *, int);
extern	int	SSL_accept_forced_with_timeout(SSL *, int);
extern	int	SSL_wait_io(SSL *, int, int);
extern	void	init_ssl_randdb();

#endif	/* defined(INC_OPENSSL_SSL_H) */

#endif	/* defined(__OPENSSLX_H) */
