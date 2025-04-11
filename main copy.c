//******************************************************************************
//  MSP430F5529 SSD1306 OLED Display
//
//  Description: This demo connects two MSP430's via the I2C bus. The master
//  transmits to the slave. This is the MASTER CODE. It cntinuously
//  transmits an array of data and demonstrates how to implement an I2C
//  master transmitter sending multiple bytes using the USCI_B0 TX interrupt.
//  ACLK = n/a, MCLK = SMCLK = BRCLK = default DCO = ~1.045MHz
//
//
//                                /|\  /|\
//                MSP430F5529     10k  10k      SSD1306 OLED
//                   master        |    |         Display
//             -----------------   |    |   -----------------
//           -|XIN  P4.1/UCB0SDA|<-|----+->|SDA              |-
//            |                 |  |       |                 |
//           -|XOUT             |  |       |                 |-
//            |     P4.2/UCB0SCL|<-+------>|SCL              |
//            |                 |          |                 |
//
//******************************************************************************

#include <msp430.h>
#include "ssd1306.h"
#include "i2c.h"
#include "clock.h"

extern unsigned char *PTxData;                  // Pointer to TX data, defined in i2c.h
extern unsigned char TXByteCtr;                 // number of bytes to transmit, defined in i2c.h

#define MAX_COUNT 4294967295UL

void setupGPIO();
unsigned char readBCDInput();

int main(void) {
    WDTCTL = WDTPW + WDTHOLD;  // Stop watchdog timer
    clock_init();              // Initialize system clock
    setupGPIO();               // Configure input pins
    i2c_init();                // Initialize I2C for OLED
    ssd1306_init();            // Initialize SSD1306 display
    ssd1306_clearDisplay();    // Clear any previous data

    while (1) {
        unsigned char bcdValue = readBCDInput(); // Read BCD input
        ssd1306_clearDisplay();  // Clear display before updating
        ssd1306_printUI32(0, 2, bcdValue, HCENTERUL_ON); // Display BCD value
        __delay_cycles(100000);  // Small delay to debounce
    }
}

void setupGPIO() {
    P1DIR &= ~(BIT2 | BIT3 | BIT4 | BIT5); // Set P1.2 - P1.5 as input
    P1REN |= (BIT2 | BIT3 | BIT4 | BIT5);  // Enable pull-up/down resistors
    P1OUT |= (BIT2 | BIT3 | BIT4 | BIT5);  // Set pull-up mode
}

unsigned char readBCDInput() {
    unsigned char bcd = (P1IN & (BIT2 | BIT3 | BIT4 | BIT5)) >> 2; // Extract bits
    return bcd;  // Return as decimal
}

//------------------------------------------------------------------------------
// The USCIAB1TX_ISR is structured such that it can be used to transmit any
// number of bytes by pre-loading TXByteCtr with the byte count. Also, TXData
// points to the next byte to transmit.
//------------------------------------------------------------------------------
#if defined(__TI_COMPILER_VERSION__) || defined(__IAR_SYSTEMS_ICC__)
#pragma vector = USCI_B1_VECTOR
__interrupt void USCI_B1_ISR(void)
#elif defined(__GNUC__)
void __attribute__ ((interrupt(USCI_B0_VECTOR))) USCI_B0_ISR (void)
#else
#error Compiler not supported!
#endif
{
  switch(__even_in_range(UCB1IV,12))
  {
  case  0: break;                           // Vector  0: No interrupts
  case  2: break;                           // Vector  2: ALIFG
  case  4: break;                           // Vector  4: NACKIFG
  case  6: break;                           // Vector  6: STTIFG
  case  8: break;                           // Vector  8: STPIFG
  case 10: break;                           // Vector 10: RXIFG
  case 12:                                  // Vector 12: TXIFG  
    if (TXByteCtr)                          // Check TX byte counter
    {
      UCB1TXBUF = *PTxData++;               // Load TX buffer
      TXByteCtr--;                          // Decrement TX byte counter
    }
    else
    {
      UCB1CTL1 |= UCTXSTP;                  // I2C stop condition
      UCB1IFG &= ~UCTXIFG;                  // Clear USCI_B1 TX int flag
      __bic_SR_register_on_exit(LPM0_bits); // Exit LPM0
    }  
  default: break;
  }
}
