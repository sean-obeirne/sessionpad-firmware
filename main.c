/*
 * SessionPad Firmware
 *
 * Dumb input device firmware for Raspberry Pi Pico.
 * Scans momentary switches, debounces, and emits serial events over USB.
 * All application logic lives on the host side.
 *
 * Protocol (line-oriented, human-readable):
 *   Boot:     READY
 *   Events:   PRESS <name>    / RELEASE <name>
 *   Commands: PING -> PONG   / GET_STATE -> STATE <bits>
 *   State:    13-char string of '0'/'1', BTN_1..BTN_12 then APPLY (1=pressed)
 */

#include <stdio.h>
#include <string.h>
#include "pico/stdlib.h"

/* -----------------------------------------------------------------------
 * GPIO pin mapping — change these to remap buttons
 * ----------------------------------------------------------------------- */

#define NUM_BUTTONS 13

typedef struct
{
    uint gpio;
    const char *name;
} button_def_t;

static const button_def_t BUTTON_DEFS[NUM_BUTTONS] = {
    {0, "BTN_1"},
    {1, "BTN_2"},
    {2, "BTN_3"},
    {3, "BTN_4"},
    {4, "BTN_5"},
    {5, "BTN_6"},
    {10, "BTN_7"},
    {11, "BTN_8"},
    {12, "BTN_9"},
    {13, "BTN_10"},
    {14, "BTN_11"},
    {15, "BTN_12"},
    {16, "APPLY"},
};

/* -----------------------------------------------------------------------
 * Debounce parameters
 * ----------------------------------------------------------------------- */

/* How many consecutive stable reads (at 1ms intervals) before accepting */
#define DEBOUNCE_COUNT 5

/* -----------------------------------------------------------------------
 * Per-button runtime state
 * ----------------------------------------------------------------------- */

typedef struct
{
    bool stable_state; /* current debounced state: true = pressed */
    bool raw_last;     /* last raw reading */
    uint8_t counter;   /* consecutive stable readings */
} button_state_t;

static button_state_t buttons[NUM_BUTTONS];

/* -----------------------------------------------------------------------
 * GPIO initialization
 * ----------------------------------------------------------------------- */

static void init_buttons(void)
{
    for (int i = 0; i < NUM_BUTTONS; i++)
    {
        uint pin = BUTTON_DEFS[i].gpio;
        gpio_init(pin);
        gpio_set_dir(pin, GPIO_IN);
        gpio_pull_up(pin);

        /* Initial state: unpressed */
        buttons[i].stable_state = false;
        buttons[i].raw_last = false;
        buttons[i].counter = 0;
    }
}

/* -----------------------------------------------------------------------
 * Button scanning with debounce
 *
 * Called once per millisecond from the main loop.
 * Reads each GPIO, tracks consecutive stable readings, and emits
 * PRESS / RELEASE events when the debounced state changes.
 *
 * Switches are active-low (pulled up, grounded when pressed).
 * ----------------------------------------------------------------------- */

static void scan_buttons(void)
{
    for (int i = 0; i < NUM_BUTTONS; i++)
    {
        /* Active-low: LOW = pressed */
        bool pressed = !gpio_get(BUTTON_DEFS[i].gpio);

        if (pressed == buttons[i].raw_last)
        {
            /* Same as previous reading — count up toward threshold */
            if (buttons[i].counter < DEBOUNCE_COUNT)
            {
                buttons[i].counter++;
            }
        }
        else
        {
            /* Reading changed — reset counter */
            buttons[i].raw_last = pressed;
            buttons[i].counter = 0;
        }

        /* Accept new state when stable long enough */
        if (buttons[i].counter == DEBOUNCE_COUNT &&
            pressed != buttons[i].stable_state)
        {
            buttons[i].stable_state = pressed;
            printf("%s %s\n", pressed ? "PRESS" : "RELEASE",
                   BUTTON_DEFS[i].name);
        }
    }
}

/* -----------------------------------------------------------------------
 * State reporting
 *
 * Prints: STATE <13 chars>
 * Each char is '1' (pressed) or '0' (not pressed), in button order.
 * ----------------------------------------------------------------------- */

static void send_state(void)
{
    printf("STATE ");
    for (int i = 0; i < NUM_BUTTONS; i++)
    {
        putchar(buttons[i].stable_state ? '1' : '0');
    }
    putchar('\n');
}

/* -----------------------------------------------------------------------
 * Command processing
 *
 * Reads available serial input one char at a time, assembles lines,
 * and dispatches recognized commands.
 * ----------------------------------------------------------------------- */

#define CMD_BUF_SIZE 32

static char cmd_buf[CMD_BUF_SIZE];
static int cmd_len = 0;

static void process_commands(void)
{
    int c;
    while ((c = getchar_timeout_us(0)) != PICO_ERROR_TIMEOUT)
    {
        if (c == '\n' || c == '\r')
        {
            if (cmd_len > 0)
            {
                cmd_buf[cmd_len] = '\0';

                if (strcmp(cmd_buf, "PING") == 0)
                {
                    printf("PONG\n");
                }
                else if (strcmp(cmd_buf, "GET_STATE") == 0)
                {
                    send_state();
                }
                /* Unknown commands are silently ignored */

                cmd_len = 0;
            }
        }
        else
        {
            if (cmd_len < CMD_BUF_SIZE - 1)
            {
                cmd_buf[cmd_len++] = (char)c;
            }
            else
            {
                /* Buffer overflow — discard line */
                cmd_len = 0;
            }
        }
    }
}

/* -----------------------------------------------------------------------
 * Main
 * ----------------------------------------------------------------------- */

int main(void)
{
    stdio_init_all();
    init_buttons();

    /* Wait briefly for USB enumeration, then announce */
    sleep_ms(1000);
    printf("READY\n");

    while (true)
    {
        scan_buttons();
        process_commands();
        sleep_ms(1); /* 1ms scan interval */
    }

    return 0; /* never reached */
}
