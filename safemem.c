/****************** Start of $RCSfile: safemem.c,v $  ****************
*
* $Source: /home/alb/afbackup/afbackup-3.5.8.12/RCS/safemem.c,v $
* $Id: safemem.c,v 1.5 2011/12/14 20:27:26 alb Exp alb $
* $Date: 2011/12/14 20:27:26 $
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

  static char * fileversion = "$RCSfile: safemem.c,v $ $Source: /home/alb/afbackup/afbackup-3.5.8.12/RCS/safemem.c,v $ $Id: safemem.c,v 1.5 2011/12/14 20:27:26 alb Exp alb $ " PACKAGE " " VERSION_STRING;

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <stdarg.h>
#include <errno.h>
#ifdef HAVE_MALLOC_H
#include <malloc.h>
#endif

#include <genutils.h>

void *
get_mem(
  void		*old_mem,
  Int32		*old_mem_malloced_ptr,
  Int32		needed_mem,
  void		*static_mem,
  Int32		static_size,
  Flag		*newmem_malloced)
{
  Int32	old_mem_malloced = 0;
  void	*new_mem = NULL;

  if(static_mem && (!old_mem || old_mem == static_mem)){
    if(needed_mem <= static_size){
	return(static_mem);
    }
  }

  if(old_mem_malloced_ptr && old_mem && old_mem != static_mem)
    old_mem_malloced = *old_mem_malloced_ptr;

  if(old_mem_malloced > 0){
    if(needed_mem > old_mem_malloced){
      if( (new_mem = RENEWP(old_mem, UChar, needed_mem)) )
	if(old_mem_malloced_ptr)
	  *old_mem_malloced_ptr = needed_mem;
    }
    else{
      new_mem = old_mem;
    }
  }
  else{
    if( (new_mem = NEWP(UChar, needed_mem)) )
      if(old_mem_malloced_ptr)
	*old_mem_malloced_ptr = needed_mem;
  }

  if(static_mem && newmem_malloced)
    *newmem_malloced = YES;

  return(new_mem);
}

void *
get_mem_cpy(
  void		*old_mem,
  Int32		*old_mem_malloced_ptr,
  Int32		needed_mem,
  void		*static_mem,
  Int32		static_size,
  Flag		*newmem_malloced)
{
  void	*new_mem;

  new_mem = get_mem(old_mem, old_mem_malloced_ptr, needed_mem,
		static_mem, static_size, newmem_malloced);

  if(new_mem && new_mem != old_mem && old_mem == static_mem)
    memcpy(new_mem, old_mem, static_size);

  return(new_mem);
}

#define	GRANULARITY	200

void * seg_malloc(Int32 size)
{
  Int32	new_size;

  if(size <= 0)
    return(NULL);

  new_size = (((size - 1) / GRANULARITY + 1) * GRANULARITY);

  return(malloc_forced(new_size));
}

void * seg_realloc(
  void		*old_ptr,
  Int32		size,
  Int32		old_size)
{
  Int32	new_p, old_p;

  if(old_size > 0)
    old_p = (((old_size - 1) / GRANULARITY + 1) * GRANULARITY);
  else
    old_p = 0;

  if(size > 0)
    new_p = (((size - 1) / GRANULARITY + 1) * GRANULARITY);
  else
    new_p = 0;

  if(! new_p)
    return(old_ptr);

  if(new_p == old_p)
    return(old_ptr);
  else if(! old_p && ! old_ptr)
    return(malloc_forced(new_p));
  else
    return(realloc_forced(old_ptr, new_p));
}

Int32 __internal_sm_import(
  void		***memlist,
  Int8		**tmpflags,
  Int32		*num,
  Int8		tmp,
  void		*ptr)
{
  Int32		n;
  void		**newmem;
  Int8		*newflags;

  n = *num;

  if(*num){
    newmem = SRENEWP(*memlist, void *, n + 1, n);
    newflags = SRENEWP(*tmpflags, Int8, n + 1, n);
  }
  else{
    newmem = SNEWP(void *, 1);
    newflags = SNEWP(Int8, 1);
  }

  if(! newmem || !newflags){
    if(newmem)
      free(newmem);
    if(newflags)
      free(newflags);

    return(-1);
  }

  *memlist = newmem;
  *tmpflags = newflags;

  newmem[n] = ptr;
  newflags[n] = (tmp ? 1 : 0);

  (*num)++;

  return(0);
}

void * __internal_sm_malloc(
  void		***memlist,
  Int8		**tmpflags,
  Int32		*num,
  Int8		tmp,
  size_t	size)
{
  void	*newmem;

  newmem = (void *) malloc_forced(size);
  if(!newmem)
    return(NULL);

  if(__internal_sm_import(memlist, tmpflags, num, tmp, newmem)){
    free(newmem);
    return(NULL);
  }

  return(newmem);
}

void
__internal_sm_freeall(
  void		**memlist,
  Int8		*tmpflags,
  Int32		num,
  Int8		tmp)
{
  Int8		*fptr;
  void		**mptr;

  num--;
  for(fptr = tmpflags + num, mptr = memlist + num; num >= 0;
			num--, fptr--, mptr--){
    if((!tmp || *fptr) && *mptr){
	free(*mptr);
    }
  }

  if(memlist)
    free(memlist);
  if(tmpflags)
    free(tmpflags);
}

void
__internal_sm_free(
  void		**memlist,
  Int8		*tmpflags,
  Int32		*num,
  void		*ptr)
{
  void		**mptr;
  Int32		n, i;

  n = *num;

  for(i = 0, mptr = memlist; i < n; i++, mptr++){
    if((void *) ptr == (void *) (*mptr)){
	free(ptr);
	n--;
	memmove(mptr, mptr + 1, (n - i) * sizeof(void *));
	memmove(tmpflags + i, tmpflags + i + 1,
				(n - i) * sizeof(Int8));
	(*num)--;
	return;
    }
  }
}

void *
__internal_sm_realloc(
  void		***memlist,
  Int8		**tmpflags,
  Int32		*num,
  void		*ptr,
  Int8		tmp,
  size_t	size)
{
  void		**mptr, *newptr;
  Int32		n, i;

  n = *num;

  for(i = 0, mptr = *memlist; i < n; i++, mptr++){
    if((void *) ptr == (void *) (*mptr)){
	newptr = (void *) realloc_forced(ptr, size);
	if(!newptr)
	  return(NULL);

	(*memlist)[i] = newptr;
	(*tmpflags)[i] = (tmp ? 1 : 0);
	return(newptr);
    }
  }

  /* pointer not found. Perform malloc */
  newptr = __internal_sm_malloc(memlist, tmpflags, num, tmp, size);
  if(!newptr)
    return(NULL);

  if(ptr){
    memcpy(newptr, ptr, size);
    free(ptr);
  }

  return(newptr);
}

UChar * __internal_sm_strdup(
  void		***memlist,
  Int8		**tmpflags,
  Int32		*num,
  Int8		tmp,
  UChar		*str)
{
  UChar		*newstr;

  newstr = __internal_sm_malloc(memlist, tmpflags, num, tmp,
			strlen(str) + 1);
  if(!newstr)
    return(NULL);

  strcpy(newstr, str);

  return(newstr);
}

UChar * __internal_sm_strapp(
  void		***memlist,
  Int8		**tmpflags,
  Int32		*num,
  Int8		tmp,
  UChar		*str1,
  UChar		*str2)
{
  UChar		*newstr;

  newstr = __internal_sm_malloc(memlist, tmpflags, num, tmp,
			strlen(str1) + strlen(str2) + 1);
  if(!newstr)
    return(NULL);

  strcpy(newstr, str1);
  strcat(newstr, str2);

  return(newstr);
}

UChar * __internal_sm_strchain(
  void		***memlist,
  Int8		**tmpflags,
  Int32		*num,
  Int8		tmp,
  ...)
{
  UChar		*newstr, *next, **cpptr;
  va_list	args;

  newstr = __internal_sm_strdup(memlist, tmpflags, num, tmp, "");
  if(!newstr)
    return(NULL);

  va_start(args, tmp);

  cpptr = (UChar **) (*memlist + *num - 1);
  if(newstr != *cpptr)
    return(NULL);

  while( (next = va_arg(args, UChar *)) ){
    newstr = RENEWP(*cpptr, UChar, strlen(next) + strlen(*cpptr) + 1);
    if(!newstr){
	va_end(args);
	return(NULL);
    }
    *cpptr = newstr;
    strcat(newstr, next);
  }

  va_end(args);

  return(*cpptr);
}
