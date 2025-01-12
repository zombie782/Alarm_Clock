#include <stdint.h>
#include <stdbool.h>

#include "FreeRTOS.h"
#include "task.h"

#include "inc/hw_gpio.h"
#include "inc/hw_memmap.h"
#include "inc/hw_ints.h"
#include "inc/hw_types.h"

#include "driverlib/gpio.h"
#include "driverlib/pwm.h"
#include "driverlib/interrupt.h"
#include "driverlib/sysctl.h"

#include "time_struct.h"
#include "priorities.h"

#define ALARM_TASK_SIZE_WORDS 50

#define ALARM_INTERVAL_MILLISECONDS 500
#define PWM_FREQ 1000

TaskHandle_t alarm_task_handle;

volatile bool alarm_ringing;

// sends PWM signal to piezo buzzer for alarm
// can only be exited using a GPIO interrupt with the ALARM_SET pin to turn off the alarm
void alarm_task(void *args){

    while(1){
        if(!alarm_ringing){
            vTaskSuspend(NULL);
        }

        PWMOutputState(PWM0_BASE, PWM_OUT_5_BIT, true);
        vTaskDelay(ALARM_INTERVAL_MILLISECONDS / portTICK_PERIOD_MS);

        PWMOutputState(PWM0_BASE, PWM_OUT_5_BIT, false);
        vTaskDelay(ALARM_INTERVAL_MILLISECONDS / portTICK_PERIOD_MS);
    }
}

// initialize pwm generator so that it sends a PWM_FREQ square wave to the piezo buzzer
void inline pwm_init(void){
    PWMClockSet(PWM0_BASE, PWM_SYSCLK_DIV_2);
    PWMGenConfigure(PWM0_BASE, PWM_GEN_2, PWM_GEN_MODE_DOWN);
    PWMGenPeriodSet(PWM0_BASE, PWM_GEN_2, configCPU_CLOCK_HZ / (2 * PWM_FREQ));
    PWMPulseWidthSet(PWM0_BASE, PWM_OUT_5, (configCPU_CLOCK_HZ / (2 * PWM_FREQ)) / 2);
    PWMGenEnable(PWM0_BASE, PWM_GEN_2);
}

// create the alarm task
void inline alarm_task_init(void){
    alarm_ringing = false;

    pwm_init();

    xTaskCreate(alarm_task, "alarm_task", ALARM_TASK_SIZE_WORDS, NULL, PRIORITY_ALARM_TASK, &alarm_task_handle);
}
