#ifndef FUNCTIONS_H
#define FUNCTIONS_H
#include <gmp.h>

#define Assert(cond, ...) \
  do { \
    if (!(cond)) { \
      fprintf(stderr, "\33[1;31m"); \
      fprintf(stderr, __VA_ARGS__); \
      fprintf(stderr, "\33[0m\n"); \
      assert(cond); \
    } \
  } while (0)

void u_tail(mpz_t& dst, mpz_t& src, mp_bitcnt_t bitcnt, unsigned long n);
void u_head(mpz_t& dst, mpz_t& src, mp_bitcnt_t bitcnt, unsigned long n);
void u_shl(mpz_t& dst, mpz_t& src, mp_bitcnt_t bitcnt, unsigned long n);
void u_shl_ui(mpz_t& dst, unsigned long src, unsigned long n);
void u_shr(mpz_t& dst, mpz_t& src, mp_bitcnt_t bitcnt, unsigned long n);
void s_shr(mpz_t& dst, mpz_t& src, mp_bitcnt_t bitcnt, unsigned long n);
void u_shr_ui(mpz_t& dst, unsigned long src, unsigned long n);
void u_cat(mpz_t& dst, mpz_t& src1, mp_bitcnt_t bitcnt1, mpz_t& src2, mp_bitcnt_t bitcnt2);
void u_cat_ui_r(mpz_t& dst, mpz_t& src, unsigned long val, mp_bitcnt_t bitcnt);
void u_cat_ui_l(mpz_t& dst, unsigned long val, mp_bitcnt_t bitcnt1, mpz_t& src, mp_bitcnt_t bitcnt2);
void u_cat_ui2(mpz_t& dst, unsigned long val1, mp_bitcnt_t bitcnt1, unsigned long val2, mp_bitcnt_t bitcnt2);
void s_asSInt(mpz_t& dst, mpz_t& src, mp_bitcnt_t bitcnt);
void u_asUInt(mpz_t& dst, mpz_t& src, mp_bitcnt_t bitcnt);
void u_asClock(mpz_t& dst, mpz_t& src, mp_bitcnt_t bitcnt);
void u_bits(mpz_t& dst, mpz_t& src, mp_bitcnt_t l, mp_bitcnt_t r);
void u_pad(mpz_t& dst, mpz_t& src, mp_bitcnt_t bitcnt, mp_bitcnt_t n);
//arithmetic
void u_mpz_add(mpz_t& dst, mpz_t& src1, mp_bitcnt_t bitcnt1, mpz_t& src2, mp_bitcnt_t bitcnt2);
void u_mpz_add_ui_l(mpz_t& dst, unsigned long val, mp_bitcnt_t bitcnt1, mpz_t& src, mp_bitcnt_t bitcnt2);
void u_mpz_add_ui_r(mpz_t& dst, mpz_t& src, unsigned long val, mp_bitcnt_t bitcnt);
void u_mpz_add_ui2(mpz_t& dst, unsigned long val1, mp_bitcnt_t bitcnt1, unsigned long val2, mp_bitcnt_t bitcnt2);
void u_mpz_sub(mpz_t& dst, mpz_t& src1, mp_bitcnt_t bitcnt1, mpz_t& src2, mp_bitcnt_t bitcnt2);
void u_mpz_sub_ui_l(mpz_t& dst, unsigned long val, mp_bitcnt_t bitcnt1, mpz_t& src, mp_bitcnt_t bitcnt2);
void u_mpz_sub_ui_r(mpz_t& dst, mpz_t& src, unsigned long val, mp_bitcnt_t bitcnt);
void u_mpz_sub_ui2(mpz_t& dst, unsigned long val1, mp_bitcnt_t bitcnt1, unsigned long val2, mp_bitcnt_t bitcnt2);
void u_mpz_mul(mpz_t& dst, mpz_t& src1, mp_bitcnt_t bitcnt1, mpz_t& src2, mp_bitcnt_t bitcnt2);
void u_mpz_mul_ui_l(mpz_t& dst, unsigned long val, mp_bitcnt_t bitcnt1, mpz_t& src, mp_bitcnt_t bitcnt2);
void u_mpz_mul_ui_r(mpz_t& dst, mpz_t& src, unsigned long val, mp_bitcnt_t bitcnt);
void u_mpz_mul_ui2(mpz_t& dst, unsigned long val1, mp_bitcnt_t bitcnt1, unsigned long val2, mp_bitcnt_t bitcnt2);
void u_mpz_div(mpz_t& dst, mpz_t& src1, mp_bitcnt_t bitcnt1, mpz_t& src2, mp_bitcnt_t bitcnt2);
void u_mpz_div_ui_l(mpz_t& dst, unsigned long val, mp_bitcnt_t bitcnt1, mpz_t& src, mp_bitcnt_t bitcnt2);
void u_mpz_div_ui_r(mpz_t& dst, mpz_t& src, unsigned long val, mp_bitcnt_t bitcnt);
void u_mpz_div_ui2(mpz_t& dst, unsigned long val1, mp_bitcnt_t bitcnt1, unsigned long val2, mp_bitcnt_t bitcnt2);
void u_mpz_rem(mpz_t& dst, mpz_t& src1, mp_bitcnt_t bitcnt1, mpz_t& src2, mp_bitcnt_t bitcnt2);
void u_mpz_rem_ui_l(mpz_t& dst, unsigned long val, mp_bitcnt_t bitcnt1, mpz_t& src, mp_bitcnt_t bitcnt2);
void u_mpz_rem_ui_r(mpz_t& dst, mpz_t& src, unsigned long val, mp_bitcnt_t bitcnt);
void u_mpz_rem_ui2(mpz_t& dst, unsigned long val1, mp_bitcnt_t bitcnt1, unsigned long val2, mp_bitcnt_t bitcnt2);
//comparison
void u_mpz_lt(mpz_t& dst, mpz_t& op1, mp_bitcnt_t bitcnt1, mpz_t& op2, mp_bitcnt_t bitcnt2);
void u_mpz_lt_ui_r(mpz_t& dst, mpz_t& src, unsigned long val, mp_bitcnt_t bitcnt);
void u_mpz_lt_ui_l(mpz_t& dst, unsigned long val, mp_bitcnt_t bitcnt1, mpz_t& src, mp_bitcnt_t bitcnt2);
void u_mpz_lt_ui2(mpz_t& dst, unsigned long val1, mp_bitcnt_t bitcnt1, unsigned long val2, mp_bitcnt_t bitcnt2);
void u_mpz_leq(mpz_t& dst, mpz_t& op1, mp_bitcnt_t bitcnt1, mpz_t& op2, mp_bitcnt_t bitcnt2);
void u_mpz_leq_ui_r(mpz_t& dst, mpz_t& src, unsigned long val, mp_bitcnt_t bitcnt);
void u_mpz_leq_ui_l(mpz_t& dst, unsigned long val, mp_bitcnt_t bitcnt1, mpz_t& src, mp_bitcnt_t bitcnt2);
void u_mpz_leq_ui2(mpz_t& dst, unsigned long val1, mp_bitcnt_t bitcnt1, unsigned long val2, mp_bitcnt_t bitcnt2);
void u_mpz_gt(mpz_t& dst, mpz_t& op1, mp_bitcnt_t bitcnt1, mpz_t& op2, mp_bitcnt_t bitcnt2);
void u_mpz_gt_ui_r(mpz_t& dst, mpz_t& src, unsigned long val, mp_bitcnt_t bitcnt);
void u_mpz_gt_ui_l(mpz_t& dst, unsigned long val, mp_bitcnt_t bitcnt1, mpz_t& src, mp_bitcnt_t bitcnt2);
void u_mpz_gt_ui2(mpz_t& dst, unsigned long val1, mp_bitcnt_t bitcnt1, unsigned long val2, mp_bitcnt_t bitcnt2);
void u_mpz_geq(mpz_t& dst, mpz_t& op1, mp_bitcnt_t bitcnt1, mpz_t& op2, mp_bitcnt_t bitcnt2);
void u_mpz_geq_ui_r(mpz_t& dst, mpz_t& src, unsigned long val, mp_bitcnt_t bitcnt);
void u_mpz_geq_ui_l(mpz_t& dst, unsigned long val, mp_bitcnt_t bitcnt1, mpz_t& src, mp_bitcnt_t bitcnt2);
void u_mpz_geq_ui2(mpz_t& dst, unsigned long val1, mp_bitcnt_t bitcnt1, unsigned long val2, mp_bitcnt_t bitcnt2);
void u_mpz_eq(mpz_t& dst, mpz_t& op1, mp_bitcnt_t bitcnt1, mpz_t& op2, mp_bitcnt_t bitcnt2);
void u_mpz_eq_ui_r(mpz_t& dst, mpz_t& src, unsigned long val, mp_bitcnt_t bitcnt);
void u_mpz_eq_ui_l(mpz_t& dst, unsigned long val, mp_bitcnt_t bitcnt1, mpz_t& src, mp_bitcnt_t bitcnt2);
void u_mpz_eq_ui2(mpz_t& dst, unsigned long val1, mp_bitcnt_t bitcnt1, unsigned long val2, mp_bitcnt_t bitcnt2);
void u_mpz_neq(mpz_t& dst, mpz_t& op1, mp_bitcnt_t bitcnt1, mpz_t& op2, mp_bitcnt_t bitcnt2);
void u_mpz_neq_ui_r(mpz_t& dst, mpz_t& src, unsigned long val, mp_bitcnt_t bitcnt);
void u_mpz_neq_ui_l(mpz_t& dst, unsigned long val, mp_bitcnt_t bitcnt1, mpz_t& src, mp_bitcnt_t bitcnt2);
void u_mpz_neq_ui2(mpz_t& dst, unsigned long val1, mp_bitcnt_t bitcnt1, unsigned long val2, mp_bitcnt_t bitcnt2);
//logical
void u_mpz_and(mpz_t& dst, mpz_t& src1, mp_bitcnt_t bitcnt1, mpz_t& src2, mp_bitcnt_t bitcnt2);
void u_mpz_and_ui_r(mpz_t& dst, mpz_t& src, unsigned long val, mp_bitcnt_t bitcnt);
void u_mpz_and_ui_l(mpz_t& dst, unsigned long val, mp_bitcnt_t bitcnt1, mpz_t& src, mp_bitcnt_t bitcnt2);
void u_mpz_and_ui2(mpz_t& dst, unsigned long val1, mp_bitcnt_t bitcnt1, unsigned long val2, mp_bitcnt_t bitcnt2);
void u_mpz_ior(mpz_t& dst, mpz_t& src1, mp_bitcnt_t bitcnt1, mpz_t& src2, mp_bitcnt_t bitcnt2);
void u_mpz_ior_ui_r(mpz_t& dst, mpz_t& src, unsigned long val, mp_bitcnt_t bitcnt);
void u_mpz_ior_ui_l(mpz_t& dst, unsigned long val, mp_bitcnt_t bitcnt1, mpz_t& src, mp_bitcnt_t bitcnt2);
void u_mpz_ior_ui2(mpz_t& dst, unsigned long val1, mp_bitcnt_t bitcnt1, unsigned long val2, mp_bitcnt_t bitcnt2);
void u_mpz_xor(mpz_t& dst, mpz_t& src1, mp_bitcnt_t bitcnt1, mpz_t& src2, mp_bitcnt_t bitcnt2);
void u_mpz_xor_ui_r(mpz_t& dst, mpz_t& src, unsigned long val, mp_bitcnt_t bitcnt);
void u_mpz_xor_ui_l(mpz_t& dst, unsigned long val, mp_bitcnt_t bitcnt1, mpz_t& src, mp_bitcnt_t bitcnt2);
void u_mpz_xor_ui2(mpz_t& dst, unsigned long val1, mp_bitcnt_t bitcnt1, unsigned long val2, mp_bitcnt_t bitcnt2);
//shift
void u_mpz_dshl(mpz_t& dst, mpz_t& src1, mp_bitcnt_t bitcnt1, mpz_t& src2, mp_bitcnt_t bitcnt2);
void s_mpz_dshr(mpz_t& dst, mpz_t& src1, mp_bitcnt_t bitcnt1, mpz_t& src2, mp_bitcnt_t bitcnt2);
void u_mpz_dshl_ui_r(mpz_t& dst, mpz_t& src, unsigned long val, mp_bitcnt_t bitcnt);
void u_mpz_dshl_ui_l(mpz_t& dst, unsigned long val, mp_bitcnt_t bitcnt1, mpz_t& src, mp_bitcnt_t bitcnt2);
void u_mpz_dshl_ui2(mpz_t& dst, unsigned long val1, mp_bitcnt_t bitcnt1, unsigned long val2, mp_bitcnt_t bitcnt2);
void u_mpz_dshr(mpz_t& dst, mpz_t& src1, mp_bitcnt_t bitcnt1, mpz_t& src2, mp_bitcnt_t bitcnt2);
void u_mpz_dshr_ui_r(mpz_t& dst, mpz_t& src, unsigned long val, mp_bitcnt_t bitcnt);
void u_mpz_dshr_ui_l(mpz_t& dst, unsigned long val, mp_bitcnt_t bitcnt1, mpz_t& src, mp_bitcnt_t bitcnt2);
void u_mpz_dshr_ui2(mpz_t& dst, unsigned long val1, mp_bitcnt_t bitcnt1, unsigned long val2, mp_bitcnt_t bitcnt2);
//bits
void u_orr(mpz_t& dst, mpz_t& src, mp_bitcnt_t bitcnt);
void u_andr(mpz_t& dst, mpz_t& src, mp_bitcnt_t bitcnt);
void u_not(mpz_t& dst, mpz_t& src, mp_bitcnt_t bitcnt);
void u_cvt(mpz_t& dst, mpz_t& src, mp_bitcnt_t bitcnt);
void u_neg(mpz_t& dst, mpz_t& src, mp_bitcnt_t bitcnt);
void u_shl(mpz_t& dst, mpz_t& src, unsigned long n);
void u_shr(mpz_t& dst, mpz_t& src, unsigned long n);
#endif
