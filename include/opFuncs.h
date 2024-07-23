/**
 * @file opFuncs.h
 * @brief operation functions
 */

#ifndef OPFUNCS_H
#define OPFUNCS_H

#include <gmp.h>

void invalidExpr1Int1(mpz_t& dst, mpz_t& src, mp_bitcnt_t bitcnt, mp_bitcnt_t n);
void u_pad(mpz_t& dst, mpz_t& src, mp_bitcnt_t bitcnt, mp_bitcnt_t n);
void s_pad(mpz_t& dst, mpz_t& src, mp_bitcnt_t bitcnt, mp_bitcnt_t n);
void u_shl(mpz_t& dst, mpz_t& src, mp_bitcnt_t bitcnt, unsigned long n);
void u_shr(mpz_t& dst, mpz_t& src, mp_bitcnt_t bitcnt, unsigned long n);
void s_shr(mpz_t& dst, mpz_t& src, mp_bitcnt_t bitcnt, unsigned long n);
void u_head(mpz_t& dst, mpz_t& src, mp_bitcnt_t bitcnt, unsigned long n);
void u_tail(mpz_t& dst, mpz_t& src, mp_bitcnt_t bitcnt, unsigned long n);

void invalidExpr1(mpz_t& dst, mpz_t& src, mp_bitcnt_t bitcnt);
void u_asUInt(mpz_t& dst, mpz_t& src, mp_bitcnt_t bitcnt);
void s_asSInt(mpz_t& dst, mpz_t& src, mp_bitcnt_t bitcnt);
void u_asClock(mpz_t& dst, mpz_t& src, mp_bitcnt_t bitcnt);
void u_asAsyncReset(mpz_t& dst, mpz_t& src, mp_bitcnt_t bitcnt);
void u_cvt(mpz_t& dst, mpz_t& src, mp_bitcnt_t bitcnt);
void s_cvt(mpz_t& dst, mpz_t& src, mp_bitcnt_t bitcnt);
void s_neg(mpz_t& dst, mpz_t& src, mp_bitcnt_t bitcnt);
void u_not(mpz_t& dst, mpz_t& src, mp_bitcnt_t bitcnt);
void u_orr(mpz_t& dst, mpz_t& src, mp_bitcnt_t bitcnt);
void u_andr(mpz_t& dst, mpz_t& src, mp_bitcnt_t bitcnt);
void u_xorr(mpz_t& dst, mpz_t& src, mp_bitcnt_t bitcnt);

void invalidExpr2(mpz_t& dst, mpz_t& src1, mp_bitcnt_t bitcnt1, mpz_t& src2, mp_bitcnt_t bitcnt2);
void us_add(mpz_t& dst, mpz_t& src1, mp_bitcnt_t bitcnt1, mpz_t& src2, mp_bitcnt_t bitcnt2);
void us_sub(mpz_t& dst, mpz_t& src1, mpz_t& src2, mp_bitcnt_t dst_bitcnt);
void us_mul(mpz_t& dst, mpz_t& src1, mp_bitcnt_t bitcnt1, mpz_t& src2, mp_bitcnt_t bitcnt2);
void us_div(mpz_t& dst, mpz_t& src1, mp_bitcnt_t bitcnt1, mpz_t& src2, mp_bitcnt_t bitcnt2);
void us_rem(mpz_t& dst, mpz_t& src1, mp_bitcnt_t bitcnt1, mpz_t& src2, mp_bitcnt_t bitcnt2);
void us_lt(mpz_t& dst, mpz_t& op1, mp_bitcnt_t bitcnt1, mpz_t& op2, mp_bitcnt_t bitcnt2);
void us_leq(mpz_t& dst, mpz_t& op1, mp_bitcnt_t bitcnt1, mpz_t& op2, mp_bitcnt_t bitcnt2);
void us_gt(mpz_t& dst, mpz_t& op1, mp_bitcnt_t bitcnt1, mpz_t& op2, mp_bitcnt_t bitcnt2);
void us_geq(mpz_t& dst, mpz_t& op1, mp_bitcnt_t bitcnt1, mpz_t& op2, mp_bitcnt_t bitcnt2);
void us_eq(mpz_t& dst, mpz_t& op1, mp_bitcnt_t bitcnt1, mpz_t& op2, mp_bitcnt_t bitcnt2);
void us_neq(mpz_t& dst, mpz_t& op1, mp_bitcnt_t bitcnt1, mpz_t& op2, mp_bitcnt_t bitcnt2);
void u_dshr(mpz_t& dst, mpz_t& src1, mp_bitcnt_t bitcnt1, mpz_t& src2, mp_bitcnt_t bitcnt2);
void u_dshl(mpz_t& dst, mpz_t& src1, mp_bitcnt_t bitcnt1, mpz_t& src2, mp_bitcnt_t bitcnt2);
void s_dshr(mpz_t& dst, mpz_t& src1, mp_bitcnt_t bitcnt1, mpz_t& src2, mp_bitcnt_t bitcnt2);
void u_and(mpz_t& dst, mpz_t& src1, mp_bitcnt_t bitcnt1, mpz_t& src2, mp_bitcnt_t bitcnt2);
void u_ior(mpz_t& dst, mpz_t& src1, mp_bitcnt_t bitcnt1, mpz_t& src2, mp_bitcnt_t bitcnt2);
void u_xor(mpz_t& dst, mpz_t& src1, mp_bitcnt_t bitcnt1, mpz_t& src2, mp_bitcnt_t bitcnt2);
void u_cat(mpz_t& dst, mpz_t& src1, mp_bitcnt_t bitcnt1, mpz_t& src2, mp_bitcnt_t bitcnt2);

void invalidExpr1Int2(mpz_t& dst, mpz_t& src, mp_bitcnt_t bitcnt, mp_bitcnt_t h, mp_bitcnt_t l);
void u_bits(mpz_t& dst, mpz_t& src, mp_bitcnt_t bitcnt, mp_bitcnt_t h, mp_bitcnt_t l);
void u_bits_noshift(mpz_t& dst, mpz_t& src, mp_bitcnt_t bitcnt, mp_bitcnt_t h, mp_bitcnt_t l);

void us_mux(mpz_t& dst, mpz_t& cond, mpz_t& src1, mpz_t& src2);

#endif
