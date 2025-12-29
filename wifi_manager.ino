#include <WiFi.h>
#include <Arduino.h>
#include <PubSubClient.h>

// Cấu hình wifi 
#define wifi_ssid "509 TQV"
#define wifi_pass "83838383"
// Cấu hình MQTT
const char* mqtt_server = "192.168.83.105";
const int mqtt_port = 1883;
const char* topic_control_led = "control/led";
const char* topic_publish_led = "publish/led";
// cấu hình phần cứng 
#define led 10
#define button 4
int led_state = 0;

WiFiClient espClient;
PubSubClient client;

void reconnectWiFi(){
  WiFi.disconnect();
  Serial.print("connecting wifi to :");
  Serial.print(wifi_ssid);
  WiFi.begin(wifi_ssid, wifi_pass);
}
void setup_wifi(){
  Serial.print("connecting to ");
  Serial.println(wifi_ssid); 
  WiFi.mode(WIFI_STA); // chuyển sang ché độ station 
  // esp32 có 3 chế độ wifi
  // WiFi_STA: esp32 là một thiết bị con kết nối vào wifi có sẵn, có IP do router cấp 
      //-> dùng khi gửi dữ liệu lên internet, server, mqtt, firebase 
  // WiFi_AP: (access point): esp32 tạo ra wifi riêng, điện thoại, máy tính kết nối trực tiếp vào esp32
      // -> sử dụng khi cấu hình thiết bị, không có router
  // WiFi_AP_STA: vừa kết nối router, vừa phát wifi cho thiết bị khác kết nối 

  WiFi.begin(wifi_ssid, wifi_pass);
  // nếu là WiFi_AP: WiFi.soft("tên wifi phát ra","mật khẩu")
  while(WiFi.status() != WL_CONNECTED){
    delay(500);
    Serial.print(".");
  }
  Serial.println("connected");
  Serial.println(WiFi.localIP());

}
void control_led(int new_state){
  if (new_state != led_state) {
    led_state = new_state;

    digitalWrite(led, led_state ? HIGH : LOW);

    // publish khi CÓ THAY ĐỔI
    if (client.connected()) {
      client.publish(topic_publish_led, led_state ? "1" : "0", true);
    }

    Serial.print("LED changed to: ");
    Serial.println(led_state);
  }
}

void call_back(char* topic, byte* payload, unsigned int length){
  String message;
  for(unsigned int i = 0; i< length; i++){
    Serial.print((char)payload[i]);
    message += (char)payload[i];
  }
  message.trim();
  Serial.println();
  if (strcmp(topic, topic_control_led) == 0) {
    int a = atoi(message.c_str());
    if (a == 0 || a == 1) {
      control_led(a);
    }
  }
}
void setup_mqtt(WiFiClient& espClient){
  client.setClient(espClient);
  client.setServer(mqtt_server, mqtt_port);
  client.setCallback(call_back);
}
void mqtt_reconnect() {
  static unsigned long last_reconnect_attempt = 0;
  unsigned long now = millis();

  // Chỉ thử kết nối lại sau mỗi 5 giây
  if (now - last_reconnect_attempt > 5000) {
    last_reconnect_attempt = now;
    Serial.print("Attempting MQTT connection...");
    
    if (client.connect("esp32_client")) {
      Serial.println("connected");
      client.subscribe(topic_control_led);
    } else {
      Serial.print("failed, rc=");
      Serial.print(client.state());
      Serial.println(" try again in 5 seconds");
    }
  }
}
void mqtt_loop() {
  if (!client.connected()) {
    mqtt_reconnect();
  } else {
    client.loop();
  }
}
void setup_led(){
  pinMode(led,OUTPUT);
  digitalWrite(led, LOW);
  led_state = 0;
}
void setup_button(){
  pinMode(button, INPUT_PULLUP);
}
void control_led_by_button() {
  int reading = digitalRead(button);

  // Phát hiện có sự thay đổi trạng thái (do bấm hoặc do nhiễu)
  if (reading != button_state_last) {
    last_time_debounce = millis();
  }

  // Chỉ xử lý khi trạng thái đã ổn định đủ lâu (debounce_delay)
  if ((millis() - last_time_debounce) > debounce_delay) {
    // Biến static để lưu trạng thái "thực sự" của nút sau khi lọc nhiễu
    static int confirmed_button_state = HIGH;
    // Nếu trạng thái ổn định mới khác với trạng thái cũ đã lưu
    if (reading != confirmed_button_state) {
      confirmed_button_state = reading;
      // Nếu trạng thái mới là LOW (nghĩa là vừa nhấn xuống)
      if (confirmed_button_state == LOW) {
        Serial.println("Nut bam vat ly: Dao trang thai LED");
        control_led(!led_state); 
      }
    }
  }
  // Cập nhật trạng thái cuối cùng để so sánh ở vòng loop tiếp theo
  button_state_last = reading;
}
void setup(){
  Serial.begin(115200);
  setup_led();
  setup_button();
  setup_wifi();
  setup_mqtt(espClient);
}
void loop(){
  if(millis() - wifi_last > wifi_limit){
    wifi_last = millis();
    if(WiFi.status()!= WL_CONNECTED ){
      reconnectWiFi();
    } 
  }
  mqtt_loop();
  control_led_by_button();
}
/*
  FreeRTOS : kernel là nhân của hệ điều hành, thực chất là một quy ước có nhiệm vụ điều phối các công việc của RTOS
  kernel sẽ điều phối hoạt động của các Task dựa vào bộ lập lịch và các thuật toán lập lịch 
  kernel sẽ quản lý tài nguyên phần cứng-bộ nhớ để lưu trữ hoạt động các task 
  kernel quản lý các công việc giao tiếp giữa các Task, xử lý ngắt 

  Một số thuật toán lập lịch RTOS 
    + Round-Robin : lập kịch kiểu "công bằng", tức là mỗi task sẽ có một thời gian thực thi nhất định, (gọi là time slice)
                    hết khoảng thời gian này thì sẽ phải nhường CPU cho các task khác thực hiện 
        -> ưu điểm : đơn giản, thực hiện tuần tự 
        -> nhước điểm : nếu có 1 task quan trọng cần thực hiện ngay thì vẫn phải chờ các task khác nhau 
        * cấu trúc : 
        #include "FreeRTOS.h"
        void Task1_LED(void *pvParameters){
          while(1){
            ......
            vTaskDelay(pdMS_TO_TICKS(1000));
          }
        } 
        int main(void){
          xTaskCreate(Task1_name, "name", 123, NULL, 1 , NULL);
          ...
          vTaskStartScheduler();
          while (1) {}
        }
    + Preemptive : Task quan trọng sẽ được gán quyền ưu tiên -> thực hiện trước , chiếm quyền sử dụng CPU của 
                   task đang thực hiện nếu cần, sau khi sử lý xong thì trả lại quyền cho task vừa bị chiếm 
        -> ưu điểm : sử lý được các task khẩn cấp 
        -> nhược điểm : các task ưu tiên có thể sẽ chiếm hết quyền sử dụng CPU
    + Cooperative : 
        * cấu trúc : 
        #include "FreeRTOS.h"
        void Task1_LED(void *pvParameters){
          while(1){
            ......
            vTaskDelay(pdMS_TO_TICKS(1000));
            taskYIELD();

          }
        } 
        int main(void){
          xTaskCreate(Task1_name, "name", 123, NULL, 1 , NULL);
        }
  Các thành phần chính 
    a. Task Management 
    // tạo task 
    xTask(TaskFunction, "TaskName", StackSize, Parameters, Priority, TaskHandle); 
    Trong đó:
      TaskFunction: hàm thực thi, tên của hàm chứa đoạn code muốn task chạy 
                    hàm này phải có cấu trúc vòng lặp vô tận for(;;) hoặc while(1), 
                    không được phép return, một hàm nhận vào một tham số void * trả về void 
      "Name": đặt tên cho task, FreeRTOS không dùng tên này để quản lý bên trong, 
            -> debug, theo dõi, khi sử dụng các công cụ theo dõi trên hệ thống, tên này giúp biết 
                task nào đang chiếm bao nhiêu CPU hoặc bị lỗi 
            note: độ dài tối đa : 16 ký tự 
      StackSize : số lượng bộ nhớ cấp cho Task để lưu trữ các biến cục bộ, gọi hàm và ngữ cảnh thực thi 
                đơn vị: bytes (512, 1024, 2048, 4096, )
                        word 
                note: nếu quá nhỏ -> Task sẽ bị Stack Overflow(tràn bộ nhớ) làm sập hệ thông 
                      nếu quá lớn -> lẵng phí Ram
      Parameters : tham số truyền vào : là một con trỏ trỏ tới bất kỳ loại dữ liệu nào muốn gửi vào task khi nó bắt đầu 
                    -> mục đích: giúp sử dụng lại code cho nhiều đối tượng 
                    nếu không dùng: truyền NULL
      Priority : độ ưu tiên 
              -> quy tắc : số càng lớn-> ưu tiên cao -> được CPU xử lý trước, các task khác phải đợi 
      TaskHandle: thẻ quản lý task 
              -> dùng để điều khiển task sau khi task được tạo ra 
              dùng khi :  vTaskSuspend(xLEDTaskHandle);    // Tạm dừng task
                          vTaskResume(xLEDTaskHandle);     // Tiếp tục task
                          uxTaskPriorityGet(xLEDTaskHandle); // Lấy độ ưu tiên
                          vTaskDelete(xLEDTaskHandle);     // Xóa task
              Không dùng truyền NULL
    b. Scheduler
      + Preemptive Scheduling: Ưu tiên cao hơn chiếm CPU
      + Time Slicing: Chia thời gian cho các task cùng priority
        sử dụng cơ chế Round Robin 
        cách hoạt động : Scheduler chia đều thời gian CPU cho các task, sau đó bị ép nhường chỗ cho task kế tiếp cùng priority
        Note: nếu không muốn chia thời gian : mà muốn task chạy xong mới đến task kia dù cùng priority thì có thể 
              tắt tính năng này trong file cấu hình FreeRTOSConfig.h bằng cách cấu hình configUSE_TIME_SLICING là 0
      + Configurable tick period: 1-1000Hz (thường 1000Hz = 1ms) nhịp đập của hệ thống 
        FreeRTOS sử dụng một bộ định thời phần cứn (hardware timew) để tạo ra các ngắt định kỳ gọi là Tick 
        Tần số (tick rate): thường được cấu hình qua configTICK_RATE_HZ 
          nếu là 1000hz: 1 tick = 1 ms -> phổ biến 
          nếu là 100hz : 1 tick = 10 ms  
    c. QUEUE
      queue : hàng đợi an toàn giữa các Task / ISR
              hoạt động theo FIFO: first in - first out
              Sử dụng khi : Task A gửi dữ liệu - Task B nhận dữ liệu 
                            không bị race condition (khi nhiều task cùng truy cập 1 tài nguyên chung và kết quả phụ thuộc 
                            vào thứ tự chạy của task -> bug ngẫu nhiên : task chạy trước , task chạy sau ->  kết quả khác nhau )
                            không cần mutex(mutual exclusion : khoá tài nguyên: chỉ cho 1 task vào dùng tài nguyên tại 1 thời điểm)
                -> dùng khi task A gửi dữ liệu task B 
                  button -> led 
                  MQTT -> control 
      // tạo queue 
      QueueHandle_t  xQueueCreate(QueueLength, ItemSize)
        trong đó xQueueCreate : 
                 QueueLength : Số phần tử tối đa queue chứa
                 ItemSize : Kích thước 1 phần tử (bytes)
                 trả về NULL nếu lỗi 
      Các hàm queue thường dùng
        + xQueueCreate : tạo 
        + xQueueSend : gửi dữ liệu 
            xQueueSend(QueueHandle, &Data, Timeout);
            trong đó QueueHandle : "Địa chỉ" của cái thùng chứa (Queue) mà bạn đã tạo trước đó.
                     &Data : Địa chỉ của biến chứa dữ liệu cần gửi. FreeRTOS sẽ copy toàn bộ vùng nhớ này vào Queue.
                     Timeout : Thời gian tối đa Task sẵn sàng ngồi "đợi" nếu Queue đang đầy.
        + xQueueReceive: nhận dữ liệu 
            xQueueReceive(QueueHandle, &Data, Timeout);
            trong đó QueueHandle : thẻ quản lý của queue muốn lấy dữ liệu 
                     &Data: địa chỉ của biến dùng chứa dữ liệu lấy ra , FreeRTOS sẽ coppy dữ liệu từ Queue vào biến này
                     Timeout: thười gian Task sẵn sàng vhowf nếu lúc gọi hàm mà Queue đang trống
         

        + xQueueOverwrite: ghi đè (queue size =1)
        + uxQueueMessagesWaiting : số phần tử đàn có 

    d. Semaphore và Mutex
      + Binary Semaphore: Đây là công cụ chủ yếu dùng để Đồng bộ hóa (Synchronization) giữa 
        Task với Task hoặc giữa Ngắt (Interrupt) với Task.
          cơ chế : chỉ có 2 giá trị 0(trống) và 1(có sẵn) 
                   Một task có thể "give", một task khác có thể "take" để số đếm tăng lên 
                   không có "chủ sở hữu"

      + Counting Semaphore: Quản lý một nhóm tài nguyên có số lượng cụ thể 
          cơ chế : có giá trị từ 0 đến 1 giá trị cực đại (MaxCount)
          -> quản lý hàng đợi dữ liệu, quản lý tài nguyên phần cứng giống nhau
      + mutex (mutual exclusion)
        cơ chế : có chủ sở hữu, task nào mutex thì task       
    e.  Timer
      B1: tạo Timer 
        TimerHandle_t xTimerCreate(
          "TimerName",       // Tên timer (dùng để debug)
          xTimerPeriodInTicks, // Chu kỳ (ví dụ: pdMS_TO_TICKS(1000))
          uxAutoReload,      // pdTRUE = Auto-reload, pdFALSE = One-shot
          pvTimerID,         // ID để phân biệt nếu dùng chung 1 callback
          vTimerCallback     // Tên hàm sẽ thực thi khi hết giờ
        );
      B2: điều khiển Timer 
      Timer sẽ không chạy ngay khi vừa tạo xong, bạn phải ra lệnh cho nó:
          xTimerStart(xTimer, 0); : Bắt đầu chạy.
          xTimerStop(xTimer, 0); : Dừng chạy.
          xTimerReset(xTimer, 0); : Đếm lại từ đầu (thường dùng để gia hạn thời gian).
    h. Event Groups (Nhóm sự kiện).
      + Cơ chế hoạt động: Bitwise (Theo bit)
          Một Event Group là một tập hợp các Bit (thường là 24 bit trên ESP32/ARM).
          Mỗi bit đại diện cho một sự kiện (Event).
            Bit = 1: Sự kiện đã xảy ra.
            Bit = 0: Sự kiện chưa xảy ra.
      + tính năng
        Đợi nhiều sự kiện (Wait for multiple events)
          Một Task có thể bị chặn (Blocked) để đợi:
            Logic AND: Đợi tất cả các bit được set (Ví dụ: Đợi có WiFi VÀ có MQTT mới gửi dữ liệu).
            Logic OR: Đợi chỉ cần một trong các bit được set (Ví dụ: Đợi nhấn nút Dừng HOẶC đợi cảm biến báo quá nhiệt).
        Đồng bộ hóa điểm hẹn (Task Rendezvous / Synchronization)
          Giúp các Task đợi nhau tại một điểm. Ví dụ: Task A, Task B, Task C cùng làm việc,
          không ông nào được đi tiếp nếu các ông kia chưa làm xong phần của mình.
        Các hàm quan trọng
            xEventGroupCreate() -> 	Tạo một nhóm sự kiện mới.
            xEventGroupSetBits(Handle, Bits)	-> Đánh dấu sự kiện đã xảy ra (Set bit lên 1).
            xEventGroupWaitBits(...)	-> Bắt Task đứng đợi các bit sự kiện.
            xEventGroupClearBits(...)	-> Xóa trạng thái sự kiện (Set bit về 0).
            vd: 
              xEventGroupWaitBits(
                  xEvents, 
                  BIT_WIFI_READY | BIT_SENSOR_READY,
                  // giả sử  1 Task chỉ được phép chạy khi: WiFi sẵn sàng (Bit 0) và Cảm biến đã khởi động (Bit 1).
                  pdTRUE,        // Xóa bit sau khi nhận được (Clear on exit)
                  pdTRUE,        // Đợi TẤT CẢ các bit (Wait for all)
                  portMAX_DELAY  // Đợi mãi mãi
              );
    i. Memory
      xPortGetFreeHeapSize(): Xem còn bao nhiêu RAM trống trong Heap của RTOS.
      uxTaskGetStackHighWaterMark(TaskHandle): Xem Task đó còn cách "vực thẳm" 
                     tràn bộ nhớ bao nhiêu Byte (Càng gần về 0 càng nguy hiểm).

*/