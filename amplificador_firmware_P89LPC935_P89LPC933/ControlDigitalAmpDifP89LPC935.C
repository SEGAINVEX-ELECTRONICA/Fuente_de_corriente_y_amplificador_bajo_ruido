/* Proyecto OT:2030735
Para un microcontrolador P89LPC935
Implementa un teclado con 3 teclas y leds para indicar el estado del sistema.

Con las teclas se actuan relés y bits que configuran el estado de un amplificador 
de instrumentación.	Configura la ganancia (1,10,100,1000), pulsador "ganancia"
el filtro de salida (sin filtro, 2º orden, 4º orden) pulsador "filtro" y si
la entrada es en AC o DC pulsador "entrada AC".

Tras pulsar una tecla, sale de power down configura el amplificador y entra en 
modo power down otra vez.

Para flasear el micro hay que encender el sistema pulsando la tecla "entrada AC",
El led "entrada AC" parpadea durante 13 segundos. Durante ese tiempo se puede 
enviar un "Break Condition" con Flash Magic para que el micro entre en la
rutina del Bootloader.

Patricio Coronado Universidad Autónoma de Madrid SEGAINVEX-ELECTRONICA
enero de 2015. V1.0
*/
// Para acciónes a realizar en modo depuración. Comentar(evitar que se compile) antes de compilar definitivamente
// #define DEPURACION
//
// Ficheros include
#include <REG935.H>
#include <intrins.h> 	//de la librería de Keil. Para poder usar _nop_()
// Declaración de tipos para definir el estado del amplificador
enum e_tecla {sw_entrada_no,sw_ganancia,sw_filtro,sw_error,power_on}; // Para el estado la tecla pulsada
enum e_ganancia {uno, diez, cien, mil}; // Para el estado de la ganancia
enum e_filtro {sin_filtro, segundo_orden, cuarto_orden}; // Para el estado del filtro
enum e_entrada {entrada_no, entrada_si}; // Para el estado de la entrada
// Prototipos de funciones
void keypad_isr  (  void  );
void timers_isr1  (  void  );
void osc_init  (  void  );
void timers_init  (  void  );
void keypad_init  (  void  );
void uart_init  (  void  );
void uart_transmit  (unsigned char value);
void ports_init  (  void  );
void configura_filtro(enum e_filtro);
void configura_entrada(enum e_entrada);
void configura_ganancia(enum e_ganancia);
void configura_estado(void);
void configura_amplificador(enum e_tecla);
enum e_tecla detecta_tecla_pulsada(void);
// Variables globales
bit tecla_valida = 0;
unsigned char pulsadores;
//unsigned char estado, estado_entrada, estado_filtro,estado_ganancia; // Para escribir en la EEPROM
unsigned int CiclosEspera,Contador;
// Estatus del sistema
enum e_ganancia Ganancia;
enum e_filtro Filtro;
enum e_entrada Entrada;
bit flag=1; // Bit para depurar
bit pwdw=0; // Para que entre en power down de forma condicional
// Constantes
#define RELE_ON 1
#define RELE_OFF 0
#define LED_ON 0
#define LED_OFF 1
#define BIT_OFF 0
#define BIT_ON 1
// Macro y bit de "modo test" para transmitir por la uart cuando está en modo DEPURACION
#define test(byte_a_transmitir) if(modo_test){uart_transmit(byte_a_transmitir);}   
//Bit para comprobar que se está en modo DEPURACION
#ifdef DEPURACION
bit modo_test = 1;
#else bit  = 0;
#endif
// Definición de pines de la aplicación
sbit SW_AC	 		= P0 ^ 1; // PULSADORES
sbit SW_FILTRO	 	= P0 ^ 2;
sbit SW_GANANCIA	= P0 ^ 3;
sbit BIT_A0		 	= P3 ^ 1; // BITS
sbit BIT_A1		 	= P0 ^ 5;
sbit RELE_AC 		= P1 ^ 6; // RELES
sbit RELE_FILTRO 	= P3 ^ 0;
sbit RELE_ORDEN 	= P0 ^ 4;
sbit LED_AC		 	= P2 ^ 0; //LEDS
sbit LED_4POLOS 	= P2 ^ 1;
sbit LED_2POLOS 	= P2 ^ 7;
sbit LED_SIN_FILTRO = P2 ^ 6;
sbit LED_G_1 		= P2 ^ 2;
sbit LED_G_10 		= P1 ^ 2;
sbit LED_G_100		= P1 ^ 3;
sbit LED_G_1000		= P1 ^ 4;
/**********************************************************************************/
void main(void)
{
	osc_init(); // Inicializamos el oscilador interno
	ports_init(); // configura los pines
	uart_init(); // Configura la UART para poder flasear el micro	
	if(!SW_AC)//Si está pulsada la tecla para flasear espera un "Break Condition"
	{	
		// Aviso luminoso de que la UART espera un "Break Condition"
		for(CiclosEspera = 0;CiclosEspera<=200;CiclosEspera++) // Unos 13 segundos
		{			
			LED_AC = LED_ON;
			while (--Contador);
				Contador=15000; // 
			LED_AC = LED_OFF;
		    while (--Contador);
				Contador=15000; // 
		}
		LED_AC= LED_OFF;// Se terminó el tiempo de espera de un "Break Condition" por la UART
	}//del if(!SW_AC) 
	// Inicialización del funcionamiento normal
	configura_estado(); // Pone ganancia, entrada y filtro por defecto
	//EBO=1; // Habilita brownout detect ?? Quitar esto
	timers_init(); // Inicializa el timer 1 para hacer de antirebote de los pulsadores
	keypad_init(); // Inicializa el teclado, mascara e interrupción
  	EA=1; // Habilita interrupciones en bloque
	uart_transmit('a'); // Para evitar un warning en la compilación
	while(1) // bucle infinito
	{
		if(pwdw) // Si está activo el flag para activar el power down lo hace
		{
			pwdw=0;	// Borra el flag que le permite entrar en el if del power down
			// Esto es necesario para que no se aniden interrupciones del teclado
			// y se quede en estado de que no sale del power down
			KBCON &= 0xFE; // reset keyb interrupt flag para evitar que se aniden interrupciones
			EKBI = 1; // enable keyb interrupt  
			#ifndef DEPURACION
			PCON |= 0x03;// Total power down
			#endif
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
  // disable uart interrupt
  // Como no vamos a comunicar, es mejor desabilitar la interrupción
  // si no, con el pin al aire entra en la interrupción y el micro
  // se pierde al no definir una función para servir a la interrupción
  ES = 0; 
  // enable uart transmit interrupt
  // EST = 1;
}
/***********************************************************************
DESC:    Transmits a 8-bit value via the UART in the current mode
         May result in a transmit interrupt if enabled.
RETURNS: Nothing
CAUTION: uart_init must be called first
************************************************************************/
void uart_transmit  (  unsigned char value   )   // data to transmit
{
  SBUF = value;
  {int i=1500;while (--i);} // Retardo para esperar a que transmita
} // uart_transmiT
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
		 pone la sin_filtro en su estado por defecto
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
}
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
} 
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
} 
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
		 Lee el teclado, con la variable "tecla_pulsada" 
		 configura el estado del amplificador.
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
			configura_amplificador(tecla_pulsada);// Actua sobre la fuente
		}
	}
	// Espera a que se suelte la tecla antes de habilitar de nuevo la interrupción del teclado
	while (pulsadores!=0xFF) // Lee el teclado hasta que se suelta la tecla 
	{
		pulsadores = P0; // salva el valor del puerto 0
		pulsadores |= 0xF1; // Pone a 1 los bits 5,6 y 7 del registro de los pulsadores
	}
	pwdw=1; // Flag para que vuelva a power down en el bucle infinito del main
	KBCON &= 0xFE; // reset keyb interrupt flag
	EKBI = 1; // enable keyb interrupt  
} 
/***********************************************************************
DESC:   	determina que tecla se ha pulsado. Modifica el flag
			tecla_valida  y devuelve el valor de la tecla detectada			
RETURNS: sw_entrada_no o sw_ganancia o sw_filtro o sw_error

************************************************************************/
enum e_tecla detecta_tecla_pulsada(void)
{
	enum e_tecla tecla_detectada;
	switch (pulsadores)
	{
		case 0xFD: // Tecla entrada AC/CD
			tecla_detectada = sw_entrada_no;
			tecla_valida=1;			
		break;	   
		case 0xFB: // Tecla del filtro
			tecla_detectada = sw_filtro;
			tecla_valida=1;			
		break;
		case 0xF7: // Tecla de ganancia
			tecla_detectada = sw_ganancia;
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
DESC:   función que modifica el estado del amplificador 
		configura los relés y los leds del amplificador en función de la
		la tecla que recibe de entrada 
RETURNS: nada
************************************************************************/
void configura_amplificador(enum e_tecla tecla_pulsada)
{
	switch (tecla_pulsada)
	{
 	//.......Se ha pulsado la tecla de filtro ............... 
		case sw_filtro:
		// Cambia el estado del Filtro
			switch(Filtro)
			{
				case sin_filtro: // Cambia a segundo_orden
					configura_filtro(segundo_orden);
				break;
				case segundo_orden: // Cambia a cuarto orden
					configura_filtro(cuarto_orden);
				break;
				case cuarto_orden: // Cambia a sin_filtro
					configura_filtro(sin_filtro);
				break;
			} // switch(Filtro)
		break; // case sw_filtro
	//.......Si se ha pulsado la tecla de entrada AC.................... 
		case sw_entrada_no:
		// Cambia el estado de la Entrada
			switch (Entrada)
			{
				case entrada_no:
				// Cambia a  entrada_si
					configura_entrada(entrada_si);
				break;
				case entrada_si:
				// Cambia a entrada ac
					configura_entrada(entrada_no);
				break;
			}//switch (Entrada)
		break; // case sw_AC
	//.......Si se ha pulsado la tecla de ganancia.................... 
		case sw_ganancia:
			// Cambia el ganancia del amplificador
			switch(Ganancia)
			{
				case uno:// Cambia a ganancia 10
					configura_ganancia(diez);
				break;// case uno
				case diez: // Cambia a gananacia 100
					configura_ganancia(cien);
					break;// case diez
				case cien:// Cambia a ganancia 1000
					configura_ganancia(mil);
				break;// case cien
				case mil:// Cambia a ganancia 1
						configura_ganancia(uno);
				break;// case mil
			}//switch(Ganancia)
		break; // case sw_ganancia
		//.... Si viene de reset o encendido configura con la palabra de estado...
//		case power_on: 
//		//lee_estado_y_configura();
//		break;
		default:
		break;
	}
}
/***********************************************************************
DESC: Pone el filtro en el estado que se le pida con "estado_deseado"
RETURNS: Nothing
CAUTION: 
************************************************************************/
void configura_filtro(enum e_filtro filtro_deseado)
{
		switch(filtro_deseado)
		{
		case segundo_orden: // Cambia a segundo_orden
			Filtro = segundo_orden;
			LED_4POLOS 	= LED_OFF;
			LED_2POLOS 	= LED_ON;
			LED_SIN_FILTRO = LED_OFF;
			RELE_FILTRO = RELE_OFF;
			RELE_ORDEN = RELE_ON;
		break;
		case cuarto_orden: // Cambia a cuarto orden
			Filtro = cuarto_orden;
			LED_4POLOS 	= LED_ON;
			LED_2POLOS 	= LED_OFF;
			LED_SIN_FILTRO = LED_OFF;
			RELE_FILTRO = RELE_OFF;
			RELE_ORDEN = RELE_OFF;
		break;
			//Actualiza la palabra de estado del filtro
		case sin_filtro: // Cambia a sin_filtro
			Filtro = sin_filtro;
			LED_4POLOS 	= LED_OFF;
			LED_2POLOS 	= LED_OFF;
			LED_SIN_FILTRO = LED_ON;
			RELE_FILTRO = RELE_ON;
			RELE_ORDEN = RELE_OFF;
		break;
		default:  // Configuración por defecto (por si no lee bien la EEPROM)
			Filtro = cuarto_orden;
			LED_4POLOS 	= LED_ON;
			LED_2POLOS 	= LED_OFF;
			LED_SIN_FILTRO = LED_OFF;
			RELE_FILTRO = RELE_OFF;
			RELE_ORDEN = RELE_OFF;
		break;
	} // switch(estado_deseado)
}
/***********************************************************************
DESC: Pone la entrada en el estado que se le pida con "entrada_deseada"
RETURNS: Nothing
CAUTION: 
************************************************************************/
void configura_entrada(enum e_entrada entrada_deseada)
{
	switch (entrada_deseada)
	{
		case entrada_no:
			// Cambia a entrada ac
			Entrada = entrada_no; // Estado actual de la Entrada
			LED_AC =  LED_OFF;
			RELE_AC = RELE_OFF;
		break;
		case entrada_si:
			// Cambia a  entrada_si
			Entrada = entrada_si; // Estado actual de la Entrada
			LED_AC =  LED_ON;
			RELE_AC = RELE_ON;
		break;
		default:  // Configuración por defecto 
			Entrada = entrada_no; // Estado actual de la Entrada
			LED_AC =  LED_OFF;
			RELE_AC = RELE_OFF;
		break;
	}//switch (entrada_deseada)
}
/***********************************************************************
DESC: Pone la ganancia en el estado que se le pida con "ganancia_deseada"
ENTRADA: Entra con una variable de estado de la gananacia. Si no es 
			correcta configura por defecto en default
RETURNS: Nothing
CAUTION: 
************************************************************************/
void configura_ganancia(enum e_ganancia ganancia_deseada)
{
	switch(ganancia_deseada)
	{
		case uno:// Cambia a ganancia 1
			Ganancia = uno;					
			LED_G_1 		= LED_ON;
			LED_G_10 		= LED_OFF;
			LED_G_100		= LED_OFF;
			LED_G_1000		= LED_OFF;					
			BIT_A0		 	= BIT_OFF;
			BIT_A1		 	= BIT_OFF;
		break;// case uno
		case diez:// Cambia a ganancia 10
			Ganancia=diez;
			LED_G_1 		= LED_OFF;
			LED_G_10 		= LED_ON;
			LED_G_100		= LED_OFF;
			LED_G_1000		= LED_OFF;					
			BIT_A0		 	= BIT_ON;
			BIT_A1		 	= BIT_OFF;
		break;// case diez
		case cien: // Cambia a gananacia 100
			Ganancia=cien;					
			LED_G_1 		= LED_OFF;
			LED_G_10 		= LED_OFF;
			LED_G_100		= LED_ON;
			LED_G_1000		= LED_OFF;					
			BIT_A0		 	= BIT_OFF;
			BIT_A1		 	= BIT_ON;
		break;// case cien
		case mil:// Cambia a ganancia 1000
			Ganancia = mil;
			LED_G_1 		= LED_OFF;
			LED_G_10 		= LED_OFF;
			LED_G_100		= LED_OFF;
			LED_G_1000		= LED_ON;					
			BIT_A0		 	= BIT_ON;
			BIT_A1		 	= BIT_ON;
		break;// case mil
		default:  // Configuración por defecto (por si no lee bien la EEPROM)
			Ganancia=diez;
			LED_G_1 		= LED_OFF;
			LED_G_10 		= LED_ON;
			LED_G_100		= LED_OFF;
			LED_G_1000		= LED_OFF;					
			BIT_A0		 	= BIT_ON;
			BIT_A1		 	= BIT_OFF;
		break;
	}//switch(ganancia_deseada)
 	// Guarda el estdo de la ganancia en la EEPROM
 }
/***********************************************************************
DESC:    Lee el estado guardado en la EEPROM y configura
		el amplificador.
		Utiliza las funciones de eeprom.c
RETURNS: 
CAUTION: 
************************************************************************/
void configura_estado(void)
{
	configura_entrada(entrada_no);//Configura el estado de la entrad del amplificador
	configura_filtro(segundo_orden);//Configura el estado del filtro del amplificador
	configura_ganancia(diez);//Configura el estado de la ganancia del amplificador
}

/*************************** FIN ****************************************/

