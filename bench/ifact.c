#include <stdio.h>
#include <gmp.h>

void ifact(mpz_t res, mpz_t n) {
  mpz_t count;
  mpz_init_set(count, n);
  mpz_set_si(res, 1);
  while(mpz_cmp_si(count, 2) >= 0) {
    mpz_mul(res, res, count);
    mpz_sub_ui(count, count, 1);
  }
  mpz_clear(count);
}

void do_ifact(signed long int xa) {
    mpz_t a, res;

    mpz_init_set_si(a, xa);
    mpz_init(res);
    ifact(res, a);
    mpz_clear(a);
    mpz_out_str(stdout, 10, res);
    putchar('\n');
    mpz_clear(res);
}

int main(void) {
  do_ifact(20000);
  exit(0);
}
