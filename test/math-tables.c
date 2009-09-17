#include <math.h>
#include <stdio.h>
#include <stdlib.h>

#define FMT	"%25.17f"

double
print_val(double x)
{
	if (isnan(x))
		return 10000;
	if (x > 1e10)
		return 9999;
	if (x < -1e10)
		return 9999;
	if (fabs(x) < 1e-20)
		return 0;
	return x;
}

static void
sin_cos_table(void)
{
	double	a;
	double	r;

	printf ("/* %20.15f */\n", asin(-1));
	printf ("typedef struct { real angle, sin, cos; } sin_cos_t;\n");
	printf ("sin_cos_t[] sin_cos_table = {\n");
	for (a = -800; a <= 800; a += 1) {
		double a_f = a / 100;
		printf ("\t{ .angle =     " FMT ", .sin = " FMT ", .cos = " FMT " },\n",
			a_f, print_val(sin(a_f)), print_val(cos(a_f)));
		printf ("\t{ .angle = π * " FMT ", .sin = " FMT ", .cos = " FMT " },\n",
			a_f, print_val(sin(M_PI * a_f)), print_val(cos(M_PI * a_f)));
	}
	printf ("};\n");

	printf ("typedef struct { real ratio, asin, acos; } asin_acos_t;\n");
	printf ("asin_acos_t[] asin_acos_table = {\n");
	for (r = -200; r <= 200; r += 1) {
		double r_f = r / 100;
		printf ("\t{ .ratio = " FMT ", .asin = " FMT ", .acos = " FMT " },\n",
			r_f, print_val(asin(r_f)), print_val(acos(r_f)));
	}
	printf ("};\n");
}

static void
tan_table(void)
{
	double	a;
	double	r;
	double	x, y;

	printf ("typedef struct { real angle, tan; } tan_t;\n");
	printf ("tan_t[] tan_table = {\n");
	for (a = -800; a <= 800; a += 1) {
		double a_f = a/100;
		printf ("\t{ .angle =     " FMT ", .tan = " FMT " },\n",
			a_f, print_val(tan(a_f)));
		printf ("\t{ .angle = π * " FMT ", .tan = " FMT " },\n",
			a_f, print_val(tan(M_PI * a_f)));
	}
	printf ("};\n");

	printf ("typedef struct { real ratio, atan; } atan_t;\n");
	printf ("atan_t[] atan_table = {\n");
	for (r = -1000; r <= 1000; r += 1) {
		double r_f = r / 10;
		printf ("\t{ .ratio = " FMT ", .atan = " FMT " },\n",
			r_f, print_val(atan(r_f)));
	}
	printf ("};\n");
	
	printf ("typedef struct { real y, x, atan2; } atan2_t;\n");
	printf ("atan2_t[] atan2_table = {\n");
	for (y = -30; y <= 30; y += 1) {
		double y_f = y / 10;
		for (x = -30; x <= 30; x += 1) {
			double x_f = x/10;
			double v;

			/*
			 * looks like glibc has a bug -- atan2(0,0) returns
			 * π/4 instead of 0
			 */
			if (y_f == 0 && x_f == 0)
				v = 0;
			else
				v = atan2(y_f,x_f);

			printf ("\t{ .y = " FMT ", .x = " FMT ", .atan2 = " FMT " },\n",
				y_f, x_f, print_val(v));
		}
	}
	printf ("};\n");
}

void
log_table(void)
{
	double	v;
	printf ("typedef struct { real in, log; } log_t;\n");
	printf ("log_t[] log_table = {\n");
	for (v = 1; v < 1e20; v *= 2) {
		double v_f = v/1e6;
		printf("\t{ .in = " FMT ", .log = " FMT " },\n",
		       v_f, log(v_f));
	}
	printf ("};\n");
	printf ("typedef struct { real in, exp; } exp_t;\n");
	printf ("exp_t[] exp_table = {\n");
	for (v = -1000; v < 1000; v += 1) {
		double v_f = v/100;
		printf("\t{ .in = " FMT ", .exp = " FMT " },\n",
		       v_f, exp(v_f));
	}
	printf ("};\n");
}

int
main(int argc, char **argv)
{
	sin_cos_table();
	tan_table();
	log_table();
	exit(0);
}
