/* Run ONLY in an isolated xvfb-run server with MM_TEST_XVFB=1. Actual SDL
 * keyboard events and virtual-controller polling feed the production adapters.
 * Inputs/ROM/state files are local fixtures, never owner foreground or saves. */
#define main mm_application_main
#include "../src/host/main.c"
#undef main
#include <SDL_syswm.h>
#include <X11/Xlib.h>
#include <X11/keysym.h>
#include <dlfcn.h>

#if SDL_VERSION_ATLEAST(2, 24, 0)
typedef int (*fake_key_t)(Display *,unsigned,Bool,unsigned long);
typedef struct {
    fake_key_t fake;
    SDL_Joystick *joystick; /* NULL selects actual XTest keyboard input. */
    SDL_atomic_t done;
} boot_inputs_t;

static int boot_inputs(void *userdata)
{
    boot_inputs_t *input = userdata;
    Display *connection = input->joystick ? NULL : XOpenDisplay(NULL);
    SDL_Delay(150); /* Let the chooser observe neutral before the first edge. */
    for (unsigned i=0; i<2; i++) {
        if (input->joystick) {
            int button = i ? SDL_CONTROLLER_BUTTON_A : SDL_CONTROLLER_BUTTON_DPAD_DOWN;
            SDL_LockJoysticks();
            SDL_JoystickSetVirtualButton(input->joystick, button, 1);
            SDL_UnlockJoysticks();
            SDL_Delay(60);
            SDL_LockJoysticks();
            SDL_JoystickSetVirtualButton(input->joystick, button, 0);
            SDL_UnlockJoysticks();
        } else if (connection) {
            unsigned key = XKeysymToKeycode(connection, i ? XK_Return : XK_Down);
            input->fake(connection, key, True, CurrentTime); XSync(connection, False);
            SDL_Delay(60);
            input->fake(connection, key, False, CurrentTime); XSync(connection, False);
        }
        SDL_Delay(80);
    }
    if (connection) XCloseDisplay(connection);
    for (unsigned i=0; i<100 && !SDL_AtomicGet(&input->done); i++) SDL_Delay(20);
    if (!SDL_AtomicGet(&input->done)) {
        SDL_Event quit = {.type = SDL_QUIT}; SDL_PushEvent(&quit); /* Failed-input watchdog. */
    }
    return 0;
}
#endif

static nes_t keyboard_nes, controller_nes;
static int errors;
#define VERIFY(c) do { if (!(c)) { fprintf(stderr,"FAIL line %d: %s\n",__LINE__,#c); errors++; } } while(0)

int main(int argc, char **argv)
{
#if !SDL_VERSION_ATLEAST(2, 24, 0)
    (void)argc; (void)argv;
    fprintf(stderr, "SDL virtual-controller parity requires SDL >= 2.24\n");
    return 2;
#else
    if (argc != 3 || !getenv("MM_TEST_XVFB")) {
        fprintf(stderr,"usage in isolated Xvfb: MM_TEST_XVFB=1 test-qol-host ROM STATE\n"); return 2;
    }
    char err[512]; cart_t cart;
    if (!cart_load(&cart,argv[1],false,err,sizeof err)) { fprintf(stderr,"%s\n",err); return 2; }
    uint8_t id[8];
    for(int i=0;i<8;i++){unsigned x;sscanf(cart.prg_sha256+2*i,"%2x",&x);id[i]=x;}
    size_t z=nes_state_size();void *state=malloc(z);
    FILE *f=fopen(argv[2],"rb"); if(!f)return 2;
    size_t got=fread(state,1,z,f);fclose(f);
    nes_init(&keyboard_nes,cart.prg,cart.prg_size);nes_init(&controller_nes,cart.prg,cart.prg_size);
    if(!nes_state_load(&keyboard_nes,state,got,id)||!nes_state_load(&controller_nes,state,got,id))return 3;
    if(!XInitThreads())return 2;
    if(SDL_Init(SDL_INIT_VIDEO|SDL_INIT_GAMECONTROLLER))return 2;
    SDL_Window *w=SDL_CreateWindow("isolated MM input parity test",0,0,320,240,SDL_WINDOW_SHOWN);
    if(!w)return 2;
    SDL_SysWMinfo wm;SDL_VERSION(&wm.version);
    if(!SDL_GetWindowWMInfo(w,&wm)||wm.subsystem!=SDL_SYSWM_X11)return 2;
    Display *d=wm.info.x11.display;
    XSetInputFocus(d,wm.info.x11.window,RevertToParent,CurrentTime);XSync(d,False);
    void *xtst=dlopen("libXtst.so.6",RTLD_NOW);if(!xtst)return 2;
    fake_key_t fake=(fake_key_t)dlsym(xtst,"XTestFakeKeyEvent");if(!fake)return 2;
    config_t *cfg=config_load(NULL,err,sizeof err);keymap_t km;keymap_load(&km,cfg);
    const KeySym symbols[8]={XK_x,XK_z,XK_Shift_R,XK_Return,XK_Up,XK_Down,XK_Left,XK_Right};
    const int buttons[8]={SDL_CONTROLLER_BUTTON_A,SDL_CONTROLLER_BUTTON_B,SDL_CONTROLLER_BUTTON_BACK,
        SDL_CONTROLLER_BUTTON_START,SDL_CONTROLLER_BUTTON_DPAD_UP,SDL_CONTROLLER_BUTTON_DPAD_DOWN,
        SDL_CONTROLLER_BUTTON_DPAD_LEFT,SDL_CONTROLLER_BUTTON_DPAD_RIGHT};
    SDL_VirtualJoystickDesc desc={0};desc.version=SDL_VIRTUAL_JOYSTICK_DESC_VERSION;
    desc.type=SDL_JOYSTICK_TYPE_GAMECONTROLLER;desc.naxes=SDL_CONTROLLER_AXIS_MAX;
    desc.nbuttons=SDL_CONTROLLER_BUTTON_MAX;desc.vendor_id=0x045e;desc.product_id=0x028e;
    desc.name="MM isolated Xbox parity fixture";
    int index=SDL_JoystickAttachVirtualEx(&desc);if(index<0)return 2;
    pads_t pads={0};pads_init(&pads.set,NULL);pads_open(&pads,index);
    if(pads.n!=1)return 2;
    SDL_Joystick *joy=SDL_GameControllerGetJoystick(pads.gc[0]);
    SDL_Renderer *renderer = SDL_CreateRenderer(w, -1, SDL_RENDERER_SOFTWARE);
    if (!renderer) return 2;
    for (unsigned device=0; device<2; device++) {
        boot_inputs_t inputs = {.fake = fake, .joystick = device ? joy : NULL};
        mm_mode_t selected = MM_MODE_ORIGINAL;
        SDL_Thread *thread = SDL_CreateThread(boot_inputs, "isolated boot inputs", &inputs);
        if (!thread) return 2;
        bool selected_ok = mode_choose(renderer, km.key, boot_pad_poll, boot_device_event, &pads, &selected);
        SDL_AtomicSet(&inputs.done, 1); SDL_WaitThread(thread, NULL);
        VERIFY(selected_ok); VERIFY(selected == MM_MODE_ENHANCED);
    }
    printf("Boot chooser: actual XTest keyboard and virtual Xbox both select Enhanced; no core frames executed; failures=%d\n", errors);
    SDL_DestroyRenderer(renderer);
    qol_native_t qk,qg;qol_native_init(&qk);qol_native_init(&qg);
    qol_options_t o={true,true,true};
    void *a=malloc(z),*b=malloc(z);uint8_t prior=0;unsigned checked=0;
    /* Neutral, held Select -> chooser, choose Push, B free-cursor modifier,
     * lone A, movement, release. These are real device-input parity checks. */
    for(unsigned frame=0;frame<100;frame++) {
        uint8_t desired=frame<2?0:frame<25?PAD_SELECT:frame<28?0:frame<31?PAD_RIGHT:
            frame<33?0:frame<36?PAD_A:frame<42?0:frame<50?(PAD_B|PAD_RIGHT):
            frame<55?0:frame<70?PAD_A:frame<78?PAD_LEFT:0;
        for(int i=0;i<8;i++){
            uint8_t bit=1u<<i;
            if((desired&bit)!=(prior&bit))fake(d,XKeysymToKeycode(d,symbols[i]),(desired&bit)!=0,CurrentTime);
            SDL_JoystickSetVirtualButton(joy,buttons[i],(desired&bit)!=0);
        }
        XSync(d,False);SDL_Delay(2);SDL_PumpEvents();SDL_GameControllerUpdate();
        uint8_t keyboard=0;const uint8_t *keys=SDL_GetKeyboardState(NULL);
        for(int i=0;i<8;i++)if(keys[km.key[i]])keyboard|=1u<<i;
        uint8_t hot=0;uint8_t controller=pads_poll(&pads,&hot);
        VERIFY(keyboard==desired);VERIFY(controller==desired);
        qol_native_update(&keyboard_nes,&qk,o,keyboard,true);
        qol_native_update(&controller_nes,&qg,o,controller,true);
        nes_set_pad(&keyboard_nes,0,keyboard);nes_set_pad(&controller_nes,0,controller);
        nes_run_frame(&keyboard_nes);nes_run_frame(&controller_nes);
        nes_state_save(&keyboard_nes,a,id);nes_state_save(&controller_nes,b,id);
        VERIFY(!memcmp(a,b,z));VERIFY(qk.policy.chooser==qg.policy.chooser);
        VERIFY(qk.policy.verb_index==qg.policy.verb_index);
        if(frame==24)VERIFY(qk.policy.chooser);
        if(frame==41)VERIFY(bus_peek(&keyboard_nes,0x611a)==9);
        checked++;prior=desired;
    }
    for(int i=0;i<8;i++)fake(d,XKeysymToKeycode(d,symbols[i]),False,CurrentTime);
    XSync(d,False);
    printf("SDL keyboard/Xbox virtual controller: %u native-frame states identical; failures=%d\n",checked,errors);
    pads_close(&pads,SDL_JoystickInstanceID(joy));SDL_JoystickDetachVirtual(index);
    config_free(cfg);SDL_DestroyWindow(w);SDL_Quit();dlclose(xtst);
    free(state);free(a);free(b);free(keyboard_nes.apu.out);free(controller_nes.apu.out);cart_free(&cart);
    return errors?1:0;
#endif
}
