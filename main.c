#include "trace.h"
#include <stdio.h>

void sub1(int a)
{
    printf("sub1 with %d\n",a);
}

void sub2(int a)
{
    printf("sub2\n");
    if (a-->0)
    {
        sub1(a);
        sub2(a);
    }
}

int main(int argc,char **argv)
{
    printf("main called with %d arguments\n",argc-1);
    if (argc==9)
        return 1;
    /* everything before won't have any function resolution */
    load_symbols(argv[0]);
    /* including the previous line */
    /* differents calls follow who will be resolved, as symbols are loaded */
    sub1(argc);
    sub2(3);
    sub1(9);
    sub2(1);
    /* including main off course */
    main(9,NULL);
    return 0;
}
