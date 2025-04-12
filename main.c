//******************************************************************************
//
//  Commands in the MSP430F5529 SSD1306 OLED Display Library:
//  
//  i2c_init(void)
//      Initialize I2C on P4.1 and P4.2
//  
//  ssd1306_init(void)
//      Initialize SSD1306 display, this sends all the setup commands to configure the display.
//  
//  ssd1306_clearDisplay(void)
//      Clear Display
//  
//  ssd1306_printText(uint8_t x, uint8_t y, char *ptString)
//      Print single line of text on row y starting at horizontal pixel x. 
//      There are a total of 7 rows starting at 1. 
//      The horizontal starting position can be from 0 to 127.
//  
//  ssd1306_printTextBlock(uint8_t x, uint8_t y, char *ptString)
//      Print a block of text that can span multiple lines, 
//      the code will automagically split up the text on multiple lines. 
//      It will print the text block starting on row y at horizontal pixel x. 
//      There are a total of 7 rows starting at 1. 
//      The horizontal starting position can be from 0 to 127. 
//      Store the text block as a char array.
//  
//  void ssd1306_printUI32( uint8_t x, uint8_t y, uint32_t val, uint8_t Hcenter)
//      Print the 32bit unsigned integer val on row y at horizontal pixel x.
//      The code automagically adds thousands comma spacing to enable easy reading of large numbers. 
//      Use Hcenter to horizontally center the number at row y regardless of the value of x. 
//      Hcenter accepts HCENTERUL_ON and HCENTERUL_OFF.
//
//******************************************************************************

#include <msp430.h>
#include <stdio.h> 

// Functions and definitions from the OLED display library
#include "ssd1306.h"
#include "i2c.h"
#include "clock.h"

#define MAX_PASSWORD_LENGTH 4

char storedPassword[MAX_PASSWORD_LENGTH + 1] = "0000"; // Default password, stores setted PIN
char enteredPassword[MAX_PASSWORD_LENGTH + 1] = {0}; // Stores PIN entries
unsigned char index = 0; // Tracks position in PIN arrays
int mode = 0; // 0 = Door Open, 1 = Set Password, 2 = Locked, 3 = Enter Password

void setupGPIO();
char getKeypadInput();
void displayMessage(const char* msg);

void setLockedLEDOn(void);
void setLockedLEDOff(void);
void flashLockedLED(void);
void setUnlockedLEDOn(void);
void setUnlockedLEDOff(void);

int main(void) {
    WDTCTL = WDTPW + WDTHOLD; // Stop watchdog timer

    // initialization functions from display library
    clock_init(); 
    i2c_init();
    ssd1306_init();

    setupGPIO(); // initialization of indicator LED and keypad pins

    // Start in unlocked state (mode 0)
    mode = 0;
    displayMessage("Unlocked. Press A to set PIN");
    setLockedLEDOff();   // Locked LED off
    setUnlockedLEDOn();  // Unlocked LED on

    while (1) {
        char key = getKeypadInput(); // checks if a key has been pressed
        if (key) { // proceeds only if valid keypress is received

            // Unlocked state: allow setting a new PIN
            if (mode == 0) {
                if (key == 'A') {
                    mode = 1; // Enter Set PIN mode
                    index = 0; // reset index
                    memset(enteredPassword, 0, sizeof(enteredPassword)); // reset enteredPassword
                    displayMessage("Enter New PIN:");
                    setLockedLEDOff(); // Locked LED off
                    setUnlockedLEDOff(); // Unlocked LED off
                }
            }
            // Set PIN mode: enter 4-digit new PIN
            else if (mode == 1) {
                if (key >= '0' && key <= '9') {
                    if (index < MAX_PASSWORD_LENGTH) {
                        // When a number key is pressed, append it to enteredPassword
                        enteredPassword[index++] = key;
                        enteredPassword[index] = '\0';
                        displayMessage(enteredPassword); // Update the display to show the current entered PIN
                    }
                }
                else if (key == 'B') {
                    // Save PIN only if 4 digits have been entered
                    if (index == MAX_PASSWORD_LENGTH) {
                        strcpy(storedPassword, enteredPassword); // copy new PIN to storedPassword
                        mode = 2;  // Move to locked state
                        displayMessage("Locked. Press C to enter PIN");
                        setLockedLEDOn();   // In locked state, turn locked LED on
                        setUnlockedLEDOff(); // Unlocked LED off
                    }
                }
            }
            // Locked state: waiting for user to try unlocking
            else if (mode == 2) {
                if (key == 'C') {
                    mode = 3; // Enter PIN entry mode
                    index = 0; // reset index
                    memset(enteredPassword, 0, sizeof(enteredPassword)); // reset enteredPassword
                    displayMessage("Enter PIN, then press D");
                    setLockedLEDOn();   // locked LED on
                    setUnlockedLEDOff(); // unlocked LED off
                }
            }
            // Enter PIN mode: user types PIN then presses D to check
            else if (mode == 3) {
                if (key >= '0' && key <= '9') {
                    if (index < MAX_PASSWORD_LENGTH) { 
                        // When a number key is pressed, append it to enteredPassword
                        enteredPassword[index++] = key;
                        enteredPassword[index] = '\0';
                        displayMessage(enteredPassword); // Update the display to show the current entered PIN
                    }
                }
                else if (key == 'D') {
                    if (index == MAX_PASSWORD_LENGTH) { 
                        // Allow PIN verification only if 4 digits have been entered
                        if (strcmp(storedPassword, enteredPassword) == 0) {
                            // if entered PIN matches the stored PIN, system is unlocked
                            mode = 0; // Unlocked
                            displayMessage("Unlocked. Press A to set PIN");
                            setLockedLEDOff();
                            setUnlockedLEDOn();
                            
                        } else {
                            // if entered PIN doesn't match the stored PIN, system remains locked
                            displayMessage("Wrong PIN! Press C to try again");
                            flashLockedLED();   // Flash locked LED 
                            mode = 2;           // Remain locked
                            setLockedLEDOn();
                            setUnlockedLEDOff();
                        }
                    }
                }
            }
        }
    }
}

void setupGPIO() {
    // Configure LED on P1.4 (locked LED) and P1.5 (unlocked LED) as outputs.
    P1DIR |= (BIT4 | BIT5);
    P1OUT &= ~(BIT4 | BIT5);  // Turn both off initially

    // Configure keypad inputs from encoder on P2.3-P2.6 with pull-up resistors.
    P2SEL &= ~(BIT3 | BIT4 | BIT5 | BIT6);
    P2DIR &= ~(BIT3 | BIT4 | BIT5 | BIT6);
    P2REN |= (BIT3 | BIT4 | BIT5 | BIT6);
    P2OUT |= (BIT3 | BIT4 | BIT5 | BIT6);
}

char getKeypadInput() {
    static char lastKey = 0;
    // Read from P2.3-P2.6; shift by 3 so BIT3 becomes BIT0.
    unsigned char bcd = (P2IN & (BIT3 | BIT4 | BIT5 | BIT6)) >> 3;
    char key = 0;
    
    // Map BCD value to the corresponding character.
    switch (bcd) {
        case 0b0000: key = '1'; break;
        case 0b0001: key = '2'; break;
        case 0b0010: key = '3'; break;
        case 0b0011: key = 'A'; break;
        case 0b0100: key = '4'; break;
        case 0b0101: key = '5'; break;
        case 0b0110: key = '6'; break;
        case 0b0111: key = 'B'; break;
        case 0b1000: key = '7'; break;
        case 0b1001: key = '8'; break;
        case 0b1010: key = '9'; break;
        case 0b1011: key = 'C'; break;
        case 0b1100: key = '*'; break;
        case 0b1101: key = '0'; break;
        case 0b1110: key = '#'; break;
        case 0b1111: key = 'D'; break;
        default: key = 0;
    }

    // Avoid processing the same key multiple times.
    if (key == lastKey) {
        return 0;
    }
    lastKey = key;
    __delay_cycles(200000);  // debounce delay
    return key;
}

void displayMessage(const char* msg) {
    char buffer[100];  // Adjust buffer size as needed.
    // Workaround for the ssd1306_printTextBlock bug: append an extra space.
    snprintf(buffer, sizeof(buffer), "%s ", msg);

    ssd1306_clearDisplay();
    ssd1306_printTextBlock(0, 2, buffer);
    __delay_cycles(100000);
}

// Functions for locked LED (P1.4)
void setLockedLEDOn(void) {
    P1OUT |= BIT4;
}
void setLockedLEDOff(void) {
    P1OUT &= ~BIT4;
}
void flashLockedLED(void) {
    int i;
    for (i = 0; i < 10; i++) {
        P1OUT ^= BIT4;       // Toggle locked LED on P1.4
        __delay_cycles(3000000);
        P1OUT ^= BIT4;
        __delay_cycles(3000000);
    }
}

// Functions for unlocked LED (P1.5)
void setUnlockedLEDOn(void) {
    P1OUT |= BIT5;
}
void setUnlockedLEDOff(void) {
    P1OUT &= ~BIT5;
}
// Below is code from display library for interrupt
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
