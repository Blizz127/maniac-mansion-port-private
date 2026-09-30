/* Exercises retail Save/Load UI in disposable core memory; no battery file I/O. */
#define main native_fixture_test_main
#include "test_party_native.c"
#undef main
static unsigned saves,loads;
static void save_load_observer(nes_t *core,void *state){
    if(core->cpu.pc==0xc429){unsigned slot=readn(core,0x6314);if(slot<20&&readn(core,0x63a1+slot)==163&&(readn(core,0x638d+slot)&63)==2){unsigned ptr=readn(core,0x7ede)|(readn(core,0x7edf)<<8),base=readn(core,0x6ca9)|(readn(core,0x6caa)<<8);unsigned offset=(ptr-base)&65535u;if(offset==0x63)saves++;if(offset==0x77)loads++;}}
    party_exec(core,state);
}
static void click_at(unsigned x,unsigned y){CHECK(native_seek((int)x,(int)y));native_wait(20);for(unsigned i=0;i<4;i++)native_step(PAD_A);native_wait(200);}
static void menu(void){for(unsigned i=0;i<4;i++)native_step(PAD_START);native_wait(180);CHECK(readn(&n,0x63c6)==50);}
int main(int argc,char**argv){if(argc!=3)return 2;char error[512];if(!cart_load(&cart,argv[1],false,error,sizeof error))return 2;for(unsigned i=0;i<8;i++){unsigned v;sscanf(cart.prg_sha256+2*i,"%2x",&v);romid[i]=(uint8_t)v;}CHECK(load(&n,argv[2]));party_state_t p;party_init(&p);known_history(&p,1,6);party_update(&p,true,true,true);n.exec_hook=save_load_observer;n.exec_hook_ud=&p;
menu();click_at(168,103);CHECK(saves==1&&loads==0);CHECK(readn(&n,0x63c6)==44);CHECK(p.history.valid&&!p.load_observed);
menu();click_at(168,123);native_wait(200);CHECK(saves==1&&loads==1);CHECK(readn(&n,0x63c6)==44);CHECK(!p.history.valid&&p.load_observed);CHECK(party_check(&n,&p,6,2)==PARTY_HISTORY_UNKNOWN);memcpy(&plain,&n,sizeof plain);CHECK(party_request(&n,&p,6,2)==PARTY_HISTORY_UNKNOWN);CHECK(same(&n,&plain));CHECK(!p.pending&&!p.executing);printf("native Save163:63=%u Load163:77=%u history-invalid/replacement-denied checks%u failures%u\n",saves,loads,checks,failures);party_destroy(&p);cart_free(&cart);return failures?1:0;}
