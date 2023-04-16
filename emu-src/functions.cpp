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
void s_cat(mpz_t& dst, mpz_t& src1, mpz_t& src2) {
  mp_bitcnt_t src2_bits = mpz_sizeinbase(src2, 2);
  mpz_mul_2exp(dst, src1, src2_bits);
  mpz_ior(dst, dst, src2); 
}
//s_asSInt
void s_asSInt(mpz_t& dst, mpz_t& src) {
  mp_bitcnt_t shiftcnt;
  assert((shiftcnt = mpz_sizeinbase(src, 2)) <= 64);
  if(shiftcnt == 0) {
    mpz_set(dst, src);
    return;
  }
  long data = (long)mpz_get_ui(src);
  shiftcnt = 64 - shiftcnt;
  data = (data << shiftcnt) >> shiftcnt;
  mpz_set_si(dst, data);
}
//s_bits
void s_bits(mpz_t& dst, mpz_t& src, mp_bitcnt_t l, mp_bitcnt_t r) {
  mp_bitcnt_t left_bits = mpz_sizeinbase(src, 2) - r;
  mpz_tdiv_q_2exp(dst, src, r);
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