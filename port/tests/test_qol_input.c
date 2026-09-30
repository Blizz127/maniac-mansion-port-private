#include "../src/host/qol_input.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

int main(void)
{
    qol_input_t s; qol_input_reset(&s);
    qol_options_t off = {0}, on = {true, true, true};
    qol_context_t c = {.gameplay=true, .input_enabled=true, .walk_verb=true, .free_walk=true};
    for (unsigned p = 0; p < 256; ++p) assert(qol_input_step(&s, off, c, (uint8_t)p, 0).pad == p);
    qol_input_reset(&s);
    assert(qol_input_step(&s,on,c,PAD_A,0).pad == PAD_A);
    c.cursor_x=20;
    assert(!qol_input_step(&s,on,c,PAD_A,249).walk_update);
    assert(qol_input_step(&s,on,c,PAD_A,250).walk_update);
    assert(!qol_input_step(&s,on,c,PAD_A,251).walk_update);
    c.cursor_x=24;
    qol_result_t combined = qol_input_step(&s,on,c,PAD_A|PAD_RIGHT,255);
    assert(combined.walk_update && combined.pad == (PAD_A|PAD_RIGHT));
    c.cursor_x=30;
    assert(!qol_input_step(&s,on,c,PAD_RIGHT,260).walk_update);
    assert(!s.holding_walk);
    qol_input_step(&s,on,c,0,261);
    assert(!qol_input_step(&s,on,c,PAD_A,270).walk_update);
    c.menu=true;
    assert(qol_input_step(&s,on,c,PAD_A,600).pad==0);
    c.menu=false;
    assert(qol_input_step(&s,on,c,PAD_A,601).pad==0);
    qol_input_step(&s,on,c,0,602);
    c.walk_verb=false;
    assert(!qol_input_step(&s,on,c,PAD_A,603).walk_update);
    c.cursor_x=40;
    assert(!qol_input_step(&s,on,c,PAD_A,1000).walk_update);
    qol_input_reset(&s);
    assert(qol_input_step(&s,on,c,PAD_SELECT,0).pad==0);
    assert(qol_input_step(&s,on,c,0,20).pad==PAD_SELECT);
    qol_input_step(&s,on,c,0,21);
    qol_input_step(&s,on,c,PAD_SELECT,100);
    assert(qol_input_step(&s,on,c,PAD_SELECT,350).chooser_open);
    qol_input_step(&s,on,c,0,351);
    qol_input_step(&s,on,c,PAD_RIGHT,352);
    qol_input_step(&s,on,c,0,353);
    qol_result_t r=qol_input_step(&s,on,c,PAD_A,354);
    assert(r.select_verb && r.verb==QOL_PUSH && r.pad==0);
    assert(qol_input_step(&s,on,c,PAD_A,355).pad==0);
    qol_input_step(&s,on,c,0,356);
    qol_input_step(&s,on,c,PAD_SELECT,400);
    assert(qol_input_step(&s,on,c,PAD_SELECT|PAD_START,401).pad==0);
    qol_input_reset(&s); c.sentence_pending=true;
    assert(qol_input_step(&s,on,c,PAD_SELECT,0).pad==PAD_SELECT);
    assert(!qol_input_step(&s,on,c,PAD_SELECT,1000).chooser_open);
    qol_target_t t[]={{3,20,20,true,true,true},{2,20,20,true,true,true},
        {1,10,10,false,true,true},{4,200,10,true,true,true},
        {5,10,11,true,true,false}};
    qol_target_t chosen;
    assert(qol_snap(t,5,0,10,20,0,0,100,100,QOL_RIGHT,&chosen) && chosen.id==2);
    assert(!qol_snap(t,5,0,10,20,0,0,100,100,QOL_LEFT,&chosen));
    nes_t n; memset(&n,0,sizeof n);
    assert(!qol_native_ready(&n));
    assert(!qol_native_cursor(&n,80,80));
    puts("qol input policy tests passed (synthetic; no gameplay proof)");
    return 0;
}
