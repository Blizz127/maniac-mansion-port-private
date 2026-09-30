#include "../src/host/party.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CHECK(x) do { if (!(x)) { fprintf(stderr,"line %d: %s\n",__LINE__,#x); return 1; } } while (0)
int main(void) {
    party_state_t s; party_init(&s);
    uint8_t wire[8];
    CHECK(party_history_encode(&s.history,wire));
    CHECK(wire[0]==1 && wire[1]==0);
    const uint8_t authentic_initial[]={1,1,7,0,7,3,2,1};
    CHECK(party_restore_history(&s,authentic_initial,8));
    CHECK(s.history.valid && s.history.activated_mask==7);
    CHECK(party_history_encode(&s.history,wire));
    CHECK(!memcmp(wire,authentic_initial,8));
    /* A retired, previously issued kid can rejoin without issuing another item. */
    const uint8_t retired[]={1,1,39,2,39,3,2,1};
    CHECK(party_restore_history(&s,retired,8));
    CHECK(s.history.retired_mask==2 && s.history.starter_issued_mask==39);
    const uint8_t malformed[][8]={
        {2,1,7,0,7,3,2,1}, /* unsupported version */
        {1,1,7,4,7,3,2,1}, /* Dave retired */
        {1,1,7,8,7,3,2,1}, /* retirement without activation */
        {1,1,7,0,15,3,2,1}, /* issuance without activation */
        {1,1,7,0,7,3,3,1}, /* duplicate roster */
        {1,1,7,0,7,2,3,1}, /* Dave moved from fixed native slot */
        {1,1,3,0,3,3,2,1}, /* initial actor missing from history */
        {1,0,7,0,7,3,2,1}, /* unknown with fabricated provenance */
        {1,1,135,0,7,3,2,1}, /* outside actor range */
    };
    for(unsigned i=0;i<sizeof(malformed)/sizeof(*malformed);i++) {
        CHECK(!party_restore_history(&s,malformed[i],8));
        CHECK(!s.history.valid && !s.history.activated_mask);
    }
    CHECK(!party_restore_history(&s,authentic_initial,7));
    CHECK(!s.history.valid);
    nes_t *n=calloc(1,sizeof(*n)); CHECK(n);
    nes_t *before=malloc(sizeof(*before)); CHECK(before);
    memcpy(before,n,sizeof(*n));
    party_update(&s,false,true,true);
    CHECK(party_request(n,&s,2,6)==PARTY_ORIGINAL);
    for(unsigned i=0;i<100;i++)party_exec(n,&s);
    CHECK(!memcmp(n,before,sizeof(*n)));
    party_update(&s,true,false,true);
    CHECK(party_request(n,&s,2,6)==PARTY_DISABLED);
    CHECK(!memcmp(n,before,sizeof(*n)));
    party_update(&s,true,true,true);
    CHECK(party_request(n,&s,2,6)==PARTY_HISTORY_UNKNOWN);
    CHECK(!memcmp(n,before,sizeof(*n)));
    for(unsigned r=0;r<=PARTY_NATIVE_FAILED;r++)CHECK(strlen(party_result_short((party_result_t)r))<=28);
    party_destroy(&s); free(before); free(n);
    puts("party history, malformed metadata, Original/OFF immutability: passed");
    return 0;
}
