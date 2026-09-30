#include "../src/host/chord.h"
#include <assert.h>
#include <stdio.h>

static void cadence(unsigned increment)
{
    chord_timed_t c; chord_timed_init(&c);bool fire;unsigned fires=0;
    for(unsigned t=0;t<=700;t+=increment) {
        bool back=t>=100&&t<150, start=t>=300&&t<400;
        uint8_t pass=chord_timed_step(&c,back,start,4,8,t,&fire);
        if(fire)fires++;
        assert(!pass); /* sequential 200ms gesture, no native leak */
    }
    assert(fires==1);
    chord_timed_init(&c);fires=0;unsigned passed=0;
    for(unsigned t=0;t<=700;t+=increment) {
        uint8_t pass=chord_timed_step(&c,t>=100&&t<120,false,4,8,t,&fire);
        fires+=fire;
        if(pass){assert(t>=400);assert(pass==4);passed++;}
    }
    assert(!fires&&passed);
    chord_timed_init(&c);
    assert(!chord_timed_step(&c,true,false,4,8,0,&fire)&&!fire);
    assert(!chord_timed_step(&c,true,false,4,8,299,&fire)&&!fire);
    assert(chord_timed_step(&c,true,false,4,8,301,&fire)==4&&!fire);
    assert(chord_timed_step(&c,true,false,4,8,500,&fire)==4&&!fire);
    chord_timed_init(&c);
    assert(!chord_timed_step(&c,true,true,4,8,0,&fire)&&fire);
    assert(!chord_timed_step(&c,true,true,4,8,500,&fire)&&!fire);
    assert(!chord_timed_step(&c,false,false,4,8,600,&fire)&&!fire);
    assert(!chord_timed_step(&c,true,true,4,8,700,&fire)&&fire);
}
int main(void)
{
    cadence(1);cadence(7);cadence(16);
    puts("timed chord: PASS (1/7/16ms polling, sequential/held chords, preserved lone taps)");
    return 0;
}
