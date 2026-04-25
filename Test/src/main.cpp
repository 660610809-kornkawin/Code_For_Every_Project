// ============================================================
//  micro-ROS Teensy 4.0 — Subscriber cmd_vel (Teleop)
//  รับ geometry_msgs/Twist จาก teleop_twist_keyboard
//  พร้อม Auto Reconnect
// ============================================================

// ── 1. LIBRARIES ─────────────────────────────────────────────
#include <Arduino.h>
#include <micro_ros_arduino.h>
#include <stdio.h>
#include <rcl/rcl.h>
#include <rcl/error_handling.h>
#include <rclc/rclc.h>
#include <rclc/executor.h>
#include <geometry_msgs/msg/twist.h>    // cmd_vel ใช้ Twist message
// ⚙️ อยากเพิ่ม message type อื่น → เพิ่ม #include ตรงนี้


// ── 2. GLOBAL VARIABLES ──────────────────────────────────────
rcl_subscription_t subscriber;          // ตัว subscribe cmd_vel
geometry_msgs__msg__Twist twist_msg;    // เก็บค่า linear.x และ angular.z
// ⚙️ อยากเพิ่ม publisher (เช่น odom) → ประกาศ rcl_publisher_t ตรงนี้

rclc_executor_t executor;
rclc_support_t support;
rcl_allocator_t allocator;
rcl_node_t node;

#define LED_PIN 13

// ── ตัวแปรเก็บค่า velocity ล่าสุด (ใช้ใน loop) ──────────────
volatile float linear_vel  = 0.0f;     // หน่วย m/s  (จาก twist_msg.linear.x)
volatile float angular_vel = 0.0f;     // หน่วย rad/s (จาก twist_msg.angular.z)
// ⚙️ ถ้าต้องการ linear.y หรือ angular.x/y → เพิ่มตัวแปรตรงนี้


// ── 3. STATE MACHINE ─────────────────────────────────────────
typedef enum {
  WAITING_AGENT,
  AGENT_AVAILABLE,
  AGENT_CONNECTED,
  AGENT_DISCONNECTED
} State;

State state = WAITING_AGENT;


// ── 4. ERROR HANDLING MACROS ─────────────────────────────────
#define RCCHECK(fn)    { rcl_ret_t temp_rc = fn; if((temp_rc != RCL_RET_OK)){return false;}}
#define RCSOFTCHECK(fn){ rcl_ret_t temp_rc = fn; if((temp_rc != RCL_RET_OK)){}}


// ── 5. SUBSCRIPTION CALLBACK ─────────────────────────────────
// ถูกเรียกทุกครั้งที่ได้รับ message จาก /cmd_vel
void cmd_vel_callback(const void * msgin) {
  const geometry_msgs__msg__Twist * msg =
      (const geometry_msgs__msg__Twist *)msgin;

  // ดึงค่า velocity จาก message
  linear_vel  = msg->linear.x;     // +ไปข้างหน้า / -ถอยหลัง
  angular_vel = msg->angular.z;    // +หมุนซ้าย / -หมุนขวา

  // ── ควบคุม motor ตรงนี้ ──────────────────────────────────
  // ⚙️ เพิ่ม logic ขับ motor ได้เลย เช่น:
  // float left_speed  = linear_vel - angular_vel;
  // float right_speed = linear_vel + angular_vel;
  // motor_left.setSpeed(left_speed);
  // motor_right.setSpeed(right_speed);

  // Debug ผ่าน Serial
  Serial.print("linear.x: ");  Serial.print(linear_vel);
  Serial.print(" | angular.z: "); Serial.println(angular_vel);
}


// ── 6. CREATE ENTITIES ───────────────────────────────────────
bool create_entities() {
  allocator = rcl_get_default_allocator();

  RCCHECK(rclc_support_init(&support, 0, NULL, &allocator));

  // ⚙️ เปลี่ยนชื่อ node ได้ตรงนี้
  RCCHECK(rclc_node_init_default(&node, "teensy_cmd_vel_node", "", &support));

  // สร้าง subscriber บน topic /cmd_vel
  // ⚙️ เปลี่ยนชื่อ topic ได้ตรงนี้
  RCCHECK(rclc_subscription_init_default(
    &subscriber,
    &node,
    ROSIDL_GET_MSG_TYPE_SUPPORT(geometry_msgs, msg, Twist),
    "cmd_vel"));

  // executor — ตัวเลข 1 = จำนวน handle (subscriber 1 ตัว)
  // ⚙️ เพิ่ม publisher/subscriber → เปลี่ยนเลข 1 → 2, 3, ...
  RCCHECK(rclc_executor_init(&executor, &support.context, 1, &allocator));
  RCCHECK(rclc_executor_add_subscription(
    &executor,
    &subscriber,
    &twist_msg,
    &cmd_vel_callback,
    ON_NEW_DATA));

  // ⚙️ เพิ่ม publisher → เพิ่ม rclc_publisher_init_default + executor_add ตรงนี้

  return true;
}


// ── 7. DESTROY ENTITIES ──────────────────────────────────────
void destroy_entities() {
  rmw_context_t * rmw_context = rcl_context_get_rmw_context(&support.context);
  (void) rmw_uros_set_context_entity_destroy_session_timeout(rmw_context, 0);

  (void) rcl_subscription_fini(&subscriber, &node);
  rclc_executor_fini(&executor);
  (void) rcl_node_fini(&node);
  rclc_support_fini(&support);

  // ⚙️ ถ้าเพิ่ม publisher → เพิ่ม rcl_publisher_fini ตรงนี้ด้วย
}


// ── 8. SETUP ─────────────────────────────────────────────────
void setup() {
  Serial.begin(115200);
  set_microros_transports();
  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, LOW);

  // ⚙️ Init hardware เพิ่มตรงนี้ เช่น:
  // motor_left.begin();
  // motor_right.begin();
  // encoder_left.begin();
}


// ── 9. LOOP (STATE MACHINE) ──────────────────────────────────
void loop() {
  switch (state) {

    case WAITING_AGENT:
      if (rmw_uros_ping_agent(500, 1) == RMW_RET_OK) {
        state = AGENT_AVAILABLE;
      }
      break;

    case AGENT_AVAILABLE:
      if (create_entities()) {
        digitalWrite(LED_PIN, HIGH);    // LED ติด = connected
        state = AGENT_CONNECTED;
      } else {
        destroy_entities();
        state = WAITING_AGENT;
      }
      break;

    case AGENT_CONNECTED:
      if (rmw_uros_ping_agent(100, 3) == RMW_RET_OK) {
        // spin executor → รอรับ cmd_vel callback
        RCSOFTCHECK(rclc_executor_spin_some(&executor, RCL_MS_TO_NS(100)));

        // ⚙️ อยากทำอะไรเพิ่มขณะ connected → เพิ่มตรงนี้ เช่น:
        // อ่าน encoder → คำนวณ odometry → publish /odom

      } else {
        state = AGENT_DISCONNECTED;
      }
      break;

    case AGENT_DISCONNECTED:
      // หยุด motor ทันทีเมื่อหลุด connection
      linear_vel  = 0.0f;
      angular_vel = 0.0f;
      // ⚙️ เพิ่ม motor stop ตรงนี้ เช่น:
      // motor_left.setSpeed(0);
      // motor_right.setSpeed(0);

      destroy_entities();
      digitalWrite(LED_PIN, LOW);
      state = WAITING_AGENT;
      break;

    default:
      break;
  }
}