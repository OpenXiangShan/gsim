#include <gmp.h>
#include <assert.h>
#include <iostream>

static mpz_t t1;

// u_tail: remain the last n bits
void u_tail(mpz_t& dst, mpz_t& src, mp_bitcnt_t bitcnt, unsigned long n) {
  if(mpz_size(dst) == 0) {
    mpz_set(dst, src);
    return;
  }
  mpz_set_ui(dst, 1);
  mpz_mul_2exp(dst, dst, n);
  mpz_sub_ui(dst, dst, 1);
  if(mpz_sgn(src)) {
    mpz_set_ui(t1, 1);
    mpz_mul_2exp(t1, t1, bitcnt);
    mpz_add(t1, t1, src);
    mpz_and(dst, dst, t1);
  } else {
    mpz_and(dst, dst, src);
  }
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
void u_cat_ui_l(mpz_t& dst, unsigned long val, mp_bitcnt_t bitcnt1, mpz_t& src, mp_bitcnt_t bitcnt2) {
  mpz_set_ui(dst, val);
  mpz_mul_2exp(dst, dst, bitcnt2);
  mpz_add_ui(dst, dst, val);
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
void u_bits(mpz_t& dst, mpz_t& src, mp_bitcnt_t h, mp_bitcnt_t l) {
  if(mpz_size(src) == 0) {
    mpz_set(dst, src);
    return;
  }
  mp_bitcnt_t left_bits = h - l + 1;
  mpz_tdiv_q_2exp(dst, src, l);
  int libms_num = (left_bits + 63) / 64;
  int trailing_bits = left_bits % 64;
  if(trailing_bits) {
    unsigned long int mask = ((unsigned long)1 << trailing_bits) - 1;
    mp_limb_t* data = mpz_limbs_modify(dst, libms_num);
    *data = *data & mask;
    mpz_limbs_finish(dst, libms_num);
  }
}
// u_pat: sign/zero extends to n bits
void u_pad(mpz_t& dst, mpz_t& src, mp_bitcnt_t bitcnt, mp_bitcnt_t n) {
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
void u_mpz_add(mpz_t& dst, mpz_t& src1, mp_bitcnt_t bitcnt1, mpz_t& src2, mp_bitcnt_t bitcnt2) {
  mpz_add(dst, src1, src2);
}
void u_mpz_add_ui_l(mpz_t& dst, unsigned long val, mp_bitcnt_t bitcnt1, mpz_t& src, mp_bitcnt_t bitcnt2) {
  mpz_add_ui(dst, src, val);
}
void u_mpz_add_ui_r(mpz_t& dst, mpz_t& src, unsigned long val, mp_bitcnt_t bitcnt) {
  mpz_add_ui(dst, src, val);
}
void u_mpz_add_ui2(mpz_t& dst, unsigned long val1, mp_bitcnt_t bitcnt1, unsigned long val2, mp_bitcnt_t bitcnt2) {
  mpz_set_ui(dst, val1);
  mpz_add_ui(dst, dst, val2);
}
//sub
void u_mpz_sub(mpz_t& dst, mpz_t& src1, mp_bitcnt_t bitcnt1, mpz_t& src2, mp_bitcnt_t bitcnt2) {
  // mpz_sub(dst, src1, src2);
  mpz_set_ui(dst, 1);
  mpz_mul_2exp(dst, dst, bitcnt2);
  mpz_sub(dst, dst, src2);
  mpz_add(dst, dst, src1);
}
void u_mpz_sub_ui_l(mpz_t& dst, unsigned long val, mp_bitcnt_t bitcnt1, mpz_t& src, mp_bitcnt_t bitcnt2) {
  mpz_set_ui(dst, val);
  mpz_sub(dst, dst, src);
}
void u_mpz_sub_ui_r(mpz_t& dst, mpz_t& src, unsigned long val, mp_bitcnt_t bitcnt) {
  mpz_sub_ui(dst, src, val);
}
void u_mpz_sub_ui2(mpz_t& dst, unsigned long val1, mp_bitcnt_t bitcnt1, unsigned long val2, mp_bitcnt_t bitcnt2) {
  mpz_set_ui(dst, val1);
  mpz_sub_ui(dst, dst, val2);
}
//mul
void u_mpz_mul(mpz_t& dst, mpz_t& src1, mp_bitcnt_t bitcnt1, mpz_t& src2, mp_bitcnt_t bitcnt2) {
  mpz_mul(dst, src1, src2);
}
void u_mpz_mul_ui_l(mpz_t& dst, unsigned long val, mp_bitcnt_t bitcnt1, mpz_t& src, mp_bitcnt_t bitcnt2) {
  mpz_mul_ui(dst, src, val);
}
void u_mpz_mul_ui_r(mpz_t& dst, mpz_t& src, unsigned long val, mp_bitcnt_t bitcnt) {
  mpz_mul_ui(dst, src, val);
}
void u_mpz_mul_ui2(mpz_t& dst, unsigned long val1, mp_bitcnt_t bitcnt1, unsigned long val2, mp_bitcnt_t bitcnt2) {
  mpz_set_ui(dst, val1);
  mpz_mul_ui(dst, dst, val2);
}
//div
void u_mpz_div(mpz_t& dst, mpz_t& src1, mp_bitcnt_t bitcnt1, mpz_t& src2, mp_bitcnt_t bitcnt2) {
  if(mpz_cmp_ui(src2, 0) == 0) return;
    mpz_tdiv_q(dst, src1, src2);
}
void u_mpz_div_ui_l(mpz_t& dst, unsigned long val, mp_bitcnt_t bitcnt1, mpz_t& src, mp_bitcnt_t bitcnt2) {
  if(mpz_cmp_ui(src, 0) == 0) return;
  mpz_set_ui(dst, val);
  mpz_tdiv_q(dst, dst, src);
}
void u_mpz_div_ui_r(mpz_t& dst, mpz_t& src, unsigned long val, mp_bitcnt_t bitcnt) {
  if(val == 0) return;
  mpz_tdiv_q_ui(dst, src, val);
}
void u_mpz_div_ui2(mpz_t& dst, unsigned long val1, mp_bitcnt_t bitcnt1, unsigned long val2, mp_bitcnt_t bitcnt2) {
  if(val2 == 0) return;
  mpz_set_ui(dst, val1 / val2);
}
//rem
void u_mpz_rem(mpz_t& dst, mpz_t& src1, mp_bitcnt_t bitcnt1, mpz_t& src2, mp_bitcnt_t bitcnt2) {
  if(mpz_cmp_ui(src2, 0) == 0) return;
  mpz_tdiv_r(dst, src1, src2);
}
void u_mpz_rem_ui_l(mpz_t& dst, unsigned long val, mp_bitcnt_t bitcnt1, mpz_t& src, mp_bitcnt_t bitcnt2) {
  if(mpz_cmp_ui(src, 0) == 0) return;
  mpz_set_ui(dst, val);
  mpz_tdiv_r(dst, dst, src);
}
void u_mpz_rem_ui_r(mpz_t& dst, mpz_t& src, unsigned long val, mp_bitcnt_t bitcnt) {
  if(val == 0) return;
  mpz_tdiv_r_ui(dst, src, val);
}
void u_mpz_rem_ui2(mpz_t& dst, unsigned long val1, mp_bitcnt_t bitcnt1, unsigned long val2, mp_bitcnt_t bitcnt2) {
  if(val2 == 0) return;
  mpz_set_ui(dst, val1 % val2);
}
//lt
void u_mpz_lt(mpz_t& dst, mpz_t& op1, mp_bitcnt_t bitcnt1, mpz_t& op2, mp_bitcnt_t bitcnt2) {
  mpz_set_ui(dst, mpz_cmp(op1, op2)<0);
}
void u_mpz_lt_ui_r(mpz_t& dst, mpz_t& src, unsigned long val, mp_bitcnt_t bitcnt) {
  mpz_set_ui(dst, mpz_cmp_ui(src, val)<0);
}
void u_mpz_lt_ui_l(mpz_t& dst, unsigned long val, mp_bitcnt_t bitcnt1, mpz_t& src, mp_bitcnt_t bitcnt2) {
  mpz_set_ui(dst, mpz_cmp_ui(src, val)>0);
}
void u_mpz_lt_ui2(mpz_t& dst, unsigned long val1, mp_bitcnt_t bitcnt1, unsigned long val2, mp_bitcnt_t bitcnt2) {
  mpz_set_ui(dst, val1 < val2);
}
//leq
void u_mpz_leq(mpz_t& dst, mpz_t& op1, mp_bitcnt_t bitcnt1, mpz_t& op2, mp_bitcnt_t bitcnt2) {
  mpz_set_ui(dst, mpz_cmp(op1, op2)<=0);
}
void u_mpz_leq_ui_r(mpz_t& dst, mpz_t& src, unsigned long val, mp_bitcnt_t bitcnt) {
  mpz_set_ui(dst, mpz_cmp_ui(src, val)<=0);
}
void u_mpz_leq_ui_l(mpz_t& dst, unsigned long val, mp_bitcnt_t bitcnt1, mpz_t& src, mp_bitcnt_t bitcnt2) {
  mpz_set_ui(dst, mpz_cmp_ui(src, val)>=0);
}
void u_mpz_leq_ui2(mpz_t& dst, unsigned long val1, mp_bitcnt_t bitcnt1, unsigned long val2, mp_bitcnt_t bitcnt2) {
  mpz_set_ui(dst, val1 <= val2);
}
//gt
void u_mpz_gt(mpz_t& dst, mpz_t& op1, mp_bitcnt_t bitcnt1, mpz_t& op2, mp_bitcnt_t bitcnt2) {
  mpz_set_ui(dst, mpz_cmp(op1, op2)>0);
}
void u_mpz_gt_ui_r(mpz_t& dst, mpz_t& src, unsigned long val, mp_bitcnt_t bitcnt) {
  mpz_set_ui(dst, mpz_cmp_ui(src, val)>0);
}
void u_mpz_gt_ui_l(mpz_t& dst, unsigned long val, mp_bitcnt_t bitcnt1, mpz_t& src, mp_bitcnt_t bitcnt2) {
  mpz_set_ui(dst, mpz_cmp_ui(src, val)<0);
}
void u_mpz_gt_ui2(mpz_t& dst, unsigned long val1, mp_bitcnt_t bitcnt1, unsigned long val2, mp_bitcnt_t bitcnt2) {
  mpz_set_ui(dst, val1 > val2);
}
//geq
void u_mpz_geq(mpz_t& dst, mpz_t& op1, mp_bitcnt_t bitcnt1, mpz_t& op2, mp_bitcnt_t bitcnt2) {
  mpz_set_ui(dst, mpz_cmp(op1, op2)>=0);
}
void u_mpz_geq_ui_r(mpz_t& dst, mpz_t& src, unsigned long val, mp_bitcnt_t bitcnt) {
  mpz_set_ui(dst, mpz_cmp_ui(src, val)>=0);
}
void u_mpz_geq_ui_l(mpz_t& dst, unsigned long val, mp_bitcnt_t bitcnt1, mpz_t& src, mp_bitcnt_t bitcnt2) {
  mpz_set_ui(dst, mpz_cmp_ui(src, val)<=0);
}
void u_mpz_geq_ui2(mpz_t& dst, unsigned long val1, mp_bitcnt_t bitcnt1, unsigned long val2, mp_bitcnt_t bitcnt2) {
  mpz_set_ui(dst, val1 >= val2);
}
//eq
void u_mpz_eq(mpz_t& dst, mpz_t& op1, mp_bitcnt_t bitcnt1, mpz_t& op2, mp_bitcnt_t bitcnt2) {
  mpz_set_ui(dst, mpz_cmp(op1, op2)==0);
}
void u_mpz_eq_ui_r(mpz_t& dst, mpz_t& src, unsigned long val, mp_bitcnt_t bitcnt) {
  mpz_set_ui(dst, mpz_cmp_ui(src, val)==0);
}
void u_mpz_eq_ui_l(mpz_t& dst, unsigned long val, mp_bitcnt_t bitcnt1, mpz_t& src, mp_bitcnt_t bitcnt2) {
  mpz_set_ui(dst, mpz_cmp_ui(src, val)==0);
}
void u_mpz_eq_ui2(mpz_t& dst, unsigned long val1, mp_bitcnt_t bitcnt1, unsigned long val2, mp_bitcnt_t bitcnt2) {
  mpz_set_ui(dst, val1 == val2);
}
//neq
void u_mpz_neq(mpz_t& dst, mpz_t& op1, mp_bitcnt_t bitcnt1, mpz_t& op2, mp_bitcnt_t bitcnt2) {
  mpz_set_ui(dst, mpz_cmp(op1, op2)!=0);
}
void u_mpz_neq_ui_r(mpz_t& dst, mpz_t& src, unsigned long val, mp_bitcnt_t bitcnt) {
  mpz_set_ui(dst, mpz_cmp_ui(src, val)!=0);
}
void u_mpz_neq_ui_l(mpz_t& dst, unsigned long val, mp_bitcnt_t bitcnt1, mpz_t& src, mp_bitcnt_t bitcnt2) {
  mpz_set_ui(dst, mpz_cmp_ui(src, val)!=0);
}
void u_mpz_neq_ui2(mpz_t& dst, unsigned long val1, mp_bitcnt_t bitcnt1, unsigned long val2, mp_bitcnt_t bitcnt2) {
  mpz_set_ui(dst, val1 != val2);
}
//and
void u_mpz_and(mpz_t& dst, mpz_t& src1, mp_bitcnt_t bitcnt1, mpz_t& src2, mp_bitcnt_t bitcnt2) {
  mpz_and(dst, src1, src2);
}
void u_mpz_and_ui_r(mpz_t& dst, mpz_t& src, unsigned long val, mp_bitcnt_t bitcnt) {
  mpz_set_ui(dst, val);
  mpz_and(dst, dst, src);
}
void u_mpz_and_ui_l(mpz_t& dst, unsigned long val, mp_bitcnt_t bitcnt1, mpz_t& src, mp_bitcnt_t bitcnt2) {
  mpz_set_ui(dst, val);
  mpz_and(dst, dst, src);
}
void u_mpz_and_ui2(mpz_t& dst, unsigned long val1, mp_bitcnt_t bitcnt1, unsigned long val2, mp_bitcnt_t bitcnt2) {
  mpz_set_ui(dst, val1 & val2);
}
//ior
void u_mpz_ior(mpz_t& dst, mpz_t& src1, mp_bitcnt_t bitcnt1, mpz_t& src2, mp_bitcnt_t bitcnt2) {
  mpz_ior(dst, src1, src2);
}
void u_mpz_ior_ui_r(mpz_t& dst, mpz_t& src, unsigned long val, mp_bitcnt_t bitcnt) {
  mpz_set_ui(dst, val);
  mpz_ior(dst, dst, src);
}
void u_mpz_ior_ui_l(mpz_t& dst, unsigned long val, mp_bitcnt_t bitcnt1, mpz_t& src, mp_bitcnt_t bitcnt2) {
  mpz_set_ui(dst, val);
  mpz_ior(dst, dst, src);
}
void u_mpz_ior_ui2(mpz_t& dst, unsigned long val1, mp_bitcnt_t bitcnt1, unsigned long val2, mp_bitcnt_t bitcnt2) {
  mpz_set_ui(dst, val1 | val2);
}
//xor
void u_mpz_xor(mpz_t& dst, mpz_t& src1, mp_bitcnt_t bitcnt1, mpz_t& src2, mp_bitcnt_t bitcnt2) {
  mpz_xor(dst, src1, src2);
}
void u_mpz_xor_ui_r(mpz_t& dst, mpz_t& src, unsigned long val, mp_bitcnt_t bitcnt) {
  mpz_set_ui(dst, val);
  mpz_xor(dst, dst, src);
}
void u_mpz_xor_ui_l(mpz_t& dst, unsigned long val, mp_bitcnt_t bitcnt1, mpz_t& src, mp_bitcnt_t bitcnt2) {
  mpz_set_ui(dst, val);
  mpz_xor(dst, dst, src);
}
void u_mpz_xor_ui2(mpz_t& dst, unsigned long val1, mp_bitcnt_t bitcnt1, unsigned long val2, mp_bitcnt_t bitcnt2) {
  mpz_set_ui(dst, val1 ^ val2);
}
//dshl
void u_mpz_dshl(mpz_t& dst, mpz_t& src1, mp_bitcnt_t bitcnt1, mpz_t& src2, mp_bitcnt_t bitcnt2) {
  mpz_mul_2exp(dst, src1, mpz_get_ui(src2));
}
void u_mpz_dshl_ui_r(mpz_t& dst, mpz_t& src, unsigned long val, mp_bitcnt_t bitcnt) {
  mpz_mul_2exp(dst, src, val);
}
void u_mpz_dshl_ui_l(mpz_t& dst, unsigned long val, mp_bitcnt_t bitcnt1, mpz_t& src, mp_bitcnt_t bitcnt2) {
  mpz_set_ui(dst, val);
  mpz_mul_2exp(dst, dst, mpz_get_ui(src));
}
void u_mpz_dshl_ui2(mpz_t& dst, unsigned long val1, mp_bitcnt_t bitcnt1, unsigned long val2, mp_bitcnt_t bitcnt2) {
  mpz_set_ui(dst, val1 << val2);
}
//dshr
void u_mpz_dshr(mpz_t& dst, mpz_t& src1, mp_bitcnt_t bitcnt1, mpz_t& src2, mp_bitcnt_t bitcnt2) {
  mpz_tdiv_q_2exp(dst, src1, mpz_get_ui(src2));
}
void s_mpz_dshr(mpz_t& dst, mpz_t& src1, mp_bitcnt_t bitcnt1, mpz_t& src2, mp_bitcnt_t bitcnt2) {
  int n = mpz_get_ui(src2);
  if(mpz_cmp_ui(src1, 0) < 0) {
    mpz_fdiv_q_2exp(dst, src1, n);
  } else {
    mpz_tdiv_q_2exp(dst, src1, n);
  }
}
void u_mpz_dshr_ui_r(mpz_t& dst, mpz_t& src, unsigned long val, mp_bitcnt_t bitcnt) {
  mpz_tdiv_q_2exp(dst, src, val);
}
void u_mpz_dshr_ui_l(mpz_t& dst, unsigned long val, mp_bitcnt_t bitcnt1, mpz_t& src, mp_bitcnt_t bitcnt2) {
  mpz_set_ui(dst, val >> mpz_get_ui(src));
}
void u_mpz_dshr_ui2(mpz_t& dst, unsigned long val1, mp_bitcnt_t bitcnt1, unsigned long val2, mp_bitcnt_t bitcnt2) {
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
void u_neg(mpz_t& dst, mpz_t& src, mp_bitcnt_t bitcnt) {
  mpz_set_ui(dst, 0);
  mpz_sub(dst, dst, src);
}

void init_functions() {
  mpz_init(t1);
}