#include "../src/host/restoration.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define CHECK(v) do { if (!(v)) { fprintf(stderr,"line %d: %s\n",__LINE__,#v); return 1; } } while (0)
int main(void) {
    const size_t size=262144;
    uint8_t *prg=malloc(size),*before=malloc(size);CHECK(prg&&before);
    memset(prg,0xa5,size);memcpy(before,prg,size);
    for(unsigned mask=0;mask<4;mask++) {
        restoration_options_t o={(mask&1)!=0,(mask&2)!=0};
        CHECK(restoration_apply(prg,size,false,o)==RESTORATION_OFF);
        CHECK(!memcmp(prg,before,size));
    }
    restoration_options_t off={false,false},on={true,true};
    CHECK(restoration_apply(prg,size,true,off)==RESTORATION_OFF);
    CHECK(!memcmp(prg,before,size));
    CHECK(restoration_apply(prg,size,true,on)==RESTORATION_WRONG_ROM);
    CHECK(!memcmp(prg,before,size));
    CHECK(restoration_apply(prg,size-1,true,on)==RESTORATION_BAD_SIZE);
    CHECK(!memcmp(prg,before,size));
    CHECK(restoration_apply(NULL,size,true,on)==RESTORATION_BAD_SIZE);
    uint8_t a[8],b[8],again[8];restoration_identity(prg,size,a);
    prg[size/2]^=1;restoration_identity(prg,size,b);
    CHECK(memcmp(a,b,8));prg[size/2]^=1;
    restoration_identity(prg,size,again);CHECK(!memcmp(a,again,8));
    free(prg);free(before);puts("restoration Original/OFF, wrong identity/size, profile identity: passed");
    return 0;
}
