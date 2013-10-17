#include <tommath.h>
#ifdef BN_MP_SET_INT_C
/* LibTomMath, multiple-precision integer library -- Tom St Denis
 *
 * LibTomMath is a library that provides multiple-precision
 * integer arithmetic as well as number theoretic functionality.
 *
 * The library was designed directly after the MPI library by
 * Michael Fromberger but has been written from scratch with
 * additional optimizations in place.
 *
 * The library is free for all purposes without any express
 * guarantee it works.
 *
 * Tom St Denis, tomstdenis@gmail.com, http://libtom.org
 */

/* set a 32-bit const */
int mp_set_int (mp_int * a, unsigned long b)
{
  int     x, res;

  mp_zero (a);
  
  /* set four bits at a time */
  for (x = 0; x < 8; x++) {
    /* shift the number up four bits */
    if ((res = mp_mul_2d (a, 4, a)) != MP_OKAY) {
      return res;
    }

    /* OR in the top four bits of the source */
    a->dp[0] |= (b >> 28) & 15;

    /* shift the source up to the next four bits */
    b <<= 4;

    /* ensure that digits are not clamped off */
    a->used += 1;
  }
  mp_clamp (a);
  return MP_OKAY;
}

int mp_set_signed(mp_int * a, intptr_t b)
{
  int     x, res;

  /* keep track of sign */
  int sign = 0;
  if (b < 0) {
      sign = 1;
      b *= -1;
  }

  mp_zero (a);

  /* set four bits at a time */
  for (x = 0; x < (int)(sizeof(intptr_t)*2); x++) {
    /* shift the number up four bits */
    if ((res = mp_mul_2d (a, 4, a)) != MP_OKAY) {
      return res;
    }

    /* OR in the top four bits of the source */
    a->dp[0] |= (b >> (sizeof(b)*CHAR_BIT-4)) & 15;

    /* shift the source up to the next four bits */
    b <<= 4;

    /* ensure that digits are not clamped off */
    a->used += 1;
  }
  a->sign = sign; // set sign
  mp_clamp (a);
  return MP_OKAY;
}

#include <math.h>
int mp_set_double(mp_int * a, double b)
{
  /* keep track of sign */
  int sign = 0;
  if (b < 0) {
      sign = 1;
      b *= -1;
  }

  b = round(b);

  if (b < 1) {
      mp_zero (a);
      return MP_OKAY;
  }

  // TODO actually take the bits and then shift it by exponent
  mp_set_signed (a, (intptr_t)b);
  a->sign = sign; // set sign
  return MP_OKAY;
}
#endif

/* $Source$ */
/* $Revision: 0.41 $ */
/* $Date: 2007-04-18 09:58:18 +0000 $ */
