# trace
allow to trace functions call from a c program without the need to instrument code

Use both /proc/<pid>/maps and ELF symbols to recover virtual address of functions inside the running process and print call stack.
main.c is an exemple of usage. You just need to include trace.h and call load_symbols(argv[0]) . Everything call before won't be translated as function symbols.
trace.c can be tuned (it's slow as it use a single table to store symbols / address mapping) or customised (printf of call stack). Debug symbols could be used for exemple)

To compile, build a proper Makefile (if your project is consequent) or use : gcc -finstrument-functions -o main main.c trace.c -lelf. Don't forget -finstrument-functions to your CFLAGS (the glue for gcc to instrument entries and exits of functions by itself) and lib elf.

Here an exemple of several runs, just to show the variations of program pointer when using different compiler optimisation and recursive call unrolls :

``` shell
 > gcc -finstrument-functions -O0 -o main main.c trace.c -lelf
 > ./main
  + 0x557734e2b58e(noinit) from 0x7fe3e4a2fd90(noinit)
main called with 0 arguments
    + 0x557734e2b64a(noinit) from 0x557734e2b5f0(noinit)
      + 0x557734e2b69f(noinit) from 0x557734e2b67a(noinit)
      + 0x557734e2b91b(noinit) from 0x557734e2b686(noinit)
    + 0x557734e2b4c9(sub1) from 0x557734e2b5fa(main + 108)
sub1 with 1
    + 0x557734e2b520(sub2) from 0x557734e2b604(main + 118)
sub2
      + 0x557734e2b4c9(sub1) from 0x557734e2b56b(sub2 + 75)
sub1 with 2
      + 0x557734e2b520(sub2) from 0x557734e2b575(sub2 + 85)
sub2
        + 0x557734e2b4c9(sub1) from 0x557734e2b56b(sub2 + 75)
sub1 with 1
        + 0x557734e2b520(sub2) from 0x557734e2b575(sub2 + 85)
sub2
          + 0x557734e2b4c9(sub1) from 0x557734e2b56b(sub2 + 75)
sub1 with 0
          + 0x557734e2b520(sub2) from 0x557734e2b575(sub2 + 85)
sub2
    + 0x557734e2b4c9(sub1) from 0x557734e2b60e(main + 128)
sub1 with 9
    + 0x557734e2b520(sub2) from 0x557734e2b618(main + 138)
sub2
      + 0x557734e2b4c9(sub1) from 0x557734e2b56b(sub2 + 75)
sub1 with 0
      + 0x557734e2b520(sub2) from 0x557734e2b575(sub2 + 85)
sub2
    + 0x557734e2b58e(main) from 0x557734e2b627(main + 153)
main called with 8 arguments
```

Here with a shorter code (taking less space)

``` shell
 > gcc -finstrument-functions -O1 -o main main.c trace.c -lelf
 > ./main
  + 0x564d0fd82563(noinit) from 0x7f2b004b0d90(noinit)
main called with 0 arguments
    + 0x564d0fd82d19(noinit) from 0x564d0fd825ae(noinit)
      + 0x564d0fd82771(noinit) from 0x564d0fd82d44(noinit)
      + 0x564d0fd82977(noinit) from 0x564d0fd82d4c(noinit)
    + 0x564d0fd824c9(sub1) from 0x564d0fd825b5(main + 82)
sub1 with 1
    + 0x564d0fd82515(sub2) from 0x564d0fd825bf(main + 92)
sub2
      + 0x564d0fd824c9(sub1) from 0x564d0fd8255a(sub2 + 69)
sub1 with 2
      + 0x564d0fd82515(sub2) from 0x564d0fd82561(sub2 + 76)
sub2
        + 0x564d0fd824c9(sub1) from 0x564d0fd8255a(sub2 + 69)
sub1 with 1
        + 0x564d0fd82515(sub2) from 0x564d0fd82561(sub2 + 76)
sub2
          + 0x564d0fd824c9(sub1) from 0x564d0fd8255a(sub2 + 69)
sub1 with 0
          + 0x564d0fd82515(sub2) from 0x564d0fd82561(sub2 + 76)
sub2
    + 0x564d0fd824c9(sub1) from 0x564d0fd825c9(main + 102)
sub1 with 9
    + 0x564d0fd82515(sub2) from 0x564d0fd825d3(main + 112)
sub2
      + 0x564d0fd824c9(sub1) from 0x564d0fd8255a(sub2 + 69)
sub1 with 0
      + 0x564d0fd82515(sub2) from 0x564d0fd82561(sub2 + 76)
sub2
    + 0x564d0fd82563(main) from 0x564d0fd825e2(main + 127)
main called with 8 arguments
```

And the one with the optimisations from compiler (calls don't always come from where we think)

``` shell
 > gcc -finstrument-functions -O9 -o main main.c trace.c -lelf
 > ./main
  + 0x55d3366be3e0(noinit) from 0x7f16e8946d90(noinit)
main called with 0 arguments
    + 0x55d3366bef70(noinit) from 0x55d3366be433(noinit)
      + 0x55d3366be9b0(noinit) from 0x55d3366bef90(noinit)
      + 0x55d3366bebc0(noinit) from 0x55d3366bef98(noinit)
    + 0x55d3366be570(sub1) from 0x55d3366be43a(main + 90)
sub1 with 1
    + 0x55d3366be5c0(sub2) from 0x55d3366be444(main + 100)
sub2
      + 0x55d3366be570(sub1) from 0x55d3366be444(main + 100)
sub1 with 2
      + 0x55d3366be5c0(sub2) from 0x55d3366be444(main + 100)
sub2
        + 0x55d3366be570(sub1) from 0x55d3366be444(main + 100)
sub1 with 1
        + 0x55d3366be5c0(sub2) from 0x55d3366be444(main + 100)
sub2
          + 0x55d3366be570(sub1) from 0x55d3366be444(main + 100)
sub1 with 0
          + 0x55d3366be5c0(sub2) from 0x55d3366be712(sub2 + 338)
sub2
    + 0x55d3366be570(sub1) from 0x55d3366be44e(main + 110)
sub1 with 9
    + 0x55d3366be5c0(sub2) from 0x55d3366be458(main + 120)
sub2
      + 0x55d3366be570(sub1) from 0x55d3366be458(main + 120)
sub1 with 0
      + 0x55d3366be5c0(sub2) from 0x55d3366be458(main + 120)
sub2
    + 0x55d3366be3e0(main) from 0x55d3366be464(main + 132)
main called with 8 arguments
```

