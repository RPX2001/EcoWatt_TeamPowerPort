/**
 * @file main.c
 * @brief Main application file for the ECOWATT project.
 * 
 * @author Ravin, Yasith
 * @version 1.1
 * @date 2025-10-01
 * 
 * @par Revision history
 * - 1.0 (2025-09-09, Ravin): Original file
 * - 1.1 (2025-10-01, Yasith): Added detailed documentation, made portable.
 */

#include <stdio.h>

#include "acquisition.h"
#include "timers_util.h"
#include "print.h"

const int POLL_INTERVAL_US = 2000000; // 2 seconds
const RegID selection[] = {REG_VAC1, REG_IAC1, REG_IPV1, REG_PAC};

void poll_timer_callback(void* param);

void app_main(void)
{   
    printf("Started ECOWATT\n");

    // Set power to 500W
    bool ok = set_power(500);  
    if (ok) {
        printf("Power set successfully.\n");
    } else {
        printf("Failed to set power.\n");
    }

    create_periodic_timer("poll_timer", poll_timer_callback, POLL_INTERVAL_US);
}

/**
 * @fn void poll_timer_callback(void* param)
 * @brief Timer callback to poll inverter data periodically.
 * 
 * @param param Unused parameter.
 */
void poll_timer_callback(void* param)
{
    DecodedValues values = read_request(selection, sizeof(selection) / sizeof(selection[0]));
    printf("Vac1: %u, Iac1: %u, Ipv1: %u, Pac: %u\n", values.values[0], values.values[1], values.values[2], values.values[3]);
}