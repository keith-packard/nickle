#include <stdio.h>
#include <gmp.h>

void rfact(mpz_t res, mpz_t n) {
  mpz_t tmp;
  if (mpz_cmp_si(n, 0) <= 0) {
    mpz_set_si(res, 1);
    return;
  }
  mpz_init_set(tmp, n);
  mpz_sub_ui(tmp, tmp, 1);
  rfact(res, tmp);
  mpz_clear(tmp);
  mpz_mul(res, res, n);
}

void do_rfact(signed long int xa) {
    mpz_t a, res;

    mpz_init_set_si(a, xa);
    mpz_init(res);
    rfact(res, a);
    mpz_clear(a);
    mpz_out_str(stdout, 10, res);
    putchar('\n');
    mpz_clear(res);
}

int main(void) {
  do_rfact(20000);
  exit(0);
}
