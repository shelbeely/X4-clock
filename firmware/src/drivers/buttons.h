#pragma once
/*
 * buttons.h — Hardware button driver for Xteink X4
 *
 * Polls the two ADC ladders (GPIO1, GPIO2) and the digital power button
 * (GPIO3) every BTN_POLL_MS milliseconds from a dedicated FreeRTOS task,
 * then posts ButtonEvent codes to a shared queue.
 *
 * Consumers call buttons_dequeue() to retrieve the next pending event.
 */

#include <Arduino.h>
#include "bsp/x4_pins.h"

// Maximum events held in the queue before oldest are dropped
#define BTN_QUEUE_DEPTH 16

// Polling interval (ms) for the background FreeRTOS task
#define BTN_POLL_MS 20

void buttons_init();

// Non-blocking: returns BTN_NONE if the queue is empty
ButtonEvent buttons_dequeue();

// True if queue is non-empty
bool buttons_available();
