/*
 * 	startup.c
 *
 */
void startup(void) __attribute__((naked)) __attribute__((section(".start_section")));

void startup(void)
{
    __asm volatile(" LDR R0,=0x2001C000\n" /* set stack */
                   " MOV SP,R0\n"
                   " BL main\n"   /* call main */
                   "_exit: B .\n" /* never return */
    );
}

// ###################################################### DEFINITIONS
// ######################################################################################

// Definerar word-adresser för initieringar
#define GPIO_D 0x40020c00 /* MD407 port D */

// Definerar alla in och utportar
#define GPIO_D_MODER ((volatile unsigned int*)(GPIO_D))
#define GPIO_D_OTYPER ((volatile unsigned short*)(GPIO_D + 0x4))
#define GPIO_D_PUPDR ((volatile unsigned int*)(GPIO_D + 0xC))
#define GPIO_D_IDR_LOW ((volatile unsigned char*)(GPIO_D + 0x10))
#define GPIO_D_IDR_HIGH ((volatile unsigned char*)(GPIO_D + 0x11))
#define GPIO_D_ODR_LOW ((volatile unsigned char*)(GPIO_D + 0x14))
#define GPIO_D_ODR_HIGH ((volatile unsigned char*)(GPIO_D + 0x15))

#define GPIO_E 0x40021000 /* MD407 port E */
#define GPIO_MODER ((volatile unsigned int*)(GPIO_E))
#define GPIO_OTYPER ((volatile unsigned short*)(GPIO_E + 0x4))
#define GPIO_OSPEEDR ((volatile unsigned int*)(GPIO_E + 0x8))
#define GPIO_PUPDR ((volatile unsigned int*)(GPIO_E + 0xC))

// Definerar byte-adresser för data och styrregister
#define GPIO_IDR_LOW ((volatile unsigned char*)(GPIO_E + 0x10))
#define GPIO_IDR_HIGH ((volatile unsigned char*)(GPIO_E + 0x11))
#define GPIO_ODR_LOW ((volatile unsigned char*)(GPIO_E + 0x14))
#define GPIO_ODR_HIGH ((volatile unsigned char*)(GPIO_E + 0x15))

// Definitioner för initieringar av SYSTCK
#define STK_CTRL ((volatile unsigned int*)(0xE000E010))
#define STK_LOAD ((volatile unsigned int*)(0xE000E014))
#define STK_VAL ((volatile unsigned int*)(0xE000E018))

// Definitioner för bitar i styrregistret
#define B_E 0x40   // Enable
#define B_RST 0x20 // Reset
#define B_CS2 0x10 // Halva 2
#define B_CS1 8    // Halva 1
#define B_SELECT 4 // 0 Graphics, 1 ASCII
#define B_RW 2     // 0 Write, 1 Read
#define B_RS 1     // 0 Command, 1 Data

// Definition av kommandon till display, sänds via dataregister.
#define LCD_ON 0x3F         // Display on
#define LCD_OFF 0x3E        // Display off
#define LCD_SET_ADD 0x40    // Set horizontal coordinate
#define LCD_SET_PAGE 0xB8   // Set vertical coordinate
#define LCD_DISP_START 0xC0 // Start address
#define LCD_BUSY 0x80       // Read busy status

// Andra definitioner
typedef unsigned char uint8_t;
//#define SIMULATOR
#define USBDM

typedef struct tPoint {
    unsigned char x;
    unsigned char y;
} POINT;

#define MAX_POINTS 30
unsigned char L = 0;
unsigned char R = 0;

const char pl_win_msg[] = "PL wins!";
const char pr_win_msg[] = "PR wins!";

typedef struct tGeometry {
    int numpoints, sizeX, sizeY;
    POINT px[MAX_POINTS];
} GEOMETRY, *PGEOMETRY;

GEOMETRY ball_geometry = { .numpoints = 12,
    .sizeX = 4,
    .sizeY = 4,
    { /* px[0,1,2 ....] */
        { 0, 1 }, { 0, 2 }, { 1, 0 }, { 1, 1 }, { 1, 2 }, { 1, 3 }, { 2, 0 }, { 2, 1 }, { 2, 2 }, { 2, 3 }, { 3, 1 },
        { 3, 2 } } };

//////////////////// NEW ///////////////////////
GEOMETRY paddle_geometry = { .numpoints = 30,
    .sizeX = 2,
    .sizeY = 15,
    { /* px[0,1,2 ....] */
        { 0, 1 }, { 0, 2 }, { 0, 3 }, { 0, 4 }, { 0, 5 }, { 0, 6 }, { 0, 7 }, { 0, 8 }, { 0, 9 }, { 0, 10 }, { 0, 11 },
        { 0, 12 }, { 0, 13 }, { 0, 14 }, { 1, 0 }, { 1, 1 }, { 1, 2 }, { 1, 3 }, { 1, 4 }, { 1, 5 }, { 1, 6 }, { 1, 7 },
        { 1, 8 }, { 1, 9 }, { 1, 10 }, { 1, 11 }, { 1, 12 }, { 1, 13 }, { 1, 14 } } };
//////////////////// NEW ///////////////////////

typedef struct tObj {
    PGEOMETRY geo;
    int dirx, diry;
    int posx, posy;
    void (*draw)(struct tobj*);
    void (*clear)(struct tobj*);
    void (*move)(struct tobj*);
    void (*set_speed)(struct tobj*, int, int);
    // void (set_pos)(struct tobj, int, int);
    // void (check_ball_collision)(struct tobj);
} OBJECT, *POBJECT;

// ############################################################ DELAYS
void delay_250ns(void)
{
    /* SystemCoreClock = 168000000 */
    *STK_CTRL = 0;
    *STK_LOAD = ((168 / 4) - 1);
    *STK_VAL = 0;
    *STK_CTRL = 5;
    while((*STK_CTRL & 0x10000) == 0)
        ;
    *STK_CTRL = 0;
}

void delay_micro(unsigned int us)
{
#ifdef SIMULATOR
    us = us / 100000;
    us++;
#endif
    for(int i = 0; i < us; i++) {
        delay_250ns();
        delay_250ns();
        delay_250ns();
        delay_250ns();
    }
}

void delay_milli(unsigned int ms)
{
#ifdef SIMULATOR
    ms = ms / 100000;
    ms++;
#endif
    while(ms > 0) {
        delay_micro(1000);
        ms--;
    }
}

void delay_500ns(void)
{
    delay_250ns();
    delay_250ns();
}

// @@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@ ASCII

void ascii_ctrl_bit_set(unsigned char x)
{
    unsigned char c;
    c = *GPIO_ODR_LOW;
    c |= (B_SELECT | x);
    *GPIO_ODR_LOW = c;
}

void ascii_ctrl_bit_clear(unsigned char x)
{
    unsigned char c;
    c = *GPIO_ODR_LOW;
    c &= (B_SELECT | ~x);
    *GPIO_ODR_LOW = c;
}

void ascii_write_controller(unsigned char c)
{
    ascii_ctrl_bit_set(B_E);
    *GPIO_ODR_HIGH = c;
    ascii_ctrl_bit_clear(B_E);
    delay_250ns();
}

unsigned char ascii_read_controller(void)
{
    unsigned char c;
    ascii_ctrl_bit_set(B_E);
    delay_250ns();
    delay_250ns();
    c = *GPIO_IDR_HIGH;
    ascii_ctrl_bit_clear(B_E);
    return c;
}

unsigned char ascii_read_status(void)
{
    unsigned char c;
    *GPIO_MODER = 0x00005555;
    ascii_ctrl_bit_set(B_RW);
    ascii_ctrl_bit_clear(B_RS);
    c = ascii_read_controller();
    *GPIO_MODER = 0x55555555;
    return c;
}

unsigned char ascii_read_data(void)
{
    *GPIO_MODER = 0x00005555;
    ascii_ctrl_bit_set(B_RS);
    ascii_ctrl_bit_set(B_RW);
    unsigned char rv = ascii_read_controller();
    *GPIO_MODER = 0x55555555;
    return rv;
}

void ascii_write_cmd(unsigned char command)
{
    ascii_ctrl_bit_clear(B_RS);
    ascii_ctrl_bit_clear(B_RW);
    ascii_write_controller(command);
}

void ascii_write_data(unsigned char data)
{
    ascii_ctrl_bit_set(B_RS);
    ascii_ctrl_bit_clear(B_RW);
    ascii_write_controller(data);
}

void ascii_command(unsigned char command)
{
    while((ascii_read_status() & 0x80) == 0x80)
        ;
    delay_micro(8);
    ascii_write_cmd(command);
}

void ascii_write_char(unsigned char chr)
{
    while(1)
        if(ascii_read_status) {
            delay_micro(8);
            ascii_write_data(chr);
            delay_micro(43);
            break;
        }
}

void ascii_gotoxy(int x, int y)
{
    int address = x - 1;
    if(y == 2) {
        address = address + 0x40;
    }
    ascii_write_cmd(0x80 | address);
    delay_micro(39);
}

void ascii_clear_screen(void)
{
    ascii_gotoxy(1, 1);
    char* t = "                         ";
    while(*t)
        ascii_write_char(*t++);
    ascii_gotoxy(1, 2);
    t = "                         ";
    while(*t)
        ascii_write_char(*t++);
}

// ############################################################ FUNTCIONS
void graphic_ctrl_bit_set(uint8_t x)
{
    uint8_t c;
    c = *GPIO_ODR_LOW;
    c &= ~B_SELECT;
    c |= (~B_SELECT & x);
    *GPIO_ODR_LOW = c;
}

void graphic_ctrl_bit_clear(uint8_t x)
{
    uint8_t c;
    c = *GPIO_ODR_LOW;
    c &= ~B_SELECT;
    c &= ~x;
    *GPIO_ODR_LOW = c;
}

void select_controller(uint8_t controller)
{
    switch(controller) {
    case 0:
        graphic_ctrl_bit_clear(B_CS1 | B_CS2);
        break;
    case B_CS1:
        graphic_ctrl_bit_set(B_CS1);
        graphic_ctrl_bit_clear(B_CS2);
        break;
    case B_CS2:
        graphic_ctrl_bit_set(B_CS2);
        graphic_ctrl_bit_clear(B_CS1);
        break;
    case B_CS1 | B_CS2:
        graphic_ctrl_bit_set(B_CS1 | B_CS2);
        break;
    }
}
static void graphic_wait_ready(void)
{
    uint8_t c;

    graphic_ctrl_bit_clear(B_E);
    *GPIO_MODER = 0x00005555; // 15-8 inputs, 7-0 outputs
    graphic_ctrl_bit_clear(B_RS);
    graphic_ctrl_bit_set(B_RW);
    delay_500ns();

    while(1) {
        graphic_ctrl_bit_set(B_E);
        delay_500ns();
        c = *GPIO_IDR_HIGH & LCD_BUSY;
        graphic_ctrl_bit_clear(B_E);
        delay_500ns();
        if(c == 0)
            break;
    }
    *GPIO_MODER = 0x55555555; // 15-0 outputs
}

static uint8_t graphic_read(uint8_t controller)
{
    uint8_t c;
    graphic_ctrl_bit_clear(B_E);
    *GPIO_MODER = 0x00005555; // 15-8 inputs, 7-0 outputs
    graphic_ctrl_bit_set(B_RS | B_RW);
    select_controller(controller);
    delay_500ns();
    graphic_ctrl_bit_set(B_E);
    delay_500ns();
    c = *GPIO_IDR_HIGH;
    graphic_ctrl_bit_clear(B_E);
    *GPIO_MODER = 0x55555555; // 15-0 outputs

    if(controller & B_CS1) {
        select_controller(B_CS1);
        graphic_wait_ready();
    }
    if(controller & B_CS2) {
        select_controller(B_CS2);
        graphic_wait_ready();
    }
    return c;
}

static uint8_t graphic_read_data(uint8_t controller)
{
    graphic_read(controller);
    return graphic_read(controller);
}

static void graphic_write(uint8_t value, uint8_t controller)
{
    *GPIO_ODR_HIGH = value;
    select_controller(controller);
    delay_500ns();
    graphic_ctrl_bit_set(B_E);
    delay_500ns();
    graphic_ctrl_bit_clear(B_E);

    if(controller & B_CS1) {
        select_controller(B_CS1);
        graphic_wait_ready();
    }
    if(controller & B_CS2) {
        select_controller(B_CS2);
        graphic_wait_ready();
    }
}

void graphic_write_data(uint8_t data, uint8_t controller)
{
    graphic_ctrl_bit_clear(B_E);
    select_controller(controller);
    graphic_ctrl_bit_set(B_RS);
    graphic_ctrl_bit_clear(B_RW);
    graphic_write(data, controller);
}

void graphic_write_command(uint8_t command, uint8_t controller)
{
    graphic_ctrl_bit_clear(B_E);
    select_controller(controller);
    graphic_ctrl_bit_clear(B_RS);
    graphic_ctrl_bit_clear(B_RW);
    graphic_write(command, controller);
}

void graphic_clear_screen(void)
{
    uint8_t i, j;
    for(j = 0; j < 8; j++) {
        graphic_write_command(LCD_SET_PAGE | j, B_CS1 | B_CS2);
        graphic_write_command(LCD_SET_ADD | 0, B_CS1 | B_CS2);
        for(i = 0; i <= 63; i++) {
            graphic_write_data(0, B_CS1 | B_CS2);
        }
    }
}

void graphic_initialize(void)
{
    graphic_ctrl_bit_set(B_E);
    delay_micro(10);
    graphic_ctrl_bit_clear(B_CS1 | B_CS2 | B_RST | B_E);
    delay_milli(30);
    graphic_ctrl_bit_set(B_RST);
    delay_milli(100);
    graphic_write_command(LCD_OFF, B_CS1 | B_CS2);
    graphic_write_command(LCD_ON, B_CS1 | B_CS2);
    graphic_write_command(LCD_DISP_START, B_CS1 | B_CS2);
    graphic_write_command(LCD_SET_ADD, B_CS1 | B_CS2);
    graphic_write_command(LCD_SET_PAGE, B_CS1 | B_CS2);
    select_controller(0);
}

void pixel(int x, int y, int set)
{
    uint8_t mask, c, controller;
    int index;
    if((x < 1) || (y < 1) || (x > 128) || (y > 64))
        return;

    index = (y - 1) / 8;

    switch((y - 1) % 8) {
    case 0:
        mask = 1;
        break;
    case 1:
        mask = 2;
        break;
    case 2:
        mask = 4;
        break;
    case 3:
        mask = 8;
        break;
    case 4:
        mask = 0x10;
        break;
    case 5:
        mask = 0x20;
        break;
    case 6:
        mask = 0x40;
        break;
    case 7:
        mask = 0x80;
        break;
    }

    if(set == 0)
        mask = ~mask;
    if(x > 64) {
        controller = B_CS2;
        x = x - 65;
    } else {
        controller = B_CS1;
        x = x - 1;
    }

    graphic_write_command(LCD_SET_ADD | x, controller);
    graphic_write_command(LCD_SET_PAGE | index, controller);
    c = graphic_read_data(controller);
    graphic_write_command(LCD_SET_ADD | x, controller);
    if(set)
        mask = mask | c;
    else
        mask = mask & c;
    graphic_write_data(mask, controller);
    graphic_write_command(LCD_ON, B_CS1 | B_CS2);
}

void set_object_speed(POBJECT o, int speedX, int speedY)
{
    o->dirx = speedX;
    o->diry = speedY;
}

void draw_object(POBJECT O)
{
    for(int i = 0; i < 12; i++) {
        pixel(O->posx + O->geo->px[i].x, O->posy + O->geo->px[i].y, 1);
    }
}

void clear_object(POBJECT O)
{
    for(int i = 0; i < MAX_POINTS; i++) {
        pixel(O->posx + O->geo->px[i].x, O->posy + O->geo->px[i].y, 0);
    }
}

void move_object(POBJECT object)
{

    clear_object(object);
    object->posx += object->dirx;
    object->posy += object->diry;

    clear_object(object);
    if(object->posx < 1) {
        R++; // Add points to right player
        if (R == 3)
        {
            object->dirx = 0; 
            object->diry = 0; 
        }

        object->posx = 128/2;
        object->posy = 64/2;
    }
    if((object->posx + object->geo->sizeX) > 128) {
        object->dirx = -(object->dirx);
    
        L++;
         if (L == 3)
        {
            object->dirx = 0; 
            object->diry = 0; 
        }

        // Add points to Left player

        object->posx = (60 - (object->geo->sizeX));
        object->posy = (30 - (object->geo->sizeY));
    }
    if(object->posy < 1) {
        object->diry = -object->diry;
        object->posy = 1;
    }
    if(object->posy + object->geo->sizeY > 64) {
        object->diry = -object->diry;
        object->posy = (64 - (object->geo->sizeY));
    }
    draw_object(object);
}

static OBJECT ball = {

    .geo = &ball_geometry,
    .dirx = 3,
    .diry = -3,
    .posx = 128/2,
    .posy = 64/2,

    draw_object,
    clear_object,
    move_object,
    set_object_speed
};

static OBJECT paddleLeft = {

    .geo = &paddle_geometry,
    .dirx = 0,
    .diry = 0,
    .posx = 5,
    .posy = 27,

    draw_object,
    clear_object,
    move_object,
    set_object_speed
};
static OBJECT paddleRight = {

    .geo = &paddle_geometry,
    .dirx = 0,
    .diry = 0,
    .posx = 123,
    .posy = 27,

    draw_object,
    clear_object,
    move_object,
    set_object_speed
};

void activeRow(unsigned char row)
{
    switch(row) {
    case 0:
        *GPIO_D_ODR_HIGH = 0x00;
        break;
    case 1:
        *GPIO_D_ODR_HIGH = 0x10;
        break;
    case 2:
        *GPIO_D_ODR_HIGH = 0x20;
        break;
    case 3:
        *GPIO_D_ODR_HIGH = 0x40;
        break;
    case 4:
        *GPIO_D_ODR_HIGH = 0x80;
        break;
    }
}

unsigned char checkColLeft()
{
    unsigned char input = *GPIO_D_IDR_HIGH;
    input &= 0x0F;
    if(input == 0x01) {
        return 1;
    } else {
        return 0;
    }
}
unsigned char checkColRight()
{
    unsigned char input = *GPIO_D_IDR_HIGH;
    input &= 0x0F;
    if(input == 0x08) {
        return 4;
    } else {
        return 0;
    }
}

unsigned char keybL(void)
{
    unsigned char keys[] = { 0x1, 0x2, 0x3, 0xA, 0x4, 0x5, 0x6, 0xB, 0x7, 0x8, 0x9, 0xC, 0xE, 0x0, 0xF, 0xD };
    for(unsigned char row = 1; row <= 4; row++) {
        activeRow(row);
        unsigned char col = checkColLeft();

        if(col != 0) {
            activeRow(0);
            return keys[4 * (row - 1) + (col - 1)];
        }
    }
    return 0xFF;
}
unsigned char keybR(void)
{
    unsigned char keys[] = { 0x1, 0x2, 0x3, 0xA, 0x4, 0x5, 0x6, 0xB, 0x7, 0x8, 0x9, 0xC, 0xE, 0x0, 0xF, 0xD };
    for(unsigned char row = 1; row <= 4; row++) {
        activeRow(row);
        unsigned char col = checkColRight();

        if(col != 0) {
            activeRow(0);
            return keys[4 * (row - 1) + (col - 1)];
        }
    }
    return 0xFF;
}
void controllPaddleR(POBJECT p)
{
    unsigned char c;
    c = keybR();
    switch(c) {
    case 0xA:
        p->set_speed(p, 0, -3);
        break;
    case 0xC:
        p->set_speed(p, 0, 3);
        break;
    }
}
void controllPaddleL(POBJECT p)
{
    unsigned char c;
    c = keybL();
    switch(c) {
    case 1:
        p->set_speed(p, 0, -3);
        break;
    case 7:
        p->set_speed(p, 0, 3);
        break;
    }
}

void resetspeed(POBJECT pL, POBJECT pR)
{
    pL->set_speed(pL, 0, 0);
    pR->set_speed(pR, 0, 0);
}
void ballPaddeIntersect(POBJECT ball, POBJECT paddle)
{

    if(((ball->posx <= paddle->posx + paddle->geo->sizeX) || (ball->posx + ball->geo->sizeX >= paddle->posx)) &&
        (ball->posy > paddle->posy) && (ball->posy < paddle->posy + paddle->geo->sizeY)) {

        ball->dirx = -ball->dirx;
    }
}

void show_result(void)
{
    ascii_clear_screen(); 
    char pL[] = "PL: ";
    char pR[] = "PR: ";
    char* s;
    
    ascii_gotoxy(1, 1);
    s = pL;
    while(*s)
    ascii_write_char(*s++);
    ascii_write_char(L + '0');
        ascii_gotoxy(1, 2);

    s = pR;
    while(*s)
        ascii_write_char(*s++);
    ascii_write_char(R + '0');
    
}
void ascii_init(void)
{
	while((ascii_read_status() & 0x80) == 0x80){}
		delay_micro(8);
		ascii_write_cmd(0x38);
		delay_micro(39);
		ascii_write_cmd(0x0E);
		delay_micro(39);
		ascii_write_cmd(0x06);
		delay_micro(39);
}
void init_app(void)
{
#ifdef USBDM
    *((unsigned long*)0x40023830) = 0x18;
    __asm volatile(" LDR R0,=0x08000209\n BLX R0 \n");
#endif
    *GPIO_MODER = 0x55555555; // display

    *GPIO_D_MODER = 0x55005555;
    *GPIO_D_OTYPER = 0x0000;
    *GPIO_D_PUPDR = 0x55AA55AA;
}
unsigned char checkRestartKey()
{
    unsigned char input = *GPIO_D_IDR_HIGH;
    input &= 0x0F;
    if(input == 0x02) {
        return 2;
    } else {
        return 0;
    }
}

unsigned char keybRestart(){
     unsigned char keys[] = { 0x1, 0x2, 0x3, 0xA, 0x4, 0x5, 0x6, 0xB, 0x7, 0x8, 0x9, 0xC, 0xE, 0x0, 0xF, 0xD };
    for(unsigned char row = 1; row <= 4; row++) {
        activeRow(row);
        unsigned char col = checkRestartKey();

        if(col != 0) {
            activeRow(0);
            return keys[4 * (row - 1) + (col - 1)];
        }
    }
}

void restart(POBJECT b) {
     unsigned char c=4;
    c = keybRestart();
    if(c==0){
        b->set_speed(b, 3, -3);
        b->posx=128/2;
        b->posy=64/2;
        L = 0; 
        R = 0; 
        
        graphic_clear_screen(); 
    }
        
    }


// ################################################################## MAIN
int main(int argc, char** argv)
{
    POBJECT b = &ball;
    POBJECT pL = &paddleLeft;
    POBJECT pR = &paddleRight;

    init_app();
    ascii_init();
    graphic_initialize();
    graphic_clear_screen();

    while(1) {
        restart(b);
        b->move(b);
        delay_milli(40);
        pL->move(pL);
        delay_milli(40);
        pR->move(pR);
        resetspeed(pL, pR);
        show_result (); 
        controllPaddleL(pL);
        controllPaddleR(pR);
        

        if(b->posx <= 10 && b->dirx < 0) {
            ballPaddeIntersect(b, pL);
        } else if(b->posx >= 118 && b->dirx > 0) {
            ballPaddeIntersect(b, pR);
        }
    }
}