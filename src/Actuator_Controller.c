// // /** Try all LED aliases; skip ones that don't exist */

// // #include "app_common.h"
// // #include "Accuator.h"

// // static const struct gpio_dt_spec led0 = GPIO_DT_SPEC_GET_OR(DT_ALIAS(led0), gpios, {0});
// // static const struct gpio_dt_spec led1 = GPIO_DT_SPEC_GET_OR(DT_ALIAS(led1), gpios, {0});
// // static const struct gpio_dt_spec led2 = GPIO_DT_SPEC_GET_OR(DT_ALIAS(led2), gpios, {0});
// // static const struct gpio_dt_spec led3 = GPIO_DT_SPEC_GET_OR(DT_ALIAS(led3), gpios, {0});


// // static void led_init_and_on(const struct gpio_dt_spec *led, const char *name)
// // {
// // 	if (!led->port) {
// // 		printk("%s alias not found\n", name);
// // 		return;
// // 	}
// // 	if (!device_is_ready(led->port)) {
// // 		printk("%s port not ready\n", name);
// // 		return;
// // 	}
// // 	int ret = gpio_pin_configure_dt(led, GPIO_OUTPUT_INACTIVE);
// // 	printk("%s configure ret=%d\n", name, ret);
// // 	gpio_pin_set_dt(led, 1); /* turn on */
// // }

// // static void led_off_safe(const struct gpio_dt_spec *led)
// // {
// // 	if (led->port) {
// // 		gpio_pin_set_dt(led, 0);
// // 	}
// // }

// // int main(void)
// // {
// // 	printk("=== ALL LED ON + SERVO TEST ===\n");

// // 	/* 1) Turn ON all LEDs we can find */

	

// // 	/* 2) Setup button (active-low typical on Nordic DKs) */
// // 	if (device_is_ready(button.port)) {
// // 		gpio_pin_configure_dt(&button, GPIO_INPUT | GPIO_PULL_UP);
// // 	} else {
// // 		printk("button port not ready\n");
// // 	}

// // 	/* 3) Setup servo */
// // 	if (!device_is_ready(servo.dev)) {
// // 		printk("servo dev not ready\n");
// // 	} else {
// // 		pwm_set_pulse_dt(&servo, min_pulse);
// // 	}

// // 	/* 4) Main loop: button controls servo; LEDs stay ON */
// // 	while (1) {
// // 		bool pressed = false;

// // 		if (device_is_ready(button.port)) {
// // 			pressed = (gpio_pin_get_dt(&button) == 0);
// // 		}

// // 		if (pressed) {
// // 			/* Move servo to mid (~90°) */
// // 			if (device_is_ready(servo.dev)) {
// // 				pwm_set_pulse_dt(&servo, (min_pulse + max_pulse) / 2);
// // 				gpio_pin_set_dt(&led1, 0);
// // 			}
// // 		} else {
// // 			/* Move servo back to min (~0°) */
// // 			if (device_is_ready(servo.dev)) {
// // 				pwm_set_pulse_dt(&servo, min_pulse);
// // 				led_init_and_on(&led1, "led1");
// // 			}
// // 		}

// // 		k_msleep(50);
// // 	}

// // 	/* not reached, but safe cleanup style */
// // 	led_off_safe(&led0);
// // 	led_off_safe(&led1);
// // 	led_off_safe(&led2);
// // 	led_off_safe(&led3);

// // 	return 0;
// // }
// #include <zephyr/kernel.h>
// #include <zephyr/drivers/gpio.h>
// #include <zephyr/drivers/pwm.h>
// #include <zephyr/logging/log.h>
// #include "app_common.h"

// LOG_MODULE_REGISTER(actuator, LOG_LEVEL_INF);

// /* ══════════════════════════════════════════════════════════════
//    HARDWARE DEFINITIONS - LOCAL TO THIS FILE ONLY
//    ══════════════════════════════════════════════════════════════ */

// /* LEDs */
// static const struct gpio_dt_spec led0 = GPIO_DT_SPEC_GET_OR(DT_ALIAS(led0), gpios, {0});
// static const struct gpio_dt_spec led1 = GPIO_DT_SPEC_GET_OR(DT_ALIAS(led1), gpios, {0});

// /* Button */
// #define BUTTON_NODE DT_ALIAS(sw0)
// #if DT_NODE_EXISTS(BUTTON_NODE)
// static const struct gpio_dt_spec button = GPIO_DT_SPEC_GET(BUTTON_NODE, gpios);
// #endif

// /* Servo PWM */
// #define SERVO_NODE DT_NODELABEL(servo)
// #if DT_NODE_EXISTS(SERVO_NODE)
// static const struct pwm_dt_spec servo = PWM_DT_SPEC_GET(SERVO_NODE);
// static const uint32_t min_pulse = PWM_USEC(700);
// static const uint32_t max_pulse = PWM_USEC(2500);
// #endif

// /* ══════════════════════════════════════════════════════════════
//    HELPER FUNCTIONS
//    ══════════════════════════════════════════════════════════════ */

// static void led_init(const struct gpio_dt_spec *led, const char *name)
// {
//     if (!led->port) {
//         LOG_WRN("%s alias not found", name);
//         return;
//     }
//     if (!device_is_ready(led->port)) {
//         LOG_ERR("%s port not ready", name);
//         return;
//     }
//     gpio_pin_configure_dt(led, GPIO_OUTPUT_INACTIVE);
//     LOG_INF("%s initialized", name);
// }

// /* ══════════════════════════════════════════════════════════════
//    ACTUATOR TASK
//    ══════════════════════════════════════════════════════════════ */

// void actuator_task(void *p1, void *p2, void *p3)
// {
//     LOG_INF("Actuator task started");

//     /* Initialize LEDs */
//     led_init(&led0, "led0");
//     led_init(&led1, "led1");

//     /* Initialize Button */
// #if DT_NODE_EXISTS(BUTTON_NODE)
//     if (device_is_ready(button.port)) {
//         gpio_pin_configure_dt(&button, GPIO_INPUT);
//         LOG_INF("Button initialized");
//     }
// #endif

//     /* Initialize Servo */
// #if DT_NODE_EXISTS(SERVO_NODE)
//     if (!pwm_is_ready_dt(&servo)) {
//         LOG_ERR("Servo PWM not ready");
//     } else {
//         pwm_set_pulse_dt(&servo, min_pulse);
//         LOG_INF("Servo initialized");
//     }
// #endif

//     ActuatorData_t local = {0};
//     ActuatorData_t prev = {0};

//     while (1) {
//         /* Get actuator commands with mutex */
//         k_mutex_lock(&actuator_mutex, K_FOREVER);
//         memcpy(&local, &shared_actuator_data, sizeof(local));
//         k_mutex_unlock(&actuator_mutex);

//         /* LED Control */
//         if (led0.port && local.ledON != prev.ledON) {
//             gpio_pin_set_dt(&led0, local.ledON ? 1 : 0);
//             LOG_INF("LED: %s", local.ledON ? "ON" : "OFF");
//         }

//         /* Servo/Motor Control */
// #if DT_NODE_EXISTS(SERVO_NODE)
//         if (pwm_is_ready_dt(&servo) && local.motorVibrate != prev.motorVibrate) {
//             if (local.motorVibrate) {
//                 pwm_set_pulse_dt(&servo, max_pulse);
//                 LOG_INF("SERVO: MAX");
//             } else {
//                 pwm_set_pulse_dt(&servo, min_pulse);
//                 LOG_INF("SERVO: MIN");
//             }
//         }
// #endif

//         /* Buzzer Control */
//         if (local.buzzerON != prev.buzzerON) {
//             LOG_INF("BUZZER: %s", local.buzzerON ? "ON" : "OFF");
//         }

//         memcpy(&prev, &local, sizeof(prev));
//         k_msleep(50);
//     }
// }
#include <zephyr/kernel.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/drivers/pwm.h>
#include <zephyr/logging/log.h>
#include "app_common.h"

LOG_MODULE_REGISTER(actuator, LOG_LEVEL_INF);

/* ══════════════════════════════════════════════════════════════
   HARDWARE DEFINITIONS - LOCAL TO THIS FILE ONLY
   ══════════════════════════════════════════════════════════════ */

/* LEDs */
static const struct gpio_dt_spec led0 = GPIO_DT_SPEC_GET_OR(DT_ALIAS(led0), gpios, {0});
static const struct gpio_dt_spec led1 = GPIO_DT_SPEC_GET_OR(DT_ALIAS(led1), gpios, {0});

/* Button */
#define BUTTON_NODE DT_ALIAS(sw0)
#if DT_NODE_EXISTS(BUTTON_NODE)
static const struct gpio_dt_spec button = GPIO_DT_SPEC_GET(BUTTON_NODE, gpios);
#endif

/* Servo PWM - with center position for ±90° */
#define SERVO_NODE DT_NODELABEL(servo)
#if DT_NODE_EXISTS(SERVO_NODE)
static const struct pwm_dt_spec servo = PWM_DT_SPEC_GET(SERVO_NODE);
static const uint32_t min_pulse = PWM_USEC(700);    // 0° position
static const uint32_t center_pulse = PWM_USEC(1500); // 90° center position
static const uint32_t max_pulse = PWM_USEC(2500);   // 180° position
#endif

/* ══════════════════════════════════════════════════════════════
   HELPER FUNCTIONS
   ══════════════════════════════════════════════════════════════ */

static void led_init(const struct gpio_dt_spec *led, const char *name)
{
    if (!led->port) {
        LOG_WRN("%s alias not found", name);
        return;
    }
    if (!device_is_ready(led->port)) {
        LOG_ERR("%s port not ready", name);
        return;
    }
    gpio_pin_configure_dt(led, GPIO_OUTPUT_INACTIVE);
    LOG_INF("%s initialized", name);
}

/* ══════════════════════════════════════════════════════════════
   ACTUATOR TASK
   ══════════════════════════════════════════════════════════════ */

void actuator_task(void *p1, void *p2, void *p3)
{
    LOG_INF("Actuator task started");

    /* Initialize LEDs */
    led_init(&led0, "led0");
    led_init(&led1, "led1");

    /* Initialize Button */
#if DT_NODE_EXISTS(BUTTON_NODE)
    if (device_is_ready(button.port)) {
        gpio_pin_configure_dt(&button, GPIO_INPUT);
        LOG_INF("Button initialized");
    }
#endif

    /* Initialize Servo */
#if DT_NODE_EXISTS(SERVO_NODE)
    if (!pwm_is_ready_dt(&servo)) {
        LOG_ERR("Servo PWM not ready");
    } else {
        pwm_set_pulse_dt(&servo, center_pulse);
        LOG_INF("Servo initialized at center (90°)");
    }
#endif

    ActuatorData_t local = {0};
    ActuatorData_t prev = {0};

    while (1) {
        /* Get actuator commands with mutex */
        k_mutex_lock(&actuator_mutex, K_FOREVER);
        memcpy(&local, &shared_actuator_data, sizeof(local));
        k_mutex_unlock(&actuator_mutex);

        /* LED Control */
        if (led0.port && local.ledON != prev.ledON) {
            gpio_pin_set_dt(&led0, local.ledON ? 1 : 0);
            LOG_INF("LED: %s", local.ledON ? "ON" : "OFF");
        }

        /* Servo/Motor Control - Toggle ±90° from center */
#if DT_NODE_EXISTS(SERVO_NODE)
        if (pwm_is_ready_dt(&servo) && local.motorVibrate != prev.motorVibrate) {
            if (local.motorVibrate) {
                pwm_set_pulse_dt(&servo, max_pulse);
                LOG_INF("SERVO: 180° (+90° from center)");
            } else {
                pwm_set_pulse_dt(&servo, min_pulse);
                LOG_INF("SERVO: 0° (-90° from center)");
            }
        }
#endif

        /* Buzzer Control */
        if (local.buzzerON != prev.buzzerON) {
            LOG_INF("BUZZER: %s", local.buzzerON ? "ON" : "OFF");
        }

        memcpy(&prev, &local, sizeof(prev));
        k_msleep(50);
    }
}