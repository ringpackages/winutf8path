gcc -c -fpic -O2 ring_winutf8path.c -I $PWD/../../language/include
gcc -shared -o $PWD/../../lib/libring_winutf8path.so ring_winutf8path.o -L $PWD/../../lib -lring
rm -f *.o
