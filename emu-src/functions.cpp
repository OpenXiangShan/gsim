#include <gmp.h>
#include <assert.h>

// s_tail: remain the last n bits
void s_tail(mpz_t& dst, mpz_t& src, unsigned long n) {
  mpz_set(dst, src);
  if(mpz_size(dst) == 0) return;
  int libms_num = (n + 63) / 64;
  unsigned long mask = ((unsigned long)1 << (n % 64)) - 1;
  mp_limb_t* data = mpz_limbs_modify(dst, libms_num);
  *data = *data & mask;
  mpz_limbs_finish(dst, libms_num);
}
//s_cat: concat src1 and src2
void s_cat(mpz_t& dst, mpz_t& src1, mp_bitcnt_t bitcnt1, mpz_t& src2, mp_bitcnt_t bitcnt2) {
  mpz_mul_2exp(dst, src1, bitcnt2);
  mpz_ior(dst, dst, src2); 
}
void s_cat_ui_r(mpz_t& dst, mpz_t& src, unsigned long val, mp_bitcnt_t bitcnt) {
  mpz_mul_2exp(dst, src, bitcnt);
  mpz_add_ui(dst, dst, val);
}
void s_cat_ui_l(mpz_t& dst, unsigned long val, mp_bitcnt_t bitcnt1, mpz_t& src, mp_bitcnt_t bitcnt2) {
  mpz_set_ui(dst, val);
  mpz_mul_2exp(dst, dst, bitcnt2);
  mpz_add_ui(dst, dst, val);
}
void s_cat_ui2(mpz_t& dst, unsigned long val1, mp_bitcnt_t bitcnt1, unsigned long val2, mp_bitcnt_t bitcnt2) {
  mpz_set_ui(dst, val1);
  mpz_mul_2exp(dst, dst, bitcnt2);
  mpz_add_ui(dst, dst, val2);
}
//s_asSInt
void s_asSInt(mpz_t& dst, mpz_t& src) {
  mpz_set(dst, src);
  mpz_limbs_finish(dst, -mpz_size(src));
}
//s_asUInt
void s_asUInt(mpz_t& dst, mpz_t& src) {
  mpz_set(dst, src);
  mpz_limbs_finish(dst, mpz_size(src));
}
//s_bits
void s_bits(mpz_t& dst, mpz_t& src, mp_bitcnt_t h, mp_bitcnt_t l) {
  if(mpz_size(src) == 0) {
    mpz_set(dst, src);
    return;
  }
  mp_bitcnt_t left_bits = h - l + 1;
  mpz_tdiv_q_2exp(dst, src, l);
  int libms_num = (left_bits + 63) / 64;
  unsigned long int mask = ((unsigned long)1 << (left_bits % 64)) - 1;
  mp_limb_t* data = mpz_limbs_modify(dst, libms_num);
  *data = *data & mask;
  mpz_limbs_finish(dst, libms_num);
}
//s_pat: sign/zero extends to n bits
void s_pad(mpz_t& dst, mpz_t& src, mp_bitcnt_t n) {
  mp_bitcnt_t bitcnt = mpz_sizeinbase(src, 2);
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
//add
void s_mpz_add(mpz_t& dst, mpz_t& src1, mp_bitcnt_t bitcnt1, mpz_t& src2, mp_bitcnt_t bitcnt2) {
  mpz_add(dst, src1, src2);
}
void s_mpz_add_ui_l(mpz_t& dst, unsigned long val, mp_bitcnt_t bitcnt1, mpz_t& src, mp_bitcnt_t bitcnt2) {
  mpz_add_ui(dst, src, val);
}
void s_mpz_add_ui_r(mpz_t& dst, mpz_t& src, unsigned long val, mp_bitcnt_t bitcnt) {
  mpz_add_ui(dst, src, val);
}
void s_mpz_add_ui2(mpz_t& dst, unsigned long val1, mp_bitcnt_t bitcnt1, unsigned long val2, mp_bitcnt_t bitcnt2) {
  mpz_set_ui(dst, val1);
  mpz_add_ui(dst, dst, val2);
}
//lt
void s_mpz_lt(mpz_t& dst, mpz_t& op1, mp_bitcnt_t bitcnt1, mpz_t& op2, mp_bitcnt_t bitcnt2) {
  mpz_set_ui(dst, mpz_cmp(op1, op2)<0);
}
void s_mpz_lt_ui_r(mpz_t& dst, mpz_t& src, unsigned long val, mp_bitcnt_t bitcnt) {
  mpz_set_ui(dst, mpz_cmp_ui(src, val)<0);
}
void s_mpz_lt_ui_l(mpz_t& dst, unsigned long val, mp_bitcnt_t bitcnt1, mpz_t& src, mp_bitcnt_t bitcnt2) {
  mpz_set_ui(dst, mpz_cmp_ui(src, val)>0);
}
void s_mpz_lt_ui2(mpz_t& dst, unsigned long val1, mp_bitcnt_t bitcnt1, unsigned long val2, mp_bitcnt_t bitcnt2) {
  mpz_set_ui(dst, val1 < val2);
}
//leq
void s_mpz_leq(mpz_t& dst, mpz_t& op1, mp_bitcnt_t bitcnt1, mpz_t& op2, mp_bitcnt_t bitcnt2) {
  mpz_set_ui(dst, mpz_cmp(op1, op2)<=0);
}
void s_mpz_leq_ui_r(mpz_t& dst, mpz_t& src, unsigned long val, mp_bitcnt_t bitcnt) {
  mpz_set_ui(dst, mpz_cmp_ui(src, val)<=0);
}
void s_mpz_leq_ui_l(mpz_t& dst, unsigned long val, mp_bitcnt_t bitcnt1, mpz_t& src, mp_bitcnt_t bitcnt2) {
  mpz_set_ui(dst, mpz_cmp_ui(src, val)>=0);
}
void s_mpz_leq_ui2(mpz_t& dst, unsigned long val1, mp_bitcnt_t bitcnt1, unsigned long val2, mp_bitcnt_t bitcnt2) {
  mpz_set_ui(dst, val1 <= val2);
}
//gt
void s_mpz_gt(mpz_t& dst, mpz_t& op1, mp_bitcnt_t bitcnt1, mpz_t& op2, mp_bitcnt_t bitcnt2) {
  mpz_set_ui(dst, mpz_cmp(op1, op2)>0);
}
void s_mpz_gt_ui_r(mpz_t& dst, mpz_t& src, unsigned long val, mp_bitcnt_t bitcnt) {
  mpz_set_ui(dst, mpz_cmp_ui(src, val)>0);
}
void s_mpz_gt_ui_l(mpz_t& dst, unsigned long val, mp_bitcnt_t bitcnt1, mpz_t& src, mp_bitcnt_t bitcnt2) {
  mpz_set_ui(dst, mpz_cmp_ui(src, val)<0);
}
void s_mpz_gt_ui2(mpz_t& dst, unsigned long val1, mp_bitcnt_t bitcnt1, unsigned long val2, mp_bitcnt_t bitcnt2) {
  mpz_set_ui(dst, val1 > val2);
}
//geq
void s_mpz_geq(mpz_t& dst, mpz_t& op1, mp_bitcnt_t bitcnt1, mpz_t& op2, mp_bitcnt_t bitcnt2) {
  mpz_set_ui(dst, mpz_cmp(op1, op2)>=0);
}
void s_mpz_geq_ui_r(mpz_t& dst, mpz_t& src, unsigned long val, mp_bitcnt_t bitcnt) {
  mpz_set_ui(dst, mpz_cmp_ui(src, val)>=0);
}
void s_mpz_geq_ui_l(mpz_t& dst, unsigned long val, mp_bitcnt_t bitcnt1, mpz_t& src, mp_bitcnt_t bitcnt2) {
  mpz_set_ui(dst, mpz_cmp_ui(src, val)<=0);
}
void s_mpz_geq_ui2(mpz_t& dst, unsigned long val1, mp_bitcnt_t bitcnt1, unsigned long val2, mp_bitcnt_t bitcnt2) {
  mpz_set_ui(dst, val1 >= val2);
}
//eq
void s_mpz_eq(mpz_t& dst, mpz_t& op1, mp_bitcnt_t bitcnt1, mpz_t& op2, mp_bitcnt_t bitcnt2) {
  mpz_set_ui(dst, mpz_cmp(op1, op2)==0);
}
void s_mpz_eq_ui_r(mpz_t& dst, mpz_t& src, unsigned long val, mp_bitcnt_t bitcnt) {
  mpz_set_ui(dst, mpz_cmp_ui(src, val)==0);
}
void s_mpz_eq_ui_l(mpz_t& dst, unsigned long val, mp_bitcnt_t bitcnt1, mpz_t& src, mp_bitcnt_t bitcnt2) {
  mpz_set_ui(dst, mpz_cmp_ui(src, val)==0);
}
void s_mpz_eq_ui2(mpz_t& dst, unsigned long val1, mp_bitcnt_t bitcnt1, unsigned long val2, mp_bitcnt_t bitcnt2) {
  mpz_set_ui(dst, val1 == val2);
}
//neq
void s_mpz_neq(mpz_t& dst, mpz_t& op1, mp_bitcnt_t bitcnt1, mpz_t& op2, mp_bitcnt_t bitcnt2) {
  mpz_set_ui(dst, mpz_cmp(op1, op2)!=0);
}
void s_mpz_neq_ui_r(mpz_t& dst, mpz_t& src, unsigned long val, mp_bitcnt_t bitcnt) {
  mpz_set_ui(dst, mpz_cmp_ui(src, val)!=0);
}
void s_mpz_neq_ui_l(mpz_t& dst, unsigned long val, mp_bitcnt_t bitcnt1, mpz_t& src, mp_bitcnt_t bitcnt2) {
  mpz_set_ui(dst, mpz_cmp_ui(src, val)!=0);
}
void s_mpz_neq_ui2(mpz_t& dst, unsigned long val1, mp_bitcnt_t bitcnt1, unsigned long val2, mp_bitcnt_t bitcnt2) {
  mpz_set_ui(dst, val1 != val2);
}
//and
void s_mpz_and(mpz_t& dst, mpz_t& src1, mp_bitcnt_t bitcnt1, mpz_t& src2, mp_bitcnt_t bitcnt2) {
  mpz_and(dst, src1, src2);
}
void s_mpz_and_ui_r(mpz_t& dst, mpz_t& src, unsigned long val, mp_bitcnt_t bitcnt) {
  mpz_set_ui(dst, val);
  mpz_and(dst, dst, src);
}
void s_mpz_and_ui_l(mpz_t& dst, unsigned long val, mp_bitcnt_t bitcnt1, mpz_t& src, mp_bitcnt_t bitcnt2) {
  mpz_set_ui(dst, val);
  mpz_and(dst, dst, src);
}
void s_mpz_and_ui2(mpz_t& dst, unsigned long val1, mp_bitcnt_t bitcnt1, unsigned long val2, mp_bitcnt_t bitcnt2) {
  mpz_set_ui(dst, val1 & val2);
}
//ior
void s_mpz_ior(mpz_t& dst, mpz_t& src1, mp_bitcnt_t bitcnt1, mpz_t& src2, mp_bitcnt_t bitcnt2) {
  mpz_ior(dst, src1, src2);
}
void s_mpz_ior_ui_r(mpz_t& dst, mpz_t& src, unsigned long val, mp_bitcnt_t bitcnt) {
  mpz_set_ui(dst, val);
  mpz_ior(dst, dst, src);
}
void s_mpz_ior_ui_l(mpz_t& dst, unsigned long val, mp_bitcnt_t bitcnt1, mpz_t& src, mp_bitcnt_t bitcnt2) {
  mpz_set_ui(dst, val);
  mpz_ior(dst, dst, src);
}
void s_mpz_ior_ui2(mpz_t& dst, unsigned long val1, mp_bitcnt_t bitcnt1, unsigned long val2, mp_bitcnt_t bitcnt2) {
  mpz_set_ui(dst, val1 | val2);
}
//xor
void s_mpz_xor(mpz_t& dst, mpz_t& src1, mp_bitcnt_t bitcnt1, mpz_t& src2, mp_bitcnt_t bitcnt2) {
  mpz_xor(dst, src1, src2);
}
void s_mpz_xor_ui_r(mpz_t& dst, mpz_t& src, unsigned long val, mp_bitcnt_t bitcnt) {
  mpz_set_ui(dst, val);
  mpz_xor(dst, dst, src);
}
void s_mpz_xor_ui_l(mpz_t& dst, unsigned long val, mp_bitcnt_t bitcnt1, mpz_t& src, mp_bitcnt_t bitcnt2) {
  mpz_set_ui(dst, val);
  mpz_xor(dst, dst, src);
}
void s_mpz_xor_ui2(mpz_t& dst, unsigned long val1, mp_bitcnt_t bitcnt1, unsigned long val2, mp_bitcnt_t bitcnt2) {
  mpz_set_ui(dst, val1 ^ val2);
}
//dshl
void s_mpz_dshl(mpz_t& dst, mpz_t& src1, mpz_t& src2) {
  mpz_mul_2exp(dst, src1, mpz_get_ui(src2));
}
void s_mpz_dshl_ui_r(mpz_t& dst, mpz_t& src, unsigned long val, mp_bitcnt_t bitcnt) {
  mpz_mul_2exp(dst, src, val);
}
void s_mpz_dshl_ui_l(mpz_t& dst, unsigned long val, mp_bitcnt_t bitcnt1, mpz_t& src, mp_bitcnt_t bitcnt2) {
  mpz_set_ui(dst, val);
  mpz_mul_2exp(dst, dst, mpz_get_ui(src));
}
void s_mpz_dshl_ui2(mpz_t& dst, unsigned long val1, mp_bitcnt_t bitcnt1, unsigned long val2, mp_bitcnt_t bitcnt2) {
  mpz_set_ui(dst, val1 << val2);
}
//orr
void s_orr(mpz_t& dst, mpz_t& src) {
  mpz_set_ui(dst, mpz_cmp_ui(src, 0) == 0 ? 0 : 1);
}