#include <x_types.h>
#include <genutils.h>

static UChar	*base64table =	"ABCDEFGHIJKLMNOPQRSTUVWXYZ"
				"abcdefghijklmnopqrstuvwxyz"
				"0123456789+/=";

/* target must be a valid pointer to a UChar*, who may be NULL, so
 *      space is allocated
 * If num is < 0, strlen is used to determine the length of the source.
 * If several subsequend data chunks should be encoded, state must be
 * a valid pointer to a Uns32, that must for the first call be
 * initialized with 0  */
Int32
base64_decode(UChar * source, UChar ** target, Int32 num, Uns32 * state)
{
  UChar		*result = NULL, *cptr, *endptr;
  Int32		i, r = 0, bp, c;

  if(!target){
    errno = EINVAL;
    return(-2);
  }

  endptr = source + (num < 0 ? strlen(source) : num);

  result = *target;
  if(!result){
    result = NEWP(UChar, (endptr - source) * 6 / 8 + 2);
    if(! result)
	return(-1);
    *target = result;
  }
  *result = '\0';

  if(state){
    c = (*state >> 8);
    bp = 10 - (*state & 0xff);
  }
  else{
    c = 0;
    bp = 10;
  }

  while(source < endptr){
    if(isspace(*source)){
	source++;
	continue;
    }

    cptr = strchr(base64table, *source);
    if(!cptr)
	CLEANUPR(-2);

    i = cptr - (&(base64table[0]));
    if(i < 64){
	c |= (i << bp);
    }
    if(bp <= 8 || i >= 64){
	if(i >= 64){
	  bp = 10;
	  c = 0;
	  break;
	}
	*(result++) = (c >> 8);
	*result = '\0';
	r++;
	c = ((c & 0xff) << 8);
	bp += 8;
    }
    bp -= 6;

    source++;
  }

 cleanup:
  if(state)
    *state = (10 - bp) | (c << 8);

  return(r);
}

/* target must be a valid pointer to a UChar*, who may be NULL, so
 *      space is allocated
 * If num is < 0, strlen is used to determine the length of the source.
 * If several subsequend data chunks should be encoded, state must be
 * a valid pointer to a Uns32, that must for the first call be
 * initialized with 0. last must be set to true only for the last
 * chunk to be encoded  */
Int32
base64_encode(UChar * source, UChar ** target, Int32 num, Flag last, Uns32 * state)
{
  UChar		*result = NULL, *cptr, *endptr;
  Int32		i, tp = 0, r = 0, bp, c;

  if(!target){
    errno = EINVAL;
    return(-2);
  }

  endptr = source + (num < 0 ? strlen(source) : num);

  result = *target;
  if(!result){
    result = NEWP(UChar, (endptr - source) * 8 / 6 + 4);
    if(! result)
	return(-1);
    *target = result;
  }
  *result = '\0';

  if(state){
    c = (*state >> 8);
    bp = (*state & 0xff);
  }
  else{
    c = 0;
    bp = 0;
  }

  endptr = source + (num < 0 ? strlen(source) : num);

  while(source < endptr){
    c = (c << 8) | *source;
    bp += 8;
    while(bp >= 6){
	bp -= 6;
	*(result++) = base64table[c >> bp];
	*result = '\0';
	r++;
	c &= (0xffff >> (16 - bp));
    }

    source++;
  }

  if(bp > 0 && last){
    *(result++) = base64table[c << (6 - bp)];
    *(result++) = base64table[64];
    *result = '\0';
    r += 2;
  }

 cleanup:
  if(state)
    *state = bp | (c << 8);

  return(r);
}
