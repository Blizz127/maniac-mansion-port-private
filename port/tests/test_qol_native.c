/* Actual owner-local ROM differential test. Usage: test-qol-native ROM FIELD_STATE... [--cutscene STATE]
 * States must match the core snapshot ABI; incompatible states fail explicitly.
 * Runs the native object hit-test in disposable diagnostic clones with a
 * synthetic return and disabled interrupts. This verifies the routine ABI;
 * it is not a full gameplay/chase/script-equivalence test. Never package inputs.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../src/core/cart.h"
#include "../src/core/nes.h"
#include "../src/host/qol_input.h"
static nes_t base, n, baseline, field, sentence_copy;
static unsigned failures;
#define CHECK(c) do { if (!(c)) { fprintf(stderr,"FAIL line %d: %s\n",__LINE__,#c); failures++; } } while(0)
static unsigned r(unsigned a) { return bus_peek(&base,a); }
static unsigned predicted(unsigned x,unsigned y) {
    for(unsigned i=1;i<=r(0x63d9)&&i<=34;i++) {
        if(r(0x65e7+i)&0x20) continue;
        unsigned j=i, guard=0;
        while(r(0x6515+j)) {
            unsigned parent=r(0x6515+j);
            if(parent>34 || ++guard>34 || (r(0x65e7+parent)&0x80)!=r(0x6538+j)) goto next;
            j=parent;
        }
        if(x<r(0x63da+i)||x>=(unsigned char)(r(0x63da+i)+r(0x6420+i))) continue;
        if((y>>2)<r(0x63fd+i)||(y>>2)>=(unsigned char)(r(0x63fd+i)+r(0x6443+i))) continue;
        return r(0x64f2+i)|(r(0x64cf+i)<<8);
        next:;
    }
    return 0;
}
static unsigned native(unsigned x,unsigned y) {
    memcpy(&n,&base,sizeof n);
    n.ppu.ctrl=0; n.cpu.need_nmi=n.cpu.prev_need_nmi=n.cpu.nmi_line=n.cpu.prev_nmi_line=false;
    n.cpu.run_irq=n.cpu.prev_run_irq=false;n.cpu.irq_sources=0;
    n.cpu.pc=0xe1e6;n.cpu.s=0xfd;n.cpu.p=0x24;
    n.ram[0x1fe]=0xff;n.ram[0x1ff]=0x4f;
    n.prgram[0x6c9d - 0x6000]=x;n.prgram[0x6c9e - 0x6000]=y;
    for(unsigned steps=0;steps<10000;steps++) {
        if(n.cpu.pc==0x5000) return n.cpu.x|(n.cpu.a<<8);
        cpu_step(&n);
    }
    fprintf(stderr,"routine timeout pc=%04x\n",n.cpu.pc);exit(4);
}
static void plain_cursor(nes_t *v,int x,int y) {
    for(unsigned a=0x710f;a<=0x7115;a++)v->prgram[a-0x6000]=0;
    v->prgram[0x7117 - 0x6000]=0;
    v->prgram[0x7116 - 0x6000]=x;v->prgram[0x7118 - 0x6000]=y;
}
static void pair_frames(unsigned pad,unsigned frames) {
    for(unsigned i=0;i<frames;i++) {
        nes_set_pad(&n,0,pad);nes_set_pad(&baseline,0,pad);
        nes_run_frame(&n);nes_run_frame(&baseline);
    }
}
static void pair_click(void) { pair_frames(0,2);pair_frames(PAD_A,3);pair_frames(0,17); }
static bool identical(void) {
    size_t z=nes_state_size();void*a=malloc(z),*b=malloc(z);uint8_t id[8]={0};
    if(!a||!b)exit(5);
    nes_state_save(&n,a,id);nes_state_save(&baseline,b,id);
    bool same=memcmp(a,b,z)==0;free(a);free(b);return same;
}
static void hook_frames(qol_native_t*q,qol_options_t o,unsigned pad,unsigned f,bool permitted) {
    for(unsigned i=0;i<f;i++) {
        qol_native_update(&n,q,o,pad,permitted);nes_set_pad(&n,0,pad);nes_run_frame(&n);
    }
}
static void pair_verb(unsigned id) {
    CHECK(qol_native_verb_cursor(&n,id));
    plain_cursor(&baseline,bus_peek(&n,0x7116),bus_peek(&n,0x7118));pair_click();CHECK(identical());
}
static bool pair_object(unsigned id) {
    qol_target_t targets[34];size_t count=qol_native_targets(&n,targets,34);
    for(size_t i=0;i<count;i++)if(targets[i].id==id){
        CHECK(qol_native_cursor(&n,targets[i].x,targets[i].y));
        plain_cursor(&baseline,targets[i].x,targets[i].y);pair_click();CHECK(identical());return true;
    }
    fprintf(stderr,"native visible object %u unavailable\n",id);failures++;return false;
}
static void pair_until(unsigned address,unsigned mask,unsigned expected) {
    unsigned f=0;while((bus_peek(&n,address)&mask)!=expected&&f++<600)pair_frames(0,1);
    CHECK((bus_peek(&n,address)&mask)==expected);pair_frames(0,60);CHECK(identical());
}
static void check_pending_chooser(bool expected_pending);
static void key_route(void) {
    /* Historical USA porch metadata; this route is attempted only with a
     * caller-provided compatible porch fixture, never a bundled game state. */
    memcpy(&n,&base,sizeof n);memcpy(&baseline,&base,sizeof baseline);
    if(r(0x63c6)!=1||r(0x660a+54)!=0x0f||r(0x660a+55)!=0x1f){puts("natural key route: skipped incompatible porch progression");return;}
    pair_verb(10);if(!pair_object(54))return;pair_until(0x660a+54,0x80,0x80);
    CHECK((bus_peek(&n,0x660a+54)&0x80)!=0);
    pair_verb(14);if(!pair_object(55))return;pair_until(0x660a+55,0x0f,bus_peek(&n,0x6100));
    CHECK((bus_peek(&n,0x660a+55)&0x0f)==bus_peek(&n,0x6100));
    pair_verb(11);CHECK(qol_native_cursor(&n,30,212));plain_cursor(&baseline,30,212);pair_click();CHECK(identical());
    CHECK(bus_peek(&n,0x611b)==55);CHECK(bus_peek(&n,0x611d)==1);check_pending_chooser(true);if(!pair_object(61))return;pair_until(0x660a+61,0xff,0x8f);
    CHECK(bus_peek(&n,0x660a+61)==0x8f);
    printf("natural key route: mat=%02x key=%02x door=%02x full_native_state_equal=%d failures=%u\n",bus_peek(&n,0x660a+54),bus_peek(&n,0x660a+55),bus_peek(&n,0x660a+61),identical(),failures);
}
static void runtime(void) {
    memcpy(&n,&base,sizeof n);memcpy(&baseline,&base,sizeof baseline);
    qol_native_t q;qol_native_init(&q);
    for(unsigned f=0;f<90;f++) {
        unsigned pad=f<15?PAD_RIGHT:f<20?PAD_A:0;
        qol_native_update(&n,&q,(qol_options_t){0},pad,true);
        CHECK(n.exec_hook==base.exec_hook);
        pair_frames(pad,1);CHECK(identical());
    }
    memcpy(&n,&base,sizeof n);memcpy(&baseline,&base,sizeof baseline);qol_native_init(&q);
    qol_options_t quick={.quick_verbs=true};
    for(unsigned f=0;f<28;f++) {
        unsigned pad=f==2?PAD_SELECT:0;qol_native_update(&n,&q,quick,pad,true);pair_frames(pad,1);
    }
    CHECK(!q.policy.chooser);CHECK(bus_peek(&n,0x6100)==bus_peek(&baseline,0x6100));
    CHECK(bus_peek(&n,0x611a)==bus_peek(&baseline,0x611a));
    CHECK(bus_peek(&n,0x6127)==bus_peek(&baseline,0x6127));
    printf("Select tap: native menu event=%02x baseline=%02x actor/verb preserved\n",bus_peek(&n,0x6127),bus_peek(&baseline,0x6127));
    memcpy(&n,&base,sizeof n);qol_native_init(&q);
    hook_frames(&q,quick,0,2,true);hook_frames(&q,quick,PAD_SELECT,22,true);
    CHECK(q.policy.chooser);CHECK(bus_peek(&n,0x611a)==r(0x611a));
    hook_frames(&q,quick,0,2,true);hook_frames(&q,quick,PAD_RIGHT,2,true);
    hook_frames(&q,quick,0,2,true);hook_frames(&q,quick,PAD_A,3,true);hook_frames(&q,quick,0,17,true);
    CHECK(bus_peek(&n,0x611a)==9);CHECK(!q.restore_cursor);
    memcpy(&n,&base,sizeof n);qol_native_init(&q);
    qol_options_t snap={.object_snap=true};
    CHECK(qol_native_cursor(&n,16,39));hook_frames(&q,snap,0,2,true);
    hook_frames(&q,snap,PAD_RIGHT,2,true);CHECK(q.highlight);
    if(q.highlight){CHECK(bus_peek(&n,0x7116)==(unsigned)q.selected.x);CHECK(bus_peek(&n,0x7118)==(unsigned)q.selected.y);}
    /* Host menu disables hook and discards ongoing gestures without input.
     * This checks the runtime adapter, not physical menu/controller polling. */
    hook_frames(&q,snap,0,1,false);CHECK(n.exec_hook==NULL);CHECK(!q.highlight);
    memcpy(&n,&base,sizeof n);memcpy(&baseline,&base,sizeof baseline);qol_native_init(&q);
    qol_options_t walk={.hold_walk=true};
    CHECK(qol_native_cursor(&n,16,39));plain_cursor(&baseline,16,39);
    for(unsigned f=0;f<30;f++) {
        unsigned pad=f<2?0:PAD_A;qol_native_update(&n,&q,walk,pad,true);pair_frames(pad,1);CHECK(identical());
    }
    CHECK(q.policy.holding_walk);CHECK(!q.last_walk_ms);
    qol_native_update(&n,&q,walk,PAD_A,false);CHECK(!q.policy.holding_walk);CHECK(!n.exec_hook);
    hook_frames(&q,walk,PAD_A,20,true);CHECK(!q.policy.holding_walk);CHECK(!q.last_walk_ms);
    hook_frames(&q,walk,0,2,true);CHECK(!q.policy.blocked);
    memcpy(&n,&base,sizeof n);qol_native_init(&q);
    CHECK(qol_native_cursor(&n,16,39));hook_frames(&q,walk,0,2,true);
    hook_frames(&q,walk,PAD_A,22,true);hook_frames(&q,walk,PAD_A|PAD_RIGHT,20,true);
    CHECK(q.last_walk_ms!=0);uint64_t last=q.last_walk_ms;
    hook_frames(&q,walk,PAD_RIGHT,20,true);CHECK(!q.policy.holding_walk);CHECK(q.last_walk_ms==last);
    memcpy(&n,&base,sizeof n);qol_native_init(&q);
    qol_options_t combined={.hold_walk=true,.object_snap=true};
    CHECK(qol_native_cursor(&n,16,39));hook_frames(&q,combined,0,2,true);
    hook_frames(&q,combined,PAD_A,22,true);
    hook_frames(&q,combined,PAD_A|PAD_RIGHT,20,true);
    CHECK(q.last_walk_ms!=0);CHECK(bus_peek(&n,0x7116)>16);CHECK(!q.highlight);
    last=q.last_walk_ms;
    hook_frames(&q,combined,PAD_RIGHT,8,true);
    CHECK(!q.policy.holding_walk);CHECK(q.last_walk_ms==last);
    hook_frames(&q,combined,0,2,true);
    CHECK(qol_native_cursor(&n,16,39));hook_frames(&q,combined,0,2,true);
    hook_frames(&q,combined,PAD_RIGHT,2,true);CHECK(q.highlight);
    CHECK(q.last_walk_ms==last);
    printf("combined hold+snap: native cursor retarget, release, resumed edge snap failures=%u\n",failures);
    memcpy(&n,&base,sizeof n);qol_native_init(&q);
    CHECK(qol_native_verb_cursor(&n,3));hook_frames(&q,(qol_options_t){0},0,2,true);
    hook_frames(&q,(qol_options_t){0},PAD_A,3,true);hook_frames(&q,(qol_options_t){0},0,17,true);
    CHECK(qol_native_cursor(&n,30,212));hook_frames(&q,(qol_options_t){0},0,2,true);
    hook_frames(&q,(qol_options_t){0},PAD_A,3,true);hook_frames(&q,(qol_options_t){0},0,17,true);
    unsigned pending=bus_peek(&n,0x611b);CHECK(pending!=0);CHECK(bus_peek(&n,0x611a)==3);CHECK(qol_native_cursor(&n,16,39));
    hook_frames(&q,snap,0,2,true);hook_frames(&q,snap,PAD_B|PAD_RIGHT,12,true);hook_frames(&q,snap,0,2,true);
    CHECK(bus_peek(&n,0x611a)==3);CHECK(bus_peek(&n,0x611b)==pending);CHECK(bus_peek(&n,0x7116)>16);
    for(unsigned vi=0;vi<2;vi++) {
        unsigned verb=vi?3:11;
        memcpy(&n,&base,sizeof n);memcpy(&baseline,&base,sizeof baseline);
        CHECK(qol_native_verb_cursor(&n,verb));
        plain_cursor(&baseline,bus_peek(&n,0x7116),bus_peek(&n,0x7118));pair_click();CHECK(identical());
        /* Original first inventory cell; same untouched native ownership and
         * sentence handling on each side, followed by a native scene target. */
        CHECK(qol_native_cursor(&n,30,212));plain_cursor(&baseline,30,212);pair_click();CHECK(identical());
        unsigned first=bus_peek(&n,0x611b)|(bus_peek(&n,0x601b)<<8);
        CHECK(first!=0);
        CHECK(bus_peek(&n,0x611d)==(vi?4:0));check_pending_chooser(vi!=0);
        qol_target_t t[34];size_t count=qol_native_targets(&n,t,34);CHECK(count>0);
        if(count){CHECK(qol_native_cursor(&n,t[0].x,t[0].y));plain_cursor(&baseline,t[0].x,t[0].y);pair_click();pair_frames(0,240);CHECK(identical());}
        printf("two-object %s: first_object=%u full_native_state_equal=%d\n",vi?"Give":"Use",first,identical());
    }
    memcpy(&n,&base,sizeof n);memcpy(&baseline,&base,sizeof baseline);qol_native_init(&q);
    pair_verb(7);CHECK(qol_native_cursor(&n,16,39));plain_cursor(&baseline,16,39);
    for(unsigned f=0;f<18;f++) {
        unsigned pad=f<2?0:f<14?PAD_RIGHT:0;
        qol_native_update(&n,&q,(qol_options_t){.object_snap=true,.quick_verbs=true},pad,true);
        pair_frames(pad,1);CHECK(identical());CHECK(!q.highlight);
    }
    CHECK(bus_peek(&n,0x7116)>16);
    printf("runtime: OFF90 and stationary-held30 frame states, Select chooser, snap, hold release/menu reset and pending-sentence free cursor verified failures=%u\n",failures);
}
static void check_pending_chooser(bool expected_pending) {
    memcpy(&sentence_copy,&n,sizeof n);
    qol_native_t state;qol_native_init(&state);
    hook_frames(&state,(qol_options_t){.quick_verbs=true},0,2,true);
    hook_frames(&state,(qol_options_t){.quick_verbs=true},PAD_SELECT,22,true);
    CHECK(state.policy.chooser==!expected_pending);
    memcpy(&n,&sentence_copy,sizeof n);
}
int main(int argc,char**argv) {
    if(argc<3)return 2;
    cart_t c;char err[512];
    if(!cart_load(&c,argv[1],false,err,sizeof err)){fprintf(stderr,"%s\n",err);return 2;}
    uint8_t id[8];for(int i=0;i<8;i++){unsigned v;sscanf(c.prg_sha256+i*2,"%2x",&v);id[i]=v;}
    nes_init(&base,c.prg,c.prg_size);
    for(int s=2;s<argc;s++) {
        bool cutscene=false;
        if(!strcmp(argv[s],"--cutscene")){cutscene=true;if(++s>=argc)return 2;}
        FILE*f=fopen(argv[s],"rb");if(!f)return 2;
        size_t z=nes_state_size();void*b=malloc(z);size_t got=fread(b,1,z,f);fclose(f);
        if(!nes_state_load(&base,b,got,id)){fprintf(stderr,"state rejected %s\n",argv[s]);return 3;}free(b);
        if(cutscene) {
            memcpy(&n,&base,sizeof n);memcpy(&baseline,&base,sizeof baseline);
            CHECK(!(r(0x6b0a)&0x10)||r(0x6b08)||r(0x6b09));
            qol_native_t q;qol_native_init(&q);q.policy.holding_walk=q.policy.chooser=true;
            qol_options_t options={.hold_walk=true,.quick_verbs=true};
            qol_native_update(&n,&q,options,PAD_A,true);
            CHECK(!n.exec_hook);CHECK(!q.policy.holding_walk&&!q.policy.chooser&&q.policy.blocked);CHECK(identical());
            pair_frames(PAD_A,30);CHECK(identical());
            memcpy(&n,&field,sizeof n);
            hook_frames(&q,options,PAD_A,25,true);CHECK(!q.policy.holding_walk);CHECK(!q.last_walk_ms);
            hook_frames(&q,options,0,2,true);CHECK(!q.policy.blocked);
            printf("actual input-disabled fixture: gesture cancellation and held-key resume drain failures=%u\n",failures);
            continue;
        }
        if(s==2)memcpy(&field,&base,sizeof field);
        unsigned count=0,mismatch=0,hit=0;
        for(unsigned sy=35;sy<160;sy+=4)for(unsigned sx=16;sx<=239;sx+=4){
            unsigned x=((sx>>3)+r(0x6c31))&255u,y=((sy+1)>>1)-18;
            unsigned p=predicted(x,y),a=native(x,y);count++;hit+=a!=0;
            if(p!=a){mismatch++;if(mismatch<4)printf("mismatch screen=(%u,%u) native=%u predicted=%u\n",sx,sy,a,p);}
        }
        printf("%s: room=%u objects=%u tested=%u hits=%u mismatches=%u\n",argv[s],r(0x63c6),r(0x63d9),count,hit,mismatch);
        CHECK(mismatch==0);
        qol_target_t targets[34];
        size_t target_count=qol_native_targets(&base,targets,34);
        CHECK(qol_native_ready(&base)); CHECK(target_count>0);
        for(size_t i=0;i<target_count;i++) {
            CHECK(targets[i].x>=16&&targets[i].x<240&&targets[i].y>=35&&targets[i].y<175);
            CHECK(targets[i].visible&&targets[i].interactive&&targets[i].hit_visible);
            unsigned x=((targets[i].x>>3)+r(0x6c31))&255u,y=((targets[i].y+1)>>1)-18;
            CHECK(native(x,y)==targets[i].id);
        }
        unsigned uncovered=0;
        for(unsigned sy=35;sy<175;sy+=4)for(unsigned sx=16;sx<=239;sx+=4) {
            unsigned id=native(((sx>>3)+r(0x6c31))&255u,((sy+1)>>1)-18);
            if(!id)continue;
            size_t j;for(j=0;j<target_count&&targets[j].id!=id;j++);
            uncovered+=(j==target_count);
        }
        CHECK(uncovered==0);
        static const unsigned verbs[]={13,9,10,3,1,2,14,11,12,4,5,7};
        unsigned tested_verbs=0;
        for(size_t v=0;v<sizeof verbs/sizeof verbs[0];v++) {
            memcpy(&n,&base,sizeof n);
            CHECK(qol_native_verb_cursor(&n,verbs[v]));
            /* Real NMI controller polling and scene script; no PC or action-latch
             * injection in this check. Hold A then release, wait 12 native frames. */
            nes_set_pad(&n,0,0);for(unsigned f=0;f<2;f++)nes_run_frame(&n);
            nes_set_pad(&n,0,PAD_A);for(unsigned f=0;f<3;f++)nes_run_frame(&n);
            nes_set_pad(&n,0,0);for(unsigned f=0;f<12;f++)nes_run_frame(&n);
            CHECK(bus_peek(&n,0x611a)==verbs[v]);
            tested_verbs++;
        }
        printf("adapter: targets=%zu uncovered native visible IDs=%u native verb clicks=%u failures=%u\n",target_count,uncovered,tested_verbs,failures);
        if(s==2){runtime();key_route();}
    }
    return failures?1:0;
}
