#include <stdint.h>
#include <stdbool.h>

#include "FreeRTOS.h"
#include "task.h"

#include "inc/hw_gpio.h"
#include "inc/hw_memmap.h"
#include "inc/hw_ints.h"
#include "inc/hw_types.h"
#include "inc/hw_i2c.h"

#include "driverlib/gpio.h"
#include "driverlib/i2c.h"
#include "driverlib/interrupt.h"
#include "driverlib/sysctl.h"

#include "time_struct.h"
#include "priorities.h"
#include "button_pins.h"

#define LCD_TASK_SIZE_WORDS 100

#define LCD_I2C_SLAVE_ADDRESS 0x3c

#define LCD_COMMAND_CLEAR_DISPLAY 0x01
#define LCD_COMMAND_FUNCTION_SET 0x38
#define LCD_COMMAND_DISPLAY_ON 0x0c
#define LCD_COMMAND_DISPLAY_ON_WITH_CURSOR 0x0e
#define LCD_COMMAND_ENTRY_MODE_SET 0x06
#define LCD_COMMAND_LINE_1 0x80
#define LCD_COMMAND_LINE_2 0xc0

#define LCD_CO 0x80

#define LCD_A0_COMMAND 0x00
#define LCD_A0_DATA 0x40

#define DEBOUNCE_TIME_MILLISECONDS 100
#define LCD_DELAY_MICROSECONDS 500

#define SYSCTL_DELAY_MILISECONDS_FACTOR (configCPU_CLOCK_HZ / 3000)
#define SYSCTL_DELAY_MICROSECONDS_FACTOR (SYSCTL_DELAY_MILISECONDS_FACTOR / 1000)

typedef enum button_event_t{
    ALARM_SET,
    HOUR_UP,
    HOUR_DOWN,
    MINUTE_UP,
    MINUTE_DOWN,
    IDLE
}button_event_t;

extern uint32_t g_ui32SysClock;
extern time_t g_cur_time;

extern volatile bool alarm_ringing;

TaskHandle_t lcd_task_handle;

volatile bool alarm_set;
volatile time_t user_time;
static volatile button_event_t cur_button;

// ISR for the buttons, sets the cur_button variable and then
// allows one loop of lcd_task to execute
void GPIO_PK_handler(void){

    GPIOIntDisable(GPIO_PORTK_BASE, HOUR_UP_PIN | HOUR_DOWN_PIN | MINUTE_UP_PIN | MINUTE_DOWN_PIN | ALARM_SET_PIN);

    // debounce the button inputs
    SysCtlDelay(SYSCTL_DELAY_MILISECONDS_FACTOR * DEBOUNCE_TIME_MILLISECONDS);
    uint32_t int_status = GPIOIntStatus(GPIO_PORTK_BASE, false);
    uint32_t gpio_in = GPIOPinRead(GPIO_PORTK_BASE, HOUR_UP_PIN | HOUR_DOWN_PIN | MINUTE_UP_PIN | MINUTE_DOWN_PIN | ALARM_SET_PIN);

    if((int_status & HOUR_UP_PIN) && (gpio_in & HOUR_UP_PIN)){
        cur_button = HOUR_UP;
    }
    else if((int_status & HOUR_DOWN_PIN) && (gpio_in & HOUR_DOWN_PIN)){
        cur_button = HOUR_DOWN;
    }
    else if((int_status & MINUTE_UP_PIN) && (gpio_in & MINUTE_UP_PIN)){
        cur_button = MINUTE_UP;
    }
    else if((int_status & MINUTE_DOWN_PIN) && (gpio_in & MINUTE_DOWN_PIN)){
        cur_button = MINUTE_DOWN;
    }
    else if((int_status & ALARM_SET_PIN) && (gpio_in & ALARM_SET_PIN)){
        alarm_ringing = false;
        cur_button = ALARM_SET;
    }

    vTaskResume(lcd_task_handle);

}

static inline void i2c_init(void){
    I2CMasterInitExpClk(I2C0_BASE, g_ui32SysClock, false);
    I2CMasterSlaveAddrSet(I2C0_BASE, LCD_I2C_SLAVE_ADDRESS, false);
    I2CTxFIFOConfigSet(I2C0_BASE, I2C_FIFO_CFG_TX_MASTER);
}

// send command to lcd via i2c fifo
static inline void lcd_send_command(uint8_t command){
    I2CMasterBurstLengthSet(I2C0_BASE, 0x02);

    I2CFIFODataPut(I2C0_BASE, LCD_A0_COMMAND);
    I2CFIFODataPut(I2C0_BASE, command);

    I2CMasterControl(I2C0_BASE, I2C_MASTER_CMD_FIFO_BURST_SEND_START);

    while(I2CMasterBusy(I2C0_BASE)){
        SysCtlDelay(SYSCTL_DELAY_MICROSECONDS_FACTOR * LCD_DELAY_MICROSECONDS);
    }
}

// send character data to lcd via i2c fifo, maximum of 7 characters at a time
static inline void lcd_send_data(char *data, uint8_t size){
    I2CMasterBurstLengthSet(I2C0_BASE, size + 0x01);

    I2CFIFODataPut(I2C0_BASE, LCD_A0_DATA);

    uint8_t i;
    for(i = 0; i != size; ++i){
        I2CFIFODataPut(I2C0_BASE, data[i]);
    }

    I2CMasterControl(I2C0_BASE, I2C_MASTER_CMD_FIFO_BURST_SEND_START);

    while(I2CMasterBusy(I2C0_BASE)){
        SysCtlDelay(SYSCTL_DELAY_MICROSECONDS_FACTOR * LCD_DELAY_MICROSECONDS);
    }
}

static inline void lcd_init(void){
    lcd_send_command(LCD_COMMAND_FUNCTION_SET);
    vTaskDelay(1 / portTICK_PERIOD_MS);

    lcd_send_command(LCD_COMMAND_DISPLAY_ON);
    vTaskDelay(1 / portTICK_PERIOD_MS);

    lcd_send_command(LCD_COMMAND_CLEAR_DISPLAY);
    vTaskDelay(10 / portTICK_PERIOD_MS);

    lcd_send_command(LCD_COMMAND_ENTRY_MODE_SET);
    vTaskDelay(1 / portTICK_PERIOD_MS);

    char buf[16] = "Connecting...";

    lcd_send_data(buf, 7);
    vTaskDelay(1 / portTICK_PERIOD_MS);

    lcd_send_data(buf + 7, 6);
    vTaskDelay(1 / portTICK_PERIOD_MS);
}

// fills a given char buffer with the time
static void lcd_fill_time(volatile time_t *time, char *buf){

    uint8_t hour_ones = time->hour;
    uint8_t hour_tens = 0;
    while(hour_ones > 9){
        hour_ones -= 10;
        ++hour_tens;
    }

    uint8_t minute_ones = time->minute;
    uint8_t minute_tens = 0;
    while(minute_ones > 9){
        minute_ones -= 10;
        ++minute_tens;
    }

    buf[0] = '0' + hour_tens;
    buf[1] = '0' + hour_ones;
    buf[2] = ':';
    buf[3] = '0' + minute_tens;
    buf[4] = '0' + minute_ones;
    buf[5] = ' ';
    buf[6] = 'P';
    buf[7] = 'S';
    buf[8] = 'T';
}

// writes the times to the entire 16x2 lcd via i2c
static inline void lcd_update(void){
    lcd_send_command(LCD_COMMAND_LINE_1);
    SysCtlDelay(SYSCTL_DELAY_MICROSECONDS_FACTOR * LCD_DELAY_MICROSECONDS);

    char buf[16];
    buf[0] = 'N';
    buf[1] = 'o';
    buf[2] = 'w';
    buf[3] = ':';
    buf[4] = ' ';

    lcd_fill_time(&g_cur_time, buf + 5);

    lcd_send_data(buf, 7);
    SysCtlDelay(SYSCTL_DELAY_MICROSECONDS_FACTOR * LCD_DELAY_MICROSECONDS);
    lcd_send_data(buf + 7, 7);
    SysCtlDelay(SYSCTL_DELAY_MICROSECONDS_FACTOR * LCD_DELAY_MICROSECONDS);

    lcd_send_command(LCD_COMMAND_LINE_2);
    SysCtlDelay(SYSCTL_DELAY_MICROSECONDS_FACTOR * LCD_DELAY_MICROSECONDS);

    if(alarm_set == true){
        buf[0] = 'A';
        buf[1] = 'l';
        buf[2] = 'a';
        buf[3] = 'r';
        buf[4] = 'm';
        buf[5] = ':';
        buf[6] = ' ';
    }
    else{
        buf[0] = 'S';
        buf[1] = 'e';
        buf[2] = 'l';
        buf[3] = 'e';
        buf[4] = 'c';
        buf[5] = 't';
        buf[6] = ':';
    }

    lcd_fill_time(&user_time, buf + 7);

    lcd_send_data(buf, 7);
    SysCtlDelay(SYSCTL_DELAY_MICROSECONDS_FACTOR * LCD_DELAY_MICROSECONDS);
    lcd_send_data(buf + 7, 7);
    SysCtlDelay(SYSCTL_DELAY_MICROSECONDS_FACTOR * LCD_DELAY_MICROSECONDS);
    lcd_send_data(buf + 14, 2);
    SysCtlDelay(SYSCTL_DELAY_MICROSECONDS_FACTOR * LCD_DELAY_MICROSECONDS);

}

// updates the lcd when necessary
void lcd_task(void *args){

    i2c_init();
    lcd_init();

    // wait for DHCP to complete
    while(g_cur_time.hour == UNSET_HOUR){
        vTaskDelay(250 / portTICK_PERIOD_MS);
    }

    // this loop executes only once every time a change happens to g_cur_time or user_time,
    // since that is when the lcd display needs to be updated
    // the GPIO interrupt remains disabled here to prevent double interrupts, which causes problems
    while(1){

        if(alarm_set == false){
            switch(cur_button){
            case HOUR_UP:
                user_time.hour = (user_time.hour == 23)? 0: user_time.hour + 1;
                break;
            case HOUR_DOWN:
                user_time.hour = (user_time.hour == 0)? 23: user_time.hour - 1;
                break;
            case MINUTE_UP:
                user_time.minute = (user_time.minute == 59)? 0: user_time.minute + 1;
                break;
            case MINUTE_DOWN:
                user_time.minute = (user_time.minute == 0)? 59: user_time.minute - 1;
                break;
            case ALARM_SET:
                alarm_set = true;
                break;
            case IDLE:
                break;
            }
        }
        else{
            switch(cur_button){
            case ALARM_SET:
                alarm_set = false;
                break;
            default:
                break;
            }

        }

        cur_button = IDLE;

        lcd_update();

        GPIOIntClear(GPIO_PORTK_BASE, HOUR_UP_PIN | HOUR_DOWN_PIN | MINUTE_UP_PIN | MINUTE_DOWN_PIN | ALARM_SET_PIN);
        while(GPIOIntStatus(GPIO_PORTK_BASE, false) & (HOUR_UP_PIN | HOUR_DOWN_PIN | MINUTE_UP_PIN | MINUTE_DOWN_PIN | ALARM_SET_PIN)){}
        GPIOIntEnable(GPIO_PORTK_BASE, HOUR_UP_PIN | HOUR_DOWN_PIN | MINUTE_UP_PIN | MINUTE_DOWN_PIN | ALARM_SET_PIN);

        vTaskSuspend(NULL);
    }
}

// create the lcd task
void inline lcd_task_init(void){

    GPIOIntRegister(GPIO_PORTK_BASE, GPIO_PK_handler);
    IntPrioritySet(INT_GPIOK_TM4C129, GPIO_PK_INT_PRIORITY);

    user_time.hour = 0;
    user_time.minute = 0;
    alarm_set = false;
    cur_button = IDLE;

    xTaskCreate(lcd_task, "lcd_task", LCD_TASK_SIZE_WORDS, NULL, PRIORITY_LCD_TASK, &lcd_task_handle);
}

