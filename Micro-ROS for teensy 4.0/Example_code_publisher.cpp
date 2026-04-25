// ============================================================
//  micro-ROS Teensy 4.0 — Auto Reconnect Template
//  อธิบายทุกบรรทัด พร้อมจุดที่ต้องแก้เมื่อเพิ่มฟีเจอร์
// ============================================================

// ── 1. LIBRARIES ─────────────────────────────────────────────
#include <Arduino.h>
#include <micro_ros_arduino.h>      // micro-ROS สำหรับ Arduino
#include <stdio.h>
#include <rcl/rcl.h>                // ROS Client Library หลัก
#include <rcl/error_handling.h>     // จัดการ error
#include <rclc/rclc.h>              // ROS Client Library C
#include <rclc/executor.h>          // executor สำหรับ spin/callback
#include <std_msgs/msg/int32.h>     // message type = Int32
// ⚙️ อยากเพิ่ม message type อื่น → เพิ่ม #include ตรงนี้
// เช่น #include <std_msgs/msg/float32.h>
// เช่น #include <geometry_msgs/msg/twist.h>


// ── 2. GLOBAL VARIABLES ──────────────────────────────────────
rcl_publisher_t publisher;          // ตัว publish message
// ⚙️ อยากเพิ่ม publisher → ประกาศ rcl_publisher_t เพิ่มตรงนี้

std_msgs__msg__Int32 msg;           // ตัวแปรเก็บ message ที่จะส่ง
// ⚙️ อยากเพิ่ม message → ประกาศ message type เพิ่มตรงนี้

rclc_executor_t executor;           // จัดการ callback ทั้งหมด
rclc_support_t support;             // เก็บ context ของ micro-ROS
rcl_allocator_t allocator;          // จัดการ memory
rcl_node_t node;                    // ROS node
rcl_timer_t timer;                  // timer สำหรับ publish ตาม interval
// ⚙️ อยากเพิ่ม subscriber → ประกาศ rcl_subscription_t ตรงนี้
// ⚙️ อยากเพิ่ม timer → ประกาศ rcl_timer_t เพิ่มตรงนี้

#define LED_PIN 13                  // LED บน Teensy 4.0


// ── 3. STATE MACHINE ─────────────────────────────────────────
typedef enum {
  WAITING_AGENT,       // รอ agent เปิดอยู่
  AGENT_AVAILABLE,     // agent พร้อม → กำลัง init
  AGENT_CONNECTED,     // connected → กำลัง publish ปกติ
  AGENT_DISCONNECTED   // agent หลุด → cleanup แล้วกลับรอใหม่
} State;

State state = WAITING_AGENT;        // เริ่มต้นที่ state รอก่อนเสมอ
// ⚙️ ไม่ต้องแก้ส่วนนี้


// ── 4. ERROR HANDLING MACROS ─────────────────────────────────
// ถ้า error → return false (ออกจาก create_entities)
#define RCCHECK(fn) { rcl_ret_t temp_rc = fn; if((temp_rc != RCL_RET_OK)){return false;}}
// ถ้า error → ไม่หยุด แค่ข้ามไป (ใช้กับ publish/spin)
#define RCSOFTCHECK(fn) { rcl_ret_t temp_rc = fn; if((temp_rc != RCL_RET_OK)){}}
// ⚙️ ไม่ต้องแก้ส่วนนี้


// ── 5. TIMER CALLBACK ────────────────────────────────────────
// ฟังก์ชันนี้จะถูกเรียกทุก timer_timeout ms (ค่าเริ่มต้น 1000ms)
void timer_callback(rcl_timer_t * timer, int64_t last_call_time) {
  RCLC_UNUSED(last_call_time);
  if (timer != NULL) {
    RCSOFTCHECK(rcl_publish(&publisher, &msg, NULL));  // publish msg
    msg.data++;   // เพิ่มค่าทุกครั้งที่ publish

    // ⚙️ อยากเพิ่ม publisher อื่น → เพิ่ม rcl_publish ตรงนี้
    // ⚙️ อยากเปลี่ยน logic → แก้ตรงนี้ เช่น:
    // msg.data = analogRead(A0);
    // RCSOFTCHECK(rcl_publish(&publisher, &msg, NULL));
  }
}

// ⚙️ อยากเพิ่ม subscriber callback → เพิ่ม function ใหม่ตรงนี้
// void subscription_callback(const void * msgin) {
//   const std_msgs__msg__Int32 * recv = (const std_msgs__msg__Int32 *)msgin;
//   // ทำอะไรกับ recv->data
// }


// ── 6. CREATE ENTITIES ───────────────────────────────────────
// เรียกตอน agent พร้อม → สร้าง node, publisher, timer ทั้งหมด
bool create_entities() {
  allocator = rcl_get_default_allocator();

  // Init micro-ROS context
  RCCHECK(rclc_support_init(&support, 0, NULL, &allocator));

  // สร้าง node
  // ⚙️ อยากเปลี่ยนชื่อ node → แก้ "micro_ros_arduino_node"
  RCCHECK(rclc_node_init_default(&node, "micro_ros_arduino_node", "", &support));

  // สร้าง publisher
  // ⚙️ อยากเปลี่ยนชื่อ topic → แก้ "micro_ros_arduino_node_publisher"
  // ⚙️ อยากเพิ่ม publisher → copy block นี้แล้วเปลี่ยนตัวแปรและชื่อ topic
  RCCHECK(rclc_publisher_init_default(
    &publisher,
    &node,
    ROSIDL_GET_MSG_TYPE_SUPPORT(std_msgs, msg, Int32),
    "micro_ros_arduino_node_publisher"));

  // ตั้ง timer interval (หน่วย ms)
  // ⚙️ อยากเปลี่ยนความถี่ publish → แก้ตัวเลข 1000
  const unsigned int timer_timeout = 1000;
  RCCHECK(rclc_timer_init_default(
    &timer,
    &support,
    RCL_MS_TO_NS(timer_timeout),
    timer_callback));

  // Init executor — ตัวเลข 1 = จำนวน handle ทั้งหมด (timer + subscriber)
  // ⚙️ อยากเพิ่ม subscriber → เปลี่ยนเลข 1 → 2, 3, ...
  RCCHECK(rclc_executor_init(&executor, &support.context, 1, &allocator));
  RCCHECK(rclc_executor_add_timer(&executor, &timer));

  // ⚙️ อยากเพิ่ม subscriber → เพิ่มตรงนี้:
  // RCCHECK(rclc_executor_add_subscription(
  //   &executor, &subscriber, &sub_msg, &subscription_callback, ON_NEW_DATA));

  msg.data = 0;   // ค่าเริ่มต้นของ message
  // ⚙️ อยากตั้งค่าเริ่มต้น message อื่น → แก้ตรงนี้
  return true;
}


// ── 7. DESTROY ENTITIES ──────────────────────────────────────
// เรียกตอน agent หลุด → ลบทุกอย่างเพื่อเตรียม reconnect
void destroy_entities() {
  rmw_context_t * rmw_context = rcl_context_get_rmw_context(&support.context);
  (void) rmw_uros_set_context_entity_destroy_session_timeout(rmw_context, 0);

  // ลบในลำดับย้อนกลับจาก create_entities
  (void) rcl_publisher_fini(&publisher, &node);
  (void) rcl_timer_fini(&timer);
  rclc_executor_fini(&executor);
  (void) rcl_node_fini(&node);
  rclc_support_fini(&support);

  // ⚙️ ถ้าเพิ่ม publisher/subscriber ใน create_entities
  //    → ต้องเพิ่ม fini ของมันตรงนี้ด้วย เช่น:
  // (void) rcl_subscription_fini(&subscriber, &node);
}


// ── 8. SETUP ─────────────────────────────────────────────────
void setup() {
  Serial.begin(115200);
  set_microros_transports();    // เชื่อมกับ agent ผ่าน USB serial
  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, LOW);   // LED ดับตอนเริ่ม (ยังไม่ connected)

  // ⚙️ Init hardware อื่นๆ เพิ่มตรงนี้ เช่น:
  // pinMode(MOTOR_PIN, OUTPUT);
  // Wire.begin();   // I2C สำหรับ IMU
}


// ── 9. LOOP (STATE MACHINE) ──────────────────────────────────
void loop() {
  switch (state) {

    case WAITING_AGENT:
      // ping agent ทุก 500ms จนกว่าจะเจอ → ไม่ต้องรีเซตบอร์ด
      if (rmw_uros_ping_agent(500, 1) == RMW_RET_OK) {
        state = AGENT_AVAILABLE;
      }
      break;

    case AGENT_AVAILABLE:
      // สร้าง node/publisher/timer → ถ้าสำเร็จ LED ติด + ไป CONNECTED
      if (create_entities()) {
        digitalWrite(LED_PIN, HIGH);   // LED ติด = connected
        state = AGENT_CONNECTED;
      } else {
        destroy_entities();
        state = WAITING_AGENT;
      }
      break;

    case AGENT_CONNECTED:
      // ทำงานปกติ → spin executor ทุก loop
      // ถ้า ping ไม่ได้ 3 ครั้ง → ถือว่าหลุด
      if (rmw_uros_ping_agent(100, 3) == RMW_RET_OK) {
        RCSOFTCHECK(rclc_executor_spin_some(&executor, RCL_MS_TO_NS(100)));
        // ⚙️ อยากทำอะไรเพิ่มขณะ connected → เพิ่มตรงนี้ เช่น:
        // อ่าน encoder, ควบคุม motor ฯลฯ
      } else {
        state = AGENT_DISCONNECTED;
      }
      break;

    case AGENT_DISCONNECTED:
      // cleanup แล้วกลับไปรอ agent ใหม่
      destroy_entities();
      digitalWrite(LED_PIN, LOW);    // LED ดับ = disconnected
      state = WAITING_AGENT;
      break;

    default:
      break;
  }
}


// ============================================================
//  สรุป: อยากเพิ่มอะไร แก้ตรงไหน
// ─────────────────────────────────────────────────────────────
//  เปลี่ยนชื่อ node/topic   → create_entities()
//  เปลี่ยนความถี่ publish   → timer_timeout ใน create_entities()
//  เพิ่ม message type       → #include บนสุด + global var
//  เพิ่ม publisher          → global var + create + destroy + timer_callback
//  เพิ่ม subscriber         → global var + callback fn + create + destroy
//  Init hardware            → setup()
//  Logic ขณะ connected      → AGENT_CONNECTED ใน loop()
// ============================================================
