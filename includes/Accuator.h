#ifndef ACCUATOR_H
#define ACCUATOR_H

#include <zephyr/drivers/pwm.h>
#include <zephyr/drivers/gpio.h>

/* Servo PWM - defined here, used only by actuator code */
static const struct pwm_dt_spec servo = PWM_DT_SPEC_GET(DT_NODELABEL(servo));
static const uint32_t min_pulse = PWM_USEC(700);
static const uint32_t max_pulse = PWM_USEC(2500);

#endif /* ACCUATOR_H */