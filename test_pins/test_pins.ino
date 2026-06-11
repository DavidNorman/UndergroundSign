/*
  Simple HUB75 pin exerciser for LOLIN S2 Mini / ESP32-S2

  The order matches the LEDs on a test board.  They go down the left
  side of the HUB75 connector, and then the right side, missing the
  grounds.

*/

#include <Arduino.h>

/* GPIO pins in ESP32 format */
#define PIN_R1     GPIO_NUM_1
#define PIN_G1     GPIO_NUM_3
#define PIN_B1     GPIO_NUM_2
#define PIN_R2     GPIO_NUM_4
#define PIN_G2     GPIO_NUM_5
#define PIN_B2     GPIO_NUM_6
#define PIN_A      GPIO_NUM_8
#define PIN_B      GPIO_NUM_7
#define PIN_C      GPIO_NUM_10
#define PIN_D      GPIO_NUM_9
#define PIN_CLK    GPIO_NUM_13
#define PIN_LAT    GPIO_NUM_11
#define PIN_OE     GPIO_NUM_14

/* Simple helper to set all outputs low */
void all_low() {
  gpio_set_level(PIN_R1, 0);
  gpio_set_level(PIN_G1, 0);
  gpio_set_level(PIN_B1, 0);
  gpio_set_level(PIN_R2, 0);
  gpio_set_level(PIN_G2, 0);
  gpio_set_level(PIN_B2, 0);
  gpio_set_level(PIN_A, 0);
  gpio_set_level(PIN_B, 0);
  gpio_set_level(PIN_C, 0);
  gpio_set_level(PIN_D, 0);
  gpio_set_level(PIN_CLK, 0);
  gpio_set_level(PIN_LAT, 0);
  gpio_set_level(PIN_OE, 0);
}

void setup() {
  delay(500);

  gpio_config_t io_conf = {
    .pin_bit_mask =
        (1ULL << PIN_R1) |
        (1ULL << PIN_G1) |
        (1ULL << PIN_B1) |
        (1ULL << PIN_R2) |
        (1ULL << PIN_G2) |
        (1ULL << PIN_B2) |
        (1ULL << PIN_A)  |
        (1ULL << PIN_B)  |
        (1ULL << PIN_C)  |
        (1ULL << PIN_D)  |
        (1ULL << PIN_CLK)|
        (1ULL << PIN_LAT)|
        (1ULL << PIN_OE),
    .mode = GPIO_MODE_OUTPUT,
    .pull_up_en = GPIO_PULLUP_DISABLE,
    .pull_down_en = GPIO_PULLDOWN_DISABLE,
    .intr_type = GPIO_INTR_DISABLE
  };

  gpio_config(&io_conf);

  all_low();
}

void pulse_pin(gpio_num_t pin, const char* name) {
  all_low();
  gpio_set_level(pin, 1);
  delay(500);
}

void loop() {
  pulse_pin(PIN_R1, "R1");
  pulse_pin(PIN_B1, "B1");
  pulse_pin(PIN_R2, "R2");
  pulse_pin(PIN_B2, "B2");
  pulse_pin(PIN_A,  "A");
  pulse_pin(PIN_C,  "C");
  pulse_pin(PIN_CLK,"CLK");
  pulse_pin(PIN_OE, "OE");
  pulse_pin(PIN_G1, "G1");
  pulse_pin(PIN_G2, "G2");
  pulse_pin(PIN_B,  "B");
  pulse_pin(PIN_D,  "D");
  pulse_pin(PIN_LAT,"LAT");
}

