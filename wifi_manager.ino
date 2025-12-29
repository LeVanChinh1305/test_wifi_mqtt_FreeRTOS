#include <WiFi.h>
#include <Arduino.h>
#include <PubSubClient.h>

// setup wifi 
#define wifi_ssid "509 TQV"
#define wifi_pass "83838383"
#define wifi_limit 5000
static unsigned long wifi_last = 0; 

// setup mqtt 
const char* mqtt_server = "192.168.83.105"; 
const int mqtt_port = 1883;
const char* topic_control_led = "control/led";
const char* topic_publish_led = "publish/led";

// setup led 
#define led 10
int led_state = 0; 

// setup button 
#define button 4
int button_state_last = HIGH;
unsigned long last_time_debounce = 0;
const unsigned long debounce_delay = 50; 

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

*/