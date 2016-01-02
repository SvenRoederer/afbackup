/****************** Start of $RCSfile: prot.h,v $  ****************
*
* $Source: /home/alb/afbackup/afbackup-3.5.8.12/RCS/prot.h,v $
* $Id: prot.h,v 1.3 2004/07/08 20:34:45 alb Exp alb $
* $Date: 2004/07/08 20:34:45 $
* $Author: alb $
*
*
******* description ***********************************************
*
*
*
*******************************************************************/

#include <x_types.h>
#include <budefs.h>

typedef	struct __afb_protocol {
  Uns32		cmd;
  Int32		num_fix_in;	/* in means: to server */
  Int32		num_fix_out;
  Int32		pos_num_in;	/* position count starts with 1 ! */
  Int32		size_num_in;
  Int32		pos_num_out;	/* dito */
  Int32		size_num_out;
} AFBProtocol;

#define	AFB_PROTOCOL	{				\
	{ NOOPERATION, 0, 0, 0, 0, 0, 0 },		\
	{ WRITETOTAPE, VARCOMMBUFSIZ, 0, 0, 0, 0, 0 },	\
	{ SETCARTRIDGE, 3, 0, 0, 0, 0, 0 },		\
	{ SETFILE, 4, 0, 0, 0, 0, 0 },			\
	{ OSETCARTRIDGE, 1, 0, 0, 0, 0, 0 },		\
	{ OSETFILE, 3, 0, 0, 0, 0, 0 },			\
        { SKIPFILES, 4, 0, 0, 0, 0, 0 },		\
        { SETCARTSET, 3, 0, 0, 0, 0, 0 },		\
        { GOODBYE, 0, 0, 0, 0, 0, 0 },			\
        { ERASETAPE, 0, 0, 0, 0, 0, 0 },		\
        { READFROMTAPE, 0, VARCOMMBUFSIZ, 0, 0, 0, 0 },	\
        { OPENFORREAD, 0, 0, 0, 0, 0, 0 },		\
        { CLOSETAPE, 0, 0, 0, 0, 0, 0 },		\
        { OPENFORWRITE, 0, 0, 0, 0, 0, 0 },		\
        { CLOSETAPEN, 0, 0, 0, 0, 0, 0 },		\
        { REQUESTNEWFILE, 0, 0, 0, 0, 0, 0 },		\
        { REQUESTNEWCART, 0, 0, 0, 0, 0, 0 },		\
        { SETRAWFILE, 4, 0, 0, 0, 0, 0 },		\
        { SETRAWCARTRIDGE, 3, 0, 0, 0, 0, 0 },		\
        { OPENFORRAWREAD, 0, 0, 0, 0, 0, 0 },		\
        { OPENFORRAWWRITE, 0, 0, 0, 0, 0, 0 },		\
        { QUERYPOSITION, 0, 7, 0, 0, 0, 0 },		\
        { QUERYWRPOSITION, 0, 7, 0, 0, 0, 0 },		\
        { QUERYRDPOSITION, 0, 7, 0, 0, 0, 0 },		\
        { OQUERYPOSITION, 0, 4, 0, 0, 0, 0 },		\
        { OQUERYWRPOSITION, 0, 4, 0, 0, 0, 0 },		\
        { QUERYNUMCARTS, 0, 3, 0, 0, 0, 0 },		\
        { QUERYCARTSET, 0, 3, 0, 0, 0, 0 },		\
        { QUERYRDYFORSERV, 0, 512, 0, 0, 0, 0 },	\
        { QUERYWRITTENTAPES, 0, 4, 0, 0, 1, 4 },	\
        { QUERYTAPEBLOCKSIZE, 0, 4, 0, 0, 0, 0 },	\
        { QUERYNEEDEDTAPES, 1, 4, 1, 1, 1, 4 },		\
        { CLIENTBACKUP, 2, 5, 1, 1, 2, 4 },		\
        { OCLIENTBACKUP, 1, 0, 1, 1, 0, 0 },		\
        { SETBUFFEREDOP, 0, 0, 0, 0, 0, 0 },		\
        { SETSERIALOP, 0, 0, 0, 0, 0, 0 },		\
        { SETCHCARTONEOT, 0, 0, 0, 0, 0, 0 },		\
        { SETERRORONEOT, 0, 0, 0, 0, 0, 0 },		\
        { GETNUMREADVALID, 0, 4, 0, 0, 0, 0 },		\
        { SETNUMWRITEVALID, 4, 0, 0, 0, 0, 0 },		\
	{ CLIENTIDENT, 128, 0, 0, 0, 0, 0 },		\
	{ SERVERIDENT, 0, 256, 0, 0, 0, 0 },		\
	{ USERIDENT, 256, 0, 0, 0, 0, 0 },		\
	{ SETCOMMBUFSIZ, 4, 0, 0, 0, 0, 0 },		\
	{ MESSAGETEXT, 4, 0, 1, 4, 0, 0 },		\
/* special case: the entry here is a dummy, handled explicitly */ \
	{ AUTHENTICATE, 4, 4, 0, 0, 0, 0 },		\
	{ GETCURMSG, 0, 4, 0, 0, 1, 4 },		\
	};

#define	MAX_PROT_CHUNKSIZE	(MAXCOMMBUFSIZ + 4)

extern	AFBProtocol	**init_prot_spec();
