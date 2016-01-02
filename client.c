/****************** Start of $RCSfile: client.c,v $  ****************
*
* $Source: /home/alb/afbackup/afbackup-3.5.8.12/RCS/client.c,v $
* $Id: client.c,v 1.18 2012/11/01 09:53:00 alb Exp alb $
* $Date: 2012/11/01 09:53:00 $
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

  static char * fileversion = "$RCSfile: client.c,v $ $Source: /home/alb/afbackup/afbackup-3.5.8.12/RCS/client.c,v $ $Id: client.c,v 1.18 2012/11/01 09:53:00 alb Exp alb $ " PACKAGE " " VERSION_STRING;

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdarg.h>
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/utsname.h>
#include <poll.h>
#ifdef	HAVE_SYS_TIME_H
#include <sys/time.h>
#endif
#ifdef	HAVE_SYS_RESOURCE_H
#include <sys/resource.h>
#endif
#ifdef	HAVE_SYS_PARAM_H
#include <sys/param.h>
#endif
#ifdef  HAVE_SYS_WAIT_H
#include <sys/wait.h>
#endif
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#ifdef	HAVE_NETINET_IN_SYSTM_H
#include <netinet/in_systm.h>
#endif
#ifdef	HAVE_NETINET_IP_H
#include <netinet/ip.h>
#endif
#include <x_types.h>
#include <x_errno.h>
#include <genutils.h>
#include <sysutils.h>
#include <netutils.h>
#include <fileutil.h>
#include <afpacker.h>

#include "server.h"
#include "cryptkey.h"
#include "afbackup.h"

#define	SERIAL_OPERATION	1
#define	QUEUED_OPERATION	2

#define	MAX_QUEUE_MEMORY	10485760	/* 10 MB */

typedef	struct _queueentry_ {
  UChar		*inmem;
  UChar		*outmem;
  Uns32		num_in;
  Uns32		num_out;
  Uns16		instruction;
  Uns16		processed;
  Uns32		req_queue_cbs;
} QueueEntry;

#define	NOT_PROCESSED		0
#define	PROCESSED_OK		RESERVED_ERROR_CODE

Flag		noqueued = NO;
UChar		comm_mode = QUEUED_OPERATION;
Uns32		queuelen = 512;
QueueEntry	*queue_entries = NULL;
Uns32		max_queuelen = 0;
UChar		*outbufmem = NULL;
UChar		*inbufmem = NULL;
Uns32		allocated_outmem = 0;
Uns32		allocated_inmem = 0;
Uns32		queueent_done_idx = 0;
Uns32		queueent_requested_idx = 0;
Uns32		queueent_processed_idx = 0;
Uns32		queue_insert_idx = 0;
Uns32		cur_num_entries_in_queue = 0;
Int32		queue_commbufsiz = 0;

Flag		std_io = NO;

#define	QUEUEDOP	(queue_entries != NULL)

#define	num_entries_in_queue	(cur_num_entries_in_queue = \
				(queue_insert_idx >= queueent_done_idx ? \
				queue_insert_idx - queueent_done_idx : \
			queuelen - queueent_done_idx + queue_insert_idx))


UChar		*servername = NULL;
short unsigned	portnum = DEFAULT_PORT;

UChar		*cryptfile = NULL;

Int32		commbufsiz = DEFCOMMBUFSIZ;
Int32		max_outarglen;
Int32		max_inresultlen;

UChar		scbuf[MAXCOMMBUFSIZ + 8];
UChar		*cbuf = &(scbuf[1]);
Int32		cbufptr = 0;		/* pointer into send/receive-buffer */

Int32		bytes_transferred = 0;

Int32		bytes_wr_unkn_pos = 0;

#define	reset_data_buffer	{ cbufptr = 0; }
#define	reset_tape_io		{ endofrec = 0; }

int		commfd = -1;
int		commfd_socktype = -1;

Int32		cart = 0;
Int32		filenum = 0;
Int32		wrcart = 0;
Int32		wrfilenum = 0;
Int32		rdcart = 0;
Int32		rdfilenum = 0;
Int32		cartset = 0;
Int32		numcarts = 0;
Int32		tapeblocksize = 0;

UChar		*infoheader = NULL;
UChar		*identitystr = NULL;

Flag		filenum_valid = NO;
Flag		wrfilenum_valid = NO;
Flag		rdfilenum_valid = NO;

Int32		endofrec = 0;

AarParams	params;

Flag		bu_create = NO;
Flag		bu_extract = NO;
Flag		bu_contents = NO;
Flag		bu_verify = NO;
Flag		bu_index = NO;
Flag		bu_ready = NO;
Flag		bu_duptape = NO;
UChar		*bu_duptapeargs = NULL;
Flag		bu_junction = NO;
Flag		bu_msg_itv = 0;
Flag		allfiles = NO;
Uns8		verbose = 0;
Flag		c_f_verbose = NO;
Flag		l_verbose = NO;
Flag		o_verbose = NO;
Flag		u_verbose = NO;
Flag		bu_request = NO;
Flag		bu_svrmsg = NO;
UChar		*servermsg = NULL;
Flag		req_newfile = NO;
Flag		req_newcart = NO;

UChar		*clientprog = NULL;

UChar		*serverversion = NULL;
Flag		server_can_sendtbufsiz = NO;
Flag		server_can_setcbufsiz = NO;
Flag		server_can_sendid = NO;
Flag		server_can_sendutapes = NO;
Flag		server_can_userid = NO;
Flag		server_can_auth = NO;
Flag		server_can_sendrdpos = NO;
Flag		server_can_sendmsgs = NO;

UChar		**filelist = NULL;

char		*toextractfilename = NULL;
char		*errorlogfile = NULL;
char		*savefile = NULL;
Flag		save_stdio = NO;

char		username[128] = "";

Int32		num_errors = 0;

UChar		long_errmsgs = 0;
UChar		**gargv;

void		(*cleanup_proc)(void *) = NULL;
void		*cleanup_arg = NULL;

#define	MAX_NUM_ERRORS		20

#define	ESTIM_PROT_OVERHEAD	50

#define	MAX_NUM_WRITE_WITHOUT_POS_UPDATE	5000000

struct fault_msgs	fault_messages[] = FAULT_MESSAGES;

Int32	delete_command_queue();
Int64	queued_read(int, UChar *, Int64);
Int64	queued_write(int, UChar *, Int64);
Int32	post_process_entry(Int32);
Int32	post_process_rest_of_queue(Int32);
Int32	append_to_queue(Uns16, UChar *, Uns32, UChar *, Uns32);
Int32	simple_command(UChar);
Int32	setnumbytesvalid(Int32);
Int32	getnumbytesvalid(Int32 *);
Int32	getcartandfile(AarParams *);
Int32	getwrcartandfile(AarParams *);
Int32	getrdcartandfile(AarParams *);
Int32	send_commbufsiz(Int32);
Int32	duplicate_tape(UChar *, AarParams *);
Int32	copytaperemote(UChar *, Int32, Int32, UChar *);
Int32	copytapelocally(Int32);

#define	set_server_serial	simple_command(SETSERIALOP)
#define	set_server_buffering	simple_command(SETBUFFEREDOP)
#define	set_server_erroneot	simple_command(SETERRORONEOT)
#define	set_server_chcartoneot	simple_command(SETCHCARTONEOT)
#define	getserverstate(ptr, w)	getbufferwithcmd(ptr, 512, QUERYRDYFORSERV, w)
#define	getserverid(ptr, w)	getbufferwithcmd(ptr, 256, SERVERIDENT, w)

#define	ER__(cmd, lerrfl)	{ if( (lerrfl = cmd) ) return(lerrfl); }

UChar *	fetch_tape_contents(AarParams * params);
UChar *	put_tape_contents(AarParams * params);

static void
errmsg(UChar * msg, ...)
{
  va_list	args;

  va_start(args, msg);

  if(long_errmsgs)
    fprintf(stderr, "%s, ", actimestr());

  vfprintf(stderr, msg, args);

  va_end(args);

  if(msg[strlen(msg) - 1] != '\n')
    fprintf(stderr, ".\n");

  fflush(stderr);
}

static Int32
E__(Int32 e)
{
  if(e)
    errmsg(T_("%sError: %s"), (e > 0 ? T_("Server ") : ""),
		(e > 0 ? fault_string(e) : (UChar *) strerror(-e)));

  return(e);
}

#define	OK__(func)	(! E__(func))

void
signal_handler(int s)
{
  if(long_errmsgs || s == SIGTERM || s == SIGINT)
    fprintf(stderr, T_("%s got signal %d, exiting.\n"),
					FN_BASENAME(gargv[0]), s);

  if(cleanup_proc)
    (*cleanup_proc)(cleanup_arg);

  exit(128 | s);
}

Int32
send_cmd(UChar cmd)		/* utility */
{
  if(write_forced(commfd, &cmd, 1) != 1)
    return(errno ? - errno : EOF);

  bytes_transferred += ESTIM_PROT_OVERHEAD + 1;

  return(0);
}

UChar
result()
{
  UChar		c;
  Int32		i;

  if(savefile)
    return(0);

  i = (read_forced(commfd, &c, 1) != 1);

  bytes_transferred += ESTIM_PROT_OVERHEAD + 1;

  if(i){
    return(errno ? - errno : EOF);
  }

  if(! c)
    return(0);

  return(c);
}

Int32	/* This function assumes, that we can do it. It is sure, that no data */
change_queue_bufsiz(Int32 newsize)	/* will be lost */
{
  Int32		new_queuelen, new_max_outarglen, new_max_inresultlen;
  Int32		i, n, upper_part_size, prev_qcbs;
  UChar		*new_outbufmem, *new_inbufmem, *scptr, *dcptr, *mcptr;
  QueueEntry	*new_queue_entries;

  if(! queue_entries)		/* no queue currently present */
    return(0);
  if(newsize == queue_commbufsiz)
    return(0);

  new_max_outarglen = new_max_inresultlen = newsize + 4;
  new_queuelen = allocated_outmem / new_max_outarglen;

  if(new_queuelen > max_queuelen){
    queue_entries = RENEWP(queue_entries, QueueEntry, new_queuelen);
    if(!queue_entries)
	return(FATAL_ERROR);

    memset(queue_entries + max_queuelen, 0,
		(new_queuelen - max_queuelen) * sizeof(QueueEntry));
    max_queuelen = new_queuelen;
  }

  if(newsize > queue_commbufsiz){	/* if there's size-change-requests */
    for(i = queueent_done_idx; i != queue_insert_idx; i = (i + 1) % queuelen){
	n = queue_entries[i].req_queue_cbs;	/* in queue to size smaller */
	if(n > 0 && n < newsize){		/* then current request */
	  queue_entries[i].req_queue_cbs = newsize;	/* change them */
	}
    }
  }

  if(new_queuelen < 2){		/* it's rediculuous, if we have that few */
    new_queuelen = 2;		/* memory, nonetheless we catch this case */
    allocated_outmem = new_max_outarglen * new_queuelen;
    allocated_inmem = new_max_inresultlen * new_queuelen;
    new_outbufmem = RENEWP(outbufmem, UChar, allocated_outmem);
    new_inbufmem = RENEWP(inbufmem, UChar, allocated_inmem);
    new_queue_entries = RENEWP(queue_entries, QueueEntry, new_queuelen);

    if(!new_outbufmem || !new_inbufmem || !new_queue_entries){
	ZFREE(new_outbufmem);
	ZFREE(new_inbufmem);
	ZFREE(new_queue_entries);
	return(delete_command_queue());
    }

    outbufmem = new_outbufmem;
    inbufmem = new_inbufmem;
    queue_entries = new_queue_entries;
  }

  do{
    prev_qcbs = queue_commbufsiz;
    if(newsize > queue_commbufsiz){			/* wait for space */
	i = post_process_rest_of_queue(new_queuelen - 1);
	if(i)				/* to move stuff around and to pack */
	  return(i);			/* the queue entry data wider */
    }
  }while(prev_qcbs != queue_commbufsiz);

  if(newsize > queue_commbufsiz){
    if(queue_insert_idx > queueent_done_idx){
	n = queue_insert_idx - queueent_done_idx;
	if(queueent_done_idx > 0){
	  memmove(outbufmem, outbufmem + max_outarglen * queueent_done_idx,
			max_outarglen * n);
	  memmove(inbufmem, inbufmem + max_inresultlen * queueent_done_idx,
			max_inresultlen * n);
	  memmove(queue_entries, queue_entries + queueent_done_idx,
			sizeof(queue_entries[0]) * n);
	}

	n--;
	scptr = outbufmem + max_outarglen * n;
	dcptr = outbufmem + new_max_outarglen * n;
	for(i = n; i > 0; i--){
	  memmove(dcptr, scptr, max_outarglen);
	  scptr -= max_outarglen;
	  dcptr -= new_max_outarglen;
	}
	scptr = inbufmem + max_inresultlen * n;
	dcptr = inbufmem + new_max_inresultlen * n;
	for(i = n; i > 0; i--){
	  memmove(dcptr, scptr, max_inresultlen);
	  scptr -= max_inresultlen;
	  dcptr -= new_max_inresultlen;
	}

	queueent_requested_idx -= queueent_done_idx;
	queueent_processed_idx -= queueent_done_idx;
	queue_insert_idx -= queueent_done_idx;
	queueent_done_idx = 0;
    }
    if(queue_insert_idx < queueent_done_idx){
	n = queuelen - new_queuelen;
	i = queuelen - queueent_done_idx;
	if(i > 0){
	  i *= max_outarglen;
	  scptr = outbufmem + allocated_outmem - i;
	  memmove(scptr, outbufmem + max_outarglen * queueent_done_idx, i);
	  dcptr = outbufmem + (queueent_done_idx - n) * new_max_outarglen;
	  for(i = queueent_done_idx; i < queuelen; i++){
	    memmove(dcptr, scptr, max_outarglen);
	    scptr += max_outarglen;
	    dcptr += new_max_outarglen;
	  }

	  i = (queuelen - queueent_done_idx) * max_inresultlen;
	  scptr = inbufmem + allocated_inmem - i;
	  memmove(scptr, inbufmem + max_inresultlen * queueent_done_idx, i);
	  dcptr = inbufmem + (queueent_done_idx - n) * new_max_inresultlen;
	  for(i = queueent_done_idx; i < queuelen; i++){
	    memmove(dcptr, scptr, max_inresultlen);
	    scptr += max_inresultlen;
	    dcptr += new_max_inresultlen;
	  }
	}

	if(queue_insert_idx > 0){
	  scptr = outbufmem + max_outarglen * (queue_insert_idx - 1);
	  dcptr = outbufmem + new_max_outarglen * (queue_insert_idx - 1);
	  for(i = queue_insert_idx - 1; i > 0; i--){
	    memmove(dcptr, scptr, max_outarglen);
	    scptr -= max_outarglen;
	    dcptr -= new_max_outarglen;
	  }

	  scptr = inbufmem + max_inresultlen * (queue_insert_idx - 1);
	  dcptr = inbufmem + new_max_inresultlen * (queue_insert_idx - 1);
	  for(i = queue_insert_idx - 1; i > 0; i--){
	    memmove(dcptr, scptr, max_inresultlen);
	    scptr -= max_inresultlen;
	    dcptr -= new_max_inresultlen;
	  }
	}

	memmove(queue_entries + queueent_done_idx - n,
		queue_entries + queueent_done_idx,
		sizeof(queue_entries[0]) * (queuelen - queueent_done_idx));

	if(queueent_requested_idx >= queueent_done_idx)
	  queueent_requested_idx -= n;
	if(queueent_processed_idx >= queueent_done_idx)
	  queueent_processed_idx -= n;
	queueent_done_idx -= n;
    }
  }
  if(newsize < queue_commbufsiz){
    if(queue_insert_idx > queueent_done_idx){
	scptr = outbufmem + max_outarglen * queueent_done_idx;
	dcptr = outbufmem + new_max_outarglen * queueent_done_idx;
	for(i = queueent_done_idx; i < queue_insert_idx; i++){
	  memmove(dcptr, scptr, new_max_outarglen);
	  scptr += max_outarglen;
	  dcptr += new_max_outarglen;
	}
	scptr = inbufmem + max_inresultlen * queueent_done_idx;
	dcptr = inbufmem + new_max_inresultlen * queueent_done_idx;
	for(i = queueent_done_idx; i < queue_insert_idx; i++){
	  memmove(dcptr, scptr, new_max_inresultlen);
	  scptr += max_inresultlen;
	  dcptr += new_max_inresultlen;
	}
    }
    if(queue_insert_idx < queueent_done_idx){
	n = new_queuelen - queuelen;

	if(queue_insert_idx > 0){
	  scptr = outbufmem + max_outarglen;
	  dcptr = outbufmem + new_max_outarglen;
	  for(i = 1; i < queue_insert_idx; i++){
	    memmove(dcptr, scptr, new_max_outarglen);
	    scptr += max_outarglen;
	    dcptr += new_max_outarglen;
	  }

	  scptr = inbufmem + max_inresultlen;
	  dcptr = inbufmem + new_max_inresultlen;
	  for(i = 1; i < queue_insert_idx; i++){
	    memmove(dcptr, scptr, new_max_inresultlen);
	    scptr += max_inresultlen;
	    dcptr += new_max_inresultlen;
	  }
	}

	upper_part_size = queuelen - queueent_done_idx;
	if(upper_part_size > 0){
	  scptr = dcptr = mcptr
			= outbufmem + queueent_done_idx * max_outarglen;
	  scptr += max_outarglen;
	  dcptr += new_max_outarglen;
	  for(i = 1; i < upper_part_size; i++){
	    memmove(dcptr, scptr, new_max_outarglen);
	    scptr += max_outarglen;
	    dcptr += new_max_outarglen;
	  }
	  i = upper_part_size * new_max_outarglen;
	  dcptr = outbufmem + new_queuelen * new_max_outarglen - i;
	  memmove(dcptr, mcptr, i);

	  scptr = dcptr = mcptr
			= inbufmem + queueent_done_idx * max_inresultlen;
	  scptr += max_inresultlen;
	  dcptr += new_max_inresultlen;
	  for(i = 1; i < upper_part_size; i++){
	    memmove(dcptr, scptr, new_max_inresultlen);
	    scptr += max_inresultlen;
	    dcptr += new_max_inresultlen;
	  }
	  i = upper_part_size * new_max_inresultlen;
	  dcptr = inbufmem + new_queuelen * new_max_inresultlen - i;
	  memmove(dcptr, mcptr, i);

	  memmove(queue_entries + queueent_done_idx + n,
		queue_entries + queueent_done_idx,
		sizeof(queue_entries[0]) * upper_part_size);
	}

	if(queueent_requested_idx >= queueent_done_idx)
	  queueent_requested_idx += n;
	if(queueent_processed_idx >= queueent_done_idx)
	  queueent_processed_idx += n;
	queueent_done_idx += n;
    }
  }

  max_outarglen = new_max_outarglen;
  max_inresultlen = new_max_inresultlen;
  queuelen = new_queuelen;
  queue_commbufsiz = newsize;

  return(0);
}

Int32
switch_queue_bufsiz(Int32 newsize)
{
  Int32		i;

  if(! queue_entries)		/* no queue currently present */
    return(0);

  if(newsize == queue_commbufsiz)
    return(0);

  if(newsize > queue_commbufsiz){
    ER__(change_queue_bufsiz(newsize), i);
  }
  else{
    queue_entries[queue_insert_idx].req_queue_cbs = newsize;
  }

  return(0);
}

Int32
alloc_commbuf(Int32 newsize)
{
  if(newsize > MAXCOMMBUFSIZ || newsize < 0)
    return(NO_VALID_COMMBUFSIZ);

  commbufsiz = newsize;

  return(scbuf ? 0 : -1);
}

Int32
set_commbufsiz(Int32 newsize)
{
  Int32		i;

  if(newsize > MAXCOMMBUFSIZ || newsize < 0)
    return(NO_VALID_COMMBUFSIZ);

  if(QUEUEDOP){
    i = switch_queue_bufsiz(newsize);
    if(i)
	return(i);
  }

  ER__(send_commbufsiz(newsize), i);

  return(alloc_commbuf(newsize));
}

#define	OPTIMIZER_CHUNKSIZE	5000000
#define	MIN_CBSDIFF		(MINCOMMBUFSIZ >> 1)

Int32				/* Albi's 3-walking-points optimizer */
try_optimize_throughput()	/* ("albimizer") */
{
  static Flag	meas_initialized = NO;
  static Real64	ratios[3];
  static Int32	cbs[3], actidx, chunksize, cbsdiff;
  static struct timeval	last_time;
  struct timeval	act_time;
  Real64	timediff, ratio;
  Int32		i, maxidx;

  if(!server_can_setcbufsiz)
    return(0);

  if(!meas_initialized){
    chunksize = queue_entries ? MAX(allocated_outmem, OPTIMIZER_CHUNKSIZE) : OPTIMIZER_CHUNKSIZE;

    gettimeofday(&last_time, NULL);

    cbs[0] = MINCOMMBUFSIZ;
    cbs[2] = MAXCOMMBUFSIZ;
    cbs[1] = (MINCOMMBUFSIZ + MAXCOMMBUFSIZ) / 2;
    cbsdiff = (MAXCOMMBUFSIZ - MINCOMMBUFSIZ) / 2;
    ratios[0] = ratios[1] = ratios[2] = -1.0;
    actidx = 0;

    bytes_transferred = 0;
    meas_initialized = YES;

    return(set_commbufsiz(cbs[actidx]));
  }

  if(bytes_transferred < chunksize)
	return(0);

  gettimeofday(&act_time, NULL);

  timediff = (Real64) act_time.tv_sec + ((Real64) act_time.tv_usec / 1.0e6)
	- ((Real64) last_time.tv_sec + ((Real64) last_time.tv_usec / 1.0e6));
  if(timediff < 0.0)
    timediff += 86400.0;

  COPYVAL(last_time, act_time);

  ratio = (Real64) bytes_transferred / timediff;

  bytes_transferred = 0;

  ratios[actidx] = ratio;

  if(ratios[0] < 0.0)
    actidx = 0;
  else if(ratios[1] < 0.0)
    actidx = 1;
  else if(ratios[2] < 0.0)
    actidx = 2;
  else
    actidx = -1;

  if(actidx >= 0)			/* still have unknown values */
    return(set_commbufsiz(cbs[actidx]));	/* go on measuring */

  maxidx = 0;
  if(ratios[1] > ratios[0])
    maxidx = 1;
  if(ratios[2] > ratios[maxidx])
    maxidx = 2;

  switch(maxidx){
    case 1:
	i = cbsdiff >> 1;
	if(i < MIN_CBSDIFF){
	  ratio = drandom();
	  actidx = 1;
	  if(ratio < 0.07){		/* curiosity kills the cat */
	    actidx = 0;
	  }
	  if(ratio > 0.93){
	    actidx = 2;
	  }
	}
	else{
	  cbsdiff = i;
	  ratios[0] = ratios[1] = ratios[2] = -1.0;
	  cbs[0] += cbsdiff;
	  cbs[2] -= cbsdiff;
	  actidx = 0;
	}

	break;

    case 0:
	actidx = 0;
	if(cbs[0] - cbsdiff < MINCOMMBUFSIZ){
	  i = cbsdiff >> 1;
	  if(i >= MIN_CBSDIFF){
	    cbsdiff = i;
	    if(cbs[0] - i < MINCOMMBUFSIZ){
		cbs[0] = MINCOMMBUFSIZ;
		cbs[1] = MINCOMMBUFSIZ + cbsdiff;
		cbs[2] = MINCOMMBUFSIZ + (cbsdiff << 1);
		ratios[0] = ratios[1] = ratios[2] = -1.0;
	    }
	    else{
		cbs[2] = cbs[1];
		cbs[1] = cbs[0];
		cbs[0] -= i;
		ratios[2] = ratios[1];
		ratios[1] = ratios[0];
	    }
	  }
	  else{
	    cbs[0] = MINCOMMBUFSIZ;
	    cbs[1] = MINCOMMBUFSIZ + MIN_CBSDIFF;
	    cbs[2] = MINCOMMBUFSIZ + (MIN_CBSDIFF << 1);
	    cbsdiff = MIN_CBSDIFF;
	    ratio = drandom();
	    if(ratio < 0.07)
		actidx = 1;
	    if(ratio < 0.005)
		actidx = 2;
	  }
	}
	else{
	    cbs[2] = cbs[1];
	    cbs[1] = cbs[0];
	    cbs[0] -= cbsdiff;
	    ratios[2] = ratios[1];
	    ratios[1] = ratios[0];
	}

	break;

     case 2:
	actidx = 2;
	if(cbs[2] + cbsdiff > MAXCOMMBUFSIZ){
	  i = cbsdiff >> 1;
	  if(i >= MIN_CBSDIFF){
	    cbsdiff = i;
	    if(cbs[2] + i > MAXCOMMBUFSIZ){
		cbs[2] = MAXCOMMBUFSIZ;
		cbs[1] = MAXCOMMBUFSIZ - cbsdiff;
		cbs[0] = MAXCOMMBUFSIZ - (cbsdiff << 1);
		ratios[0] = ratios[1] = ratios[2] = -1.0;
	    }
	    else{
		cbs[0] = cbs[1];
		cbs[1] = cbs[2];
		cbs[2] += i;
		ratios[0] = ratios[1];
		ratios[1] = ratios[2];
	    }
	  }
	  else{
	    cbs[2] = MAXCOMMBUFSIZ;
	    cbs[1] = MAXCOMMBUFSIZ - MIN_CBSDIFF;
	    cbs[0] = MAXCOMMBUFSIZ - (MIN_CBSDIFF << 1);
	    cbsdiff = MIN_CBSDIFF;
	    ratio = drandom();
	    if(ratio < 0.07)
		actidx = 1;
	    if(ratio < 0.005)
		actidx = 0;
	  }
	}
	else{
	    cbs[0] = cbs[1];
	    cbs[1] = cbs[2];
	    cbs[2] += cbsdiff;
	    ratios[0] = ratios[1];
	    ratios[1] = ratios[2];
	}

	break;
  }

  return(set_commbufsiz(cbs[actidx]));
}

Int32
send_svrmsg(int fromfd, int tofd, UChar * msg, Int32 len)
{
  Int32		i;
  UChar		lbuf[DEFCOMMBUFSIZ + 2];

  if(len > DEFCOMMBUFSIZ - 4)
    return(-1);

  Uns32_to_xref(lbuf + 1, len);
  memcpy(lbuf + 1 + 4, msg, len * sizeof(UChar));

  if(QUEUEDOP)
    return(append_to_queue(MESSAGETEXT, NULL, 0, lbuf + 1, 4 + len));

  lbuf[0] = MESSAGETEXT;

  i = 1 + 4 + len;
  if(write_forced(tofd, lbuf, i) != i)
    return(- errno);

  bytes_transferred += ESTIM_PROT_OVERHEAD + i;

  return(result());
}

UChar
send_buffer_to_server()
{
  Int32		i, num, r;
  UChar		*cptr;

  scbuf[0] = WRITETOTAPE;

  cptr = scbuf;
  num = commbufsiz + 1;

  if(savefile){
    cptr = cbuf;
    num = commbufsiz;

    i = write_forced(commfd, cptr, num);

    return(i < num ? - errno : 0);
  }

  filenum_valid = wrfilenum_valid = rdfilenum_valid = NO;

  if(QUEUEDOP){
    r = append_to_queue(WRITETOTAPE, NULL, 0, cbuf, commbufsiz);

    return(r);
  }

  if(write_forced(commfd, cptr, num) != num)
    return(- errno);

  bytes_transferred += ESTIM_PROT_OVERHEAD + num;

  r = result();

  return(r);
}

Int32
send_pending(Flag rawaccess)
{
  Int32		i, num_pad_bytes = 0;

  while(cbufptr > commbufsiz){
    ER__(send_buffer_to_server(), i);

    cbufptr -= commbufsiz;

    memcpy(cbuf, cbuf + commbufsiz, cbufptr);
  }

  if(cbufptr > 0){
    if(cbufptr < commbufsiz){
	num_pad_bytes = commbufsiz - cbufptr;
	memset(cbuf + cbufptr, 0, num_pad_bytes * sizeof(UChar));

	if(rawaccess)
	  setnumbytesvalid(cbufptr);
    }

    ER__(send_buffer_to_server(), i);
  }

  cbufptr = 0;

  if(!savefile && !rawaccess){
    memset(cbuf, 0, commbufsiz * sizeof(UChar));
    num_pad_bytes = MAXCOMMBUFSIZ - num_pad_bytes;

    /* This was on the server side previously, what is inaccurate. */
    /* The client should take care itself, that there is enough pad */
    /* space after the data to find the end of packing mark */
    for(; num_pad_bytes > 0; num_pad_bytes -= commbufsiz){
	ER__(send_buffer_to_server(), i);
    }
  }

  return(0);
}

Int32
send_to_server(UChar * buf, Int32 num)
{
  UChar		*cptr;
  Int32		j, n;

  if(num == 0)
    return(0);

  cptr = buf;

  forever{
    while(cbufptr > commbufsiz){
	ER__(send_buffer_to_server(), j);

	cbufptr -= commbufsiz;

	memcpy(cbuf, cbuf + commbufsiz, cbufptr);
    }

    if(cbufptr + num <= commbufsiz){
      memcpy(cbuf + cbufptr, cptr, num * sizeof(UChar));

      cbufptr += num;

      return(0);
    }

    n = commbufsiz - cbufptr;
    if(n > 0){
      memcpy(cbuf + cbufptr, cptr, n * sizeof(UChar));
    }

    ER__(send_buffer_to_server(), j);

    cptr += n;
    num -= n;

    cbufptr = 0;
  }

  return(0);
}

Int32
bu_output(UChar * buf, Int32 num, AarParams * params)
{
  if(savefile)
    return(send_to_server(buf, num));

  params->vars.bytes_saved += (Real64) num;

  try_optimize_throughput();

  bytes_wr_unkn_pos += num;		/* assuming write will succeed */
  if(bytes_wr_unkn_pos > MAX_NUM_WRITE_WITHOUT_POS_UPDATE){
    filenum_valid = wrfilenum_valid = rdfilenum_valid = NO;
					/* force a tape position update */
    getwrcartandfile(params);		/* from time to time to avoid */
    filenum = wrfilenum;			/* falling behind too far */
    cart = wrcart;
  }

  return(send_to_server(buf, num));
}

Int32
receive_buffer_from_server()
{
  Int32		i, j;

  filenum_valid = wrfilenum_valid = rdfilenum_valid = NO;

  if(QUEUEDOP){
    i = append_to_queue(READFROMTAPE, cbuf, commbufsiz, NULL, 0);

    j = post_process_rest_of_queue(0);

    return(i ? i : j);
  }

  if(!savefile){
    if( (i = send_cmd(READFROMTAPE)) )
      return(i);
  }

  if((i = read_forced(commfd, cbuf, commbufsiz)) != commbufsiz)
    return(errno ? - errno : (savefile ? ENDOFFILEREACHED : PROTOCOL_ERROR));

  bytes_transferred += ESTIM_PROT_OVERHEAD + commbufsiz;

  return(result());
}

Int32
receive(UChar * buf, Int32 num, Int32 * num_read)
{
  static Int32	nbytes_in_inbuf = 0;
  static Flag	no_more_to_receive = NO;
  Int32		res, j;
  UChar		*cptr;

  if(num == 0){
    *num_read = 0;
    return(0);
  }

  cptr = buf;

  j = num;

  forever{
    if(cbufptr == 0){
	if(!endofrec){
	  res = receive_buffer_from_server();
	  no_more_to_receive = NO;
	}
	else{
	  no_more_to_receive = YES;
	  res = 0;
	}

	if(!res){
	  nbytes_in_inbuf = (no_more_to_receive ? 0 : commbufsiz);
	}
	else{
	  if(res < 0){
	    *num_read = num - j;
	    return(res);
	  }
	  else{
	    if(res == ENDOFFILEREACHED || res == ENDOFTAPEREACHED){
		getnumbytesvalid(&nbytes_in_inbuf);

		endofrec = 1;
	    }
	    else{
		*num_read = num - j;
		return(res);
	    }
	  }
	}
    }

    if(nbytes_in_inbuf - cbufptr <= j){
	if(cbufptr < nbytes_in_inbuf)
	  memcpy(cptr, cbuf + cbufptr, nbytes_in_inbuf - cbufptr);

	cptr += (nbytes_in_inbuf - cbufptr);
	j -= (nbytes_in_inbuf - cbufptr);
	cbufptr = 0;

	if(endofrec){
	  *num_read = num - j;
	  return(ENDOFFILEREACHED);
	}
	if(j == 0){
	  *num_read = num;
	  return(0);
	}
    }
    else{		/* in case of eof this is not really good code */
	if(j > 0)
	  memcpy(cptr, cbuf + cbufptr, j);

	cbufptr += j;
	*num_read = num;
	return(0);
    }
  }

  return(0);	/* NOTREACHED */
}

Int32
bu_input(UChar * buf, Int32 num, AarParams * params)
{
  Int32		i, r;

  i = receive(buf, num, &r);

  if(i && i != ENDOFFILEREACHED && i != ENDOFTAPEREACHED){
    fprintf(params->errfp,
	T_("%sError: unexpected fault receiving from server: %s. Trying to continue ...\n"),
		(i > 0 ? T_("Server ") : ""),
		(i > 0 ? fault_string(i) : (UChar *) strerror(-i)));
  }

  try_optimize_throughput();

  return(r);
}

Int32
getcartandfile(AarParams * params)
{
  static Flag	first_call = YES;
  static Int32	prev_cart = -1;

  UChar		buf[10];
  Int32		r = 0;

  if(filenum_valid)
    return(0);

  if(QUEUEDOP){
    r = append_to_queue(QUERYPOSITION, NULL, 7, NULL, 0);

    if(first_call){
	r = post_process_rest_of_queue(0);
    }
  }
  else{
    if( (r = send_cmd(QUERYPOSITION)) )
	return(r);

    if(read_forced(commfd, buf, 7) != 7)
	return(- errno);

    bytes_transferred += ESTIM_PROT_OVERHEAD + 7;

    if( (r = result()) )
	return(r);

    xref_to_UnsN(&cart, buf, 24);
    xref_to_Uns32(&filenum, buf + 3);
  }

  filenum_valid = YES;
  bytes_wr_unkn_pos = 0;

  if(first_call){
    params->vars.firstcart = cart;
    params->vars.firstfile = filenum;

    first_call = NO;
  }

  if(cart != prev_cart){
    params->vars.used_carts = add_to_Uns32Ranges(params->vars.used_carts,
					cart, cart);
    prev_cart = cart;
  }

  return(r);
}

Int32
getrdcartandfile(AarParams * params)
{
  static Flag	first_call = YES;
  static Int32	prev_cart = -1;

  UChar		buf[10];
  Int32		r = 0;

  if(rdfilenum_valid)
    return(0);

  if(QUEUEDOP){
    r = append_to_queue(QUERYRDPOSITION, NULL, 7, NULL, 0);

    if(first_call){
	r = post_process_rest_of_queue(0);
    }
  }
  else{
    if( (r = send_cmd(QUERYRDPOSITION)) )
	return(r);

    if(read_forced(commfd, buf, 7) != 7)
	return(- errno);

    bytes_transferred += ESTIM_PROT_OVERHEAD + 7;

    if( (r = result()) )
	return(r);

    xref_to_UnsN(&rdcart, buf, 24);
    xref_to_Uns32(&rdfilenum, buf + 3);
  }

  rdfilenum_valid = YES;

  if(first_call){
    params->vars.firstcart = rdcart;
    params->vars.firstfile = rdfilenum;

    first_call = NO;
  }

  if(cart != prev_cart){
    params->vars.used_carts = add_to_Uns32Ranges(params->vars.used_carts,
					rdcart, rdcart);
    prev_cart = rdcart;
  }

  return(r);
}

Int32
getwrcartandfile(AarParams * params)
{
  static Flag	first_call = YES;
  static Int32	prev_cart = -1;

  UChar		buf[10];
  Int32		r = 0;

  if(wrfilenum_valid)
    return(0);

  if(QUEUEDOP){
    r = append_to_queue(QUERYWRPOSITION, NULL, 7, NULL, 0);

    if(first_call)
	r = post_process_rest_of_queue(0);
  }
  else{
    if( (r = send_cmd(QUERYWRPOSITION)) )
	return(r);

    if(read_forced(commfd, buf, 7) != 7)
	return(- errno);

    bytes_transferred += ESTIM_PROT_OVERHEAD + 7;

    if( (r = result()) )
	return(r);

    xref_to_UnsN(&wrcart, buf, 24);
    xref_to_Uns32(&wrfilenum, buf + 3);
  }

  wrfilenum_valid = YES;
  bytes_wr_unkn_pos = 0;

  if(first_call){
    params->vars.firstcart = wrcart;
    params->vars.firstfile = wrfilenum;

    first_call = NO;
  }

  if(cart != prev_cart){
    params->vars.used_carts = add_to_Uns32Ranges(params->vars.used_carts,
					wrcart, wrcart);
    prev_cart = cart;
  }

  return(r);
}

Int32
getint(UChar cmd, Int32 * res, Int32 size, Flag wait_if_buffered)
{
  UChar		buf[10];
  Int32		i;

  if(QUEUEDOP){
    i = append_to_queue(cmd, NULL, size, NULL, 0);
    if(i)
	return(i);

    if(wait_if_buffered)
	return(post_process_rest_of_queue(0));

    return(i);
  }

  if( (i = send_cmd(cmd)) )
    return(i);

  if(read_forced(commfd, buf, size) != size)
    return(- errno);

  bytes_transferred += ESTIM_PROT_OVERHEAD + size;

  if( (i = result()) )
    return(i);

  xref_to_UnsN(res, buf, (size << 3));

  return(0);
}

Int32
getnumcarts(AarParams * params, Flag wait_if_buffered)
{
  static UChar	have_numcarts = 0;
  Int32		i;

  if(have_numcarts)
    return(0);

  i = getint(QUERYNUMCARTS, &numcarts, 3, wait_if_buffered);
  if(i)
    return(i);

  have_numcarts = 1;
  return(0);
}

Int32
gettapeblocksize(AarParams * params, Flag wait_if_buffered)
{
  return(getint(QUERYTAPEBLOCKSIZE, &tapeblocksize, 4, wait_if_buffered));
}

Int32
getcartset(AarParams * params, Flag wait_if_buffered)
{
  return(getint(QUERYCARTSET, &cartset, 3, wait_if_buffered));
}

Int32
getusedtapes(UChar ** retbuf, UChar * clientid)
{
  UChar		*cptr, buf[4];
  Int32		l, n;

  n = 1 + 1 + (l = strlen(clientid));

  cptr = NEWP(UChar, n);
  if(!cptr)
    return(-errno);

  cptr[0] = QUERYNEEDEDTAPES;
  cptr[1] = (UChar) l;
  memcpy(cptr + 2, clientid, l);

  if(write_forced(commfd, cptr, n) != n)
    return(- errno);

  bytes_transferred += ESTIM_PROT_OVERHEAD + n;

  if(read_forced(commfd, buf, 4) != 4)
    return(- errno);

  xref_to_Uns32(&l, buf);
  l++;
  cptr = RENEWP(cptr, UChar, l);
  if(!cptr)
    return(-errno);

  if(read_forced(commfd, cptr, l) != l)
    return(- errno);

  bytes_transferred += ESTIM_PROT_OVERHEAD + 4 + l;

  l--;
  n = (Int32) cptr[l];
  cptr[l] = '\0';

  if(l && (n == COMMAND_OK)){
    *retbuf = cptr;
  }
  else{
    *retbuf = NULL;
    free(cptr);
  }

  return(n);
}

Int32
getbufferwithcmd(UChar * buffer, Int32 len, Int32 cmd, Flag wait_if_buffered)
{
  Int32		r = 0;
  UChar		*rbuf = NULL;

  rbuf = NEWP(UChar, len);
  if(!rbuf){
    r = - errno;
    GETOUT;
  }

  if(QUEUEDOP){
    r = append_to_queue(cmd, rbuf, len, NULL, 0);
    if(r)
	GETOUT;

    if(wait_if_buffered)
	r = post_process_rest_of_queue(0);

    GETOUT;
  }

  if( (r = send_cmd(cmd)) )
    GETOUT;

  if(read_forced(commfd, rbuf, len) != len){
    r = - errno;
    GETOUT;
  }

  bytes_transferred += ESTIM_PROT_OVERHEAD + len;

  memcpy(buffer, rbuf, len * sizeof(UChar));

  r = result();

 getout:
  ZFREE(rbuf);

  return(r);
}

Int32
setcart(Int32 cartnum)
{
  UChar		buf[8], cmd;
  Int32		i;

  filenum_valid = rdfilenum_valid = NO;

  cmd = SETCARTRIDGE;
  if(cartnum < 0){
    cartnum = - cartnum;
    cmd = SETRAWCARTRIDGE;
  }

  buf[0] = cmd;
  UnsN_to_xref(buf + 1, cartnum, 24);

  reset_data_buffer;

  if(QUEUEDOP){
    i = append_to_queue(cmd, NULL, 0, buf + 1, 3);

    return(i);
  }

  if(write_forced(commfd, buf, 4) != 4)
    return(- errno);

  bytes_transferred += ESTIM_PROT_OVERHEAD + 4;

  if( (i = result()) )
    return(i);

  cart = cartnum;
  return(0);
}

Int32
setcartset(Int32 cset)
{
  UChar		buf[8];
  Int32		i;

  filenum_valid = wrfilenum_valid = rdfilenum_valid = NO;

  buf[0] = SETCARTSET;
  UnsN_to_xref(buf + 1, cset, 24);

  if(QUEUEDOP){
    i = append_to_queue(buf[0], NULL, 0, buf + 1, 3);

    return(i);
  }

  if(write_forced(commfd, buf, 4) != 4)
    return(- errno);

  bytes_transferred += ESTIM_PROT_OVERHEAD + 4;

  if( (i = result()) )
    return(i);

  cartset = cset;

  return(0);
}

Int32
setfilenum(Int32 filen)
{
  UChar		buf[8], cmd;
  Int32		i;

  cmd = SETFILE;
  if(filen < 0){
    filen = - filen;
    cmd = SETRAWFILE;
  }

  filenum_valid = wrfilenum_valid = rdfilenum_valid = NO;

  buf[0] = cmd;
  Uns32_to_xref(buf + 1, filen & 0x7fffffff);

  reset_data_buffer;

  if(QUEUEDOP){
    i = append_to_queue(cmd, NULL, 0, buf + 1, 4);

    return(i);
  }

  if(write_forced(commfd, buf, 5) != 5)
    return(- errno);

  bytes_transferred += ESTIM_PROT_OVERHEAD + 5;

  if( (i = result()) )
    return(i);

  filenum = filen;

  return(0);
}

Int32
sendint(UChar cmd, Int32 n, Int32 size)
{
  UChar		buf[8];
  Int32		i;

  buf[0] = cmd;
  UnsN_to_xref(buf + 1, n, size << 3);

  if(QUEUEDOP){
    i = append_to_queue(cmd, NULL, 0, buf + 1, size);

    return(i);
  }

  size++;
  if(write_forced(commfd, buf, size) != size)
    return(- errno);

  bytes_transferred += ESTIM_PROT_OVERHEAD + size;

  return(result());
}

Int32
send_commbufsiz(Int32 cbsize)
{
  return(sendint(SETCOMMBUFSIZ, cbsize, 4));
}

Int32
getnumbytesvalid(Int32 * numbytes)
{
  UChar		buf[8];
  Int32		i;

  if(QUEUEDOP){
    i = append_to_queue(GETNUMREADVALID, NULL, 0, buf + 1, 4);

    return(i);
  }

  if( (i = send_cmd(GETNUMREADVALID)) )
    return(i);

  if(read_forced(commfd, buf, 4) != 4)
    return(- errno);

  bytes_transferred += ESTIM_PROT_OVERHEAD + 4;

  xref_to_Uns32(numbytes, buf);

  return(result());
}

Int32
send_identity(UChar * idstr)
{
  UChar		buf[129];

  memset(buf, 0, 129 * sizeof(UChar));
  buf[0] = CLIENTIDENT;
  strcpy(buf + 1, idstr);

  if(write_forced(commfd, buf, 129) != 129)
    return(- errno);

  bytes_transferred += ESTIM_PROT_OVERHEAD + 129;

  return(result());
}

Int32
setnumbytesvalid(Int32 numbytes)
{
  UChar		buf[8];
  Int32		i;

  Uns32_to_xref(buf + 1, numbytes);

  if(QUEUEDOP){
    i = append_to_queue(SETNUMWRITEVALID, NULL, 0, buf + 1, 4);

    return(i);
  }

  buf[0] = SETNUMWRITEVALID;

  if(write_forced(commfd, buf, 5) != 5)
    return(- errno);

  bytes_transferred += ESTIM_PROT_OVERHEAD + 5;

  return(result());
}

Int32
skipfiles(Int32 numfiles)
{
  UChar		buf[8];
  Int32		i;

  filenum_valid = wrfilenum_valid = rdfilenum_valid = NO;

  buf[0] = SKIPFILES;
  Uns32_to_xref(buf + 1, numfiles & 0x7fffffff);
  
  reset_data_buffer;

  if(QUEUEDOP){
    i = append_to_queue(SKIPFILES, NULL, 0, buf + 1, 4);

    return(i);
  }

  if(write_forced(commfd, buf, 5) != 5)
    return(- errno);

  bytes_transferred += ESTIM_PROT_OVERHEAD + 5;

  return(result());
}

Int32
simple_command(UChar cmd)
{
  Int32		i;

  if(QUEUEDOP){
    i = append_to_queue(cmd, NULL, 0, NULL, 0);

    return(i);
  }

  if(write_forced(commfd, &cmd, 1) != 1)
    return(- errno);

  bytes_transferred += ESTIM_PROT_OVERHEAD + 1;

  return(result());
}

Int32
open_write(Int8 raw)
{
  Int32		i;
  UChar		cmd;

  filenum_valid = wrfilenum_valid = rdfilenum_valid = NO;

  cmd = (raw ? OPENFORRAWWRITE : OPENFORWRITE);

  endofrec = 0;

  reset_data_buffer;

  filenum = wrfilenum;
  cart = wrcart;

  if(QUEUEDOP)
    return(append_to_queue(cmd, NULL, 0, NULL, 0));

  reset_tape_io;

  bytes_wr_unkn_pos = 0;

  if( (i = send_cmd(cmd)) )
    return(i);

  return(result());
}

Int32
open_read(Int8 raw)
{
  Int32		i;
  UChar		cmd;

  filenum_valid = wrfilenum_valid = rdfilenum_valid = NO;

  cmd = (raw ? OPENFORRAWREAD : OPENFORREAD);

  endofrec = 0;

  reset_data_buffer;

  filenum = rdfilenum;
  cart = rdcart;

  if(QUEUEDOP)
    return(append_to_queue(cmd, NULL, 0, NULL, 0));

  reset_tape_io;

  if( (i = send_cmd(cmd)) )
    return(i);

  return(result());
}

Int32
closetape(Int8 rewind)
{
  Int32		i;
  UChar		cmd;

  cmd = (rewind ? CLOSETAPE : CLOSETAPEN);

  filenum_valid = wrfilenum_valid = rdfilenum_valid = NO;

  reset_data_buffer;

  if(QUEUEDOP){
    i = append_to_queue(cmd, NULL, 0, NULL, 0);

#if 0		/* don't waste time waiting for the tape */
    j = post_process_rest_of_queue(0);
#endif

    return(i);
  }

  reset_tape_io;

  if( (i = send_cmd(cmd)) )
    return(i);

  return(result());
}

Int32
request_newfile()
{
  Int32		i;

  filenum_valid = wrfilenum_valid = rdfilenum_valid = NO;

  if(QUEUEDOP)
    return(append_to_queue(REQUESTNEWFILE, NULL, 0, NULL, 0));

  if( (i = send_cmd(REQUESTNEWFILE)) )
    return(i);

  return(result());
}

Int32
request_newcart()
{
  Int32		i;

  filenum_valid = wrfilenum_valid = rdfilenum_valid = NO;

  if(QUEUEDOP)
    return(append_to_queue(REQUESTNEWCART, NULL, 0, NULL, 0));

  if( (i = send_cmd(REQUESTNEWCART)) )
    return(i);

  return(result());
}

Int32
erasetape()
{
  Int32		i;
  UChar		cmd;

  cmd = ERASETAPE;

  if(QUEUEDOP){
    i = append_to_queue(cmd, NULL, 0, NULL, 0);

    return(i);
  }

  if( (i = send_cmd(cmd)) )
    return(i);

  return(result());
}

UChar *
fault_string(Int32 code)
{
  Int32		i, sz;

  if(!code)
    return("");

  sz = sizeof(fault_messages) / sizeof(fault_messages[0]);

  for(i = 0; i < sz; i++){
    if(code == (Int32) fault_messages[i].code)
	return(fault_messages[i].msg);
  }

  return(T_("unknown fault"));
}


Int32
close_connection()
{
  Int32		i, j;

  filenum_valid = wrfilenum_valid = rdfilenum_valid = NO;

  reset_data_buffer;

  if(QUEUEDOP){
    i = append_to_queue(GOODBYE, NULL, 0, NULL, 0);

    j = post_process_rest_of_queue(0);

    return(i ? i : j);
  }

  if( (i = send_cmd(GOODBYE)) )
    return(i);

  return(result());
}

Int32
request_client_backup(UChar * cmdname)
{
  Int32		i;
  Uns32		n;
  UChar		*buf, lenstr[5], exitst, *cptr, *cptr2;

  n = strlen(cmdname);

  if(n == 0){
    errmsg(T_("Warning: Empty command supplied. Nothing happens"));
    return(0);
  }

  buf = strapp("...", cmdname);
  if(!buf)
    return(-errno);
  buf[0] = CLIENTBACKUP;
  buf[1] = n % 251;
  buf[2] = 1;		/* suppress output if queued */

  if(QUEUEDOP){
    i = append_to_queue(CLIENTBACKUP, cbuf, 5, buf + 1, n + 2);

    free(buf);

    return(i);
  }

  buf[2] = 0;
  i = n + 3;
  i = (write_forced(commfd, buf, i) != i);
  free(buf);

  bytes_transferred += ESTIM_PROT_OVERHEAD + i;

  if(i)
    return(i);

  if(read_forced(commfd, lenstr, 5) != 5)
    return(- errno);

  bytes_transferred += ESTIM_PROT_OVERHEAD + 5;

  exitst = lenstr[0];
  xref_to_Uns32(&n, lenstr + 1);

  buf = NEWP(UChar, n + 1);
  if(!buf){
    errmsg("Error: No memory, where absolutely needed");
  }

  if(read_forced(commfd, buf, n) != n)
    return(- errno);
  buf[n] = '\0';

  bytes_transferred += ESTIM_PROT_OVERHEAD + n;

  if(n > 0){
    cptr = buf;
    forever{
	if(!(*cptr))
	  break;
	cptr2 = strchr(cptr, '\n');
	if(cptr2)
	  *cptr2 = '\0';
	if(c_f_verbose)
	  fprintf(stdout, "%s: ", servername);

	fprintf(stdout, "%s\n", cptr);
	if(cptr2)
	  cptr = cptr2 + 1;
	else
	  break;
    }
  }
  free(buf);

  if(n < 1 && exitst){
    fprintf(stderr,
	T_(">>> Remote process exited with status %d, but produced no output.\n"),
	exitst);
  }

  return(result() | (exitst << 16));
}

Int32
get_message(UChar ** retmsg)
{
  Int32		i, n;
  UChar		c, *msgbuf = NULL, nbuf[4];

  c = GETCURMSG;
  if(write_forced(commfd, &c, 1) != 1)
    return(- errno);

  if(read_forced(commfd, nbuf, 4) != 4)
    return(- errno);

  xref_to_Uns32(&n, nbuf);
  msgbuf = NEWP(UChar, n + 1);
  if(!msgbuf)
    return(- errno);

  i = read_forced(commfd, msgbuf, n);
  if(i < 0)
	return(- errno);

  msgbuf[i] = '\0';

  *retmsg = msgbuf;

  return(result());
}

Int32
poll_messages()
{
  UChar		*msg, *cptr, *cptr2;
  time_t	msgtime, servertime, starttime;

  starttime = UNSPECIFIED_TIME;

  forever{
    while(!get_message(&msg)){
	cptr = strchr(msg, '\n');
	cptr2 = NULL;
	if(cptr)
	  cptr2 = strchr(++cptr, '\n');
	if(cptr2){
	  msgtime = strint2time(msg);
	  servertime = strint2time(cptr);

	  if(starttime == UNSPECIFIED_TIME){
	    starttime = servertime;
	  }

	  if(msgtime != UNSPECIFIED_TIME
			&& servertime != UNSPECIFIED_TIME
			&& msgtime >= starttime){
	    fprintf(stdout, "%s", ++cptr2);
	    fflush(stdout);

	    starttime = msgtime + 1;
	  }
	}

	free(msg);
	ms_sleep(bu_msg_itv * 1000);
    }
    ms_sleep(bu_msg_itv * 1000);
  }

  return(0);
}

UChar **
get_file_list(FILE * fp)
{
  UChar		*line;
  UChar		**filelist = NULL;
  Int32		num_files = 0;

  while(!feof(fp)){
    line = fget_alloc_str(fp);
    if(!line)
	continue;
    chop(line);

    filelist = ZRENEWP(filelist, UChar *, num_files + 2);
    if(!filelist){
	errmsg("Error: memory exhausted by the list of files");
	exit(54);
    }

    filelist[num_files] = line;
    num_files++;
    filelist[num_files] = NULL;
  }

  return(filelist);
}

void
pre_extended_verbose(UChar * str, AarParams * params)
{
  if(! savefile){
    if(bu_create){
	E__(getwrcartandfile(params));
	filenum = wrfilenum;
	cart = wrcart;
    }
    else{
	if(server_can_sendrdpos){
	  E__(getrdcartandfile(params));
	  filenum = rdfilenum;
	  cart = rdcart;
	}
	else{
	  E__(getcartandfile(params));
	  rdfilenum = filenum;	/* for backward compatibility. In open_read */
	  rdcart = cart;	/* rdxxxx must be set correctly */
	}
    }

    params->vars.actfile = filenum;
    params->vars.actcart = cart;
  }
}

void
extended_verbose(UChar * str, AarParams * params)
{
  static UChar	*lstr = NULL;	/* not multithreading safe */
  static Int32	nall = 0;

  FILE		*fp;
  UChar		mtimestr[30];
  Int32		l;

  fp = ((params->mode == MODE_CREATE && save_stdio) ?
			params->errfp : params->outfp);

  if(verbose){
    if(verbose & VERBOSE_LOCATION)
	fprintf(fp, "%s%s%d%s", servername, PORTSEP, (int) portnum, LOCSEP);

    if(verbose & VERBOSE_CART_FILE)
	fprintf(fp, "%d.%d", (int) params->vars.actcart,
			(int) params->vars.actfile);

    if(verbose & VERBOSE_UID)
	fprintf(fp, UIDSEP "%d", (int) params->vars.uid);

    if(verbose & VERBOSE_MTIME){
	time_t_to_intstr(params->vars.mtime, mtimestr);
	fprintf(fp, MTIMESEP "%s", mtimestr);
    }

    fprintf(fp, ": ");
  }

  l = strlen(str) + 1;
  if(l > nall)
    lstr = ZRENEWP(lstr, UChar, nall = l);
  if(!lstr){
    errmsg("Error: No memory, where absolutely needed");
    return;
  }

  strcpy(lstr, str);
  rm_backspace(lstr);

  fprintf(fp, "%s", lstr);
}

void
normal_verbose(UChar * str, AarParams * params)
{
  static UChar	*lstr = NULL;	/* not multithreading safe */
  static Int32	nall = 0;

  FILE		*fp;
  Int32		l;

  l = strlen(str) + 1;
  if(l > nall)
    lstr = ZRENEWP(lstr, UChar, nall = l);
  if(!lstr){
    errmsg("Error: No memory, where absolutely needed");
    return;
  }

  strcpy(lstr, str);
  rm_backspace(lstr);

  fp = ((params->mode == MODE_CREATE && save_stdio) ?
			params->errfp : params->outfp);

  fprintf(fp, "%s", lstr);
}

void
write_reportfile(UChar * filename, AarParams * params, Int32 rc)
{
  FILE		*fp;
  UChar		numbuf[50];
  Int32		i;

  i = unlink(filename);
  if(i < 0 && errno && errno != ENOENT){
    errmsg(T_("Error: Cannot remove report file `%s'\n"), filename);
    return;
  }

  fp = fopen(filename, "w");
  if(!fp){
    errmsg(T_("Error: Cannot write report file `%s'\n"), filename);
    return;
  }

  fprintf(fp, T_("Backup statistics:\n"));
  fprintf(fp, "==================\n\n");
  if(identitystr)
    fprintf(fp, T_("Client Identifier:             %s\n\n"), identitystr);
  fprintf(fp, T_("Exit Status:                   %ld\n\n"), (long int) rc);
  fprintf(fp, T_("Start-Time:                    %s\n"), params->vars.startdate);
  fprintf(fp, T_("End-Time:                      %s\n\n"), params->vars.enddate);
  fprintf(fp, T_("Backup Server:                 %s\n"), servername);
  fprintf(fp, T_("Backup Port:                   %d\n"), (int) portnum);
  fprintf(fp, T_("First Cartridge used:          %ld\n"), (long int) params->vars.firstcart);
  fprintf(fp, T_("First Tapefile accessed:       %ld\n"), (long int) params->vars.firstfile);
  fprintf(fp, T_("Last Cartridge used:           %ld\n"), (long int) params->vars.lastcart);
  fprintf(fp, T_("Last Tapefile accessed:        %ld\n"), (long int) params->vars.lastfile);
  fprintf(fp, "%-31s", T_("Used Cartridges:"));
  fprint_Uns32Ranges(fp, params->vars.used_carts, 0); fprintf(fp, "\n\n");
  fprintf(fp, T_("Stored filesystem entries:     %ld\n"), (long int) params->vars.num_fsentries);
  fprintf(fp, T_("Bytes written to media:        %s\n"),
		Real64_to_intstr(params->vars.bytes_saved, numbuf));
  fprintf(fp, T_("Sum of filesizes:              %s\n"),
		Real64_to_intstr(params->vars.sum_filesizes, numbuf));
  fprintf(fp, T_("Sum of compressed filesizes:   %s\n"),
		Real64_to_intstr(params->vars.sum_compr_filesizes, numbuf));
  fprintf(fp, T_("Sum of uncompressed filesizes: %s\n"),
		Real64_to_intstr(params->vars.sum_uncompr_filesizes, numbuf));
  if(params->vars.sum_compr_filesizes > 0){
    fprintf(fp, T_("Total compression factor:      %.2f\n"),
		(float) params->vars.sum_filesizes /
		(float) (params->vars.sum_compr_filesizes
				+ params->vars.sum_uncompr_filesizes));
    fprintf(fp, T_("Real compression factor:       %.2f\n"),
		(float) (params->vars.sum_filesizes -
				params->vars.sum_uncompr_filesizes) /
		(float) (params->vars.sum_compr_filesizes));
  }
  fprintf(fp, "\n");

  fclose(fp);
}

Int32		/* buffered mode not supported for this function */
add_written_tapes(Uns32Range ** alltapes)
{
  Uns32Range	*newtapes = NULL;
  UChar		*instr = NULL, buf[4];
  Int32		i, r = 0;

  if( (i = send_cmd(QUERYWRITTENTAPES)) )
    return(i);

  if(read_forced(commfd, buf, 4) != 4)
    return(- errno);

  bytes_transferred += ESTIM_PROT_OVERHEAD + 4;

  xref_to_Uns32(&i, buf);
  if(!i)
    return(result());

  instr = NEWP(UChar, i + 1);
  if(!instr){
    r = - errno;
    GETOUT;
  }

  if( (r = (read_forced(commfd, instr, i) != i ? - errno : 0)) )
    GETOUT;
  instr[i] = '\0';

  bytes_transferred += ESTIM_PROT_OVERHEAD + i;

  if(!(newtapes = sscan_Uns32Ranges(instr, 0, 0, NULL, NULL))){
    r = - errno;
    GETOUT;
  }

  if(alltapes)
    r = merge_Uns32Ranges(alltapes, newtapes);
  else{
    *alltapes = newtapes;
    newtapes = NULL;
  }

 getout:
  ZFREE(instr);
  ZFREE(newtapes);

  i = result();

  return(r ? r : i);
}

Int32
create_command_queue()
{
    Int32	required_entry_mem, i;
#ifndef	_WIN32
    struct rlimit	rlim;
#endif

    max_outarglen = max_inresultlen = commbufsiz + 4;
    required_entry_mem = max_outarglen + max_inresultlen + sizeof(QueueEntry);

#ifndef	_WIN32
    i = getrlimit(RLIMIT_STACK, &rlim);
    if(i){
	errmsg("Error: cannot get maximum available memory");
    }
    queuelen = rlim.rlim_cur / required_entry_mem / 4;
#else
    queuelen = 1000000 / required_entry_mem / 4;
#endif

    if(queuelen < 1){
	comm_mode = SERIAL_OPERATION;
    }
    else{
	if(required_entry_mem * queuelen > MAX_QUEUE_MEMORY){
	  queuelen = MAX_QUEUE_MEMORY / required_entry_mem;
	}

	if(max_outarglen * queuelen < 100000){
	  comm_mode = SERIAL_OPERATION;
	}
	else{
	  allocated_outmem = max_outarglen * queuelen;
	  allocated_inmem = max_inresultlen * queuelen;
	  outbufmem = NEWP(UChar, allocated_outmem);
	  inbufmem = NEWP(UChar, allocated_inmem);
	  queue_entries = NEWP(QueueEntry, queuelen);

	  if(!outbufmem || !inbufmem || !queue_entries){
	    errmsg("Error: Strange. Can't get memory. Disabling queued operation");
	    comm_mode = SERIAL_OPERATION;
	    allocated_outmem = allocated_inmem = 0;
	    ZFREE(outbufmem);
	    ZFREE(inbufmem);
	    ZFREE(queue_entries);
	  }
	  else{
	    params.readfunc = queued_read;
	    params.writefunc = queued_write;

	    memset(queue_entries, 0, queuelen * sizeof(QueueEntry));
	  }
	}
    }

    max_queuelen = queuelen;
    queueent_done_idx = queueent_requested_idx = queueent_processed_idx
			= queue_insert_idx = 0;
    queue_commbufsiz = commbufsiz;

    return(0);
}

Int32
delete_command_queue()
{
  Int32		r = 0;

  if(QUEUEDOP)
    r = post_process_rest_of_queue(0);

  comm_mode = SERIAL_OPERATION;

  ZFREE(outbufmem);
  ZFREE(inbufmem);
  ZFREE(queue_entries);

  queuelen = 0;
  allocated_outmem = allocated_inmem = 0;

  params.readfunc = read_forced;
  params.writefunc = write_forced;

  return(r);
}

void
give_help()
{	/* automatically generated: */
{
char *l[335];
int i;
l[0]=T_("Description");
l[1]=T_("===========");
l[2]="";
l[3]=T_("This program is used to maintain archives on a backup server");
l[4]=T_("host or in a file. Archives can be created, extracted or their");
l[5]=T_("contents be listed. At least one of the following flags has to");
l[6]=T_("be supplied:");
l[7]="";
l[8]=T_(" -c  to create an archive");
l[9]="";
l[10]=T_(" -x  to extract from an archive");
l[11]="";
l[12]=T_(" -t  to list the contents of an archive");
l[13]="";
l[14]=T_(" -d  to verify (compare) the contents of an archive");
l[15]="";
l[16]=T_(" -C  to set a certain cartridge on the backup server");
l[17]=T_("       (makes only sense extracting or listing with -x or");
l[18]=T_("        -t, the writing position can't be changed by clients)");
l[19]="";
l[20]=T_(" -F  to set a certain file on the backup server's tape");
l[21]=T_("       (the same applies as for -C)");
l[22]="";
l[23]=T_(" -q  to printout the current cartridge and tape file number");
l[24]=T_("       on the backup server");
l[25]="";
l[26]=T_(" -Q  to printout the cartridge and tape file number for the");
l[27]=T_("       the next write access on the backup server");
l[28]="";
l[29]=T_(" -X  followed by the full path name of a program to be started on");
l[30]=T_("       the client. This can be used to trigger a backup remotely.");
l[31]=T_("       If the program needs arguments, the command together with");
l[32]=T_("       the arguments has to be enclosed by quotes");
l[33]="";
l[34]=T_(" -I  to printout an index of the backups written to the current");
l[35]=T_("       cartridge");
l[36]="";
l[37]=T_(" -w  to check the status of the streamer on the server side, e.g.");
l[38]=T_("       whether it is ready and waiting for requests to service");
l[39]="";
l[40]=T_(" -G  to request a new cartridge for the next writing operation.");
l[41]=T_("       If the current writing position is already at the beginning");
l[42]=T_("       of a new or reused tape, nothing happens");
l[43]="";
l[44]=T_(" -D <destination> to make an exact copy of a tape to another one");
l[45]=T_("       (duplicate). See below how to specify the destination tape.");
l[46]=T_("       Duplication can be either from one cartridge to another on");
l[47]=T_("       the same server, or from one server to another one. When");
l[48]=T_("       copying to the same server chunks of data are stored in a");
l[49]=T_("       temporary directory on the client, where the command is");
l[50]=T_("       started, what should preferably be the source server");
l[51]="";
l[52]=T_(" -M <message> send a message to the server. Messages will in the");
l[53]=T_("       most cases contain whitespace, so they should be enclosed");
l[54]=T_("       in quotes. Server messages should be sent to the single");
l[55]=T_("       stream server (port), the multi stream server might hang");
l[56]=T_("       receiving a message due to systematical reasons. Several");
l[57]=T_("       messages can be put into the string. They must be separated");
l[58]=T_("       by a real newline character or the usual C-like \\n .");
l[59]=T_("       The following messages are currently supported:");
l[60]="";
l[61]=T_("        PreciousTapes: <client-id> <list-of-tapes>");
l[62]=T_("                   The list of tapes is inserted into the table");
l[63]=T_("                   with the tapes, that are crucial for clients");
l[64]=T_("                   to restore all files, that are listed in all");
l[65]=T_("                   existing index files. The list is assigned to");
l[66]=T_("                   the client with the given client identifier,");
l[67]=T_("                   regardless of an id suppied with option -W .");
l[68]=T_("                   These tapes will not be overwritten until it");
l[69]=T_("                   is explicitly permitted. This message is sent");
l[70]=T_("                   automatically by full_backup or incr_backup");
l[71]=T_("                   and should not be used in other user contexts");
l[72]="";
l[73]=T_("        ReuseTapes: <list-of-tapes>");
l[74]=T_("                   The opposite of PreciousTapes. Sending this");
l[75]=T_("                   message permits the server to overwrite the");
l[76]=T_("                   listed tapes, though they are crucial for");
l[77]=T_("                   some client");
l[78]="";
l[79]=T_("        TapesReadOnly: <list-of-tapes>");
l[80]=T_("                   The list of tapes is inserted into the file");
l[81]=T_("                   listing the files, that should not be written");
l[82]=T_("                   any more for whatever reason");
l[83]="";
l[84]=T_("        TapesReadWrite: <list-of-tapes>");
l[85]=T_("                   This reverts the status of tapes set read-only");
l[86]=T_("                   to read-write, the opposite of TapesReadOnly");
l[87]="";
l[88]=T_("        CartridgeReady");
l[89]=T_("                   When an operator is requested to do something");
l[90]=T_("                   the server is waiting for, this message can be");
l[91]=T_("                   sent to trigger the server to proceed. This");
l[92]=T_("                   message has the same effect as the cartready");
l[93]=T_("                   command");
l[94]="";
l[95]=T_("        DeleteClient: <client-identifier>");
l[96]=T_("                   The tapes, that are marked as reserved for a");
l[97]=T_("                   client to recover all the data in his indexes,");
l[98]=T_("                   are freed. That is, the appropriate line is");
l[99]=T_("                   removed from the server's precious_tapes file");
l[100]="";
l[101]=T_(" -m [ <interval> ] poll the current message from the server and");
l[102]=T_("       write it to standard output, if it is new. Polling is");
l[103]=T_("       done regularly with the given interval in seconds, default");
l[104]=T_("       interval is 3 seconds");
l[105]="";
l[106]=T_("-c, -x, -t, -d, -X, -D, -I and -m are mutual exclusive. The other");
l[107]=T_("options can be supplied as needed. To set the cartridge and/or");
l[108]=T_("the tape file on the backup server is only making sense when not");
l[109]=T_("creating an archive. The serial order of writing to tape is");
l[110]=T_("handled by the server machine independently of the client.");
l[111]="";
l[112]="";
l[113]=T_("Filenames");
l[114]="";
l[115]=T_("The names of the files and directories, that have to be put");
l[116]=T_("into or extracted from an archive are by default read from the");
l[117]=T_("standard input. If filenames are supplied in the command line");
l[118]=T_("the -a flag is given when extracting, standard input is not read.");
l[119]=T_("The same applies, when filenames are read from a file with the");
l[120]=T_("-T option. When reading the names from a file or from standard");
l[121]=T_("input, they must be given one per line. If a name contains");
l[122]=T_("special characters (like newline or nonprintable ones), they");
l[123]=T_("have to be specified using backslash-sequences like in C-code,");
l[124]=T_("e.g. \\n for newline.");
l[125]=T_("In save mode (-c) filenames can be prefixed with character");
l[126]=T_("sequences, that have special meanings (no space between prefix");
l[127]=T_("and filename):");
l[128]="";
l[129]=T_(" /../   The file is not saved with all attributes present in");
l[130]=T_("        the inode, but only the contents are saved. This might");
l[131]=T_("        be useful for saving raw-devices");
l[132]=T_(" //../  With /../ the configured processing is not applied to");
l[133]=T_("        the file contents for safety reasons. With this prefix");
l[134]=T_("        processing can be forced nonetheless");
l[135]=T_(" |||    and a mandatory space character indicates, that the");
l[136]=T_("        following characters up to (but not including) another");
l[137]=T_("        triple bar ||| should be interpreted as a shell command,");
l[138]=T_("        that is started and whose standard output is written to");
l[139]=T_("        the backup. At restore time the command following the");
l[140]=T_("        second triple bar is started and the data stream read");
l[141]=T_("        at backup time is written to it's standard input. This");
l[142]=T_("        might be useful for saving e.g. databases. The second");
l[143]=T_("        command may be terminated by a triple sharp ###, that");
l[144]=T_("        starts an optional comment. Example:");
l[145]=T_("        ||| pg_dumpall ||| psql db_tmpl ### Store Postgres DBs");
l[146]="";
l[147]="";
l[148]=T_("Destination");
l[149]="";
l[150]=T_("The destination tape for the duplicate operation can be given");
l[151]=T_("in two ways: either with the options -h, -p, -C and -k following");
l[152]=T_("the -D immediately without space and enclosed in quotes, so that");
l[153]=T_("they appear as an own argument list in one real argument, e.g.:");
l[154]=T_(" -D' -C 5 -h targethost -p targetport'");
l[155]=T_("(double quotes are of course also possible ...).");
l[156]=T_("The second format is as follows:");
l[157]=T_(" [<targetcart>][@<targethost>][%<targetport>][:<targetcryptkeyfile>]");
l[158]=T_("At least one of the specifiers must be present. Examples:");
l[159]=T_(" 5@otherhost  5%2990:/keyfile/for/target/server @otherhost%2970");
l[160]=T_("If one of the specifiers is omitted, it is assumed identical with");
l[161]=T_("the copy source specified in the normal options -h, -p, -C and -k.");
l[162]=T_("Copying a tape to itself is prevented.");
l[163]="";
l[164]="";
l[165]=T_("More options in alphabetical order:");
l[166]="";
l[167]=T_(" -            in combination with -c: read standard input and");
l[168]=T_("                write it to tape, in combination with -x: read");
l[169]=T_("                tape and write it to standard output");
l[170]="";
l[171]=T_(" -A <time>    process files (save or extract) modified after");
l[172]=T_("                the given time in seconds since 1.1.1970 00:00");
l[173]="";
l[174]=T_(" -a           in combination with -x: extract all files and");
l[175]=T_("                directories in the archive");
l[176]="";
l[177]=T_(" -B <time>    process files (save or extract) modified before");
l[178]=T_("                the given time in seconds since 1.1.1970 00:00");
l[179]="";
l[180]=T_(" -b           don't enter buffering mode");
l[181]="";
l[182]=T_(" -e <errlog>  Use the file <errlog> to write error messages to");
l[183]=T_("                instead of the standard error output");
l[184]="";
l[185]=T_(" -f <file>    write to or read from a file instead of querying");
l[186]=T_("                the backup server");
l[187]="";
l[188]=T_(" -g           while extracting/reading: ignore leading garbage,");
l[189]=T_("                suppress error messages at the beginning. This");
l[190]=T_("                is useful when extracting from tape files, that");
l[191]=T_("                are not the first ones of a whole archive.");
l[192]="";
l[193]=T_(" -H <header>  put the supplied informational header to the begin");
l[194]=T_("                of the backup. If a - is supplied (no space may");
l[195]=T_("                follow -H i.e. -H-) the information is read from");
l[196]=T_("                the first line of stdin. Backslash sequences of");
l[197]=T_("                C-like style are replaced");
l[198]="";
l[199]=T_(" -h <host>    use the backup server with the name <host>");
l[200]=T_("                default host is the machine with the name");
l[201]=T_("                backuphost");
l[202]="";
l[203]=T_(" -i           while extracting: ignore the stored ownership and");
l[204]=T_("                do not restore it");
l[205]="";
l[206]=T_(" -j           when starting to write: request starting a new");
l[207]=T_("                tape file");
l[208]="";
l[209]=T_(" -K           when packing, do not keep the access time of the");
l[210]=T_("                file. By default after packing a filesystem entry");
l[211]=T_("                it's previous atime is restored");
l[212]="";
l[213]=T_(" -k <file>    use the contents of the given file as encryption");
l[214]=T_("                key for authenticating to the server");
l[215]="";
l[216]=T_(" -l           for each packed or unpacked filename, if sending");
l[217]=T_("                to or receiving from a backup server in verbose");
l[218]=T_("                   mode in combination with -n:");
l[219]=T_("                printout server name and port number at the");
l[220]=T_("                beginning of the line, e.g.: orion%2988!");
l[221]="";
l[222]=T_(" -N <file>    while archiving: ignore files with a modification");
l[223]=T_("                time before the one of the given file, only save");
l[224]=T_("                newer files or such with the same age in seconds");
l[225]="";
l[226]=T_(" -n           for each packed or unpacked filename, if sending");
l[227]=T_("                to or receiving from a backup server in verbose");
l[228]=T_("                   mode:");
l[229]=T_("                printout cartridge and tape file number at the");
l[230]=T_("                beginning of the line, e. g.: 7.15: <filename>");
l[231]=T_("                In combination with -X: precede each line of");
l[232]=T_("                output received from the remotely started program");
l[233]=T_("                with the identifier of the remote host and a");
l[234]=T_("                colon, e. g.:  darkstar: Full backup finished.");
l[235]="";
l[236]=T_(" -O           for each packed file creating a backup in verbose");
l[237]=T_("                mode: printout the user-ID of the file owner at");
l[238]=T_("                the beginning of the line prefixed with a bar |");
l[239]=T_("                eventually behind cartridge and file number");
l[240]="";
l[241]=T_(" -o <uid>     archive or extract only files owned by the user");
l[242]=T_("                with the given user-ID (an integer)");
l[243]="";
l[244]=T_(" -p <portno>  use a different port number for communicating with");
l[245]=T_("                the backup server. Default is TCP-Port 2988");
l[246]="";
l[247]=T_(" -R           pack or extract directories recursively with all");
l[248]=T_("                of their contents");
l[249]="";
l[250]=T_(" -r           use filenames relative to the current directory,");
l[251]=T_("                whether they start with a slash or not. If -r");
l[252]=T_("                is given more then 1 time, also let symlinks");
l[253]=T_("                originally pointing to absolute paths now point");
l[254]=T_("                to paths relative to the directory, where the");
l[255]=T_("                symlink will be created. If given twice, the");
l[256]=T_("                current directory is assumed to be the relative");
l[257]=T_("                root directory for the symbolic link target.");
l[258]=T_("                If given three times, the root directory of the");
l[259]=T_("                current process is used as the relative root");
l[260]=T_("                directory of the symbolic link targets");
l[261]="";
l[262]=T_(" -S <cartset> The cartridge set to use, where <cartset> is the");
l[263]=T_("                number of a valid cartridge set on the server");
l[264]=T_("                side. Default is 1. This option makes sense only");
l[265]=T_("                when creating backups with -c");
l[266]="";
l[267]=T_(" -s <filepat> do not attempt processing on files matching the");
l[268]=T_("                given filename pattern. This parameter may");
l[269]=T_("                appear several times");
l[270]="";
l[271]=T_(" -T <file/dir> read the filenames to process from the <file>.");
l[272]=T_("                The filenames must be separated by whitespace.");
l[273]=T_("                If whitespace is part of a filename, it has to");
l[274]=T_("                be enclosed by double quotes. Double quotes or");
l[275]=T_("                backslashes within the filename have to be");
l[276]=T_("                preceded by a backslash. In combination with");
l[277]=T_("                -D: the tape files to be copied are temporarily");
l[278]=T_("                stored in the given directory instead of the");
l[279]=T_("                default directory /tmp");
l[280]="";
l[281]=T_(" -U           for each packed file creating a backup in verbose");
l[282]=T_("                mode: printout the modification time of the file");
l[283]=T_("                in seconds since 1970/1/1 0:00 at the beginning");
l[284]=T_("                of the line prefixed with a tilde ~ eventually");
l[285]=T_("                behind cartridge number, file number and owner");
l[286]="";
l[287]=T_(" -u           while extracting: remove existing files with the");
l[288]=T_("                same name as found in the archive. Otherwise");
l[289]=T_("                no existing files are overwritten");
l[290]="";
l[291]=T_(" -V <file>    write a report containing statistics at the end of");
l[292]=T_("                a backup to the <file>");
l[293]="";
l[294]=T_(" -v           verbose mode: print the filenames while creating");
l[295]=T_("                or extracting, be a little more verbose while");
l[296]=T_("                listing contents. If -v is the only given flag:");
l[297]=T_("                print out software name and version");
l[298]="";
l[299]=T_(" -W <id>      identify as <id> to the server. This is needed when");
l[300]=T_("                connecting a multi-stream server to distinguish");
l[301]=T_("                between the clients. Default is the string");
l[302]=T_("                \"<client-program>\"");
l[303]="";
l[304]=T_(" -z <z> <uz>  use <z> as the command, that is used to process");
l[305]=T_("                files, <uz> for the corresponding unprocess.");
l[306]=T_("                The command has to read from stdin and to write");
l[307]=T_("                to stdout. If arguments have to be supplied to");
l[308]=T_("                <z> and/or <uz>, don't forget to use quotes. If");
l[309]=T_("                built-in compression is desired, the command for");
l[310]=T_("                processing has to start with a dot (.), followed");
l[311]=T_("                by a space and a number ranging from 1 to 9, that");
l[312]=T_("                specifies the compression level. If an additional");
l[313]=T_("                external command should process the data, it may");
l[314]=T_("                follow, separated from the compression level by");
l[315]=T_("                whitespace. The order of processing is: First the");
l[316]=T_("                external program processes the data, then built-in");
l[317]=T_("                compression is applied. An empty string has to be");
l[318]=T_("                supplied for <uz> (or any other dummy is ok), if");
l[319]=T_("                only built-in compression is desired.");
l[320]=T_("                Examples for <z>:");
l[321]=T_("                 gzip       (run external command gzip),");
l[322]=T_("                 \"gzip -2\"  (the same with an argument),");
l[323]=T_("                 \". 8\"      (only built-in compression level 8),");
l[324]=T_("                 \". 3 __descrpt -k /my/key\" (run command __descrpt");
l[325]=T_("                            and apply built-in compression level 3)");
l[326]="";
l[327]=T_(" -Z           while printing out the contents: check those files");
l[328]=T_("                in the archive that are processed for integrity.");
l[329]=T_("                While creating an archive: write a CRC32 checksum");
l[330]=T_("                for each file, file contents or command output to");
l[331]=T_("                the backup stream");
l[332]="";
l[333]="";
l[334]=T_(" -?           to printout this text");
for(i=0;i<335;i++)
fprintf(stdout,"%s\n",l[i]);
}

  exit(0);
}

void
usage(UChar * prnam)
{
  prnam = FN_BASENAME(prnam);

  errmsg(T_("usage: %s -{c|x|t|d|I|D<dest>|M<msg>|m[<poll-interval>]} \\\n"), prnam);
  errmsg(T_("               [ -[RraunlOUvgiqQwZbGK] ] \\\n"));
  errmsg(T_("               [ -h <backup-server> ] \\\n"));
  errmsg(T_("               [ -z <proccmd> <unproccmd> ] \\\n"));
  errmsg(T_("               [ -T <to-extract-filename> ] \\\n"));
  errmsg(T_("               [ -C <cartridge-number> ] \\\n"));
  errmsg(T_("               [ -F <filenumber-on-tape> ] \\\n"));
  errmsg(T_("               [ -f <archive-filename> ] \\\n"));
  errmsg(T_("               [ -e <errorlog-filename> ] \\\n"));
  errmsg(T_("               [ -p <server-port-number> ] \\\n"));
  errmsg(T_("               [ -N <newer-than-filename> ] \\\n"));
  errmsg(T_("               [ -o <user-ID> ] \\\n"));
  errmsg(T_("               [ -W <client-identifier> ] \\\n"));
  errmsg(T_("               [ -k <encryption-key-file> ] \\\n"));
  errmsg(T_("               [ -s <dont-process-filepattern> [ -s ... ] ] \\\n"));
  errmsg(T_("               [ -V <statistics-report-file> ] \\\n"));
  errmsg(T_("               [ -A <after-time-seconds> ] \\\n"));
  errmsg(T_("               [ -B <before-time-seconds> ] \\\n"));
  errmsg(T_("               [ <files> <directories> ... ]\n"));
  errmsg(T_("\n       %s -X <program> \\\n"), prnam);
  errmsg(T_("               [ -h <backup-client> ]\n"));
  errmsg(T_("\n       %s -\\?  (to get help)\n"), prnam);

  exit(1);
}

static Int32
start_server_comm(Int32 timeout)
{
  Int32		i;
  time_t	starttime;
  UChar		cmd;

  starttime = time(NULL);

  fcntl(commfd, F_SETFD, fcntl(commfd, F_GETFD) | 1);

  forever{
    i = logon_to_server(commfd, commfd, GREETING_MESSAGE,
			VERSION_MESSAGE_KEY, &serverversion,
#ifdef	HAVE_DES_ENCRYPTION
			AUTH_USE_DES
#else
			0
#endif
			);
    if(!i)
	break;

    if((i && time(NULL) - starttime > timeout) || i == AUTHENTICATION)
	return(i);

    ms_sleep(drandom() * 400 + 100);
  }

  if(!serverversion)
	serverversion = LAST_SERVER_WITHOUT_VERSIONMSG;

  if(compare_version_strings(serverversion, "3.2") >= 0)
	server_can_sendtbufsiz = YES;
  if(compare_version_strings(serverversion, "3.2.4") >= 0)
	server_can_setcbufsiz = server_can_sendid = YES;
  if(compare_version_strings(serverversion, "3.2.7") >= 0)
	server_can_sendutapes = YES;
  if(compare_version_strings(serverversion, "3.3") >= 0)
	server_can_userid = YES;
  if(compare_version_strings(serverversion, "3.3.1") >= 0)
	server_can_auth = YES;
  if(compare_version_strings(serverversion, "3.3.6") >= 0)
	server_can_sendrdpos = YES;
  if(compare_version_strings(serverversion, "3.3.8") >= 0)
	server_can_sendmsgs = YES;

  if(server_can_auth){
    cmd = AUTHENTICATE;
    if(write_forced(commfd, &cmd, 1) != 1)
	i = AUTHENTICATION;
    else
	i = authenticate_client(commfd, commfd, NO, "", NO, AUTHENTICATION,
#ifdef	HAVE_DES_ENCRYPTION
			AUTH_USE_DES
#else
			0
#endif
			| AUTH_NOCONFIRM, 0);

    if(i){
	errmsg(T_("Error: Server did not proof authentication correctly. "));
	errmsg(T_("Possible authentication key spoofing.\n"));

	return(i);
    }
  }

  return(0);
}

main(int argc, char ** argv)
{
  struct hostent	*he;
  Int32		i, j, n;
  Int32		gexitst = 0;
  struct stat	statb;
  FILE		*fp;
  UChar		c, buf[BUFFERSIZ], *line;
  UChar		givehelp = 0;
  UChar		querypos = 0;
  UChar		querywrpos = 0;
  UChar		*req_portstr = NULL;
  UChar		**filelist = NULL;
  UChar		**files = NULL;
  UChar		*found_files = NULL;
  UChar		*uidstr = NULL;
  Int32		s_cart = -1;
  Int32		s_file = -1;
  Int32		s_cset = 1;
  Int32		num_unused = 0;
  UChar		*newer_than_file = NULL;
  UChar		**unused_args = NULL;
  UChar		*cptr = NULL;
  UChar		*reportfile = NULL;
  UChar		*older_than_sec = NULL;
  UChar		*newer_than_sec = NULL;
  int		i1, i2, n1;
  int		eexitst = 15;
  time_t	timemem;
  Flag		uxsock = NO;
  Flag		opened_write = NO;

#ifdef ENABLE_NLS
  setlocale (LC_ALL, "");
  bindtextdomain (PACKAGE, LOCALEDIR);
  textdomain (PACKAGE);
#endif
  
  gargv = (UChar **) argv;

  aarparams_init(&params);

  params.program = find_program(argv[0]);

  strcpy(params.vars.startdate, actimestr());

  sigemptyset(&params.blocked_signals);
  sigprocmask(SIG_BLOCK, &params.blocked_signals, NULL);

  if(sizeof(Uns32) > 4){
    fprintf(stderr, T_("Error: long int is wider than 4 Bytes. Inform author about machine type.\n"));
    exit(3);
  }

  n = -1;
  i = goptions(-argc, (UChar **) argv,
	"b:c;b:x;b:t;b:d;b:I;b:w;s:D;b:R;c:r;b:v;b:n;b:O;b:l;b:u;b:a;s:h;b:;"
	"s2:z;s:M;s:T;s:p;s:e;s:f;i:C;i:F;i:S;b:g;b:i;b:?;b:q;b:Q;b:J;*;b:Z;"
	"s:X;s:N;s:o;s:k;b:b;s:s;s:V;s:B;s:A;b:j;b:G;b:E;s:H;s:W;b:U;b:K;"
	"i-1:m",
			&bu_create, &bu_extract, &bu_contents, &bu_verify,
			&bu_index, &bu_ready, &bu_duptapeargs,
			&params.recursive, &params.relative,
			&verbose, &c_f_verbose, &o_verbose, &l_verbose,
			&params.unlink, &allfiles, &servername, &std_io,
			&params.zipcmd, &params.unzipcmd, &servermsg,
			&toextractfilename, &req_portstr, &errorlogfile,
			&savefile, &s_cart, &s_file, &s_cset,
			&params.skip_garbage, &params.ignoreown,
			&givehelp, &querypos, &querywrpos, &bu_junction,
			&num_unused, &unused_args, &params.check,
			&clientprog, &newer_than_file, &uidstr,
			&cryptfile, &noqueued, &cptr, &reportfile,
			&older_than_sec, &newer_than_sec, &req_newfile,
			&req_newcart, &long_errmsgs, &infoheader,
			&identitystr, &u_verbose, &params.dont_keep_atime,
			&n, &bu_msg_itv);

  if(i)
    usage(argv[0]);

  if(n == 0)
    bu_msg_itv = DEFAULT_GETCURMSG_ITV;

  if(cptr){
    params.dont_compress = NEWP(UChar *, 1);
    params.dont_compress[0] = NULL;

    for(i = 1, j = 0; i < argc; i++){
      if(!strcmp(argv[i], "-s")){
	params.dont_compress = RENEWP(params.dont_compress, UChar *, j + 2);
	params.dont_compress[j] = strdup(argv[i + 1]);
	j++;
	params.dont_compress[j] = NULL;
	i++;
      }
    }
  }

  if(givehelp)
    give_help();

  if(noqueued || (!bu_create))
    comm_mode = SERIAL_OPERATION;

  if(clientprog)
    bu_request = 1;

  if(bu_duptapeargs)
    bu_duptape = 1;

  if(servermsg){
    bu_svrmsg = 1;

    if(strlen(servermsg) > (i = commbufsiz - 4)){
	errmsg(T_("Error: The server message length must be smaller than %d"), i);
	usage(argv[0]);
    }
  }

  if(bu_create + bu_extract + bu_contents + bu_request + bu_verify
		+ bu_index + bu_duptape + bu_junction
		+ (bu_msg_itv > 0) > 1){
    errmsg(T_("You may supply only one of the flags: cxtdJXIDm"));
    usage(argv[0]);
  }
  if(bu_create + bu_extract + bu_contents + bu_request + bu_verify
	+ bu_index + bu_ready + bu_duptape + bu_svrmsg + bu_junction
	+ (bu_msg_itv > 0) < 1
		&& s_cart < 0 && s_file < 0 && ! querypos && ! querywrpos
		&& ! req_newcart){
    if(verbose){
	fprintf(stdout, "%s: %s %s\n", T_("Version"), PACKAGE, VERSION_STRING);
	exit(0);
    }

    errmsg(T_("You have to supply one of the flags: cxtCFqQwXIDMGm"));
    usage(argv[0]);
  }
  if(std_io && !bu_extract && !bu_create){
    errmsg(T_("The flag - may only be given in combination wit -x or -c"));
    usage(argv[0]);
  }
  if(toextractfilename && (! bu_extract && ! bu_create && ! bu_duptape)){
    errmsg(T_("The flag -T may only be given in combination wit -x, -c or -D"));
    usage(argv[0]);
  }
  if(toextractfilename && allfiles){
    errmsg(T_("The flags -T and -a don't make sense together. Ignoring -a"));
    allfiles = 0;
  }
  if(unused_args && (toextractfilename || allfiles)){
    errmsg(T_("If you name files and/or directories in the commandline,\n -a or -T don't make any sense. Using filenames from the command line"));
    allfiles = 0;
    toextractfilename = NULL;
  }
  if(allfiles && ! bu_extract){
    errmsg(T_("Warning: the flag -a makes only sense in combination with -x"));
  }
  if(params.unlink && ! bu_extract){
    errmsg(T_("Warning: the flag -u makes only sense in combination with -x"));
  }
  if(params.recursive && !(bu_create || bu_extract)){
    errmsg(T_("Warning: the flag -R makes only sense in combination with -x or -c"));
  }
  if(params.relative && ! bu_extract && ! bu_verify){
    errmsg(T_("Warning: the flag -r makes only sense in combination with -x or -d"));
  }
  if(params.ignoreown && ! bu_extract){
    errmsg(T_("Warning: the flag -i makes only sense in combination with -x"));
  }
  if(newer_than_file && ! bu_create){
    errmsg(T_("Warning: the flag -N only makes sense in combination with -c"));
  }
  if(newer_than_file && newer_than_sec){
    errmsg(T_("Warning: -N and -A may not be specified together, -N ignored."));
    ZFREE(newer_than_file);
  }
  if(params.zipcmd && params.unzipcmd && !bu_create){
    errmsg(T_("Warning: to specify (un-)processing is only allowed with -c"));
  }
  if(params.check && ! bu_contents && ! bu_create && ! bu_extract){
    errmsg(T_("Warning: the flag -Z makes only sense in combination with -t, -x or -c. Ignoring -Z"));
    params.check = NO;
  }
  if(bu_index && savefile){
    errmsg(T_("Error: You cannot get a tape index (-I) of a file (-f)"));
    usage(argv[0]);
  }
  if(bu_duptape && savefile){
    errmsg(T_("Error: You cannot duplicate a file (-f) to tape"));
    usage(argv[0]);
  }
  if(servername && savefile){
    errmsg(T_("Error: You may not specify -h and -f together"));
    usage(argv[0]);
  }
  if((s_cart != -1 || s_file != -1 || querypos || querywrpos) && savefile){
    errmsg(T_("Error: with a file (-f) you cannot set/get cartridge and/or tape position"));
    usage(argv[0]);
  }
  if(savefile && c_f_verbose){
    errmsg(T_("Warning: -n makes no sense in combination with -f. -n is ignored"));
    c_f_verbose = 0;
  }
  if(savefile && l_verbose){
    errmsg(T_("Warning: -l makes no sense in combination with -f. -l is ignored"));
    l_verbose = 0;
  }
  if(!bu_create && !bu_contents && !bu_extract && o_verbose){
    errmsg(T_("Warning: -O is only supported in combination with -c, -x or -t, -O ignored"));
    o_verbose = 0;
  }
  if(savefile && cryptfile){
    errmsg(T_("Warning: -k makes no sense in combination with -f"));
  }
  if(savefile && identitystr){
    errmsg(T_("Warning: -W makes no sense in combination with -f, -W ignored."));
  }
  if(s_file >	0x7fffffff){
    errmsg(T_("Error: requested file number is too high - must be <= %ld"),
		0x7fffffffL);
    usage(argv[0]);
  }
  if(bu_request){
    if(strlen(clientprog) > 250){
      errmsg(T_("Error: program line supplied with -X is too long (max. 250 chars)"));
      usage(argv[0]);
    }
  }

#ifndef	USE_ZLIB

  if(params.check && bu_create){
    fprintf(stderr, T_("Warning: CRC32 checksumming not available (requires zlib)\n"));
    params.check = NO;
  }

#endif

  i1 = i2 = -1;		/* evaluate uidstr */
  if(uidstr)
    sscanf(uidstr, "%d:%d", &i1, &i2);
  if(i1 != -1)
    params.uid = i1;
  if(i2 != -1){
    params.gid = i2;
    cptr = strchr(uidstr, ':');
    if(cptr){
      cptr = strchr(cptr + 1, ':');
      if(cptr){
	while(sscanf(cptr + 1, "%d%n", &i1, &i2) > 0){
	  params.gids = ZRENEWP(params.gids, gid_t, params.ngids + 1);
	  if(!params.gids){
	    errmsg(T_("Error: cannot allocate memory"));
	    exit(51);
	  }

	  params.gids[params.ngids++] = i1;

	  cptr += (1 + i2);
	  if(*cptr != ',')
	    break;
	}
      }
    }
  }	

  if(params.check && bu_extract && params.uid != 0){
    i = set_eff_ugids(params.uid, params.gid, params.ngids, params.gids);
    if(!i)		/* if we can setuid to the user, there is no need */
	params.check = NO;	/* to check later, cause we are the user */
  }

  if(E__(alloc_commbuf(DEFCOMMBUFSIZ) ? - ENOMEM : 0))
    exit(50);

  if(set_cryptkey(cryptfile, ACCESSKEYSTRING,
#ifdef	HAVE_DES_ENCRYPTION
			YES
#else
			NO
#endif
			) && cryptfile){
	errmsg(T_("Warning: Cannot read enough characters from encryption key file `%s'"),
		cryptfile);
	errmsg(T_("         Ignoring file, using compiled-in key"));
  }

  if(!servername)
    servername = DEFAULT_SERVER;

  if(!identitystr)
    identitystr = strdup(DEFAULT_CLIENT_IDSTR);

  if(identitystr){
    repl_esc_seq(identitystr, '\\');

    if(strlen(identitystr) > 128){
	errmsg(T_("Warning: Identity string is too long, truncating to 128 characters"));
	identitystr[128] = '\0';
    }
  }

  if(params.zipcmd){
    if(params.zipcmd[0] == '.'){
      params.bi_compr_level = 5;

      if(params.zipcmd[1] && isspace(params.zipcmd[1])){
	i = sscanf(params.zipcmd + 2, "%d,%d%n", &i1, &i2, &n1);
	if(i < 2)
	  i = sscanf(params.zipcmd + 2, "%d%n", &i1, &n1);
	if(i >= 1){
	  memmove(params.zipcmd, params.zipcmd + 2 + n1,
				strlen(params.zipcmd) + 2 + n1 + 1);
	  if(empty_string(params.zipcmd)){
	    ZFREE(params.zipcmd);
	    ZFREE(params.unzipcmd);
	  }

	  params.bi_compr_level = i1;
	  if(i >= 2)
	    params.bi_compr_maxblock = i2;
	}
      }
      else{
	ZFREE(params.zipcmd);
	ZFREE(params.unzipcmd);
      }

#ifndef	USE_ZLIB
      errmsg(T_("Warning: Built-in compression not available (not compiled in)."
		" Ignoring built-in compression request."));
      params.bi_compr_level = params.bi_compr_maxblock = 0;
#endif

    }
  }

  signal(SIGTERM, signal_handler);
  signal(SIGINT, signal_handler);
  signal(SIGHUP, signal_handler);
/*  signal(SIGSEGV, signal_handler);*/
  signal(SIGBUS, signal_handler);
  signal(SIGPIPE, signal_handler);

  params.verbose = verbose;

  if(verbose || bu_contents){
    verbose = VERBOSE_NORMAL;

    if(c_f_verbose)
	verbose |= VERBOSE_CART_FILE;
    if(l_verbose)
	verbose |= VERBOSE_LOCATION;
    if(o_verbose)
	verbose |= VERBOSE_UID;
    if(u_verbose)
	verbose |= VERBOSE_MTIME;
  }

  params.verbosefunc = normal_verbose;
  if(c_f_verbose || o_verbose || u_verbose){
    params.verbosefunc = extended_verbose;
    params.pre_verbosefunc = pre_extended_verbose;
  }

  if(newer_than_file){
    if(stat(newer_than_file, &statb)){
      errmsg(T_("Warning: Cannot stat file `%s'. Option ignored"),
				newer_than_file);
    }
    else{
      params.time_newer = statb.st_mtime;
    }
  }

  if(older_than_sec){
    params.time_older = strint2time(older_than_sec);
    if(params.time_older == UNSPECIFIED_TIME){
	errmsg(T_("Warning: Could not read upper time limit from "
			"`%s'. Option ignored"), older_than_sec);
	params.time_older = 0;
    }
  }
  if(newer_than_sec){
    params.time_newer = strint2time(newer_than_sec);
    if(params.time_newer == UNSPECIFIED_TIME){
	errmsg(T_("Warning: Could not read lower time limit from "
			"`%s'. Option ignored"), newer_than_sec);
	params.time_newer = 0;
    }
  }

  if(savefile)
    if(!strcmp(savefile, "-"))
	save_stdio = YES;

  if(savefile && !bu_create){
    if(!save_stdio && eaccess(savefile, R_OK)){
	errmsg(T_("Error: Cannot open file `%s' for reading"), savefile);
	exit(9);
    }
  }

  if(unused_args){
    filelist = unused_args;
  }
  else if((toextractfilename || (bu_extract && !allfiles))
				&& !bu_duptape && !std_io){
    if(toextractfilename){
	fp = fopen(toextractfilename, "r");
	if(!fp){
	  errmsg(T_("Error: cannot open file `%s'"), toextractfilename);
	  exit(4);
	}
    }
    else
	fp = stdin;

    filelist = get_file_list(fp);

    if(toextractfilename)
	fclose(fp);

    if(! filelist){
	errmsg(T_("Exiting, because %s does not contain any filename"),
			toextractfilename ? toextractfilename : T_("input"));
	exit(0);
    }
  }

  if(filelist){
    for(files = filelist, i = 0; *files; i++, files++);

    if(i > 0){
	found_files = NEWP(UChar, i);
	memset(found_files, 0, i * sizeof(UChar));

	if(!found_files){
	  errmsg(T_("Error: cannot allocate memory"));
	  exit(51);
	}
    }
  }

  if(errorlogfile){
    fp = fopen(errorlogfile, "w");
    if(!fp){
	errmsg(T_("Error: cannot open error log file `%s'"),
				errorlogfile);
	errmsg(T_("       Writing errors to stdout"));
    }
    else
	params.errfp = fp;
  }

  if(!savefile){
    if(!req_portstr)
	req_portstr = DEFAULT_SERVICE;

    if(FN_ISPATH(req_portstr)){
	uxsock = YES;
    }
    else{
	i = get_tcp_portnum(req_portstr);
	if(i >= 0)
	  portnum = (short unsigned) i;
    }

    he = get_host_by_name(servername);
    if(he)
	free(he);
    if(!uxsock && !he){
	errmsg(T_("Error: Cannot resolve hostname %s"), servername);
	exit(55);
    }

    timemem = time(NULL);
    forever{
	commfd = connect_afbu_server(servername,
			(uxsock ? req_portstr : NULL), portnum,
				AFSERVER_CONNECT_TIMEOUT / 10 + 1);

	if(commfd < 0){
	  if(time(NULL) - timemem > AFSERVER_CONNECT_TIMEOUT){
	    errmsg(T_("Error: Cannot open communication socket: %s"),
		errno ? strerror(errno) : T_("Unknown error"));
	    exit(52);
	  }

	  ms_sleep(1000);
	  continue;
	}

	i = start_server_comm(AFSERVER_CONNECT_TIMEOUT / 10 + 1);
	if(!i)			/* buffered mode is enabled later, cause */
	  break;		/* it makes no sense for the first actions */

	if(i == AUTHENTICATION){
	  errmsg(T_("Error: %s"), fault_string(i));
	  exit(49);
	}

	close(commfd);
	if(time(NULL) - timemem > AFSERVER_CONNECT_TIMEOUT){
	  errmsg(T_("Error: %s. Server closed connection"), fault_string(i));
	  exit(53);
	}
    }

    if(server_can_sendtbufsiz){
	E__(gettapeblocksize(&params, YES));

	if(server_can_setcbufsiz && !savefile && !bu_junction){
	  n = MIN(tapeblocksize, MAXCOMMBUFSIZ);

	  E__(set_commbufsiz(n));
	}
    }

    if(server_can_userid && (cptr = getlogin())){
	struct utsname	unam;

	strncpy(username, cptr, 128);
	username[127] = '\0';

	memset(buf, 0, 256);
	buf[0] = USERIDENT;
	strcpy(buf + 1, username);
	if(!uname(&unam))
	  strncpy(buf + 1 + 128, unam.nodename, 128);
	buf[1 + 128 + 127] = '\0';
	E__(write_forced(commfd, buf, 1 + 256) != 1 + 256);
	E__(result());
    }
  }
  else{
    comm_mode = SERIAL_OPERATION;

    if(bu_create){
      if(save_stdio){
	commfd = 1;
      }
      else{
	if( (fp = fopen(savefile, "w")) ){
	  fclose(fp);

	  commfd = open(savefile, O_WRONLY | O_CREAT | O_BINARY, 0644);
	}
	else
	  commfd = -1;
      }
    }
    else{
      if(save_stdio){
	commfd = 0;
      }
      else{
	commfd = open(savefile, O_RDONLY | O_BINARY);
      }
    }

    if(commfd < 0){
	errmsg(T_("Error: Cannot open file `%s'"), savefile);
	exit(6);
    }
  }

  if(bu_msg_itv){
    if(server_can_sendmsgs)
	poll_messages();	/* does not return */

    errmsg(T_("Info: Server version is too low to send current messages"));
    exit(7);
  }

  if(s_cset != 1){
    if(E__(setcartset(s_cset)))
	GETOUT;
  }
  if(s_cart != -1){
    if(E__(setcart(bu_duptape ? - s_cart : s_cart)))
	GETOUT;
  }
  if(s_file != -1){
    if(E__(setfilenum(s_file)))
	GETOUT;
  }

  if(identitystr && !savefile)
    E__(send_identity(identitystr));

  if(bu_request){
    E__( (i = request_client_backup(clientprog)) & 0xffff);

    gexitst = (i >> 16) & 0xff;
  }

  if(bu_ready){
    if(OK__(i = getserverstate(buf, YES))){
	c = buf[0];
	j = c & STREAMER_FLAGS_MASK;
	c &= STREAMER_STATE_MASK;
	fprintf(stdout, T_("Streamer state: %s"),
			c == STREAMER_READY ? "READY" : (
			c == STREAMER_BUSY ? "BUSY" : (
			c == STREAMER_UNLOADED ? "UNLOADED" : (
			c == STREAMER_DEVINUSE ? "DEVINUSE" : (
			c == STREAMER_UNAVAIL ? "UNAVAILABLE" : "UNKNOWN")))));
	if(j & STREAMER_CHANGEABLE)
	  fprintf(stdout, "+CHANGEABLE");

	xref_to_Uns32(&j, buf + 1);
	if(j & OPENED_RAW_ACCESS){
	  switch(j & MODE_MASK){
	    case OPENED_FOR_READ:
	    case OPENED_READING:
		fprintf(stdout, "+RAWREADING");
		break;

	    case OPENED_FOR_WRITE:
	    case OPENED_WRITING:
		fprintf(stdout, "+RAWWRITING");
		break;
	    default:
		break;
	  }
	}
	else{
	  xref_to_Uns32(&n, buf + 5);

	  switch(j){
	    case OPENED_FOR_READ:
	    case OPENED_READING:
		fprintf(stdout, "+READING=%d", (int) n);
		break;

	    case OPENED_FOR_WRITE:
	    case OPENED_WRITING:
		fprintf(stdout, "+WRITING=%d", (int) n);
		break;
	  }
	}

	xref_to_Uns32(&j, buf + 9);
	if(j)
	  fprintf(stdout, "+PENDING=%d", (int) j);

	fprintf(stdout, "\n");
    }
    if(i)
	errmsg(T_("Error: Cannot determine streamer state on server"));

    if(verbose){
      if(server_can_sendid && OK__(i = getserverid(buf, YES))){
	fprintf(stdout, "Server-ID: %s\n", buf);
      }

      if(server_can_sendutapes && OK__(i = getusedtapes(&cptr, identitystr))){
	if(cptr){
	  fprintf(stdout, "Precious tapes: %s\n", cptr);
	  free(cptr);
	}
      }
    }
  }

  if(req_newcart){			/* this must be before querywrpos */
    if(request_newcart())
	errmsg(T_("Warning: No new or reusable cartridge found, keeping old writing position."));

    if(QUEUEDOP)
	E__(i = post_process_rest_of_queue(0));
  }

  if(querypos){
    if(OK__(i = getcartandfile(&params))){
	if(QUEUEDOP)
	  E__(i = post_process_rest_of_queue(0));

	if(!i){
	  fprintf(stdout, T_("Current tape access position\n"));
	  fprintf(stdout, T_("Cartridge: %d\nFile:      %d\n"),
				(int) cart, (int) filenum);
	}
    }
    if(i)
	errmsg(T_("Error: Cannot determine current tape access position"));
  }

  if(querywrpos){			/* this must be behind req_newcart */
    if(bu_create && !savefile){		/* if create is requested, we open */
	if(req_newfile)			/* the tape already here, cause */
	  if(E__(request_newfile()))	/* otherwise the writing position */
	    GETOUT;			/* obtained below might be wrong */

	if(E__(open_write(0)))
	  GETOUT;

	opened_write = YES;
    }

    if(OK__(i = getwrcartandfile(&params))){
	if(QUEUEDOP)
	  E__(i = post_process_rest_of_queue(0));

	if(!i){
	  fprintf(stdout, T_("Next tape writing position\n"));
	  fprintf(stdout, T_("Cartridge: %d\nFile:      %d\n"),
				(int) wrcart, (int) wrfilenum);
	}
    }
    if(i){
	errmsg(T_("Error: Cannot determine next tape writing position"));
	if(bu_create || bu_duptape)
	  bu_create = bu_duptape = NO;
    }
  }

  if((querypos || querywrpos) && verbose){
    if(OK__(i = getnumcarts(&params, YES))){
	if(!i){
	  fprintf(stdout, T_("Number of cartridges: %d\n"), (int) numcarts);
	}
    }
    if(i)
	errmsg(T_("Error: Cannot determine number of cartridges"));

    if(OK__(i = getcartset(&params, YES))){
	if(!i){
	  fprintf(stdout, T_("Current cartridge set: %d\n"), (int) cartset);
	}
    }
    if(i)
	errmsg(T_("Error: Cannot determine current cartridge set"));
  }

  if((bu_create || bu_extract || bu_contents || bu_verify || bu_index || bu_duptape)
	&& (querypos || querywrpos || bu_ready) && (verbose || bu_contents)){
    fprintf(stdout, "--\n");
    fflush(stdout);
    fflush(stderr);
  }

  if(!savefile){	/* Now start the buffered operation mode */
    if(comm_mode == QUEUED_OPERATION){
	create_command_queue();
    }

    if(QUEUEDOP){
	sigaddset(&params.blocked_signals, SIGALRM);
	sigaddset(&params.blocked_signals, SIGPIPE);
    }
  }

  if(bu_svrmsg){	/* this must be before bu_junction (minrestoreinfo) */
    repl_esc_seq(servermsg, '\\');

    if(send_svrmsg(commfd, commfd, servermsg, strlen(servermsg))){
	errmsg(T_("Error: Evaluating server message failed"));
    }
  }

  if(bu_duptape){
    E__( (i = duplicate_tape(bu_duptapeargs, &params)) );

    gexitst = (i >> 16) & 0xff;
  }

  if(bu_create){
    params.mode = MODE_CREATE;

    params.outputfunc = bu_output;

    if(!savefile && !opened_write){	/* if not already done above */
	if(req_newfile)
	  if(E__(request_newfile()))
	    GETOUT;

	if(E__(open_write(0)))
	  GETOUT;
    }

    if(!std_io){
	if(infoheader){
	  if(!strcmp(infoheader, "-")){
	    infoheader = fget_alloc_str(stdin);
	    if(!infoheader)
		infoheader = "";
	    chop(infoheader);
	  }

	  repl_esc_seq(infoheader, '\\');
	  sprintf(buf, "%d;%d;", INFORMATION, (int) strlen(infoheader));

	  if(E__(bu_output(buf, strlen(buf), &params))
		|| E__(bu_output(infoheader, strlen(infoheader), &params))
		|| E__(bu_output(".", 1, &params)))
	    GETOUT;
	}

	if(filelist){
	  while(*filelist){
	    i = E__(pack_writeout(*filelist, &params, 0));

	    if(i){
		if(i > 0)
		  num_errors++;

		if(num_errors > MAX_NUM_ERRORS){
		  errmsg(T_("Too many errors on server. Exiting"));

		  break;
		}
	    }
	    else{
		num_errors = 0;
	    }

	    filelist++;
	  }
	}
	else{
	  while(!feof(stdin)){
	    line = fget_alloc_str(stdin);
	    if(!line)
		continue;
	    chop(line);

	    i = E__(pack_writeout(line, &params, 0));

	    free(line);

	    if(i){
		if(i > 0)
		  num_errors++;

		if(num_errors > MAX_NUM_ERRORS){
		  errmsg(T_("Too many errors on server. Exiting"));

		  break;
		}
	    }
	    else{
		num_errors = 0;
	    }
	  }
	}

	pack_writeout(NULL, &params, ENDOFARCHIVE);
    }
    else{
	forever{
	  i = read(0, buf, BUFFERSIZ);
	  if(i < 1)
	    break;

	  bu_output(buf, i, &params);
	}
    }

    if(E__(send_pending(NO)))
	GETOUT;

    if(!savefile){
	if(E__(getwrcartandfile(&params)))
	  GETOUT;

	if(QUEUEDOP)
	  E__(post_process_rest_of_queue(0));

	params.vars.used_carts = add_to_Uns32Ranges(params.vars.used_carts,
							wrcart, wrcart);
	params.vars.lastcart = wrcart;
	params.vars.lastfile = wrfilenum;

	if(E__(closetape(1)))
	  GETOUT;
    }
  }
  else if(bu_extract || bu_verify){
    params.mode = (bu_extract ? MODE_EXTRACT : MODE_VERIFY);

    params.inputfunc = bu_input;

    if(!savefile){
	if(E__(open_read(0)))
	  GETOUT;
    }

    if(!std_io){
	if(bu_extract)
	  pack_readin(filelist, &params, found_files);
	else
	  pack_verify(filelist, &params, found_files);
    }
    else{
	do{
	  i = bu_input(buf, BUFFERSIZ, &params);

	  i = write_forced(1, buf, i);
	} while(i > 0);
    }

    if(!savefile){
	if(E__(closetape(1)))
	  GETOUT;
    }

    if(filelist){
	for(files = filelist, i = 0; *files; files++, i++){
	  if(!found_files[i]){
	    errmsg(T_("%s not found in archive"), *files);
	    gexitst = 5;
	  }
	}
    }
  }
  else if(bu_contents){
	params.mode = MODE_CONTENTS;

	params.inputfunc = bu_input;

	if(!savefile){
	  if(E__(open_read(0)))
	    GETOUT;
	}

	pack_contents(&params);

	if(!savefile){
	  if(E__(closetape(1)))
	    GETOUT;
	}
  }
  else if(bu_index){
    while(bu_index){
	Int8	eot = 0;
	Int32	len;
	UChar	htypebuf[20];

	params.mode = MODE_INDEX;

	sprintf(htypebuf, "%d;", INFORMATION);
	j = strlen(htypebuf);

	if(E__(set_server_serial))
	  break;

	if(E__(set_server_erroneot))
	  break;

	params.inputfunc = bu_input;

	while(!eot){
	  if(open_read(0)){
	    eot = 1;
	    break;
	  }

	  if(params.pre_verbosefunc)
	    params.pre_verbosefunc("", &params);

	  i = params.inputfunc(buf, j, &params);
	  if(i < j){
	    E__(closetape(0));
	    break;
	  }
	  buf[i] = '\0';

	  if(!strcmp(htypebuf, buf)){
	    len = 0;
	    cptr = NULL;

	    forever{
		i = params.inputfunc(&c, 1, &params);
		if(i < 1)
		  break;

		if(isdigit(c))
		  len = len * 10 + (c - '0');
		else if(c == ';'){
		  cptr = NEWP(UChar, len + 1);

		  i = params.inputfunc(cptr, len + 1, &params);
		  if(i < len + 1){
		    break;
		  }
		  if(cptr[len] != '.')
		    break;
		  cptr[len] = '\0';

		  filenum_valid = wrfilenum_valid = rdfilenum_valid = NO;
		  getcartandfile(&params);

		  params.verbosefunc(cptr, &params);
		  fflush(params.infp);
		  fflush(params.outfp);

		  break;
		}
		else
		  break;
	    }		/* reading header information */

	    ZFREE(cptr);
	  }		/* if have header */

	  E__(closetape(0));

	  if(skipfiles(1)){
	    eot = 1;
	    break;
	  }
	}

	break;
    }
  }
  else if(bu_junction){	/* this must be after bu_svrmsg (minrestoreinfo) */
    struct pollfd	fds[2];

    fds[0].fd = 0;
    fds[0].events = POLLIN;
    fds[1].fd = commfd;
    fds[1].events = POLLIN;

    forever{
	i = poll(fds, 2, -1);
	if(i < 0 && errno != EINTR){
	  errmsg(T_("Error: poll failed in main: %s"), strerror(errno));
	  exit(10);
	}

	if(POLLINEVENT(fds[0].revents)){
	  i = read(0, buf, BUFFERSIZ);
	  if(i < 1)
	    break;

	  write_forced(commfd, buf, i);
	}
	if(POLLINEVENT(fds[1].revents)){
	  i = read(commfd, buf, BUFFERSIZ);
	  if(i < 1)
	    break;

	  write_forced(1, buf, i);
	}
    }
  }

  if(QUEUEDOP)
    E__(delete_command_queue());	/* synchronous mode for disconnect */

  if(reportfile)
    add_written_tapes(&params.vars.used_carts);

  if(!savefile)
    close_connection();
  else{
    if(save_stdio)
      close(commfd);
  }

  if(errorlogfile)
    fclose(params.errfp);

  strcpy(params.vars.enddate, actimestr());

  if(reportfile)
    write_reportfile(reportfile, &params, 0);

  if(!gexitst)
    gexitst = (params.vars.errnum ? 14 : 0);

  if(params.vars.errnum){
    UChar	*estrs[33], **cpptr;

    cpptr = packer_strerrors(estrs, params.vars.errnum);
    errmsg(T_("The following errors occurred"));
    while(*cpptr)
	errmsg(*(cpptr++));
  }

  exit(gexitst);

/* Note on exit status:  1-49 and 100- are fatal
 *                      50-99 are retryable
 */

 getout:

  if(!savefile)
    close_connection();
  else{
    if(save_stdio)
      close(commfd);
  }

  if(QUEUEDOP){
     E__(post_process_rest_of_queue(0));
  }

  if(errorlogfile)
    fclose(params.errfp);

  strcpy(params.vars.enddate, actimestr());

  if(reportfile)
    write_reportfile(reportfile, &params, 1);

  exit(eexitst);
}

Int32
process_queue_entry_response()
{
  QueueEntry	*entry;
  UChar		*inbuf;
  Int32		res, num_to_read;

  entry = queue_entries + queueent_processed_idx;

  inbuf = inbufmem + max_inresultlen * queueent_processed_idx;
  num_to_read = entry->num_in;

  entry->processed = PROCESSED_OK;

  if(num_to_read > 0){
    res = (read_forced(commfd, inbuf, num_to_read) != num_to_read);
    if(res)
	entry->processed = - errno;

    bytes_transferred += ESTIM_PROT_OVERHEAD + num_to_read;
  }

  if( (res = result()) )
	entry->processed = res;

  queueent_processed_idx++;
  if(queueent_processed_idx >= queuelen)
    queueent_processed_idx = 0;

  return(1);
}

Int32
process_queue_entry_request()
{
  QueueEntry	*entry;
  Uns16		instruction;
  UChar		*outbuf;
  UChar		c;
  Int32		res, num_to_write, i;

  entry = queue_entries + queueent_requested_idx;

  outbuf = outbufmem + max_outarglen * queueent_requested_idx;
  num_to_write = entry->num_out;
  instruction = entry->instruction;

  if(num_to_write == 0){
    c = (UChar) instruction;
    res = (write_forced(commfd, &c, 1) != 1);
    if(res){
      entry->processed = - errno;
    }

    bytes_transferred += ESTIM_PROT_OVERHEAD + 1;
  }

  if(num_to_write > 0){
    outbuf[1] = (UChar) instruction;

    i = num_to_write + 1;
    res = (write_forced(commfd, outbuf + 1, i) != i);

    if(res)
	entry->processed = - errno;

    bytes_transferred += ESTIM_PROT_OVERHEAD + i;
  }

  queueent_requested_idx++;
  if(queueent_requested_idx >= queuelen)
    queueent_requested_idx = 0;

  return(1);
}

Int64
queued_read(int fd, UChar * buf, Int64 num)
{
  struct pollfd	fds[2];
  Int32		i;
  Flag		did_sth;

  fds[0].fd = fd;
  fds[0].events = POLLIN;
  fds[1].fd = commfd;

  do{
    fds[1].events = 0;

    did_sth = NO;

    if(queue_insert_idx != queueent_processed_idx){
	if(queue_insert_idx != queueent_requested_idx)
	  fds[1].events |= POLLOUT;

	if(queueent_requested_idx != queueent_processed_idx)
	  fds[1].events |= POLLIN;

	i = poll(fds, 2, -1);
	if(i < 0)
	  return(i);

	if(POLLINEVENT(fds[1].revents)){
	  i = process_queue_entry_response();
	  did_sth = YES;
	}

	if(POLLOUTEVENT(fds[1].revents)){
	  i = process_queue_entry_request();
	  did_sth = YES;
	}
    }
  } while(did_sth);

  return(read_forced(fd, buf, num));
}

Int64
queued_write(int fd, UChar * buf, Int64 num)
{
  struct pollfd	fds[2];
  Int32		i;
  Flag		did_sth;

  fds[0].fd = fd;
  fds[0].events = POLLOUT;
  fds[1].fd = commfd;

  do{
    fds[1].events = 0;

    did_sth = NO;

    if(queue_insert_idx != queueent_processed_idx){
	if(queue_insert_idx != queueent_requested_idx)
	  fds[1].events |= POLLOUT;

	if(queueent_requested_idx != queueent_processed_idx)
	  fds[1].events |= POLLIN;

	i = poll(fds, 2, -1);
	if(i < 0)
	  return(i);

	if(POLLINEVENT(fds[1].revents)){
	  i = process_queue_entry_response();
	  did_sth = YES;
	}

	if(POLLOUTEVENT(fds[1].revents)){
	  i = process_queue_entry_request();
	  did_sth = YES;
	}
    }
  } while(did_sth);

  return(write_forced(fd, buf, num));
}

Int32
process_queue(Flag do_wait, Flag one_step_suff)
{
  struct pollfd	fds;
  Int32		i;
  Flag		did_sth;

  fds.fd = commfd;

  do{
    fds.events = 0;

    did_sth = NO;

    if(queue_insert_idx != queueent_processed_idx){
	if(queue_insert_idx != queueent_requested_idx)
	  fds.events |= POLLOUT;

	if(queueent_requested_idx != queueent_processed_idx)
	  fds.events |= POLLIN;

	i = poll(&fds, 1, do_wait ? -1 : 0);
	if(!i)
	  return(0);
	if(i < 0)
	  return(i);

	if(POLLINEVENT(fds.revents)){
	  i = process_queue_entry_response();
	  did_sth = YES;
	}

	if(POLLOUTEVENT(fds.revents)){
	  i = process_queue_entry_request();
	  did_sth = YES;
	}
    }
  } while(did_sth && !one_step_suff);

  return(0);
}

Int32
post_process_proc(Uns32 entryidx)
{
  QueueEntry	*entry;
  UChar		*inmem;
  UChar		*inbuf;
  UChar		*outbuf;
  Uns32		num_in;
  Int32		res, n;
  Uns16		instruction;

  entry = queue_entries + entryidx;

  inmem = entry->inmem;
  inbuf = inbufmem + entryidx * max_inresultlen;
  outbuf = outbufmem + entryidx * max_outarglen + 2;
  num_in = entry->num_in;
  instruction = entry->instruction;

  if(inmem && num_in)
    memcpy(inmem, inbuf, num_in * sizeof(UChar));

  switch(instruction){
    case QUERYPOSITION:
	xref_to_UnsN(&cart, inbuf, 24);
	xref_to_Uns32(&filenum, inbuf + 3);
	break;

    case QUERYWRPOSITION:
	xref_to_UnsN(&wrcart, inbuf, 24);
	xref_to_Uns32(&wrfilenum, inbuf + 3);
	break;

    case QUERYRDPOSITION:
	xref_to_UnsN(&rdcart, inbuf, 24);
	xref_to_Uns32(&rdfilenum, inbuf + 3);
	break;

    case QUERYNUMCARTS:
	xref_to_UnsN(&numcarts, inbuf, 24);
	break;

    case QUERYCARTSET:
	xref_to_UnsN(&cartset, inbuf, 24);
	break;

    case QUERYTAPEBLOCKSIZE:
	xref_to_Uns32(&tapeblocksize, inbuf);
	break;

    case SETFILE:
    case SETRAWFILE:
	xref_to_Uns32(&filenum, outbuf);
	break;

    case SETCARTRIDGE:
    case SETRAWCARTRIDGE:
	xref_to_UnsN(&cart, outbuf, 24);
	break;
  }

  switch(instruction){
    case OPENFORWRITE:
    case OPENFORREAD:
    case OPENFORRAWWRITE:
    case OPENFORRAWREAD:
    case SKIPFILES:
    case SETFILE:
    case SETRAWFILE:
    case SETCARTRIDGE:
    case SETRAWCARTRIDGE:
    case CLOSETAPE:
    case CLOSETAPEN:
	reset_tape_io;
	break;
  }

  res = (entry->processed == PROCESSED_OK ? NO_ERROR
			: entry->processed);

  if( (n = entry->req_queue_cbs) > 0){
    entry->req_queue_cbs = 0;

    if(n < queue_commbufsiz){
	ER__(change_queue_bufsiz(n), res);
    }
  }

  return(res);
}

Int32
post_process_rest_of_queue(Int32 num_remain)
{
  Int32		res = NO_ERROR;

  while(num_entries_in_queue > num_remain){
    if(queue_entries[queueent_done_idx].processed == NOT_PROCESSED){
	process_queue(YES, NO);

	continue;
    }

    res = post_process_proc(queueent_done_idx);
    if(res)
	return(res);

    queueent_done_idx++;
    if(queueent_done_idx >= queuelen)
	queueent_done_idx = 0;
  }

  return(res);
}

Int32
post_process_pending()
{
  Int32		res = NO_ERROR;

  while(queueent_done_idx != queueent_processed_idx){
    res = post_process_proc(queueent_done_idx);

    if(res)
	return(res);

    queueent_done_idx++;
    if(queueent_done_idx >= queuelen)
	queueent_done_idx = 0;
  }

  return(res);
}

Int32
append_to_queue(
  Uns16		instruction,
  UChar		*inmem,
  Uns32		num_in,
  UChar		*outmem,
  Uns32		num_out)
{
  Int32		newidx, res;
  UChar		*outbuf;

  if( (res = post_process_pending()) )
    return(res);

  while((newidx = (queue_insert_idx + 1) % queuelen) == queueent_done_idx){
    process_queue(YES, YES);			/* queue full */

    if( (res = post_process_pending()) )
      return(res);
  }

  outbuf = outbufmem + max_outarglen * queue_insert_idx;

  queue_entries[queue_insert_idx].instruction = instruction;
  queue_entries[queue_insert_idx].inmem = inmem;
  queue_entries[queue_insert_idx].num_in = num_in;
  queue_entries[queue_insert_idx].outmem = outmem;
  queue_entries[queue_insert_idx].num_out = num_out;
  queue_entries[queue_insert_idx].processed = NOT_PROCESSED;

  if(outmem && num_out)
    memcpy(outbuf + 2, outmem, num_out * sizeof(UChar));
		/* data is always put into byte 3..., so we */
		/* can put the instruction into byte 2 (and */
		/* probably 1) and send it all together in */
		/* a single packet */

  queue_insert_idx = newidx;

  return(0);
}

Int32
duplicate_tape(UChar * args, AarParams * params)
{
  UChar		**argv;
  Int32		argc;
  UChar		*targethost = NULL;
  UChar		*targetcryptkeyfile = NULL;
  UChar		*cptr, *cptr2;
  UChar		c;	/* uninitialized OK */
  UChar		*targetportstr = NULL;
  Int32		targetport = -1;
  Int32		targetcart = -1;
  Int32		i, r = 0;
  int		i1, n;

  if(cart < 1){	/* if cartridge is not supplied, ask server for current */
    i = getcartandfile(params);
    if(i)
	return(i);
  }
    
  argc = str2wordsq(&argv, args);
  if(argc < 0){
    errmsg("Memory exhausted");
    return(- errno);
  }

  i = goptions(-(argc + 1), argv - 1, "s:h;s:p;i:C;s:k",
			&targethost, &targetportstr, &targetcart,
			&targetcryptkeyfile);
  free_array(argv, argc);		/* don't need that anymore */
  if(!i){
    if(targetportstr){
	targetport = get_tcp_portnum(targetportstr);
	if(targetport < 0){
	  fprintf(stderr, T_("Error: Cannot resolve TCP service `%s'.\n"),
				targetportstr);
	  GETOUT;
	}
    }
  }
  else{
    i = sscanf(args, "%d%n", &i1, &n);
    cptr = first_nospace(i > 0 ? args + n : args);
    if(i > 0)
	targetcart = i1;

    if(*cptr && (UChar *) strstr(cptr, SEPLOC) != cptr
			&& (UChar *) strstr(cptr, PORTSEP) != cptr
			&& *cptr != ':')
	usage(gargv[0]);

    if((UChar *) strstr(cptr, SEPLOC) == cptr){
	cptr++;
	cptr2 = strstr(cptr, PORTSEP);
	if(!cptr2)
	  cptr2 = strchr(cptr, ':');

	if(cptr2){
	  c = *cptr2;
	  *cptr2 = '\0';
	}

	i = strlen(cptr);
	massage_string(cptr);

	if(!isfqhn(cptr))
	  usage(gargv[0]);

	targethost = strdup(cptr);
	if(cptr2){
	  cptr = cptr2;
	  *cptr = c;
	}
	else
	  cptr += i;
    }

    if((UChar *) strstr(cptr, PORTSEP) == cptr){
	cptr2 = strchr(++cptr, ':');
	if(cptr2)
	  *cptr2 = '\0';

	i = get_tcp_portnum(cptr);
	if(i < 1){
	  fprintf(stderr, T_("Error: Cannot resolve TCP service `%s'.\n"), cptr);
	  GETOUT;
	}

	if(cptr2)
	  *(cptr = cptr2) = ':';

	targetport = i;
    }

    if(*cptr == ':'){
	targetcryptkeyfile = strdup(cptr + 1);
	massage_string(targetcryptkeyfile);
    }
  }

  if(!targethost && servername)		/* set defaults, if not given */
    targethost = strdup(servername);
  if(targetport < 0)
    targetport = portnum;
  if(targetcart < 0)
    targetcart = cart;
  if(!targetcryptkeyfile && cryptfile)
    targetcryptkeyfile = strdup(cryptfile);

  if(targetport < 0){		/* if no port supplied: find defaults */
    targetport = get_tcp_portnum(DEFAULT_SERVICE);
    if(targetport < 0)
	targetport = DEFAULT_PORT;
  }

  i = same_host(targethost, servername);
  if(i < 0){
    errmsg(T_("Error: Cannot resolve hostname %s"), "");
    r = - errno;
    GETOUT;
  }

  if(i && targetport == (Int32) portnum && targetcart == cart){
    errmsg(T_("Error: Attempt to copy tape to itself"));
    r = - EACCES;
    GETOUT;
  }

  if(!i || targetport != (Int32) portnum)
    r = copytaperemote(targethost, targetport, targetcart,
						targetcryptkeyfile);
  else
    r = copytapelocally(targetcart);

 cleanup:
  ZFREE(targethost);
  ZFREE(targetcryptkeyfile);

  return(r);

 getout:
  CLEANUP;
}

#define	DPIPEBUFLEN	(8192 - 5)

Int32
copytaperemote(
  UChar		*targethost,
  Int32		targetport,
  Int32		targetcart,
  UChar		*targetcryptkeyfile)
{
  Int32		i, j, n, r, curfilenum, numfilescopied;
  int		pid, pst, inp[2], fd;
  UChar		commbuf[DPIPEBUFLEN + 10];
  Flag		first_read;		/* uninitialized OK */
  Flag		first_file, opened;
  time_t	timemem;

  if(pipe(inp))
    return(-2);

  pid = fork_forced();
  if(pid < 0){
    errmsg(T_("Error: Cannot create second client process"));
    return(- errno);
  }

  if(pid){	/* on master side receiving from source server */
    close(inp[0]);
    fd = inp[1];

    i = setcart(- cart);		/* set source cartridge */
    if(i){
	errmsg(T_("Error: %s. Can't set cartridge %d on source server"),
					fault_string(i), cart);
	goto getout_master;
    }

    curfilenum = numfilescopied = 0;

    if(filenum > 0){
      i = setfilenum(filenum);		/* set source file */
      if(i){
	errmsg(T_("Error: %s. Can't set tape file %d on source server"),
					fault_string(i), filenum);
	goto getout_master;
      }

      curfilenum = filenum;
    }

    if(verbose)
	fprintf(stdout, T_("Starting to copy from cartridge %d to %d\n"),
			(int) cart, (int) targetcart);

    opened = NO;

    first_file = YES;		/* flag for rewriting tape label */

    forever{
      forever{
	if(!opened){
	  i = open_read(1);		/* open source tape */
	  if(i)
	    goto cleanup_master;	/* end of data */

	  first_read = opened = YES;	/* flag for the case: nothing read */

	  if(filenum <= 0)	/* increment before printout */
	    curfilenum++;

	  numfilescopied++;
	  if(verbose)
	    fprintf(stdout, T_("Copying tape file number %d\n"), (int) curfilenum);

	  if(filenum > 0)
	    curfilenum++;
	}

	j = i = receive(commbuf + 5, DPIPEBUFLEN, &r);

	if(r > 0 && first_read && first_file && filenum <= 0)
	  rewrite_tape_label(commbuf + 5, targetcart, servername, portnum);

	memcpy(commbuf + 1, &r, 4);	/* set number of read bytes */
	commbuf[0] = 0;			/* flag for end of transmission */
	i = DPIPEBUFLEN + 5;
	i = (write_forced(fd, commbuf, i) != i);	/* send to slave */
	if(i){
	  errmsg(T_("Error: %s, cannot send buffer to slave"), fault_string(i));
	  goto getout_master;
	}

	if(j == ENDOFFILEREACHED)
	  break;

	first_read = NO;
      }

      i = closetape(0);
      if(i){
	errmsg(T_("Error: %s"), fault_string(i));
	goto getout_master;
      }
      opened = NO;

      first_file = NO;

      if(first_read)	/* nothing could be read from tape: end of data */
	break;
    }

   cleanup_master:
    memset(commbuf, 0, DPIPEBUFLEN + 5);	/* send dummy indicating */
    commbuf[0] = 1;				/* end of transmission */
    i = DPIPEBUFLEN + 5;
    i = (write_forced(fd, commbuf, i) != i);

    close(fd);				/* close pipe to slave client */
    waitpid_forced(pid, &pst, 0);

    if( (i = WEXITSTATUS(pst)) )
	errmsg(T_("Error: %s, slave client failed"), fault_string(i));

    if(verbose)
	fprintf(stdout, T_("Copied %d tape files.\n"), (int) numfilescopied);

    return(i << 16);

   getout_master:
    close(fd);				/* close pipe to slave client */
    kill(pid, SIGTERM);
    waitpid_forced(pid, &pst, 0);

    return(i << 16);
  }
  else{		/* on slave side sending to target server */
    close(inp[1]);
    fd = inp[0];

    if(commfd >= 0)
      close(commfd);	/* close connection to source server, open to target */

    if(targetcryptkeyfile){
	if(set_cryptkey(targetcryptkeyfile, ACCESSKEYSTRING,
#ifdef	HAVE_DES_ENCRYPTION
			YES
#else
			NO
#endif
			)){
	  errmsg(T_("Warning: Cannot read enough characters from encryption key file `%s'"),
		targetcryptkeyfile);
	  errmsg(T_("         Ignoring file, using compiled-in key"));
	}
    }

    ER__(delete_command_queue(), i);
    ER__(alloc_commbuf(DEFCOMMBUFSIZ), i);

    timemem = time(NULL);
    forever{
	commfd = connect_afbu_server(targethost, NULL, targetport,
					AFSERVER_CONNECT_TIMEOUT / 10 + 1);
	if(commfd < 0){
	  if(time(NULL) - timemem > AFSERVER_CONNECT_TIMEOUT){
	    errmsg(T_("Error: Cannot connect to target server"));
	    i = FATAL_ERROR;
	    goto getout_slave;
	  }
	  else
	    continue;
	}

	i = start_server_comm(AFSERVER_CONNECT_TIMEOUT / 10 + 1);
	if(!i)
	  break;

	close(commfd);
	if(time(NULL) - timemem > AFSERVER_CONNECT_TIMEOUT){
	  errmsg(T_("Error: %s. Target server closed connection"), fault_string(i));
	  i = AUTHENTICATION;
	  goto getout_slave;
	}
    }

    if(server_can_sendtbufsiz && server_can_setcbufsiz){
	i = MIN(tapeblocksize, MAXCOMMBUFSIZ);

	E__(set_commbufsiz(i));
    }

    if(!noqueued)
	comm_mode = QUEUED_OPERATION;
    if(comm_mode == QUEUED_OPERATION)
	create_command_queue();		/* buffered mode for sending */

    i = setcart(- targetcart);
    if(i){
	errmsg(T_("Error: %s. Can't set cartridge %d on target server"),
					fault_string(i), targetcart);
	goto getout_slave;
    }

    if(filenum <= 0){
      if( (i = erasetape()) ){
	errmsg(T_("Error: %s. Can't erase tape on target server"),
					fault_string(i));
	goto getout_slave;
      }
    }

    if( (i = setfilenum(filenum > 0 ? filenum : -1)) ){
	errmsg(T_("Error: %s. Can't set tape file on target server"),
					fault_string(i));
	goto getout_slave;
    }

    opened = NO;

    forever{			/* receive from master side */
	i = read_forced(fd, commbuf, DPIPEBUFLEN + 5);
	if(i < DPIPEBUFLEN + 5){
	  errmsg(T_("Error: Not enough bytes passed, possible parent process error"));
	  goto getout_slave;
	}

	memcpy(&n, commbuf + 1, 4);	/* number of valid bytes */

	if(!opened && n > 0){		/* if not yet open */
	  i = open_write(1);		/* open target tape */
	  if(i){
	    errmsg(T_("Error: Could not open target tape for writing"));
	    goto getout_slave;
	  }

	  opened = YES;
	}

	i = send_to_server(commbuf + 5, n);

	if(i){
	  errmsg(T_("Error: Could not send data to target backup server"));
	  goto getout_slave;
	}

	if(n < DPIPEBUFLEN){		/* end of this tape file */
	  if(opened){
	    i = send_pending(YES);
	    if(!i)
		i = closetape(0);	/* close without rewind */

	    if(i){
		errmsg(T_("Error: Target tape could not be closed properly"));
		goto getout_slave;
	    }
	  }

	  opened = NO;

	  if(commbuf[0])
	    goto cleanup_slave;
	}
    }

   cleanup_slave:
    i = closetape(1);		/* close target tape with rewind */

    close(fd);			/* close connection to master */

    return(i << 16);

   getout_slave:
    closetape(1);

    close(fd);

    return(i << 16);
  }
}

static Int32		/* remove all files in the given directory */
cleandir(UChar * path)
{
  UChar		**files, **f, *newpath;

  newpath = strchain(path, FN_DIRSEPSTR, "*", NULL);
  if(!newpath)
    GETOUT;

  files = fnglob(newpath);
  if(!files)
    GETOUT;

  for(f = files; *f; f++)
    unlink(*f);

  free(newpath);
  free_array(files, f - files);

  return(0);

 getout:
  ZFREE(newpath);

  return(-1);
}

static void
cleanup_tmpdir(UChar * path)
{
  cleandir(path);
  rmdir(path);
}

Int32
flush_files_to_target(
  UChar		*tmpdir,
  Int32		targetcart,
  Int32		first_file,
  Int32		last_file)
{
  Int32		i, r = 0;
  UChar		tmpf[100], dbuf[8192];
  int		fd = -1;

  if(!noqueued)
    comm_mode = QUEUED_OPERATION;
  if(comm_mode == QUEUED_OPERATION)
    create_command_queue();		/* buffered mode for sending */

  if( (r = setcart(- targetcart)) )
    GETOUT;
  if( (r = setfilenum(filenum > 0 ? first_file : - first_file)) )
    GETOUT;

  for(; first_file <= last_file; first_file++){
    sprintf(tmpf, "%s%s%d", tmpdir, FN_DIRSEPSTR, (int) first_file);

    fd = open(tmpf, O_RDONLY);
    if(fd < 0){
	r = FATAL_ERROR;
	GETOUT;
    }

    if( (r = open_write(1)) )
	CLEANUP;

    if(verbose)
	fprintf(stdout, T_("Writing tape file %d to target tape\n"),
					(int) first_file);

    while( (i = read_forced(fd, dbuf, 8192)) > 0){
	r = send_to_server(dbuf, i);
	if(r)
	  GETOUT;
    }

    close(fd);
    fd = -1;
    unlink(tmpf);

    if( (r = send_pending(YES)) )
	GETOUT;
    if( (r = closetape(0)) )
	GETOUT;
  }

 cleanup:
  if(fd >= 0)
    close(fd);

  if( (i = closetape(1)) )
    if(!r)
      r = i;

  if( (i = delete_command_queue()) )
    if(!r)
      r = i;

  return(r);

 getout:
  CLEANUP;
}

Int32
copytapelocally(Int32 targetcart)
{
  Real64	fsspace = 0.0, bytes_in_files;
  off_t		largest_filesize = 0, bytes_in_file;
  UChar		*tmpfile = NULL, *tmpdir = NULL, *tmpf = NULL, *cptr;
  UChar		dbuf[8192];
  Flag		first_read, save_files, end_of_tape;
  Int32		i, j, n, r = 0, num_files, first_file, sourcecart;
  Int32		curfilenum, numfilescopied;
  int		fd;

  if(toextractfilename){		/* -T option: non-default tmp-dir */
    cptr = strchain(toextractfilename, FN_DIRSEPSTR, "afbcp", NULL);
    if(!cptr)
	CLEANUPR(FATAL_ERROR);
    tmpfile = tmp_name(cptr);
    free(cptr);
    cleanpath(tmpfile);
  }
  else{
    tmpfile = tmp_name(FN_TMPDIR FN_DIRSEPSTR "afbcp");
  }
  tmpdir = strdup(tmpfile);
  tmpf = NEWP(UChar, strlen(tmpfile) + 20);
  if(!tmpfile || !tmpdir || !tmpf)
    CLEANUPR(FATAL_ERROR);

  if( (cptr = FN_LASTDIRDELIM(tmpfile)) )
    *cptr = '\0';

  get_fs_space(tmpfile, &fsspace);
  if(fsspace < 1000000.0){
    errmsg(T_("Error: 1 MB of free space in %s is for sure too few"));
    CLEANUPR(FATAL_ERROR);
  }

  cleanup_proc = (void (*)(void *)) cleanup_tmpdir;
  cleanup_arg = tmpdir;

  if(mkdir(tmpdir, 0700))
    CLEANUPR(errno << 16);

  sourcecart = cart;

  i = setcart(- sourcecart);
  if(i){
    errmsg(T_("Error: %s. Can't set cartridge %d on source server"),
					fault_string(i), sourcecart);
    CLEANUPR(i);
  }

  curfilenum = numfilescopied = 0;

  if(filenum > 0){
    curfilenum = filenum;
    if( (i = setfilenum(filenum)) )
	CLEANUPR(i);
  }

  end_of_tape = NO;
  first_file = -1;
  bytes_in_files = 0.0;

  if(verbose)
    fprintf(stdout, T_("Going to copy cartridge %d to %d\n"),
			(int) sourcecart, (int) targetcart);

  for(num_files = filenum > 0 ? filenum : 1; !end_of_tape; num_files++){
    if(first_file < 0)
	first_file = num_files;

    i = open_read(1);
    if(i){
	save_files = end_of_tape = YES;
	num_files--;
    }
    else{
      if(filenum <= 0)
	curfilenum++;
      if(verbose)
	fprintf(stdout, T_("Copying tape file %d to disk\n"), (int) curfilenum);
      if(filenum > 0)
	curfilenum++;
      numfilescopied++;

      save_files = NO;
      first_read = YES;
      bytes_in_file = 0;

      sprintf(tmpf, "%s%s%d", tmpdir, FN_DIRSEPSTR, (int) num_files);

      fd = open(tmpf, O_WRONLY | O_CREAT | O_TRUNC, 0600);
      if(fd < 0){
	if(first_file == num_files){
	  errmsg(T_("Error: Cannot save entire tape file. Too few space in %s, aborting"),
				tmpdir);
	  CLEANUPR(FATAL_ERROR);
	}

	save_files = YES;
	num_files--;
      }
      else{
        forever{
	  i = receive(dbuf, 8192, &n);

	  if(n > 0){
	    if(num_files == 1 && first_read && filenum <= 0)
		rewrite_tape_label(dbuf, targetcart, servername, portnum);

	    j = write_forced(fd, dbuf, n);
	    bytes_in_file += (j > 0 ? j : 0);

	    if(j < n){
		close(fd);
		if(fd >= 0)
		  fd = -1;
		unlink(tmpf);

		if(first_file == num_files){
		errmsg(T_("Error: Cannot save entire tape file. Too few space in %s, aborting"),
				tmpdir);
		CLEANUPR(FATAL_ERROR);
		}

		num_files--;
		save_files = YES;
		break;
	    }
	  }
	  else{
	    if(first_read){
		save_files = end_of_tape = YES;
		num_files--;
	    }
	  }

	  if(i == ENDOFFILEREACHED){
	    break;
	  }

	  first_read = NO;
        }
      }

      if(fd >= 0)
	close(fd);
      fd = -1;

      bytes_in_files += (Real64) bytes_in_file;

      if( (r = closetape(0)) )
	CLEANUP;

      if(bytes_in_file > largest_filesize)
	largest_filesize = bytes_in_file;
    }

    get_fs_space(tmpfile, &fsspace);
    if(fsspace - bytes_in_files < (Real64) largest_filesize || end_of_tape)
	save_files = YES;		/* force saving of files */

    if(save_files){
	i = flush_files_to_target(tmpdir, targetcart,
					first_file, num_files);
	if(i)
	  CLEANUPR(i);

	if(!end_of_tape){
	  if( (i = setcart(- sourcecart)) )
	    CLEANUPR(i);

	  i = - (num_files + 1);
	  if(filenum > 0)
	    i = - i;		/* positive i.e. logical file number */
	  if( (i = setfilenum(i)) )
	    CLEANUPR(i);
	}

	first_file = -1;

	cleandir(tmpdir);
	bytes_in_files = 0.0;
    }
  }

  if(verbose)
    fprintf(stdout, T_("Copied %d tape files\n"), (int) numfilescopied);

 cleanup:
  cleandir(tmpdir);
  rmdir(tmpdir);

  ZFREE(tmpdir);
  ZFREE(tmpfile);
  ZFREE(tmpf);

  cleanup_proc = NULL;

  return(r);
}

