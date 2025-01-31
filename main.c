// SPDX-License-Identifier: LGPL-2.1-or-later

#include <errno.h>
#include <spawn.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <systemd/sd-bus.h>
#include <unistd.h>

#define RAII(clean_up) __attribute__((__cleanup__(clean_up)))

static bool verbose_output;
static const char *inhibit_reason;
static const char *application;

static void debug(const char *s) {
    if (verbose_output) {
        fputs(s, stderr);
    }
}

static bool inhibit_via_inhibit_portal(sd_bus *bus) {
    RAII(sd_bus_message_unrefp) sd_bus_message *msg = NULL;
    RAII(sd_bus_error_free) sd_bus_error error = SD_BUS_ERROR_NULL;

    debug("Trying to inhibit idle via org.freedesktop.portal.Inhibit interface: ");

    const int ret = sd_bus_call_method(bus, "org.freedesktop.portal.Desktop", "/org/freedesktop/portal/desktop",
                                       "org.freedesktop.portal.Inhibit", "Inhibit", &error, &msg, "sua{sv}",
                                       /* Window id */ "", /* Flags */ 8, 1, "reason", "s", inhibit_reason);

    if (ret < 0) {
        debug("Failed.\n");
        fprintf(stderr, "Failed to inhibit idle: %s\n", error.message);
        return false;
    } else {
        debug("Ok.\n");
        return true;
    }
}

static bool inhibit_via_screen_saver(sd_bus *bus) {
    RAII(sd_bus_message_unrefp) sd_bus_message *msg = NULL;
    RAII(sd_bus_error_free) sd_bus_error error = SD_BUS_ERROR_NULL;

    debug("Trying to inhibit screensaver via org.freedesktop.ScreenSaver interface: ");
    const int ret =
        sd_bus_call_method(bus, "org.freedesktop.ScreenSaver", "/org/freedesktop/ScreenSaver",
                           "org.freedesktop.ScreenSaver", "Inhibit", &error, &msg, "ss", inhibit_reason, application);

    if (ret < 0) {
        debug("Failed.\n");
        fprintf(stderr, "Failed to inhibit screensaver: %s\n", error.message);
        return false;
    } else {
        debug("Ok.\n");
        return true;
    }
}

static bool inhibit_via_power_management(sd_bus *bus) {
    RAII(sd_bus_message_unrefp) sd_bus_message *msg = NULL;
    RAII(sd_bus_error_free) sd_bus_error error = SD_BUS_ERROR_NULL;

    debug("Trying to inhibit power management via org.freedesktop.PowerManagement.Inhibit interface: ");
    const int ret = sd_bus_call_method(
        bus, "org.freedesktop.PowerManagement.Inhibit", "/org/freedesktop/PowerManagement/Inhibit",
        "org.freedesktop.PowerManagement.Inhibit", "Inhibit", &error, &msg, "ss", inhibit_reason, application);

    if (ret < 0) {
        debug("Failed.\n");
        fprintf(stderr, "Failed to inhibit power saving: %s\n", error.message);
        return false;
    } else {
        debug("Ok.\n");
        return true;
    }
}

int main(int argc, char *argv[]) {
    verbose_output = !!getenv("INHIBIT_DEBUG");
    inhibit_reason = getenv("INHIBIT_REASON") ?: "A game is running";
    application = argc < 2 ? argv[0] : argv[1];

    RAII(sd_bus_unrefp) sd_bus *bus = NULL;
    int ret = sd_bus_open_user(&bus);
    if (ret < 0) {
        fprintf(stderr, "Failed to connect to user bus: %s\n", strerror(-ret));
        return 1;
    }

    inhibit_via_inhibit_portal(bus);
    inhibit_via_screen_saver(bus);
    inhibit_via_power_management(bus);

    if (argc < 2) {
        while (true) {
            pause();
        }
    }

    debug("Starting process: ");
    pid_t child_pid;
    ret = posix_spawnp(&child_pid, argv[1], NULL, NULL, argv + 1, environ);
    if (ret != 0) {
        fprintf(stderr, "Failed to start process: %s\n", strerror(ret));
        return 1;
    }
    debug("OK\n");

    int wstatus;
    const int rc = TEMP_FAILURE_RETRY(waitpid(child_pid, &wstatus, 0));
    if (rc != 0) {
        if (WIFEXITED(wstatus)) {
            if (WEXITSTATUS(wstatus) == 0) {
                debug("Child process exited normally\n");
            } else {
                fprintf(stderr, "Child process exited with code %d\n", WEXITSTATUS(wstatus));
            }
            return WEXITSTATUS(wstatus);
        }
        if (WIFSIGNALED(wstatus)) {
            fprintf(stderr, "Child process killed by signal %s\n", strsignal(WTERMSIG(wstatus)));
            return WTERMSIG(wstatus) + 128;
        }
        fprintf(stderr, "Child process exited abnormally");
        return 127;
    }

    fprintf(stderr, "Child process does not exist");
    return 1;
}
