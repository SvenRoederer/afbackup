/****************** Start of $RCSfile: opensslx.c,v $  ****************
*
* $Source: /home/alb/afbackup/afbackup-3.5.8.12/RCS/opensslx.c,v $
* $Id: opensslx.c,v 1.3 2013/08/04 15:43:04 alb Exp alb $
* $Date: 2013/08/04 15:43:04 $
* $Author: alb $
*
*
******* description ***********************************************
*
*
*
*******************************************************************/

#include "conf.h"

#ifdef	HAVE_OPENSSL_SSL_H

#include <errno.h>
#include <unistd.h>
#include <sys/types.h>
#include <poll.h>

#include <fileutil.h>
#include <opensslx.h>


int
SSL_connect_forced_with_timeout(SSL *ssl, int timeout)
{
  int		e, f;
  time_t	starttime, curtime;

  starttime = time(NULL);

  forever{
    f = SSL_connect(ssl);
    if(f > 0)
	break;

    e = SSL_get_error(ssl, f);
    if(e == SSL_ERROR_WANT_READ || e == SSL_ERROR_WANT_WRITE){
	curtime = time(NULL);
	if(curtime - starttime > timeout && timeout > 0)
	  break;

	f = SSL_wait_io(ssl, e, timeout > 0 ? timeout - (curtime - starttime) : 1);
	if(f <= 0)
	  break;
    }
    else
	break;
  }

  return(f);
}

int
SSL_accept_forced_with_timeout(SSL *ssl, int timeout)
{
  int		e, f;
  time_t	starttime, curtime;

  starttime = time(NULL);

  forever{
    f = SSL_accept(ssl);
    if(f > 0)
	break;

    e = SSL_get_error(ssl, f);
    if(e == SSL_ERROR_WANT_READ || e == SSL_ERROR_WANT_WRITE){
	curtime = time(NULL);
	if(curtime - starttime > timeout && timeout > 0)
	  break;

	f = SSL_wait_io(ssl, e, timeout > 0 ? timeout - (curtime - starttime) : 1);
	if(f <= 0)
	  break;
    }
    else
	break;
  }

  return(f);
}

int
SSL_read_forced_with_timeout(SSL *ssl, char *buf, int num, int timeout)
{
  int		fd, nread = 0, e, n, s;
  time_t	endtime, curtime;
  Flag		tried_reconnect = NO;

  fd = SSL_get_fd(ssl);

  endtime = time(NULL) + timeout;

  while(nread < num){
    n = SSL_read(ssl, buf, s = num - nread);
    if(n <= 0){
	e = SSL_get_error(ssl, n);
	if(n == 0 && e == SSL_ERROR_ZERO_RETURN)
	  break;

	if(e != SSL_ERROR_WANT_READ && e != SSL_ERROR_WANT_WRITE){
	  if(tried_reconnect)
	    break;

	  e = SSL_connect_forced_with_timeout(ssl,
			timeout > 0 ? endtime - time(NULL) : timeout);
	  tried_reconnect = YES;
	  continue;
	}

	curtime = time(NULL);
	if(curtime > endtime && timeout > 0)
	  break;

	SSL_wait_io(ssl, e, timeout > 0 ? endtime - curtime : 1);
	continue;
    }

    tried_reconnect = NO;	/* reading succeeded, so reset flag */

    nread += n;
    buf += n;
  }

  return(nread);
}

int
SSL_write_forced_with_timeout(SSL * ssl, char * buf, int num, int timeout)
{
  int		fd, nwr = 0, e, n, s;
  time_t	endtime, curtime;
  Flag		tried_reconnect = NO;

  fd = SSL_get_fd(ssl);

  endtime = time(NULL) + timeout;

  while( (n = num - nwr) > 0){
    if(n > SSL_MTU_SIZE)
	n = SSL_MTU_SIZE;

    n = SSL_write(ssl, buf, s = n);
    if(n <= 0){
	e = SSL_get_error(ssl, n);
	if(e != SSL_ERROR_WANT_WRITE && e != SSL_ERROR_WANT_READ){
	  if(tried_reconnect)
	    break;

	  e = SSL_connect_forced_with_timeout(ssl,
			timeout > 0 ? endtime - time(NULL) : timeout);
	  tried_reconnect = YES;
	  continue;
	}

	curtime = time(NULL);
	if(curtime > endtime && timeout > 0)
	  break;

	SSL_wait_io(ssl, e, timeout > 0 ? endtime - curtime : 1);
	continue;
    }

    tried_reconnect = NO;	/* writing succeeded, so reset flag */

    nwr += n;
    buf += n;
  }

  return(nwr);
}

int
SSL_wait_io(SSL * ssl, int err, int timeout)
{
  int	fd;

  if(timeout < 0)
    timeout = 0;

  fd = SSL_get_fd(ssl);
  if(fd < 0){
    errno = EINVAL;
    return(-1);
  }

  if(err == SSL_ERROR_WANT_READ)
    return(wait_for_input(fd, timeout));

  if(err == SSL_ERROR_WANT_WRITE)
    return(wait_until_writable(fd, timeout));

  errno = EINVAL;
  return(-1);
}

static UChar	*rndcmds[] = { "ls -l /tmp /var/tmp; ls -l /proc",
			"netstat -an", "ps -e ; ps xa",
			"date ; echo $$ ; ls -lutr /dev", NULL, };

void
init_ssl_randdb(){
  UChar		*cmdoutput, **cpptr = NULL;
  Int32		n;

  if(!access("/dev/random", R_OK))	/* if /dev/random is available it's used */
    return;

  while(!RAND_status()){
    if(!cpptr)
	cpptr = rndcmds;
    if(!*cpptr)
	cpptr = rndcmds;

    cmdoutput = read_command_output(*cpptr, 1 + 2, NULL);
    if(!cmdoutput){
	sleep(2);	/* try again later */
	continue;
    }

    n = strlen(cmdoutput);
    RAND_add(cmdoutput, n, 32.0);

    free(cmdoutput);

    cpptr++;
  }
}

#endif	/* defined(HAVE_OPENSSL_SSL_H) */
