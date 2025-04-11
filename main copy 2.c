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
#include <string.h>
#include <stdio.h>

#define MAX_PASSWORD_LENGTH 4

// Default PIN is "0000"
char storedPassword[MAX_PASSWORD_LENGTH + 1] = "0000";
char enteredPassword[MAX_PASSWORD_LENGTH + 1] = {0};
unsigned char index = 0;
// Mode definitions:
// 0 = Unlocked (display current PIN)
// 1 = Set PIN (enter new 4-digit PIN)
// 2 = Locked (waiting for user to start PIN entry)
// 3 = Enter PIN (user types PIN then confirms)
int mode = 0;

// Function prototypes
void setupGPIO(void);
char getKeypadInput(void);
void updateDisplay(char *digits);
void clearDisplay(void);
void displayError(void);

int main(void) {
    WDTCTL = WDTPW + WDTHOLD; // Stop watchdog timer

    setupGPIO();

    // Start in unlocked state: display current PIN (default "0000")
    mode = 0;
    updateDisplay(storedPassword);

    while (1) {
        char key = getKeypadInput();
        if (key) {
            // --- Unlocked State ---
            if (mode == 0) {
                // To set a new PIN, press 'A'
                if (key == 'A') {
                    mode = 1;  // Set PIN mode
                    index = 0;
                    memset(enteredPassword, 0, sizeof(enteredPassword));
                    clearDisplay();
                }
            }
            // --- Set PIN Mode ---
            else if (mode == 1) {
                // Allow only digits to be entered
                if (key >= '0' && key <= '9') {
                    if (index < MAX_PASSWORD_LENGTH) {
                        enteredPassword[index++] = key;
                        enteredPassword[index] = '\0';
                        updateDisplay(enteredPassword);
                    }
                }
                // Press 'B' to confirm new PIN (only when 4 digits entered)
                else if (key == 'B') {
                    if (index == MAX_PASSWORD_LENGTH) {
                        strcpy(storedPassword, enteredPassword);
                        mode = 2;  // Lock the system
                        // In locked mode, we simply show the stored PIN
                        updateDisplay(storedPassword);
                    }
                }
            }
            // --- Locked State ---
            else if (mode == 2) {
                // To attempt unlocking, press 'C'
                if (key == 'C') {
                    mode = 3;  // Enter PIN entry mode
                    index = 0;
                    memset(enteredPassword, 0, sizeof(enteredPassword));
                    clearDisplay();
                }
            }
            // --- Enter PIN Mode ---
            else if (mode == 3) {
                if (key >= '0' && key <= '9') {
                    if (index < MAX_PASSWORD_LENGTH) {
                        enteredPassword[index++] = key;
                        enteredPassword[index] = '\0';
                        updateDisplay(enteredPassword);
                    }
                }
                // Press 'D' to confirm entered PIN
                else if (key == 'D') {
                    if (index == MAX_PASSWORD_LENGTH) {
                        if (strcmp(storedPassword, enteredPassword) == 0) {
                            mode = 0; // Correct PIN: unlocked
                            updateDisplay(storedPassword);
                        } else {
                            // Wrong PIN: show error pattern (here, "8888")
                            displayError();
                            mode = 2;  // Remain locked; show stored PIN again
                            updateDisplay(storedPassword);
                        }
                    }
                }
            }
        }
    }
}

//----------------------------------------------------------
// Setup GPIO for both keypad and 7-segment display
//----------------------------------------------------------
void setupGPIO(void) {
    // -- Keypad Setup (using P2.3 - P2.6) --
    P2SEL &= ~(BIT3 | BIT4 | BIT5 | BIT6);
    P2DIR &= ~(BIT3 | BIT4 | BIT5 | BIT6);
    P2REN |= (BIT3 | BIT4 | BIT5 | BIT6);
    P2OUT |= (BIT3 | BIT4 | BIT5 | BIT6);

    // -- 7-Segment Display Setup --
    // P2.0 is used as the strobe (latch) signal
    P2DIR |= BIT0;
    P2OUT &= ~BIT0;

    // P6.0 - P6.3 carry the BCD digit (for the 7-seg decoder)
    P6DIR |= 0x0F;  // Lower 4 bits
    P6OUT = 0;

    // P4.0 - P4.1 are used as digit select lines (to choose one of 4 digits)
    P4DIR |= 0x03;  // Lower 2 bits
    P4OUT = 0;
}

//----------------------------------------------------------
// updateDisplay: Sends a 4-digit string to the 7-seg display.
// Each digit is sent by selecting the digit (via P4OUT) and then
// outputting the BCD code (via P6OUT). After cycling through all
// digits, the strobe (P2.0) is pulsed to latch the display.
// If the input string is shorter than 4 characters, it is padded.
//----------------------------------------------------------
void updateDisplay(char *digits) {
    char displayBuffer[5] = "0000"; // Default content
    int len = strlen(digits);
    int i, digitVal;

    // Copy entered digits (left aligned) into displayBuffer.
    for (i = 0; i < 4; i++) {
        if (i < len) {
            displayBuffer[i] = digits[i];
        } else {
            // Here we choose to show 0 for “empty” digits.
            displayBuffer[i] = '0';
        }
    }
    displayBuffer[4] = '\0';

    // For each digit position, output its value.
    // The digit select is given by P4OUT:
    //   0 => least significant digit, 1 => next, etc.
    // The BCD value (digit 0-9) is sent on P6OUT.
    for (i = 0; i < 4; i++) {
        // Set digit selector (assuming 0,1,2,3 map to the 4 positions)
        P4OUT = i;
        // Convert ASCII digit to numeric value; if non-digit, default to 0.
        if (displayBuffer[i] >= '0' && displayBuffer[i] <= '9')
            digitVal = displayBuffer[i] - '0';
        else
            digitVal = 0;
        P6OUT = digitVal;
        // A short delay lets the digit be visible.
        __delay_cycles(50000);
    }
    // Latch the data: pulse the strobe (P2.0)
    P2OUT |= BIT0;
    __delay_cycles(50000);
    P2OUT &= ~BIT0;
}

//----------------------------------------------------------
// clearDisplay: Sets all digits to 0 (can be used to “clear” the screen)
//----------------------------------------------------------
void clearDisplay(void) {
    updateDisplay("0000");
}

//----------------------------------------------------------
// displayError: Shows an error pattern (here "8888") for a moment.
//----------------------------------------------------------
void displayError(void) {
    updateDisplay("8888");
    __delay_cycles(200000);
}

//----------------------------------------------------------
// getKeypadInput: Reads 4 input pins on port 2 (P2.2-P2.5) and maps
// the resulting 4-bit value to a keypad character.
// A simple debounce is implemented by ignoring repeated key codes.
//----------------------------------------------------------
char getKeypadInput(void) {
    static char lastKey = 0;
    unsigned char bcd = (P2IN & (BIT3 | BIT4 | BIT5 | BIT6)) >> 2;
    char key = 0;

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

    if (key == lastKey)
        return 0;
    lastKey = key;
    __delay_cycles(200000);
    return key;
}

