#!/bin/sh
while read bmname
do
  gcc -O4 -o "$bmname" "$bmname.c" -lgmp > /dev/null 2>&1 || exit 1
  echo "../nickle $bmname.5c"
  echo "bc $bmname.bc"
  echo "./$bmname /dev/null"
done <<'EOF' |
ifact
rfact
choose
composite
EOF
while read lang input
do
  echo "$lang $input"
  for i in 1 2 3 4 5
  do
    echo -n "$i. "
    ( time $lang < $input > /dev/null ) 2>&1
  done
done
