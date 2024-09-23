// Lab 9 - Final Project
// Xavier A. Portillo-Catalan
// 1001779115

//-----------------------------------------------------------------------------
// Hardware Target
//-----------------------------------------------------------------------------

// Target Platform: EK-TM4C123GXL Evaluation Board
// Target uC:       TM4C123GH6PM
// System Clock:    40 MHz

//-----------------------------------------------------------------------------
// Device includes, defines, and assembler directives
//-----------------------------------------------------------------------------

#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <inttypes.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include "clock.h"
#include "wait.h"
#include "uart0.h"
#include "adc0.h"
#include "tm4c123gh6pm.h"

// Bitband aliases
#define RED_LED      (*((volatile uint32_t *)(0x42000000 + (0x400253FC-0x40000000)*32 + 1*4))) //PF1
#define BLUE_LED     (*((volatile uint32_t *)(0x42000000 + (0x400253FC-0x40000000)*32 + 2*4))) //PF2
#define GREEN_LED    (*((volatile uint32_t *)(0x42000000 + (0x400253FC-0x40000000)*32 + 3*4))) //PF3
#define PUSH_BUTTON  (*((volatile uint32_t *)(0x42000000 + (0x400253FC-0x40000000)*32 + 4*4))) //PF4

#define BRIGHT_RED   (*((volatile uint32_t *)(0x42000000 + (0x400243FC-0x40000000)*32 + 4*4))) //PE4

#define DT           (*((volatile uint32_t *)(0x42000000 + (0x400243FC-0x40000000)*32 + 2*4))) //PE2 DT
#define SCK          (*((volatile uint32_t *)(0x42000000 + (0x400243FC-0x40000000)*32 + 3*4))) //PE3 SCK

#define SPEAKER      (*((volatile uint32_t *)(0x42000000 + (0x400073FC-0x40000000)*32 + 1*4))) //PD1

// Masks
// PortF Masks
#define RED_LED_MASK 2      //PF1
#define BLUE_LED_MASK 4     //PF2
#define GREEN_LED_MASK 8    //PF3
#define PUSH_BUTTON_MASK 16 //PF4

// PortE Masks
#define BRIGHT_RED_MASK 16  //PE4
#define DT_MASK 4           //PE2
#define SCK_MASK 8          //PE3
#define AIN8_MASK 32        //PE5

// PortD Masks
#define SPEAKER_MASK 2      //PD1
#define FREQ_IN_MASK 4      //PD2

//-----------------------------------------------------------------------------
// Global variables
//-----------------------------------------------------------------------------

bool timeMode = false;
bool pulse_active = false;
bool loopCheck = false;
bool exhale = false;

uint32_t frequency = 0;
uint32_t pulseTime = 0;
uint32_t breathTime = 0;
//uint32_t time = 0;
uint32_t pulse_time = 0;
int32_t valueDT = 0;
int32_t prevDT = 0;
int32_t difference = 0;
int32_t prevDifference = 0;
int32_t breath_time = 1;
int32_t breathCounter = 0;


uint32_t sum = 0;

uint32_t x[3];
uint32_t index = 0;
uint32_t indexDT = 0;

uint32_t breathingBuffer[9];

uint32_t alarmPulseMin = 40;
uint32_t alarmPulseMax = 250;

uint32_t alarmRespirationMin = 5;
uint32_t alarmRespirationMax = 50;

bool speakerStatus = false;


//-----------------------------------------------------------------------------
// Subroutines
//-----------------------------------------------------------------------------

//------------------------------------------------------------------------------------------------------------------------------------

void enableCounterMode() //Frequency Enable // Not used
{
    // Configure Timer 1 as the time base
    TIMER3_CTL_R &= ~TIMER_CTL_TAEN;                 // turn-off timer before reconfiguring
    TIMER3_CFG_R = TIMER_CFG_32_BIT_TIMER;           // configure as 32-bit timer (A+B)
    TIMER3_TAMR_R = TIMER_TAMR_TAMR_PERIOD;          // configure for periodic mode (count down)
    TIMER3_TAILR_R = 40000000;                       // set load value to 40e6 for 1 Hz interrupt rate
    TIMER3_IMR_R = TIMER_IMR_TATOIM;                 // turn-on interrupts
    TIMER3_CTL_R |= TIMER_CTL_TAEN;                  // turn-on timer
    NVIC_EN1_R |= 1 << (INT_TIMER3A-16);             // turn-on interrupt 51 (TIMER3A)

    // Configure Wide Timer 1 as counter of external events on CCP0 pin
    WTIMER3_CTL_R &= ~TIMER_CTL_TAEN;                // turn-off counter before reconfiguring
    WTIMER3_CFG_R = 4;                               // configure as 32-bit counter (A only)
    WTIMER3_TAMR_R = TIMER_TAMR_TAMR_CAP | TIMER_TAMR_TACDIR; // configure for edge count mode, count up
    WTIMER3_CTL_R = TIMER_CTL_TAEVENT_POS;           // count positive edges
    WTIMER3_IMR_R = 0;                               // turn-off interrupts
    WTIMER3_TAV_R = 0;                               // zero counter for first period
    WTIMER3_CTL_R |= TIMER_CTL_TAEN;                 // turn-on counter
}

void disableCounterMode() // Frequency Disable
{
    TIMER3_CTL_R &= ~TIMER_CTL_TAEN;                 // turn-off time base timer
    WTIMER3_CTL_R &= ~TIMER_CTL_TAEN;                // turn-off event counter
    NVIC_DIS1_R |= 1 << (INT_TIMER3A-16);            // turn-off interrupt 51 (TIMER3A)
}

void enableTimerMode() // Time Enable
{
    //Lab 6 pulse timer
    WTIMER3_CTL_R &= ~TIMER_CTL_TAEN;                // turn-off counter before reconfiguring
    WTIMER3_CFG_R = 4;                               // configure as 32-bit counter (A only)
    WTIMER3_TAMR_R = TIMER_TAMR_TACMR | TIMER_TAMR_TAMR_CAP | TIMER_TAMR_TACDIR;
                                                     // configure for edge time mode, count up
    WTIMER3_CTL_R = TIMER_CTL_TAEVENT_POS;           // measure time from positive edge to positive edge
    WTIMER3_IMR_R = TIMER_IMR_CAEIM;                 // turn-on interrupts
    WTIMER3_TAV_R = 0;                               // zero counter for first period
    WTIMER3_CTL_R |= TIMER_CTL_TAEN;                 // turn-on counter
    NVIC_EN3_R |= 1 << (INT_WTIMER3A-16-96);         // turn-on interrupt 116 (WTIMER3A)

    //Lab 7 finger detection timer
    TIMER1_CTL_R &= ~TIMER_CTL_TAEN;                 // turn-off timer before reconfiguring
    TIMER1_CFG_R = TIMER_CFG_32_BIT_TIMER;           // configure as 32-bit timer (A+B)
    TIMER1_TAMR_R = TIMER_TAMR_TAMR_PERIOD;          // configure for periodic mode (count down)
    TIMER1_TAILR_R = 40000000;                       // set load value to 40e6 for 1 Hz interrupt rate
    TIMER1_IMR_R = TIMER_IMR_TATOIM;                 // turn-on interrupts
    TIMER1_CTL_R |= TIMER_CTL_TAEN;                  // turn-on timer
    NVIC_EN0_R |= 1 << (INT_TIMER1A-16);             // turn-on interrupt 37 (TIMER1A)

    //clear breath_time every 5 seconds timer
    TIMER2_CTL_R &= ~TIMER_CTL_TAEN;                 // turn-off timer before reconfiguring
    TIMER2_CFG_R = TIMER_CFG_32_BIT_TIMER;           // configure as 32-bit timer (A+B)
    TIMER2_TAMR_R = TIMER_TAMR_TAMR_PERIOD;          // configure for periodic mode (count down)
    TIMER2_TAILR_R = 200000000;                       // set load value to 40e6 for 1 Hz interrupt rate
    TIMER2_IMR_R = TIMER_IMR_TATOIM;                 // turn-on interrupts

    NVIC_EN0_R |= 1 << (INT_TIMER2A-16);             // turn-on interrupt 39 (TIMER2A)
    TIMER2_CTL_R |= TIMER_CTL_TAEN;                  // turn-on timer
}

void disableTimerMode() // Time Disable
{
    WTIMER3_CTL_R &= ~TIMER_CTL_TAEN;                // turn-off counter
    NVIC_DIS3_R |= 1 << (INT_WTIMER3A-16-96);        // turn-off interrupt  116 (WTIMER3A)
}

//------------------------------------------------------------------------------------------------------------------------------------

void enableDataLine()
{
    //Pg 664
    GPIO_PORTE_IM_R &= ~0xFF;
    GPIO_PORTE_IS_R &= ~DT_MASK;
    GPIO_PORTE_IBE_R &= ~DT_MASK;
    GPIO_PORTE_RIS_R &= ~DT_MASK;
    GPIO_PORTE_IEV_R |= DT_MASK;
    GPIO_PORTE_IM_R |= DT_MASK;

    NVIC_EN0_R |= 1 << (INT_GPIOE-16); // turn-on interrupt 20 (WTIMER3A)
}

void disableDataLine()//664
{
    NVIC_DIS0_R |= 1 << (INT_GPIOE-16);        // turn-off interrupt  20 (WTIMER3A)
}

//------------------------------------------------------------------------------------------------------------------------------------
// Pulse timer/frequency ISR's
//------------------------------------------------------------------------------------------------------------------------------------

// Frequency counter service publishing latest frequency measurements every second // Never used
void timer1Isr()
{
    frequency = WTIMER3_TAV_R;                   // read counter input
    WTIMER3_TAV_R = 0;                           // reset counter for next period
    GREEN_LED ^= 1;                              // status
    TIMER3_ICR_R = TIMER_ICR_TATOCINT;           // clear interrupt flag
}

// Period timer service publishing latest time measurements every positive edge
void wideTimer1Isr()
{
    pulse_time = WTIMER3_TAV_R;                        // read counter input
    WTIMER3_TAV_R = 0;                           // zero counter for next edge
    WTIMER3_ICR_R = TIMER_ICR_CAECINT;           // clear interrupt flag
}

void fingerDetectIsr()
{
    uint32_t microSec;
    uint32_t pulse;
    uint32_t raw;

    uint32_t i = 0;
    float firV;

    if(pulse_active == false)
    {
        // Clear filter taps
        for (i = 0; i < 4; i++)
        {
            x[i] = 0;
        }

        sum = 0;
        index = 0;
        pulseTime = 0;

        raw = readAdc0Ss3();
        firV = ((raw+0.5) / 4096.0 * 3.3);

        if ((firV >= 3.02))
        {
            BRIGHT_RED = 1;
            pulse_active = true;
        }
    }

    if(pulse_active == true)
    {
        raw = readAdc0Ss3();
        firV = ((raw+0.5) / 4096.0 * 3.3);

        if((firV < 2.7))
        {
            BRIGHT_RED = 0;
            pulse_active = false;
        }

        if(pulse_active == true)
        {
            microSec = pulse_time / 40;
            pulse = (60000000/microSec);

            if (pulse >= 40 && pulse <=250 )
            {
                // FIR sliding average filter with circular addressing
                sum -= x[index];
                sum += pulse;
                x[index] = pulse;
                index = (index + 1) & 3;

                if((sum/4) >= 40 && (sum/4) <= 200)
                {
                    pulseTime = (sum/4);
                }

                //Speaker
                if(pulseTime > alarmPulseMin && pulseTime < alarmPulseMax)
                {
                    if(speakerStatus == true)
                    {
                        PWM1_0_CMPB_R = 0;
                    }
                    RED_LED = 0;
                }

                if(pulseTime < alarmPulseMin || pulseTime > alarmPulseMax)
                {
                    if(speakerStatus == true)
                    {
                        PWM1_0_CMPB_R = 1023;
                        waitMicrosecond(500000);
                        PWM1_0_CMPB_R = 0;
                    }
                    RED_LED = 1;
                    waitMicrosecond(500000);
                    RED_LED = 0;
                }
            }
        }
    }
    TIMER1_ICR_R = TIMER_ICR_TATOCINT;
}

//------------------------------------------------------------------------------------------------------------------------------------
// Initialize Hardware
//------------------------------------------------------------------------------------------------------------------------------------

void initHw()
{
    // Initialize system clock to 40 MHz
    initSystemClockTo40Mhz();

    // Enable clocks
    SYSCTL_RCGCPWM_R |= SYSCTL_RCGCPWM_R1;       // PWM1
    SYSCTL_RCGCTIMER_R |= SYSCTL_RCGCTIMER_R1 | SYSCTL_RCGCTIMER_R2 | SYSCTL_RCGCTIMER_R3;   // FrequencyR3 (not needed) // other timers
    SYSCTL_RCGCWTIMER_R |= SYSCTL_RCGCWTIMER_R3; // page 1351 PD2 (WT3CCP0)
    SYSCTL_RCGCGPIO_R = SYSCTL_RCGCGPIO_R1 | SYSCTL_RCGCGPIO_R2 | SYSCTL_RCGCGPIO_R3 | SYSCTL_RCGCGPIO_R4 | SYSCTL_RCGCGPIO_R5;
    _delay_cycles(3);

    // Configure LED pins
    GPIO_PORTF_DIR_R  |= RED_LED_MASK | GREEN_LED_MASK | BLUE_LED_MASK; // bits 1, 2, and 3 are outputs
    GPIO_PORTF_DR2R_R |= RED_LED_MASK | GREEN_LED_MASK | BLUE_LED_MASK; // set drive strength to 2mA (not needed since default configuration -- for clarity)
    GPIO_PORTF_DEN_R  |= RED_LED_MASK | GREEN_LED_MASK | BLUE_LED_MASK; // enable LEDs

    ///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

    //RED_LED_BRIGHT
    GPIO_PORTE_DIR_R  |= BRIGHT_RED_MASK; // Enable output
    GPIO_PORTE_DR2R_R |= BRIGHT_RED_MASK; // set drive strength to 2mA (not needed since default configuration -- for clarity)
    GPIO_PORTE_DEN_R  |= BRIGHT_RED_MASK; // enable LED

    ///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

    // Configure SIGNAL_IN for frequency and time measurements
    GPIO_PORTD_AFSEL_R |= FREQ_IN_MASK;              // select alternative functions for SIGNAL_IN pin
    GPIO_PORTD_PCTL_R &= ~GPIO_PCTL_PD2_M;           // map alt fns to SIGNAL_IN
    GPIO_PORTD_PCTL_R |= GPIO_PCTL_PD2_WT3CCP0;
    GPIO_PORTD_DEN_R |= FREQ_IN_MASK;                // enable bit 2 for digital input

    ///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

    // Configure AIN8 as an analog input
    GPIO_PORTE_AFSEL_R |= AIN8_MASK;                 // select alternative functions for AN8 (PE5)
    GPIO_PORTE_DEN_R &= ~AIN8_MASK;                  // turn off digital operation on pin PE5
    GPIO_PORTE_AMSEL_R |= AIN8_MASK;                 // turn on analog operation on pin PE5

    ///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

    //SCK
    GPIO_PORTE_DIR_R  |= SCK_MASK; //output
    GPIO_PORTE_DR2R_R |= SCK_MASK; // set drive strength to 2mA (not needed since default configuration -- for clarity)
    GPIO_PORTE_DEN_R  |= SCK_MASK; // enable

    //DT
    GPIO_PORTE_DIR_R &= ~DT_MASK; //input
    //GPIO_PORTE_DR2R_R |= DT_MASK; // set drive strength to 2mA (not needed since default configuration -- for clarity)
    GPIO_PORTE_DEN_R  |= DT_MASK; // enable

    ///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

    //SPEAKER
    GPIO_PORTD_DEN_R |= SPEAKER_MASK;
    GPIO_PORTD_AFSEL_R |= SPEAKER_MASK;
    GPIO_PORTD_PCTL_R &= ~(GPIO_PCTL_PD1_M);
    GPIO_PORTD_PCTL_R |= GPIO_PCTL_PD1_M1PWM1;

    // SPEAKER on M1PWM1 (PD1), M1PWM1b
    SYSCTL_SRPWM_R = SYSCTL_SRPWM_R1;                // reset PWM1 module
    SYSCTL_SRPWM_R = 0;                              // leave reset state
    PWM1_0_CTL_R = 0;                                // turn-off PWM1 generator 0 (drives outs 0 and 1)

    PWM1_0_GENB_R = PWM_1_GENB_ACTCMPBD_ONE | PWM_1_GENB_ACTLOAD_ZERO;  // output 1 on PWM1, gen 0b, cmpb

    PWM1_0_LOAD_R = 50000;              // set frequency to 40 MHz sys clock / 2 / 1024 = 19.53125 kHz
                                        // (internal counter counts down from load value to zero)

    PWM1_0_CMPB_R = 25000;              // speaker off (0=always low, 1023=always high)

    PWM1_0_CTL_R = PWM_1_CTL_ENABLE;    // turn-on PWM1 generator 0

    PWM1_ENABLE_R = PWM_ENABLE_PWM1EN;  // enable outputs

    ///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

}

void portEIsr(void)
{
    uint32_t i = 0;
    int32_t temp = 0;

    //char str[80];

    for (i = 0; i < 24; i++)
    {
        SCK = 1;
        _delay_cycles(1600); //1,600 (1 clock = 25 ns)
        temp <<= 1;
        temp |= DT;
        SCK = 0;
        _delay_cycles(9);
    }
    SCK = 1;
    _delay_cycles(4);
    SCK = 0;

    valueDT = temp;

    difference = valueDT - prevDT;

    if(prevDT > valueDT)
    {
        GREEN_LED = 1;
        if(exhale == true)
        {
            //Print here for testing since my string gauge is broken and borrowed one is too small.
            //snprintf(str, sizeof(str), "\nBreath per minute:    %d\n", (600/breathCounter));
            //putsUart0(str);
            breath_time = (600/breathCounter);
            breathCounter = 0;
            exhale = false;
            GREEN_LED = 0;
        }
        breathCounter++;
    }

    else if(difference > 15000)
    {
        exhale = true;
    }

    //Speaker
    if(breath_time > alarmRespirationMin && breath_time < alarmRespirationMax)
    {
        if(speakerStatus == true)
        {
            PWM1_0_CMPB_R = 0;
        }
        BLUE_LED = 0;
    }

    else if(breath_time < alarmRespirationMin || breath_time > alarmRespirationMax)
    {
        if(speakerStatus == true)
        {
            PWM1_0_CMPB_R = 10120;
            waitMicrosecond(500000);
            PWM1_0_CMPB_R = 0;
        }
        BLUE_LED = 1;
        waitMicrosecond(500000);
        BLUE_LED = 0;
    }

    prevDT = valueDT;

    GPIO_PORTE_ICR_R = GPIO_ICR_GPIO_M;
}

//sets breath_time to 0 every 5 seconds
void breathingCycle()
{
    breath_time = 0;
    TIMER2_ICR_R = TIMER_ICR_TATOCINT;
}

//-----------------------------------------------------------------------------
// Main
//-----------------------------------------------------------------------------

int main(void)
{
    USER_DATA data;

    initHw();
    initUart0();
    initAdc0Ss3();
    setUart0BaudRate(115200, 40e6);
    PWM1_0_CMPB_R = 0;

    enableTimerMode(); // Timers
    enableDataLine();  // Enable DT

    // Use AIN8 input with N=4 hardware sampling
    setAdc0Ss3Mux(8);
    setAdc0Ss3Log2AverageCount(2);

    char str[80];
    bool valid = false;

    while (true)
    {
        putsUart0("\n");
        putsUart0("Command list:\n\n");
        putsUart0("     \"pulse\"                                    prints pulse (beats per minute)\n");
        putsUart0("     \"respiration\"                              prints respiration (breaths per minute)\n");
        putsUart0("     \"alarmPulse\" \"min\" \"max\"                   sets pulse limits\n");
        putsUart0("     \"alarmRespiration\" \"min\" \"max\"             sets respiration limits\n");
        putsUart0("     \"speaker\" ON|OFF                           turns speaker ON/OFF\n\n");

        putsUart0(" > ");
        valid = false;

        getsUart0(&data);
        //putsUart0(data.buffer);

        // Parse fields
        parseFields(&data);

        // pulse
        if (isCommand(&data, "pulse", 0))
        {
            valid = true;
            if(pulse_active == false)
            {
                putsUart0("\n Pulse Not Detected\n");
                continue;
            }
            putsUart0("\n");
            putsUart0("Pulse Rate:    ");
            putsUart0(itostr(pulseTime));
            putsUart0(" bpm\n");
        }


        // respiration
        if (isCommand(&data, "respiration", 0))
        {
            valid = true;

            if(breath_time == 0)
            {
                putsUart0("\n Breath Not Detected\n");
                continue;
            }

            snprintf(str, sizeof(str), "\nRespiration Rate:    %d bpm\n", breath_time);
            putsUart0(str);
        }

        // alarmPulse min max
        if (isCommand(&data, "alarmPulse", 2))
        {
            uint32_t minimum = getFieldInteger(&data, 1);
            uint32_t maximum = getFieldInteger(&data, 2);
            valid = true;

            alarmPulseMin = minimum;
            alarmPulseMax = maximum;
        }

        // alarmRespiration min max
        if (isCommand(&data, "alarmRespiration", 1))
        {
            uint32_t minimum = getFieldInteger(&data, 1);
            uint32_t maximum = getFieldInteger(&data, 2);
            valid = true;

            alarmRespirationMin = minimum;
            alarmRespirationMax = maximum;
        }

        // speaker ON|OFF
        if (isCommand(&data, "speaker", 1))
        {
            char* str = getFieldString(&data, 1);
            valid = true;

            if(customStrcmp("ON", str))
            {
                speakerStatus = true;
            }
            else if(customStrcmp("OFF", str))
            {
                speakerStatus = false;
                PWM1_0_CMPB_R = 0;
            }
        }

        // Process other commands here
        // Look for error
        if (!valid)
        {
            putsUart0("\nInvalid command\n");
        }
    }
}
