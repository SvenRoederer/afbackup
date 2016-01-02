/****************** Start of $RCSfile: prot.c,v $  ****************
*
* $Source: /home/alb/afbackup/afbackup-3.5.8.12/RCS/prot.c,v $
* $Id: prot.c,v 1.2 2004/07/08 20:34:45 alb Exp alb $
* $Date: 2004/07/08 20:34:45 $
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

  static char * fileversion = "$RCSfile: prot.c,v $ $Source: /home/alb/afbackup/afbackup-3.5.8.12/RCS/prot.c,v $ $Id: prot.c,v 1.2 2004/07/08 20:34:45 alb Exp alb $ " PACKAGE " " VERSION_STRING;

#include <ctype.h>
#include <stdio.h>
#include <string.h>
#include <prot.h>

static AFBProtocol	prot[] = AFB_PROTOCOL;

static AFBProtocol	*(prot_items[256]);

AFBProtocol **
init_prot_spec()
{
  Int32		i;
  AFBProtocol	*nop = NULL;

  memset(prot_items, 0, 256 * sizeof(prot_items[0]));
  for(i = 0; i < sizeof(prot) / sizeof(prot[0]); i++){
    prot_items[prot[i].cmd] = prot + i;
    if(prot[i].cmd == NOOPERATION)
	nop = prot + i;
  }

  for(i = 0; i < 256; i++)
    if(isspace(i))
	prot_items[i] = nop;

  return(prot_items);
}
