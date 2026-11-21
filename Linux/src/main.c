#include "ui.h"
#include "backend.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <locale.h>
#include <signal.h>
#include <sys/stat.h>

static void stop_private_bus(void);   /* fwd — defined with the bus helper */

static void on_signal(int sig)
{
    (void)sig;
    backend_cleanup();
    stop_private_bus();
    _exit(0);
}

/* ── Restore environment variables from --env-VAR=VALUE args ────────── */

static int restore_env(int *argc, char ***argv)
{
    int new_argc = 0;
    char **new_argv = malloc(*argc * sizeof(char *));
    if (!new_argv) return -1;

    for (int i = 0; i < *argc; i++) {
        if (strncmp((*argv)[i], "--env-", 6) == 0) {
            const char *eq = strchr((*argv)[i] + 6, '=');
            if (eq && eq > (*argv)[i] + 6) {
                int namelen = (int)(eq - ((*argv)[i] + 6));
                char name[256];
                if (namelen < (int)sizeof(name)) {
                    memcpy(name, (*argv)[i] + 6, namelen);
                    name[namelen] = '\0';
                    setenv(name, eq + 1, 1);
                }
            }
        } else {
            new_argv[new_argc++] = (*argv)[i];
        }
    }

    *argc = new_argc;
    *argv = new_argv;
    return 0;
}

/* ── Elevate to root via pkexec if not already root ─────────────────── */

static void elevate_if_necessary(int argc, char **argv)
{
    if (geteuid() == 0) return;
    /* Dev/QA mode: exercise the UI without SMU access and without pkexec */
    if (getenv("TUXTIMINGS_ALLOW_NO_BACKEND")) return;

    /* Build argv for pkexec: pkexec <self> --env-VAR=VALUE ... [original args] */
    /* DBUS_SESSION_BUS_ADDRESS is deliberately NOT forwarded: as root we are
     * refused by the user's session bus, and the failed connection just
     * produces a startup warning ("Unable to acquire session bus"). */
    const char *env_vars[] = {
        "DISPLAY", "WAYLAND_DISPLAY", "XDG_RUNTIME_DIR", "XAUTHORITY",
        "XDG_CONFIG_HOME", "HOME",
        "TUXTIMINGS_AOD_RMP", "TUXTIMINGS_NCT_FORCE_ID", NULL
    };

    /* Count env args */
    int env_count = 0;
    for (int i = 0; env_vars[i]; i++) {
        if (getenv(env_vars[i])) env_count++;
    }

    /* +3: pkexec, self, NULL; +argc for original args;
     * +1 extra for the conditional --env-GDK_BACKEND=wayland entry */
    int total = 2 + env_count + 1 + (argc - 1) + 1;
    char **new_argv = malloc(total * sizeof(char *));
    if (!new_argv) return;

    int n = 0;
    new_argv[n++] = "pkexec";

    /* Get self path */
    char self[4096];
    ssize_t len = readlink("/proc/self/exe", self, sizeof(self) - 1);
    if (len <= 0) { free(new_argv); return; }
    self[len] = '\0';

    /* Prefer installed path for polkit policy match — but never an older
     * build than the one being run, or dev builds silently launch the stale
     * system copy. */
    const char *installed = "/opt/TuxTimings/bin/tuxtimings";
    struct stat st_self, st_inst;
    if (access(installed, X_OK) == 0 &&
        stat(self, &st_self) == 0 && stat(installed, &st_inst) == 0 &&
        st_inst.st_mtim.tv_sec >= st_self.st_mtim.tv_sec)
        new_argv[n++] = (char *)installed;
    else
        new_argv[n++] = self;

    /* Add env forwarding args */
    char env_bufs[16][512];
    int ei = 0;
    for (int i = 0; env_vars[i] && ei < 16; i++) {
        const char *val = getenv(env_vars[i]);
        if (val) {
            snprintf(env_bufs[ei], sizeof(env_bufs[ei]), "--env-%s=%s", env_vars[i], val);
            new_argv[n++] = env_bufs[ei];
            ei++;
        }
    }

    /* If on Wayland, forward that hint */
    if (getenv("WAYLAND_DISPLAY") && ei < 16) {
        snprintf(env_bufs[ei], sizeof(env_bufs[ei]), "--env-GDK_BACKEND=wayland");
        new_argv[n++] = env_bufs[ei++];
    }

    /* Copy original args (skip argv[0]) */
    for (int i = 1; i < argc; i++)
        new_argv[n++] = argv[i];
    new_argv[n] = NULL;

    execvp("pkexec", new_argv);
    /* If execvp returns, pkexec failed */
    perror("pkexec");
    free(new_argv);
    exit(1);
}

/* ── Private session bus fallback ──────────────────────────────────────
 * The pkexec'd root copy has no DBUS_SESSION_BUS_ADDRESS by design (the
 * user's session bus refuses root connections). GTK4 then asks GDBus to
 * autolaunch a bus via `dbus-launch`, which does not exist on systems
 * where that helper was dropped — producing one startup warning:
 *   Gtk-WARNING: Unable to acquire session bus: Failed to execute child
 *   process "dbus-launch" (No such file or directory)
 * GTK keeps working without the bus (it is only needed for portals /
 * session registration), so this is cosmetic — but we can silence it
 * honestly: start a private, TuxTimings-only dbus-daemon session bus and
 * tear it down on exit. Never touches an existing session bus (normal
 * user runs are unaffected). */
static pid_t s_bus_pid = 0;

static void stop_private_bus(void)
{
    if (s_bus_pid > 0) {
        kill(s_bus_pid, SIGTERM);
        s_bus_pid = 0;
    }
}

static void ensure_session_bus(void)
{
    if (getenv("DBUS_SESSION_BUS_ADDRESS"))
        return;

    FILE *p = popen("dbus-daemon --session --fork --print-address=1 "
                    "--print-pid=1 --nopidfile 2>/dev/null", "r");
    if (!p)
        return;
    char addr[512], pidbuf[32];
    /* prints the address, then the daemon's pid (it forks off) */
    if (fgets(addr, sizeof(addr), p) && strncmp(addr, "unix:", 5) == 0 &&
        fgets(pidbuf, sizeof(pidbuf), p)) {
        addr[strcspn(addr, "\r\n")] = '\0';
        setenv("DBUS_SESSION_BUS_ADDRESS", addr, 1);
        long pid = strtol(pidbuf, NULL, 10);
        if (pid > 0) s_bus_pid = (pid_t)pid;
    }
    pclose(p);
}

/* ── Main ───────────────────────────────────────────────────────────── */

int main(int argc, char *argv[])
{
    signal(SIGTERM, on_signal);
    signal(SIGINT,  on_signal);

    restore_env(&argc, &argv);
    elevate_if_necessary(argc, argv);
    ensure_session_bus();

    /* Force C locale for numeric formatting (dots, not commas) */
    setlocale(LC_NUMERIC, "C");

    if (!backend_is_supported() && !getenv("TUXTIMINGS_ALLOW_NO_BACKEND")) {
        fprintf(stderr, "TuxTimings: ryzen_smu driver not found at /sys/kernel/ryzen_smu_drv/\n"
                        "Please install the ryzen_smu kernel module.\n");
        return 1;
    }

    GtkApplication *app = ui_create(argc, argv);
    int status = g_application_run(G_APPLICATION(app), argc, argv);
    g_object_unref(app);
    backend_cleanup();
    stop_private_bus();
    return status;
}
