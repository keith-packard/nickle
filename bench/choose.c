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

void choose(mpz_t res, mpz_t n, mpz_t r) {
  mpz_t ifr, ifnr, diff;
  mpz_init(ifr);
  mpz_init(ifnr);
  mpz_init_set(diff, n);
  mpz_sub(diff, diff, r);
  ifact(res, n);
  ifact(ifr, r);
  ifact(ifnr, diff);
  mpz_mul(ifnr, ifnr, ifr);
  mpz_fdiv_q(res, res, ifnr);
  mpz_clear(ifr);
  mpz_clear(ifnr);
  mpz_clear(diff);
}

void do_choose(signed long int xa1, signed long int xa2) {
    mpz_t a1, a2, res;

    mpz_init_set_si(a1, xa1);
    mpz_init_set_si(a2, xa2);
    mpz_init(res);
    choose(res, a1, a2);
    mpz_clear(a1);
    mpz_clear(a2);
    mpz_out_str(stdout, 10, res);
    putchar('\n');
    mpz_clear(res);
}

int main(void) {
  do_choose(20000, 5000);
  exit(0);
}
