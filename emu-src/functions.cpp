#include <gmp.h>
#include <assert.h>
#include <iostream>
#if 0
static mpz_t t1;
static mpz_t t2;

// u_tail: remain the last n bits
void u_tail(mpz_t& dst, mpz_t& src, mp_bitcnt_t bitcnt, unsigned long n) {
  if(mpz_sgn(src) == 0) {
    mpz_set(dst, src);
    return;
  }
  mpz_set_ui(dst, 1);
  mpz_mul_2exp(dst, dst, n);
  mpz_sub_ui(dst, dst, 1);
  if(mpz_sgn(src) < 0) {
    mpz_set_ui(t1, 1);
    mpz_mul_2exp(t1, t1, bitcnt);
    mpz_add(t1, t1, src);
    mpz_and(dst, dst, t1);
  } else {
    mpz_and(dst, dst, src);
  }
}
unsigned long ui_tail(mpz_t& src, mp_bitcnt_t bitcnt, unsigned long n) {
  u_tail(t2, src, bitcnt, n);
  return mpz_get_ui(t2);
}
__uint128_t ui_tail128(mpz_t& src, mp_bitcnt_t bitcnt, unsigned long n) {
  u_tail(t2, src, bitcnt, n);
  if (mpz_size(t2) == 1) return mpz_get_ui(t2);
  else return (__uint128_t)mpz_getlimbn(src, 1) << 64 | mpz_get_ui(src);
}
// u_head: remove the last n bits
void u_head(mpz_t& dst, mpz_t& src, mp_bitcnt_t bitcnt, unsigned long n) {
  mpz_tdiv_q_2exp(dst, src, n);
}
// u_cat: concat src1 and src2
void u_cat(mpz_t& dst, mpz_t& src1, mp_bitcnt_t bitcnt1, mpz_t& src2, mp_bitcnt_t bitcnt2) {
  mpz_mul_2exp(dst, src1, bitcnt2);
  mpz_ior(dst, dst, src2); 
}
void u_cat_ui_r(mpz_t& dst, mpz_t& src, unsigned long val, mp_bitcnt_t bitcnt) {
  mpz_mul_2exp(dst, src, bitcnt);
  mpz_add_ui(dst, dst, val);
}
void u_cat_ui_r128(mpz_t& dst, mpz_t& src, __uint128_t val, mp_bitcnt_t bitcnt) {
  mpz_mul_2exp(dst, src, bitcnt - 64);
  mpz_add_ui(dst, dst, val >> 64);
  mpz_mul_2exp(dst, src, 64);
  mpz_add_ui(dst, dst, val);
}
void u_cat_ui_l(mpz_t& dst, unsigned long val, mp_bitcnt_t bitcnt1, mpz_t& src, mp_bitcnt_t bitcnt2) {
  mpz_set_ui(dst, val);
  mpz_mul_2exp(dst, dst, bitcnt2);
  mpz_add(dst, dst, src);
}
void u_cat_ui_l128(mpz_t& dst, __uint128_t val, mp_bitcnt_t bitcnt1, mpz_t& src, mp_bitcnt_t bitcnt2) {
  mpz_import(dst, 2, -1, 8, 0, 0, (mp_limb_t*)&val);
  mpz_mul_2exp(dst, dst, bitcnt2);
  mpz_add(dst, dst, src);
}
void u_cat_ui2(mpz_t& dst, unsigned long val1, mp_bitcnt_t bitcnt1, unsigned long val2, mp_bitcnt_t bitcnt2) {
  mpz_set_ui(dst, val1);
  mpz_mul_2exp(dst, dst, bitcnt2);
  mpz_add_ui(dst, dst, val2);
}
// s_asSInt
void s_asSInt(mpz_t& dst, mpz_t& src, mp_bitcnt_t bitcnt) {
  if(mpz_sgn(src) > 0 && mpz_tstbit(src, bitcnt - 1)) {
    mpz_set_ui(dst, 1);
    mpz_mul_2exp(dst, dst, bitcnt);
    mpz_sub(dst, dst, src);
    mpz_neg(dst, dst);
  } else {
    mpz_set(dst, src);
  }
}
void s_asSInt_ui(mpz_t& dst, unsigned long src, mp_bitcnt_t bitcnt) {
  if(src & (1 << (bitcnt - 1))) {
    mpz_set_ui(dst, (1 << bitcnt) - src);
    mpz_neg(dst, dst);
  } else {
    mpz_set_ui(dst, src);
  }
}
// u_asUInt
void u_asUInt(mpz_t& dst, mpz_t& src, mp_bitcnt_t bitcnt) {
  if(mpz_cmp_ui(src, 0) < 0) {
    mpz_set_ui(dst, 1);
    mpz_mul_2exp(dst, dst, bitcnt);
    mpz_add(dst, dst, src);
  } else {
    mpz_set(dst, src);
  }
}
// u_asClock
void u_asClock(mpz_t& dst, mpz_t& src, mp_bitcnt_t bitcnt) {
  mpz_set(dst, src);
}
// u_bits
void u_bits(mpz_t& dst, mpz_t& src, mp_bitcnt_t bitcnt, mp_bitcnt_t h, mp_bitcnt_t l) {

  if(mpz_sgn(src) < 0) {
    mpz_set_ui(dst, 1);
    mpz_mul_2exp(dst, dst, bitcnt);
    mpz_add(dst, dst, src);
    mpz_tdiv_q_2exp(dst, dst, l);
  } else {
    mpz_tdiv_q_2exp(dst, src, l);
  }
  mpz_set_ui(t1, 1);
  mpz_mul_2exp(t1, t1, h - l + 1);
  mpz_sub_ui(t1, t1, 1);
  mpz_and(dst, dst, t1);
}

__uint128_t u_bits_basic128(mpz_t& src, mp_bitcnt_t bitcnt, int h, int l) {
  int n = h - l + 1;
  mpz_tdiv_q_2exp(t1, src, l);
  mpz_set_ui(t2, 1);
  mpz_mul_2exp(t2, t2, n);
  mpz_sub_ui(t2, t2, 1);
  mpz_and(t1, t1, t2);
  if (n > 64) {
    return (__uint128_t)mpz_getlimbn(t1, 1) << 64 | mpz_get_ui(t1);
  } else {
    return mpz_get_ui(t1);
  }
}

unsigned long u_bits_basic(mpz_t& src, mp_bitcnt_t bitcnt, int h, int l) {
  if(mpz_sgn(src) < 0) {
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
  mpz_and(t1, t1, t2);
  return mpz_get_ui(t1);
}

// u_pat: sign/zero extends to n bits
void u_pad(mpz_t& dst, mpz_t& src, mp_bitcnt_t bitcnt, mp_bitcnt_t n) {
  mpz_set(dst, src);
}
void u_pad_ui(mpz_t& dst, unsigned long src, mp_bitcnt_t n) {
  mpz_set_ui(dst, src);
}
void s_pad(mpz_t& dst, mpz_t& src, mp_bitcnt_t bitcnt, mp_bitcnt_t n) {
  if(bitcnt >= n || (mpz_cmp_ui(src, 0) >= 0)) {
    mpz_set(dst, src);
    return;
  }
  mpz_set_ui(dst, 1);
  mpz_mul_2exp(dst, dst, n-bitcnt);
  mpz_sub_ui(dst, dst, 1);
  mpz_mul_2exp(dst, dst, bitcnt);
  mpz_ior(dst, dst, src);
}
//shl
void u_shl(mpz_t& dst, mpz_t& src, mp_bitcnt_t bitcnt, unsigned long n) {
  mpz_mul_2exp(dst, src, n);
}
void u_shl_ui(mpz_t& dst, unsigned long src, unsigned long n) {
  mpz_set_ui(dst, src);
  mpz_mul_2exp(dst, dst, n);
}
void u_shr(mpz_t& dst, mpz_t& src, mp_bitcnt_t bitcnt, unsigned long n) {
  mpz_tdiv_q_2exp(dst, src, n);
}
void s_shr(mpz_t& dst, mpz_t& src, mp_bitcnt_t bitcnt, unsigned long n) {
  if(mpz_cmp_ui(src, 0) < 0) {
    mpz_fdiv_q_2exp(dst, dst, n);
  } else {
    mpz_tdiv_q_2exp(dst, src, n);
  }
}
void u_shr_ui(mpz_t& dst, unsigned long src, unsigned long n) {
  mpz_set_ui(dst, src >> n);
}
//add
void u_add(mpz_t& dst, mpz_t& src1, mp_bitcnt_t bitcnt1, mpz_t& src2, mp_bitcnt_t bitcnt2) {
  mpz_add(dst, src1, src2);
}
void u_add_ui_l(mpz_t& dst, unsigned long val, mp_bitcnt_t bitcnt1, mpz_t& src, mp_bitcnt_t bitcnt2) {
  mpz_add_ui(dst, src, val);
}
void u_add_ui_r(mpz_t& dst, mpz_t& src, unsigned long val, mp_bitcnt_t bitcnt) {
  mpz_add_ui(dst, src, val);
}
void u_add_si_r(mpz_t& dst, mpz_t& src, long val, mp_bitcnt_t bitcnt) {
  if(val > 0)
    mpz_add_ui(dst, src, val);
  else
    mpz_sub_ui(dst, src, -val);
}
void u_add_ui2(mpz_t& dst, unsigned long val1, mp_bitcnt_t bitcnt1, unsigned long val2, mp_bitcnt_t bitcnt2) {
  mpz_set_ui(dst, val1);
  mpz_add_ui(dst, dst, val2);
}
//sub
void u_sub(mpz_t& dst, mpz_t& src1, mp_bitcnt_t bitcnt1, mpz_t& src2, mp_bitcnt_t bitcnt2) {
  if(mpz_sgn(src2) == 0) {
    mpz_set(dst, src1);
    return;
  }
  mpz_set_ui(dst, 1);
  mpz_mul_2exp(dst, dst, bitcnt2);
  mpz_sub(dst, dst, src2);
  mpz_add(dst, dst, src1);
}
void u_sub_ui_l(mpz_t& dst, unsigned long val, mp_bitcnt_t bitcnt1, mpz_t& src, mp_bitcnt_t bitcnt2) {
  mpz_set_ui(dst, val);
  mpz_sub(dst, dst, src);
}
void u_sub_ui_r(mpz_t& dst, mpz_t& src, unsigned long val, mp_bitcnt_t bitcnt) {
  mpz_sub_ui(dst, src, val);
}

void u_sub_ui_r128(mpz_t& dst, mpz_t& src, __uint128_t val, mp_bitcnt_t bitcnt) {
  mpz_import(t1, 2, -1, 8, 0, 0, (mp_limb_t*)&val);
  mpz_sub(dst, src, t1);
}

void u_sub_ui2(mpz_t& dst, unsigned long val1, mp_bitcnt_t bitcnt1, unsigned long val2, mp_bitcnt_t bitcnt2) {
  mpz_set_ui(dst, val1);
  mpz_sub_ui(dst, dst, val2);
}
//mul
void u_mul(mpz_t& dst, mpz_t& src1, mp_bitcnt_t bitcnt1, mpz_t& src2, mp_bitcnt_t bitcnt2) {
  mpz_mul(dst, src1, src2);
}
void u_mul_ui_l(mpz_t& dst, unsigned long val, mp_bitcnt_t bitcnt1, mpz_t& src, mp_bitcnt_t bitcnt2) {
  mpz_mul_ui(dst, src, val);
}
void u_mul_si_l(mpz_t& dst, long val, mp_bitcnt_t bitcnt1, mpz_t& src, mp_bitcnt_t bitcnt2) {
  mpz_mul_si(dst, src, val);
}
void u_mul_ui_r(mpz_t& dst, mpz_t& src, unsigned long val, mp_bitcnt_t bitcnt) {
  mpz_mul_ui(dst, src, val);
}
void u_mul_si_r(mpz_t& dst, mpz_t& src, long val, mp_bitcnt_t bitcnt) {
  mpz_mul_si(dst, src, val);
}

void u_mul_si_r128(mpz_t& dst, mpz_t& src, __int128_t val, mp_bitcnt_t bitcnt) {
  mpz_import(t1, 2, -1, 8, 0, 0, &val);
  mpz_mul(dst, src, t1);
}

void u_mul_ui2(mpz_t& dst, unsigned long val1, mp_bitcnt_t bitcnt1, unsigned long val2, mp_bitcnt_t bitcnt2) {
  mpz_set_ui(dst, val1);
  mpz_mul_ui(dst, dst, val2);
}
//div
void u_div(mpz_t& dst, mpz_t& src1, mp_bitcnt_t bitcnt1, mpz_t& src2, mp_bitcnt_t bitcnt2) {
  if(mpz_cmp_ui(src2, 0) == 0) return;
    mpz_tdiv_q(dst, src1, src2);
}
void u_div_ui_l(mpz_t& dst, unsigned long val, mp_bitcnt_t bitcnt1, mpz_t& src, mp_bitcnt_t bitcnt2) {
  if(mpz_cmp_ui(src, 0) == 0) return;
  mpz_set_ui(dst, val);
  mpz_tdiv_q(dst, dst, src);
}
void u_div_ui_r(mpz_t& dst, mpz_t& src, unsigned long val, mp_bitcnt_t bitcnt) {
  if(val == 0) return;
  mpz_tdiv_q_ui(dst, src, val);
}
void u_div_ui2(mpz_t& dst, unsigned long val1, mp_bitcnt_t bitcnt1, unsigned long val2, mp_bitcnt_t bitcnt2) {
  if(val2 == 0) return;
  mpz_set_ui(dst, val1 / val2);
}
//rem
void u_rem(mpz_t& dst, mpz_t& src1, mp_bitcnt_t bitcnt1, mpz_t& src2, mp_bitcnt_t bitcnt2) {
  if(mpz_cmp_ui(src2, 0) == 0) return;
  mpz_tdiv_r(dst, src1, src2);
}
void u_rem_ui_l(mpz_t& dst, unsigned long val, mp_bitcnt_t bitcnt1, mpz_t& src, mp_bitcnt_t bitcnt2) {
  if(mpz_cmp_ui(src, 0) == 0) return;
  mpz_set_ui(dst, val);
  mpz_tdiv_r(dst, dst, src);
}
void u_rem_ui_r(mpz_t& dst, mpz_t& src, unsigned long val, mp_bitcnt_t bitcnt) {
  if(val == 0) return;
  mpz_tdiv_r_ui(dst, src, val);
}
void u_rem_ui2(mpz_t& dst, unsigned long val1, mp_bitcnt_t bitcnt1, unsigned long val2, mp_bitcnt_t bitcnt2) {
  if(val2 == 0) return;
  mpz_set_ui(dst, val1 % val2);
}
//lt
void u_lt(mpz_t& dst, mpz_t& op1, mp_bitcnt_t bitcnt1, mpz_t& op2, mp_bitcnt_t bitcnt2) {
  mpz_set_ui(dst, mpz_cmp(op1, op2)<0);
}
void u_lt_ui_r(mpz_t& dst, mpz_t& src, unsigned long val, mp_bitcnt_t bitcnt) {
  mpz_set_ui(dst, mpz_cmp_ui(src, val)<0);
}
void u_lt_ui_l(mpz_t& dst, unsigned long val, mp_bitcnt_t bitcnt1, mpz_t& src, mp_bitcnt_t bitcnt2) {
  mpz_set_ui(dst, mpz_cmp_ui(src, val)>0);
}
void u_lt_ui2(mpz_t& dst, unsigned long val1, mp_bitcnt_t bitcnt1, unsigned long val2, mp_bitcnt_t bitcnt2) {
  mpz_set_ui(dst, val1 < val2);
}
//leq
void u_leq(mpz_t& dst, mpz_t& op1, mp_bitcnt_t bitcnt1, mpz_t& op2, mp_bitcnt_t bitcnt2) {
  mpz_set_ui(dst, mpz_cmp(op1, op2)<=0);
}
void u_leq_ui_r(mpz_t& dst, mpz_t& src, unsigned long val, mp_bitcnt_t bitcnt) {
  mpz_set_ui(dst, mpz_cmp_ui(src, val)<=0);
}
void u_leq_ui_l(mpz_t& dst, unsigned long val, mp_bitcnt_t bitcnt1, mpz_t& src, mp_bitcnt_t bitcnt2) {
  mpz_set_ui(dst, mpz_cmp_ui(src, val)>=0);
}
void u_leq_ui2(mpz_t& dst, unsigned long val1, mp_bitcnt_t bitcnt1, unsigned long val2, mp_bitcnt_t bitcnt2) {
  mpz_set_ui(dst, val1 <= val2);
}
//gt
void u_gt(mpz_t& dst, mpz_t& op1, mp_bitcnt_t bitcnt1, mpz_t& op2, mp_bitcnt_t bitcnt2) {
  mpz_set_ui(dst, mpz_cmp(op1, op2)>0);
}
void u_gt_ui_r(mpz_t& dst, mpz_t& src, unsigned long val, mp_bitcnt_t bitcnt) {
  mpz_set_ui(dst, mpz_cmp_ui(src, val)>0);
}
void u_gt_ui_l(mpz_t& dst, unsigned long val, mp_bitcnt_t bitcnt1, mpz_t& src, mp_bitcnt_t bitcnt2) {
  mpz_set_ui(dst, mpz_cmp_ui(src, val)<0);
}
void u_gt_ui2(mpz_t& dst, unsigned long val1, mp_bitcnt_t bitcnt1, unsigned long val2, mp_bitcnt_t bitcnt2) {
  mpz_set_ui(dst, val1 > val2);
}
//geq
void u_geq(mpz_t& dst, mpz_t& op1, mp_bitcnt_t bitcnt1, mpz_t& op2, mp_bitcnt_t bitcnt2) {
  mpz_set_ui(dst, mpz_cmp(op1, op2)>=0);
}
bool ui_geq(mpz_t& op1, mp_bitcnt_t bitcnt1, mpz_t& op2, mp_bitcnt_t bitcnt2) {
  return mpz_cmp(op1, op2)>=0;
}
void u_geq_ui_r(mpz_t& dst, mpz_t& src, unsigned long val, mp_bitcnt_t bitcnt) {
  mpz_set_ui(dst, mpz_cmp_ui(src, val)>=0);
}
void u_geq_ui_l(mpz_t& dst, unsigned long val, mp_bitcnt_t bitcnt1, mpz_t& src, mp_bitcnt_t bitcnt2) {
  mpz_set_ui(dst, mpz_cmp_ui(src, val)<=0);
}
void u_geq_ui2(mpz_t& dst, unsigned long val1, mp_bitcnt_t bitcnt1, unsigned long val2, mp_bitcnt_t bitcnt2) {
  mpz_set_ui(dst, val1 >= val2);
}
//eq
bool ui_eq(mpz_t& op1, mp_bitcnt_t bitcnt1, mpz_t& op2, mp_bitcnt_t bitcnt2) {
  return mpz_cmp(op1, op2)==0;
}
void u_eq(mpz_t& dst, mpz_t& op1, mp_bitcnt_t bitcnt1, mpz_t& op2, mp_bitcnt_t bitcnt2) {
  mpz_set_ui(dst, mpz_cmp(op1, op2)==0);
}
void u_eq_ui_r(mpz_t& dst, mpz_t& src, unsigned long val, mp_bitcnt_t bitcnt) {
  mpz_set_ui(dst, mpz_cmp_ui(src, val)==0);
}
void u_eq_ui_l(mpz_t& dst, unsigned long val, mp_bitcnt_t bitcnt1, mpz_t& src, mp_bitcnt_t bitcnt2) {
  mpz_set_ui(dst, mpz_cmp_ui(src, val)==0);
}
void u_eq_ui2(mpz_t& dst, unsigned long val1, mp_bitcnt_t bitcnt1, unsigned long val2, mp_bitcnt_t bitcnt2) {
  mpz_set_ui(dst, val1 == val2);
}
//neq
bool ui_neq(mpz_t& op1, mp_bitcnt_t bitcnt1, mpz_t& op2, mp_bitcnt_t bitcnt2) {
  return mpz_cmp(op1, op2)!=0;
}
void u_neq(mpz_t& dst, mpz_t& op1, mp_bitcnt_t bitcnt1, mpz_t& op2, mp_bitcnt_t bitcnt2) {
  mpz_set_ui(dst, mpz_cmp(op1, op2)!=0);
}
void u_neq_ui_r(mpz_t& dst, mpz_t& src, unsigned long val, mp_bitcnt_t bitcnt) {
  mpz_set_ui(dst, mpz_cmp_ui(src, val)!=0);
}
void u_neq_ui_l(mpz_t& dst, unsigned long val, mp_bitcnt_t bitcnt1, mpz_t& src, mp_bitcnt_t bitcnt2) {
  mpz_set_ui(dst, mpz_cmp_ui(src, val)!=0);
}
void u_neq_ui2(mpz_t& dst, unsigned long val1, mp_bitcnt_t bitcnt1, unsigned long val2, mp_bitcnt_t bitcnt2) {
  mpz_set_ui(dst, val1 != val2);
}
//and
void u_and(mpz_t& dst, mpz_t& src1, mp_bitcnt_t bitcnt1, mpz_t& src2, mp_bitcnt_t bitcnt2) {
  mpz_and(dst, src1, src2);
}
void u_and_ui_r(mpz_t& dst, mpz_t& src, unsigned long val, mp_bitcnt_t bitcnt) {
  mpz_set_ui(dst, val);
  mpz_and(dst, dst, src);
}
void u_and_ui_l(mpz_t& dst, unsigned long val, mp_bitcnt_t bitcnt1, mpz_t& src, mp_bitcnt_t bitcnt2) {
  mpz_set_ui(dst, val);
  mpz_and(dst, dst, src);
}
void u_and_ui2(mpz_t& dst, unsigned long val1, mp_bitcnt_t bitcnt1, unsigned long val2, mp_bitcnt_t bitcnt2) {
  mpz_set_ui(dst, val1 & val2);
}
//ior
void u_or(mpz_t& dst, mpz_t& src1, mp_bitcnt_t bitcnt1, mpz_t& src2, mp_bitcnt_t bitcnt2) {
  mpz_ior(dst, src1, src2);
}
void u_or_ui_r(mpz_t& dst, mpz_t& src, unsigned long val, mp_bitcnt_t bitcnt) {
  mpz_set_ui(dst, val);
  mpz_ior(dst, dst, src);
}
void u_or_ui_l(mpz_t& dst, unsigned long val, mp_bitcnt_t bitcnt1, mpz_t& src, mp_bitcnt_t bitcnt2) {
  mpz_set_ui(dst, val);
  mpz_ior(dst, dst, src);
}
void u_or_ui2(mpz_t& dst, unsigned long val1, mp_bitcnt_t bitcnt1, unsigned long val2, mp_bitcnt_t bitcnt2) {
  mpz_set_ui(dst, val1 | val2);
}
//xor
void u_xor(mpz_t& dst, mpz_t& src1, mp_bitcnt_t bitcnt1, mpz_t& src2, mp_bitcnt_t bitcnt2) {
  mpz_xor(dst, src1, src2);
}
void u_xor_ui_r(mpz_t& dst, mpz_t& src, unsigned long val, mp_bitcnt_t bitcnt) {
  mpz_set_ui(dst, val);
  mpz_xor(dst, dst, src);
}
void u_xor_ui_l(mpz_t& dst, unsigned long val, mp_bitcnt_t bitcnt1, mpz_t& src, mp_bitcnt_t bitcnt2) {
  mpz_set_ui(dst, val);
  mpz_xor(dst, dst, src);
}
void u_xor_ui2(mpz_t& dst, unsigned long val1, mp_bitcnt_t bitcnt1, unsigned long val2, mp_bitcnt_t bitcnt2) {
  mpz_set_ui(dst, val1 ^ val2);
}
//dshl
void u_dshl(mpz_t& dst, mpz_t& src1, mp_bitcnt_t bitcnt1, mpz_t& src2, mp_bitcnt_t bitcnt2) {
  mpz_mul_2exp(dst, src1, mpz_get_ui(src2));
}
void u_dshl_ui_r(mpz_t& dst, mpz_t& src, unsigned long val, mp_bitcnt_t bitcnt) {
  mpz_mul_2exp(dst, src, val);
}
void u_dshl_ui_l(mpz_t& dst, unsigned long val, mp_bitcnt_t bitcnt1, mpz_t& src, mp_bitcnt_t bitcnt2) {
  mpz_set_ui(dst, val);
  mpz_mul_2exp(dst, dst, mpz_get_ui(src));
}
void u_dshl_ui2(mpz_t& dst, unsigned long val1, mp_bitcnt_t bitcnt1, unsigned long val2, mp_bitcnt_t bitcnt2) {
  mpz_set_ui(dst, val1 << val2);
}
//dshr
void u_dshr(mpz_t& dst, mpz_t& src1, mp_bitcnt_t bitcnt1, mpz_t& src2, mp_bitcnt_t bitcnt2) {
  mpz_tdiv_q_2exp(dst, src1, mpz_get_ui(src2));
}
void s_dshr(mpz_t& dst, mpz_t& src1, mp_bitcnt_t bitcnt1, mpz_t& src2, mp_bitcnt_t bitcnt2) {
  int n = mpz_get_ui(src2);
  if(mpz_cmp_ui(src1, 0) < 0) {
    mpz_fdiv_q_2exp(dst, src1, n);
  } else {
    mpz_tdiv_q_2exp(dst, src1, n);
  }
}
void u_dshr_ui_r(mpz_t& dst, mpz_t& src, unsigned long val, mp_bitcnt_t bitcnt) {
  mpz_tdiv_q_2exp(dst, src, val);
}
void u_dshr_ui_l(mpz_t& dst, unsigned long val, mp_bitcnt_t bitcnt1, mpz_t& src, mp_bitcnt_t bitcnt2) {
  mpz_set_ui(dst, val >> mpz_get_ui(src));
}
void u_dshr_ui2(mpz_t& dst, unsigned long val1, mp_bitcnt_t bitcnt1, unsigned long val2, mp_bitcnt_t bitcnt2) {
  mpz_set_ui(dst, val1 >> val2);
}
//orr
void u_orr(mpz_t& dst, mpz_t& src, mp_bitcnt_t bitcnt) {
  mpz_set_ui(dst, mpz_cmp_ui(src, 0) == 0 ? 0 : 1);
}
void u_andr(mpz_t& dst, mpz_t& src, mp_bitcnt_t bitcnt) {
  mpz_set_ui(dst, 1);
  mpz_mul_2exp(dst, dst, bitcnt);
  mpz_sub_ui(dst, dst, 1);
  mpz_set_ui(dst, mpz_cmp(dst, src) == 0);
}
void u_xorr(mpz_t& dst, mpz_t& src, mp_bitcnt_t bitcnt) {
  mpz_set_ui(dst, mpz_popcount(src) & 1); // not work for negtive src
}
void u_not(mpz_t& dst, mpz_t& src, mp_bitcnt_t bitcnt) {
  mpz_set_ui(dst, 1);
  mpz_mul_2exp(dst, dst, bitcnt);
  mpz_sub_ui(dst, dst, 1);
  mpz_xor(dst, dst, src);
}
void u_not_ui(mpz_t& dst, unsigned long src, mp_bitcnt_t bitcnt) {
  unsigned long mask = ((unsigned long)1 << bitcnt) - 1;
  mpz_set_ui(dst, (~src) & mask);
}
void u_cvt(mpz_t& dst, mpz_t& src, mp_bitcnt_t bitcnt) {
  mpz_set(dst, src);
}
void s_cvt(mpz_t& dst, mpz_t& src, mp_bitcnt_t bitcnt) {
  mpz_set(dst, src);
}
void s_cvt_ui(mpz_t&dst, unsigned long src, mp_bitcnt_t bitcnt) {
  mpz_set_ui(dst, src);
}
void u_neg(mpz_t& dst, mpz_t& src, mp_bitcnt_t bitcnt) {
  mpz_set_ui(dst, 0);
  mpz_sub(dst, dst, src);
}

void init_functions() {
  mpz_init(t1);
  mpz_init(t2);
}
#endif