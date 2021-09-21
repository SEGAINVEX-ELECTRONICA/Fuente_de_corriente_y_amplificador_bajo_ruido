/* Proyecto OT:2030734
Para un microcontrolador P89LPC933
Implementa un teclado con 3 teclas y leds para indicar el estado del sistema.

Con las teclas se actuan relés que configuran una fuente de corriente de 5
décadas. 

Se comporta como un conmutador para seleccionar la corriente de salida
, un interruptor de 3 posiciones para seleccionar la referencia (interna, 
interna invertida o externa) y un conmutador de apagado encendido.

Tras pulsar una tecla configura la fuente y entra en modo power down. Al
pulsar una tecla de nuevo despierta de power down.

Para flasear el micro hay que encender el sistema pulsando la tecla ON_OFF,
El led ON_OFF parpadea durante 13 segundos. Durante ese tiempo se puede 
enviar un "Break Condition" con Flash Magic para que el micro entre en la
rutina del Bootloader.

Patricio Coronado Universidad Autónoma de Madrid SEGAINVEX-ELECTRONICA
10 de julio de 2014. V1.0
*/
// Ficheros include
//-------------------------------------------
#include "REG933.H"
#include <intrins.h> 	//de la librería de Keil. Para poder usar _nop_()
//-------------------------------------------
// Declaración de tipos
enum e_tecla {sw_on_off,sw_reference,sw_range,sw_error}; // Para el estado las teclas
enum e_rango {i10uA, i100uA, i1mA, i10mA, i100mA}; // Para el estado de la corriente de salida
enum e_fuente {encendida, apagada}; // Para el estado de la salida
enum e_referencia {interna, interna_invertida, externa}; // Para el estado de la referencia
//-------------------------------------------
// Prototipos de funciones
void keypad_isr  (  void  );
void timers_isr1  (  void  );
void osc_init  (  void  );
void timers_init  (  void  );
void keypad_init  (  void  );
void uart_init  (  void  );
void ports_init  (  void  );
void configura_fuente_de_corriente(enum e_tecla);
enum e_tecla detecta_tecla_pulsada(void);
//-------------------------------------------
// Definición de pines de la aplicación
// define pin names
sbit p00 			= P0 ^ 0;
sbit SW_ON_OFF 		= P0 ^ 1;
sbit SW_REFERENCE 	= P0 ^ 2;
sbit SW_RANGE 		= P0 ^ 3;
sbit RELE_ON_OFF 	= P0 ^ 4;
sbit RELE_IN_EX 	= P0 ^ 5;
sbit p06 			= P0 ^ 6;
sbit p07 			= P0 ^ 7;
sbit LED_1MA 		= P1 ^ 2;
sbit LED_10MA 		= P1 ^ 3;
sbit LED_100MA 		= P1 ^ 4;
sbit RELE_NIVEL_2 	= P1 ^ 6;
sbit RELE_NIVEL_1 	= P1 ^ 7;
sbit LED_ON_OFF 	= P2 ^ 0;
sbit LED_REF_EXT 	= P2 ^ 1;
sbit LED_100UA 		= P2 ^ 2;
sbit LED_10UA 		= P2 ^ 3;
sbit p24 			= P2 ^ 4;
sbit p25 			= P2 ^ 5;
sbit LED_REF_INT_INV = P2 ^ 6;
sbit LED_REF_INT 	= P2 ^ 7;
sbit RELE_REF 		= P3 ^ 0;
sbit RELE_NIVEL_3 	= P3 ^ 1;
//---------------------------------------------------------------------------------
// Variables globales
bit tecla_valida = 0;
unsigned char pulsadores;
unsigned int CiclosEspera,Contador;
// Estatus del sistema
enum e_rango rango;
enum e_fuente fuente;
enum e_referencia referencia;
bit flag=1; // Bit para depurar
bit pwdw=0; // Para que entre en power down de forma condicional
//---------------------------------------------------------------------------------
// Constantes
#define RELE_ON 1
#define RELE_OFF 0
#define LED_ON 0
#define LED_OFF 1
// Para depurar
#define TEST	if (flag) {LED_100MA=LED_ON;flag=0;} else {LED_100MA=LED_OFF;flag=1;}

//---------------------------------------------------------------------------------
// 									main
//---------------------------------------------------------------------------------
void main(void)
{
	osc_init(); // Inicializamos el oscilador interno
	ports_init(); // Lo primero coloca los pines con valores adecuados
	uart_init(); // Configura la UART para poder flasear el micro	
	if(!SW_ON_OFF)//Si está pulsada la tecla para flasear espera un "Break Condition"
	{	
		// Aviso luminoso de que la UART espera un "Break Condition"
		for(CiclosEspera = 0;CiclosEspera<=200;CiclosEspera++) // Unos 13 segundos
		{			
			LED_ON_OFF = LED_ON;
			while (--Contador);
				Contador=15000; // 
			LED_ON_OFF = LED_OFF;
		    while (--Contador);
				Contador=15000; // 
		}
		LED_ON_OFF= LED_OFF;// Se terminó el tiempo de espera de un "Break Condition" por la UART
	}//del if(!SW_ON_OFF) 
	// Inicialización del funcionamiento normal
  	timers_init(); // Inicializa el timer 1 para hacer de antirebote de los pulsadores
	keypad_init(); // Inicializa el teclado, mascara e interrupción
	EBO=1; // Habilita brownout detect
  	EA=1; // Habilita interrupciones en bloque
	while(1) // bucle infinito
	{
		if(pwdw) // Si se activa el flag para entrar en power down, lo hace
		{
			pwdw=0;	// Borra el flag que le permite entrar en el if del power down
			// Esto es necesario para que no se aniden interrupciones del teclado
			// y se quede en estado de que no sale del power down
			KBCON &= 0xFE; // reset keyb interrupt flag
			EKBI = 1; // enable keyb interrupt  
			PCON |= 0x03;// Total power down
			_nop_();_nop_();_nop_();_nop_();_nop_();_nop_();
		} //if(pwdw) 
	} // While
}

/***********************************************************************
DESC:    Initializes UART for mode 1
         Baudrate: 9600
RETURNS: Nothing
CAUTION: If interrupts are being used then EA must be set to 1
         after calling this function
************************************************************************/
void uart_init  (  void  )
{
  // configure UART
  // clear SMOD0
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
  // Configura los pines de la UART
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
DESC:    Inicializa las teclas, la interrupción del teclado y la máscara
RETURNS: Nothing
CAUTION: EA must be set to 1 after calling this function
************************************************************************/
void keypad_init  (  void  )
{
  // define pattern
  KBPATN = 0x0E;
  // define P0 pins that trigger interrupt
  KBMASK = 0x0E;
  // pattern must not match
  KBCON = 0x00;
  // set P0 pins as inputs
  P0M1 |= 0x0E;
  P0M2 &= ~0x0E;
// set isr priority to 3
  IP1 &= 0xFD;
  IP1H &= 0xFD;
  IP1 |= 0x02;
  IP1H |= 0x02;
  // Habilita la interrupción del teclado
  EKBI = 1;
}
/***********************************************************************
DESC:    Inicializa los pines de los puertos para relés o leds y 
		 pone la fuente en su estado por defecto
RETURNS: Nothing
CAUTION: Call before the initialization functions of other peripherals
************************************************************************/
void ports_init  (  void  )
{
// Configuración de los pines
// Los pines de la UART y el teclado no se configuran aquí
	 P0M1 &= 0xCE;
	 P0M2 |= 0x31;
	 P1M1 &= 0x3F;
	 P1M1 |= 0x3C;
	 P1M2 &= 0xDF;
	 P1M2 |= 0xDC;
	 P2M1 &= 0xCF;
	 P2M1 |= 0xCF;
	 P2M2 &= 0xCF;
	 P2M2 |= 0xCF;
	 P3M1 &= 0xFC;
	 P3M2 |= 0x03;
	// Coloca los pines en la posición inicial
	RELE_ON_OFF		= RELE_OFF;
	RELE_IN_EX 		= RELE_OFF;
	RELE_REF 		= RELE_OFF;
	RELE_NIVEL_3	= RELE_OFF;
	RELE_NIVEL_2	= RELE_OFF;
	RELE_NIVEL_1	= RELE_OFF;
	LED_10UA 		= LED_ON;
	LED_100UA 		= LED_OFF;
	LED_1MA 		= LED_OFF;
	LED_10MA 		= LED_OFF;
	LED_100MA 		= LED_OFF;
	LED_ON_OFF 		= LED_OFF;
	LED_REF_EXT 	= LED_ON;
	LED_REF_INT_INV = LED_OFF;
	LED_REF_INT 	= LED_OFF;
	// Estatus del sistema tras reset o encendido
	rango = i10uA;
	fuente = apagada;
	referencia = externa;
} // ports_init
/***********************************************************************
DESC:    Initializes the oscillator to generate a CPU clock at
         7.3728 MHz

         Uses on-chip oscillator
RETURNS: Nothing
************************************************************************/
extern void osc_init  (  void  )
{
  // clock divider
  DIVM = 0x00;
  // no clock out on CLKOUT pin
  TRIM &= ~0x40;
  // select low power clock (CPU Clock 8MHz or less)
//  AUXR1 |= 0x80;
} // osc_init
/***********************************************************************
DESC:    Initializes timers
         Timer 0 is not used
         Timer 1 generates an interrupt every 17.7ms
RETURNS: Nothing
CAUTION: If interrupts are being used then EA must be set to 1
         after calling this function
************************************************************************/
void timers_init   (  void  )
{
  // configure timer 1
  TMOD &= 0x0F;
  TMOD |= 0x10;
  TAMOD &= 0xEF;
  // timer values
  TH1 = 0x01;
  TL1 = 0x1E;
  // set timer 1 isr priority to 0
  //IP0 &= 0xF7;
  //IP0H &= 0xF7;
  // enable timer 1 interrupt
  ET1 = 1;
} // timers_init
/***********************************************************************
DESC:    Keypad Interrupt Service Routine
RETURNS: Nothing
CAUTION: keypad_init must be called first
         EA must be set to 1
************************************************************************/
void keypad_isr  (  void  ) interrupt 7 using 1
{
  // clear interrupt flag
	EKBI = 0; // disable keyb interrupt
	TR1 = 1; // start timer1
}

/***********************************************************************
DESC:    Timer 1 Interrupt Service Routine
RETURNS: Nothing
CAUTION: timers_init must be called first
         EA must be set to 1
************************************************************************/
void timers_isr1  (  void  ) interrupt 3 using 2
{
	enum e_tecla tecla_pulsada;
	TR1 = 0; // detiene el timer
	// Lee el estado de los pulsadores
	pulsadores = P0; // salva el valor del puerto 0
	pulsadores |= 0xF1; // Pone a 1 los bits 0, 4, 5, 6 y 7 de pulsadores
	if (pulsadores != 0xFF) // Si pulsadores es distinto de FF se han pulsado teclas
	{ 
		tecla_pulsada = detecta_tecla_pulsada(); // Mira que tecla es
		if (tecla_valida ) // y si es una tecla válida...
		{
			configura_fuente_de_corriente(tecla_pulsada);// Actua sobre la fuente
		}
	}
	// Espera a que se suelte la tecla antes de habilitar de nuevo la interrupción del teclado
	while (pulsadores!=0xFF) // Lee el teclado hasta que se suelta la tecla 
	{
		pulsadores = P0; // salva el valor del puerto 0
		pulsadores |= 0xF1; // Pone a 1 los bits 5,6 y 7 de pulsadores
	}
	pwdw=1; // Flag para que vuelva a power down en el bucle infinito del main
	KBCON &= 0xFE; // reset keyb interrupt flag
	EKBI = 1; // enable keyb interrupt  
} // timers_isr1
/***********************************************************************
DESC:   	determina que tecla se ha pulsado. Modifica el flag
			tecla_valida			
RETURNS: sw_on_off,sw_reference,sw_range,sw_error

************************************************************************/
enum e_tecla detecta_tecla_pulsada(void)
{
	enum e_tecla tecla_detectada;
	switch (pulsadores)
	{
		case 0xFD: // Tecla ON_OFF
			tecla_detectada = sw_on_off;
			tecla_valida=1;			
		break;	   
		case 0xFB: // Tecla REFERENCE
			tecla_detectada = sw_reference;
			tecla_valida=1;			
		break;
		case 0xF7: // Tecla RANGE
			tecla_detectada = sw_range;
			tecla_valida=1;			
		break;
		default: // Cualquier otro valor
		tecla_detectada = sw_error;
		tecla_valida=0;	
		break;
	}
return tecla_detectada;
}
/***********************************************************************
DESC:    configura los relés y los leds de la fuente de corriente
RETURNS: nada
************************************************************************/
void configura_fuente_de_corriente(enum e_tecla tecla_pulsada)
{
	switch (tecla_pulsada)
	{
 	//.......Se ha pulsado la tecla de ON_OFF.................... 
		case sw_on_off:
		// Cambia el estado de la salida de la fuente
			switch(fuente)
			{
				case encendida: // Cambia a apagada
					fuente = apagada;
					RELE_ON_OFF=RELE_OFF;
					LED_ON_OFF=LED_OFF;
				break;
				case apagada: // Cambia a encendida
					fuente = encendida;
					RELE_ON_OFF = RELE_ON;	
					LED_ON_OFF=LED_ON;
				break;
			} // switch(fuente)
		break; // case sw_on_off

	//.......Se ha pulsado la tecla de REFERENCE.................... 
		case sw_reference:
		// Primero apaga la fuente
			RELE_ON_OFF=RELE_OFF;
			LED_ON_OFF=LED_OFF;
			fuente = apagada;
		// Cambia el estado de la referencia
			switch (referencia)
			{
				case interna:
				// Cambia a referencia interna invertida
					LED_REF_EXT 	= LED_OFF;
					LED_REF_INT 	= LED_OFF;
					LED_REF_INT_INV = LED_ON;
					RELE_IN_EX 		= RELE_ON;
					RELE_REF 		= RELE_ON;
					referencia = interna_invertida; // Estado actual de la referencia
				break;
				case interna_invertida:
				// Cambia a referencia externa
					LED_REF_INT_INV = LED_OFF;
					LED_REF_INT 	= LED_OFF;
					LED_REF_EXT 	= LED_ON;
					RELE_REF 		= RELE_OFF;
					RELE_IN_EX 		= RELE_OFF;
					referencia = externa; // Estado actual de la referencia
				break;
				case externa:
				// Cambia a referencia interna
					LED_REF_EXT 	= LED_OFF;
					LED_REF_INT_INV = LED_OFF;
					LED_REF_INT 	= LED_ON;
					RELE_IN_EX 		= RELE_ON;
					RELE_REF 		= RELE_OFF;
					referencia = interna; // Estado actual de la referencia
				break;
			}//switch (referencia)
		break; // case sw_reference
	//........Se ha cambiado es estado de la referencia............. 
	//.......Se ha pulsado la tecla de RANGE........................ 
		case sw_range:
		// Primero apaga la fuente
			RELE_ON_OFF=RELE_OFF;
			LED_ON_OFF=LED_OFF;
			fuente = apagada;
			// Cambia el rango de la fuente
			switch(rango)
			{
				case i10uA:// Cambia a 100uA
					LED_10UA=LED_OFF;
					LED_1MA=LED_OFF;
					LED_10MA=LED_OFF;
					LED_100MA=LED_OFF;
					LED_100UA=LED_ON;
					RELE_NIVEL_2=RELE_OFF;
					RELE_NIVEL_1=RELE_OFF;					
					RELE_NIVEL_3=RELE_ON;
					rango=i100uA;					
				break;// case i10uA
				case i100uA: // Cambia a 1mA
					LED_10UA=LED_OFF;
					LED_100UA=LED_OFF;
					LED_10MA=LED_OFF;
					LED_100MA=LED_OFF;
					LED_1MA=LED_ON;
					RELE_NIVEL_2=RELE_OFF;
					RELE_NIVEL_3=RELE_OFF;					
					RELE_NIVEL_2=RELE_ON;
					rango=i1mA;					
					break;// case i100uA
				case i1mA:// Cambia a 10mA
					LED_10UA=LED_OFF;
					LED_100UA=LED_OFF;
					LED_1MA=LED_OFF;
					LED_100MA=LED_OFF;
					LED_10MA=LED_ON;
					RELE_NIVEL_3=RELE_OFF;
					RELE_NIVEL_2=RELE_ON;					
					RELE_NIVEL_1=RELE_ON;
					rango = i10mA;
				break;// case i1mA
				case i10mA:// Cambia a 100mA
					LED_10UA=LED_OFF;
					LED_100UA=LED_OFF;
					LED_1MA=LED_OFF;
					LED_10MA=LED_OFF;
					LED_100MA=LED_ON;
					RELE_NIVEL_3=RELE_ON;
					RELE_NIVEL_2=RELE_ON;
					RELE_NIVEL_1=RELE_ON;
					rango = i100mA;					
				break;// case i10mA
				case i100mA: // Cambia a 10uA
					LED_100MA=LED_OFF;
					LED_100UA=LED_OFF;
					LED_1MA=LED_OFF;
					LED_10MA=LED_OFF;
					LED_10UA=LED_ON;
					RELE_NIVEL_3=RELE_OFF;
					RELE_NIVEL_2=RELE_OFF;
					RELE_NIVEL_1=RELE_OFF;
					rango = i10uA;
				break;// case i100mA
			}//switch(range)
		break; // case sw_range
	//.......Se ha configurado el RANGE........................ 
		default:
		break;
	}
}
/************************************************************************/