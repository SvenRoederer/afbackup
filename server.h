/****************** Start of $RCSfile: server.h,v $  ****************
*
* $Source: /home/alb/afbackup/afbackup-3.5.8.12/RCS/server.h,v $
* $Id: server.h,v 1.2 2004/07/08 20:34:45 alb Exp alb $
* $Date: 2004/07/08 20:34:45 $
* $Author: alb $
*
*
******* description ***********************************************
*
*
*
*******************************************************************/

#ifndef	__AFSERVER_H
#define	__AFSERVER_H	__AFSERVER_H

/* tape access states */

#define	NOT_OPEN		0x0

#define	CLOSED_NOTREW		0x1

#define	OPENED_FOR_READ		0x8
#define	OPENED_FOR_WRITE	0x9
#define	OPENED_READING		0x80
#define	OPENED_WRITING		0x90
#define	MODE_MASK		0xff

#define	OPENED_RAW_ACCESS	0x100
#define	MODE_FLAG_MASK		0xff00


/* streamer operating modes */

#define	BUFFERED_OPERATION	0x01
#define	CHANGE_CART_ON_EOT	0x02

#endif	/* !defined(__AFSERVER_H) */
