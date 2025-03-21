#define LAYER_STATE_8BIT
#undef LOCKING_SUPPORT_ENABLE
#undef LOCKING_RESYNC_ENABLE
#define PS2_MOUSE_SCROLL_BTN_MASK 0
#define PS2_MOUSE_ROTATE 90
#define PS2_MOUSE_USE_REMOTE_MODE
#define PS2_MOUSE_X_MULTIPLIER 3
#define PS2_MOUSE_Y_MULTIPLIER 3
#define PS2_MOUSE_V_MULTIPLIER 1
#define PS2_MOUSE_USE_2_1_SCALING
#define PS2_INT_INIT()  do {    \
    PCICR |= (1<<PCIE0);        \
} while (0)                       
#define PS2_INT_ON()  do {      \
    PCMSK0 |= (1<<PCINT5);      \
} while (0)                       
#define PS2_INT_OFF() do {      \
    PCMSK0 &= ~(1<<PCINT5);     \
} while (0)                       
#define PS2_INT_VECT   PCINT0_vect