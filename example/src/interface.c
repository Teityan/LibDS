/*
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

#include "interface.h"

#include <sds.h>
#include <LibDS.h>
#include <stdio.h>
#include <stdlib.h>
#include <curses.h>

/*
 * Define window sizes
 */
#define TOP_HEIGHT     3
#define BOTTOM_HEIGHT  3
#define CENTRAL_HEIGHT 24 - BOTTOM_HEIGHT - TOP_HEIGHT

/*
 * Define basic label states
 */
#define CHECKED   "[*]"
#define UNCHECKED "[ ]"
#define NO_DATA   "--.--"
#define ENABLED   "Enabled"
#define DISABLED  "Disabled"

/*
 * Define windows
 */
static WINDOW* window;
static WINDOW* enabled_win;
static WINDOW* console_win;
static WINDOW* status_info;
static WINDOW* voltage_win;
static WINDOW* robot_status;
static WINDOW* bottom_window;

/*
 * Define window elements
 */
static sds can_str;
static sds cpu_str;
static sds ram_str;
static sds disk_str;
static sds status_str;
static sds enabled_str;
static sds voltage_str;
static sds console_str;
static sds stick_check_str;
static sds rcode_check_str;
static sds robot_check_str;

/**
 * Changes the \a label to "[*]" if checked is greater than \c 0,
 * otherwise, the function will change the \a label to "[ ]"
 */
static sds set_checked (sds label, int checked)
{
    sdsfree (label);
    label = sdsnew (checked ? CHECKED : UNCHECKED);

    return label;
}

/**
 * Sets the default label texts
 */
static void init_strings()
{
    robot_check_str = set_checked (robot_check_str, 0);
    rcode_check_str = set_checked (rcode_check_str, 0);
    stick_check_str = set_checked (stick_check_str, 0);

    can_str = sdsnew (NO_DATA);
    cpu_str = sdsnew (NO_DATA);
    ram_str = sdsnew (NO_DATA);
    disk_str = sdsnew (NO_DATA);
    voltage_str = sdsnew (NO_DATA);
    enabled_str = sdsnew (DISABLED);
    status_str = sdsnew (DS_GetStatusString());
    console_str = sdsnew ("[INFO] Welcome to the ConsoleDS!");
}

/**
 * De-allocates the memory used by the strings
 */
static void close_strings()
{
    sdsfree (can_str);
    sdsfree (cpu_str);
    sdsfree (ram_str);
    sdsfree (disk_str);
    sdsfree (status_str);
    sdsfree (enabled_str);
    sdsfree (voltage_str);
    sdsfree (console_str);
    sdsfree (stick_check_str);
    sdsfree (rcode_check_str);
    sdsfree (robot_check_str);
}

/**
 * Creates the base windows of the application
 */
static void draw_windows()
{
    /* Delete the windows */
    delwin (enabled_win);
    delwin (console_win);
    delwin (status_info);
    delwin (voltage_win);
    delwin (robot_status);
    delwin (bottom_window);

    /* Create the windows */
    voltage_win   = newwin (TOP_HEIGHT, 20, 0, 0);
    enabled_win   = newwin (TOP_HEIGHT, 20, 0, 60);
    robot_status  = newwin (TOP_HEIGHT, 40, 0, 20);
    bottom_window = newwin (BOTTOM_HEIGHT, 80, 24 - BOTTOM_HEIGHT,  0);
    console_win   = newwin (CENTRAL_HEIGHT, 60, TOP_HEIGHT, 0);
    status_info   = newwin (CENTRAL_HEIGHT, 20, TOP_HEIGHT, 60);

    /* Draw borders */
    wborder (voltage_win,   0, 0, 0, 0, 0, 0, 0, 0);
    wborder (robot_status,  0, 0, 0, 0, 0, 0, 0, 0);
    wborder (enabled_win,   0, 0, 0, 0, 0, 0, 0, 0);
    wborder (console_win,   0, 0, 0, 0, 0, 0, 0, 0);
    wborder (status_info,   0, 0, 0, 0, 0, 0, 0, 0);
    wborder (bottom_window, 0, 0, 0, 0, 0, 0, 0, 0);

    /* Add topbar elements */
    mvwaddstr (console_win,  1, 1, console_str);
    mvwaddstr (enabled_win,  1, 2, enabled_str);
    mvwaddstr (robot_status, 1, 2, status_str);

    /* Add voltage elements */
    mvwaddstr (voltage_win,  1,  2, "Voltage:");
    mvwaddstr (voltage_win,  1, 12, voltage_str);

    /* Add status panel elements */
    mvwaddstr (status_info, 1, 2, "STATUS:");
    mvwaddstr (status_info, 3, 2, robot_check_str);
    mvwaddstr (status_info, 4, 2, rcode_check_str);
    mvwaddstr (status_info, 5, 2, stick_check_str);
    mvwaddstr (status_info, 3, 6, "Robot Comms");
    mvwaddstr (status_info, 4, 6, "Robot Code");
    mvwaddstr (status_info, 5, 6, "Joysticks");

    /* Add robot status elements */
    mvwaddstr (status_info,  7, 2, "ROBOT STATUS:");
    mvwaddstr (status_info,  9, 2, "CAN:");
    mvwaddstr (status_info, 10, 2, "CPU:");
    mvwaddstr (status_info, 11, 2, "RAM:");
    mvwaddstr (status_info, 12, 2, "Disk:");
    mvwaddstr (status_info,  9, 8, can_str);
    mvwaddstr (status_info, 10, 8, cpu_str);
    mvwaddstr (status_info, 11, 8, ram_str);
    mvwaddstr (status_info, 12, 8, disk_str);

    /* Add bottom bar labels */
    mvwaddstr (bottom_window, 1, 1,  "Quit (q)");
    mvwaddstr (bottom_window, 1, 12, "Set enabled (e,d)");
    mvwaddstr (bottom_window, 1, 33, "Set Control Mode (o,a,t)");
    mvwaddstr (bottom_window, 1, 61, "More Options (m)");
}

/**
 * Refreshes the contents of each window
 */
static void refresh_windows()
{
    wnoutrefresh (window);
    wnoutrefresh (enabled_win);
    wnoutrefresh (console_win);
    wnoutrefresh (status_info);
    wnoutrefresh (voltage_win);
    wnoutrefresh (robot_status);
    wnoutrefresh (bottom_window);
}

/**
 * Creates the main window and sets the default label values
 */
void init_interface()
{
    init_strings();
    window = initscr();

    if (!window) {
        printf ("Cannot initialize window!\n");
        exit (EXIT_FAILURE);
    }

    noecho();
    curs_set (0);
    keypad (window, 1);
}

/**
 * Removes the main window and deletes the allocated label memory
 */
void close_interface()
{
    delwin (window);
    endwin();
    refresh();
    close_strings();

#ifdef WIN32
    system ("cls");
#else
    system ("clear");
#endif
}

/**
 * Re-draws the user interface
 */
void update_interface()
{
    refresh();
    draw_windows();
    refresh_windows();
}

/**
 * Updates the status label to display the current state
 * of the robot and the LibDS
 */
void update_status_label()
{
    sdsfree (status_str);
    status_str = sdsnew (DS_GetStatusString());
}

/**
 * Updates the value displayed in the CAN field
 */
void set_can (const int can)
{
    can_str = sdscatfmt (can_str, "%i %%", can);
}

/**
 * Updates the value displayed in the CPU field
 */
void set_cpu (const int cpu)
{
    cpu_str = sdscatfmt (cpu_str, "%i %%", cpu);
}

/**
 * Updates the value displayed in the RAM field
 */
void set_ram (const int ram)
{
    ram_str = sdscatfmt (ram_str, "%i %%", ram);
}

/**
 * Updates the value displayed in the disk field
 */
void set_disk (const int disk)
{
    disk_str = sdscatfmt (disk_str, "%i %%", disk);
}

/**
 * Updates the text of the \a enabled label
 */
void set_enabled (const int enabled)
{
    sdsfree (enabled_str);
    enabled_str = sdsnew (enabled ? ENABLED : DISABLED);
}

/**
 * Updates the state of the robot code checkbox
 */
void set_robot_code (const int code)
{
    set_checked (rcode_check_str, code);
}

/**
 * Updates the state of the robot communications checkbox
 */
void set_robot_comms (const int comms)
{
    set_checked (robot_check_str, comms);
}

/**
 * Updates the text of the robot voltage field
 */
void set_voltage (const double voltage)
{
    voltage_str = sdscatprintf (voltage_str, "%f", voltage);
}

/**
 * Updates the state of the joysticks checkbox
 */
void set_has_joysticks (const int joysticks)
{
    set_checked (stick_check_str, joysticks);
}