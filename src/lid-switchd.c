/*
 * Copyright (c) 2026 Devin Teske <dteske@FreeBSD.org>
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

/*
 * lid-switchd: root helper for hw.acpi.lid_switch_state.
 *
 * Boot (rc.d start): apply lid_switchd_switch_state to
 * hw.acpi.lid_switch_state, then listen on a Unix socket (0660,
 * group lid_switchd) so a session can change policy or suspend
 * without becoming root.
 *
 * Line protocol (one command per connection, reply one line):
 *   awake | s0ix | toggle | status | suspend-s0ix | suspend-s3 | quit
 *
 * Usage:
 *   lid-switchd start|stop|status|serve
 *   lid-switchd awake|s0ix|toggle|status|suspend-s0ix|suspend-s3
 *     (direct, when already root; also used internally)
 */
#include <sys/param.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/un.h>
#include <sys/wait.h>

#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <grp.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define DEFAULT_SOCK	"/var/run/lid-switchd.sock"
#define DEFAULT_PID	"/var/run/lid-switchd.pid"
#define DEFAULT_GROUP	"lid_switchd"
#define DEFAULT_STATE	"suspend_to_idle"
#define SYSCTL_LID	"hw.acpi.lid_switch_state"
#define SYSCTL_SUSPEND	"kern.power.suspend"

static const char *sock_path = DEFAULT_SOCK;
static const char *pid_path = DEFAULT_PID;
static const char *sock_group = DEFAULT_GROUP;
static volatile sig_atomic_t running = 1;

static void
on_signal(int sig __unused)
{
	running = 0;
}

static int
run_capture(char *const argv[], char *out, size_t outsz)
{
	int pfd[2];
	pid_t pid;
	ssize_t n, off;
	int status;

	if (pipe(pfd) != 0)
		return (-1);
	pid = fork();
	if (pid < 0) {
		close(pfd[0]);
		close(pfd[1]);
		return (-1);
	}
	if (pid == 0) {
		close(pfd[0]);
		if (dup2(pfd[1], STDOUT_FILENO) < 0)
			_exit(127);
		close(pfd[1]);
		closefrom(3);
		execvp(argv[0], argv);
		_exit(127);
	}
	close(pfd[1]);
	off = 0;
	while (off + 1 < (ssize_t)outsz &&
	    (n = read(pfd[0], out + off, outsz - 1 - off)) > 0)
		off += n;
	out[off] = '\0';
	close(pfd[0]);
	while (waitpid(pid, &status, 0) < 0) {
		if (errno != EINTR)
			return (-1);
	}
	if (!WIFEXITED(status) || WEXITSTATUS(status) != 0)
		return (-1);
	/* trim trailing newline */
	while (off > 0 && (out[off - 1] == '\n' || out[off - 1] == '\r'))
		out[--off] = '\0';
	return (0);
}

static int
sysctl_get(const char *oid, char *out, size_t outsz)
{
	char oidbuf[128];
	char *argv[4];

	if (strlcpy(oidbuf, oid, sizeof(oidbuf)) >= sizeof(oidbuf))
		return (-1);
	argv[0] = "/sbin/sysctl";
	argv[1] = "-n";
	argv[2] = oidbuf;
	argv[3] = NULL;
	return (run_capture(argv, out, outsz));
}

static int
sysctl_set(const char *oid, const char *val)
{
	char arg[128];
	pid_t pid;
	int status;

	if (snprintf(arg, sizeof(arg), "%s=%s", oid, val) >= (int)sizeof(arg))
		return (-1);
	pid = fork();
	if (pid < 0)
		return (-1);
	if (pid == 0) {
		execl("/sbin/sysctl", "sysctl", arg, (char *)NULL);
		_exit(127);
	}
	while (waitpid(pid, &status, 0) < 0) {
		if (errno != EINTR)
			return (-1);
	}
	if (!WIFEXITED(status) || WEXITSTATUS(status) != 0)
		return (-1);
	return (0);
}

static int
sysrc_set_state(const char *val)
{
	char arg[128];
	pid_t pid;
	int status;

	if (snprintf(arg, sizeof(arg), "lid_switchd_switch_state=%s",
	    val) >= (int)sizeof(arg))
		return (-1);
	pid = fork();
	if (pid < 0)
		return (-1);
	if (pid == 0) {
		execl("/usr/sbin/sysrc", "sysrc", "-f", "/etc/rc.conf", arg,
		    (char *)NULL);
		_exit(127);
	}
	while (waitpid(pid, &status, 0) < 0) {
		if (errno != EINTR)
			return (-1);
	}
	if (!WIFEXITED(status) || WEXITSTATUS(status) != 0)
		return (-1);
	return (0);
}

static int
persist_and_apply(const char *state)
{
	if (strcmp(state, "NONE") != 0 &&
	    strcmp(state, "suspend_to_idle") != 0)
		return (-1);
	if (sysctl_set(SYSCTL_LID, state) != 0)
		return (-1);
	if (sysrc_set_state(state) != 0)
		warnx("sysrc persist failed for %s (sysctl applied)", state);
	return (0);
}

static int
do_status(char *out, size_t outsz)
{
	return (sysctl_get(SYSCTL_LID, out, outsz));
}

static int
do_awake(char *out, size_t outsz)
{
	if (persist_and_apply("NONE") != 0)
		return (-1);
	return (do_status(out, outsz));
}

static int
do_s0ix(char *out, size_t outsz)
{
	if (persist_and_apply("suspend_to_idle") != 0)
		return (-1);
	return (do_status(out, outsz));
}

static int
do_toggle(char *out, size_t outsz)
{
	char cur[64];

	if (do_status(cur, sizeof(cur)) != 0)
		return (-1);
	if (strcmp(cur, "NONE") == 0)
		return (do_s0ix(out, outsz));
	return (do_awake(out, outsz));
}

static int
do_suspend_s3(char *out, size_t outsz)
{
	pid_t pid;
	int status;

	warnx("requesting S3 (acpiconf -s 3)");
	pid = fork();
	if (pid < 0)
		return (-1);
	if (pid == 0) {
		execl("/usr/sbin/acpiconf", "acpiconf", "-s", "3",
		    (char *)NULL);
		_exit(127);
	}
	while (waitpid(pid, &status, 0) < 0) {
		if (errno != EINTR)
			return (-1);
	}
	if (!WIFEXITED(status) || WEXITSTATUS(status) != 0) {
		snprintf(out, outsz, "error");
		return (-1);
	}
	snprintf(out, outsz, "ok");
	return (0);
}

static int
do_suspend_s0ix(char *out, size_t outsz)
{
	char old[64];
	pid_t pid;
	int status;
	int rc = -1;

	if (sysctl_get(SYSCTL_SUSPEND, old, sizeof(old)) != 0)
		return (-1);
	warnx("requesting S0ix (was kern.power.suspend=%s)", old);
	if (sysctl_set(SYSCTL_SUSPEND, "suspend_to_idle") != 0)
		return (-1);
	pid = fork();
	if (pid < 0) {
		(void)sysctl_set(SYSCTL_SUSPEND, old);
		return (-1);
	}
	if (pid == 0) {
		execl("/usr/sbin/zzz", "zzz", (char *)NULL);
		_exit(127);
	}
	while (waitpid(pid, &status, 0) < 0) {
		if (errno != EINTR) {
			(void)sysctl_set(SYSCTL_SUSPEND, old);
			return (-1);
		}
	}
	if (WIFEXITED(status) && WEXITSTATUS(status) == 0)
		rc = 0;
	(void)sysctl_set(SYSCTL_SUSPEND, old);
	snprintf(out, outsz, rc == 0 ? "ok" : "error");
	return (rc);
}

/*
 * Handle one command.  Writes reply into out.  Returns 0 on success,
 * -1 on error (out still may hold a message), 1 if quit requested.
 */
static int
handle_cmd(const char *cmd, char *out, size_t outsz)
{
	if (strcmp(cmd, "awake") == 0)
		return (do_awake(out, outsz) == 0 ? 0 : -1);
	if (strcmp(cmd, "s0ix") == 0)
		return (do_s0ix(out, outsz) == 0 ? 0 : -1);
	if (strcmp(cmd, "toggle") == 0)
		return (do_toggle(out, outsz) == 0 ? 0 : -1);
	if (strcmp(cmd, "status") == 0)
		return (do_status(out, outsz) == 0 ? 0 : -1);
	if (strcmp(cmd, "suspend-s3") == 0)
		return (do_suspend_s3(out, outsz) == 0 ? 0 : -1);
	if (strcmp(cmd, "suspend-s0ix") == 0)
		return (do_suspend_s0ix(out, outsz) == 0 ? 0 : -1);
	if (strcmp(cmd, "quit") == 0) {
		snprintf(out, outsz, "bye");
		running = 0;
		return (1);
	}
	snprintf(out, outsz, "error: unknown command");
	return (-1);
}

static void
trim_line(char *s)
{
	size_t n;

	n = strlen(s);
	while (n > 0 && (s[n - 1] == '\n' || s[n - 1] == '\r' ||
	    s[n - 1] == ' ' || s[n - 1] == '\t'))
		s[--n] = '\0';
}

static void
fix_sock_owner(void)
{
	struct group *gr;

	gr = getgrnam(sock_group);
	if (gr == NULL)
		errx(1, "group %s: not found", sock_group);
	if (chown(sock_path, 0, gr->gr_gid) != 0)
		err(1, "chown %s", sock_path);
	if (chmod(sock_path, 0660) != 0)
		err(1, "chmod %s", sock_path);
}

static int
serve_loop(void)
{
	struct sockaddr_un sun;
	int lfd, cfd;
	char buf[256], reply[256];
	ssize_t n;
	size_t len;

	(void)unlink(sock_path);
	lfd = socket(AF_UNIX, SOCK_STREAM, 0);
	if (lfd < 0)
		err(1, "socket");
	memset(&sun, 0, sizeof(sun));
	sun.sun_family = AF_UNIX;
	if (strlcpy(sun.sun_path, sock_path, sizeof(sun.sun_path)) >=
	    sizeof(sun.sun_path))
		errx(1, "socket path too long");
	if (bind(lfd, (struct sockaddr *)&sun, SUN_LEN(&sun)) != 0)
		err(1, "bind %s", sock_path);
	fix_sock_owner();
	if (listen(lfd, 4) != 0)
		err(1, "listen");

	signal(SIGTERM, on_signal);
	signal(SIGINT, on_signal);
	signal(SIGPIPE, SIG_IGN);

	while (running) {
		int hr = 0;

		cfd = accept(lfd, NULL, NULL);
		if (cfd < 0) {
			if (errno == EINTR)
				continue;
			if (!running)
				break;
			warn("accept");
			continue;
		}
		len = 0;
		while (len + 1 < sizeof(buf)) {
			n = read(cfd, buf + len, sizeof(buf) - 1 - len);
			if (n < 0) {
				if (errno == EINTR)
					continue;
				break;
			}
			if (n == 0)
				break;
			len += (size_t)n;
			if (memchr(buf, '\n', len) != NULL)
				break;
		}
		buf[len] = '\0';
		trim_line(buf);
		reply[0] = '\0';
		if (buf[0] != '\0') {
			hr = handle_cmd(buf, reply, sizeof(reply));
			if (reply[0] == '\0')
				snprintf(reply, sizeof(reply),
				    hr == 0 ? "ok" : "error");
			dprintf(cfd, "%s\n", reply);
		}
		close(cfd);
		if (hr == 1)
			break;
	}
	close(lfd);
	(void)unlink(sock_path);
	return (0);
}

static const char *
rc_state_default(void)
{
	const char *e;

	e = getenv("lid_switchd_switch_state");
	if (e != NULL && e[0] != '\0')
		return (e);
	return (DEFAULT_STATE);
}

static int
cmd_start_foreground(void)
{
	const char *state;

	if (geteuid() != 0)
		errx(1, "must run as root");
	state = rc_state_default();
	if (strcmp(state, "NONE") != 0 &&
	    strcmp(state, "suspend_to_idle") != 0) {
		warnx("invalid lid_switchd_switch_state=%s; using %s",
		    state, DEFAULT_STATE);
		state = DEFAULT_STATE;
	}
	if (sysctl_set(SYSCTL_LID, state) != 0)
		err(1, "sysctl %s=%s", SYSCTL_LID, state);
	warnx("applied %s=%s; serving %s", SYSCTL_LID, state, sock_path);
	return (serve_loop());
}

static int
read_pid(void)
{
	FILE *fp;
	int pid = -1;

	fp = fopen(pid_path, "r");
	if (fp == NULL)
		return (-1);
	if (fscanf(fp, "%d", &pid) != 1)
		pid = -1;
	fclose(fp);
	return (pid);
}

static int
cmd_stop(void)
{
	int pid;

	pid = read_pid();
	if (pid > 1) {
		if (kill(pid, SIGTERM) != 0 && errno != ESRCH)
			warn("kill %d", pid);
	}
	(void)unlink(sock_path);
	(void)unlink(pid_path);
	return (0);
}

static int
cmd_status_svc(void)
{
	int pid;
	char cur[64];

	pid = read_pid();
	if (pid > 1 && kill(pid, 0) == 0)
		printf("lid-switchd is running as pid %d.\n", pid);
	else
		printf("lid-switchd is not running.\n");
	if (do_status(cur, sizeof(cur)) == 0)
		printf("%s=%s\n", SYSCTL_LID, cur);
	return (0);
}

static void
usage(void)
{
	fprintf(stderr,
	    "usage: lid-switchd start|stop|status|serve\n"
	    "       lid-switchd awake|s0ix|toggle|status|"
	    "suspend-s0ix|suspend-s3\n");
	exit(1);
}

int
main(int argc, char **argv)
{
	char reply[256];
	const char *env;
	int hr;

	env = getenv("lid_switchd_socket");
	if (env != NULL && env[0] != '\0')
		sock_path = env;
	env = getenv("lid_switchd_group");
	if (env != NULL && env[0] != '\0')
		sock_group = env;

	if (argc < 2)
		usage();

	if (strcmp(argv[1], "start") == 0 || strcmp(argv[1], "serve") == 0)
		return (cmd_start_foreground());
	if (strcmp(argv[1], "stop") == 0)
		return (cmd_stop());
	if (strcmp(argv[1], "status") == 0 && argc == 2)
		return (cmd_status_svc());

	/* Direct commands (root) or fall through for client-less testing. */
	if (geteuid() != 0)
		errx(1, "must run as root for direct commands");
	hr = handle_cmd(argv[1], reply, sizeof(reply));
	if (reply[0] != '\0')
		printf("%s\n", reply);
	return (hr < 0 ? 1 : 0);
}
