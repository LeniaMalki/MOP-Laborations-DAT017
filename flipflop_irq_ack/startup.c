/*
 * 	startup.c
 *
 */
void startup(void) __attribute__((naked)) __attribute__((section (".start_section")) );

void startup ( void )
{
__asm volatile(
	" LDR R0,=0x2001C000\n"		/* set stack */
	" MOV SP,R0\n"
	" BL main\n"				/* call main */
	"_exit: B .\n"				/* never return */
	) ;
}
/*
 *	flipflop_irq_ack 
 * 
 */

#define PORT_E 0x40021000
#define PORT_E_Moder ((volatile unsigned int*) (PORT_E))
#define PORT_E_Otyper ((volatile unsigned short *) (PORT_E+0x4))
#define PORT_E_Ospeedr ((volatile unsigned int *) (PORT_E+0x8))
#define PORT_E_Pupdr ((volatile unsigned int *) (PORT_E+0xC1))
#define PORT_E_IdrLow ((volatile unsigned short *) (PORT_E+0x10))
#define PORT_E_IdrHigh ((volatile unsigned char *) (PORT_E+0x11))
#define PORT_E_OdrLow ((volatile unsigned char *) (PORT_E+0x14))
#define PORT_E_OdrHigh ((volatile unsigned char *) (PORT_E+0x14+1)) 

#define PORT_D 0x40020C00
#define PORT_D_Moder ((volatile unsigned int*) (PORT_D))
#define PORT_D_Otyper ((volatile unsigned short *) (PORT_D+0x4))
#define PORT_D_Ospeedr ((volatile unsigned int *) (PORT_D+0x8))
#define PORT_D_Pupdr ((volatile unsigned int *) (PORT_D+0xC))
#define PORT_D_IdrLow ((volatile unsigned short *) (PORT_D+0x10))
#define PORT_D_IdrHigh ((volatile unsigned char *) (PORT_D+0x11))
#define PORT_D_OdrLow ((volatile unsigned char *) (PORT_D+0x14))
#define PORT_D_OdrHigh ((volatile unsigned char *) (PORT_D+0x14+1))


#define SYSCFG_EXTICR1 ((volatile unsigned int*) (0x40013808))

#define EXTI 0x40013C00
#define EXTI_IMR ((volatile unsigned int*)(EXTI)) 
#define EXTI_RTSR ((volatile unsigned int*)(EXTI + 8))
#define EXTI_FTSR ((volatile unsigned int*)(EXTI + 0xC))
#define EXTI_PR ((volatile unsigned int*) (EXTI + 0x14))
#define EXTI3_IRQ3_BPOS (1<<3)

#define IRQ0 1
#define IRQ1 (1<<1)
#define IRQ2 (1<<2)


#define NVIC 0xE000E100
#define NVIC_ISER0 ((volatile unsigned int*)(NVIC))

//Global variables
volatile unsigned int count = 0;
void irq_handler(void);

void init_app(void){
    
     #ifdef USBDM
	 *( (unsigned long *) 0x40023830) = 0x18;
	 __asm volatile( " LDR R0,=0x08000209\n BLX R0 \n");
#endif
	*PORT_E_Moder = 0;
	*PORT_D_Moder &= 0;
	*PORT_D_Moder |= 0x55555555;
	*PORT_D_OdrHigh = 0;
	
	/* aktiverar NVIC för EXTI3*/
	*NVIC_ISER0 |= (1<<9);
	
	*((void(**)(void))0x2001C064) = irq_handler;
	/* Nollställ fält */
	*SYSCFG_EXTICR1 &= ~0xFFFF;
	/* PE3 -> EXTI3*/
	*SYSCFG_EXTICR1 |= 0x4444;
	
	*EXTI_IMR |= 0xF;
	*EXTI_FTSR |= 0xF;
	*EXTI_RTSR &= 0;
	
	

}

/* flipflop_irq
 void irq_handler(void)
{
	if((*EXTI_PR & EXTI3_IRQ3_BPOS) != 0){ 	//Om bit 3 i EXTI_PR är 1 --> trigg har upträtt
		count++;
		*EXTI_PR |= EXTI3_IRQ3_BPOS;
	}
}*/

void irq_handler(void)
{
	if((*EXTI_PR & EXTI3_IRQ3_BPOS) != 0){ 	//Om bit 3 i EXTI_PR är 1 --> trigg har upträtt
		
		if((*EXTI_PR & IRQ0) != 0) {
			*EXTI_PR |= IRQ0;
			count++;
		}
		else if((*EXTI_PR & IRQ1) != 0){
			*EXTI_PR |= IRQ1;
			count = 0;
		}
		else if((*EXTI_PR & IRQ2) != 0){
			*EXTI_PR |= IRQ2;
			*PORT_D_OdrHigh = ~*PORT_D_OdrHigh;		//Tänd/släck diodlampa
		}
		*EXTI_PR |= EXTI3_IRQ3_BPOS;
	}
}


void main(void)
{
	init_app();
	while(1){
		*PORT_D_OdrLow = count;
	}
}
