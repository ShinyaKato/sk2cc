prog="$(cat -)"

echo "-- assemble"
echo "$prog" | gcc -std=c11 -pedantic-errors -fno-asynchronous-unwind-tables -S -xc - -o - || exit 1
echo ""

echo "-- execution"
echo "$prog" | gcc -std=c11 -pedantic-errors -xc - -o tmp/a.out || exit 1
./tmp/a.out
echo "(exit status: $?)"
