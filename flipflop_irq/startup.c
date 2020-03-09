/*
 * 	startup.c
 *
 */
__attribute__((naked)) __attribute__((section (".start_section")) )
void startup ( void )
{
__asm__ volatile(" LDR R0,=0x2001C000\n");		/* set stack */
__asm__ volatile(" MOV SP,R0\n");
__asm__ volatile(" BL main\n");					/* call main */
__asm__ volatile(".L1: B .L1\n");				/* never return */
}

#define STK_CTRL ((volatile unsigned int *)(0xE000E010))
#define STK_LOAD ((volatile unsigned int *)(0xE000E014))
#define STK_VAL ((volatile unsigned int *)(0xE000E018))

#define EXTI ((volatile unsigned int *) (0x40013C00))

#define IRQ

#define GPIO_D 0x40020C00
#define GPIO_MODER ((volatile unsigned int *) (GPIO_D))
#define GPIO_OTYPER ((volatile unsigned short *) (GPIO_D+0x4))
#define GPIO_PUPDR ((volatile unsigned int *) (GPIO_D+0xC))
#define GPIO_IDR_LOW ((volatile unsigned char *) (GPIO_D+0x10))
#define GPIO_IDR_HIGH ((volatile unsigned char *) (GPIO_D+0x11))
#define GPIO_ODR_LOW ((volatile unsigned char *) (GPIO_D+0x14))
#define GPIO_ODR_HIGH ((volatile unsigned char *) (GPIO_D+0x15)) 

#define SYSCFG_EXTICR1 ((volatile unsigned short *) (0x40013808))
#define NVIC 0xE000E100
#define NVIC_ISER0 ((volatile unsigned int * ) (NVIC))

#define EXTI_IMR ((volatile unsigned int*) (0x40013C00))
#define EXTI_RTSR ((volatile unsigned int *) (0x40013C08))
#define EXTI_FTSR ((volatile unsigned int *) (0x40013C0C))

#define EXTI3_IRQ_BPOS (1<<3) //fråga om denna

#define EXTI_PR ((volatile unsigned int *) (0x40013C14))
#define EXTI_SWIER ((volatile unsigned int *) (0x40013C10))

#define DELAY_COUNT 100
static volatile int count;

/*
void systick_irq_handler( void )
{
    *STK_CTRL = 0;
    delay_count -- ;
    if( delay_count > 0 )
        delay_1mikro();
    else
        systick_flag = 1;
}*/

void irq_handler(void)
{
    if( (*EXTI_PR & EXTI3_IRQ_BPOS) != 0 )
        count++;
        *EXTI_PR |= EXTI3_IRQ_BPOS;
}

void init_app()
{
	*((unsigned long*) 0x40023830) = 0x18;
	*((unsigned long*) 0x40023844) |= 0x4000;
	*((unsigned long*) 0xE000ED08) = 0x2001C000;
    *((volatile unsigned int *) 0x40020C00) = 0x00005555; //Port-D alla utgångar (ska det vara så?)
   // *((volatile unsigned int *) 0x40021000) = 0x0; //port E 
     
    
    
    //Config sys
    *SYSCFG_EXTICR1 &= 0x0FFF; // Nollställ bitfältet 
    *SYSCFG_EXTICR1 |= 0x4000; //Ställ in 0100 för PE
    
    //config Exti
    *EXTI_IMR |= 0x8; //Öppna exti3 för avbrott
    *EXTI_RTSR |= 0x8; //positiv flank
    *EXTI_FTSR &= ~0x8;
    


    
    *( (void (**) (void) ) (0x2001C064)) = irq_handler;
    
        //config NVIC
    //NVIC_ISER0 |= (1<<9);  //nvic
    
    *((unsigned int *) 0xe000e100) |= (1<<9);

    
}

void main(void)
{
    init_app();
    *GPIO_ODR_LOW = 0x4;
    while(1)
    {
         *GPIO_ODR_LOW = count; //skriv count till utport
    }
}