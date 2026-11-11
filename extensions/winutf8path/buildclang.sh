clang -c -fpic -O2 ring_winutf8path.c -I $PWD/../../language/include
clang -dynamiclib -o $PWD/../../lib/libring_winutf8path.dylib ring_winutf8path.o -L $PWD/../../lib -lring
rm -f *.o
