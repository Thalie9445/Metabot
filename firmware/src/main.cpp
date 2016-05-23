#include <stdlib.h>
#include <wirish/wirish.h>
#include <servos.h>
#include <terminal.h>
#include <main.h>
#include <math.h>
#include <dxl.h>
#include <function.h>
#include <commands.h>
#include <rc.h>
#include <rhock.h>
#include "voltage.h"
#include "buzzer.h"
#include "distance.h"
#include "config.h"
#include "motion.h"
#include "leds.h"
#include "mapping.h"
#include "imu.h"
#include "bt.h"

bool isUSB = false;

#define LIT     22

// Time
TERMINAL_PARAMETER_FLOAT(t, "Time", 0.0);

TERMINAL_COMMAND(version, "Getting firmware version")
{
    terminal_io()->print("version=");
    terminal_io()->println(METABOT_VERSION);
}

TERMINAL_COMMAND(started, "Is the robot started?")
{
    terminal_io()->print("started=");
    terminal_io()->println(started);
}

TERMINAL_COMMAND(rc, "Go to RC mode")
{
    RC.begin(921600);
    terminal_init(&RC);
    isUSB = false;
}

// Enabling/disabling move
TERMINAL_PARAMETER_BOOL(move, "Enable/Disable move", true);


TERMINAL_COMMAND(suicide, "Lit the fuse")
{
    digitalWrite(LIT, HIGH);
}

// Setting the flag, called @50hz
bool flag = false;
void setFlag()
{
    flag = true;
}

/**
 * Initializing
 */
void setup()
{
    // Initializing terminal on the RC port
    RC.begin(921600);
    terminal_init(&RC);

    // Lit pin is output low
    digitalWrite(LIT, LOW);
    pinMode(LIT, OUTPUT);

    // Initializing bluetooth module
    bt_init();

    // Initializing
    motion_init();

    // Initializing voltage measurement
    voltage_init();

    // Initializing the DXL bus
    delay(500);
    dxl_init();
    dxl_pidp(24);

    // Initializing config (see config.h)
    config_init();

    // initializing distance
    distance_init();

    // Initializing the IMU
    imu_init();

    // Initializing positions to 0
    for (int i=0; i<12; i++) {
        dxl_set_position(i+1, 0.0);
    }
    for (int i=0; i<4; i++) {
        l1[i] = l2[i] = l3[i] = 0;
    }

    // Configuring board LED as output
    pinMode(BOARD_LED_PIN, OUTPUT);
    digitalWrite(BOARD_LED_PIN, LOW);

    // Initializing the buzzer, and playing the start-up melody
    buzzer_init();
    buzzer_play(MELODY_BOOT);

    // Enable 50hz ticking
    servos_init();
    servos_attach_interrupt(setFlag);
    
    RC.begin(921600);
}

/**
 * Computing the servo values
 */
void tick()
{
    if (!move || !started) {
        t = 0.0;
        return;
    }

    // Incrementing and normalizing t
    t += motion_get_f()*0.02;
    if (t > 1.0) {
        t -= 1.0;
        colorize();
    }
    if (t < 0.0) t += 1.0;

    if (voltage_error()) {
        dxl_disable_all();
        if (t < 0.5) {
            led_set_all(LED_R|LED_G);
        } else {
            led_set_all(0);
        }
    } else {
        motion_tick(t);
    }

    // Sending order to servos
    dxl_set_position(mapping[0], l1[0]);
    dxl_set_position(mapping[3], l1[1]);
    dxl_set_position(mapping[6], l1[2]);
    dxl_set_position(mapping[9], l1[3]);

    dxl_set_position(mapping[1], l2[0]);
    dxl_set_position(mapping[4], l2[1]);
    dxl_set_position(mapping[7], l2[2]);
    dxl_set_position(mapping[10], l2[3]);

    dxl_set_position(mapping[2], l3[0]);
    dxl_set_position(mapping[5], l3[1]);
    dxl_set_position(mapping[8], l3[2]);
    dxl_set_position(mapping[11], l3[3]);
}

void loop()
{
    // Buzzer update
    buzzer_tick();
    // IMU ticking
    imu_tick();
    // Sampling the voltage
    voltage_tick();

    // Updating the terminal
    terminal_tick();
#if defined(RHOCK)
    rhock_tick();
#endif
    if (SerialUSB.available() && !isUSB) {
        isUSB = true;
        terminal_init(&SerialUSB);
    }
    if (!SerialUSB.getDTR() && isUSB) {
        isUSB = false;
        terminal_init(&RC);
    }

    // Calling user motion tick
    if (flag) {
        flag = false;
        tick();
        dxl_flush();
    }
}
