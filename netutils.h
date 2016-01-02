/****************** Start of $RCSfile: netutils.h,v $  ****************
*
* $Source: /home/alb/afbackup/afbackup-3.5.8.12/RCS/netutils.h,v $
* $Id: netutils.h,v 1.7 2013/08/04 12:43:08 alb Exp alb $
* $Date: 2013/08/04 12:43:08 $
* $Author: alb $
*
*
******* description ***********************************************
*
*
*
*******************************************************************/

#include <lconf.h>

#ifndef __NETUTILS_H
#define	__NETUTILS_H	__NETUTILS_H

#include <stdio.h>
#include <sys/types.h>
#ifdef	HAVE_SYS_SELECT_H
#include <sys/select.h>
#endif
#include <sys/socket.h>
#include <netdb.h>
#include <netinet/in.h>

#include <x_types.h>
#include <genutils.h>
#include <sysutils.h>

/* The IPv4 in IPv6 mapping part: 0:0:0:0:0:0:0:0:0:0:ff:ff */
#define	IPv4_IN_IPv6_MAPPING_PART	{	\
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,	\
	0xff, 0xff }

#define	TCPMUX_INETD_STARTED		1
#define	TCPMUX_STOP_ON_LAST_CLOSE	( TCPMUX_INETD_STARTED << 1 )
#define	TCPMUX_UXSOCK_IS_FILEDESC	( TCPMUX_INETD_STARTED << 8 )

typedef	struct _tcpmux_input_done_actions {
  Flag	*conns_to_close;
  Flag	have_conns_to_close;
} TcpMuxCallbDoneActions;

typedef	struct _node_address_space {
  char	just_space[256];
}	nodeaddr;

#ifdef	__cplusplus
extern	"C"	{
#endif

#define	open_tcpip_conn_resvp(host, srvname, fb_port)	open_tcpip_conn((host), (srvname), - (fb_port))
extern	int	open_tcpip_conn(UChar *, UChar *, Int32);
extern	int	open_tcpip_conn_forced(UChar *, UChar *, Int32, Int32);
extern	int	get_tcp_portnum(UChar *);
extern	Int8	same_host(UChar *, UChar *);
extern	Int32	set_ip_throughput(int);
extern	Int32	set_tcp_nodelay(int, Flag);
extern	Int32	set_socket_keepalive(int);
extern	UChar	*get_connected_peername(int);
extern	UChar	*get_hostnamestr(struct sockaddr *);
extern	Int32	port_from_sockaddr(struct sockaddr *);
extern	UChar	*get_my_off_hn();
extern	struct hostent	*get_host_by_name(UChar *);
extern	struct hostent	*get_host_by_addr(void *, int, int);
extern	struct hostent	*get_host_by_sockaddr(struct sockaddr *);
extern	Int32	ip4addr_from_ip6addr(struct sockaddr *);
extern	Int32	ip6addr_from_ip4addr(struct sockaddr *);
extern	UChar	*addr_to_string(int, void *);
extern	UChar	*sockaddr_to_string(struct sockaddr *);
extern	void	*string_to_addr(UChar *, void *, Int32 *, int *);
extern	Int32	set_tcp_sockaddr_hp(struct sockaddr *, struct hostent *, int);
extern	Int32	anon_tcp_sockaddr(struct sockaddr *, Int32 *, int);
extern	Int32	set_sockaddr_port(struct sockaddr *, int);
extern	void	*inaddr_from_sockaddr(struct sockaddr *, Int32 *);
extern	Int32	tcp_mux_service(int, UChar *,
			void *(*)(int, Int32, void *, struct sockaddr *,
					void *, TcpMuxCallbDoneActions *),
			Int32 (*)(int, void *, Int32, void *,
					TcpMuxCallbDoneActions *, void *),
			Int32 (*)(int, void *, Int32, void *, void *),
			Int32, void (*)(int, void *, void *), Uns32, void *);
extern	Int64	tcp_mux_long_io(void *, int, UChar *, Int32, Int32,
					Int64 (*)(int, UChar *, Int64));
extern	int	tcp_mux_unhandle(void *, int);
extern	int	tcp_mux_rehandle(void *, int);
#define	tcp_mux_long_write(tmstat, fd, data, num)	\
		tcp_mux_long_io(tmstat, fd, data, num, 1, write_forced)
#define	tcp_mux_long_read(tmstat, fd, data, num)	\
		tcp_mux_long_io(tmstat, fd, data, num, 0, read_forced)
extern	int	tcp_mux_fork(void *);

#ifdef	__cplusplus
}
#endif

#endif	/* ! defined(__NETUTILS_H) */

