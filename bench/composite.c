#include <stdio.h>
#include <gmp.h>

/* Miller-Rabin test from Corwin/Rivest/Leiserson */
void witnessexp(mpz_t res, mpz_t b, mpz_t e, mpz_t m) {
  mpz_t xres, tmp, rem, m1;
  if (mpz_cmp_si(e, 0) == 0) {
    mpz_set_si (res, -1);
    return;
  }
  if (mpz_cmp_si(e, 1) == 0) {
    mpz_fdiv_r (res, b, m);
    return;
  }
  mpz_init(tmp);
  mpz_init(rem);
  mpz_fdiv_qr_ui(tmp, rem, e, 2);
  mpz_init(xres);
  witnessexp(xres, b, tmp, m);
  if (mpz_cmp_si(xres, -1) == 0) {
    mpz_set(res, xres);
    mpz_clear(tmp);
    mpz_clear(rem);
    mpz_clear(xres);
    return;
  }
  mpz_mul(tmp, xres, xres);
  mpz_fdiv_r(tmp, tmp, m);
  mpz_init(m1);
  mpz_sub_ui(m1, m, 1);
  if (mpz_cmp_si(tmp, 1) == 0 &&
      mpz_cmp_si(xres, 1) != 0 &&
      mpz_cmp(xres, m1) != 0) {
    mpz_set_si(res, -1);
    mpz_clear(tmp);
    mpz_clear(rem);
    mpz_clear(xres);
    mpz_clear(m1);
    return;
  }
  if (mpz_cmp_si(rem, 0) == 0) {
    mpz_set(res, tmp);
    mpz_clear(tmp);
    mpz_clear(rem);
    mpz_clear(xres);
    mpz_clear(m1);
    return;
  }
  mpz_mul(tmp, tmp, b);
  mpz_fdiv_r(res, tmp, m);
  mpz_clear(tmp);
  mpz_clear(rem);
  mpz_clear(xres);
  mpz_clear(m1);
}

/* Note that rather than trying random
   bases, we try _all_ bases[!]... */
/* ([!] Don't even _think_ it.) */
void composite(mpz_t res, mpz_t n) {
  mpz_t n1, j;
  mpz_init(n1);
  mpz_sub_ui(n1, n, 1);
  for (mpz_init_set_si(j, 1);
       mpz_cmp(j, n) < 0;
       mpz_add_ui(j, j, 1)) {
    mpz_t xres;
    mpz_init(xres);
    witnessexp(xres, j, n1, n);
    if (mpz_cmp_si(xres, 1) != 0) {
      mpz_set(res, j);
      mpz_clear(n1);
      mpz_clear(j);
      mpz_clear(xres);
      return;
    }
    mpz_clear(xres);
  }
  mpz_set_si(res, 0);
  mpz_clear(n1);
  mpz_clear(j);
  return;
}

void do_composite(signed long int xa) {
    mpz_t a, res;

    mpz_init_set_si(a, xa);
    mpz_init(res);
    composite(res, a);
    mpz_clear(a);
    mpz_out_str(stdout, 10, res);
    putchar('\n');
    mpz_clear(res);
}

int main(void) {
  do_composite(39157);
  exit(0);
}
