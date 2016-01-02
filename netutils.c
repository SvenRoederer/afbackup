/****************** Start of $RCSfile: netutils.c,v $  ****************
*
* $Source: /home/alb/afbackup/afbackup-3.5.8.12/RCS/netutils.c,v $
* $Id: netutils.c,v 1.13 2013/11/08 17:58:04 alb Exp alb $
* $Date: 2013/11/08 17:58:04 $
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

  static char * fileversion = "$RCSfile: netutils.c,v $ $Source: /home/alb/afbackup/afbackup-3.5.8.12/RCS/netutils.c,v $ $Id: netutils.c,v 1.13 2013/11/08 17:58:04 alb Exp alb $ " PACKAGE " " VERSION_STRING;

#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/utsname.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <signal.h>
#ifdef	HAVE_SYS_TIME_H
#include <sys/time.h>
#endif
#include <sys/un.h>
#include <netinet/in.h>
#include <netdb.h>
#ifdef	HAVE_NETINET_IN_SYSTM_H
#include <netinet/in_systm.h>
#endif
#ifdef	HAVE_NETINET_IP_H
#include <netinet/ip.h>
#endif
#ifdef	HAVE_NETINET_TCP_H
#include <netinet/tcp.h>
#endif
#ifdef	HAVE_ARPA_INET_H
#include <arpa/inet.h>
#endif
#include <x_types.h>
#include <sysutils.h>
#include <netutils.h>
#include <fileutil.h>
#include <genutils.h>
#ifdef	HAVE_SYS_SELECT_H
#include <sys/select.h>
#endif
#include <poll.h>
#ifdef	HAVE_POSIX_THREADS
#ifdef	HAVE_PTHREAD_H
#include <pthread.h>
#endif
#endif

#ifndef	SOMAXCONN
/* actually 1024 seems not supported by any known OS and according to
 * any documentation found, is reset to sth lower, typically 64 or 128.
 * So read this value: i want as many as possible */
#define	SOMAXCONN	1024
#endif

#ifndef	INADDR_NONE
#define	INADDR_NONE	((unsigned long) -1)
#endif

#define	EMPTY_USER_DATA	((void *) 1)

typedef struct _tcpmux_conn_data {
  int		fd;
  int		saved_fd;
  FILE		*fp;
} TcpMuxConn;

typedef struct _tcpmux_status {
  int		listensock;
  int		unixsock;
  Int32		maxconn;
  Int32		num_conns;
  TcpMuxConn	*conns;
  struct pollfd	*allfds;
  Flag		*closed_conns;
  Flag		have_closed_conns;
  void		**conn_user_data;
  void		(*failed_conn_func)(int, void *, void *);
  void		*(*conn_init_func)(int, Int32, void *, struct sockaddr *,
				void *, TcpMuxCallbDoneActions *);
  void		*data;
  Uns32		flags;
  void		*conn_user_data_to_be_freed;
  struct sigaction	org_sigact;
  int		sync_pipe[2];
  UChar		*errstr;
} TcpMuxStatus;

typedef	struct __hostent_cacheentry {
  struct hostent	entry;
  time_t		timestamp;
} HostCacheEntry;

struct hostent *
get_host_by_name(UChar * name)
{
  static KeyValue	*hostent_cache = NULL;	/* built-in nscd */
#ifdef	HAVE_POSIX_THREADS
  static pthread_mutex_t	hostent_cache_mutex = PTHREAD_MUTEX_INITIALIZER;
#endif
  HostCacheEntry	*hc, *new_hc = NULL;
  struct hostent	*he, hent, *new_he = NULL;
  nodeaddr	netaddr;
  Int32		nal, naddr, i;
  size_t	nalloc = 0;
  char		*cptr, **cpptr, *addrlist[2];
  int		addrtype;

  if(!name)
    return(NULL);

#ifdef	HAVE_POSIX_THREADS
  pthread_mutex_lock(&hostent_cache_mutex);
#endif

  hc = get_named_data(hostent_cache, name, NO, &nalloc);
  if(hc){
    if(hc->timestamp > time(NULL) - 30){
	he = &(hc->entry);
	CLEANUP;
    }
  }

#if	defined(HAVE_GETIPNODEBYNAME) && defined(HAVE_IP6)
 {
  int		error_num;

	/* None of the flags (arg 3) does, what i consider appropriate. */
	/* i guess, most of the networks will move from IP4 to IP6, thus */
	/* introduce new IP6 nodes, that should be able to communicate */
	/* with old and existing IP4 nodes. If there are new IP6 nodes, */
	/* having also a IP4 address, the latter one can happily be used. */
	/* If others should use IP6 to communicate with the host, that */
	/* still has also an IP4 entry, the administrator should gefaelligst */
	/* delete the IP4 entry, because then it makes no sense any more. */
	/* Thus i implement it explicitely requiring 2 lookups :-( */
  he = getipnodebyname(name, AF_INET, 0, &error_num);
  if(!he){
    he = getipnodebyname(name, AF_INET6, 0, &error_num);
    errno = error_num;
  }
 }
#else
  he = gethostbyname(name);
#endif

  /* probably it's a very old system, that is not resolving IP-address strings */
  if(!he){

    i = sizeof(netaddr);
    if(string_to_addr(name, &netaddr, &i, &addrtype)){
	addrlist[0] = (char *)(&netaddr);
	addrlist[1] = NULL;
	hent.h_name = name;
	hent.h_aliases = addrlist + 1;
	hent.h_addrtype = addrtype;
	hent.h_length = i;
	hent.h_addr_list = addrlist;
	he = &hent;
    }
  }

  if(he){
    nalloc = align_n(sizeof(HostCacheEntry), 8) + align_n(strlen(he->h_name) + 1, 8) + 2 * sizeof(char *);

    nal = naddr = 0;
    if(he->h_aliases)		/* can be NULL !?! */
      for(cpptr = he->h_aliases; *cpptr; cpptr++, nal++)
	nalloc += align_n(strlen(*cpptr) + 1, 8) + sizeof(char *);
    if(he->h_addr_list)		/* can be NULL !?! */
      for(cpptr = he->h_addr_list; *cpptr; cpptr++, naddr++)
	nalloc += align_n(he->h_length, 8) + sizeof(char *);

    new_hc = (HostCacheEntry *) NEWP(char, nalloc);
    if(new_hc){
	memcpy(&(new_hc->entry), he, sizeof(struct hostent));
	new_hc->timestamp = time(NULL);
	cptr = (char *) new_hc + sizeof(HostCacheEntry);
	strcpy(cptr, he->h_name);
	new_hc->entry.h_name = cptr;
	cptr += align_n(strlen(cptr) + 1, 8);

	new_hc->entry.h_aliases = (char **) cptr;
	cptr += (nal + 1) * sizeof(char *);
	for(i = 0; i < nal; i++){
	  new_hc->entry.h_aliases[i] = cptr;
	  strcpy(cptr, he->h_aliases[i]);
	  cptr += align_n(strlen(cptr) + 1, 8);
	}
	new_hc->entry.h_aliases[i] = NULL;

	new_hc->entry.h_addr_list = (char **) cptr;
	cptr += (naddr + 1) * sizeof(char *);
	for(i = 0; i < naddr; i++){
	  new_hc->entry.h_addr_list[i] = cptr;
	  memcpy(cptr, he->h_addr_list[i], he->h_length);
	  cptr += he->h_length;
	}
	new_hc->entry.h_addr_list[i] = NULL;

	hc = NULL;
	set_named_data_r(&hostent_cache, name, new_hc, nalloc, (void **)(&hc));
	if(hc)
	  he = &(hc->entry);
    }
    else if(hc){
	unset_named_data(hostent_cache, name);
    }
  }

 cleanup:
  if(he){
    new_he = (struct hostent *) NEWP(char *, nalloc);
    if(new_he)
	memcpy(new_he, he, nalloc);
  }

#ifdef	HAVE_POSIX_THREADS
  pthread_mutex_unlock(&hostent_cache_mutex);
#endif

  return(new_he);
}

/* The IPv4 in IPv6 mapping part: 0:0:0:0:0:0:0:0:0:0:ff:ff */
static	char	IPv4_mapped_part[] = IPv4_IN_IPv6_MAPPING_PART;

struct hostent *
get_host_by_addr(void * addr, int len, int type)
{
  struct hostent	*he;

  if(!addr)
    return(NULL);

#if	defined(HAVE_GETIPNODEBYADDR) && defined(HAVE_IP6)
  {
    int		error_num;

    he = getipnodebyaddr(addr, len, type, &error_num);
    if(!he)
	errno = error_num;
  }

  return(he);
#else

  he = gethostbyaddr(addr, len, type);

#ifdef	HAVE_IP6
  if(!he){
    struct in6_addr	addr6;
    struct in_addr	addr4;

    if(type == AF_INET){
	memset(&addr6, 0, sizeof(addr6));
	memcpy(&addr6, IPv4_mapped_part, sizeof(IPv4_mapped_part));
	memcpy((char *)(&addr6) + sizeof(IPv4_mapped_part),
				addr, sizeof(struct in_addr));

	he = gethostbyaddr(&addr6, sizeof(addr6), AF_INET6);
    }
    else if(type == AF_INET6){
	if(!memcmp(addr, IPv4_mapped_part, sizeof(IPv4_mapped_part))){
	  he = gethostbyaddr((char *) addr + sizeof(IPv4_mapped_part),
				sizeof(addr4), AF_INET);
	}
    }
  }

#endif
  return(he);
#endif
}

struct hostent *
get_host_by_sockaddr(struct sockaddr * addr)
{
  if(!addr)
    return(NULL);

  if(((struct sockaddr *)addr)->sa_family == AF_INET)
    return(get_host_by_addr(&(((struct sockaddr_in *)addr)->sin_addr),
			sizeof(struct in_addr), AF_INET));

#ifdef	HAVE_IP6
  if(((struct sockaddr *)addr)->sa_family == AF_INET6)
    return(get_host_by_addr(&(((struct sockaddr_in6 *)addr)->sin6_addr),
			sizeof(struct in6_addr), AF_INET6));
#endif

  return(NULL);
}

static Flag
accept_connection(TcpMuxStatus * status, Flag in_subr, Flag uxsock)
{
  int		fl, fd, sock;
  Flag		end = NO;
  Int32		sock_optlen[2];
#define	addr_len sock_optlen
  nodeaddr	peeraddr;
  void		*userconndata;
  TcpMuxCallbDoneActions	init_done_actions;

  if(!status)
    return(YES);

  sock = (uxsock ? status->unixsock : status->listensock);

  fl = fcntl(sock, F_GETFL);
  fcntl(sock, F_SETFL, fl | NONBLOCKING_FLAGS);

  *((int *) &(addr_len[0])) = sizeof(peeraddr);
  fd = accept(sock, (struct sockaddr *)(&peeraddr), (int *) &(addr_len[0]));
  if(fd < 0 || (status->maxconn > 0 && status->num_conns + 1 > status->maxconn)){
    if(status->failed_conn_func)
	status->failed_conn_func(fd >= 0 ? fd : sock, status->data, status);

    if(fd >= 0)
	close(fd);
  }
  else{
    fcntl(sock, F_SETFL, fl & INV_NONBLOCKING_FLAGS);

    fl = fcntl(fd, F_GETFL);
    fcntl(fd, F_SETFL, fl & INV_NONBLOCKING_FLAGS);

    set_closeonexec(fd);

    if(in_subr && ! status->conn_user_data_to_be_freed){
		/* the memory space must be valid until exit from input_proc_func, */
		/* cause it might be in use by that function, so we store it in a */
		/* status field and free it later */
	status->conn_user_data_to_be_freed = status->conn_user_data;

	status->conn_user_data = NEWP(void *, status->num_conns + 2);

	memcpy(status->conn_user_data, status->conn_user_data_to_be_freed,
			sizeof(void *) * (status->num_conns + 1));
    }
    else{
	status->conn_user_data =
		ZRENEWP(status->conn_user_data, void *, status->num_conns + 2);
    }

    status->conns = ZRENEWP(status->conns, TcpMuxConn, status->num_conns + 1);
    status->closed_conns = ZRENEWP(status->closed_conns, Flag, status->num_conns + 1);
    status->allfds = ZRENEWP(status->allfds, struct pollfd, 3 + status->num_conns + 1);
    status->allfds[3 + status->num_conns].fd = -1;
    status->allfds[3 + status->num_conns].events = 0;

    status->closed_conns[status->num_conns] = NO;

    SETZERO(status->conns[status->num_conns]);
    status->conns[status->num_conns].saved_fd = -1;

			/* this is for programs using the conn_user_data */
			/* structure to identify the last item */
    status->conn_user_data[status->num_conns + 1] = NULL;

    if(status->conn_init_func){
	SETZERO(init_done_actions);
	init_done_actions.conns_to_close = status->closed_conns;

	userconndata = status->conn_user_data[status->num_conns] =
			 (*status->conn_init_func)(fd,
				status->num_conns, &status->conn_user_data,
				(struct sockaddr *)(&peeraddr), status->data,
				&init_done_actions);
	if(! userconndata){
	  close(fd);
	  fd = -1;
	  if(status->num_conns < 1 && (status->flags & TCPMUX_STOP_ON_LAST_CLOSE))
	    end = YES;
	}

	if(init_done_actions.have_conns_to_close)
	  status->have_closed_conns = YES;
    }
    else{
      status->conn_user_data[status->num_conns] = EMPTY_USER_DATA;
    }

    if(fd >= 0){
	set_socket_keepalive(fd);

	status->conns[status->num_conns].fd = fd;
	status->conns[status->num_conns].fp = fdopen(fd, "r+");

	status->allfds[3 + status->num_conns].fd = fd;
	status->allfds[3 + status->num_conns].events = POLLIN;

	status->num_conns++;
    }
  }

  return(end);
}

Int64
tcp_mux_long_io(
  void *	tmstatptr,
  int		fd,
  UChar *	data,
  Int32		num,
  Int32		do_write,
  Int64		(*rw_func)(int, UChar *, Int64))
{
  struct pollfd	fds[3];
  int		sock, uxsock;
  Int32		i, n;
  TcpMuxStatus	*tcpmux_status;
  Flag		lastconn;

  if(!tmstatptr)
    return((*rw_func)(fd, data, num));

  tcpmux_status = (TcpMuxStatus *) tmstatptr;
  sock = tcpmux_status->listensock;
  uxsock = tcpmux_status->unixsock;

  n = 1;
  fds->fd = fd;
  fds->events = (do_write ? POLLOUT : POLLIN);
  if(sock >= 0){
    fds[1].fd = sock;
    fds[1].events = POLLIN;
    n = 2;
  }
  else{
    fds[1].fd = -1;
    fds[1].events = 0;
  }
  if(uxsock >= 0){
    fds[2].fd = uxsock;
    fds[2].events = POLLIN;
    n = 3;
  }
  else{
    fds[2].fd = -1;
    fds[2].events = 0;
  }

  forever{
    i = poll(fds, n, -1);
    if(i <= 0)
	return(i);

    if(do_write ? POLLOUTEVENT(fds->revents) : POLLINEVENT(fds->revents))
	return((*rw_func)(fd, data, num));

    lastconn = NO;
    if(sock >= 0)
      if(POLLINEVENT(fds[1].revents))
	lastconn = (lastconn || accept_connection(tmstatptr, YES, NO));
    if(uxsock >= 0)
      if(POLLINEVENT(fds[2].revents))
	lastconn = (lastconn || accept_connection(tmstatptr, YES, YES));

    if(lastconn)
	fprintf(stderr, "Internal error in netutils.c, 1.\n");
  }
}

/* when wanting to fork in a callback function of tcp_mux_service,
 * use this function passing the last argument along */
int
tcp_mux_fork(void * param)
{
  TcpMuxStatus	*st;
  int	pid;

  pid = fork_forced();
  if(!pid){
    st = (TcpMuxStatus *) param;

    if(st->listensock >= 0)
	close(st->listensock);
    if(st->unixsock >= 0)
	close(st->unixsock);

    sigaction(SIGPIPE, &(st->org_sigact), NULL);
  }

  return(pid);
}

/* Disable handling of a channel by tcp_mux_service temporarily */
/* This function must be called in the same thread like tcp_mux_service */
int
tcp_mux_unhandle(void * s, int fd)
{
  Int32		i;
  TcpMuxStatus	*status;

  status = (TcpMuxStatus *) s;

  for(i = 0; i < status->num_conns; i++){
    if(status->conns[i].fd == fd){
	if(status->conns[i].saved_fd >= 0){
	  if(status->allfds[i + 3].fd >= 0)
	    return(-2);		/* error: fd and saved_fd must not be both valid */

	  return(1);		/* already turned off */
	}

	status->conns[i].saved_fd = status->allfds[i + 3].fd;
	status->allfds[i + 3].fd = -1;
	status->allfds[i + 3].events = 0;
    }
  }

  return(0);
}

/* Re-enable handling of a temporarily disabled channel by tcp_mux_service */
int
tcp_mux_rehandle(void * s, int fd)
{
  TcpMuxStatus	*status;
  UChar		buf[5];

  status = (TcpMuxStatus *) s;

  buf[0] = 'A';
  Uns32_to_xref(buf + 1, fd);
  write_forced(status->sync_pipe[1], buf, 5);

  return(0);
}

Int32
tcp_mux_service(
  int		port,
  UChar		*ux_socket,
  void		*(*conn_init_func)(int, Int32, void *, struct sockaddr *, void *, TcpMuxCallbDoneActions *),
  Int32		(*input_proc_func)(int, void *, Int32, void *, TcpMuxCallbDoneActions *, void *),
  Int32		(*conn_deinit_func)(int, void *, Int32, void *, void *),
  Int32		maxconn,
  void		(*failed_conn_func)(int, void *, void *),
  Uns32		flags,
  void		*data)
{
  Int32		i, j, k;
  Int32		r = 0;
  Int32		actconn;
  nodeaddr	addr;
  int		sock_optval, sockfd_socktype;
  int		sock_typesize, sock_family, f;
  Int32		sock_optlen[2];
  Flag		end = NO;
  UChar		buf[5];
  TcpMuxStatus	status;
  TcpMuxCallbDoneActions	input_done_actions;
  struct sigaction	pipe_sigact;

  SETZERO(status);
  status.maxconn = maxconn;
  status.failed_conn_func = failed_conn_func;
  status.conn_init_func = conn_init_func;
  status.data = data;
  status.flags = flags;
  status.listensock = -1;
  status.unixsock = -1;
  status.sync_pipe[0] = status.sync_pipe[1] = -1;
  status.allfds = NEWP(struct pollfd, 3);
  if(!status.allfds)
    CLEANUPR(-1);

  if(pipe(status.sync_pipe) < 0)
    CLEANUPR(-1);

  for(i = 0; i < 3; i++){
    status.allfds[i].fd = -1;
    status.allfds[i].events = 0;
  }

  SETZERO(pipe_sigact);
  pipe_sigact.sa_handler = SIG_IGN;
#ifdef	SA_RESTART
  pipe_sigact.sa_flags = SA_RESTART;
#endif
  sigaction(SIGPIPE, &pipe_sigact, &status.org_sigact);

  if(flags & TCPMUX_INETD_STARTED){
    status.listensock = 0;
  }
  else{
#ifdef	HAVE_IP6
    status.listensock = socket(sock_family = AF_INET6, SOCK_STREAM, 0);
    if(status.listensock < 0)
#endif
     status.listensock = socket(sock_family = AF_INET, SOCK_STREAM, 0);
    if(status.listensock >= 0){
	*((int *) &(sock_optlen[0])) = sizeof(int);
	i = getsockopt(status.listensock, SOL_SOCKET, SO_TYPE,
			(char *) &sockfd_socktype, (int *) &(sock_optlen[0]));
	if(i)
	  sockfd_socktype = -1;

	if(sockfd_socktype == SOCK_STREAM){
	  sock_optval = 1;
	  i = setsockopt(status.listensock, SOL_SOCKET, SO_REUSEADDR,
				(char *) &sock_optval, sizeof(int));
	  /* i is not evaluated: if set REUSEADDR fails, the bind
		below fails. That's sufficient */
	}

#ifdef	HAVE_IP6
	i = -1;
	if(sock_family == AF_INET6){
	  ((struct sockaddr *)(&addr))->sa_family = AF_INET6;
	  ((struct sockaddr_in6 *)(&addr))->sin6_port = htons(port);
	  SETZERO(((struct sockaddr_in6 *)(&addr))->sin6_addr.s6_addr);
	  sock_typesize = sizeof(struct sockaddr_in6);

	  i = bind(status.listensock, (struct sockaddr *)(&addr), sock_typesize);
	}
	if(i){
#endif

	((struct sockaddr *)(&addr))->sa_family = AF_INET;
	((struct sockaddr_in *)(&addr))->sin_port = htons(port);
	SETZERO(((struct sockaddr_in *)(&addr))->sin_addr.s_addr);
	sock_typesize = sizeof(struct sockaddr_in);

	if( (i = bind(status.listensock, (struct sockaddr *)(&addr), sock_typesize)) ){
	  close(status.listensock);
	  status.listensock = -1;
	}
    
#ifdef	HAVE_IP6
	}
#endif

	if(!i){
	  set_closeonexec(status.listensock);

	  if(listen(status.listensock, SOMAXCONN)){
	    close(status.listensock);
	    status.listensock = -1;
	  }
	}
    }
  }

  if(ux_socket){
    if(flags & TCPMUX_UXSOCK_IS_FILEDESC)
	status.unixsock = *((int *) ux_socket);
    else
	status.unixsock = create_unix_socket(ux_socket);
    if(status.unixsock >= 0){
	set_closeonexec(status.unixsock);

	i = listen(status.unixsock, SOMAXCONN);
	if(i){
	  close(status.unixsock);
	  status.unixsock = -1;
	}
    }
  }

  if(status.listensock < 0 && status.unixsock < 0)
    CLEANUPR(-1);

  if(status.listensock >= 0){
    status.allfds[0].fd = status.listensock;
    status.allfds[0].events = POLLIN;
  }
  if(status.unixsock >= 0){
    status.allfds[1].fd = status.unixsock;
    status.allfds[1].events = POLLIN;
  }
  status.allfds[2].fd = status.sync_pipe[0];
  status.allfds[2].events = POLLIN;

  actconn = status.num_conns;	/* force accept first */

  do{
    status.have_closed_conns = NO;

    i = poll(status.allfds, 3 + status.num_conns, -1);

    memset(status.closed_conns, 0, status.num_conns * sizeof(status.closed_conns[0]));

    for(i = -1; i < status.num_conns; i++){	/* one more because of */
	if(actconn >= status.num_conns){	/* rotating accept and */
	  if(status.listensock >= 0){
	    if(POLLINEVENT(status.allfds[0].revents)){	/* continue */
	      if(accept_connection(&status, NO, NO))
		end = YES;
	    }
	  }
	  if(status.unixsock >= 0){
	    if(POLLINEVENT(status.allfds[1].revents)){	/* continue */
	      if(accept_connection(&status, NO, YES))
		end = YES;
	    }
	  }

		/* channel reenable request from tcp_mux_rehandle s.a. */
	  if(POLLINEVENT(status.allfds[2].revents)){
	    if(read_forced(status.sync_pipe[0], buf, 5) == 5){
		if(buf[0] == 'A'){
		  xref_to_Uns32(&f, buf + 1);
		  for(k = 0; k < status.num_conns; k++){
		    if(status.conns[k].fd == f){
		      if(status.conns[k].saved_fd >= 0 && status.allfds[k + 3].fd == -1){
			status.allfds[k + 3].fd = status.conns[k].saved_fd;
			status.allfds[k + 3].events = POLLIN;
			status.conns[k].saved_fd = -1;
		      }
		    }
		  }
		}
		else{
		  status.errstr = "Internal Error: No A from syncpipe";
		}
	    }
	  }

	  actconn = 0;

	  continue;
	}

	if(POLLINEVENT(status.allfds[actconn + 3].revents)){
		/* the additional check for input makes no sense. However, */
		/* i experienced, that we get here and there's no input :-( */
	  if(input_proc_func && wait_for_input(status.conns[actconn].fd, 0)){
	    SETZERO(input_done_actions);
	    input_done_actions.conns_to_close = status.closed_conns;

	    j = (*input_proc_func)(status.conns[actconn].fd,
				status.conn_user_data + actconn, actconn, data,
				&input_done_actions, &status);

	    if(j)
		status.closed_conns[actconn] = status.have_closed_conns = YES;

	    ZFREE(status.conn_user_data_to_be_freed);

	    if(input_done_actions.have_conns_to_close)
		status.have_closed_conns = YES;
	  }
	}

	actconn++;
    }

    if(status.have_closed_conns){
      for(i = 0; i < status.num_conns; i++){
	if(status.closed_conns[i]){
	  if(conn_deinit_func){
	    j = (*conn_deinit_func)(status.conns[i].fd, status.conn_user_data + i, i,
					data, &status);
	  }

	  status.allfds[i + 3].fd = -1;
	  status.allfds[i + 3].events = 0;

	  if(status.conns[i].fp)
	    fclose(status.conns[i].fp);
	  else
	    close(status.conns[i].fd);

	  memmove(status.conns + i, status.conns + i + 1,
			(status.num_conns - i - 1) * sizeof(status.conns[0]));
	  memmove(status.closed_conns + i, status.closed_conns + i + 1,
			(status.num_conns - i - 1) * sizeof(status.closed_conns[0]));
	  memmove(status.conn_user_data + i, status.conn_user_data + i + 1,
			(status.num_conns - i) * sizeof(status.conn_user_data[0]));
	  memmove(status.allfds + i + 3, status.allfds + i + 3 + 1,
			(status.num_conns - i - 1) * sizeof(struct pollfd));
	  i--;
	  status.num_conns--;

	  if(status.num_conns < 1 && (flags & TCPMUX_STOP_ON_LAST_CLOSE)){
	    end = YES;
	    break;
	  }
	}
      }
    }
  } while(! end);

 cleanup:

  if(status.listensock >= 0)
    close(status.listensock);
  if(status.unixsock >= 0)
    close(status.unixsock);
  if(status.sync_pipe[0] >= 0)
    close(status.sync_pipe[0]);
  if(status.sync_pipe[1] >= 0)
    close(status.sync_pipe[1]);

  ZFREE(status.conns);
  ZFREE(status.closed_conns);
  ZFREE(status.conn_user_data);
  ZFREE(status.allfds);

  sigaction(SIGPIPE, &status.org_sigact, NULL);

  return(r);
}

/*
 * Use configured name services to get our official host name.  Returns
 * a malloc'd string on success, NULL on failure.
 */

UChar *
get_my_off_hn()
{
  struct utsname	unamb;
  struct hostent	*hp;
  UChar		*res;

  if(uname(&unamb) < 0)
    return(NULL);

  hp = get_host_by_name(unamb.nodename);
  if(!hp)
    return(NULL);

  res = strdup(hp->h_name);

  free(hp);
  return(res);
}

Int8
same_host(UChar * hn1, UChar * hn2)
{
  struct hostent	*hep1 = NULL, *hep2 = NULL;
  Int32		i, j, n1, n2, l1, l2;
  Int8		r = 0, ce;
  char		*addresses1 = NULL, *addresses2 = NULL;

  if(! hn1 || ! hn2){
    errno = EINVAL;
    return(-1);
  }

  ce = (strcmp(hn1, hn2) ? 0 : 2);

  if(ce)
    return(ce);

  hep1 = get_host_by_name(hn1);
  if(!hep1)
    return(ce);

  l1 = hep1->h_length;
  for(n1 = 0; hep1->h_addr_list[n1]; n1++);
  addresses1 = NEWP(UChar, n1 * l1);
  if(!addresses1){
    r = - errno;
    GETOUT;
  }

  for(i = 0; i < n1; i++)
    memcpy(addresses1 + i * l1, hep1->h_addr_list[i], l1);

  hep2 = get_host_by_name(hn2);
  if(!hep2)
    CLEANUPR(ce);

  l2 = hep2->h_length;
  for(n2 = 0; hep2->h_addr_list[n2]; n2++);
  addresses2 = NEWP(UChar, n2 * l2);
  if(!addresses2){
    r = - errno;
    GETOUT;
  }

  for(i = 0; i < n2; i++)
    memcpy(addresses2 + i * l2, hep2->h_addr_list[i], l2);

  if(l1 != l2){
    r = ce;
    CLEANUP;
  }

  for(i = 0; i < n1; i++){
    for(j = 0; j < n2; j++){
	if(! memcmp(addresses1 + i * l1, addresses2 + j * l2, l1)){
	  r = 1;
	  i = n1;
	  break;
	}
    }
  }

 cleanup:
  ZFREE(addresses1);
  ZFREE(addresses2);
  ZFREE(hep1);
  ZFREE(hep2);

  return(r);

 getout:
  CLEANUP;
}

Int32
ip4addr_from_ip6addr(struct sockaddr * addr)
{
  if(!addr){
    errno = EINVAL;
    return(4);
  }
#ifndef	HAVE_IP6

  return(3);

#else
 {
  struct sockaddr_in	ip4addr;
  struct sockaddr_in6	*ip6addrp;

  if(addr->sa_family != AF_INET6)
    return(2);

  ip6addrp = (struct sockaddr_in6 *) addr;
  if(memcmp(&ip6addrp->sin6_addr, &IPv4_mapped_part, sizeof(IPv4_mapped_part)))
    return(1);

  SETZERO(ip4addr);
  ip4addr.sin_family = AF_INET;
  ip4addr.sin_port = ip6addrp->sin6_port;
  memcpy(& ip4addr.sin_addr,
		(UChar *)(&ip6addrp->sin6_addr) + sizeof(IPv4_mapped_part),
				sizeof(ip4addr.sin_addr));

  memcpy(addr, &ip4addr, sizeof(ip4addr));
 }

  return(0);
#endif
}

Int32
ip6addr_from_ip4addr(struct sockaddr * addr)
{
  if(!addr){
    errno = EINVAL;
    return(4);
  }
#ifndef	HAVE_IP6

  return(0);	/* there are no IP6 addresses, nothing to convert */

#else
 {
  struct sockaddr_in	*ip4addrp;
  struct sockaddr_in6	ip6addr;

  if(addr->sa_family == AF_INET6)
    return(0);
  if(addr->sa_family != AF_INET)
    return(-1);

  ip4addrp = (struct sockaddr_in *) addr;

  SETZERO(ip6addr);
  ip6addr.sin6_family = AF_INET6;
  ip6addr.sin6_port = ip4addrp->sin_port;

  memcpy(& ip6addr.sin6_addr, &IPv4_mapped_part, sizeof(IPv4_mapped_part));
  memcpy((UChar *)(&ip6addr.sin6_addr) + sizeof(IPv4_mapped_part),
		&ip4addrp->sin_addr, sizeof(ip4addrp->sin_addr));
 }

  return(0);
#endif
}

/*
 * Takes <servicename>, which may either be a number or a name as specified
 * in /etc/services or in a name service (according to /etc/nsswitch.conf),
 * and returns the associated port number as an int, always for protocol TCP
 * Returns a value < 0 on error.
 */

int
get_tcp_portnum(UChar * servicename)
{
  struct servent	*se;
  int			n, p, len, l;
  UChar			*sn_cp;

  if(!servicename){
    errno = EINVAL;
    return(-1);
  }

  sn_cp = strdup(servicename);
  if(!sn_cp)
    return(-1);

  massage_string(sn_cp);
  len = strlen(sn_cp);

  l = -1;
  n = sscanf(sn_cp, "%d%n", &p, &l);
  free(sn_cp);

  if(n > 0 && len == l)
    return(p);

  se = getservbyname(servicename, "tcp");
  if(!se)
    return(-1);

  return(ntohs(se->s_port));
}

Int32
anon_tcp_sockaddr(
  struct sockaddr	*addr,
  Int32		*sockaddrsize,
  int		addrfamily)
{
  if(addrfamily != AF_INET
#ifdef	HAVE_IP6
			&& addrfamily != AF_INET6
#endif
							)
    return(-5);

  if(addrfamily == AF_INET){
    if(addr){
	SETZERO(((struct sockaddr_in *)addr)[0]);
	((struct sockaddr_in *)addr)->sin_port = htons(0);
    }
    if(sockaddrsize)
	*sockaddrsize = sizeof(struct sockaddr_in);
  }
#ifdef	HAVE_IP6
  if(addrfamily == AF_INET6){
    if(addr){
	SETZERO(((struct sockaddr_in6 *)addr)[0]);
	((struct sockaddr_in6 *)addr)->sin6_port = htons(0);
    }
    if(sockaddrsize)
	*sockaddrsize = sizeof(struct sockaddr_in6);
  }
#endif

  if(addr)
    addr->sa_family = addrfamily;

  return(0);
}

Int32
set_sockaddr_port(
  struct sockaddr	*addr,
  int		port)
{
  if(!addr)
    return(0);

  if(addr->sa_family != AF_INET
#ifdef	HAVE_IP6
			&& addr->sa_family != AF_INET6
#endif
							)
    return(-5);

  if(addr->sa_family == AF_INET){
	((struct sockaddr_in *)addr)->sin_port = htons(port);
  }
#ifdef	HAVE_IP6
  if(addr->sa_family == AF_INET6){
	((struct sockaddr_in6 *)addr)->sin6_port = htons(port);
  }
#endif

  return(0);
}

void *
inaddr_from_sockaddr(struct sockaddr * sockaddr, Int32 * addrsize)
{
  void		*retaddr = NULL;

  if(!sockaddr)
    return(NULL);

  if(sockaddr->sa_family != AF_INET
#ifdef	HAVE_IP6
			&& sockaddr->sa_family != AF_INET6
#endif
							)
    return(NULL);

  if(sockaddr->sa_family == AF_INET){
    retaddr = &(((struct sockaddr_in *)sockaddr)->sin_addr);
    if(addrsize)
	*addrsize = sizeof(struct in_addr);
  }
#ifdef	HAVE_IP6
  if(sockaddr->sa_family == AF_INET6){
    retaddr = &(((struct sockaddr_in6 *)sockaddr)->sin6_addr);
    if(addrsize)
	*addrsize = sizeof(struct in6_addr);
  }
#endif

  return(retaddr);
}

Int32
set_tcp_sockaddr_hp(
  struct sockaddr	*addr,
  struct hostent	*hp,
  int		portnum)
{
  if(!hp){
    errno = EINVAL;
    return(-6);
  }
  if(!addr)
    return(0);

  if(hp->h_addrtype != AF_INET
#ifdef	HAVE_IP6
			&& hp->h_addrtype != AF_INET6
#endif
							)
    return(-5);

  addr->sa_family = hp->h_addrtype;

  if(hp->h_addrtype == AF_INET){
    if(hp)
	memcpy(&(((struct sockaddr_in *)addr)->sin_addr),
					hp->h_addr, hp->h_length);
    if(portnum >= 0)
	((struct sockaddr_in *)addr)->sin_port = htons(portnum);
  }
#ifdef	HAVE_IP6
  if(hp->h_addrtype == AF_INET6){
    if(hp)
	memcpy(&(((struct sockaddr_in6 *)addr)->sin6_addr),
					hp->h_addr, hp->h_length);
    if(portnum >= 0)
	((struct sockaddr_in6 *)addr)->sin6_port = htons(portnum);
  }
#endif

  return(0);
}

#ifndef	IPPORT_RESERVED
#define	IPPORT_RESERVED	1024
#endif
#define	MINPORT	550
#define	MAXPORT	(IPPORT_RESERVED - 1)

/* return value <= - 128: may be a temporary resource shortage */
/* negative fallback_port means: local privileged port requested */
int
open_tcpip_conn(UChar * hostname, UChar * servname, Int32 fallback_port)
{
  nodeaddr	addr;
  struct hostent	*hp;
  struct linger	linger;
  int		sockfd, fl, startport = 0, srcport, sock_optval, r = 0;
  Int32		sock_typesize, i, portnum;

  if(!hostname){
    errno = EINVAL;
    return(-1);
  }

  sockfd = -1;

  if(fallback_port < 0){
    fallback_port = - fallback_port;
    startport = srcport = (int) (drandom() * (double)(MAXPORT + 1 - MINPORT)) + MINPORT;
  }

  if(!servname){
    portnum = fallback_port;
  }
  else{
    portnum = get_tcp_portnum(servname);
    if(portnum < 0)
	return(-3);
  }

  hp = get_host_by_name(hostname);
  if(!hp)
    return(-2);

  if( (i = anon_tcp_sockaddr((struct sockaddr *)(&addr), &sock_typesize, hp->h_addrtype)) )
    GETOUTR(i);

  sockfd = socket(hp->h_addrtype, SOCK_STREAM, 0);
  if(sockfd < 0)
    GETOUTR(-4 - 128);

  sock_optval = 1;
  i = setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR,
				(char *) &sock_optval, sizeof(int));
  i = -1;
  if(startport){
    for(srcport++; srcport != startport; srcport++){
	if(srcport > MAXPORT)
	  srcport = MINPORT;

	set_sockaddr_port((struct sockaddr *)(&addr), srcport);

	i = bind(sockfd, (struct sockaddr *)(&addr), sock_typesize);
	if(! i)
	  break;
    }
    set_sockaddr_port((struct sockaddr *)(&addr), 0);
  }

  if(i)
    i = bind(sockfd, (struct sockaddr *)(&addr), sock_typesize);
  if(i)
    GETOUTR(-5 - 128);

  set_tcp_sockaddr_hp((struct sockaddr *)(&addr), hp, portnum);

  if(connect(sockfd, (struct sockaddr *)(&addr), sock_typesize) < 0){
    i = 128;

    if(errno == EACCES || errno == EPERM
#ifdef	ENETUNREACH
	|| errno == ENETUNREACH
#endif
#ifdef	EADDRNOTAVAIL
	|| errno == EADDRNOTAVAIL
#endif
		)
	i = 0;

    GETOUTR(-6 - i);
  }

#if 0
  linger.l_onoff = 1;
  linger.l_linger = 5;
  i = setsockopt(sockfd, SOL_SOCKET, SO_LINGER, (char *) &linger,
						sizeof(linger));
#endif

  fl = fcntl(sockfd, F_GETFL);
  fcntl(sockfd, F_SETFL, fl & INV_NONBLOCKING_FLAGS);

  r = sockfd;

 cleanup:
  ZFREE(hp);

  return(r);

 getout:
  if(sockfd >= 0)
    close(sockfd);
  CLEANUP;
}

Int32
set_ip_throughput(int sockfd)
{
    int		sock_optval;
    Int32	r, i;

    r = 0;

#if defined(IP_TOS) && defined(IPTOS_THROUGHPUT)
    sock_optval = IPTOS_THROUGHPUT;
    if( (i = setsockopt(sockfd, IPPROTO_IP, IP_TOS,
				(char *) &sock_optval, sizeof(int))) < 0)
	r -= 1;
#endif

    return(r);
}

Int32
set_tcp_nodelay(int sockfd, Flag turn_on)
{
    int			sock_optval, sockfd_socktype;
    Int32		sock_optlen[2], r, i;

    r = 0;

	/* I use 32 Bit int for sock_optlen. See server.c for a *comment* */
    *((int *) &(sock_optlen[0])) = sizeof(int);
    i = getsockopt(sockfd, SOL_SOCKET, SO_TYPE,
			(char *) &sockfd_socktype, (int *) &(sock_optlen[0]));
    if(i){
	r -= 1;
	sockfd_socktype = -1;
    }

#ifdef	TCP_NODELAY
    sock_optval = (turn_on ? 1 : 0);
    if(sockfd_socktype == SOCK_STREAM){
      if( (i = setsockopt(sockfd, IPPROTO_TCP, TCP_NODELAY,
				(char *) &sock_optval, sizeof(int))) < 0)
	r -= 2;
    }
#endif

    return(r);
}

Int32
set_socket_keepalive(int sockfd)
{
    int		sock_optval;
    Int32	r, i;

    r = 0;

#ifdef	SO_KEEPALIVE
    sock_optval = 1;
    if( (i = setsockopt(sockfd, SOL_SOCKET, SO_KEEPALIVE,
				(char *) &sock_optval, sizeof(int))) < 0)
	r -= 1;
#endif

    return(r);
}

UChar *
addr_to_string(int socktype, void * addr)
{
  UChar		*ascaddr = NULL, abuf[256];

  if(!addr)
    return(NULL);

  if(socktype != AF_INET
#ifdef	HAVE_IP6
  			&& socktype != AF_INET6
#endif
						)
    return(NULL);

#ifdef	HAVE_INET_NTOP		/* this should work, if IP6 is present */

    if(inet_ntop(socktype, addr, abuf, 256))
	ascaddr = strdup(abuf);

#else

    if(socktype == AF_INET){

#ifdef	HAVE_INET_NTOA

	ascaddr = strdup(inet_ntoa(*((struct in_addr *) addr)));

#else

	{
	  char		naddr[32];	/* manually: assuming IP-V4-address */
	  unsigned long	laddr;

	  laddr = ntohl(*((Uns32 *)addr));

	  sprintf(naddr, "%u.%u.%u.%u", laddr >> 24, (laddr >> 16) & 0xff,
				(laddr >> 8) & 0xff, laddr & 0xff);
	  ascaddr = strdup(naddr);
	}	  

#endif	/* defined(HAVE_INET_NTOA) */

    }

#endif	/* defined(HAVE_INET_NTOP) */

  return(ascaddr);
}

UChar *
sockaddr_to_string(struct sockaddr * saddr)
{
  int		s_family;
  void		*addr;
  nodeaddr	netaddr;

  if(!saddr)
    return(NULL);

  s_family = saddr->sa_family;

  if(s_family == AF_INET){
    addr = &(((struct sockaddr_in *) saddr)->sin_addr);
  }
#ifdef	HAVE_IP6
  else if(s_family == AF_INET6){
    memcpy((void *)(&netaddr), saddr, sizeof(struct sockaddr_in6));
    if(!ip4addr_from_ip6addr((struct sockaddr *)(&netaddr))){
	addr = &(((struct sockaddr_in *) (&netaddr))->sin_addr);
	s_family = ((struct sockaddr *) (&netaddr))->sa_family;
    }
  }
#endif
  else if(s_family == AF_UNIX){
    return(strdup(((struct sockaddr_un *) saddr)->sun_path));
  }
  else{
    return(NULL);
  }

  return(addr_to_string(s_family, addr));
}

/* str is the ascii representation of an IP(6) address. The binary
 * address representation is returned in raddr, if not NULL. If the
 * sizep is not NULL and raddr is neither NULL, the value pointed
 * to must contain the size of the available space in raddr.
 * If raddr is NULL, space is allocated and returned and must be
 * freed by the caller, if no longer needed. The value pointed to
 * by sizep, if not NULL, is modified to the appropriate value, if
 * the address could be converted. Then also the value pointed to
 * by typep, is set to AF_INET or AF_INET6, respectively.
 * If raddr is not NULL, but the value indicated by *sizep is not
 * large enough, NULL is returned. However, *typep and *sizep are
 * set appropriately, if the string could be converted.
 * To just find out, if a string can be converted, pass *sizep set
 * to 0, a non-NULL dummy pointer in raddr and check, if *sizep is
 * > 0 afterwards */
void *
string_to_addr(UChar * str, void * raddr, Int32 * sizep, int * typep)
{
  nodeaddr	netaddr;
  Int32		r;
  unsigned long	a;
  int		atype;

  atype = AF_INET;

#ifdef	HAVE_INET_PTON
#ifdef	HAVE_IP6

  if(inet_pton(AF_INET6, str, (void *)(&netaddr)) > 0){
    atype = AF_INET6;
    CLEANUPR(sizeof(struct in6_addr));
  }

#endif	/* defined(HAVE_IP6) */

  if(inet_pton(AF_INET, str, (void *)(&netaddr)) > 0)
    CLEANUPR(sizeof(struct in_addr));

#endif	/* defined(HAVE_INET_PTON) */
#ifdef	HAVE_INET_ATON

  if(inet_aton(str, (struct in_addr *)(&netaddr)))
    CLEANUPR(sizeof(struct in_addr));

#endif	/* defined(HAVE_INET_ATON) */

  a = inet_addr(str);
  if(a != INADDR_NONE){
    memcpy((void *)(&netaddr), &a, sizeof(a));
    CLEANUPR(sizeof(a));
  }

  if(sizep)
    *sizep = -1;

  return(NULL);

 cleanup:
  if(typep)
    *typep = atype;

  if(sizep){
    a = *sizep;
    *sizep = r;

    if(raddr)
      if(r > a)
	return(NULL);
  }

  if(!raddr){
    raddr = NEWP(UChar, r);
    if(!raddr)
	return(NULL);
  }

  memcpy(raddr, (void *)(&netaddr), r);

  return(raddr);
}

UChar *
get_hostnamestr(struct sockaddr * peeraddr)
{
  struct hostent	*hp;
  UChar		*hostname = NULL;
  void		*addr;
  Int32		alen;

  if(!peeraddr)
    return(get_my_off_hn());

  if(peeraddr->sa_family == AF_UNIX)
    return(strdup("localhost"));

  if(peeraddr->sa_family == AF_INET){
    addr = &(((struct sockaddr_in *)peeraddr)->sin_addr);
    alen = sizeof(struct in_addr);
  }

#ifdef	HAVE_IP6

  else if(peeraddr->sa_family == AF_INET6){
    addr = &(((struct sockaddr_in6 *) peeraddr)->sin6_addr);
    alen = sizeof(struct in6_addr);
  }

#endif

  else
    return(NULL);

  hp = get_host_by_addr(addr, alen, peeraddr->sa_family);
  if(hp)
    hostname = strdup(hp->h_name);
  else
    hostname = addr_to_string(peeraddr->sa_family, addr);

  return(hostname);
}

Int32
port_from_sockaddr(struct sockaddr * addr)
{
  Int32		port;
  short int	port_raw;

  if(!addr){
    errno = EINVAL;
    return(-1);
  }

  if(addr->sa_family == AF_INET){
    port_raw = ((struct sockaddr_in *)addr)->sin_port;
  }
#ifdef	HAVE_IP6
  else if(addr->sa_family == AF_INET6){
    port_raw = ((struct sockaddr_in6 *)addr)->sin6_port;
  }
#endif
  else{
    errno = EINVAL;
    return(-1);
  }

  return(ntohs(port_raw));
}

UChar *
get_connected_peername(int sock)
{
  int		i;
  nodeaddr	peeraddr;
  Int32		len[2];

  len[0] = len[1] = 0;
  *((int *) &(len[0])) = sizeof(peeraddr);
  i = getpeername(sock, (struct sockaddr *) &peeraddr, (int *) &(len[0]));
  if(i){
    return(NULL);
  }

  return(get_hostnamestr((struct sockaddr *)(&peeraddr)));
}

/* *comment* on getsockopt argument 5:
 * The IBM had the *great* idea to change the type of argument 5 of
 * getsockopt (and BTW argument 3 of getpeername) to size_t *. On the
 * Digital Unix size_t is an 8 Byte unsigned integer. Therefore the
 * safety belt 
 *  int sock_optlen[2].
 * I tried to find a workaround using autoconfig. Parsing the Headers
 * using cpp and perl or awk does not lead to satisfying results. Using
 * the TRY_COMPILE macro of autoconfig fails sometimes. E.G. the DEC
 * C-compiler does not complain on wrong re-declarations (neither via
 * error-message nor by the exit status). Try yourself, if you don't
 * believe. Furthermore there are 4 ways of declaration i found on
 * several systems. 4th argument as char * or void *, 5th arg as
 * explained of type int * or size_t *. Testing all these is annoying.
 * Maybe some other vendors have other ideas, how getsockopt should
 * be declared (sigh). Therefore i use two 32-Bit integers. Even if
 * a system call thinks, it must put there a 64-Bit int, the program
 * does not fail.
 */
