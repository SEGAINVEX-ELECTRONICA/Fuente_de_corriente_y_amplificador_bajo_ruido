// Ficheros include
//-------------------------------------------
#include <reg922.h> 
#include <intrins.h> 	//de la librería de Keil. Para poder usar _nop_()
// Prototipos de funciones
//-------------------------------------------
void osc_init  (void);
void uart_init (void);
void configura_pin_led(void);
void keypad_init(void);
void keypad_isr( void);// interrupt 7 using 1
void timer_init(void);
void timer_isr1(void); //interrupt 3 using 2
void configura_puertos(void);
unsigned char detecta_tecla_pulsada(void);
void configura_fuente_de_corriente(void);
// Definición de pines del microcontrolador
//-------------------------------------------
sbit LED = P0 ^ 6;
sbit OK = P0 ^ 1;
// Declaración de variables	globales
//-------------------------------------------
static bit tecla_valida = 0;
static unsigned char pulsadores;
static unsigned char tecla;
static bit flag = 0;
unsigned int CiclosEspera,Contador;
//---------------------------------------------------------------------------------
// 									main
//---------------------------------------------------------------------------------
void main(void)
{
	osc_init(); // Inicializamos el oscilador interno
	configura_puertos(); // Lo primero coloca los pines con valores adecuados
	uart_init(); // Configura la UART	
	LED=0;		// Enciende el led
	if(!OK) // Si está pulsada la tecla para reprogramar espera un "start bootloader"
	{
		// Inicializa la UART  
		// Aviso luminoso de que está esperando un "start bootloader" por la uart
		
			for(CiclosEspera = 0;CiclosEspera<=200;CiclosEspera++) // Unos 13 segundos
			{			
						LED=1;
						while (--Contador);
						Contador=15000; // 
						LED=0;
					    while (--Contador);
						Contador=15000; // 
			}
			LED=0;		// Se terminó el tiempo de espera de un start bootloader por la UART
		
	}//del if(!OK) 
	// Fin del tiempo de espera para recibir un start bootloader
	// Inicialización
  	timer_init(); // Inicializa el timer 1 para hacer de antirebote de los pulsadores
	keypad_init(); // Inicializa el teclado, mascara e interrupción
//	EBO=1; // Habilita brownout detect
  	EA=1; // Habilita interrupciones en bloque
	PCON |= 0x03; // get into power down mode
	_nop_();_nop_();_nop_();_nop_();_nop_();_nop_();_nop_();_nop_();_nop_();_nop_();_nop_();_nop_();
	_nop_();_nop_();_nop_();_nop_();_nop_();_nop_();_nop_();_nop_();_nop_();_nop_();_nop_();_nop_();
	while(1) // endless loop
	{
		if(flag) // Hay que hacer esto para que salga del modo power down (if con un flag activado por el teclado)
		{
			flag=0;
			PCON |= 0x03; 
			_nop_();_nop_();_nop_();_nop_();_nop_();_nop_();_nop_();_nop_();_nop_();_nop_();
			_nop_();_nop_();_nop_();_nop_();_nop_();_nop_();_nop_();_nop_();_nop_();_nop_();
		}
	} // While
}
/*************************************************************************/
void Timer1_interrupt(void) interrupt 3 
{
	TR1 = 0; // stop timer
	// Lee el estado de los pulsadores
	pulsadores = P0; // save value of port 0
	pulsadores |= 0xE0; // Pone a 1 los bits 5,6 y 7 de pulsadores
	if (pulsadores != 0xFF) // If pulsadores != FF, means that a key was pressed
	{ 
		tecla = detecta_tecla_pulsada();
		if (tecla_valida )
		{
			configura_fuente_de_corriente();
		}
			flag=1;
	}
	// Espera a que se suelte la tecla antes de habilitar de nuevo la interrupción del teclado
	while (pulsadores!=0xFF) 
	{
		pulsadores = P0; // save value of port 0
		pulsadores |= 0xE0; // Pone a 1 los bits 5,6 y 7 de pulsadores
	}
	KBCON &= 0xFE; // reset keyb interrupt flag
	EKBI = 1; // enable keyb interrupt
}
/*************************************************************************/
void Keyboard_Interrupt(void) interrupt 7
{
	EKBI = 0; // disable keyb interrupt
	TR1 = 1; // start timer1
}
/***********************************************************************
CONTAINS:  Routines for configuring the timer 1
           Timer 1 generates an interrupt every 17.7ms
		   P89LPC922
************************************************************************/
void timer_init(void)
{
// configure timer 1 mode 1, 16 bit timer
  TMOD &= 0x0F;
  TMOD |= 0x10;
  TAMOD &= 0xEF;
  // timer values  17.7mS, 7.3728MHz
  TH1 = 0x01;
  TL1 = 0x1E;
  // set timer 1 isr priority to 3
//  IP0 &= 0xF7;
//  IP0H &= 0xF7;
//  IP0 |= 0x08;
//  IP0H |= 0x08;
  // enable Timer 1 interrupt  	
  ET1 = 1; 
}
/***********************************************************************
DESC:    Initializes the keypad
RETURNS: Nothing
CAUTION: EA must be set to 1 after calling this function
************************************************************************/
void keypad_init(void)
{
  // define pattern
  KBPATN = 0x1F;
  // define P0 pins that trigger interrupt
  KBMASK = 0x1F;
  // pattern must not match
  KBCON = 0x00;
  // set P0 pins as inputs	P0(0,1,2,3,4)
  P0M1 |= 0x1F;
  P0M2 &= ~0x1F;
// set isr priority to 2
//  IP1 &= 0xFD;
//  IP1H &= 0xFD;
//  IP1H |= 0x02;
  // enable keypad interrupt
  EKBI = 1;
}
/***********************************************************************
CONTAINS:  Routines for configuring the clock source on the Philips
           P89LPC922
         Uses on-chip oscillator
************************************************************************/
void osc_init(void)
{
  // clock divider
  DIVM = 0x00;
  // no clock out on CLKOUT pin
  TRIM &= ~0x40;
  // select low power clock (CPU Clock 8MHz or less)
  // AUXR1 |= 0x80;
} // osc_init
/***********************************************************************
DESC:    Initializes UART for mode 1
         Baudrate: 9600
************************************************************************/
void uart_init  ( void )
{
 // configure UART
  PCON &= ~0x40;
  SCON = 0x50;
  // set or clear SMOD1
  PCON &= 0x7f;
  PCON |= (0 << 8);
  SSTAT = 0x00;
  // enable break detect
  AUXR1 |= 0x40;
  // configure baud rate generator
  BRGCON = 0x00;
  BRGR0 = 0xF0;
  BRGR1 = 0x02;
  BRGCON = 0x03;
  // TxD = push-pull, RxD = input
  P1M1 &= ~0x01;
  P1M2 |= 0x01;
  P1M1 |= 0x02;
  P1M2 &= ~0x02;
  // set isr priority to 0
  IP0 &= 0xEF;
  IP0H &= 0xEF;
  // enable uart interrupt
  ES = 1;
} // uart_init

/***********************************************************************
DESC:    Initializa los pines para mandar los relés y los leds
RETURNS: Nothing
************************************************************************/
void configura_puertos(void)
{
	configura_pin_led();
}
/***********************************************************************
DESC:    Initializa el pin P0.6 como drenador abierto
RETURNS: Nothing
************************************************************************/
void configura_pin_led  ( void )
{
  //P0^5 = Open Drain
  P0M1 |= 0x40; //P0M1.6=1
  P0M2 |= 0x40;	//P0M2.6=1
} // configura_pin_led
/***********************************************************************
DESC:   	determina que tecla se ha pulsado. Modifica el flag
			tecla_valida			
RETURNS: el valor de la tecla pulsada 0 si error
************************************************************************/
unsigned char detecta_tecla_pulsada(void)
{
	unsigned char tecla_detectada;
	switch (pulsadores)
	{
		case 0xEF:
			tecla_detectada = 5;
			tecla_valida=1;			
		break;
		case 0xF7:
			tecla_detectada = 4;
			tecla_valida=1;			
		break;
		case 0xFB:
			tecla_detectada = 3;
			tecla_valida=1;			
		break;
		case 0xFD:
			tecla_detectada = 2;
			tecla_valida=1;			
		break;
		case 0xFE:
			tecla_detectada = 1;
			tecla_valida=1;			
		break;
		default:
		tecla_detectada = 0;
		tecla_valida=0;			
		break;
	}

return tecla_detectada;
}
/***********************************************************************
DESC:    configura los relés y los leds de la fuente de corriente
RETURNS: nada
************************************************************************/
void configura_fuente_de_corriente(void)
{
			for(CiclosEspera = 0;CiclosEspera<=tecla;CiclosEspera++) // Unos 5 segundos 500*10ms
			{			
						LED=1;
						while (--Contador);
						Contador=10000; // 
						LED=0;
					    while (--Contador);
						Contador=10000; // 
			}
			LED=0;		// Se terminó el tiempo de espera de un start bootloader por la UART
}
/************************************************************************/