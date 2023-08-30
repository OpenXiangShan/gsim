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
unsigned long ui_tail(mpz_t& src, mp_bitcnt_t bitcnt, unsigned long n);
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
void s_asSInt_ui(mpz_t& dst, unsigned long src, mp_bitcnt_t bitcnt);
void u_asUInt(mpz_t& dst, mpz_t& src, mp_bitcnt_t bitcnt);
void u_asClock(mpz_t& dst, mpz_t& src, mp_bitcnt_t bitcnt);
void u_bits(mpz_t& dst, mpz_t& src, mp_bitcnt_t bitcnt, mp_bitcnt_t l, mp_bitcnt_t r);
unsigned long u_bits_basic(mpz_t& src, mp_bitcnt_t bitcnt, int h, int l);
void u_pad(mpz_t& dst, mpz_t& src, mp_bitcnt_t bitcnt, mp_bitcnt_t n);
void u_pad_ui(mpz_t& dst, unsigned long src, mp_bitcnt_t n);
void s_pad(mpz_t& dst, mpz_t& src, mp_bitcnt_t bitcnt, mp_bitcnt_t n);
//arithmetic
void u_add(mpz_t& dst, mpz_t& src1, mp_bitcnt_t bitcnt1, mpz_t& src2, mp_bitcnt_t bitcnt2);
void u_add_ui_l(mpz_t& dst, unsigned long val, mp_bitcnt_t bitcnt1, mpz_t& src, mp_bitcnt_t bitcnt2);
void u_add_ui_r(mpz_t& dst, mpz_t& src, unsigned long val, mp_bitcnt_t bitcnt);
void u_add_si_r(mpz_t& dst, mpz_t& src, long val, mp_bitcnt_t bitcnt);
void u_add_ui2(mpz_t& dst, unsigned long val1, mp_bitcnt_t bitcnt1, unsigned long val2, mp_bitcnt_t bitcnt2);
void u_sub(mpz_t& dst, mpz_t& src1, mp_bitcnt_t bitcnt1, mpz_t& src2, mp_bitcnt_t bitcnt2);
void u_sub_ui_l(mpz_t& dst, unsigned long val, mp_bitcnt_t bitcnt1, mpz_t& src, mp_bitcnt_t bitcnt2);
void u_sub_ui_r(mpz_t& dst, mpz_t& src, unsigned long val, mp_bitcnt_t bitcnt);
void u_sub_ui2(mpz_t& dst, unsigned long val1, mp_bitcnt_t bitcnt1, unsigned long val2, mp_bitcnt_t bitcnt2);
void u_mul(mpz_t& dst, mpz_t& src1, mp_bitcnt_t bitcnt1, mpz_t& src2, mp_bitcnt_t bitcnt2);
void u_mul_ui_l(mpz_t& dst, unsigned long val, mp_bitcnt_t bitcnt1, mpz_t& src, mp_bitcnt_t bitcnt2);
void u_mul_si_l(mpz_t& dst, long val, mp_bitcnt_t bitcnt1, mpz_t& src, mp_bitcnt_t bitcnt2);
void u_mul_ui_r(mpz_t& dst, mpz_t& src, unsigned long val, mp_bitcnt_t bitcnt);
void u_mul_si_r(mpz_t& dst, mpz_t& src, long val, mp_bitcnt_t bitcnt);
void u_mul_ui2(mpz_t& dst, unsigned long val1, mp_bitcnt_t bitcnt1, unsigned long val2, mp_bitcnt_t bitcnt2);
void u_div(mpz_t& dst, mpz_t& src1, mp_bitcnt_t bitcnt1, mpz_t& src2, mp_bitcnt_t bitcnt2);
void u_div_ui_l(mpz_t& dst, unsigned long val, mp_bitcnt_t bitcnt1, mpz_t& src, mp_bitcnt_t bitcnt2);
void u_div_ui_r(mpz_t& dst, mpz_t& src, unsigned long val, mp_bitcnt_t bitcnt);
void u_div_ui2(mpz_t& dst, unsigned long val1, mp_bitcnt_t bitcnt1, unsigned long val2, mp_bitcnt_t bitcnt2);
void u_rem(mpz_t& dst, mpz_t& src1, mp_bitcnt_t bitcnt1, mpz_t& src2, mp_bitcnt_t bitcnt2);
void u_rem_ui_l(mpz_t& dst, unsigned long val, mp_bitcnt_t bitcnt1, mpz_t& src, mp_bitcnt_t bitcnt2);
void u_rem_ui_r(mpz_t& dst, mpz_t& src, unsigned long val, mp_bitcnt_t bitcnt);
void u_rem_ui2(mpz_t& dst, unsigned long val1, mp_bitcnt_t bitcnt1, unsigned long val2, mp_bitcnt_t bitcnt2);
//comparison
void u_lt(mpz_t& dst, mpz_t& op1, mp_bitcnt_t bitcnt1, mpz_t& op2, mp_bitcnt_t bitcnt2);
void u_lt_ui_r(mpz_t& dst, mpz_t& src, unsigned long val, mp_bitcnt_t bitcnt);
void u_lt_ui_l(mpz_t& dst, unsigned long val, mp_bitcnt_t bitcnt1, mpz_t& src, mp_bitcnt_t bitcnt2);
void u_lt_ui2(mpz_t& dst, unsigned long val1, mp_bitcnt_t bitcnt1, unsigned long val2, mp_bitcnt_t bitcnt2);
void u_leq(mpz_t& dst, mpz_t& op1, mp_bitcnt_t bitcnt1, mpz_t& op2, mp_bitcnt_t bitcnt2);
void u_leq_ui_r(mpz_t& dst, mpz_t& src, unsigned long val, mp_bitcnt_t bitcnt);
void u_leq_ui_l(mpz_t& dst, unsigned long val, mp_bitcnt_t bitcnt1, mpz_t& src, mp_bitcnt_t bitcnt2);
void u_leq_ui2(mpz_t& dst, unsigned long val1, mp_bitcnt_t bitcnt1, unsigned long val2, mp_bitcnt_t bitcnt2);
void u_gt(mpz_t& dst, mpz_t& op1, mp_bitcnt_t bitcnt1, mpz_t& op2, mp_bitcnt_t bitcnt2);
void u_gt_ui_r(mpz_t& dst, mpz_t& src, unsigned long val, mp_bitcnt_t bitcnt);
void u_gt_ui_l(mpz_t& dst, unsigned long val, mp_bitcnt_t bitcnt1, mpz_t& src, mp_bitcnt_t bitcnt2);
void u_gt_ui2(mpz_t& dst, unsigned long val1, mp_bitcnt_t bitcnt1, unsigned long val2, mp_bitcnt_t bitcnt2);
void u_geq(mpz_t& dst, mpz_t& op1, mp_bitcnt_t bitcnt1, mpz_t& op2, mp_bitcnt_t bitcnt2);
bool ui_geq(mpz_t& op1, mp_bitcnt_t bitcnt1, mpz_t& op2, mp_bitcnt_t bitcnt2);
void u_geq_ui_r(mpz_t& dst, mpz_t& src, unsigned long val, mp_bitcnt_t bitcnt);
void u_geq_ui_l(mpz_t& dst, unsigned long val, mp_bitcnt_t bitcnt1, mpz_t& src, mp_bitcnt_t bitcnt2);
void u_geq_ui2(mpz_t& dst, unsigned long val1, mp_bitcnt_t bitcnt1, unsigned long val2, mp_bitcnt_t bitcnt2);
bool ui_eq(mpz_t& op1, mp_bitcnt_t bitcnt1, mpz_t& op2, mp_bitcnt_t bitcnt2);
void u_eq(mpz_t& dst, mpz_t& op1, mp_bitcnt_t bitcnt1, mpz_t& op2, mp_bitcnt_t bitcnt2);
void u_eq_ui_r(mpz_t& dst, mpz_t& src, unsigned long val, mp_bitcnt_t bitcnt);
void u_eq_ui_l(mpz_t& dst, unsigned long val, mp_bitcnt_t bitcnt1, mpz_t& src, mp_bitcnt_t bitcnt2);
void u_eq_ui2(mpz_t& dst, unsigned long val1, mp_bitcnt_t bitcnt1, unsigned long val2, mp_bitcnt_t bitcnt2);
bool ui_neq(mpz_t& op1, mp_bitcnt_t bitcnt1, mpz_t& op2, mp_bitcnt_t bitcnt2);
void u_neq(mpz_t& dst, mpz_t& op1, mp_bitcnt_t bitcnt1, mpz_t& op2, mp_bitcnt_t bitcnt2);
void u_neq_ui_r(mpz_t& dst, mpz_t& src, unsigned long val, mp_bitcnt_t bitcnt);
void u_neq_ui_l(mpz_t& dst, unsigned long val, mp_bitcnt_t bitcnt1, mpz_t& src, mp_bitcnt_t bitcnt2);
void u_neq_ui2(mpz_t& dst, unsigned long val1, mp_bitcnt_t bitcnt1, unsigned long val2, mp_bitcnt_t bitcnt2);
//logical
void u_and(mpz_t& dst, mpz_t& src1, mp_bitcnt_t bitcnt1, mpz_t& src2, mp_bitcnt_t bitcnt2);
void u_and_ui_r(mpz_t& dst, mpz_t& src, unsigned long val, mp_bitcnt_t bitcnt);
void u_and_ui_l(mpz_t& dst, unsigned long val, mp_bitcnt_t bitcnt1, mpz_t& src, mp_bitcnt_t bitcnt2);
void u_and_ui2(mpz_t& dst, unsigned long val1, mp_bitcnt_t bitcnt1, unsigned long val2, mp_bitcnt_t bitcnt2);
void u_or(mpz_t& dst, mpz_t& src1, mp_bitcnt_t bitcnt1, mpz_t& src2, mp_bitcnt_t bitcnt2);
void u_or_ui_r(mpz_t& dst, mpz_t& src, unsigned long val, mp_bitcnt_t bitcnt);
void u_or_ui_l(mpz_t& dst, unsigned long val, mp_bitcnt_t bitcnt1, mpz_t& src, mp_bitcnt_t bitcnt2);
void u_or_ui2(mpz_t& dst, unsigned long val1, mp_bitcnt_t bitcnt1, unsigned long val2, mp_bitcnt_t bitcnt2);
void u_xor(mpz_t& dst, mpz_t& src1, mp_bitcnt_t bitcnt1, mpz_t& src2, mp_bitcnt_t bitcnt2);
void u_xor_ui_r(mpz_t& dst, mpz_t& src, unsigned long val, mp_bitcnt_t bitcnt);
void u_xor_ui_l(mpz_t& dst, unsigned long val, mp_bitcnt_t bitcnt1, mpz_t& src, mp_bitcnt_t bitcnt2);
void u_xor_ui2(mpz_t& dst, unsigned long val1, mp_bitcnt_t bitcnt1, unsigned long val2, mp_bitcnt_t bitcnt2);
//shift
void u_dshl(mpz_t& dst, mpz_t& src1, mp_bitcnt_t bitcnt1, mpz_t& src2, mp_bitcnt_t bitcnt2);
void s_dshr(mpz_t& dst, mpz_t& src1, mp_bitcnt_t bitcnt1, mpz_t& src2, mp_bitcnt_t bitcnt2);
void u_dshl_ui_r(mpz_t& dst, mpz_t& src, unsigned long val, mp_bitcnt_t bitcnt);
void u_dshl_ui_l(mpz_t& dst, unsigned long val, mp_bitcnt_t bitcnt1, mpz_t& src, mp_bitcnt_t bitcnt2);
void u_dshl_ui2(mpz_t& dst, unsigned long val1, mp_bitcnt_t bitcnt1, unsigned long val2, mp_bitcnt_t bitcnt2);
void u_dshr(mpz_t& dst, mpz_t& src1, mp_bitcnt_t bitcnt1, mpz_t& src2, mp_bitcnt_t bitcnt2);
void u_dshr_ui_r(mpz_t& dst, mpz_t& src, unsigned long val, mp_bitcnt_t bitcnt);
void u_dshr_ui_l(mpz_t& dst, unsigned long val, mp_bitcnt_t bitcnt1, mpz_t& src, mp_bitcnt_t bitcnt2);
void u_dshr_ui2(mpz_t& dst, unsigned long val1, mp_bitcnt_t bitcnt1, unsigned long val2, mp_bitcnt_t bitcnt2);
//bits
void u_orr(mpz_t& dst, mpz_t& src, mp_bitcnt_t bitcnt);
void u_andr(mpz_t& dst, mpz_t& src, mp_bitcnt_t bitcnt);
void u_xorr(mpz_t& dst, mpz_t& src, mp_bitcnt_t bitcnt);
void u_not(mpz_t& dst, mpz_t& src, mp_bitcnt_t bitcnt);
void u_not_ui(mpz_t& dst, unsigned long src, mp_bitcnt_t bitcnt);
void u_cvt(mpz_t& dst, mpz_t& src, mp_bitcnt_t bitcnt);
void s_cvt(mpz_t& dst, mpz_t& src, mp_bitcnt_t bitcnt);
void s_cvt_ui(mpz_t&dst, unsigned long src, mp_bitcnt_t bitcnt);
void u_neg(mpz_t& dst, mpz_t& src, mp_bitcnt_t bitcnt);
void u_shl(mpz_t& dst, mpz_t& src, unsigned long n);
void u_shr(mpz_t& dst, mpz_t& src, unsigned long n);

void init_functions();

#endif
