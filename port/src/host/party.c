#include "party.h"
#include <stdlib.h>
#include <string.h>

static unsigned get(const nes_t *n, unsigned a) {
    return a < 0x800 ? n->ram[a] : n->prgram[a-0x6000];
}
static void set(nes_t *n,unsigned a,unsigned v) {
    if(a<0x800)n->ram[a]=(uint8_t)v;else n->prgram[a-0x6000]=(uint8_t)v;
}
static unsigned word(const nes_t *n,unsigned v) { return get(n,0x6100+v)|(get(n,0x6000+v)<<8); }
static unsigned bit(unsigned actor) { return actor>=1&&actor<=7 ? 1u<<(actor-1) : 0; }
static unsigned starter(unsigned actor) {
    switch(actor){case 1:return 49;case 2:return 47;case 3:return 46;case 4:return 52;
        case 5:return 48;case 6:return 50;case 7:return 51;default:return 0;}
}
static bool roster(const nes_t*n,uint8_t ids[3]) {
    unsigned mask=0;for(unsigned i=0;i<3;i++){unsigned id=word(n,47+i);
        if(!bit(id)||(mask&bit(id)))return false;
        ids[i]=(uint8_t)id;mask|=bit(id);}
    return ids[0]==3;
}
static bool sane(const party_history_t*h) {
    if(h->version!=1||h->valid>1)return false;
    if(!h->valid)return !h->activated_mask&&!h->retired_mask&&!h->starter_issued_mask&&
        !h->initial_roster[0]&&!h->initial_roster[1]&&!h->initial_roster[2];
    unsigned mask=0;for(unsigned i=0;i<3;i++){unsigned b=bit(h->initial_roster[i]);if(!b||(mask&b))return false;mask|=b;}
    return h->initial_roster[0]==3&&(h->activated_mask&128)==0&&(h->retired_mask&128)==0&&
        (h->starter_issued_mask&128)==0&&(h->activated_mask&mask)==mask&&
        (h->starter_issued_mask&mask)==mask&&!(h->retired_mask&~h->activated_mask)&&
        !(h->starter_issued_mask&~h->activated_mask)&&!(h->retired_mask&bit(3));
}
static void unknown(party_history_t*h){memset(h,0,sizeof(*h));h->version=1;}
void party_init(party_state_t*s){memset(s,0,sizeof(*s));unknown(&s->history);}
void party_destroy(party_state_t*s){free(s->rollback);party_init(s);}
void party_cancel(nes_t*n,party_state_t*s){
    if(s->executing&&s->rollback&&n)memcpy(n,s->rollback,sizeof(*n));
    free(s->rollback);s->rollback=NULL;s->pending=s->executing=false;s->result=PARTY_NATIVE_FAILED;
}
const char *party_actor_name(unsigned a){
    static const char*const names[]={"","Syd","Razor","Dave","Michael","Bernard","Wendy","Jeff"};
    return a<8?names[a]:"Unknown";
}
const char *party_result_name(party_result_t r){
    static const char*const names[]={"Ready","Available only in Enhanced mode","Enable team changes first",
        "Team history unavailable; start a new Enhanced game or load a trusted Enhanced snapshot",
        "Native team state is inconsistent","Dave must remain on the team","Outgoing kid is not on the team",
        "Incoming kid is already on the team","Return Dave and the outgoing kid to the roadside",
        "Select Dave before changing teammates","Kid is unavailable, captured, dead or not safely retired",
        "Wait for the native story or action to finish","Finish or cancel the current sentence",
        "Wait until the kids stop moving","Release all game buttons first","Original starter item is not safely available",
        "Team change is in progress","Native team change failed; previous state restored"};
    return (unsigned)r<sizeof(names)/sizeof(*names)?names[r]:"Unavailable";
}
const char *party_result_short(party_result_t r){
    static const char*const names[]={"Ready","Enhanced mode only","Enable team changes",
        "Trusted team history needed","Inconsistent native team","Dave must stay",
        "Not a current teammate","Already on the team","Return to the roadside",
        "Select Dave first","Kid unavailable","Wait for native action",
        "Finish current sentence","Wait until kids stop","Release game buttons",
        "Starter item unavailable","Changing teammates...","Change failed; restored"};
    return (unsigned)r<sizeof(names)/sizeof(*names)?names[r]:"Unavailable";
}
bool party_history_encode(const party_history_t*h,uint8_t out[8]){
    if(!h||!out||!sane(h))return false;
    out[0]=h->version;out[1]=h->valid;out[2]=h->activated_mask;out[3]=h->retired_mask;
    out[4]=h->starter_issued_mask;memcpy(out+5,h->initial_roster,3);return true;
}
bool party_history_decode(party_history_t*h,const uint8_t*b,size_t size){
    if(!h)return false;
    unknown(h);
    if(!b||size!=8)return false;
    party_history_t v={b[0],b[1],b[2],b[3],b[4],{b[5],b[6],b[7]}};
    if(!sane(&v))return false;
    *h=v;return true;
}
bool party_restore_history(party_state_t*s,const uint8_t*b,size_t size){
    if(s->executing)return false;
    free(s->rollback);s->rollback=NULL;s->pending=false;s->constructor_phase=0;
    s->fresh_observed=s->load_observed=false;s->changed=false;
    return party_history_decode(&s->history,b,size);
}
void party_update(party_state_t*s,bool enhanced,bool enabled,bool neutral){
    s->enhanced=enhanced;s->enabled=enabled;s->neutral=neutral;
    if(!enhanced&&!s->executing){s->pending=false;unknown(&s->history);s->constructor_phase=0;}
}
static bool safe_scripts(const nes_t*n){
    for(unsigned i=0;i<20;i++)if(get(n,0x633d+i)){
        if((get(n,0x638d+i)&63)!=2)return false;
        unsigned id=get(n,0x63a1+i);
        if(id!=7&&id!=57&&id!=177&&id!=178)return false;
    }
    return true;
}
static bool free_actor(const nes_t*n,unsigned a){
    return get(n,0x6b43+a)==a&&!(get(n,0x60b8)&(1u<<a))&&!(get(n,0x60b7)&(1u<<a));
}
static bool moving(const nes_t*n,unsigned a){
    unsigned slot=get(n,0x6d22+a);
    return slot<4&&(get(n,0x6d1a+slot)!=a||get(n,0x6cce + slot)!=0);
}
party_result_t party_check(const nes_t*n,const party_state_t*s,unsigned out,unsigned in){
    if(!s->enhanced)return PARTY_ORIGINAL;
    if(!s->enabled)return PARTY_DISABLED;
    if(s->pending||s->executing)return PARTY_PENDING;
    if(!s->history.valid||!sane(&s->history))return PARTY_HISTORY_UNKNOWN;
    uint8_t ids[3];if(!roster(n,ids))return PARTY_BAD_ROSTER;
    unsigned selected=bit(ids[0])|bit(ids[1])|bit(ids[2]);
    if((selected&s->history.activated_mask)!=selected||(selected&s->history.retired_mask))return PARTY_BAD_ROSTER;
    if(out==3||in==3)return PARTY_DAVE_REQUIRED;
    if(!bit(out)||!(selected&bit(out)))return PARTY_NOT_TEAMMATE;
    if(!bit(in)||(selected&bit(in)))return PARTY_ALREADY_SELECTED;
    if(get(n,0x63c6)!=44||get(n,0x6b5d+out)!=44||get(n,0x6b5d+3)!=44)return PARTY_RETURN_TO_ROAD;
    if(word(n,0)!=3)return PARTY_SELECT_DAVE;
    if(!free_actor(n,out)||!free_actor(n,in)||!free_actor(n,3)||get(n,0x6b5d+in)!=0||
        ((s->history.activated_mask&bit(in))&&!(s->history.retired_mask&bit(in))))return PARTY_ACTOR_UNAVAILABLE;
    if(!(get(n,0x6b0a)&0x10)||get(n,0x6b08)||get(n,0x6b09)||word(n,175)||!safe_scripts(n))return PARTY_STORY_BUSY;
    if(word(n,27)||word(n,28)||word(n,29)||get(n,0x6c9c)||get(n,0x6cac))return PARTY_SENTENCE_BUSY;
    if(moving(n,out)||moving(n,3)||get(n,0x6d22+in)<4)return PARTY_MOVING;
    if(!s->neutral)return PARTY_INPUT_HELD;
    if(!(s->history.starter_issued_mask&bit(in))){
        unsigned item=starter(in);if(get(n,0x660a+item)!=0x6f)return PARTY_STARTER_UNSAFE;
        for(unsigned i=0;i<60;i++)if(get(n,0x6260+i)==item)return PARTY_STARTER_UNSAFE;
        /* Native C725 searches slots 59..1; slot zero is reserved. */
        bool empty=false;for(unsigned i=1;i<60;i++)if(!get(n,0x6260+i))empty=true;
        if(!empty)return PARTY_STARTER_UNSAFE;
    }
    return PARTY_OK;
}
party_result_t party_request(nes_t*n,party_state_t*s,unsigned out,unsigned in){
    party_result_t r=party_check(n,s,out,in);s->result=r;if(r!=PARTY_OK)return r;
    s->outgoing=(uint8_t)out;s->incoming=(uint8_t)in;s->pending=true;
    s->changed=false;s->result=PARTY_PENDING;return PARTY_OK;
}
/* Only caller operands are restored. NMI counters/pads, PPU/APU, mapper state,
 * script delays and real elapsed CPU/master clocks continue normally. */
static const unsigned scratch[]={0x15,0x16,0x74,0x75,0xaf,0xb0,0xb7,0xd7,0xe9,0xea,
    0x6c3c,0x6c68,0x6c69,0x6c9d,0x6c9e,0x6ca1,0x6cb1,0x7edd,0x7ede,0x7edf,0x7ee0};
static void operand(nes_t*n,unsigned index,unsigned value){set(n,0x6c3c,index);set(n,0x6c68,value);set(n,0x6c69,0);}
static bool verified_result(nes_t*n,party_state_t*s){
    if(word(n,0)!=s->old_ego||word(n,47+s->roster_slot)!=s->incoming||get(n,0x6b5d+s->outgoing)!=0||
        get(n,0x6d22+s->outgoing)<4||get(n,0x6b5d+s->incoming)!=44||get(n,0x6b43+s->incoming)!=s->incoming)return false;
    unsigned slot=get(n,0x6d22+s->incoming);if(slot>=4||get(n,0x6d1a+slot)!=s->incoming)return false;
    if(!(s->history.starter_issued_mask&bit(s->incoming))){
        unsigned item=starter(s->incoming),count=0;
        if(get(n,0x660a+item)!=(0x60|s->incoming))return false;
        for(unsigned i=0;i<60;i++)if(get(n,0x6260+i)==item)count++;
        if(count!=1)return false;
    }
    return true;
}
static void next_call(nes_t*n,party_state_t*s){
    bool issue=!(s->history.starter_issued_mask&bit(s->incoming));unsigned pc=0;
    for(;;){switch(s->call_phase++){
        case 0:set(n,0xd7,s->outgoing);n->cpu.x=s->outgoing;n->cpu.a=0;pc=0xcfd4;break;
        case 1:set(n,0xd7,s->incoming);n->cpu.x=s->incoming;n->cpu.a=44;pc=0xcfd4;break;
        case 2:set(n,0xd7,s->incoming);set(n,0x6ca1,44);set(n,0x6c9d,s->spawn_x);set(n,0x6c9e,s->spawn_y);pc=0xe114;break;
        case 3:if(!issue)continue;operand(n,0,s->incoming);pc=0xc8c2;break;
        case 4:if(!issue)continue;set(n,0x6c68,starter(s->incoming));set(n,0x6c69,0);pc=0xcb4e;break;
        case 5:if(!issue)continue;set(n,0x6c68,starter(s->incoming));set(n,0x6c69,0);pc=0xc701;break;
        case 6:if(!issue)continue;n->cpu.a&=0x7f;pc=0xc715;break;
        case 7:if(!issue)continue;operand(n,0,s->old_ego);pc=0xc8c2;break;
        case 8:operand(n,47+s->roster_slot,s->incoming);pc=0xc8c2;break;
        case 9:n->cpu.a=164;pc=0xc582;break;
        case 10:n->cpu.a=s->old_bank;pc=0xdf85;break;
        default:
            if(!verified_result(n,s)){party_cancel(n,s);return;}
            for(unsigned i=0;i<sizeof(scratch)/sizeof(*scratch);i++)set(n,scratch[i],s->saved_scratch[i]);
            set(n,0x100|s->saved_s,s->saved_stack[0]);set(n,0x100|((s->saved_s-1)&255),s->saved_stack[1]);
            n->cpu.a=s->saved_a;n->cpu.x=s->saved_x;n->cpu.y=s->saved_y;n->cpu.p=s->saved_p;n->cpu.s=s->saved_s;
            s->history.activated_mask|=(uint8_t)bit(s->incoming);
            s->history.retired_mask|=(uint8_t)bit(s->outgoing);s->history.retired_mask&=(uint8_t)~bit(s->incoming);
            s->history.starter_issued_mask|=(uint8_t)bit(s->incoming);
            free(s->rollback);s->rollback=NULL;s->executing=false;s->changed=true;s->result=PARTY_OK;return;
    }break;}
    /* Synthetic JSR continuation; actual retail instructions run asynchronously
     * under normal nes_run_frame, including every native NMI and audio tick. */
    set(n,0x100|n->cpu.s,0xc0);n->cpu.s--;set(n,0x100|n->cpu.s,0x48);n->cpu.s--;
    n->cpu.pc=(uint16_t)pc;
}
static void observe(nes_t*n,party_state_t*s){
    if(n->cpu.pc!=0xc429)return;
    unsigned slot=get(n,0x6314);if(slot>=20||(get(n,0x638d+slot)&63)!=2)return;
    unsigned id=get(n,0x63a1+slot);
    unsigned ptr=get(n,0x7ede)|(get(n,0x7edf)<<8),base=get(n,0x6ca9)|(get(n,0x6caa)<<8);
    unsigned offset=(ptr-base)&65535u;
    if(id==1&&offset==0){unknown(&s->history);s->constructor_phase=1;s->changed=true;}
    if(id==1&&offset==0x327&&s->constructor_phase==1)s->constructor_phase=2;
    if(id==163&&offset==0x63){unknown(&s->history);s->constructor_phase=0;s->load_observed=true;s->changed=true;}
    if(id==131&&offset==0x9a&&s->constructor_phase==2){
        uint8_t ids[3];if(!roster(n,ids))return;
        unsigned mask=0;for(unsigned i=0;i<3;i++){
            if(get(n,0x6b43+ids[i])!=ids[i]||get(n,0x6b5d+ids[i])!=44||
                (get(n,0x660a+starter(ids[i]))&15)!=ids[i])return;
            mask|=bit(ids[i]);
        }
        unknown(&s->history);s->history.valid=1;s->history.activated_mask=(uint8_t)mask;
        s->history.starter_issued_mask=(uint8_t)mask;memcpy(s->history.initial_roster,ids,3);
        s->constructor_phase=0;s->fresh_observed=true;s->changed=true;
    }
}
void party_exec(nes_t*n,party_state_t*s){
    if(!s->enhanced)return;
    if(!s->executing)observe(n,s);
    if(s->executing){
        if(n->cpu.cycles-s->call_cycles>1000000u){party_cancel(n,s);return;}
        if(n->cpu.pc==0xc049&&n->cpu.s==s->saved_s)next_call(n,s);
        return;
    }
    if(!s->pending||n->cpu.pc!=0xc049)return;
    s->pending=false;party_result_t r=party_check(n,s,s->outgoing,s->incoming);
    if(r!=PARTY_OK){s->result=r;return;}
    if(get(n,0x6314)!=255||get(n,0x6c49)||get(n,0x710d)||get(n,0x72d8)||
        get(n,0x6ebc)!=get(n,0x6ebd)||(get(n,0x6eff)&2)||n->cpu.s<32){s->pending=true;return;}
    s->rollback=malloc(sizeof(*n));if(!s->rollback){s->result=PARTY_NATIVE_FAILED;return;}
    memcpy(s->rollback,n,sizeof(*n));s->saved_a=n->cpu.a;s->saved_x=n->cpu.x;s->saved_y=n->cpu.y;
    s->saved_p=n->cpu.p;s->saved_s=n->cpu.s;s->call_cycles=n->cpu.cycles;
    s->saved_stack[0]=(uint8_t)get(n,0x100|n->cpu.s);s->saved_stack[1]=(uint8_t)get(n,0x100|((n->cpu.s-1)&255));
    for(unsigned i=0;i<sizeof(scratch)/sizeof(*scratch);i++)s->saved_scratch[i]=(uint8_t)get(n,scratch[i]);
    s->old_bank=(uint8_t)((n->mmc1.prg_lo-n->prg)/16384);s->old_ego=(uint8_t)word(n,0);
    uint8_t ids[3];roster(n,ids);for(unsigned i=0;i<3;i++)if(ids[i]==s->outgoing)s->roster_slot=(uint8_t)i;
    unsigned slot=get(n,0x6d22+s->outgoing);
    s->spawn_x=(uint8_t)(slot<4?get(n,0x6cbe + slot):get(n,0x6b91+s->outgoing));
    s->spawn_y=(uint8_t)(slot<4?get(n,0x6cc2+slot):get(n,0x6b77+s->outgoing));
    s->call_phase=0;s->executing=true;next_call(n,s);
}
