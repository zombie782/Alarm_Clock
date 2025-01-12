#include <stdint.h>
#include <stdbool.h>

#include "FreeRTOS.h"
#include "task.h"

#include "driverlib/gpio.h"

#include "inc/hw_gpio.h"
#include "inc/hw_memmap.h"

#include "utils/lwiplib.h"

#include "http/http_client.h"

#include "time_struct.h"
#include "priorities.h"
#include "button_pins.h"

#define TIME_TASK_SIZE_WORDS 250

#define TIME_INTERVAL_MILLISECONDS 500

time_t g_cur_time;

extern TaskHandle_t lcd_task_handle;
extern TaskHandle_t alarm_task_handle;

extern volatile bool alarm_set;
extern volatile time_t user_time;

extern volatile bool alarm_ringing;

static uint32_t l_ui32IPAddress;
static const ip_addr_t IP_ADDR_GOOGLE = {0xae2efb8e};

static void result(void *arg, httpc_result_t httpc_result, u32_t rx_content_len, u32_t srv_res, err_t err){

}

// convert to PST
static inline uint8_t gmt_to_pst_hour(uint8_t gmt_hour){
    return (gmt_hour >= 8)? gmt_hour - 8: gmt_hour + 16;
}

// finds the date field in the http header and stores it in g_cur_time
// returns ERR_ABRT because we only need the header in order to get the current time
// if the time changes, it updates the g_cur_time variable and allows
// one loop of lcd_task to run to update the lcd with the current time
static err_t header(httpc_state_t *connection, void *arg, struct pbuf *hdr, u16_t hdr_len, u32_t content_len){

    char *payload_ptr = (char*)(hdr->payload);

    uint16_t i;
    for(i = 0; i != hdr_len; ++i){
        if(payload_ptr[i] == 'D' &&
           hdr_len - i > 34 &&
           payload_ptr[i + 1] == 'a' &&
           payload_ptr[i + 2] == 't' &&
           payload_ptr[i + 3] == 'e' &&
           payload_ptr[i + 4] == ':'){

            uint8_t http_hour = (payload_ptr[i + 23] - '0') * 10 + (payload_ptr[i + 24] - '0');
            uint8_t pst_hour = gmt_to_pst_hour(http_hour);

            uint8_t http_minute = (payload_ptr[i + 26] - '0') * 10 + (payload_ptr[i + 27] - '0');

            if(http_minute != g_cur_time.minute || pst_hour != g_cur_time.hour){
                taskENTER_CRITICAL();

                g_cur_time.hour = pst_hour;
                g_cur_time.minute = http_minute;

                taskEXIT_CRITICAL();

                vTaskResume(lcd_task_handle);

                // sends control to the alarm task if the current time equals the set alarm time
                // disables all button GPIO interrupts except SET_ALARM because that is the only way to exit the alarm task
                // not disabling other interrupts caused the speed of the alarm buzzer to be changeable
                // by pressing the other buttons because of the debounce time inside the ISR for the button interrupts
                if(user_time.minute == g_cur_time.minute && user_time.hour == g_cur_time.hour && alarm_set){
                    alarm_ringing = true;
                    GPIOIntDisable(GPIO_PORTK_BASE, HOUR_UP_PIN | HOUR_DOWN_PIN | MINUTE_UP_PIN | MINUTE_DOWN_PIN);
                    vTaskResume(alarm_task_handle);
                }

                break;
            }
        }
    }

    return ERR_ABRT;
}

static err_t recv(void *arg, struct tcp_pcb *tpcb, struct pbuf *p, err_t err){
    return ERR_OK;
}

// gets the time from an http request to google.com every INTERVAL_MILLISECONDS and converts it from GMT to PST
// makes use of http_client from lwip 2.2.0 modified to work with lwip 1.4.1, which TI provides a library for
static void time_task(void *args){

    const char empty[] = "/";
    httpc_connection_t http_settings;
    http_settings.use_proxy = 0;
    http_settings.result_fn = result;
    http_settings.headers_done_fn = header;

    while(1){
        l_ui32IPAddress = lwIPLocalIPAddrGet();
        if(l_ui32IPAddress != 0x0 && l_ui32IPAddress != 0xffffffff){
            httpc_get_file(&IP_ADDR_GOOGLE, HTTP_DEFAULT_PORT, empty, &http_settings, recv, NULL, NULL);
        }
        vTaskDelay(TIME_INTERVAL_MILLISECONDS / portTICK_PERIOD_MS);
    }

}

// create the time task
void inline time_task_init(void){
    g_cur_time.hour = UNSET_HOUR;
    g_cur_time.minute = UNSET_MINUTE;
    xTaskCreate(time_task, "time_task", TIME_TASK_SIZE_WORDS, NULL, PRIORITY_TIME_TASK, NULL);
}
