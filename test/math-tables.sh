#!/bin/sh

case "$1" in
    "-o")
	shift
	output="$1"
	shift
	exec > $output
	;;
esac

dir=`dirname $0`

# Which tables to generate

make_sin=y
make_asin=y
make_tan=y
make_atan=y
make_log=y
make_exp=y
make_jn=y
make_yn=y

if [ $# -gt 0 ]; then
    make_sin=n
    make_asin=n
    make_tan=n
    make_atan=n
    make_log=n
    make_exp=n
    make_jn=n
    make_yn=n
    for i in "$@"; do
	case "$i" in
	    sin)
		make_sin=y
		;;
	    asin)
		make_asin=y
		;;
	    tan)
		make_tan=y
		;;
	    atan)
		make_atan=y
		;;
	    log)
		make_log=y
		;;
	    exp)
		make_exp=y
		;;
	    jn)
		make_jn=y;
		;;
	    yn)
		make_yn=y;
		;;
	esac
    done
fi

inc=11
ainc=11

# sine and cosine table

echo "typedef struct { real angle, sin, cos; } sin_cos_t;"
echo "sin_cos_t[] sin_cos_table = {"

a="-800"
while [ "$a" -le "800" -a $make_sin = "y" ]; do
    sin=`echo "s($a / 100)" | bc -l "$dir"/math-funcs.bc | fmt -w 500 | tr -d '\\\n ' `
    cos=`echo "c($a / 100)" | bc -l "$dir"/math-funcs.bc | fmt -w 500 | tr -d '\\\n ' `
    psin=`echo "s(p * $a / 100)" | bc -l "$dir"/math-funcs.bc | fmt -w 500 | tr -d '\\\n ' `
    pcos=`echo "c(p * $a / 100)" | bc -l "$dir"/math-funcs.bc | fmt -w 500 | tr -d '\\\n ' `
    echo "    { .angle = $a / 100.0,"
    echo "      .sin = $sin,"
    echo "      .cos = $cos },"
    echo "    { .angle = π * $a / 100.0,"
    echo "      .sin = $psin,"
    echo "      .cos = $pcos },"
    a=`expr "$a" + "$inc"`
done

echo "};"

# arcsine and arccosine table

echo "typedef struct { real ratio, asin, acos; } asin_acos_t;"
echo "asin_acos_t[] asin_acos_table = {"

r="-200"
while [ "$r" -le 200 -a $make_asin = "y" ]; do
    asin=`echo "b($r / 100)" | bc -l "$dir"/math-funcs.bc | fmt -w 500 | tr -d '\\\n ' `
    acos=`echo "d($r / 100)" | bc -l "$dir"/math-funcs.bc | fmt -w 500 | tr -d '\\\n ' `
    echo "    { .ratio = $r / 100,"
    echo "      .asin = $asin,"
    echo "      .acos = $acos },"
    r=`expr "$r" + "$inc"`
done

echo "};"

# tangent table

echo "typedef struct { real angle, tan; } tan_t;"
echo "tan_t[] tan_table = {"

a="-800"
while [ "$a" -le 800 -a $make_tan = "y" ]; do
    tan=`echo "t($a / 100)" | bc -l "$dir"/math-funcs.bc | fmt -w 500 | tr -d '\\\n ' `
    ptan=`echo "t(p * $a / 100)" | bc -l "$dir"/math-funcs.bc | fmt -w 500 | tr -d '\\\n ' `
    echo "    { .angle = $a / 100.0,"
    echo "      .tan = $tan },"
    echo "    { .angle = π * $a / 100.0,"
    echo "      .tan = $ptan },"
    a=`expr "$a" + "$inc"`
done
echo "};"

# arctangent table

echo "typedef struct { real ratio, atan; } atan_t;"
echo "atan_t[] atan_table = {"

r="-1000"
while [ "$r" -le 1000 -a $make_atan = "y" ]; do
    atan=`echo "a($r / 100)" | bc -l "$dir"/math-funcs.bc | fmt -w 500 | tr -d '\\\n ' `
    echo "    { .ratio = $r / 100,"
    echo "      .atan = $atan },"
    r=`expr "$r" + "$inc"`
done
echo "};"

# arctangent(y/x) table

echo "typedef struct { real y, x, atan2; } atan2_t;"
echo "atan2_t[] atan2_table = {"

y="-30"
while [ "$y" -le 30 -a "$make_atan" = "y" ]; do
      x="-30"
      while [ "$x" -le 30 ]; do
	  x=`expr "$x" + "$ainc"`
	  atan2=`echo "u($y / 100, $x / 100)" | bc -l "$dir"/math-funcs.bc | fmt -w 500 | tr -d '\\\n ' `
	  echo "    { .y = $y / 100.0, .x = $x / 100.0,"
	  echo "      .atan2 = $atan2 },"
      done
      y=`expr "$y" + "$ainc"`
done
echo "};"

# log table

echo "typedef struct { real in, log; } log_t;"
echo "log_t[] log_table = {"

r="0"
while [ "$r" -le 66 -a "$make_log" = "y" ]; do
    in=`echo "2 ^ $r" | bc`
    log=`echo "l($in / 1000000)" | bc -l "$dir"/math-funcs.bc | fmt -w 500 | tr -d '\\\n ' `
    echo "    { .in = $in / 1000000.0,"
    echo "      .log = $log },"
    r=`expr "$r" + "$ainc"`
done
echo "};"

# exp table

echo "typedef struct { real in, exp; } exp_t;"
echo "exp_t[] exp_table = {"

r="-1000"
while [ "$r" -le 1000 -a $make_exp = "y" ]; do
    exp=`echo "e($r / 100)" | bc -l "$dir"/math-funcs.bc | fmt -w 500 | tr -d '\\\n ' `
    echo "    { .in = $r / 100.0,"
    echo "      .exp = $exp },"
    r=`expr "$r" + "$inc"`
done
echo "};"

# jn table

echo "typedef struct { int n; real in, jn; } jn_t;"
echo "jn_t[] jn_table = {"

n="-5"
r="-1000"
while [ "$n" -le 5 -a $make_jn = "y" ]; do
    exp=`echo "q($n, $r / 100)" | bc -l "$dir"/math-funcs.bc | fmt -w 500 | tr -d '\\\n ' `
    echo "    { .n = $n, .in = $r / 100.0,"
    echo "      .jn = $exp },"
    r=`expr "$r" + "$inc" '*' 4`
    if [ "$r" -gt 1000 ]; then
	n=`expr "$n" + 1`
	r=-1000
    fi
done

# Cases covering each path in Math::jn at the 333-bit test
# precision: x == 0; heavy cancellation in the power series;
# asymptotic series tried but diverging (about 119 < x < 139);
# asymptotic series skipped because n is too large; asymptotic
# series used, including negative n and negative x

if [ $make_jn = "y" ]; then
    for p in 0:0 3:0 2:60.25 0:125.5 4:130.25 20:150.5 0:150.5 1:333.25 \
	     5:500.25 -2:750.5 1:-200.75 3:-1000.5 1:2000.25; do
	n=${p%%:*}
	x=${p#*:}
	exp=`echo "q($n, $x)" | bc -l "$dir"/math-funcs.bc | fmt -w 500 | tr -d '\\\n ' `
	echo "    { .n = $n, .in = $x,"
	echo "      .jn = $exp },"
    done
fi
echo "};"

# yn table

echo "typedef struct { int n; real in, yn; } yn_t;"
echo "yn_t[] yn_table = {"

n="-5"
r="44"
while [ "$n" -le 5 -a $make_yn = "y" ]; do
    exp=`echo "y($n, $r / 100)" | bc -l "$dir"/math-funcs.bc | fmt -w 500 | tr -d '\\\n ' `
    echo "    { .n = $n, .in = $r / 100.0,"
    echo "      .yn = $exp },"
    r=`expr "$r" + "$inc" '*' 4`
    if [ "$r" -gt 1000 ]; then
	n=`expr "$n" + 1`
	r=44
    fi
done

# Cases covering each path in Math::yn at the 333-bit test
# precision: x < 0; x == 0; small x; heavy cancellation in the
# power series; asymptotic series tried but diverging (about
# 119 < x < 139); asymptotic series skipped because n is too
# large; asymptotic series used; negative n

if [ $make_yn = "y" ]; then
    for p in 0:-1 1:0 0:0.01 2:0.01 0:60.25 3:100.5 0:125.5 4:130.25 \
	     20:150.5 0:150.5 1:200.75 2:333.25 7:500.25 1:1000.5 \
	     -2:2.5 -3:750.5; do
	n=${p%%:*}
	x=${p#*:}
	exp=`echo "y($n, $x)" | bc -l "$dir"/math-funcs.bc | fmt -w 500 | tr -d '\\\n ' `
	echo "    { .n = $n, .in = $x,"
	echo "      .yn = $exp },"
    done
fi
echo "};"

