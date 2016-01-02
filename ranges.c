/****************** Start of $RCSfile: ranges.c,v $  ****************
*
* $Source: /home/alb/afbackup/afbackup-3.5.8.12/RCS/ranges.c,v $
* $Id: ranges.c,v 1.5 2011/12/14 20:27:23 alb Exp alb $
* $Date: 2011/12/14 20:27:23 $
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

  static char * fileversion = "$RCSfile: ranges.c,v $ $Source: /home/alb/afbackup/afbackup-3.5.8.12/RCS/ranges.c,v $ $Id: ranges.c,v 1.5 2011/12/14 20:27:23 alb Exp alb $ " PACKAGE " " VERSION_STRING;

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#ifdef HAVE_MALLOC_H
#include <malloc.h>
#endif

#include <genutils.h>
#include <fileutil.h>

Int32
len_Uns32Ranges(Uns32Range * ranges)
{
  Int32		i;

  if(!ranges)
    return(0);

  for(i = 0; ranges[i].first <= ranges[i].last; i++);

  return(i);
}

Int32
num_Uns32Ranges(Uns32Range * aranges)
{
  Uns32Range	*ranges, *rp;
  Int32		n;

  if(!aranges)
    return(0);

  ranges = dup_Uns32Ranges(aranges);
  if(!ranges)
    return(-1);

  pack_Uns32Ranges(ranges, NULL);

  for(rp = ranges, n = 0; rp->first <= rp->last; rp++)
    n += (rp->last - rp->first + 1);

  free(ranges);

  return(n);
}

Uns32Range *
add_to_Uns32Ranges(
  Uns32Range	*ranges,
  Int32		first,
  Int32		last)
{
  Int32		n;
  Int32		b;

  if(ranges)
    n = len_Uns32Ranges(ranges);
  else
    n = 0;

  ranges = ZRENEWP(ranges, Uns32Range, n + 2);
  if(!ranges)
    return(NULL);

  if(first > last)
    b = first, first = last, last = b;

  ranges[n].first = first;
  ranges[n].last = last;
  n++;
  ranges[n].first = 1;
  ranges[n].last = 0;

  pack_Uns32Ranges(ranges, &n);

  return(ranges);
}

Uns32Range *
del_one_from_Uns32Ranges(
  Uns32Range	*ranges,
  Int32		num)
{
  Int32		i, l;

  l = len_Uns32Ranges(ranges);

  for(i = 0; i < l; i++){
    if(num == ranges[i].first){
	ranges[i].first++;
	if(ranges[i].first > ranges[i].last){
	  memmove(ranges + i, ranges + i + 1, (l - i) * sizeof(Uns32Range));
	  i--;
	  l--;
	}
    }
    else if(num == ranges[i].last){
	ranges[i].last--;
	if(ranges[i].first > ranges[i].last){
	  memmove(ranges + i, ranges + i + 1, (l - i) * sizeof(Uns32Range));
	  i--;
	  l--;
	}
    }
    else if(num < ranges[i].last && num > ranges[i].first){
	l++;
	ranges = ZRENEWP(ranges, Uns32Range, l + 1);
	if(!ranges)
	  return(NULL);

	memmove(ranges + i + 1, ranges + i, (l - i) * sizeof(Uns32Range));
	ranges[i].last = num - 1;
	ranges[i + 1].first = num + 1;
	i += 2;
    }
  }

  return(ranges);	
}

Uns32Range *
del_range_from_Uns32Ranges(
  Uns32Range	*range,
  Uns32Range	*to_remove)
{
  Int32		i, j;

  if(!to_remove)
    return(range);

  for(i = 0; to_remove[i].first <= to_remove[i].last; i++){
    for(j = to_remove[i].first; j <= to_remove[i].last; j++){
	range = del_one_from_Uns32Ranges(range, j);
	if(!range)
	  return(NULL);
    }
  }

  return(range);
}

Uns32Range *
common_Uns32Ranges(Uns32Range * ranges1, Uns32Range * ranges2)
{
  Int32		i, j, n;
  Uns32Range	*result;

  result = empty_Uns32Ranges();
  if(!result)
    return(NULL);

  n = len_Uns32Ranges(ranges1);
  for(i = 0; i < n; i++){
    for(j = ranges1[i].first; j <= ranges1[i].last; j++){
      if(in_Uns32Ranges(ranges2, j)){
	result = add_to_Uns32Ranges(result, j, j);
	if(!result)
	  return(NULL);
      }
    }
  }

  pack_Uns32Ranges(result, NULL);

  return(result);
}

Uns32Range *
empty_Uns32Ranges()
{
  Uns32Range	*empty;

  empty = NEWP(Uns32Range, 1);
  if(!empty)
    return(NULL);

  empty->first = 1;
  empty->last = 0;
  return(empty);
}

Flag
in_Uns32Ranges(Uns32Range * ranges, Int32 val)
{
  if(!ranges)		/* we allow this */
    return(NO);

  while(ranges->first <= ranges->last){
    if(val >= ranges->first && val <= ranges->last)
	return(YES);
    ranges++;
  }

  return(NO);
}

Flag
overlap_Uns32Ranges(Uns32Range * r1, Uns32Range * r2)
{
  Uns32Range	*p1, *p2;

  if(!r1 || !r2)
    return(NO);

  for(p1 = r1; p1->last >= p1->first; p1++){
    for(p2 = r2; p2->last >= p2->first; p2++){
      if(p1->first <= p2->last && p1->last >= p2->first){
	return(YES);
      }
    }
  }

  return(NO);
}

Int32
next_in_Uns32Ranges(Uns32Range * ranges, Int32 val, Flag rotate)
{
  Int32		higher, lowest;

  higher = lowest = -1;
  val++;

  while(ranges->first <= ranges->last){
    if(ranges->first <= val && ranges->last >= val)
	return(val);

    if(ranges->first >= val && (ranges->first < higher || higher < 0))
	higher = ranges->first;

    if(lowest < 0)
	lowest = ranges->first;

    if(ranges->first < lowest)
	lowest = ranges->first;

    ranges++;
  }

  if(higher >= 0)
    return(higher);

  return(rotate ? lowest : -1);
}

Int32
prev_in_Uns32Ranges(Uns32Range * ranges, Int32 val, Flag rotate)
{
  Int32		highest, lower;

  highest = lower = -1;
  val--;

  while(ranges->first <= ranges->last){
    if(ranges->first <= val && ranges->last >= val)
	return(val);

    if(ranges->last <= val && (ranges->last > lower || lower < 0))
	lower = ranges->last;

    if(highest < 0)
	highest = ranges->last;

    if(ranges->last > highest)
	highest = ranges->last;

    ranges++;
  }

  if(lower >= 0)
    return(lower);

  return(rotate ? highest : -1);
}

Uns32Range *
sscan_Uns32Ranges(
  UChar		*string,
  Int32		firstu,
  Int32		lastu,
  Int32		*num,
  UChar		**nextc)
{
  Uns32Range	*ranges;
  Int32		num_ranges = 0;
  int		i, j, k, n, nm1;

  ranges = empty_Uns32Ranges();
  if(!ranges)
    return(NULL);

  forever{
    while(*string == ',')
	string++;

    i = sscanf(string, "%d%n", &j, &n);
    if(i < 1)
	break;

    string = first_nospace(string + n);
    nm1 = num_ranges;
    num_ranges++;
    ranges = ZRENEWP(ranges, Uns32Range, num_ranges + 1);
    if(!ranges)
	return(NULL);

    ranges[num_ranges].first = 1;
    ranges[num_ranges].last = 0;

    if(j < 0){
	ranges[nm1].first = firstu;
	ranges[nm1].last = - j;
    }
    else{
	ranges[nm1].first = j;
	if(*string != '-'){
	  ranges[nm1].last = j;

	  if(ranges[nm1].first > ranges[nm1].last)
	    memswap(&(ranges[nm1].first), &(ranges[nm1].last), sizeof(Int32));

	  if(*string != ',')
	    break;

	  string++;
	  continue;
	}

	if(*string == '-'){
	  string++;
	  i = sscanf(string, "%d%n", &k, &n);
	  if(i < 1){
	    ranges[nm1].last = lastu;
	  }
	  else{
	    ranges[nm1].last = k;
	    string = first_nospace(string + n);
	  }

	  if(ranges[nm1].first > ranges[nm1].last)
	    memswap(&(ranges[nm1].first), &(ranges[nm1].last), sizeof(Int32));

	  continue;
	}

	break;
    }
  }

  if(num)
    *num = num_ranges;
  if(nextc)
    *nextc = string;

  return(ranges);
}

Uns32Range *		/* here there may be whitespace inbetween */
sscan_Uns32Ranges__(
  UChar		*string,
  Int32		firstu,
  Int32		lastu,
  Int32		*num,
  UChar		**nextc)
{
  UChar		*cptr, *cptr2;
  Uns32Range	*ranges, *newranges = NULL;

  ranges = empty_Uns32Ranges();
  if(!ranges)
    GETOUT;

  cptr2 = NULL;
  cptr = string;
  forever{
    newranges = sscan_Uns32Ranges(cptr, firstu, lastu, NULL, &cptr2);
    if(!newranges)
	GETOUT;

    if(cptr2 == cptr)
	break;

    cptr = cptr2;
    if(merge_Uns32Ranges(&ranges, newranges))
	GETOUT;
    ZFREE(newranges);
  }

  if(num)
    *num = len_Uns32Ranges(ranges);
  if(nextc)
    *nextc = cptr;

 cleanup:
  ZFREE(newranges);

  return(ranges);

 getout:
  ZFREE(ranges);
  CLEANUP;
}

Uns32Range *	/* here there may be whitespace inbetween, order is retained */
sscan_Uns32Ranges_(
  UChar		*string,
  Int32		firstu,
  Int32		lastu,
  Int32		*num,
  UChar		**nextc)
{
  UChar		*nstring, *cptr;
  Uns32Range	*ranges = NULL;

  nstring = strdup(string);
  if(!nstring)
    GETOUT;

  for(cptr = first_space(nstring); *cptr; cptr = first_space(cptr))
    *(cptr++) = ',';

  ranges = sscan_Uns32Ranges(nstring, firstu, lastu, NULL, &cptr);
  if(!ranges)
    GETOUT;

  if(num)
    *num = len_Uns32Ranges(ranges);
  if(nextc)
    *nextc = cptr;

 cleanup:
  ZFREE(nstring);

  return(ranges);

 getout:
  ZFREE(ranges);
  CLEANUP;
}

Int32
pack_Uns32Ranges(Uns32Range * ranges, Int32 * n)
{
  Int32		i, j, num = 0;
  Uns32Range	*rptr1, *rptr2;

  if(n)
    num = *n;

  if(num <= 0)
    num = len_Uns32Ranges(ranges);

  for(i = 0; i < num - 1; i++){
    rptr1 = ranges + i;
    for(j = i + 1; j < num; j++){
	rptr2 = ranges + j;
	if((rptr1->first < rptr2->first && rptr1->last > rptr2->last)
		|| (rptr1->first > rptr2->first && rptr1->last < rptr2->last)
		|| (rptr1->first + 1 >= rptr2->first && rptr1->first <= rptr2->last + 1)
		|| (rptr2->first + 1 >= rptr1->first && rptr2->first <= rptr1->last + 1)
		|| (rptr1->last <= rptr2->last + 1 && rptr1->last + 1 >= rptr2->first)
		|| (rptr2->last <= rptr1->last + 1 && rptr2->last + 1 >= rptr1->first)){
	  rptr1->first = MIN(rptr1->first, rptr2->first);
	  rptr1->last = MAX(rptr1->last, rptr2->last);

	  memmove(rptr2, rptr2 + 1, (num - j) * sizeof(Uns32Range));
	  num--;
	  i--;
	  break;
	}
    }
  }

  if(n)
    *n = num;

  return(0);
}

Int32
fprint_Uns32Ranges(FILE *fp, Uns32Range * ranges, Int32 n)
{
  Int32		i;
  UChar		*sep = "";

  if(!ranges)
    return(0);

  for(i = 0; (n <= 0 && ranges[i].first <= ranges[i].last) || i < n; i++){
    if(ranges[i].last > ranges[i].first){
	if(fprintf(fp, "%s%lu-%lu", sep,
			(unsigned long) ranges[i].first,
			(unsigned long) ranges[i].last) < 1)
	  break;
    }
    else{
	if(fprintf(fp, "%s%lu", sep,
			(unsigned long) ranges[i].first) < 1)
	  break;
    }

    sep = ",";
  }

  return(i);
}

UChar *
str_Uns32Ranges(Uns32Range * ranges, Int32 n)
{
  Int32		i, newlen;
  UChar		*sep = "";
  UChar		ubuf[64];
  UChar		*str;

  str = strdup("");
  if(!str)
    return(NULL);

  if(!ranges)
    return(str);

  for(i = 0; (n <= 0 && ranges[i].first <= ranges[i].last) || i < n; i++){
    if(ranges[i].last > ranges[i].first){
	sprintf(ubuf, "%s%lu-%lu", sep,
			(unsigned long) ranges[i].first,
			(unsigned long) ranges[i].last);
    }
    else{
	sprintf(ubuf, "%s%lu", sep,
			(unsigned long) ranges[i].first);
    }

    sep = ",";

    newlen = strlen(ubuf) + 1;
    newlen += strlen(str);
    str = RENEWP(str, UChar, newlen);
    if(!str)
	return(NULL);

    strcat(str, ubuf);
  }

  return(str);
}

Int32
merge_Uns32Ranges(Uns32Range ** target_range, Uns32Range * mergeable)
{
  Int32		n1, n2;
  Uns32Range	*new_ranges;

  n1 = len_Uns32Ranges(*target_range);
  n2 = len_Uns32Ranges(mergeable);

  new_ranges = ZRENEWP(*target_range, Uns32Range, n1 + (++n2));
  if(!new_ranges)
    return(-1);

  *target_range = new_ranges;

  if(mergeable)
    memcpy(new_ranges + n1, mergeable, n2 * sizeof(*mergeable));
  else{
    new_ranges[n1].first = 1;
    new_ranges[n1].last = 0;
  }

  return(pack_Uns32Ranges(new_ranges, NULL));
}

Uns32Range *
dup_Uns32Ranges(Uns32Range * source)
{
  Int32		n;
  Uns32Range	*new_ranges;

  if(!source)
    return(NULL);

  n = len_Uns32Ranges(source) + 1;
  new_ranges = NEWP(Uns32Range, n);
  if(new_ranges)
    memcpy(new_ranges, source, n * sizeof(*source));

  return(new_ranges);
}

Int32
foreach_Uns32Ranges(
  Uns32Range	*ranges,
  Int32		(*func)(Int32, void *),
  void		*data)
{
  Int32		i, j, r, rt = 0;

  if(!ranges)
    return(0);

  for(i = 0; ranges[i].first <= ranges[i].last; i++){
    for(j = ranges[i].first; j <= ranges[i].last; j++){
	r = (*func)(j, data);
	if(r < 0)
	  return(r);
	if(r > 0)
	  rt = r;
    }
  }

  return(rt);
}
