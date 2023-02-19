#include <xc.h>
 
#pragma config FOSC=HS,WDTE=OFF,PWRTE=OFF,MCLRE=ON,CPD=OFF
#pragma config IESO=OFF,FCMEN=OFF,WRT=OFF,VCAPEN=OFF,PLLEN=OFF,STVREN=OFF,LVP=OFF
#define _XTAL_FREQ 8000000

#define testBit(number, bit) ((number) & (1<<bit))
/*
 Makro that is being used to calculate the 10 - bit resolution for AD conversion
*/
#define calculate(num1, num2) (unsigned int)(((num1)<<8) | (num2)) 

unsigned char counter = 0, ready = 0;
const unsigned char preload = 8;
unsigned char measure = 0x00, measureVreff = 0x01;
double voltage, vReff = 20.;
const double koe = 0.053805;


/*
 Regulator parameters
 */
const long double a = 0.000102;
double y, y1 = 0, e1 = 0, e;

void initPorts () {
  ANSELD = 0;
  ANSELB = 0;
  TRISD = 0;
  TRISB = 0;
  LATD = 0;
  LATB = 0;
}
/*
 Function that is being used to print three numbers, using 74LS47 to three 7 segment display
*/
void BCD_decoder (double vReff) {

  int number = (int)(vReff * 10);
  unsigned char tens = 0, ones = 0, decimal = 0;

  while (number >= 100) {
    tens++;
    number -= 100;
  }

  while (number >= 10) {
    ones++;
    number -= 10;
  }

  decimal = (unsigned char) number;

  LATB = (tens << 4) | ones;
  LATD = decimal;                        
}


void initAnalog(){
    TRISA = 0xFF;
    ANSELA = 0xFF;
    ADCON1bits.ADCS = 3;
    ADCON1bits.ADNREF = 0;
    ADCON1bits.ADPREF = 0;
    ADCON1bits.ADFM = 1;
    ADCON0bits.CHS = measure;
    ADCON0bits.ADON = 1;
    PIR1bits.ADIF = 0;
    PIE1bits.ADIE = 1;
}

/*
 Max value for d is 408, which is equal to the duty-cycle of 100%
 */
void initDutyCycle(unsigned int d){
    CCP2CONbits.DC2B0 = testBit(d, 0);
    CCP2CONbits.DC2B1 = testBit(d, 1); 
    CCPR2L = (unsigned char)(d>>2);
}

/*
 The PWM freq is set to be 4.9 kHz
 */
void setupPWM(){
    TRISC = 0xFD;
    LATC = 0x00;
    
    CCP2CONbits.CCP2M2 = 1;
    CCP2CONbits.CCP2M3 = 1;
    PR2 = 0x65;
    T2CONbits.T2CKPS0 = 1;
    T2CONbits.T2CKPS1 = 0;
    T2CONbits.TMR2ON = 1;
    initDutyCycle(0.5*408.);
}

/*
 The timer is set to overflow every 0.02 s, which is equal to the sempling time
*/
void initTimer(){
    OPTION_REGbits.T0CS = 0;
    OPTION_REGbits.PSA = 0;
    OPTION_REGbits.PS = 3;
    TMR0 = preload;
    INTCONbits.TMR0IF = 0;
    INTCONbits.TMR0IE = 1;
}    

void __interrupt() function(){
    if(INTCONbits.TMR0IE & INTCONbits.TMR0IF){
        INTCONbits.TMR0IF = 0;
        TMR0 = preload;
        
        if((++counter) > 10){
            counter = 0;
            ADGO = 1;
        }
    }
    /*
     Calculating the output voltage
    */
    if(PIE1bits.ADIE & PIR1bits.ADIF & (ADCON0bits.CHS == measure)){
        PIR1bits.ADIF = 0;
        ADCON0bits.CHS = measureVreff;
        voltage = koe*calculate(ADRESH, ADRESL);
        ADGO = 1;
    }    
    /*
     Calculating the referent voltage
    */
    if(PIE1bits.ADIE & PIR1bits.ADIF & (ADCON0bits.CHS == measureVreff)){
        ADIF = 0;
        ADCON0bits.CHS = measure;
        vReff = 20 + 0.01953 * calculate(ADRESH, ADRESL);
        ready = 1;
    }
}

void main(void) {
    setupPWM();
    initTimer();
    initAnalog();
    initPorts ();
    INTCONbits.PEIE = 1;
    INTCONbits.GIE = 1;    
    while(1){
        /*
         Regulator implementation and printing voltage reff
         */
        if(ready){
            e = vReff - voltage;
            y = y1 + a*e1;
            
            if(y<0){
                y = 0;
            }else if(y>1){
                y = 1;
            }
        
            initDutyCycle((1.-y)*408.);
            y1 = y;
            e1 = e;
            BCD_decoder(vReff);
            ready = 0;
        }
    }


    return;
}
