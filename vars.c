/****************** Start of $RCSfile: vars.c,v $  ****************
*
* $Source: /home/alb/afbackup/afbackup-3.5.8.12/RCS/vars.c,v $
* $Id: vars.c,v 1.6 2012/11/01 09:53:10 alb Exp alb $
* $Date: 2012/11/01 09:53:10 $
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

  static char * fileversion = "$RCSfile: vars.c,v $ $Source: /home/alb/afbackup/afbackup-3.5.8.12/RCS/vars.c,v $ $Id: vars.c,v 1.6 2012/11/01 09:53:10 alb Exp alb $ " PACKAGE " " VERSION_STRING;

#include <string.h>
#include <stdarg.h>
#include <x_types.h>
#include <x_errno.h>
#include <genutils.h>
#include <fileutil.h>


int
cmp_KeyValue_bykey(void * ptr1, void * ptr2)
{
  if(!ptr1 || !ptr2)
    return(0);

  return(strcmp(((KeyValue *) ptr1)->key, ((KeyValue *) ptr2)->key));
}

int
cmp_KeyValue_bykeyn(void * ptr1, void * ptr2)
{
  int		k1, k2;

  if(!ptr1 || !ptr2)
    return(0);

  k1 = k2 = 0;
  sscanf(((KeyValue *) ptr1)->key, "%d", &k1);
  sscanf(((KeyValue *) ptr2)->key, "%d", &k2);
  return(k1 > k2 ? 1 : (k1 < k2 ? -1 : 0));
}

Int32
set_var(KeyValue ** vars, UChar * name, UChar * value)
{
  KeyValue	*vptr, *newvars, cmpvar;
  UChar		*newval;
  Int32		vl = 0, nl = 0, n, voffs;
  Flag		need_alloc = NO, eq;	/* uninitialized ok */

  if(!vars || !name){
    errno = EINVAL;
    return(-1);
  }

  cmpvar.key = name;

  if(*vars){
    newvars = *vars;
    for(vptr = newvars, n = 0; vptr->key; vptr++, n++);

    vptr = b_locate(&cmpvar, newvars, n, sizeof(KeyValue), cmp_KeyValue_bykey);
    voffs = vptr - newvars;

    if(!vptr->key)
	need_alloc = YES;
    else if(!(eq = (!strcmp(vptr->key, name))) )
	need_alloc = YES;

    if(need_alloc){
	newvars = RENEWP(newvars, KeyValue, n + 2);
	if(!newvars)
	  return(-5);
	*vars = newvars;
	vptr = newvars + voffs;
    }

    if(vptr->key){
      if(eq){
	if(value)
	  nl = strlen(value);
	if(vptr->value)
	  vl = strlen(vptr->value);
	if(nl > vl){
	  newval = ZRENEWP(vptr->value, UChar, nl + 1);
	  if(!newval)
	    return(-2);
	  vptr->value = newval;
	}
      }
      else{
	memmove(vptr + 1, vptr, (n + 1 - voffs) * sizeof(KeyValue));
	SETZERO(*vptr);
      }
    }
    else{
	vptr = newvars + n;
	memset(vptr, 0, 2 * sizeof(KeyValue));
    }
  }
  else{
    *vars = vptr = NEWP(KeyValue, 2);
    if(!vptr)
	return(-4);
    memset(vptr, 0, 2 * sizeof(KeyValue));
  }

  if(!vptr->key){
    if(!(vptr->key = strdup(name)))
	return(-6);
  }
  if(!vptr->value){
    if(value)
      if(!(vptr->value = strdup(value)))
	return(-7);
  }
  else{
    if(value)
	strcpy(vptr->value, value);
    else
	ZFREE(vptr->value);
  }

  return(0);
}

Int32
unset_var(KeyValue * vars, UChar * name)
{
  KeyValue	*vptr, cmpvar;
  Int32		n, pos;

  if(!name)		/* nothing to unset is ok */
    return(0);

  if(!vars){
    errno = EINVAL;
    return(-1);
  }

  cmpvar.key = name;
  for(n = 0, vptr = vars; vptr->key; vptr++, n++);

  vptr = b_search(&cmpvar, vars, n, sizeof(KeyValue), cmp_KeyValue_bykey);

  if(vptr){
    pos = vptr - vars;
    free(vptr->key);
    free(vptr->value);
    memmove(vars + pos, vars + pos + 1, (n - pos) * sizeof(KeyValue));
  }

  return(0);
}

UChar *
get_var(KeyValue * vars, UChar * name, Flag all)
{
  KeyValue	*vptr, cmpvar;
  UChar		*rptr;
  Int32		n;

  if(!vars || !name)
    return(NULL);

  cmpvar.key = name;
  for(n = 0, vptr = vars; vptr->key; vptr++, n++);

  vptr = b_search(&cmpvar, vars, n, sizeof(KeyValue), cmp_KeyValue_bykey);
  if(vptr){
    rptr = vptr->value;
    if(all)
	rptr = strdup(rptr);
    return(rptr);
  }

  return(NULL);
}

UChar *
repl_vars(UChar * str, KeyValue * vars)
{
  UChar		*cptr, *cptr2, *newstr = NULL, *tmpstr;
  UChar		*varname = NULL;
  Int32		varoffset, vallen, newstrlen;
  Int32		varlen;		/* uninitialized ok */

  if(!str){
    errno = EINVAL;
    return(NULL);
  }
  if(!vars)
    return(strdup(str));

  if(!(newstr = strdup(str)))
    return(NULL);
  if(!(varname = strdup(str)))
    GETOUT;

  cptr = newstr;
  while( (cptr = strchr(cptr, '$')) ){
	if(escaped(newstr, cptr, '\\')){
	  memmove(cptr - 1, cptr, sizeof(UChar) * (strlen(cptr) + 1));
	  continue;
	}

	varoffset = -1;

	if(*(cptr + 1) == '{'){
	  cptr2 = strchr(cptr + 2, '}');
	  if(!cptr2)
	    GETOUT;

	  strcpy(varname, cptr + 2);
	  varname[cptr2 - cptr - 2] = '\0';

	  varoffset = cptr - newstr;
	  varlen = cptr2 - cptr + 1;
	}
	else{
	  for(cptr2 = cptr + 1;
		*cptr2 && (isalnum(*cptr2) || *cptr2 == '_'); cptr2++);
	  if(cptr2 > cptr + 1){
	    strcpy(varname, cptr + 1);
	    varname[cptr2 - cptr - 1] = '\0';

	    varoffset = cptr - newstr;
	    varlen = cptr2 - cptr;
	  }
	}

	if(varoffset >= 0){
	  cptr2 = get_var(vars, varname, NO);
	  if(!cptr2)
	    cptr2 = "";
	  vallen = strlen(cptr2);

	  newstrlen = strlen(newstr) - varlen + vallen + 1;

	  if(varlen > vallen)
	    memmove(newstr + varoffset + vallen, newstr + varoffset + varlen,
		sizeof(UChar) * strlen(newstr + varoffset + varlen) + 1);

	  tmpstr = RENEWP(newstr, UChar, newstrlen);
	  if(!tmpstr)
	    GETOUT;
	  newstr = tmpstr;

	  if(varlen < vallen)
	    memmove(newstr + varoffset + vallen, newstr + varoffset + varlen,
		sizeof(UChar) * strlen(newstr + varoffset + varlen) + 1);

	  if(vallen > 0)
	    memcpy(newstr + varoffset, cptr2, sizeof(UChar) * vallen);

	  cptr = newstr + varoffset + vallen;
	}
	else
	  cptr++;
  }
	  
 cleanup:
  ZFREE(varname);

  return(newstr);

 getout:
  ZFREE(newstr)
  CLEANUP;
}

Int32
num_vars(KeyValue * vars)
{
  KeyValue	*vptr;
  Int32		n;

  if(!vars)
    return(0);

  for(vptr = vars, n = 0; vptr->key; vptr++, n++);

  return(n);
}

Int32
set_named_data_r(
  KeyValue	**vars,
  UChar		*name,
  void		*value,
  size_t	size,
  void		**ret)
{
  KeyValue	*vptr, *newvars, cmpvar;
  UChar		*newval;
  Int32		n, voffs;
  size_t	vl = 0;
  Flag		need_alloc = NO, eq;	/* uninitialized ok */

  if(!vars || !name){
    errno = EINVAL;
    return(-1);
  }

  cmpvar.key = name;

  if(*vars){
    newvars = *vars;
    n = num_named_data(*vars);

    vptr = b_locate(&cmpvar, newvars, n, sizeof(KeyValue), cmp_KeyValue_bykey);
    voffs = vptr - newvars;

    if(!vptr->key)
	need_alloc = YES;
    else if(!(eq = (!strcmp(vptr->key, name))) )
	need_alloc = YES;

    if(need_alloc){
	newvars = RENEWP(newvars, KeyValue, n + 2);
	if(!newvars)
	  return(-5);
	*vars = newvars;
	vptr = newvars + voffs;
    }

    if(vptr->key){
      if(eq){
	if(vptr->value)
	  vl = *((size_t *) vptr->value);
	if(size > vl){
	  newval = ZRENEWP(vptr->value, UChar,
			align_n(sizeof(size_t) + size, sizeof(size_t)));
	  if(!newval)
	    return(-2);
	  vptr->value = newval;
	}
      }
      else{
	memmove(vptr + 1, vptr, (n + 1 - voffs) * sizeof(KeyValue));
	SETZERO(*vptr);
      }
    }
    else{
	vptr = newvars + n;
	memset(vptr, 0, 2 * sizeof(KeyValue));
    }
  }
  else{
    *vars = vptr = NEWP(KeyValue, 2);
    if(!vptr)
	return(-4);
    memset(vptr, 0, 2 * sizeof(KeyValue));
  }

  if(!vptr->key){
    if(!(vptr->key = strdup(name)))
	return(-6);
  }
  if(!vptr->value){
    if(value)
      if(!(vptr->value = NEWP(UChar,
			align_n(sizeof(size_t) + size, sizeof(size_t)))))
	return(-7);
  }
  if(value){
    *((size_t *) vptr->value) = size;
    newval = (UChar *) vptr->value + sizeof(size_t);
    memcpy(newval, value, size);
  }
  else{
    ZFREE(vptr->value);
    newval = NULL;
  }
  if(ret)
    *ret = newval;

  return(0);
}

void *
get_named_data(KeyValue * data, UChar * name, Flag all, size_t * size)
{
  void		*rptr, *nptr;
  KeyValue	*vptr, cmpvar;
  size_t	sz;

  if(!data || !name)
    return(NULL);

  cmpvar.key = name;
  vptr = b_search(&cmpvar, data, num_named_data(data),
				sizeof(KeyValue), cmp_KeyValue_bykey);
  if(!vptr)
    return(NULL);
  rptr = vptr->value;

  sz = *((size_t *) rptr);
  rptr = (void *)((UChar *) rptr + sizeof(size_t));

  if(all){
    if(!(nptr = NEWP(UChar, sz)))
	return(NULL);
    memcpy(nptr, rptr, sz);
  }

  if(size)
    *size = sz;

  return(all ? nptr : rptr);
}

