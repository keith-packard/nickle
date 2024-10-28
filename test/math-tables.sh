#!/bin/sh

dir=`dirname $0`

# Which tables to generate

make_sin=y
make_asin=y
make_tan=y
make_atan=y
make_log=y
make_exp=y

if [ $# -gt 0 ]; then
    make_sin=n
    make_asin=n
    make_tan=n
    make_atan=n
    make_log=n
    make_exp=n
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
	esac
    done
fi

# sine and cosine table

inc=1
ainc=1

echo "typedef struct { real angle, sin, cos; } sin_cos_t;"
echo "sin_cos_t[] sin_cos_table = {"

a="-800"
while [ "$a" -le "800" -a $make_sin = "y" ]; do
    sin=`echo "s($a / 100)" | bc -l "$dir"/math-funcs.bc | fmt --width=500 | tr -d '\\\n ' `
    cos=`echo "c($a / 100)" | bc -l "$dir"/math-funcs.bc | fmt --width=500 | tr -d '\\\n ' `
    psin=`echo "s(p * $a / 100)" | bc -l "$dir"/math-funcs.bc | fmt --width=500 | tr -d '\\\n ' `
    pcos=`echo "c(p * $a / 100)" | bc -l "$dir"/math-funcs.bc | fmt --width=500 | tr -d '\\\n ' `
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
    asin=`echo "b($r / 100)" | bc -l "$dir"/math-funcs.bc | fmt --width=500 | tr -d '\\\n ' `
    acos=`echo "d($r / 100)" | bc -l "$dir"/math-funcs.bc | fmt --width=500 | tr -d '\\\n ' `
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
    tan=`echo "t($a / 100)" | bc -l "$dir"/math-funcs.bc | fmt --width=500 | tr -d '\\\n ' `
    ptan=`echo "t(p * $a / 100)" | bc -l "$dir"/math-funcs.bc | fmt --width=500 | tr -d '\\\n ' `
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
    atan=`echo "a($r / 100)" | bc -l "$dir"/math-funcs.bc | fmt --width=500 | tr -d '\\\n ' `
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
	  atan2=`echo "u($y / 100, $x / 100)" | bc -l "$dir"/math-funcs.bc | fmt --width=500 | tr -d '\\\n ' `
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
    log=`echo "l($in / 1000000)" | bc -l "$dir"/math-funcs.bc | fmt --width=500 | tr -d '\\\n ' `
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
    exp=`echo "e($r / 100)" | bc -l "$dir"/math-funcs.bc | fmt --width=500 | tr -d '\\\n ' `
    echo "    { .in = $r / 100.0,"
    echo "      .exp = $exp },"
    r=`expr "$r" + "$inc"`
done
echo "};"
