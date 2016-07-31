/*
 * The Driver Station Library (LibDS)
 * Copyright (C) 2015-2016 Alex Spataru <alex_spataru@outlook>
 *
 * Permission is hereby granted, free of charge, to any person obtaining
 * a copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included
 * in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 */

#include "DS_Config.h"
#include "DS_Client.h"
#include "DS_Events.h"
#include "DS_Timer.h"
#include "DS_Protocol.h"

#include <stdlib.h>

/*
 * Define the sender watchdogs (when one expires, we send a packet)
 */
static DS_Timer fms_send_timer;
static DS_Timer radio_send_timer;
static DS_Timer robot_send_timer;

/*
 * Define the receiver watchdogs.
 * When one of the watchdog expires, the communications with the exired target
 * will be reset and the configuration will be updated to a safe state (e.g.
 * disabling the robot when the robot watchdog expires).
 */
static DS_Timer fms_recv_timer;
static DS_Timer radio_recv_timer;
static DS_Timer robot_recv_timer;

/*
 * If set to \c 1, then the LibDS will send and receive packets
 */
static int running = 0;

/*
 * Protocol read success booleans (used to feed the watchdogs)
 */
static int fms_read = 0;
static int radio_read = 0;
static int robot_read = 0;

/*
 * Holds a pointer to the current protocol, we use it to know if the
 * current protocol has changed (and update the configuration as needed).
 */
static DS_Protocol* protocol;

/**
 * Sends a new packet to the FMS
 */
static void send_fms_data()
{
    uint8_t* data = protocol->create_fms_packet();
    DS_SocketSend (&protocol->fms_socket, (char*) data);
    free (data);
}

/**
 * Sends a new packet to the radio
 */
static void send_radio_data()
{
    uint8_t* data = protocol->create_radio_packet();
    DS_SocketSend (&protocol->radio_socket, (char*) data);
    free (data);
}

/**
 * Sends a new packet to the robot
 */
static void send_robot_data()
{
    uint8_t* data = protocol->create_robot_packet();
    DS_SocketSend (&protocol->robot_socket, (char*) data);
    free (data);
}

/**
 * Sends a message to the NetConsole (if any message)
 */
static void send_netconsole_data()
{
    char* data = CFG_GetNetConsoleData();
    DS_SocketSend (&protocol->netconsole_socket, data);
    free (data);
}

/**
 * Sends data over the network using the functions of the current protocol.
 * If there is no protocol running, then this function will do nothing.
 */
static void send_data()
{
    /* Check if protocol is valid */
    if (!protocol)
        return;

    /* Update sender timers */
    DS_TimerUpdate (&fms_send_timer);
    DS_TimerUpdate (&radio_send_timer);
    DS_TimerUpdate (&robot_send_timer);

    /* Send FMS packet */
    if (fms_send_timer.expired) {
        send_fms_data();
        DS_TimerReset (&fms_send_timer);
    }

    /* Send radio packet */
    if (radio_send_timer.expired) {
        send_radio_data();
        DS_TimerReset (&radio_send_timer);
    }

    /* Send robot packet */
    if (robot_send_timer.expired) {
        send_robot_data();
        DS_TimerReset (&robot_send_timer);
    }

    /* Send NetConsole data */
    send_netconsole_data();
}

/**
 * Reads the received data using the functions provided by the current protocol.
 * If there is no protocol running, then this function will do nothing.
 */
static void recv_data()
{
    /* Check if protocol is valid */
    if (!protocol)
        return;

    /* Create the data pointers */
    char* fms_data = (char*) malloc (sizeof (char));
    char* radio_data = (char*) malloc (sizeof (char));
    char* robot_data = (char*) malloc (sizeof (char));

    /* Read data from sockets */
    DS_SocketRead (&protocol->fms_socket, fms_data);
    DS_SocketRead (&protocol->radio_socket, radio_data);
    DS_SocketRead (&protocol->robot_socket, robot_data);

    /* Let the protocol interpret received data */
    fms_read = protocol->read_fms_packet (fms_data);
    radio_read = protocol->read_radio_packet (radio_data);
    robot_read = protocol->read_robot_packet (robot_data);

    /* Free the data pointers */
    free (fms_data);
    free (radio_data);
    free (robot_data);
}

/**
 * Feeds the watchdogs, updates them and checks if any of them has expired
 */
static void update_watchdogs()
{
    /* Feed the watchdogs if applicable */
    if (fms_read)   DS_TimerReset (&fms_recv_timer);
    if (radio_read) DS_TimerReset (&radio_recv_timer);
    if (robot_read) DS_TimerReset (&robot_recv_timer);

    /* Update watchdog timings */
    DS_TimerUpdate (&fms_recv_timer);
    DS_TimerUpdate (&radio_recv_timer);
    DS_TimerUpdate (&robot_recv_timer);

    /* Check if any of the watchdogs expired */
    if (fms_recv_timer.expired)   CFG_FMSWatchdogExpired();
    if (radio_recv_timer.expired) CFG_RadioWatchdogExpired();
    if (robot_recv_timer.expired) CFG_RobotWatchdogExpired();

    /* Clear the read success values */
    fms_read = 0;
    radio_read = 0;
    robot_read = 0;
}

/**
 * Checks if the current protocol has been changed.
 * If so, this function will re-configure the timings of the watchdogs.
 */
static void check_protocol()
{
    if (protocol != DS_CurrentProtocol()) {
        protocol = DS_CurrentProtocol();

        /* Update sender timers */
        fms_send_timer.time = protocol->fms_interval;
        radio_send_timer.time = protocol->radio_interval;
        robot_send_timer.time = protocol->robot_interval;

        /* Updater receiver timers */
        fms_recv_timer.time = protocol->fms_interval * 50;
        radio_recv_timer.time = protocol->radio_interval * 50;
        robot_recv_timer.time = protocol->robot_interval * 50;
    }
}

/**
 * This function is called periodically, the function does the following:
 *    - Send data to the FMS, robot and radio
 *    - Read received data from the FMS, robot and radio
 *    - Feed/reset the watchdogs
 *    - Check if any of the watchdogs has expired
 */
static void run_event_loop()
{
    check_protocol();

    if (running) {
        send_data();
        recv_data();
        update_watchdogs();
    }
}

/**
 * Initializes the watchdogs and calls the main event loop function
 */
void Events_Init()
{
    if (!running) {
        /* Initialize FMS timers */
        DS_TimerInit (&fms_send_timer, 0);
        DS_TimerInit (&fms_recv_timer, 0);

        /* Initialize radio timers */
        DS_TimerInit (&radio_send_timer, 0);
        DS_TimerInit (&radio_recv_timer, 0);

        /* Initialize robot timers */
        DS_TimerInit (&robot_send_timer, 0);
        DS_TimerInit (&robot_recv_timer, 0);

        /* Start the event loop */
        running = 1;
        run_event_loop();
    }
}

/**
 * Disallows the event loop from performing any work
 */
void Events_Close()
{
    running = 0;
}

/**
 * Adds the data of the last event in the given event \a info.
 * If there are no events, this function will return \c 0, otherwise, this
 * function will return \c 1
 */
int DS_PollEvent (DS_Event* info)
{
    return 0;
}
