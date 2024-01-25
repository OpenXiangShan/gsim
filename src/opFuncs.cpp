/**
 * @file opFuncs.cpp
 * @brief operation functions
 */

#include <gmp.h>
#include <assert.h>
#include <iostream>

#include "common.h"

static mpz_t t1;
static mpz_t t2;
// expr1int1
void invalidExpr1Int1(mpz_t& dst, mpz_t& src, mp_bitcnt_t bitcnt, mp_bitcnt_t n) {
  Assert(0, "Invalid Expr1Int1 function\n");
}
void u_pad(mpz_t& dst, mpz_t& src, mp_bitcnt_t bitcnt, mp_bitcnt_t n) { mpz_set(dst, src); }
void s_pad(mpz_t& dst, mpz_t& src, mp_bitcnt_t bitcnt, mp_bitcnt_t n) {
  if (bitcnt >= n || (mpz_cmp_ui(src, 0) >= 0)) {
    mpz_set(dst, src);
    return;
  }
  mpz_set_ui(t1, 1);
  mpz_mul_2exp(t1, t1, n - bitcnt);
  mpz_sub_ui(t1, t1, 1);
  mpz_mul_2exp(t1, t1, bitcnt);
  mpz_ior(dst, t1, src);
}

void u_shl(mpz_t& dst, mpz_t& src, mp_bitcnt_t bitcnt, unsigned long n) { mpz_mul_2exp(dst, src, n); }

void u_shr(mpz_t& dst, mpz_t& src, mp_bitcnt_t bitcnt, unsigned long n) { mpz_tdiv_q_2exp(dst, src, n); }
void s_shr(mpz_t& dst, mpz_t& src, mp_bitcnt_t bitcnt, unsigned long n) {
  if (mpz_cmp_ui(src, 0) < 0) {
    mpz_fdiv_q_2exp(dst, dst, n);
  } else {
    mpz_tdiv_q_2exp(dst, src, n);
  }
}
// u_head: remove the last n bits
void u_head(mpz_t& dst, mpz_t& src, mp_bitcnt_t bitcnt, unsigned long n) { mpz_tdiv_q_2exp(dst, src, n); }
// u_tail: remain the last n bits
void u_tail(mpz_t& dst, mpz_t& src, mp_bitcnt_t bitcnt, unsigned long n) {
  if (mpz_sgn(src) == 0) {
    mpz_set(dst, src);
    return;
  }
  mpz_set_ui(t1, 1);
  mpz_mul_2exp(t1, t1, n);
  mpz_sub_ui(t1, t1, 1);
  if (mpz_sgn(src) < 0) {
    mpz_set_ui(t2, 1);
    mpz_mul_2exp(t2, t2, bitcnt);
    mpz_add(t2, t2, src);
    mpz_and(dst, t1, t2);
  } else {
    mpz_and(dst, t1, src);
  }
}
// expr1
void invalidExpr1(mpz_t& dst, mpz_t& src, mp_bitcnt_t bitcnt) { Assert(0, "Invalid Expr1 function\n"); }
void u_asUInt(mpz_t& dst, mpz_t& src, mp_bitcnt_t bitcnt) {
  if (mpz_cmp_ui(src, 0) < 0) {
    mpz_set_ui(t1, 1);
    mpz_mul_2exp(t1, t1, bitcnt);
    mpz_add(dst, t1, src);
  } else {
    mpz_set(dst, src);
  }
}

void s_asSInt(mpz_t& dst, mpz_t& src, mp_bitcnt_t bitcnt) {
  if (mpz_sgn(src) > 0 && mpz_tstbit(src, bitcnt - 1)) {
    mpz_set_ui(t1, 1);
    mpz_mul_2exp(t1, t1, bitcnt);
    mpz_sub(t1, t1, src);
    mpz_neg(dst, t1);
  } else {
    mpz_set(dst, src);
  }
}

void u_asClock(mpz_t& dst, mpz_t& src, mp_bitcnt_t bitcnt) { mpz_set(dst, src); }
void u_asAsyncReset(mpz_t& dst, mpz_t& src, mp_bitcnt_t bitcnt) { mpz_set(dst, src); }

void u_cvt(mpz_t& dst, mpz_t& src, mp_bitcnt_t bitcnt) { mpz_set(dst, src); }
void s_cvt(mpz_t& dst, mpz_t& src, mp_bitcnt_t bitcnt) { mpz_set(dst, src); }
void s_neg(mpz_t& dst, mpz_t& src, mp_bitcnt_t bitcnt) {
  mpz_set_ui(t1, 0);
  mpz_sub(dst, t1, src);
}
void u_not(mpz_t& dst, mpz_t& src, mp_bitcnt_t bitcnt) {
  mpz_set_ui(t1, 1);
  mpz_mul_2exp(t1, t1, bitcnt);
  mpz_sub_ui(t1, t1, 1);
  mpz_xor(dst, t1, src);
}
void u_orr(mpz_t& dst, mpz_t& src, mp_bitcnt_t bitcnt) { mpz_set_ui(dst, mpz_cmp_ui(src, 0) == 0 ? 0 : 1); }
void u_andr(mpz_t& dst, mpz_t& src, mp_bitcnt_t bitcnt) {
  mpz_set_ui(t1, 1);
  mpz_mul_2exp(t1, t1, bitcnt);
  mpz_sub_ui(t1, t1, 1);
  mpz_set_ui(dst, mpz_cmp(dst, src) == 0);
}
void u_xorr(mpz_t& dst, mpz_t& src, mp_bitcnt_t bitcnt) {
  mpz_set_ui(dst, mpz_popcount(src) & 1);  // not work for negtive src
}

void invalidExpr2(mpz_t& dst, mpz_t& src1, mp_bitcnt_t bitcnt1, mpz_t& src2, mp_bitcnt_t bitcnt2) {
  Assert(0, "Invalid Expr2 function\n");
}

void us_add(mpz_t& dst, mpz_t& src1, mp_bitcnt_t bitcnt1, mpz_t& src2, mp_bitcnt_t bitcnt2) {
  mpz_add(dst, src1, src2);
}
void us_sub(mpz_t& dst, mpz_t& src1, mp_bitcnt_t bitcnt1, mpz_t& src2, mp_bitcnt_t bitcnt2) {
  if (mpz_sgn(src2) == 0) {
    mpz_set(dst, src1);
    return;
  }
  mpz_set_ui(t1, 1);
  mpz_mul_2exp(t1, t1, bitcnt2);
  mpz_sub(t1, t1, src2);
  mpz_add(dst, t1, src1);
}
void us_mul(mpz_t& dst, mpz_t& src1, mp_bitcnt_t bitcnt1, mpz_t& src2, mp_bitcnt_t bitcnt2) {
  mpz_mul(dst, src1, src2);
}
void us_div(mpz_t& dst, mpz_t& src1, mp_bitcnt_t bitcnt1, mpz_t& src2, mp_bitcnt_t bitcnt2) {
  if (mpz_cmp_ui(src2, 0) == 0) return;
  mpz_tdiv_q(dst, src1, src2);
}
void us_rem(mpz_t& dst, mpz_t& src1, mp_bitcnt_t bitcnt1, mpz_t& src2, mp_bitcnt_t bitcnt2) {
  if (mpz_cmp_ui(src2, 0) == 0) return;
  mpz_tdiv_r(dst, src1, src2);
}
void us_lt(mpz_t& dst, mpz_t& op1, mp_bitcnt_t bitcnt1, mpz_t& op2, mp_bitcnt_t bitcnt2) {
  mpz_set_ui(dst, mpz_cmp(op1, op2) < 0);
}
void us_leq(mpz_t& dst, mpz_t& op1, mp_bitcnt_t bitcnt1, mpz_t& op2, mp_bitcnt_t bitcnt2) {
  mpz_set_ui(dst, mpz_cmp(op1, op2) <= 0);
}
void us_gt(mpz_t& dst, mpz_t& op1, mp_bitcnt_t bitcnt1, mpz_t& op2, mp_bitcnt_t bitcnt2) {
  mpz_set_ui(dst, mpz_cmp(op1, op2) > 0);
}
void us_geq(mpz_t& dst, mpz_t& op1, mp_bitcnt_t bitcnt1, mpz_t& op2, mp_bitcnt_t bitcnt2) {
  mpz_set_ui(dst, mpz_cmp(op1, op2) >= 0);
}
void us_eq(mpz_t& dst, mpz_t& op1, mp_bitcnt_t bitcnt1, mpz_t& op2, mp_bitcnt_t bitcnt2) {
  mpz_set_ui(dst, mpz_cmp(op1, op2) == 0);
}
void us_neq(mpz_t& dst, mpz_t& op1, mp_bitcnt_t bitcnt1, mpz_t& op2, mp_bitcnt_t bitcnt2) {
  mpz_set_ui(dst, mpz_cmp(op1, op2) != 0);
}
void u_dshl(mpz_t& dst, mpz_t& src1, mp_bitcnt_t bitcnt1, mpz_t& src2, mp_bitcnt_t bitcnt2) {
  mpz_mul_2exp(dst, src1, mpz_get_ui(src2));
}
void u_dshr(mpz_t& dst, mpz_t& src1, mp_bitcnt_t bitcnt1, mpz_t& src2, mp_bitcnt_t bitcnt2) {
  mpz_tdiv_q_2exp(dst, src1, mpz_get_ui(src2));
}
void s_dshr(mpz_t& dst, mpz_t& src1, mp_bitcnt_t bitcnt1, mpz_t& src2, mp_bitcnt_t bitcnt2) {
  int n = mpz_get_ui(src2);
  if (mpz_cmp_ui(src1, 0) < 0) {
    mpz_fdiv_q_2exp(dst, src1, n);
  } else {
    mpz_tdiv_q_2exp(dst, src1, n);
  }
}
void u_and(mpz_t& dst, mpz_t& src1, mp_bitcnt_t bitcnt1, mpz_t& src2, mp_bitcnt_t bitcnt2) { mpz_and(dst, src1, src2); }
void u_ior(mpz_t& dst, mpz_t& src1, mp_bitcnt_t bitcnt1, mpz_t& src2, mp_bitcnt_t bitcnt2) { mpz_ior(dst, src1, src2); }
void u_xor(mpz_t& dst, mpz_t& src1, mp_bitcnt_t bitcnt1, mpz_t& src2, mp_bitcnt_t bitcnt2) { mpz_xor(dst, src1, src2); }
void u_cat(mpz_t& dst, mpz_t& src1, mp_bitcnt_t bitcnt1, mpz_t& src2, mp_bitcnt_t bitcnt2) {
  Assert(mpz_sgn(src1) >= 0 && mpz_sgn(src2) >= 0, "src1 / src2 < 0\n");
  mpz_mul_2exp(t1, src1, bitcnt2);
  mpz_ior(dst, t1, src2);
}

void invalidExpr1Int2(mpz_t& dst, mpz_t& src, mp_bitcnt_t bitcnt, mp_bitcnt_t h, mp_bitcnt_t l) {
  Assert(0, "Invalid Expr1Int2 function\n");
}

void u_bits(mpz_t& dst, mpz_t& src, mp_bitcnt_t bitcnt, mp_bitcnt_t h, mp_bitcnt_t l) {
  if (mpz_sgn(src) < 0) {
    mpz_set_ui(t1, 1);
    mpz_mul_2exp(t1, t1, bitcnt);
    mpz_add(t1, t1, src);
    mpz_tdiv_q_2exp(t1, t1, l);
  } else {
    mpz_tdiv_q_2exp(t1, src, l);
  }
  mpz_set_ui(t2, 1);
  mpz_mul_2exp(t2, t2, h - l + 1);
  mpz_sub_ui(t2, t2, 1);
  mpz_and(dst, t1, t2);
}

void us_mux(mpz_t& dst, mpz_t& cond, mpz_t& src1, mpz_t& src2) {
  if (mpz_sgn(cond) != 0)
    mpz_set(dst, src1);
  else
    mpz_set(dst, src2);
}
