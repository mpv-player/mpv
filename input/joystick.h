
#define JOY_BASE   (0x100+128)
#define JOY_UP (JOY_BASE+0)
#define JOY_DOWN (JOY_BASE+1)
#define JOY_LEFT (JOY_BASE+2)
#define JOY_RIGHT (JOY_BASE+3)

#define JOY_BTN0 (JOY_BASE+4)
#define JOY_BTN1 (JOY_BASE+5)
#define JOY_BTN2 (JOY_BASE+6)
#define JOY_BTN3 (JOY_BASE+7)
#define JOY_BTN4 (JOY_BASE+8)
#define JOY_BTN5 (JOY_BASE+9)
#define JOY_BTN6 (JOY_BASE+10)
#define JOY_BTN7 (JOY_BASE+11)
#define JOY_BTN8 (JOY_BASE+12)
#define JOY_BTN9 (JOY_BASE+13)

int mp_input_joystick_init(char* dev);

int mp_input_joystick_read(int fd);


