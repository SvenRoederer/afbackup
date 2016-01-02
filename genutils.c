/****************** Start of $RCSfile: genutils.c,v $  ****************
*
* $Source: /home/alb/afbackup/afbackup-3.5.8.12/RCS/genutils.c,v $
* $Id: genutils.c,v 1.11 2013/11/08 17:58:05 alb Exp alb $
* $Date: 2013/11/08 17:58:05 $
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

  static char * fileversion = "$RCSfile: genutils.c,v $ $Source: /home/alb/afbackup/afbackup-3.5.8.12/RCS/genutils.c,v $ $Id: genutils.c,v 1.11 2013/11/08 17:58:05 alb Exp alb $ " PACKAGE " " VERSION_STRING;

#include <stdio.h>
#include <string.h>
#include <errno.h>
#ifdef HAVE_MALLOC_H
#include <malloc.h>
#endif
#ifdef	HAVE_SYS_WAIT_H
#include <sys/wait.h>
#endif
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <unistd.h>
#include <sys/stat.h>
#include <math.h>
#include <mvals.h>
#include <fcntl.h>
#include <signal.h>
#ifdef HAVE_TERMIOS_H
# include <termios.h>
#else
# include <termio.h>
#endif
#include <genutils.h>
#include <x_data.h>
#include <fileutil.h>
#include <sys/types.h>
#include <time.h>
#include <poll.h>

static	char	*compiler_error = "Compiler problems. Call for maintainance";

static void
util_fatal(UChar * s)
{
    fprintf(stderr, "\n%s failed !\n", s);
    exit(-1);
}

Int32
FSEEK_SAFE(
    FILE	*fp,
    Int32	start,
    Int32	flag)
{ 
    Int32	val;		/* exit on failed fseek */

    if((val = fseek(fp, start, flag)) != 0)
	util_fatal("fseek");

    return(val);
}

void *
MALLOC_SAFE(Int32 nr)
{
    void	*ret;		/* exit on failed malloc */

    if((ret = (void *) malloc_forced(nr)) == NULL)
	util_fatal("malloc");

    return(ret);
}

void *
REALLOC_SAFE(void * old_ptr, Int32 nr)
{
    void	*ret;		/* exit on failed realloc */

    if((ret = (void *) realloc_forced((void *) old_ptr, nr)) == NULL)
	util_fatal("realloc");

    return(ret);
}

UChar *
STRDUP_SAFE(UChar * str)
{
    UChar	*ret;

    if((ret = strdup(str)) == NULL)
	util_fatal("strdup");

    return(ret);
}

Int32
empty_string(UChar * str)
{
  Int32	i, o;

  if(!str)
    return(1);

  o = strlen(str);

  for(i = 0; i < o; i++)
    if(! isspace(str[i]))
      return(0);

  return(1);
}

void
nomemmsgexit(void * ptr, FILE * fp)
{
  if(!fp)
    fp = stderr;

  if(!ptr){
    if(fp)
	fprintf(fp, T_("Error: Cannot allocate memory.\n"));
    exit(1);
  }
}

static	UChar	*yesstrings[] = {
			"1",
			"yes",
			"true",
			"ok",
			TN_("Yes"),
			TN_("true"),
			TN_("ok"),
			NULL,
		};

Flag
is_yes(UChar * string, Int32 len)
{
  UChar	**cpptr;
  int	r;

  if(!string)
    return(NO);

  for(cpptr = yesstrings; *cpptr; cpptr++){
    if(len > 0)
	r = strncasecmp(T_(*cpptr), string, len);
    else
	r = strcasecmp(T_(*cpptr), string);

    if(!r)
	return(YES);
  }

  return(NO);
}

/*
 * Returns a pointer to the first non-whitespace character in string.
 * Returns NULL and sets errno on error.
 */

UChar *
first_nospace(UChar * string)
{
  if(!string){
    errno = EINVAL;
    return(NULL);
  }

  while(isspace(*string) && *string)
    string++;

  return(string);
}

UChar *
first_space(UChar * string)
{
  if(!string){
    errno = EINVAL;
    return(NULL);
  }

  while(!isspace(*string) && *string)
    string++;

  return(string);
}

Int32				/* look, if file "name" exists, */
existfile(UChar * name)		/* make sure, it's not a directory */
				/* return 1, if ok, else 0 */
{		
    struct stat	stp;

    if(!name){
	errno = EINVAL;
	return(0);
    }

    if(stat(name, &stp) != 0){
	if(errno == NO_SUCH_FILE)
	    errno = 0;
	return(0);
    }

    if(S_ISDIR(stp.st_mode))
	return(0);

    return(1);
}

FILE *
FOPEN_SAFE(
    UChar	*name,
    UChar	*mode)
{
  FILE	*fp;

  if(!name)
	return(stdin);
  if(!mode)
	mode = "r";

  fp = fopen(name, mode);

  if(fp == NULL)
	util_fatal("fopen");

  return(fp);
}

#define	MEMSWAPMAXSIZE	512

Int32
memswap(		/* exchange values of miscallaneous */
  void	*var1,		/* types of variables */
  void	*var2,
  Int32	size)
{
  UChar	*ptr1, *ptr2, *tswapspace;
  UChar	memswapspace[MEMSWAPMAXSIZE];
  Int32	i;

  if(size < 0 || !var1 || !var2){
    errno = EINVAL;
    return(-1);
  }

  ptr1 = (UChar *) var1;
  ptr2 = (UChar *) var2;

  tswapspace = NEWP(UChar, size);

  if(tswapspace){
    memcpy(tswapspace, ptr1, size);
    memcpy(ptr1, ptr2, size);
    memcpy(ptr2, tswapspace, size);

    free(tswapspace);
  }
  else{
    while(size > 0){
	i = size > MEMSWAPMAXSIZE ? MEMSWAPMAXSIZE : size;

	memcpy(memswapspace, ptr1, i);
	memcpy(ptr1, ptr2, i);
	memcpy(ptr2, memswapspace, i);
	ptr1 += i;
	ptr2 += i;
	size -= i;
    }
  }

  return(0);
}

UChar *
memfind(UChar * mem1, Int32 len1, UChar * mem2, Int32 len2)
{
  UChar		*cptr1, *eptr, *cptr2, *cptr3;
  Int32	i;

  if(!mem1 || !mem2){
    errno = EINVAL;
    return(NULL);
  }

  if(len2 > len1 || len2 == 0)
    return(NULL);

  eptr = mem1 + len1 - len2;
  len2--;
  for(cptr1 = mem1; cptr1 <= eptr; cptr1++){
    if(*cptr1 == *mem2){
      cptr2 = cptr1;
      cptr2++;
      cptr3 = mem2;
      cptr3++;
      for(i = len2; i; i--, cptr2++, cptr3++){
	if(*cptr2 != *cptr3)
	  break;
      }

      if(! i)
	return(cptr1);
    }
  }

  return(NULL);
}

#define	MEMMOVEBUFSIZE	512

void *
mem_move(void * vdest, void * vsrc, size_t n)
{
  UChar		buf[MEMMOVEBUFSIZE], *src, *dest;
  Int32		i;

  if(n < 0 || !vsrc || !vdest){
    errno = EINVAL;
    return(NULL);
  }

  if(vdest == vsrc)
    return(vdest);

  src = (UChar *) vsrc;
  dest = (UChar *) vdest;

  if(dest > src + n || src > dest + n){
    memcpy(dest, src, n);
    return(vdest);
  }

  if(dest > src){
    dest += n;
    src += n;

    while(n > 0){
	i = n > MEMMOVEBUFSIZE ? MEMMOVEBUFSIZE : n;

	src -= i;
	dest -= i;
	n -= i;

	memcpy(buf, src, i);
	memcpy(dest, buf, i);
    }
  }
  else{
    while(n > 0){
	i = n > MEMMOVEBUFSIZE ? MEMMOVEBUFSIZE : n;

	memcpy(buf, src, i);
	memcpy(dest, buf, i);
	n -= i;

	src += i;
	dest += i;
    }
  }

  return(vdest);
}

void *
mem_move2(void * vdest, void * vsrc, size_t n)
{
  UChar		buf[(MEMMOVEBUFSIZE << 3)], *src, *dest;
  Int32		i;

  if(n < 0 || !vsrc || !vdest){
    errno = EINVAL;
    return(NULL);
  }

  if(vdest == vsrc)
    return(vdest);

  src = (UChar *) vsrc;
  dest = (UChar *) vdest;

  if(dest > src + n || src > dest + n){
    memcpy(dest, src, n);
    return(vdest);
  }

  if(dest > src){
    dest += n;
    src += n;

    while(n > 0){
	i = n > (MEMMOVEBUFSIZE << 3) ? (MEMMOVEBUFSIZE << 3) : n;

	src -= i;
	dest -= i;
	n -= i;

	memcpy(buf, src, i);
	memcpy(dest, buf, i);
    }
  }
  else{
    while(n > 0){
	i = n > (MEMMOVEBUFSIZE << 3) ? (MEMMOVEBUFSIZE << 3) : n;

	memcpy(buf, src, i);
	memcpy(dest, buf, i);
	n -= i;

	src += i;
	dest += i;
    }
  }

  return(vdest);
}

void *
mem_move3(void * vdest, void * vsrc, size_t n)
{
  UChar		buf[(MEMMOVEBUFSIZE << 6)], *src, *dest;
  Int32		i;

  if(n < 0 || !vsrc || !vdest){
    errno = EINVAL;
    return(NULL);
  }

  if(vdest == vsrc)
    return(vdest);

  src = (UChar *) vsrc;
  dest = (UChar *) vdest;

  if(dest > src + n || src > dest + n){
    memcpy(dest, src, n);
    return(vdest);
  }

  if(dest > src){
    dest += n;
    src += n;

    while(n > 0){
	i = n > (MEMMOVEBUFSIZE << 6) ? (MEMMOVEBUFSIZE << 6) : n;

	src -= i;
	dest -= i;
	n -= i;

	memcpy(buf, src, i);
	memcpy(dest, buf, i);
    }
  }
  else{
    while(n > 0){
	i = n > (MEMMOVEBUFSIZE << 6) ? (MEMMOVEBUFSIZE << 6) : n;

	memcpy(buf, src, i);
	memcpy(dest, buf, i);
	n -= i;

	src += i;
	dest += i;
    }
  }

  return(vdest);
}

void *
mem_move4(void * vdest, void * vsrc, size_t n)
{
  UChar		buf[(MEMMOVEBUFSIZE << 9)], *src, *dest;
  Int32		i;

  if(n < 0 || !vsrc || !vdest){
    errno = EINVAL;
    return(NULL);
  }

  if(vdest == vsrc)
    return(vdest);

  src = (UChar *) vsrc;
  dest = (UChar *) vdest;

  if(dest > src + n || src > dest + n){
    memcpy(dest, src, n);
    return(vdest);
  }

  if(dest > src){
    dest += n;
    src += n;

    while(n > 0){
	i = n > (MEMMOVEBUFSIZE << 9) ? (MEMMOVEBUFSIZE << 9) : n;

	src -= i;
	dest -= i;
	n -= i;

	memcpy(buf, src, i);
	memcpy(dest, buf, i);
    }
  }
  else{
    while(n > 0){
	i = n > (MEMMOVEBUFSIZE << 9) ? (MEMMOVEBUFSIZE << 9) : n;

	memcpy(buf, src, i);
	memcpy(dest, buf, i);
	n -= i;

	src += i;
	dest += i;
    }
  }

  return(vdest);
}

void
repl_esc_seq_x(UChar * string, UChar escchar, Uns8 flags)
{
  UChar	*cptr1, *cptr2, c;
  Int32	i, n;

  if(!string)
    return;

  for(cptr1 = cptr2 = string; *cptr1; cptr1++, cptr2++){
    if(*cptr1 == escchar){
      c = *(++cptr1);

      switch(c){
	case 'n':
	  *cptr2 = '\n';
	  break;
	case 't':
	  *cptr2 = '\t';
	  break;
	case 'a':
	  *cptr2 = '\a';
	  break;
	case 'b':
	  *cptr2 = '\b';
	  break;
	case 'r':
	  *cptr2 = '\r';
	  break;
	case 'f':
	  *cptr2 = '\f';
	  break;
	case 'v':
	  *cptr2 = '\v';
	  break;
	default:
	  if(c >= '0' && c <= '7'){
	    if((flags & REPLESC_NOOCTALS)){
		*(cptr2++) = escchar;
		*cptr2 = c;
	    }
	    else{
		n = 0;
		for(i = 0; i < 3; i++){
		  c = cptr1[i];
		  if(c < '0' || c > '7')
		    break;

	 	  n = (n << 3) | (c - '0');
	        }

		cptr1 += i - 1;

		*cptr2 = (UChar) n;
	    }
	  }
	  else{
	    *cptr2 = *cptr1;

	    if(! *cptr2)
	      cptr1--;
	  }
      }
    }
    else{
      *cptr2 = *cptr1;
    }
  }

  *cptr2 = '\0';
}

Int32
rm_backspace(UChar * str)
{
  Int32		l;
  UChar		*cptr;

  if(!str)
    return(0);

  l = strlen(str);
  while((cptr = strchr(str + 1, '\b'))){
    memmove(cptr - 1, cptr + 1,
		(l /* + 1 */ - (cptr /* + 1 */ - str)) * sizeof(UChar));
    l -= 2;
  }

  return(0);
}

void *
memdup(void * mem, size_t n)
{
  void		*newmem;

  newmem = NEWP(UChar, n * sizeof(UChar));
  if(newmem && mem)
    memcpy(newmem, mem, n * sizeof(UChar));
  return(newmem);
}

UChar *
mk_esc_seq(UChar * string, UChar escchar, UChar * ret_string)
{
  UChar	*new_string, *cptr, c, *strptr, r, obuf[5], *endp;
  Int32	len;

  if(!string){
    if(ret_string){
	strcpy(ret_string, "");
	return(ret_string);
    }
    return(strdup(""));
  }

  len = strlen(string);
  strptr = string + len;

  if(!ret_string){
    new_string = NEWP(UChar, (len << 2) + 1);
    if(!new_string)
	return(NULL);
  }
  else
    new_string = ret_string;

  endp = cptr = new_string + (len << 2);
  *cptr = '\0';
  for(; len > 0; len--){
    c = *(--strptr);
    if(c == '\"' || c == escchar){
	*(--cptr) = c;
	*(--cptr) = escchar;
    }
    else{
	if(isprint(c) || ((c & 0x80) && (c & 0x60))){
	  *(--cptr) = c;
	}
	else{
	  r = '\0';

	  switch(c){
	   case '\n':
	    r = 'n';
	    break;
	   case '\t':
	    r = 't';
	    break;
	   case '\a':
	    r = 'a';
	    break;
	   case '\b':
	    r = 'b';
	    break;
	   case '\r':
	    r = 'r';
	    break;
	   case '\f':
	    r = 'f';
	    break;
	   case '\v':
	    r = 'v';
	    break;
	  }

	  if(r){
	    *(--cptr) = r;
	    *(--cptr) = escchar;
	  }
	  else{
	    cptr -= 2;
	    sprintf(obuf, "%03o", (int) c);
	    memcpy(--cptr, obuf, 3 * sizeof(UChar));
	    *(--cptr) = escchar;
	  }
	}
    }
  }

  len = endp - cptr;
  memmove(new_string, cptr, ++len);
  if(!ret_string)
    new_string = RENEWP(new_string, UChar, len);

  return(new_string);
}

Int32
memrepl(
  UChar		*buf,
  Int32		buflen,
  UChar		*repl,
  Int32		repllen,
  UChar		*new,
  Int32		newlen)
{
  UChar		*cptr, *cptr2, *cptr3, *eptr, *sptr;

  if(!buf || !repl){
    errno = EINVAL;
    return(-1);
  }
  if(!new){
    new = "";
    newlen = 0;
  }

  sptr = buf;
  while( (cptr = memfind(sptr, buflen - (sptr - buf), repl, repllen)) ){
    cptr += repllen;
    if(newlen > repllen){
	cptr2 = buf + buflen - 1;
	cptr3 = cptr2 + newlen - repllen;
	while(cptr2 >= cptr)
	  *(cptr3--) = *(cptr2--);
    }
    if(newlen < repllen){
	cptr3 = cptr;
	cptr2 = cptr3 + newlen - repllen;
	eptr = buf + buflen;
	while(cptr3 < eptr)
	  *(cptr2++) = *(cptr3++);
    }

    memcpy(cptr - repllen, new, newlen * sizeof(UChar));
    buflen += newlen - repllen;
    sptr = cptr - repllen + newlen;
  }

  return(buflen);
}

Int32
fscanword(FILE * fp, UChar * string)
{
  Int32	i = 0;
  UChar	a;

  if(!fp)
	fp = stdin;

  do{
        if(fread(&a, sizeof(UChar), 1, fp) < 1){
          fclose(fp);
          return((Int32) EOF);
        }
  } while(isspace(a));

  if(string)
    string[0] = a;

  do{
        i++;
        if(fread(&a, sizeof(UChar), 1, fp) < 1){
          fclose(fp);
          return((Int32) EOF);
        }
	if(string)
	  string[i] = a;
  } while(!isspace(a));

  if(string)
    string[i] = '\0';

  return(NO_ERROR);
}

Int32
fscanwordq(FILE * fp, UChar * word)
{
  Flag	quoted = NO;
  Int32	numc;
  UChar	c;

  if(!fp)
	fp = stdin;

  numc = 0;
  c = '\0';
  do{
    if(fread(&c, sizeof(UChar), 1, fp) < 1){
	if(word)
	  *word = '\0';

	return(EOF);
    }

    numc++;
  }while(isspace(c));

  forever{
    if(!c){
	if(fread(&c, sizeof(UChar), 1, fp) < 1)
	  break;
	numc++;
    }

    if(isspace(c) && !quoted)
	break;

    if(c == '\\'){
	if(fread(&c, sizeof(UChar), 1, fp) < 1)
	  break;
	numc++;

	if(word){
	  if(c != '\"')
	    *(word++) = '\\';
	  *(word++) = c;
	}
    }
    else{
	if(c == '\"'){
	  quoted = ! quoted;
	  c = '\0';
	  continue;
	}

	if(word){
	  *(word++) = c;
	}
    }
    c = '\0';
  }

  if(word)
    *word = '\0';

  return(numc);
}

UChar *
sscanword(
   UChar         *str,
   UChar         *word)
{
  UChar		*wptr;

  if(!str){
    errno = EINVAL;
    return(NULL);
  }

  wptr = word;

  while(ISSPACE(*str)){
	if(*str == '\0'){
	    if(word)
		*wptr = '\0';
	    return(NULL);
	}
	str++;
  }

  while(!ISSPACE(*str)){
	if(word)
	  *wptr = *str;
	str++;
	wptr++;
  }

  if(word){
    if(wptr == str && *str)
 	str++;

    *wptr = '\0';
  }

  while(*str && isspace(*str))
    str++;

  return(str);
}

/*
 * Scans the first argument <str> to locate the next word (a word is defined
 * as a sequence of non-whitespace characters, or a mixture of whitespace
 * and nonwhitespace delimited by a pair of double quotes (")).
 *
 * Returns a pointer to the first non-whitespace character following the 
 * first word.
 *
 * If the second argument, <word>, is non-NULL, then the scanned word
 * is copied into <word>.  (<word> is then '\0'-terminated).
 */

UChar *
sscanwordq(UChar * str, UChar * word)
{
  Flag	quoted = NO;

  if(!str){
    errno = EINVAL;
    return(NULL);
  }

  str = first_nospace(str);
  if(! *str)
    return(NULL);

  for(; *str && (!isspace(*str) || quoted); str++){
    if(*str == '\\'){
	str++;
	if(word){
	  if(*str != '\"' && *str)
	    *(word++) = '\\';
	  *(word++) = *str;
	}
    }
    else{
	if(*str == '\"'){
	  quoted = ! quoted;
	  continue;
	}

	if(word){
	  *(word++) = *str;
	}
    }
  }

  if(word){
    if(word == str && *str)
      str++;

    *word = '\0';
  }

  return(first_nospace(str));	
}

Int32
fprintwordq(
  FILE	*file,
  UChar	*word)
{
  Int32	i;

  if(!file)
    file = stdout;

  if(! word)
    i = fprintf(file, "\"\"");
  else if(word_count(word) != 1)
    i = fprintf(file, "\"%s\"", word);
  else
    i = fprintf(file, "%s", word);

  return(i);
}

Int32
sprintwordq(
  UChar	*str,
  UChar	*word)
{
  Int32	i;

  if(!str){
    errno = EINVAL;
    return(-1);
  }

  strcpy(str, "");

  if(! word)
    strcpy(str, "\"\"");
  else if(word_count(word) != 1)
    sprintf(str, "\"%s\"", word);
  else
    strcpy(str, word);

  return(strlen(str));
}

Int32
ffindword(
    FILE	*fp,
    UChar	*string)
{
  UChar	d[200];

  if(!string)
    return(0);
  if(!strcmp(string, ""))
    return(0);

  if(!fp)
    fp = stdin;

  do{
	if(fscanword(fp, d) == EOF)
	    return((Int32) EOF);
  } while(strcmp(d, string) != 0);

  return(NO_ERROR);
}

#ifndef	HAVE_STRRSTR	/* exists in HP-UX (!) */
UChar *
strrstr(UChar * string, UChar * substr)
{
  Int32		len, sublen;
  UChar		*cptr;

  if(!string || !substr)
    return(NULL);

  len = strlen(string);
  sublen = strlen(substr);

  if(len < sublen || len == 0 || sublen == 0)
    return(NULL);

  for(cptr = string + len - sublen; cptr >= string; cptr--)
    if(*cptr == substr[0])
      if(!strncmp(cptr, substr, sublen))
	return(cptr);

  return(NULL);
}
#endif

#ifndef	HAVE_STRCASECMP
Int32
strcasecmp(UChar * str1, UChar * str2)
{
  UChar		c1, c2;
  Int32		res;

  if(!str1 && !str2)
    return(0);
  if(!str2)
    return(1);
  if(!str1)
    return(-1);

  forever{
    c1 = tolower(*str1++);
    c2 = tolower(*str2++);
    if (c1 == '\0' || c2 == '\0' || c1 != c2)
	return((Int32) c1 - (Int32) c2);
  }
}
#endif

#ifndef	HAVE_STRNCASECMP
Int32
strncasecmp(UChar * str1, UChar * str2, int maxcnt)
{
  UChar		c1, c2;
  Int32		res;

  if(!str1 && !str2)
    return(0);
  if(!str2)
    return(1);
  if(!str1)
    return(-1);

  while(0 < maxcnt--){
    c1 = tolower(*str1++);
    c2 = tolower(*str2++);
    if (c1 == '\0' || c2 == '\0' || c1 != c2)
	return((Int32) c1 - (Int32) c2);
  }
  return(0);
}
#endif

#ifndef HAVE_STRCASESTR
UChar *
strcasestr(UChar * str1, UChar * str2)
{
  UChar         *result = NULL, *lstr1, *lstr2, *cptr;

  if(!str1 || !str2){
    errno = EINVAL;
    return(NULL);
  }

  lstr1 = strdup(str1);
  lstr2 = strdup(str2);
  if(!lstr1 || !lstr2)
    GETOUT;

  for(cptr = lstr1; *cptr; cptr++)
    *cptr = tolower(*cptr);
  for(cptr = lstr2; *cptr; cptr++)
    *cptr = tolower(*cptr);

  cptr = strstr(lstr1, lstr2);
  if(cptr)
    result = str1 + (cptr - lstr1);

 getout:
  ZFREE(lstr1);
  ZFREE(lstr2);

  return(result);
}
#endif

UChar *
strword(UChar * str, Int32 n)
{
  Int32		numw, l;
  UChar		*cptr;

  if(!str){
    errno = EINVAL;
    return(NULL);
  }

  while(*str && isspace(*str))
    str++;

  numw = word_count(str);
  if(numw < 1)
    return(NULL);

  if(n < 0)
    n += numw;

  if(n < 0 || n >= numw){
    errno = ERANGE;
    return(NULL);
  }

  while(n > 0){
    while(*str && !isspace(*str))
	str++;
    while(*str && isspace(*str))
	str++;
    n--;
  }

  for(cptr = str; *cptr && !isspace(*cptr); cptr++);
  l = cptr - str;

  cptr = NEWP(UChar, l + 1);
  if(cptr){
    strncpy(cptr, str, l);
    cptr[l] = '\0';
  }

  return(cptr);
}

/*
 * Takes a string of (potentially quoted) words <str>, and returns a malloc'd
 * copy of the n'th word (zero-indexed) in the string.
 *
 * If <n> is positive, the count is done from the beginning of the string.
 * If <n> is negative, the count is done from the end of the string.
 *
 * Returns NULL on error or if there are no words in <str>. On error, 
 * errno is also set.
 */

UChar *
strwordq(UChar * str, Int32 n)
{
  Int32		numw, l;
  UChar		*cptr;

  if(!str){
    errno = EINVAL;
    return(NULL);
  }

  while(*str && isspace(*str))
    str++;

  numw = word_countq(str);
  if(numw < 1)
    return(NULL);

  if(n < 0)
    n += numw;

  if(n < 0 || n >= numw){
    errno = ERANGE;
    return(NULL);
  }

  while(n > 0){
    str = sscanwordq(str, NULL);
    n--;
  }

  /* 
   * At this point, str points the n'th word.  Determine the length of the
   * word, not including any trailing whitespace.  Then allocate a buffer
   * of that length, and copy the word into that buffer, finally returning
   * the buffer.
   */

  cptr = sscanwordq(str, NULL);
  l = cptr - str;
  while(isspace(str[l - 1]))
    l--;

  cptr = NEWP(UChar, l + 1);
  if(cptr)
    sscanwordq(str, cptr);

  return(cptr);
}

int
trailingint(UChar * str)
{
  int	mul = 1, r = 0;

  if(!str){
    errno = EINVAL;
    return(-1);
  }

  str += strlen(str) - 1;
  while(isspace(*str))
    str--;
  while(isdigit(*str)){
    r += mul * (*str - '0');
    mul *= 10;
    str--;
  }

  return(r);
}

Flag
chop(UChar * line)
{
  UChar	*cptr;

  if(!line)
    return(NO);

  cptr = line + strlen(line) - 1;
  if(cptr >= line && *cptr == '\n'){
    *cptr = '\0';
    return(YES);
  }

  return(NO);
}

UChar *
word_skip(UChar * cptr, Int32 num, Int32 walk)
{
  Int32		dir;

  if(!cptr){
    errno = EINVAL;
    return(NULL);
  }

  if(walk < 0)
   num = - num;

  if(num == 0){
    dir = (isspace(*cptr) ? 1 : -1);
  }
  else{
    dir = (num > 0 ? 1 : -1);
    num = (num > 0 ? num - 1 : - num);
  }

  if(walk > 0){
    walk = 1;
  }
  else{
    walk = -1;
    dir = - dir;
  }

  forever{
    while(*cptr && !isspace(*cptr))
	cptr += dir;
    while(*cptr && isspace(*cptr))  
	cptr += walk;
    if(num == 0)
	break;

    if(dir * walk < 0){
      cptr -= walk;
      while(*cptr && isspace(*cptr))
	cptr -= walk;
    }

    num--;
  }

  return(cptr);
}

Int32
ffindwordb(FILE *fp, UChar *string) 
{ 
  UChar	c; 
  Int32	word_len; 
  Int32	i; 

  if(!fp)
	fp = stdin;

  if(!string) 
        return(EINVAL); 
     
  word_len = strlen((char *)string);  
     
  do{ 
	/* find the first occurrence of the desired character */ 
	do{ 
	  if(fread(&c, sizeof(UChar), 1, fp) < 1) 
	    return ((Int32) EOF); 
	}while (c != string[0]); 
     
	/* Found. Is the rest equal ? */ 

	for(i = 1; i < word_len && string[i - 1] == c; i++){ 
	  if (fread(&c, sizeof(UChar), 1, fp) < 1) 
		return((Int32) EOF); 
	}
    } 
    while(i < word_len); 
     
    return(NO_ERROR); 
} 

Int32
ishn(UChar * str)
{
  UChar	c;

  if(!str){
    errno = EINVAL;
    return(0);
  }

  while( (c = *(str++)) ){
    if(!ishnchr(c))
	return(0);
  }

  return(1);
}

Int32
isfqhn(UChar * str)
{
  UChar	c;

  if(!str){
    errno = EINVAL;
    return(0);
  }

  while( (c = *(str++)) ){
    if(!isfqhnchr(c))
	return(0);
  }

  return(1);
}

Int32
minmax(
    Real64	*array,
    Int32	nr,
    Real64	*min,
    Real64	*max)
{
    Int32	i;
    Real64	mini, maxi;

    if(nr <= 0)
	return(-1);

    if(!array){
	errno = EINVAL;
	return(-1);
    }

    mini = maxi = *array;

    for(i = 1; i < nr; i++){
	if(*(array + i) > maxi)
	    maxi = *(array + i);
	if(*(array + i) < mini)
	    mini = *(array + i);
    }

    if(min)
	*min = mini;
    if(max)
	*max = maxi;

    return(NO_ERROR);
}

Int32
strint(UChar * cptr, Int32 * r)
{
  int	i1, n;

  n = -1;

  if(!cptr || !r){
    errno = EFAULT;
    return(-1);
  }

  if(sscanf(cptr, "%d%n", &i1, &n) < 1){
    errno = EINVAL;
    return(-1);
  }

  *r = (Int32) i1;

  return(n);
}

Int32
ffindchr(FILE * fp, UChar c)
{
    UChar	a;

    if(!fp)
	fp = stdin;

    do{
	if(fread(&a, sizeof(UChar), 1, fp) < 1)
	    return((Int32) EOF);
    } while (c != a);

    return(NO_ERROR);
}

/*
 * Returns the number of words in <line>.  This routine does not understand
 * quoting (words are strictly a sequence of non-whitespace characters).
 */

Int32
word_count(UChar * line)
{
    Int32        i = 0, nr = 0;

    if(!line){
	errno = EINVAL;
	return(-1);
    }

    for (;;) {
	while (ISSPACE(line[i])) {
	    if (! line[i])
		return (nr);
	    i++;
	}
	if (line[i] == '\0')
	    return (nr);

	nr++;
	while (!ISSPACE(line[i]))
	    i++;
    }
}

/*
 * Counts the number of (potentially quoted) words in the string <line>.
 * Returns the count, or -1 on error.
 */

Int32
word_countq(UChar * line)
{
  Int32	n;

  if(!line){
    errno = EINVAL;
    return(-1);
  }

  n = 0;
  while(line){
    if( (line = sscanwordq(line, NULL)) )
	n++;
  }

  return(n);
}

static void
__q_sort__(
  void		*p1,
  void		*p2,
  Uns32		size,
  UChar		*array,
  void		*buffer,
  int		(*cmp_func)(void *, void *),
  void		*cmparg
)
{
  UChar		*ptr1 = (UChar *) p1, *ptr2 = (UChar *) p2;
  Int32		forward = 1;
  int		res, (*cmp_func2)(void *, void *, void *);

  cmp_func2 = (int(*)(void *, void *, void *)) cmp_func;

  if(ptr1 + size < ptr2){
    memcpy(buffer, ptr2, size);

    do{
      if(forward){
	if(cmparg == (void *) 1)
	  res = (*cmp_func)(ptr1, buffer);
	else
	  res = (*cmp_func2)(ptr1, buffer, cmparg);
	if(res > 0){
	  memcpy(ptr2, ptr1, size);
	  forward = 0;
	}
      }
      else{
	if(cmparg == (void *) 1)
	  res = (*cmp_func)(ptr2, buffer);
	else
	  res = (*cmp_func2)(ptr2, buffer, cmparg);
	if(res < 0){
	  memcpy(ptr1, ptr2, size);
	  forward = 1;
	}
      }
      if(forward)
	ptr1 += size;
      else
	ptr2 -= size;

    }while(ptr1 < ptr2);

    memcpy(ptr1, buffer, size);

    if((UChar *) p1 < ptr1 - size)
      __q_sort__(p1, ptr1 - size, size, array, buffer, cmp_func, cmparg);
    if(ptr1 + size < (UChar *) p2)
      __q_sort__(ptr1 + size, p2, size, array, buffer, cmp_func, cmparg);
  }
  else if(ptr1 + size == ptr2){
    if(cmparg == (void *) 1)
	res = (*cmp_func)(p1, p2);
    else
	res = (*cmp_func2)(p1, p2, cmparg);
    if(res > 0){
	memcpy(buffer, p1, size);
	memcpy(p1, p2, size);
	memcpy(p2, buffer, size);
    }
  }
}

void q_sort2(
  void		*base,
  Uns32		nel,
  Uns32		width,
  int		(*cmp_func2)(void *, void *, void *),
  void		*cmparg)
{
  UChar		*buffer, *array;
  int		res, (*comp)(void *, void *);

  comp = (int(*)(void *, void *)) cmp_func2;

  if(!base || !comp || width < 1){
    errno = EINVAL;
    return;
  }

  if(nel < 2)
    return;

  array = (UChar *) base;
  buffer = NEWP(UChar, width);

  if(! buffer)
    return;

  nel--;

  if(nel == 1){
    if(cmparg == (void *) 1)
	res = (*comp)(array, array + width);
    else
	res = (*cmp_func2)(array, array + width, cmparg);

    if(res > 0){
	memcpy(buffer, array, width);
	memcpy(array, array + width, width);
	memcpy(array + width, buffer, width);
    }
  }
  else{
    __q_sort__(array, array + nel * width, width, array, buffer, comp, cmparg);
  }

  free(buffer);
}

void q_sort(
  void		*base,
  Uns32		nel,
  Uns32		width,
  int		(*comp)(void *, void *))
{
  q_sort2(base, nel, width, (int(*)(void *, void *, void *)) comp, (void *) 1);
}

#define	MODE_B_SEARCH	0
#define	MODE_B_LOCATE	1

static void *_b_search_func(
  void		*key,
  void		*base,
  Uns32		nel,
  Uns32		size,
  int		(*comp)(void *, void *),
  Uns32		mode)
{
  Int32		i, lowest, uppest, mid;
  UChar		*upptr, *midptr;

  if(! size || ! key || ! base || ! comp || nel < 0)
    return(NULL);

  if(nel == 0)
    return(mode == MODE_B_LOCATE ? base : NULL);

  i = comp(key, base);
  if(! i)
    return(base);
  if(i < 0)
    return(mode == MODE_B_LOCATE ? base : NULL);
  if(nel == 1)
    return(mode == MODE_B_LOCATE ? (UChar *) base + size : NULL);

  uppest = nel - 1;
  upptr = (UChar *) base + uppest * size;
  i = comp(key, upptr);
  if(! i)
    return(upptr);
  if(i > 0)
    return(mode == MODE_B_LOCATE ? upptr + size : NULL);

  lowest = 0;

  while(uppest > lowest + 1){
    mid = (uppest + lowest) >> 1;
    midptr = (UChar *) base + mid * size;

    i = comp(key, midptr);
    if(! i)
	return(midptr);
    if(i < 0)
	uppest = mid;
    else
	lowest = mid;
  }

  return(mode == MODE_B_LOCATE ? (UChar *) base + uppest * size : NULL);
}

void *b_search(
  void		*key,
  void		*base,
  Uns32		nel,
  Uns32		size,
  int		(*comp)(void *, void *))
{
  return(_b_search_func(key, base, nel, size, comp, MODE_B_SEARCH));
}

void *b_locate(
  void		*key,
  void		*base,
  Uns32		nel,
  Uns32		size,
  int		(*comp)(void *, void *))
{
  return(_b_search_func(key, base, nel, size, comp, MODE_B_LOCATE));
}

void *l_find(
  void		*key,
  void		*base,
  Uns32		*nelp,
  Uns32		width,
  int		(*comp)(void *, void *))
{
  UChar		*ptr, *eptr;

  eptr = ((UChar *) base) + (*nelp * width);

  for(ptr = (UChar *) base; ptr < eptr; ptr += width){
    if(comp(ptr, key) == 0)
	return(ptr);
  }

  return(NULL);
}

void *l_search(
  void		*key,
  void		*base,
  Uns32		*nelp,
  Uns32		width,
  int		(*comp)(void *, void *))
{
  UChar		*ptr, *eptr;

  if(! key || ! base || ! comp || !nelp || width < 1){
    errno = EINVAL;
    return(NULL);
  }

  eptr = ((UChar *) base) + (*nelp * width);

  for(ptr = (UChar *) base; ptr < eptr; ptr += width){
    if(comp(ptr, key) == 0)
	return(ptr);
  }

  (*nelp)++;

  base = realloc_forced(base, (*nelp * width));
  eptr = ((UChar *) base) + width * (*nelp - 1);
  memcpy(eptr, key, width);

  return((void *) eptr);
}


#define	PSIZE	200

/*
 * Reads a line of data of arbitrary length from <fp> and returns the
 * result as a malloc'd buffer.  Returns NULL on error.
 */

UChar *
fget_alloc_str(FILE * fp)
{
  UChar		piece[PSIZE];
  UChar		*line, *cptr;
  Int32		newlen, oldlen, plen;

  if(!fp)
    fp = stdin;

  line = NULL;
  oldlen = 0;

  forever{
    cptr = fgets(piece, PSIZE, fp);
    if(!cptr)
	return(line);

    plen = strlen(piece);

    if(line){
	newlen = strlen(line) + plen + 1;
	line = SRENEWP(line, UChar, newlen, oldlen);
	oldlen = newlen;
    }
    else{
	newlen = oldlen = plen + 1;
	line = SNEWP(UChar, newlen);
	if(line)
	  line[0] = '\0';
    }

    if(!line)
	return(NULL);

    strcat(line, piece);

    if(newlen > 1 && plen > 0)
      if(piece[plen - 1] == '\n' ||
		(line[newlen - 2] != '\n' && plen < PSIZE - 1))
	return(line);
  }
}

/* same as fget_alloc_str, but reuses malloc'ed memory space that *sptr
 * points to, if not NULL. The number of malloc'ed bytes is maintained
 * in *num_alloced. If many lines are to be read, using this function
 * is more efficient than fget_alloc_str. Returns a value < 0 on EOF
 * or error. Typical example (with fp already being initialized):
 *
 * UChar	*line = NULL;
 * Int32	numchars = 0;
 *
 * while(!feof(fp)){
 *   if(fget_realloc_str(fp, &line, &numchars))
 *     continue;
 *  ... do sth. with line ...
 * }
 */

Int32
fget_realloc_str(FILE * fp, UChar ** sptr, Int32 * num_alloced)
{
  UChar		piece[PSIZE];
  UChar		*cptr;
  Int32		newlen, plen, slen, n_all;
  Flag		got_sth = NO;

  if(!sptr){
    errno = EINVAL;
    return(-1);
  }

  if(!fp)
    fp = stdin;

  slen = n_all = 0;
  if(*sptr)
    *sptr[0] = '\0';

  if(num_alloced)
    n_all = *num_alloced;

  forever{
    cptr = fgets(piece, PSIZE, fp);
    if(!cptr)
	break;

    got_sth = YES;

    plen = strlen(piece);

    newlen = slen + plen + 1;
    if(*sptr){
	if(newlen > n_all){
	  *sptr = RENEWP(*sptr, UChar, newlen);
	  n_all = newlen;
	}

	cptr = *sptr;
    }
    else{
	cptr = *sptr = NEWP(UChar, newlen);
	n_all = newlen;
	if(cptr)
	  cptr[0] = '\0';
    }

    if(!cptr)
	return(-1);

    strcpy(cptr + slen, piece);
    slen += plen;

    if(newlen > 1 && plen > 0)
      if(piece[plen - 1] == '\n' ||
		(cptr[newlen - 2] != '\n' && plen < PSIZE - 1))
	break;
  }

  if(num_alloced)
    *num_alloced = n_all;

  return(got_sth ? 0 : -1);
}

Int32
wait_for_input(int fd, Int32 timeout)
{
  struct pollfd		fds;
  int	i;

  if(timeout < 0)
    timeout = 0;

  fds.fd = fd;
  fds.events = POLLIN;

  i = poll(&fds, 1, 1000 * timeout);

  return(i > 0 ? (POLLINEVENT(fds.revents) ? 1 : 0) : i);
}

Int32
read_with_timeout(int fd, UChar * buf, Int32 n, Int32 timeout)
{
  if(timeout > 0){
    if(wait_for_input(fd, timeout) < 1)
	return(-1);
  }

  return(read_forced(fd, buf, n));
}

Int32
wait_until_writable(int fd, Int32 timeout)
{
  struct pollfd		fds;
  struct timeval	tv;
  int	i;

  if(timeout < 0)
    timeout = 0;

  fds.fd = fd;
  fds.events = POLLOUT;

  i = poll(&fds, 1, timeout * 1000);

  return(i > 0 ? (POLLOUTEVENT(fds.revents) ? 1 : 0) : i);
}

void *ba_search(
  void		*key,
  void		*base,
  Uns32		nel,
  Uns32		size,
  int		(*comp)(void *, void *))
{
  UChar		*ptr, *eptr, *bptr;
  Int32		act_idx, i, d, prev, prev_d;

  if(! key || ! base || ! nel || ! size || ! comp)
    return(NULL);

  act_idx = ((nel - 1) >> 1) + 1;
  prev_d = d = ((act_idx - 1) >> 1) + 1;
  prev = 0;
  bptr = (UChar *) base;
  eptr = bptr + size * (nel - 1);

  i = comp(key, base);
  if(! i)
    return(base);
  if(i < 0)
    return(NULL);
  i = comp(key, eptr);
  if(i >= 0)
    return(eptr);

  forever{
    ptr = bptr + (act_idx - 1) * size;

    i = comp(key, ptr);

    if(i == 0)
	return(ptr);

    if(((prev > 0 && i < 0) || (prev < 0 && i > 0)) && prev_d <= 1)
	return(i < 0 ? ptr - size : ptr);

    prev = i;

    if(i > 0){
	act_idx += d;
	if(act_idx > nel){
	  if(d == 1)
	    return(ptr);
	  act_idx -= d;
	  i = nel - act_idx - 1;
	  if(i <= 1)
	    return(ptr);
	  d = (i >> 1) + 1;
	  act_idx += d;
	  if(act_idx > nel){
	    fprintf(stderr, "%s\n", compiler_error);
	    exit(0);
	  }
	}
    }
    else{
	act_idx -= d;
	if(act_idx < 1){
	  if(d == 1)
	    return(NULL);
	  act_idx += d;
	  d = ((act_idx - 1) >> 1) + 1;
	  act_idx -= d;
	  if(act_idx < 1){
	    fprintf(stderr, "%s\n", compiler_error);
	    exit(0);
	  }
	}
    }

    prev_d = d;
    d = ((d - 1) >> 1) + 1;

    if(!d)
	return(NULL);
  }

  return(NULL);
}

/*
 * removes all leading and trailing space (if any) from str
 */

void
massage_string(UChar * str)
{
  UChar		*c1;

  if(!str)
    return;

  c1 = str;
  while(isspace(*c1) && *c1)
    c1++;

  if(! *c1){
    *str = '\0';
    return;
  }

  while(*c1)
    *(str++) = *(c1++);

  do{
    *(str--) = '\0';
  } while(isspace(*str));
}

/*
 * On success returns a malloc'd string the contents of which is the
 * concatenation of string1 and string2.  Returns NULL on failure.
 */

UChar *
strapp(UChar * string1, UChar * string2)
{
  UChar		*newstr;
  Int32		len1, len2;

  len1 = (string1 ? strlen(string1) : 0);
  len2 = (string2 ? strlen(string2) : 0);

  newstr = (UChar *) malloc_forced((len1 + len2 + 1) * sizeof(UChar));

  if(!newstr)
    return(NULL);

  strcpy(newstr, "");
  if(string1)
    strcat(newstr, string1);
  if(string2)
    strcat(newstr, string2);

  return(newstr);
}

/*
 * Take a list of strings and return a malloc'd string which is the 
 * concatenation of all the arguments. The list of arguments must
 * be terminated with a NULL to indicate, that the end of the list
 * is reached
 */

UChar *
strchain(UChar * str, ...)
{
  UChar		*newstr;
  Int32		len;
  va_list	args;

  va_start(args, str);

  if(!str)
    GETOUT;
  str = strdup(str);
  if(!str)
    GETOUT;

  while( (newstr = va_arg(args, UChar *)) ){
    len = strlen(str) + strlen(newstr) + 1;

    str = RENEWP(str, UChar, len);
    if(!str)
	break;

    strcat(str, newstr);
  }

 getout:
  va_end(args);

  return(str);
}

UChar *			/* replace pattern by substitute in string */
repl_substring(UChar * string, UChar * pattern, UChar * substitute)
{
  UChar	*result, *subst, *walking, *newone, *working;

  if(!string){
    errno = EINVAL;
    return(NULL);
  }

  working = (UChar *) strdup(string);
  if(!working)
    return(NULL);

  if(!pattern)
    return(working);

  result = (UChar *) strdup("");
  if(!result){
    free(working);
    return(NULL);
  }

  walking = working;

  while( (subst = strstr(walking, pattern)) ){
    *subst = '\0';
    newone = strapp(result, walking);
    *subst = pattern[0];

    free(result);

    if(!newone){
	free(working);
	return(NULL);
    }

    result = strapp(newone, substitute);
    free(newone);

    if(!result){
	free(working);
	return(NULL);
    }

    walking = subst + strlen(pattern);
  }

  newone = strapp(result, walking);

  free(result);
  free(working);
  if(! newone)
    return(NULL);

  return(newone);
}

Int32
repl_substrings(UChar ** str, ReplSpec * repls, Int32 n)
{
  UChar	*cptr;
  Int32	i;

  if(!str){
    errno = EINVAL;
    return(-1);
  }

  if(! (*str) || !repls || n < 1)
    return(0);

  for(i = 0; i < n; i++){
    if(strstr(*str, repls[i].token)){
      cptr = repl_substring(*str, repls[i].token,
		repls[i].repl ? repls[i].repl : *(repls[i].replptr));
      if(!cptr)
	return(-1);

      free(*str);
      *str = cptr;
    }
  }

  return(0);
}

/*
 * Takes the string <str> and stores into <wordsp> an array of strings
 * where each array entry is one word from <str>.  Words are sequences
 * of non-whitespace characters.  This function doesn't know anything about
 * quoting.
 *
 * On success, returns the number of words in the array, -1 on error.
 */

Int32
str2words(UChar *** wordsp, UChar * str)
{
  Int32		i, w;
  UChar		**words = NULL, *cptr, *strc = NULL, c;

  if(!wordsp || !str){
    errno = EINVAL;
    return(-1);
  }

  w = word_count(str);
  words = NEWP(UChar *, w + 1);
  memset(words, 0, sizeof(UChar *) * (w + 1));
  strc = strdup(str);

  if(!words || !strc)
    GETOUT;

  cptr = strc;
  for(i = 0; i < w; i++){
    while(isspace(*cptr) && *cptr)
	cptr++;
    if(! *cptr)
	break;

    str = cptr;
    while(!isspace(*cptr) && *cptr)
	cptr++;
    c = *cptr;
    *cptr = '\0';

    words[i] = strdup(str);
    if(!words[i])
	GETOUT;

    *cptr = c;
  }

  free(strc);

  *wordsp = words;
  return(w);

 getout:
  if(strc)
    free(strc);
  if(words){
    for(i = 0; i < w; i++)
	if(words[i])
	  free(words[i]);
    free(words);
  }

  return(-1);
}

/* same like str2words, but quoting "..." is allowed in <str> to put several
 * non-whitespace sequences together into one word
 */

Int32
str2wordsq(UChar *** wordsp, UChar * str)
{
  UChar		*buf = NULL, *cptr;
  UChar		**words = NULL, **w;
  Int32		ret = 0, n;

  if(!wordsp || !str){
    errno = EINVAL;
    GETOUT;
  }

  buf = strdup(str);
  if(!buf)
    GETOUT;

  words = NEWP(UChar *, 1);
  if(!words)
    GETOUT;
  *words = NULL;

  cptr = str;
  n = 0;
  while( (cptr = sscanwordq(cptr, buf)) ){
    words = RENEWP(words, UChar *, n + 2);
    if(!words)
	GETOUT;

    words[n + 1] = NULL;
    words[n] = strdup(buf);
    if(!words[n])
	GETOUT;
    n++;
  }
  ret = n;

  *wordsp = words;

 cleanup:
  if(buf)
    free(buf);

  return(ret);

 getout:
  ret = -1;
  if(words){
    for(w = words; *w; w++)
	free(*w);
    free(words);
  }

  CLEANUP;
}

void
sscancchars(UChar * source, UChar * target)
{
  UChar		c, a;
  int		i;

  if(!source || !target){
    errno = EINVAL;
    return;
  }

  while(*source){
    if(*source != '\\'){
	*(target++) = *(source++);
	continue;
    }
    else{
	source++;
	switch(*source){
	  case 'a':
	    c = '\a';
	    break;
	  case 'b':
	    c = '\b';
	    break;
	  case 'n':
	    c = '\n';
	    break;
	  case 'f':
	    c = '\f';
	    break;
	  case 'r':
	    c = '\r';
	    break;
	  case 't':
	    c = '\t';
	    break;
	  case 'v':
	    c = '\v';
	    break;
	  default:
	    if(*source >= '0' && *source <= '7'){
		c = 0;
		for(i = 0; i < 3; i++){
		  if(c > 037)
		    break;
		  a = source[i];
		  if(a > '7' || a < '0')
		    break;
		  c = (c << 3) | (a - '0');
		}

		source += i - 1;
	    }
	    else{
		c = *source;
	    }
	}
	source++;
	*(target++) = c;
    }
  }

  *target = '\0';
}

UChar *
sscancstr(UChar * source, UChar * target)
{
  UChar		*str, *cptr, *cptr2, escaped;

  if(!source || !target){
    errno = EINVAL;
    return(NULL);
  }

  if(*source != '\"'){
    errno = EINVAL;
    return(NULL);
  }
  source++;

  for(cptr = strchr(source, '\"'); cptr; cptr = strchr(cptr + 1, '\"')){
    escaped = 0;
    for(cptr2 = cptr - 1; cptr2 >= source; cptr2--){
      if(*cptr2 == '\\')
	escaped = ! escaped;
      else
	break;
    }

    if(!escaped)
	break;
  }

  if(!cptr){
    errno = EINVAL;
    return(NULL);
  }

  if(! (str = strdup(source)))
    return(NULL);

  str[cptr - source] = '\0';

  sscancchars(str, target);
  free(str);

  return(source + 1);
}

Int8
escaped(UChar * str, UChar * c, UChar escc)
{
  Int8		escaped = 0;

  if(!str || !c || c < str || c >= str + strlen(str)){
    errno = EINVAL;
    return(-1);
  }

  if(!escc)
    escc = '\\';

  while(c > str){
    c--;

    if(*c == escc){
	escaped = !escaped;
    }
    else
	break;
  }

  return(escaped);
}

Uns8
parity_byte(UChar * str, Int32 num)
{
  Uns32		sum;

  if(!str)
    return(0);

  for(sum = 0; num > 0; num--)
    sum += (Uns32) *(str++);

  return((Uns8)(sum & 0xff));
}

static Int32
_cmd2argv_(char *** argvp, char * cmd, UChar flags)
{
  UChar		**words, *cptr, *cptr2, quoted, fullpath;
  Int32		i, w;

  if(!argvp)
    return(0);

  if(!cmd)
    cmd = "";

  quoted = flags & 1;
  fullpath = flags & 2;

  w = word_count(cmd);
  if(w < 1){
    errno = EINVAL;
    return(-1);
  }

  if(quoted)
    w = str2wordsq(&(words), (UChar *) cmd);
  else
    w = str2words(&(words), (UChar *) cmd);

  if(w < 0)
    return(w);

  words = (UChar **) realloc_forced(words, sizeof(UChar *) * (w + 2));

  if(!words)
    return(-1);

  words[w + 1] = NULL;
  for(i = w - 1; i >= 0; i--)
    words[i + 1] = words[i];

  if(fullpath)
    words[1] = strdup(words[0]);
  else{
    cptr = FN_LASTDIRDELIM(words[0]);
    if(cptr)
	cptr2 = strdup(cptr + 1);
    else
      cptr2 = strdup(words[1]);

    if(cptr2)
	words[1] = cptr2;
    else{
      if(cptr)
	words[1] = cptr + 1;
    }
  }

  *argvp = (char **) words;

  return(0);
}

Int32
cmd2argv(char *** argvp, char * cmd)
{
  return(_cmd2argv_(argvp, cmd, 0));
}

Int32
cmd2argvq(char *** argvp, char * cmd)
{
  return(_cmd2argv_(argvp, cmd, 1));
}

Int32
cmd2argvf(char *** argvp, char * cmd)
{
  return(_cmd2argv_(argvp, cmd, 2 + 0));
}

Int32
cmd2argvqf(char *** argvp, char * cmd)
{
  return(_cmd2argv_(argvp, cmd, 2 + 1));
}


/* compare functions */

int
cmp_UCharPTR(void * p1, void * p2)
{
  return(strcmp(*((char **) p1), *((char **) p2)));
}

#define	intcmp(type)				\
int						\
cmp_##type(void * p1, void * p2)		\
{						\
  if(!p1 || !p2)				\
    return(0);					\
  return(*((type *) p1) - *((type *) p2));	\
}

intcmp(Int32)

intcmp(Int16)

intcmp(Int8)

intcmp(SChar)

#define	unscmp(type)					\
int							\
cmp_##type(void * p1, void * p2)			\
{							\
  if(!p1 || !p2)					\
    return(0);						\
  return(*((type *) p1) == *((type *) p2) ? 0 :		\
	(*((type *) p1) > *((type *) p2) ? 1 : -1));	\
}

unscmp(Uns32)

unscmp(Uns16)

unscmp(Uns8)

unscmp(UChar)

int
cmp_Uns32Ranges(void * p1, void * p2)
{
  if(!p1 || !p2)
    return(0);
  return((Int32)(((Uns32Range *)p1)->first) - (Int32)(((Uns32Range *)p2)->last));
}

#define	RET(ret)	{ r = (ret); goto done; }

static int
process_version_strings(UChar * v1, UChar * v2, Flag cmp)
{
  int		i1, i2, n1, n2, i, j, g = 0, r;
  UChar		*cptr;

  if(!v1 || !v2)
    RET(0);
  forever{
    n1 = n2 = 0;
    i = sscanf(v1, "%d%n", &i1, &n1);
    j = sscanf(v2, "%d%n", &i2, &n2);
    if(i < 1 && j < 1)
	RET(0);
    if(i < 1)
	RET(-1);
    if(j < 1)
	RET(1);
    if(i1 > i2)
	RET(1);
    if(i2 > i1)
	RET(-1);

    g += 8;
    v1 += n1;
    v2 += n2;

    while(!isalnum(*v1) && *v1)
	v1++;
    while(!isalnum(*v2) && *v2)
	v2++;
    for(cptr = v1; (*cptr == '0' || !isalnum(*cptr)) && *cptr; cptr++);
    if(!*cptr)
	v1 = cptr;
    for(cptr = v2; (*cptr == '0' || !isalnum(*cptr)) && *cptr; cptr++);
    if(!*cptr)
	v2 = cptr;

    while(isalpha(*v1) && isalpha(*v2)){
	if(tolower(*v1) > tolower(*v2))
	  RET(1);
	if(tolower(*v1) < tolower(*v2))
	  RET(-1);

	g++;
	v1++;
	v2++;
    }
    if(isalpha(*v1))
	RET(1);
    if(isalpha(*v2))
	RET(-1);

    if(!(*v1) && !(*v2))
	RET(0);
    if(!(*v1))
	RET(-1);
    if(!(*v2))
	RET(1);

    if(isdigit(*v1) && isdigit(*v2))
	continue;
    if(isdigit(*v1))
	RET(1);
    if(isdigit(*v2))
	RET(-1);

    while(!isalnum(*v1) && *v1)
	v1++;
    while(!isalnum(*v2) && *v2)
	v2++;

    if(!(*v1) && !(*v2))
	RET(0);
    if(!(*v1))
	RET(-1);
    if(!(*v2))
	RET(1);
  }

  return(-1000);

 done:
  if(cmp)
    return(r);

  return(g);
}

#undef	RET

int
compare_version_strings(UChar * v1, UChar * v2)
{
  return(process_version_strings(v1, v2, YES));
}

int
version_string_corr(UChar * v1, UChar * v2)
{
  return(process_version_strings(v1, v2, NO));
}

int
closer_version(UChar * ref, UChar * v1, UChar * v2)
{
  int		i1, i2, iref, n1, n2, nref, c1, c2, cref, r, vc, d1, d2;
  Flag		s1 = NO, s2 = NO;	/* smaller than reference */
  UChar		*cptr;

  if(!v1 || !v2 || !ref)
    return(0);

  vc = compare_version_strings(v1, v2);
  if(!vc)
    return(0);
  c1 = compare_version_strings(v1, ref);
  if(!c1)
    return(1);
  if(c1 < 0)
    s1 = YES;
  c2 = compare_version_strings(v2, ref);
  if(!c2)
    return(2);
  if(c2 < 0)
    s2 = YES;
  if(c1 > 0 && c2 > 0)
    return(vc > 0 ? 2 : 1);
  if(c1 < 0 && c2 < 0)
    return(vc < 0 ? 2 : 1);

  forever{
    i1 = i2 = iref = n1 = n2 = nref = 0;
    c1 = sscanf(v1, "%d%n", &i1, &n1);
    c2 = sscanf(v2, "%d%n", &i2, &n2);
    cref = sscanf(ref, "%d%n", &iref, &nref);
    if(c1 > 0)
	v1 += n1;
    if(c2 > 0)
	v2 += n2;
    if(cref > 0)
	ref += nref;
    while(!isalnum(*v1) && *v1)
	v1++;
    while(!isalnum(*v2) && *v2)
	v2++;
    while(!isalnum(*ref) && *ref)
	ref++;
    for(cptr = v1; (*cptr == '0' || !isalnum(*cptr)) && *cptr; cptr++);
    if(!*cptr)
	v1 = cptr;
    for(cptr = v2; (*cptr == '0' || !isalnum(*cptr)) && *cptr; cptr++);
    if(!*cptr)
	v2 = cptr;
    for(cptr = ref; (*cptr == '0' || !isalnum(*cptr)) && *cptr; cptr++);
    if(!*cptr)
	ref = cptr;
    if(c1 < 1 && c2 < 1){
	if(*v1 && !*v2)
	  r = 2;
	else if(!*v1 && *v2)
	  r = 1;
	else if(!*v1 && !*v2)
	  return(0);
	else if(tolower(*v1) < tolower(*v2))
	  r = 1;
	else if(tolower(*v1) > tolower(*v2))
	  r = 2;
	else
	  return(0);
	return(s1 ? r : 2 - r);	/* if both compare versions are exhausted here, */
    }				/* s1 == s2, so it is sufficient to check s1 */
    if(s1 && s2 && i1 != i2)
	return(i1 > i2 ? 1 : 2);
    if((s1 && !s2) || (!s1 && s2)){
	if(i2 - iref > iref - i1)
	  return(vc > 0 ? 2 : 1);
	if(i2 - iref < iref - i1)
	  return(vc > 0 ? 1 : 2);
    }
    else{
	d1 = ABS(i1 - iref);
	d2 = ABS(i2 - iref);
	if(d1 < d2)
	  return(1);
	if(d1 > d2)
	  return(2);
    }
    while(isalpha(*v1) && isalpha(*v2) && isalpha(*ref)){
	i1 = tolower(*v1);
	i2 = tolower(*v2);
	iref = tolower(*ref);
	if(s1 && s2 && i1 != i2)
	  return(i1 > i2 ? 1 : 2);
	if((s1 && !s2) || (!s1 && s2)){
	  if(i2 - iref > iref - i1)
	    return(vc > 0 ? 2 : 1);
	  if(i2 - iref < iref - i1)
	    return(vc > 0 ? 1 : 2);
	}
	else{
	  d1 = ABS(i1 - iref);
	  d2 = ABS(i2 - iref);
	  if(d1 < d2)
	    return(1);
	  if(d1 > d2)
	    return(2);
	}
	v1++;
	v2++;
	ref++;
	while(!isalnum(*v1) && *v1)
	  v1++;
	while(!isalnum(*v2) && *v2)
	  v2++;
	while(!isalnum(*ref) && *ref)
	  ref++;
    }
    if(isalpha(*ref)){
	c1 = tolower(*ref);
	if(isalpha(*v1)){
	  if(s1 && s2)
	    return(1);
	  if(!s1 && !s2)
	    return(2);
	  d1 = ABS(c1 - tolower(*v1));
	  d2 = ABS(c1 - 'a' + 1);
	  if(d1 < d2)
	    return(1);
	  return(2);
	}
	if(isalpha(*v2)){
	  if(s1 && s2)
	    return(2);
	  if(!s1 && !s2)
	    return(1);
	  d1 = ABS(c1 - tolower(*v2));
	  d2 = ABS(c1 - 'a' + 1);
	  if(d1 < d2)
	    return(2);
	  return(1);
	}
	return(vc > 0 ? 1 : 2);
    }
    else{
	while(isalpha(*v1) && isalpha(*v2)){
	  if(tolower(*v1) < tolower(*v2))
	    return(s1 ? 2 : 1);
	  if(tolower(*v1) > tolower(*v2))
	    return(s1 ? 1 : 2);
	  v1++;
	  v2++;
	  while(!isalnum(*v1) && *v1)
	    v1++;
	  while(!isalnum(*v2) && *v2)
	    v2++;
	}
	if(isalpha(*v1))
	  return(s1 ? 1 : 2);
	if(isalpha(*v2))
	  return(s2 ? 2 : 1);
    }
	
    while(!isalnum(*v1) && *v1)
	v1++;
    while(!isalnum(*v2) && *v2)
	v2++;
    while(!isalnum(*ref) && *ref)
	ref++;

    if(!*v1 && !*v2)
	return(0);
  }

  return(-1000);
}

static UChar	*chars64 = "0123456789+-"
			"abcdefghijklmnopqrstuvwxzy"
			"ABCDEFGHIJKLMNOPQRSTUVWXZY";

UChar
char64(Int32 n)
{
  if(n > 64 || n < 0)
    return('\0');

  return(chars64[n]);
}

Int32
ugids_from_str(
  UChar		*str,
  uid_t		*uid,
  gid_t		*gid,
  int		*ngids,
  gid_t		**gids)
{
  int		luid, lgid, i1, n1, lngids = 0;
  gid_t		*lgids = NULL;
  Int32		r = 0;

  if(!str){
    errno = EINVAL;
    return(-1);
  }

  luid = lgid = -1;
  sscanf(str, "%d:%d%n", &luid, &lgid, &n1);
  if(lgid == -1)
    CLEANUPR(-1);

  str += n1;
  while(*str){
    i1 = -1;
    sscanf(str, ":%d%n", &i1, &n1);
    if(i1 == -1 && *str)
	CLEANUPR(-2);
    if(!(lgids = ZRENEWP(lgids, gid_t, lngids + 1)))
	CLEANUPR(-3);
    lgids[lngids] = i1;
    lngids++;
    str += n1;
  }

  if(uid)
    *uid = luid;
  if(gid)
    *gid = lgid;
  if(ngids)
    *ngids = lngids;
  if(gids){
    *gids = lgids;
    lgids = NULL;
  }

 cleanup:
  ZFREE(lgids);

  return(r);
}

UChar *
str_from_ugids(uid_t uid, gid_t gid, int ngids, gid_t * gids)
{
  UChar		*uidstr = NULL;
  Int32		i;

  if(!(uidstr = NEWP(UChar, (2 + ngids) * 20)))
    return(NULL);

  sprintf(uidstr, "%d:%d", (int) uid, (int) gid);
  if(gids)
    for(i = 0; i < ngids; i++)
      sprintf(uidstr + strlen(uidstr), ":%d", (int) gids[i]);

  return(uidstr);
}

UChar *
read_command_output(UChar * cmd, int fds, int * rpst)
{
  UChar		*cptr, *dbuf = NULL, *r = NULL;
  Int32		n = 0, i;
  int		pid = 0, pst, fd;

  if(!cmd)
    cmd = "";

  dbuf = NEWP(UChar, 1024);
  if(!dbuf)
    return(NULL);

  fd = open_from_pipe(cmd, NULL, fds, &pid);
  if(fd < 0)
    GETOUT;

  forever{
    i = read_forced(fd, dbuf + n, 1024);
    if(i <= 0)
	break;

    n += i;
    dbuf = RENEWP(dbuf, UChar, n + 1024 + 1);
    if(!dbuf)
	GETOUT;
  }

  dbuf = RENEWP(dbuf, UChar, n + 1);
  if(!dbuf)
    GETOUT;	/* should never happen */

  dbuf[n] = '\0';
  r = dbuf;

 cleanup:
  if(fd >= 0){
    close(fd);
    if(pid > 0){
	i = waitpid(pid, &pst, WNOHANG);
	if(i != pid){
	  kill(pid, SIGKILL);
	  i = waitpid(pid, &pst, 0);
	}
    }
  }
  if(rpst)
    *rpst = pst;
  return(r);

 getout:
  ZFREE(dbuf)
  CLEANUP;
}

UChar *
create_uuid(UChar * base)
{
  UChar		*uuid;

  uuid = tmp_name(base ? base : (UChar *) "");
  if(!uuid)
    return(NULL);

  if(!base && uuid[0] == '_')
    memmove(uuid, uuid + 1, strlen(uuid));

  return(uuid);
}
