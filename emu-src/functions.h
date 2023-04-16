#ifndef FUNCTIONS_H
#define FUNCTIONS_H
#include <gmp.h>

void s_tail(mpz_t& dst, mpz_t& src, unsigned long n);
void s_cat(mpz_t& dst, mpz_t& src1, mpz_t& src2);
void s_asSInt(mpz_t& dst, mpz_t& src);
void s_bits(mpz_t& dst, mpz_t& src, mp_bitcnt_t l, mp_bitcnt_t r);
void s_pad(mpz_t& dst, mpz_t& src, mp_bitcnt_t n);
#endif
