/****************** Start of $RCSfile: mserver.c,v $  ****************
*
* $Source: /home/alb/afbackup/afbackup-3.5.8.12/RCS/mserver.c,v $
* $Id: mserver.c,v 1.10 2011/12/14 20:27:22 alb Exp alb $
* $Date: 2011/12/14 20:27:22 $
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

  static char * fileversion = "$RCSfile: mserver.c,v $ $Source: /home/alb/afbackup/afbackup-3.5.8.12/RCS/mserver.c,v $ $Id: mserver.c,v 1.10 2011/12/14 20:27:22 alb Exp alb $ " PACKAGE " " VERSION_STRING;

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdarg.h>
#include <sys/types.h>
#include <signal.h>
#include <fcntl.h>
#ifdef  HAVE_SYS_WAIT_H
#include <sys/wait.h>
#endif
#include <sys/socket.h>
#include <netinet/in.h>
#include <syslog.h>
#include <x_types.h>
#include <netutils.h>
#include <genutils.h>
#include <mvals.h>
#include <fileutil.h>

#include "cryptkey.h"
#include "prot.h"
#include "afbackup.h"
#include "server.h"

#define	MPX_CHUNKSIZE	1000

#define	ER__(cmd, lerrfl)	{ if( (lerrfl = cmd) ) return(lerrfl); }
#define	EM__(nmem)	{ nomemfatal(nmem); }
#define	EEM__(st)	{ if(st) nomemfatal(NULL); }
#define	ENM__		nomemfatal(NULL)

typedef	struct __conn_stat__ {
  Int32		status;
  Int32		tape_status;
  UChar		*inbuf;
  UChar		*outbuf;
  Int32		commbufsiz;
  UChar		*tapebuf;
  Int32		tapebufsiz;
  Int32		tapebufptr;
  Int32		headerlen;
  Uns32		inbuf_allocated;
  Uns32		outbuf_allocated;
  UChar		*peername;
  UChar		*clientid;
  struct sockaddr	*peeraddr;
  int		fd;
  Int32		req_cartset;
  Int32		req_cart;
  Int32		req_filenum;
  Uns32		pending_cmd;
  Flag		pending;
  Flag		newfilerequested;
  Uns8		req_streamermode;
} ConnStatus;

#define	CONNSTAT_UNINITIALIZED	0
#define	CONNSTAT_WAIT_AUTH	1
#define	CONNSTAT_AUTHENTICATED	2
#define	CONNSTAT_CONNECTED	3

#define	PROC_ERROR_MASK		(0xff << 16)
#define	PROC_ERROR(num)		((num << 16) & PROC_ERROR_MASK)
#define	PROC_ERRVAL(val)	((val & PROC_ERROR_MASK) >> 16)
#define	PROC_DEFAULT_PROC	(1 << 24)
#define	PROC_POST_PROC		(PROC_DEFAULT_PROC << 1)
#define	PROC_FINISH_CONN	(PROC_DEFAULT_PROC << 2)
#define	PROC_PROC_MASK		(PROC_DEFAULT_PROC | PROC_POST_PROC)

Int32		server_status = NOT_OPEN;

ConnStatus	*connections = NULL;
AFBProtocol	**prot_items;

UChar		*backuphome = NULL;

UChar		*default_configfilenames[] = {	\
				DEFAULT_SERVER_CONFIGFILES, NULL, NULL };
UChar		*configfilename = NULL;

UChar		*loggingfile = NULL;
UChar		*syslog_ident = NULL;

UChar		*cryptfile = NULL;

Flag		edebug = NO;
Flag		unsecure = NO;
Flag		daemonize = NO;
Flag		nobuffering = NO;

UChar		*progname = NULL;
char		**sargv;
char		*sprog;

Int32		cartset = 1;
Int32		cartnum = 0;
Int32		filenum = 0;
Int32		wrcartnum = 0;
Int32		wrfilenum = 0;
Int32		rdcartnum = 0;
Int32		rdfilenum = 0;
Int32		numcarts = 0;

Int32		numwrclients = 0;
Int32		numrdclients = 0;

Flag		tapepos_valid = NO;
Flag		wrtapepos_valid = NO;
Flag		rdtapepos_valid = NO;

Flag		reading_in_sync = NO;
Flag		raw_mode = NO;

UChar		*tapebuf = NULL;
Int32		tapebuf_allocated = 0;
Int32		commbufsiz = DEFCOMMBUFSIZ;
Int32		tapebufptr = 0;
Int32		tapeblocksize = 0;
Flag		newfilerequested = NO;

int		tofd = -1;
int		fromfd = -1;
int		pid = -1;
int		pst;
int		lfd = 2;

Flag		interrupted = NO;

UChar		*vardir = NULL;
UChar		*bindir = NULL;
UChar		*libdir = NULL;
UChar		*confdir = NULL;

UChar		*pidfile = NULL;
Flag		daemonized = NO;

ReplSpec	replacements[] = {
	{ "%B",	NULL, &bindir },
	{ "%L",	NULL, &libdir },
	{ "%V",	NULL, &vardir },
	{ "%C", NULL, &confdir },
	};

typedef struct __ClientPos__ {
  UChar		*clientname;
  Int32		cartnum;
  Int32		filenum;
} ClientPos;

ClientPos	*prev_client_conns = NULL;


ParamFileEntry	entries[] = {
	{ &cryptfile, NULL,
	(UChar *) "^[ \t]*\\([Ee]n\\)?[Cc]rypti?o?n?[-_ \t]*[Kk]ey[-_ \t]*[Ff]iles?:?[ \t]*",
		TypeUCharPTR	},
	{ &loggingfile, NULL,
	(UChar *) "^[ \t]*[Ll]ogg?i?n?g?[-_ \t]*[Ff]ile:?[ \t]*",
		TypeUCharPTR	},
	{ &vardir, NULL,
	(UChar *) "^[ \t]*[Vv][Aa][Rr][-_ \t]*[Dd]ire?c?t?o?r?y?:?[ \t]*",
		TypeUCharPTR	},
};

#define	repl_dirs(string)	repl_substrings((string), replacements,	\
		sizeof(replacements) / sizeof(replacements[0]))

static Int32	handle_prot_func(int, void *, Int32, void *,
					TcpMuxCallbDoneActions *, void *);

static void
do_exit(int est)
{
  if(pid > 0){
    kill(pid, SIGTERM);

    if(tofd >= 0)
	close(tofd);
    if(fromfd >= 0)
	close(fromfd);

    waitpid(pid, &pst, 0);

    est = WEXITSTATUS(pst);
  }

  if(pidfile)
    unlink(pidfile);

  exit(est);
}

static void
logmsg(int prio, UChar * fmt, ...)
{
  va_list	args;
  FILE		*fp, *lfp = NULL;

  if(loggingfile)
    lfp = fopen(loggingfile, "a");

  fp = (lfp ? lfp : stderr);

  va_start(args, fmt);
  fprintf(fp, "%s, ", actimestr());
  vfprintf(fp, fmt, args);
  fflush(fp);
  va_end(args);

  if(lfp)
    fclose(lfp);

  if(lfp && daemonize){
    va_start(args, fmt);
    vfprintf(stderr, fmt, args);
    fflush(stderr);
    va_end(args);
  }

  if(syslog_ident){
    va_start(args, fmt);
    gvsyslog(syslog_ident, LOG_CONS | LOG_PID, LOG_DAEMON, prio, fmt, args);
    va_end(args);
  }
}

static void
fatal(UChar * msg)
{
  logmsg(LOG_CRIT, "%s", msg);

  do_exit(1);
}

static void
nomemfatal(void * ptr)
{
  if(!ptr)
    fatal("Error: Cannot allocate memory.\n");
}

#define	request_new_file(fromfd, tofd, tcpmux_status)	\
		simple_command(REQUESTNEWFILE, fromfd, tofd, tcpmux_status)

static Flag
need_new_tapefile_for_wrclient(UChar * name, Int32 cartn, Int32 filen)
{
  Flag		needit = NO;
  Int32		i;
  Int32		idx;	/* uninitialized OK */

  if(!prev_client_conns){
    EM__(prev_client_conns = NEWP(ClientPos, 2));

    idx = 0;
    SETZERO(prev_client_conns[1]);
  }
  else{
    for(i = 0; prev_client_conns[i].clientname; i++){
	if(!strcmp(prev_client_conns[i].clientname, name)){
	  needit = YES;
	  idx = i;
	}
    }

    if(!needit){
	EM__(prev_client_conns = RENEWP(prev_client_conns, ClientPos, i + 2));
	idx = i;
	SETZERO(prev_client_conns[idx + 1]);
    }
  }

  if(needit){
    if(filen != prev_client_conns[idx].filenum
			|| cartn != prev_client_conns[idx].cartnum)
	needit = NO;
  }
  else{
    EM__(prev_client_conns[idx].clientname = strdup(name));
  }

  prev_client_conns[idx].cartnum = cartn;
  prev_client_conns[idx].filenum = filen;

  return(needit);
}

static void
free_prev_wrclient_list()
{
  ClientPos	*poss;

  if(prev_client_conns){
    for(poss = prev_client_conns; poss->clientname; poss++)
	free(poss->clientname);
    ZFREE(prev_client_conns);
  }
}

static void
sig_handler(int sig)
{
  signal(sig, sig_handler);

  switch(sig){
    case SIGPIPE:
	logmsg(LOG_ERR, T_("Error: Connection to client lost. Exiting.\n"));
	do_exit(1);
    case SIGUSR1:
	edebug = 0;
	break;
    case SIGTERM:
    case SIGINT:
    case SIGABRT:
    case SIGQUIT:
    case SIGHUP:
	do_exit(128 + sig);
  }
}

static Int8
check_interrupted(ConnStatus * connstat)
{
  int		i;
  Int32		len[2];
  nodeaddr	peeraddr;

  if(interrupted){
    return(1);
  }

  len[0] = len[1] = 0;
		/* I use 32 Bit int for len. See server.c for a *comment* */
  *((int *) &(len[0])) = sizeof(peeraddr);
  i = getpeername(connstat->fd, (struct sockaddr *)(&peeraddr), (int *) &(len[0]));
  if(i && errno == ENOTCONN){
	logmsg(LOG_ERR, T_("Error: Client disconnected. Exiting.\n"));
	kill(getpid(), SIGPIPE);
  }

  return(interrupted);
}

static void
usage(UChar * prnam)
{
  logmsg(LOG_ERR, T_("Usage: %s [ -db ] [ <configuration-file> ] [ -p <service/port> ] [ -L <locale> ]\n"), prnam);
  do_exit(1);
}

static Int32
send_status(int fd, UChar statusc)	/* utility */
{
  return(write_forced(fd, &statusc, 1) != 1 ? - errno : 0);
}

#define	send_cmd	send_status

static Int32
result(int fromfd, void *tcpmux_status)
{
  Int32		i;
  UChar		c;

  i = (tcp_mux_long_read(tcpmux_status, fromfd, &c, 1) != 1);

  return(i ? - errno : (Int32) c);  
}

static Int32
simple_command(UChar cmd, int fromfd, int tofd, void * tcpmux_status)
{
  Int32		i;

  ER__(send_cmd(tofd, cmd), i);

  return(result(fromfd, tcpmux_status));
}

static Int32
num_conns(void * cs, Int32 index)
{
  ConnStatus	**csp;

  csp = (ConnStatus **) cs;

  csp -= index;

  for(index = 0; *csp; csp++, index++);

  return(index);
}

static Int32
check_access(
  int		fromfd,
  int		tofd,
  void		*tcpmux_status,
  UChar		cmd,
  Int32		n,
  UChar		*client)
{
  UChar	buf[264];
  Int32	r;

  n = ABS(n);			/* raw cartridge numbers (< 0) are allowed */

  buf[0] = CHECKACCESS;
  buf[1] = cmd;
  Uns32_to_xref(buf + 1 + 1, n);
  strncpy(buf + 1 + 1 + 4, client, 256);
  if(write_forced(tofd, buf, 1 + 1 + 4 + 256) != 1 + 1 + 4 + 256)
    return(- errno);

  if( (r = result(fromfd, tcpmux_status)) )
    logmsg(LOG_CRIT, T_("Warning: Client %s tried to access cartridge %s%d, denied.\n"),
			client, cmd == SETCARTSET ? T_("set ") : "", (int) n);

  return(r);
}

static Int32
set_cryptkey_for_host(UChar * cryptfiles, UChar * hostname)
{
  Int32		i;
  UChar		*curkey;

  curkey = NULL;
  if(cryptfiles){
    i = get_entry_by_host(cryptfiles, hostname, &curkey);
    if(i < 0){
	logmsg(LOG_ERR,
		T_("Error: No authentication key file configured for %s.\n"),
				hostname);
	return(AUTHENTICATION);
    }
  }

  if(curkey){
    if( (i = check_cryptfile(curkey)) ){
	logmsg((i > 0 ? LOG_WARNING : LOG_ERR),
			T_("%s on encryption key file `%s': %s.\n"),
			(i > 0 ? T_("Warning") : T_("Error")),
			curkey, check_cryptfile_msg(i));

	if(i < 0)
	  return(AUTHENTICATION);

	logmsg(LOG_WARNING, T_("Warning: Ignoring file `%s', using compiled-in key.\n"),
					curkey);
	ZFREE(curkey);
    }
  }

  set_cryptkey(curkey, ACCESSKEYSTRING,
#ifdef	HAVE_DES_ENCRYPTION
		YES
#else
		NO
#endif
		);
  ZFREE(curkey);

  return(0);
}

static UChar *
set_client_id(ConnStatus * connstat, UChar * idstr)
{
  ZFREE(connstat->clientid);
  connstat->clientid = strdup(idstr);

  connstat->headerlen = 1 + strlen(idstr) + 1;

  return(connstat->clientid);
}

static Int32
resize_tapebuf(ConnStatus * connstat, Int32 newsize)
{
  UChar		*cbuf;

  if(newsize <= connstat->tapebufsiz)
    return(0);

  cbuf = RENEWP(connstat->tapebuf, UChar, newsize);
  if(!cbuf)
    return(FATAL_ERROR);

  connstat->tapebuf = cbuf;
  connstat->tapebufsiz = newsize;

  return(0);
}

static Int32
flush_to_server(
  int		fromfd,
  int		tofd,
  void		*tcpmux_status)
{
  Int32		i, e;

  if(tapebufptr <= 2)
    return(0);

  memset(tapebuf + tapebufptr, 0, commbufsiz + 2 - tapebufptr);

  i = commbufsiz + 1;
  ER__((write_forced(tofd, tapebuf + 1, i) != i), e);	/* send stuff */

  return(result(fromfd, tcpmux_status));
}

static Int32
write_to_server(
  UChar		*buf,
  Int32		num,
  int		fromfd,
  int		tofd,
  void		*tcpmux_status)
{
  Int32		n, i, e;

  while(num > 0){
    n = commbufsiz + 2 - tapebufptr;

    if(n > num){
	memcpy(tapebuf + tapebufptr, buf, num);
	tapebufptr += num;
	return(0);
    }

    memcpy(tapebuf + tapebufptr, buf, n);

    if(newfilerequested){
	ER__(request_new_file(fromfd, tofd, NULL), i);

	newfilerequested = NO;
    }

    i = commbufsiz + 1;				/* send stuff */
    ER__((tcp_mux_long_write(tcpmux_status, tofd, tapebuf + 1, i) != i), e);

    ER__(result(fromfd, tcpmux_status), e);		/* get result */

    num -= n;
    buf += n;
    tapebufptr = 2;
  }

  return(0);
}

static Int32
writetotape(
  ConnStatus	*connstat,
  int		fromfd,
  int		tofd,
  void		*tcpmux_status)
{
  Int32		i, n, tocb, inbufptr;

  tocb = connstat->commbufsiz;		/* how much do we have to copy */
  inbufptr = 0;					/* where we are currently */

  do{
    n = MPX_CHUNKSIZE - connstat->tapebufptr;	/* space in send buffer */

    if(tocb < n){				/* rest fits into buffer */
	memcpy(connstat->tapebuf + connstat->tapebufptr,	/* copy it */
				connstat->inbuf + 1 + inbufptr, tocb);
	connstat->tapebufptr += tocb;	/* update pointer, that's it */

	return(0);
    }

    if(n > 0)					/* fill up send buffer */
	memcpy(connstat->tapebuf + connstat->tapebufptr,
				connstat->inbuf + 1 + inbufptr, n);
		/* + 1 cause inbuf contains command "W" in 1st position */

    if(connstat->newfilerequested){
	newfilerequested = YES;
	connstat->newfilerequested = NO;
    }

    i = write_to_server(connstat->tapebuf, MPX_CHUNKSIZE,
					fromfd, tofd, tcpmux_status);
    ER__(connstat->outbuf[0] = (UChar) i, i);

    server_status = connstat->tape_status = OPENED_WRITING;

    connstat->tapebufptr = connstat->headerlen;		/* reset pointer */

    tocb -= n;
    inbufptr += n;
  } while(tocb > 0);

  return(0);
}

static Int32
readfromtape(
  ConnStatus	**csp,
  Int32		idx,
  int		fromfd,
  int		tofd,
  void		*tcpmux_status)
{
  Int32		cycles = 0;
  Int32		i, j, n;
  UChar		*cptr, res = 0;
  Flag		moved_start_byte;
  ConnStatus	*connstat, **allcs;

  connstat = *csp;
  allcs = csp - idx;

  while((n = connstat->commbufsiz - connstat->tapebufptr) > 0){
    cycles++;			/* first a check for interrupted, useful, */
    if(cycles > 100){			/* if we ain't getting sync'd */
	cycles = 0;
	if(check_interrupted(connstat))
	  return(interrupted);
    }

    while(tapebufptr < MPX_CHUNKSIZE){
	ER__(send_cmd(tofd, READFROMTAPE), j);	/* need more from tape */
	i = commbufsiz + 1;			/* one more cause of result */
	j = tcp_mux_long_read(tcpmux_status, fromfd, tapebuf + tapebufptr, i);
	tapebufptr += commbufsiz;		/* now points to result byte */
	res = tapebuf[tapebufptr];
	if(j != i || res)
	  return(res ? res : - errno);
    }

    if(!reading_in_sync){
	moved_start_byte = NO;

	for(cptr = tapebuf; cptr < tapebuf + tapebufptr; cptr++){
	  if((Uns16)(*cptr) == 0xff){
	    for(csp = allcs; *csp; csp++){
		j = (*csp)->tape_status;
		if((j == OPENED_FOR_READ || j == OPENED_READING)
			&& (cptr - tapebuf) + 1 + strlen((*csp)->clientid) + 1 < tapebufptr
			&& (!memcmp(cptr + 1, (*csp)->clientid, strlen((*csp)->clientid) + 1))){
		  reading_in_sync = YES;
		  break;
		}
	    }

	    i = cptr - tapebuf;
	    if(i > 0){
		if(tapebufptr > i){
		  memmove(tapebuf, cptr, tapebufptr - i);
		  moved_start_byte = YES;
		}
		tapebufptr -= i;
		cptr -= i;
	    }

	    if(reading_in_sync){
		break;
	    }
	  }
	}

	if(!reading_in_sync){
	  if(!moved_start_byte)
	    tapebufptr = 0;
	  continue;
	}
    }

    if((Uns16)(tapebuf[0]) != 0xff){
	UChar	cbuf[200];

	reading_in_sync = NO;
	logmsg(LOG_ERR,
		T_("Error: Demultiplexing stream falling out of sync."
			" Current bytes in buffer:\n"));
	strcpy(cbuf, "");
	for(j = 0; j < 32; j++)
	  sprintf(cbuf + strlen(cbuf), "%02x ", (int) tapebuf[j]);
	i = strlen(cbuf);
	for(j = 0; j < 32; j++){
	  cbuf[i++] = (isprint(tapebuf[j]) ? tapebuf[j] : (UChar) '.');
	  cbuf[i] = '\0';
	}
	logmsg(LOG_ERR, "%s\n", cbuf);

	continue;
    }

    if(tapebufptr < MPX_CHUNKSIZE)
	continue;

    if(!strcmp(tapebuf + 1, connstat->clientid)){
	j = MPX_CHUNKSIZE - connstat->headerlen;
	ER__(resize_tapebuf(connstat, connstat->tapebufptr + j), i);

	memcpy(connstat->tapebuf + connstat->tapebufptr,
			tapebuf + connstat->headerlen, j);
	connstat->tapebufptr += j;
    }

    if(tapebufptr > MPX_CHUNKSIZE)
	memmove(tapebuf, tapebuf + MPX_CHUNKSIZE, tapebufptr - MPX_CHUNKSIZE);
    tapebufptr -= MPX_CHUNKSIZE;
  }

  memcpy(connstat->outbuf, connstat->tapebuf, connstat->commbufsiz);
  connstat->outbuf[connstat->commbufsiz] = COMMAND_OK;

  if(connstat->tapebufptr > connstat->commbufsiz)
    memmove(connstat->tapebuf, connstat->tapebuf + connstat->commbufsiz,
		connstat->tapebufptr - connstat->commbufsiz);

  connstat->tapebufptr -= connstat->commbufsiz;

  return(0);
}

static Int32
getpos(int fromfd, int tofd, void * tcpmux_status, UChar cmd)
{
  UChar		buf[8];
  Int32		*cartptr, *fileptr;
  Int32		e;

  if(cmd == QUERYWRPOSITION ? wrtapepos_valid
		: (cmd == QUERYRDPOSITION ? rdtapepos_valid : tapepos_valid))
    return(0);

  ER__(send_cmd(tofd, cmd), e);

  ER__((read_forced(fromfd, buf, 8) != 8), e);

  if(buf[7])
    return((Int32) buf[7]);

  cartptr = (cmd == QUERYWRPOSITION ? &wrcartnum
		: (cmd == QUERYRDPOSITION ? &rdcartnum : &cartnum));
  fileptr = (cmd == QUERYWRPOSITION ? &wrfilenum
		: (cmd == QUERYRDPOSITION ? &rdfilenum : &filenum));

  xref_to_UnsN(cartptr, buf, 24);
  xref_to_Uns32(fileptr, buf + 3);

  if(cmd == QUERYWRPOSITION)
    wrtapepos_valid = YES;
  else if(cmd == QUERYRDPOSITION)
    rdtapepos_valid = YES;
  else
    tapepos_valid = YES;

  return(0);
}

static Int32
setcart(Int32 cart, int fromfd, int tofd, void * tcpmux_status)
{
  UChar		buf[4];
  Int32		e;

  tapepos_valid = NO;

  buf[0] = (cart > 0 ? SETCARTRIDGE : SETRAWCARTRIDGE);
  UnsN_to_xref(buf + 1, ABS(cart), 24);

  ER__((tcp_mux_long_write(tcpmux_status, tofd, buf, 4) != 4), e);

  ER__(result(fromfd, tcpmux_status), e);

  return(getpos(fromfd, tofd, tcpmux_status, QUERYPOSITION));
}

static Int32
setfile(Int32 file, int fromfd, int tofd, void * tcpmux_status)
{
  UChar		buf[5];
  Int32		e;

  tapepos_valid = NO;

  buf[0] = (file > 0 ? SETFILE : SETRAWFILE);
  Uns32_to_xref(buf + 1, ABS(file));

  ER__((tcp_mux_long_write(tcpmux_status, tofd, buf, 5) != 5), e);

  ER__(result(fromfd, tcpmux_status), e);

  return(getpos(fromfd, tofd, tcpmux_status, QUERYPOSITION));
}

static Int32
gettapeblocksize(Int32 * size, int fromfd, int tofd, void * tcpmux_status)
{
  UChar		buf[5];
  Int32		e;

  ER__(send_cmd(tofd, QUERYTAPEBLOCKSIZE), e);

  ER__(tcp_mux_long_read(tcpmux_status, fromfd, buf, 4) != 4, e);

  xref_to_Uns32(size, buf);

  return(result(fromfd, tcpmux_status));
}

static Int32
set_tapebuf_size(Int32 newsize)
{
  if(newsize > tapebuf_allocated){
    tapebuf = ZRENEWP(tapebuf, UChar, newsize);

    if(!tapebuf)
	return(FATAL_ERROR);
  }
  tapebuf_allocated = newsize;

  return(0);
}

static Int32
set_commbufsiz(
  Int32		newsize,
  int		fromfd,
  int		tofd,
  void		*tcpmux_status)
{
  UChar		buf[6];
  Int32		e;

  ER__(set_tapebuf_size((newsize + 2) * 2), e);

  buf[0] = SETCOMMBUFSIZ;
  Uns32_to_xref(buf + 1, newsize);

  ER__((write_forced(tofd, buf, 5) != 5), e);

  commbufsiz = newsize;

  return(result(fromfd, tcpmux_status));
}

static Int32
set_pos_if_req(
  ConnStatus	*connstat,
  int		fromfd,
  int		tofd,
  void		*tcpmux_status)
{
  Int32		i;

  if(connstat->req_cart /* && connstat->req_cart != cartnum */){
    i = setcart(connstat->req_cart, fromfd, tofd, tcpmux_status);
    connstat->req_cart = 0;
    connstat->outbuf[0] = (UChar) i;
    if(i)
	return(i);
  }

  if(connstat->req_filenum /* && connstat->req_filenum != filenum */){
    i = setfile(connstat->req_filenum, fromfd, tofd, tcpmux_status);
    connstat->req_filenum = 0;
    connstat->outbuf[0] = (UChar) i;
    if(i)
	return(i);
  }

  return(0);
}

static Int32
set_req_streamermode(
  ConnStatus	*connstat,
  int		fromfd,
  int		tofd,
  void		*tcpmux_status)
{
  Int32		e;

  ER__(simple_command(connstat->req_streamermode & BUFFERED_OPERATION ?
	SETBUFFEREDOP : SETSERIALOP, fromfd, tofd, tcpmux_status), e);

  ER__(simple_command(connstat->req_streamermode & CHANGE_CART_ON_EOT ?
	SETCHCARTONEOT : SETERRORONEOT, fromfd, tofd, tcpmux_status), e);

  return(0);
}

static Int32
pending_close_ops(
  ConnStatus 	*connstat,
  int		fromfd,
  int		tofd,
  void		*tcpmux_status)
{
  Int32		i, l, n;

  switch(connstat->tape_status){
    case OPENED_FOR_WRITE:
    case OPENED_WRITING:
	if(connstat->tapebufptr > connstat->headerlen){
	  memset(connstat->tapebuf + connstat->tapebufptr, 0,
		sizeof(UChar) * (MPX_CHUNKSIZE - connstat->tapebufptr));
	  i = write_to_server(connstat->tapebuf, MPX_CHUNKSIZE,
					fromfd, tofd, tcpmux_status);
	  ER__(connstat->outbuf[0] = (UChar) i, i);
	}
	l = MPX_CHUNKSIZE - connstat->headerlen;
	memset(connstat->tapebuf + connstat->headerlen, 0, l);
	for(n = MAX_PROT_CHUNKSIZE; n > 0; n -= l){
	  i = write_to_server(connstat->tapebuf, MPX_CHUNKSIZE,
					fromfd, tofd, tcpmux_status);
	  ER__(connstat->outbuf[0] = (UChar) i, i);
	}
  }

  return(0);
}

static Int32
closetape(
  Uns32		cmd, 
  int		fd,
  ConnStatus	**csp,
  Int32		idx,
  void		*user_data,
  int		fromfd,
  int		tofd,
  void		*tcpmux_status)
{
  Int32		i;
  ConnStatus	*connstat, **allconns;

  connstat = *csp;

  if(connstat->tape_status == NOT_OPEN || connstat->tape_status == CLOSED_NOTREW)
    return(0);

  allconns = csp - idx;

  i = pending_close_ops(connstat, fromfd, tofd, tcpmux_status);

  connstat->tape_status = (cmd == CLOSETAPE ? NOT_OPEN : CLOSED_NOTREW);
  raw_mode = NO;

  if(server_status & OPENED_RAW_ACCESS){
    server_status = connstat->tape_status = NOT_OPEN;

    return(PROC_DEFAULT_PROC);
  }

  switch(server_status){
	case OPENED_FOR_WRITE:
	case OPENED_WRITING:
		numwrclients--;

		tapepos_valid = wrtapepos_valid = rdtapepos_valid = NO;

		if(numwrclients == 0){
		  i = flush_to_server(fromfd, tofd, tcpmux_status);
		  connstat->outbuf[0] = (UChar) i;

		  server_status = connstat->tape_status = NOT_OPEN;

		  free_prev_wrclient_list();

		  return(PROC_DEFAULT_PROC);
		}

		if(!i)
		  i = getpos(fromfd, tofd, tcpmux_status, QUERYWRPOSITION);

		need_new_tapefile_for_wrclient(connstat->clientid,
						cartnum, filenum);

		if(i)
		  return(i);

		break;

	case OPENED_FOR_READ:
	case OPENED_READING:
		numrdclients--;

		tapepos_valid = wrtapepos_valid = rdtapepos_valid = NO;

		if(numrdclients == 0){
		  server_status = connstat->tape_status = NOT_OPEN;

		  return(PROC_DEFAULT_PROC);
		}

		break;
  }

  return(0);
}

Int32
start_slave_server()
{
  int		inpipe[2], outpipe[2];
  Int32		i, n;

  if(pipe(inpipe) || pipe(outpipe)){
    logmsg(LOG_ERR, T_("Error: Cannot create pipes to subprocess.\n"));
    do_exit(6);
  }

  pid = fork_forced();
  if(pid < 0){
    logmsg(LOG_ERR, T_("Error: Cannot start subprocess.\n"));
    do_exit(7);
  }

  if(!pid){
    close(inpipe[1]);
    close(outpipe[0]);

    dup2(inpipe[0], 0);
    dup2(outpipe[1], 1);

    execvp(sprog, sargv);

    logmsg(LOG_ERR, T_("Error: Cannot start program `%s'.\n"), sprog);

    exit(99);
  }

  close(inpipe[0]);
  close(outpipe[1]);
  fromfd = outpipe[0];
  tofd = inpipe[1];

  if(set_cryptkey_for_host(cryptfile, ""))
    do_exit(15);

  i = logon_to_server(fromfd, tofd, GREETING_MESSAGE, VERSION_MESSAGE_KEY,
				NULL,
#ifdef	HAVE_DES_ENCRYPTION
			AUTH_USE_DES
#else
			0
#endif
			);
  if(i){
    logmsg(LOG_ERR, T_("Error: Cannot logon to server ?!?!?\n"));
    do_exit(8);
  }

  n = (MPX_CHUNKSIZE + 2) * 2;
  ER__(set_tapebuf_size(n), i);
  memset(tapebuf, 0, n);

  i = getpos(fromfd, tofd, NULL, QUERYPOSITION);

  i = gettapeblocksize(&tapeblocksize, fromfd, tofd, NULL);
  i = set_commbufsiz(MIN(MAXCOMMBUFSIZ, tapeblocksize), fromfd, tofd, NULL);

  return(i);
}

/* The protocol handling routines */
static void *
init_conn_func(
  int		fd,
  Int32		idx,
  void		*allconns,
  struct sockaddr	*addr,
  void		*data,
  TcpMuxCallbDoneActions	*ret_actions)
{
  Int32		i;
  ConnStatus	*connstat;
  UChar		*peername = NULL;

  if(daemonize && pid < 0){
    i = start_slave_server();

    if(i)
	return(NULL);
  }

  peername = get_hostnamestr(addr);
  if(set_cryptkey_for_host(cryptfile, peername)){
    ZFREE(peername);
    return(NULL);
  }

  i = authenticate_client(fd, fd, unsecure,
			VERSION_MESSAGE GREETING_MESSAGE, 0, AUTHENTICATION,
#ifdef	HAVE_DES_ENCRYPTION
			AUTH_USE_DES
#else
			0
#endif
			, 0);
  if(i){
    ZFREE(peername);
    return(NULL);
  }

  connstat = NEWP(ConnStatus, 1);
  if(!connstat){
    ZFREE(peername);
    return(NULL);
  }

  SETZERO(connstat[0]);
  connstat->status = CONNSTAT_CONNECTED;
  connstat->tape_status = NOT_OPEN;
  connstat->fd = fd;
  connstat->inbuf = NEWP(UChar, MAX_PROT_CHUNKSIZE);
  connstat->outbuf = NEWP(UChar, MAX_PROT_CHUNKSIZE);
  connstat->commbufsiz = DEFCOMMBUFSIZ;
  connstat->tapebuf = NEWP(UChar, MPX_CHUNKSIZE);
  connstat->tapebufsiz = MPX_CHUNKSIZE; 
  connstat->req_streamermode = (nobuffering ? 0 : BUFFERED_OPERATION)
					| CHANGE_CART_ON_EOT;
  connstat->peeraddr = addr;
  connstat->peername = peername;
  set_client_id(connstat, connstat->peername);

  if(! connstat->peername || ! connstat->inbuf || ! connstat->outbuf
			|| ! connstat->clientid|| !connstat->tapebuf){
    ZFREE(connstat->peername);
    ZFREE(connstat->clientid);
    ZFREE(connstat->inbuf);
    ZFREE(connstat->outbuf);
    return(NULL);
  }

  connstat->inbuf_allocated = connstat->outbuf_allocated = MAX_PROT_CHUNKSIZE;
  connstat->req_cartset = 1;

  return(connstat);
}

static Int32
close_conn_func(
  int		fd,
  void		*conn_data,
  Int32		idx,
  void		*user_data,
  void		*tcpmux_status)
{
  ConnStatus	*connstat;

  connstat = *((ConnStatus **) conn_data);

  if(connstat->tape_status != NOT_OPEN){
    closetape(CLOSETAPE, fd, conn_data, idx, user_data, fromfd, tofd, tcpmux_status);
  }

  ZFREE(connstat->peername);
  ZFREE(connstat->clientid);
  ZFREE(connstat->inbuf);
  ZFREE(connstat->outbuf);
  ZFREE(connstat->tapebuf);

  /* handle buffer contents depending on conn_status ... */

  free(connstat);

  return(0);
}

static void
toomany_func(int fd, void * data, void * tcpmux_status)
{
  authenticate_client(fd, fd, unsecure, VERSION_MESSAGE GREETING_MESSAGE,
			TOO_MANY_CLIENTS, AUTHENTICATION,
#ifdef	HAVE_DES_ENCRYPTION
			AUTH_USE_DES
#else
			0
#endif
			, 0);
}

static Int32
flush_pending_ops(
  ConnStatus	*connstat,
  void		*user_data,
  int		fromfd,
  int		tofd,
  void		*tcpmux_status)
{
  return(0);
}

static Int32
handle_pending(
  ConnStatus	**csp,
  int		fromfd,
  int		tofd,
  void		*user_data,
  void		*tcpmux_status)
{
  ConnStatus	**cs, *connstat;
  Int32		idx, r = 0;

  for(cs = csp, idx = 0; *cs; cs++, idx++){
    connstat = *cs;

    if(connstat->pending && connstat->pending_cmd){
	connstat->pending = NO;

	r = handle_prot_func(connstat->fd, cs, idx, user_data, NULL, tcpmux_status);

	break;
    }
  }

  return(r);
}

static Int32
handle_mserver_prot(
  Uns32		command,
  int		fd,
  ConnStatus	**csp,
  Int32		idx,
  void		*user_data,
  int		fromfd,
  int		tofd,
  void		*tcpmux_status)
{
  Int32		i, n, st = COMMAND_OK;
  ConnStatus	*connstat, **allconns;
  UChar		cmd, *cptr;

  cmd = (UChar) command;
  connstat = *csp;

  if(raw_mode && cmd != CLOSETAPE && cmd != CLOSETAPEN && cmd != GOODBYE
				&& cmd != SETCOMMBUFSIZ)
    return(PROC_DEFAULT_PROC);

  allconns = csp - idx;

  connstat->outbuf[0] = COMMAND_OK;

  if(isspace(cmd))
    cmd = NOOPERATION;

  switch(cmd){
    case WRITETOTAPE:
	i = (connstat->tape_status & MODE_MASK);
	if(i != OPENED_FOR_WRITE && i != OPENED_WRITING){
	  connstat->outbuf[0] = DEVNOTOPENWR;
	  break;
	}

	i = (server_status & MODE_MASK);
	if(i != OPENED_FOR_WRITE && i != OPENED_WRITING)
	  return(PROC_DEFAULT_PROC);	/* should never happen */

	i = writetotape(connstat, fromfd, tofd, tcpmux_status);
	tapepos_valid = wrtapepos_valid = rdtapepos_valid = NO;

	break;

    case READFROMTAPE:
	i = (connstat->tape_status & MODE_MASK);
	if(i != OPENED_FOR_READ && i != OPENED_READING){
	  connstat->outbuf[0] = DEVNOTOPENRD;
	  break;
	}

	i = (server_status & MODE_MASK);
	if(i != OPENED_FOR_READ && i != OPENED_READING)
	  return(PROC_DEFAULT_PROC);	/* should never happen */

	i = readfromtape(csp, idx, fromfd, tofd, tcpmux_status);
	if(i)
	  connstat->outbuf[connstat->commbufsiz] = i;

	tapepos_valid = NO;

	break;

    case SETCOMMBUFSIZ:
	xref_to_Uns32(&(connstat->commbufsiz), connstat->inbuf + 1);
	i = connstat->commbufsiz + 4;
	if(i > connstat->inbuf_allocated){
	  connstat->inbuf = RENEWP(connstat->inbuf, UChar, i);
	  connstat->inbuf_allocated = i;
	  if(!connstat->inbuf){
	    connstat->outbuf[0] = FATAL_ERROR;
	    break;
	  }
	}
	if(i > connstat->outbuf_allocated){
	  cptr = RENEWP(connstat->outbuf, UChar, i);
	  connstat->outbuf_allocated = i;
	  if(!cptr)
	    connstat->outbuf[0] = FATAL_ERROR;
	  else
	    connstat->outbuf = cptr;
	}

	if(resize_tapebuf(connstat, i)){
	  connstat->outbuf[0] = FATAL_ERROR;
	  break;
	}

	if(raw_mode)
	  return(PROC_DEFAULT_PROC);

	break;

    case QUERYPOSITION:
	if(tapepos_valid){
	  UnsN_to_xref(connstat->outbuf, cartnum, 24);
	  Uns32_to_xref(connstat->outbuf + 3, filenum);
	  connstat->outbuf[7] = COMMAND_OK;
	  break;
	}

	return(PROC_DEFAULT_PROC | PROC_POST_PROC);
	break;

    case QUERYWRPOSITION:
	if(wrtapepos_valid){
	  UnsN_to_xref(connstat->outbuf, wrcartnum, 24);
	  Uns32_to_xref(connstat->outbuf + 3, wrfilenum);
	  connstat->outbuf[7] = COMMAND_OK;
	  break;
	}

	return(PROC_DEFAULT_PROC | PROC_POST_PROC);
	break;

    case QUERYRDPOSITION:
	if(rdtapepos_valid){
	  UnsN_to_xref(connstat->outbuf, rdcartnum, 24);
	  Uns32_to_xref(connstat->outbuf + 3, rdfilenum);
	  connstat->outbuf[7] = COMMAND_OK;
	  break;
	}

	return(PROC_DEFAULT_PROC | PROC_POST_PROC);
	break;

    case OQUERYPOSITION:
	if(tapepos_valid){
	  connstat->outbuf[0] = (UChar) cartnum;
	  UnsN_to_xref(connstat->outbuf + 1, filenum, 24);
	  connstat->outbuf[4] = COMMAND_OK;
	  break;
	}

	return(PROC_DEFAULT_PROC | PROC_POST_PROC);
	break;

    case OQUERYWRPOSITION:
	if(wrtapepos_valid){
	  connstat->outbuf[0] = (UChar) wrcartnum;
	  UnsN_to_xref(connstat->outbuf + 1, wrfilenum, 24);
	  connstat->outbuf[4] = COMMAND_OK;
	  break;
	}

	return(PROC_DEFAULT_PROC | PROC_POST_PROC);
	break;

    case QUERYNUMCARTS:
	if(numcarts > 0){
	  UnsN_to_xref(connstat->outbuf, numcarts, 24);
	  connstat->outbuf[4] = COMMAND_OK;
	  break;
	}

	return(PROC_DEFAULT_PROC | PROC_POST_PROC);
	break;

    case QUERYCARTSET:
    case QUERYRDYFORSERV:
	return(PROC_DEFAULT_PROC | PROC_POST_PROC);

    case OPENFORWRITE:
	connstat->req_cart = connstat->req_filenum = 0;	/* discard silently */

	wrtapepos_valid = rdtapepos_valid = tapepos_valid = NO;

	switch(server_status){
	  case OPENED_FOR_WRITE:	/* note: raw access is special case */
	  case OPENED_WRITING:
		if(cartset != connstat->req_cartset){	/* wrong cartset */
		  connstat->pending_cmd = cmd;		/* -> must wait */
		  connstat->pending = 1;
		  break;
		}

		i = num_conns(csp, idx);
		for(i--; i >= 0; i--){
		  if(i == idx)
		    continue;

		  if(! strcmp(connstat->clientid, allconns[i]->clientid)){
		    connstat->outbuf[0] = CLIENT_NOT_UNIQUE;
		    break;
		  }
		}
		if(i >= 0)
		  break;

		if( (i = getpos(fromfd, tofd, tcpmux_status, QUERYWRPOSITION)) )
		  break;

		if(need_new_tapefile_for_wrclient(connstat->clientid,
						cartnum, filenum))
		  if(request_new_file(fromfd, tofd, tcpmux_status))
		    break;

	  case NOT_OPEN:
		if( (i = getpos(fromfd, tofd, tcpmux_status, QUERYWRPOSITION)) )
		  break;

		i = check_access(fromfd, tofd, tcpmux_status,
				SETCARTRIDGE, wrcartnum, connstat->peername);
		if(i){
		  connstat->outbuf[0] = (UChar) i;
		  break;
		}

		connstat->tapebuf[0] = (UChar) 0xff;	/* start of header */
		strcpy(connstat->tapebuf + 1, connstat->clientid);

		connstat->tapebufptr = connstat->headerlen;
					/* header is: 0xff clientid 0x00 */

		if(server_status == NOT_OPEN){
		  tapebuf[0] = '\0';
		  tapebuf[1] = WRITETOTAPE;
		  tapebufptr = 2;

		  ER__(set_req_streamermode(connstat, fromfd, tofd, tcpmux_status), i);

		  return(PROC_DEFAULT_PROC | PROC_POST_PROC);
		}

		connstat->tape_status = OPENED_FOR_WRITE;
		numwrclients++;

		break;

	  default:
		connstat->pending_cmd = cmd;
		connstat->pending = 1;
	}

	break;

    case OPENFORREAD:
	switch(server_status){
/* multi-read not implemented, then must change MAXINT to OPENED_FOR_READ */
	  case MAXINT:	/* this never happens */
		if(connstat->req_cart == cartnum && connstat->req_filenum == filenum){
		  i = num_conns(csp, idx);
		  for(i--; i >= 0; i--){
		    if(i == idx)
			continue;

		    if(! strcmp(connstat->clientid, allconns[i]->clientid)){
			connstat->outbuf[0] = CLIENT_NOT_UNIQUE;
			break;
		    }
		  }
		  if(i >= 0)
		    break;

		  connstat->tape_status = OPENED_FOR_READ;
		  connstat->tapebufptr = 0;
		  numrdclients++;

		  break;
		}

	  case OPENED_FOR_READ:
	  case OPENED_READING:
		connstat->tapebufptr = 0;

		connstat->pending = 1;
		connstat->pending_cmd = cmd;

		break;

	  case NOT_OPEN:
		if(! connstat->req_cart){
		  i = getpos(fromfd, tofd, tcpmux_status, QUERYPOSITION);
		  if(i){
		    connstat->outbuf[0] = (UChar) i;
		    break;
		  }
		}

		i = check_access(fromfd, tofd, tcpmux_status, SETCARTRIDGE,
			connstat->req_cart ? connstat->req_cart : cartnum,
				connstat->peername);
		if(i){
		  connstat->outbuf[0] = (UChar) i;
		  break;
		}

		i = set_pos_if_req(connstat, fromfd, tofd, tcpmux_status);

		ER__(set_req_streamermode(connstat, fromfd, tofd, tcpmux_status), i);

		return(PROC_DEFAULT_PROC | PROC_POST_PROC);

		break;

	  default:
		connstat->pending_cmd = cmd;
		connstat->pending = 1;
	}

	break;

    case CLOSETAPE:
    case CLOSETAPEN:
	ER__(closetape(cmd, fd, csp, idx, user_data, fromfd, tofd, tcpmux_status), i);

	break;

    case OPENFORRAWWRITE:
    case OPENFORRAWREAD:
	if(server_status != NOT_OPEN){
	  connstat->pending = YES;
	  connstat->pending_cmd = cmd;
	  break;
	}

	if(! connstat->req_cart){
	  i = getpos(fromfd, tofd, tcpmux_status, QUERYPOSITION);
	  if(i){
	    connstat->outbuf[0] = (UChar) i;
	    break;
	  }
	}

	i = check_access(fromfd, tofd, tcpmux_status, SETCARTRIDGE,
		connstat->req_cart ? connstat->req_cart : cartnum,
			connstat->peername);
	if(i){
	  connstat->outbuf[0] = (UChar) i;
	  break;
	}

	ER__(set_req_streamermode(connstat, fromfd, tofd, tcpmux_status), i);

	ER__(set_pos_if_req(connstat, fromfd, tofd, tcpmux_status), i);

	ER__(set_commbufsiz(connstat->commbufsiz, fromfd, tofd, tcpmux_status), i);

	return(PROC_DEFAULT_PROC | PROC_POST_PROC);

	break;

    case REQUESTNEWFILE:
	connstat->newfilerequested = YES;
	break;

    case SETCARTSET:
	xref_to_UnsN(&n, connstat->inbuf + 1, 24);

	if( (i = check_access(fromfd, tofd, tcpmux_status, SETCARTSET,
					n, connstat->peername)) ){
	  connstat->outbuf[0] = (UChar) i;
	  break;
	}

	connstat->req_cartset = n;

	switch(server_status){
	  case NOT_OPEN:
		return(PROC_POST_PROC | PROC_DEFAULT_PROC);
		break;				/* no obstacle to set it */

	  default:
		if(connstat->req_cartset == cartset)
		  return(0);			/* cartset already correct */

		connstat->pending = 1;		/* device open -> must wait */
		connstat->pending_cmd = cmd;
	}

	break;	/* that's it. No further actions here */

    case SETCARTRIDGE:
    case SETRAWCARTRIDGE:
    case OSETCARTRIDGE:
	i = (cmd != OSETCARTRIDGE ? 24 : 8);
	xref_to_UnsN(&n, connstat->inbuf + 1, i);

	if( (i = check_access(fromfd, tofd, tcpmux_status, SETCARTRIDGE,
					n, connstat->peername)) ){
	  connstat->outbuf[0] = (UChar) i;
	  break;
	}

	connstat->req_cart = n;
	connstat->outbuf[0] = COMMAND_OK;

	if(cmd == SETRAWCARTRIDGE)
	  connstat->req_cart = - connstat->req_cart;

	break;	/* that's it. Request will be serviced on an OPEN... command */

    case SETFILE:
    case OSETFILE:
    case SETRAWFILE:
	i = (cmd != OSETFILE ? 32 : 24);
	xref_to_UnsN(&(connstat->req_filenum), connstat->inbuf + 1, i);

	connstat->outbuf[0] = COMMAND_OK;

	if(cmd == SETRAWFILE)
	  connstat->req_filenum = - connstat->req_filenum;

	break;	/* that's it. Request will be serviced on an OPEN... command */

    case CLIENTIDENT:
	if(!set_client_id(connstat, connstat->inbuf + 1))
	  return(PROC_ERROR(ENOMEM));

	return(PROC_DEFAULT_PROC);	/* there's nothing, that could fail */
	break;

    case GOODBYE:
	st = flush_pending_ops(connstat, user_data,
				fromfd, tofd, tcpmux_status);

	connstat->outbuf[0] = (UChar) st;

	raw_mode = NO;

	return(PROC_FINISH_CONN);
	break;

    case SKIPFILES:
    case ERASETAPE:
	tapepos_valid = NO;
    case CLIENTBACKUP:
    case OCLIENTBACKUP:
    case GETNUMREADVALID:
    case SETNUMWRITEVALID:
    case QUERYTAPEBLOCKSIZE:
    case MESSAGETEXT:
    case REQUESTNEWCART:
    case QUERYWRITTENTAPES:
    case SERVERIDENT:
    case USERIDENT:
    case QUERYNEEDEDTAPES:
    case GETCURMSG:
	return(PROC_DEFAULT_PROC);

    case NOOPERATION:
	break;

    case SETBUFFEREDOP:
	connstat->req_streamermode |= BUFFERED_OPERATION;
	break;

    case SETSERIALOP:
	connstat->req_streamermode &= (~BUFFERED_OPERATION);
	break;

    case SETCHCARTONEOT:
	connstat->req_streamermode |= CHANGE_CART_ON_EOT;
	break;

    case SETERRORONEOT:
	connstat->req_streamermode &= (~CHANGE_CART_ON_EOT);
	break;

    default:
	return(PROC_DEFAULT_PROC);
  }

  return(0);
}

static Int32
handle_mserver_prot_post(
  Uns32		cmd,
  int		fd,
  ConnStatus	**csp,
  Int32		idx,
  void		*user_data,
  int		fromfd,
  int		tofd,
  void		*tcpmux_status)
{
  ConnStatus	*connstat, **cs;
  Int32		i;

  connstat = *csp;

  if(isspace(cmd))
    cmd = NOOPERATION;

  switch(cmd){
    case OPENFORWRITE:
	if(connstat->outbuf[0] == COMMAND_OK){
	  server_status = connstat->tape_status = OPENED_FOR_WRITE;
	  numwrclients++;
	}

	break;

    case OPENFORREAD:
	if(connstat->outbuf[0] == COMMAND_OK){
	  server_status = connstat->tape_status = OPENED_FOR_READ;
	  numrdclients++;
	  reading_in_sync = NO;
	  connstat->tapebufptr = 0;
	  tapebufptr = 0;
	}

	break;

    case SETCARTSET:
	if(connstat->outbuf[0] == COMMAND_OK)
	  cartset = connstat->req_cartset;

	break;

    case SETCARTRIDGE:
    case SETRAWCARTRIDGE:
    case OSETCARTRIDGE:
	if(connstat->outbuf[0] == COMMAND_OK)
	  getpos(fromfd, tofd, tcpmux_status, QUERYPOSITION);

	break;

    case SETFILE:
    case SETRAWFILE:
    case OSETFILE:
	if(connstat->outbuf[0] == COMMAND_OK)
	  getpos(fromfd, tofd, tcpmux_status, QUERYPOSITION);

	break;

    case QUERYPOSITION:
	if(connstat->outbuf[7] == COMMAND_OK){
	  xref_to_UnsN(&cartnum, connstat->outbuf, 24);
	  xref_to_Uns32(&filenum, connstat->outbuf + 3);
	}

	break;

    case QUERYWRPOSITION:
	if(connstat->outbuf[7] == COMMAND_OK){
	  xref_to_UnsN(&wrcartnum, connstat->outbuf, 24);
	  xref_to_Uns32(&wrfilenum, connstat->outbuf + 3);
	}

	break;

    case QUERYRDPOSITION:
	if(connstat->outbuf[7] == COMMAND_OK){
	  xref_to_UnsN(&rdcartnum, connstat->outbuf, 24);
	  xref_to_Uns32(&rdfilenum, connstat->outbuf + 3);
	}

	break;

    case OQUERYPOSITION:
	if(connstat->outbuf[4] == COMMAND_OK){
	  cartnum = (Int32) connstat->outbuf[0];
	  xref_to_UnsN(&filenum, connstat->outbuf + 1, 24);
	}

	break;

    case OQUERYWRPOSITION:
	if(connstat->outbuf[4] == COMMAND_OK){
	  wrcartnum = (Int32) connstat->outbuf[0];
	  xref_to_UnsN(&wrfilenum, connstat->outbuf + 1, 24);
	}

	break;

    case QUERYNUMCARTS:
	if(connstat->outbuf[3] == COMMAND_OK)
	  xref_to_UnsN(&numcarts, connstat->outbuf, 24);

	break;

    case QUERYCARTSET:
	if(connstat->outbuf[3] == COMMAND_OK)
	  xref_to_UnsN(&cartset, connstat->outbuf, 24);

	break;

    case QUERYRDYFORSERV:
	i = (server_status > 0 ? server_status : 0);
	Uns32_to_xref(connstat->outbuf + 1, i);

	i = server_status & MODE_MASK;
	if(i == OPENED_FOR_READ || i == OPENED_READING)
	  i = numrdclients;
	else if(i == OPENED_FOR_WRITE || i == OPENED_WRITING)
	  i = numwrclients;
	else
	  i = 0;
	Uns32_to_xref(connstat->outbuf + 5, i);

	for(cs = csp - idx, i = 0; *cs; cs++)
	  if((*cs)->pending && (*cs)->pending_cmd)
	    i++;

	Uns32_to_xref(connstat->outbuf + 9, i);

	break;

    case OPENFORRAWWRITE:
    case OPENFORRAWREAD:
	if( (i = connstat->outbuf[0]) )
#if	1
	  break;
#else
	  return(PROC_ERROR(i));
#endif

	i = (cmd == OPENFORRAWWRITE ? OPENED_FOR_WRITE : OPENED_FOR_READ);

	server_status = connstat->tape_status = (i | OPENED_RAW_ACCESS);
	raw_mode = YES;

	break;

    case NOOPERATION:
    case WRITETOTAPE:
    case SKIPFILES:
    case ERASETAPE:
    case READFROMTAPE:
    case CLOSETAPE:
    case CLOSETAPEN:
    case CLIENTBACKUP:
    case OCLIENTBACKUP:
    case SETBUFFEREDOP:
    case SETSERIALOP:
    case SETCHCARTONEOT:
    case SETERRORONEOT:
    case GETNUMREADVALID:
    case SETNUMWRITEVALID:
    case CLIENTIDENT:
    case GOODBYE:
	break;

    default:
	return(PROC_ERROR(EINVAL));
  }

  return(0);
}

static Int32
handle_prot_func(
  int		fd,
  void		*conn_data,
  Int32		idx,
  void		*user_data,
  TcpMuxCallbDoneActions	*ret_actions,
  void		*tcpmux_status)
{
  ConnStatus	*connstat;
  Uns32		cmd;
  Int32		i, k, n, num;
  Uns32		proc_mode;
  AFBProtocol	*op;
  UChar		*cptr;

  connstat = *((ConnStatus **) conn_data);

  if(connstat->pending && connstat->pending_cmd)
    return(0);

  cptr = connstat->inbuf;
  if(connstat->pending_cmd && ! connstat->pending){
    *cptr = (UChar) connstat->pending_cmd;
    connstat->pending_cmd = NOOPERATION /* 0 */;
    n = 1;
  }
  else{
    n = read_forced(fd, cptr, 1);
    if(n < 1)
	return(1);
  }

  op = prot_items[*cptr];

  if(!op || ((cmd = op->cmd) != *cptr && op->cmd != NOOPERATION)){
    logmsg(LOG_CRIT, T_("Internal Error: Protocol not properly initialized.\n"));
    return(2);
  }

  switch(op->cmd){		/* special cases, to be handled explicitly */
    case AUTHENTICATE:
	ER__(set_cryptkey_for_host(cryptfile, connstat->peername), i);

	i = logon_to_server(fd, fd, NULL, NULL, NULL,
#ifdef	HAVE_DES_ENCRYPTION
				AUTH_USE_DES
#else
				0
#endif
				| AUTH_NOCONFIRM);
	return(i);

	break;
  }

  k = n = 0;
  if(op->num_fix_in > 0){
    num = (op->num_fix_in == VARCOMMBUFSIZ ?
			connstat->commbufsiz : op->num_fix_in);
    n = read_forced(fd, cptr + 1, num);
    if(n != num)
	return(3);

    if(op->pos_num_in > 0){
	xref_to_UnsN(&k, cptr + op->pos_num_in, op->size_num_in << 3);

	if((i = n + k + 1) > connstat->inbuf_allocated){
	  cptr = NEWP(UChar, i);
	  if(!cptr)
	    return(9);
	  memcpy(cptr, connstat->inbuf, (n + 1) * sizeof(UChar));
	  free(connstat->inbuf);
	  connstat->inbuf = cptr;
	  connstat->inbuf_allocated = i;
	}

	i = read_forced(fd, cptr + 1 + n, k);
	if(i != k)
	  return(4);
    }
  }

  proc_mode = handle_mserver_prot(cmd, fd, conn_data, idx, user_data,
					fromfd, tofd, tcpmux_status);

  if(proc_mode & PROC_ERROR_MASK)
    return(- PROC_ERRVAL(proc_mode));

  if(proc_mode & PROC_DEFAULT_PROC){
    i = n + k + 1;
    i = (write_forced(tofd, cptr, i) != i);
    if(i)
	return(- errno);
  }

  n = k = 0;
  cptr = connstat->outbuf;
  if(op->num_fix_out > 0){
    num = (op->num_fix_out == VARCOMMBUFSIZ ?
			connstat->commbufsiz : op->num_fix_out);
    if(proc_mode & PROC_DEFAULT_PROC){
	n = tcp_mux_long_read(tcpmux_status, fromfd, cptr, num);
	if(n != num)
	  return(5);
    }
    n = num;

    if(op->pos_num_out > 0){
	xref_to_UnsN(&k, cptr + op->pos_num_out - 1, op->size_num_out << 3);

	if(proc_mode & PROC_DEFAULT_PROC){
	  if((i = n + k + 1) > connstat->outbuf_allocated){
	    cptr = NEWP(UChar, i);
	    if(!cptr)
		return(8);
	    memcpy(cptr, connstat->outbuf, n * sizeof(UChar));
	    free(connstat->outbuf);
	    connstat->outbuf = cptr;
	    connstat->outbuf_allocated = i;
	  }

	  i = tcp_mux_long_read(tcpmux_status, fromfd, cptr + n, k);
	  if(i != k)
	    return(6);
	}
    }
  }

  if(proc_mode & PROC_DEFAULT_PROC){
    if(op->cmd != NOOPERATION){
      i = tcp_mux_long_read(tcpmux_status,
			fromfd, cptr + n + k, 1);	/* result byte */
      if(i < 1)
	return(7);
    }
  }

  if(!connstat->pending){
    if(proc_mode & PROC_POST_PROC){
	proc_mode = handle_mserver_prot_post(cmd, fd, conn_data, idx, user_data,
					fromfd, tofd, tcpmux_status);
	if(proc_mode & PROC_ERROR_MASK)
	  return(- PROC_ERRVAL(proc_mode));
    }

    if(op->cmd != NOOPERATION){
      i = n + k + 1;
      i = (write_forced(fd, cptr, i) != i);
      if(i)
	return(- errno);
    }
  }

  if(server_status == NOT_OPEN){
    i = handle_pending(((ConnStatus **) conn_data) - idx, fromfd, tofd,
					user_data, tcpmux_status);
  }			/* pending commands never lead to connection end */

  return(proc_mode & PROC_FINISH_CONN);
}


main(int argc, char ** argv)
{
  UChar		*cptr, **cpptr = NULL, buf[200], *portname = NULL;
  UChar		*localestr = NULL;
  Int32		i, k = 0, maxconns, portnum;
  Uns32		tcpmux_flags;
  char		**nargv;
  Flag		restart = NO, autodaemonize = NO;
  struct stat	statb;

  prot_items = init_prot_spec();

  if(goptions(argc, (UChar **) argv, "s-1:;s:l;b:D;b:s;b:d;s:p;b:b;s:L;b:@;*",
		&i, &configfilename, &loggingfile, &edebug, &unsecure,
		&daemonize, &portname, &nobuffering, &localestr, &restart,
		&k, &cpptr))	/* unused arguments are passed to server */
    usage(argv[0]);

#ifdef ENABLE_NLS
  setlocale(LC_ALL, localestr ? localestr : (UChar *)"");
  if(localestr){	/* some implementations of gettext don't honour */
    set_env("LC_ALL", localestr);	/* the settings done by setlocale, */
  }					/* but always look into the env */
  bindtextdomain(PACKAGE, LOCALEDIR);
  textdomain(PACKAGE);
#endif

  if(isatty(0) && !daemonize){
    fprintf(stderr, T_("Warning: Not started by the inetd and not with option -d for daemonize. Assuming daemon startup is desired.\n"));
    daemonize = autodaemonize = YES;
  }

  free_array(cpptr, k);		/* don't need memory for additional args */

  signal(SIGUSR1, sig_handler);
  while(edebug && !daemonize);	/* For debugging the caught running daemon */

  portnum = 0;

  if(daemonize){
    portnum = -1;

    if(portname)
	portnum = get_tcp_portnum(portname);

    if(portnum < 0){
      portnum = get_tcp_portnum(DEFAULT_MSERVICE);
      if(portnum < 0){
	logmsg(LOG_WARNING, T_("Warning: Cannot resolve service name `%s' to port number, using default port %d.\n"),
			portname, (int) DEFAULT_MPORT);
      }
      else{
	if(portname)
	  logmsg(LOG_WARNING, T_("Warning: Cannot resolve service name `%s' to port number, using default service `%s'.\n"),
			portname, DEFAULT_MSERVICE);
      }
    }
  }

  progname = find_program(argv[0]);
  if(!progname){
    EM__(progname = strapp(DEFSERVBINDIR FN_DIRSEPSTR, "afmserver"));
    if(eaccess(progname, X_OK)){
	logmsg(LOG_ERR, T_("Error: Could not find program file of `%s'\n"),
				argv[0]);
	do_exit(3);
    }
  }
  cptr = mkabspath(progname, NULL);
  free(progname);
  progname = cptr;

  cptr = resolve_path__(progname, NULL);
  if(!cptr){
    logmsg(LOG_ERR, T_("Error: Could not find program file of `%s'\n"),
				argv[0]);
    do_exit(4);
  }

  free(progname);
  progname = cptr;

  EM__(backuphome = strdup(progname));
  cptr = FN_LASTDIRDELIM(backuphome);
  if(cptr){
    *cptr = '\0';
    cptr = FN_LASTDIRDELIM(backuphome);
    if(cptr)
	*cptr = '\0';
  }

  if(!configfilename){
    for(cpptr = default_configfilenames; *cpptr; cpptr++);

    EM__(*cpptr = strapp(backuphome, FN_DIRSEPSTR "lib" FN_DIRSEPSTR DEFSERVERCONF));

    for(cpptr = default_configfilenames; *cpptr; cpptr++){
      configfilename = *cpptr;
      if(!stat(*cpptr, &statb) && eaccess(*cpptr, R_OK)){
	while(*cpptr) cpptr++;
	break;
      }
      if(!stat(*cpptr, &statb))
	break;
    }

    configfilename = *cpptr;
  }

  i = read_param_file(configfilename, entries,
			sizeof(entries) / sizeof(entries[0]), NULL, NULL);
  if(i){
    logmsg(LOG_ERR, T_("Warning: Error reading configuration file `%s'.\n"),
		configfilename ? configfilename : (UChar *) T_("<none found>"));  }

  if(vardir)
    if(empty_string(vardir))
      ZFREE(vardir);
  if(vardir)
    massage_string(vardir);
#ifndef	ORIG_DEFAULTS
  if(!vardir)
    vardir = strdup(DEFSERVVARDIR);
  if(!libdir)
    libdir = strdup(DEFSERVLIBDIR);
  if(!confdir)
    confdir = strdup(DEFSERVCONFDIR);
  if(!bindir)
    bindir = strdup(DEFSERVBINDIR);
#else
  if(!vardir)
    vardir = strapp(backuphome, FN_DIRSEPSTR "var");
  if(!libdir)
    libdir = strapp(backuphome, FN_DIRSEPSTR "lib");
  if(!confdir)
    confdir = strapp(backuphome, FN_DIRSEPSTR "etc");
  if(!bindir)
    bindir = strapp(backuphome, FN_DIRSEPSTR "bin");
#endif
  if(!bindir || !vardir || !libdir || !confdir)
     ENM__;

  if(cryptfile)
    if(empty_string(cryptfile))
      cryptfile = NULL;
  if(cryptfile){
    EEM__(repl_dirs(&cryptfile));
    massage_string(cryptfile);
  }
  if(loggingfile)
    if(empty_string(loggingfile))
      loggingfile = NULL;
  if(loggingfile){
    massage_string(loggingfile);
    EEM__(repl_dirs(&loggingfile));

    if(loggingfile[0] == '@'){
	if(isspace(loggingfile[1]) || !loggingfile[1])
	  syslog_ident = strdup(PACKAGE);
	else
	  syslog_ident = strword(loggingfile + 1, 0);

	EM__(syslog_ident);

	cptr = first_nospace(first_space(loggingfile));
	memmove(loggingfile, cptr, strlen(cptr) + 1);
	if(empty_string(loggingfile))
	  ZFREE(loggingfile);
    }
  }

  /* check encryption key */
  i = set_cryptkey_for_host(cryptfile, "");
  if(i < 0)
    do_exit(11);

  EM__(nargv = NEWP(char *, argc + 5));
  nargv[0] = progname;
  i = 0;
  if(argc <= 1 || strcmp(argv[1], "-@")){	/* 1 || * => 1 implication !! */
	i = 1;
	EM__(nargv[1] = strdup("-@"));
  }
  if(autodaemonize)
    EM__(nargv[++i] = strdup("-d"));
    
  memcpy(nargv + 1 + i, argv + 1, sizeof(char *) * (argc - 1));
  nargv[argc + i] = NULL;

  if(daemonize){
    cptr = FN_BASENAME(progname);
    EM__(pidfile = strchain(vardir, FN_DIRSEPSTR, cptr, ".pid", NULL));
  }

  if(daemonize && ! edebug && ! restart){
    i = fork_forced();
    if(i < 0){
	fprintf(stderr, T_("Warning: cannot fork, continuing to run in foreground.\n"));
    }
    if(i == 0){
	chdir("/");

	setsid();

	detach_from_tty();

	daemonized = YES;
    }
    if(i > 0){
	exit(0);
    }

    i = set_wlock(pidfile);	/* lock here and not in parent, because */
    if(i < 0){			/* locks are not inherited by child procs */
	fprintf(stderr,
		T_("Error: Cannot lock PID file `%s', probably `%s' is already running.\n"),
		pidfile, cptr);
	ZFREE(pidfile);
	do_exit(12);
    }

    sprintf(buf, "%d\n", getpid());
    k = strlen(buf);
    if(write_forced(i, buf, k) < k
#ifdef	HAVE_FSYNC
		|| fsync(i) < 0
#endif
				){
	fprintf(stderr, T_("Warning: Cannot write PID file `%s' correctly.\n"),
		pidfile);
	unlink(pidfile);
	ZFREE(pidfile);
    }
  }

  signal(SIGTERM, sig_handler);
  signal(SIGINT, sig_handler);
  signal(SIGABRT, sig_handler);
  signal(SIGQUIT, sig_handler);
  signal(SIGHUP, sig_handler);

  if(!restart){
    cptr = loggingfile;
    if(!cptr)
	cptr = NULLFILE;
    if(eaccess(cptr, W_OK))
	cptr = NULLFILE;
    lfd = open(cptr, O_APPEND | O_CREAT | O_WRONLY, 0644);
    if(lfd < 0)
	lfd = 2;
    else{
	dup2(lfd, 1);
	dup2(lfd, 2);
    }
  }

  EM__(sprog = strdup(progname));

  cptr = FN_LASTDIRDELIM(sprog);
  if(!cptr){
    logmsg(LOG_ERR, T_("Error: Could not separate path of `%s'\n"),
				argv[0]);
    do_exit(5);
  }

  *cptr = '\0';
  EM__(cptr = strapp(sprog, FN_DIRSEPSTR "afserver"));

  free(sprog);
  sprog = cptr;

  EM__(sargv = NEWP(char *, argc + 2));

  memcpy(sargv + 1, argv, argc * sizeof(char *));
  sargv[argc + 1] = NULL;
  sargv[0] = sprog;
  sargv[1] = "-S";

  for(i = 2; i < argc + 1; i++){
    if(!strcmp(sargv[i], "-d") || !strcmp(sargv[i], "-@")){
	memmove(sargv + i, sargv + i + 1, sizeof(char *) * (argc - i + 1));
	i--;
	argc--;
    }
    else if(!strcmp(sargv[i], "-p")){
	memmove(sargv + i, sargv + i + 2, sizeof(char *) * (argc - i));
	i -= 1;
	argc -= 2;
    }
  }

  if(daemonized){
    close(0);
    close(1);
    close(2);
  }

  if(!daemonize)
    i = start_slave_server();

  maxconns = free_fds() - 10;	/* safety distance 10 */

  tcpmux_flags = TCPMUX_INETD_STARTED | TCPMUX_STOP_ON_LAST_CLOSE;
  if(daemonize)
    tcpmux_flags = TCPMUX_STOP_ON_LAST_CLOSE;

  i = tcp_mux_service(portnum, NULL,
		init_conn_func, handle_prot_func, close_conn_func,
		maxconns, toomany_func, tcpmux_flags, NULL);
  if(i){
    i = errno;
    logmsg(LOG_ERR, T_("Warning: Service unexpectedly terminated.\n"));
    if(i == EADDRINUSE){
	logmsg(LOG_ERR, T_("Warning: Address already in use. Exiting.\n"));
	daemonize = NO;
    }
  }

  if(pid > 0){
    buf[0] = GOODBYE;
    k = (write_forced(tofd, buf, 1) != 1);
    if(k){
	logmsg(LOG_ERR, T_("Warning: Cannot send command %d (%c) to slave server.\n"),
			buf[0], buf[0]);
	i = i ? i : (k ? - errno : 0);
    }

    k = result(fromfd, NULL);
    if(k){
	logmsg(LOG_ERR, T_("Warning: Did not get success response for command %d (%c) from slave server.\n"),
			buf[0], buf[0]);
	i = i ? i : k;
    }

    waitpid_forced(pid, &pst, 0);
  }

  close(fromfd);
  close(tofd);

  if(lfd != 2)
    close(lfd);

  pst = WEXITSTATUS(pst);

  if(daemonize){
    execv(progname, nargv);
  }

  exit(pst ? pst : i);
}
