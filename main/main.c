#include "freertos/FreeRTOS.h"
#include <string.h>
//#include <stdio.h>
#include "esp_log.h"
#include "nvs_flash.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"

#include "esp_system.h"
#include "esp_wifi.h"
#include "esp_event.h"

#include "lwip/err.h"
#include "lwip/sys.h"


#define  EXAMPLE_ESP_WIFI_SSID  "MIKUSAMA"
#define  EXAMPLE_ESP_WIFI_PASS  "chuyinweilai"
#define  EXAMPLE_ESP_MAXIMUM_RETRY 10



static EventGroupHandle_t s_wifi_event_group;

#define WIFI_CONNECTED_BIT BIT0
#define WIFI_FAIL_BIT      BIT1

static int s_retry_num = 0;

void print_nvs_info(){

    nvs_stats_t nvs_status_default;
    nvs_get_stats(NULL,&nvs_status_default);
    printf("Count: UsedEntries = (%zu) , FreeEntries = (%zu) ,"
           " AvailableEntreis = (%zu) , TotalEntreis = (%zu) , Namespace_count = (%zu) \n" ,
           nvs_status_default.used_entries , nvs_status_default.free_entries, nvs_status_default.available_entries ,
           nvs_status_default.total_entries , nvs_status_default.namespace_count);

    nvs_iterator_t  it = NULL;
    //esp_err_t res = nvs_entry_find(mynvs_partionname ,nvs_namespace ,NVS_TYPE_ANY , &it);
}

void print_allNamespace(){

    //size_t namespace_count;

}

//设置wifi call back 函数
static void event_handler(void* arg,esp_event_base_t event_base,int32_t event_id,void* event_data)
{
    if(event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START)
    {
        ESP_LOGI("CONNECT" , "Connecting!!");
        esp_wifi_connect();
    }
    else if (event_base == WIFI_EVENT  && event_id == WIFI_EVENT_STA_DISCONNECTED){
        if( s_retry_num < EXAMPLE_ESP_MAXIMUM_RETRY){
            esp_wifi_connect();
            s_retry_num++;
            ESP_LOGI("WIFI", "retry to AP");
        }else{
            xEventGroupSetBits(s_wifi_event_group , WIFI_FAIL_BIT);
            ESP_LOGI("WIFI" , "FAILED to connect");
        }
    }
    else if(event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP){
        ip_event_got_ip_t* event = (ip_event_got_ip_t*) event_data;
        ESP_LOGI("WIFI" , "got ip: " IPSTR, IP2STR( &event->ip_info.ip)); // -> 优先级更高
        s_retry_num = 0;
        xEventGroupSetBits(s_wifi_event_group , WIFI_CONNECTED_BIT);
    }

}

void wifi_init_sta()
{

    s_wifi_event_group = xEventGroupCreate();   // 创建Event群组，其他task如果被激活，则这个对象能够响应
    ESP_ERROR_CHECK(esp_netif_init()); //初始化TCP/IP stack 任务（LwIP任务）
    ESP_ERROR_CHECK(esp_event_loop_create_default()); //Init EVENT loop 中间件用于监听底层发来的
    esp_netif_create_default_wifi_sta(); // 创建TCP-IP task创建


    // 初始化 wifi 模块 -- 固定操作 ,
    ESP_LOGI("WIFI", "1 WI-FI初始化第一部分");
    wifi_init_config_t  cfg = WIFI_INIT_CONFIG_DEFAULT(); // 你可以初始化 Wi-Fi 栈并配置各种参数，例如 Wi-Fi 模式（STA、AP 或 STA+AP）、最大传输功率、Wi-Fi 认证模式等。
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));       //  创建 wifi task

    esp_event_handler_instance_t  instance_id; // 方便后续取消监听（取消注册）对应的事件
    esp_event_handler_instance_t  instance_ip; // 这个实例是一个唯一的标识符，代表了具体的事件处理函数。

    //esp_event_handler_t 这个参数定义了 ---> 委托（函数指针的类型）
    /// arg1 哪种event
    /// arg2 该event哪个ID
    /// arg3 - 4 所需要的处理函数 以及事件触发时所携带的参数
    esp_event_handler_instance_register(WIFI_EVENT,
                                        ESP_EVENT_ANY_ID,
                                        &event_handler,
                                        NULL,
                                        &instance_id);

    esp_event_handler_instance_register(IP_EVENT,
                                        IP_EVENT_STA_GOT_IP,
                                        &event_handler,
                                    NULL,
                                    &instance_ip); // 在注册完成时返回该实例子


    //结构体用于配置Wi-Fi连接的参数
    wifi_config_t wifi_config = {
            .sta = {
              .ssid = EXAMPLE_ESP_WIFI_SSID,
              .password = EXAMPLE_ESP_WIFI_PASS,
              //.threshold.authmode = ESP_WIFI_S,
            },
    };


    ESP_LOGI("WIFI", "2 WI-FI初始化第二部分");
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));

    esp_err_t  confgerr = esp_wifi_set_config(WIFI_IF_STA,&wifi_config); //*** ERROR: ***
    //ESP_LOGE("CINFIG" , "ERRR%s" , esp_err_to_name(confgerr));

    //ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA,&wifi_config)); //设定要链接的wifi类型
    ESP_ERROR_CHECK(esp_wifi_start());


    ESP_LOGI("WIFI", "3 WI-FI尝试连接 ");
    // Handler 对事件做响应，这里已知等待
    EventBits_t  bits = xEventGroupWaitBits(
            s_wifi_event_group,
            WIFI_CONNECTED_BIT | WIFI_FAIL_BIT,
            pdFALSE,
            pdFALSE,
            portMAX_DELAY);



    if(bits & WIFI_CONNECTED_BIT){
        ESP_LOGI("WIFI" , "connected to ap SSID:%s password:%s",
                 EXAMPLE_ESP_WIFI_SSID ,EXAMPLE_ESP_WIFI_PASS);
        ESP_LOGI("WIFI" , "SUCCESSS！！！！");

    }else if( bits & WIFI_FAIL_BIT){
        ESP_LOGE("WIFI", "Cannot Connect to WIFI!");
    }

}



TaskStatus_t* pxTaskStatusArray;

void task_list(void)
{
    char buffer[24];  // 定义一个足够大的缓冲区
    vTaskList(buffer);
    uint32_t ulTotalRunTime, ulStatsAsPercentage;

    uint  TaskSize = uxTaskGetNumberOfTasks();
    ESP_LOGE("Task" , "number of task %d" , TaskSize);

    pxTaskStatusArray = pvPortMalloc(sizeof(TaskStatus_t) * TaskSize);

    if( pxTaskStatusArray != NULL ){
        TaskSize = uxTaskGetSystemState(pxTaskStatusArray , TaskSize, &ulTotalRunTime); //这个接口才是真的在创建Task信息
        printf("Task Numebr %d \n", TaskSize);
        for( int x = 0 ; x < TaskSize ; ++x){
            sprintf( buffer, "%s\t", pxTaskStatusArray[ x ].pcTaskName);
            printf("MY String : %s\n" , buffer);
        }
    }

//    for (int i = 0 ; i<1024 ; ++i){
//        printf("%c", buffer[i]);
//
//    }
    //buffer[sizeof(buffer) - 1] = '\0';
    //printf("Task List:\n%.*s\n", 10240, buffer);

    //printf("Task List:\n%.*s\n", buffer);
}


void app_task(void *pt){
    //使用函数指针创建任务
    ESP_LOGI("APP_TASK", "APP INIT");
    while(1){
        ESP_LOGI("APP_TASK", "APP RUNING");
        vTaskDelay(1000/portTICK_PERIOD_MS);
    }
}

void app_main(void)
{
    ESP_LOGI("TASK" , "显示所有FreeRTOS任务");

//    xTaskCreate(
//            app_task,
//            "app_task",
//            1024*10,
//            NULL,
//            1,
//            NULL
//            );

    ESP_LOGI("WIFI" , "0, 初始化NVS");
    esp_err_t ret = nvs_flash_init();
    if(ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND){
        //如果内存爆
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }

    print_nvs_info();
    ESP_ERROR_CHECK(ret);

    //ESP_LOGI("TASK" , "CREATE APPTASK");

    ESP_LOGI("WIFI STATION" ,  "ESP_WIFI_MODE_STA");
    wifi_init_sta();

    //task_list(); //显示当前所有任务 +3 event task ， TCP task ， WI-FI task


}
