#include <time.h>
#include <stdio.h>
#include <errno.h>
#include <fcntl.h>
#include <string.h>
#include <signal.h>
#include <unistd.h>
#include <stdlib.h>
#include <dirent.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <sys/file.h>
#include <sys/ipc.h>
#include <sys/msg.h>

static char HTTP_LOG_FILE[256];
static char PID_LOCK_FILE[256];
static char CORE_MAIN_DIR[256];

static int g_token_fd;
static int g_notify_stop;
static pid_t g_process_id;
static pid_t g_http_pid;
static pid_t g_midb_pid;
static pid_t g_dac_pid;
static pid_t g_zcore_pid;
static pid_t g_supervised_process;


/*
 *  set the stop flag and relay signal to supervised process
 *  @param
 *      sig         signal type
 */
void supervisor_sigstop(int sig)
{
	g_notify_stop = 1;
	if (g_supervised_process > 0) {
		kill(g_supervised_process, SIGTERM);
	}
}

/*
 *  make the supervised process stop, and the supervised process will be
 *  started again by supervisor process
 *  @param
 *      sig         signal type
 */
void supervisor_sigrestart(int sig)
{
	if (g_supervised_process > 0) {
		kill(g_supervised_process, SIGKILL);
	}
}


/*
 *	stop the daemon process, the signal will also be relayed to supervisors
 *	@param
 *		sig			signal type
 */
void daemon_sigstop(int sig)
{
	g_notify_stop = 1;
	if (g_http_pid > 0) {
		kill(g_http_pid, SIGTERM);
	}
	if (g_midb_pid > 0) {
		kill(g_http_pid, SIGTERM);
	}
	if (g_dac_pid > 0) {
		kill(g_dac_pid, SIGTERM);
	}
	if (g_zcore_pid > 0) {
		kill(g_zcore_pid, SIGTERM);
	}
}


/*
 *  start http service
 */
void start_http()
{
	int fd, status;
	char temp_path[256];
	struct stat node_stat;
	char *args[] = {"./http", "../config/http.cfg", NULL};
	
	if (0 != stat(HTTP_LOG_FILE, &node_stat) 
		|| node_stat.st_size > 128*1024*1024) {
		fd = open(HTTP_LOG_FILE, O_WRONLY|O_CREAT|O_TRUNC, 0666);
		close(fd);
	}
	g_http_pid = fork();
	if (g_http_pid < 0) {
		return;
	} else if (0 == g_http_pid) {
		g_notify_stop = 0;
		signal(SIGTERM, supervisor_sigstop);
		signal(SIGALRM, supervisor_sigrestart);
		while (0 == g_notify_stop) {
			g_supervised_process = fork();
			if (0 == g_supervised_process) {
				fd = open(HTTP_LOG_FILE, O_WRONLY|O_APPEND);
				close(STDOUT_FILENO);
				dup2(fd, STDOUT_FILENO);
				close(fd);
				chdir(CORE_MAIN_DIR);
				if (execve("./http", args, NULL) == -1) {
					exit(EXIT_FAILURE);
				}
			} else if (g_supervised_process > 0) {
				waitpid(g_supervised_process, &status, 0);
			}
			sleep(1);
		}
		exit(EXIT_SUCCESS);
	}
}

/*
 *  start midb service
 */
void start_midb()
{
	int status;
	char temp_path[256];
	struct stat node_stat;
	char *args[] = {"./midb", "../config/midb.cfg", NULL};

	g_midb_pid = fork();
	if (g_midb_pid < 0) {
		return;
	} else if (0 == g_midb_pid) {
		g_notify_stop = 0;
		signal(SIGTERM, supervisor_sigstop);
		signal(SIGALRM, supervisor_sigrestart);
		while (0 == g_notify_stop) {
			g_supervised_process = fork();
			if (0 == g_supervised_process) {
				chdir(CORE_MAIN_DIR);
				if (execve("./midb", args, NULL) == -1) {
					exit(EXIT_FAILURE);
				}
			} else if (g_supervised_process > 0) {
				waitpid(g_supervised_process, &status, 0);
			}
			sleep(1);
		}
		exit(EXIT_SUCCESS);
	}
}

/*
 *  start dac service
 */
void start_dac()
{
	int status;
	char temp_path[256];
	struct stat node_stat;
	char *args[] = {"./dac", "../config/dac.cfg", NULL};

	g_dac_pid = fork();
	if (g_dac_pid < 0) {
		return;
	} else if (0 == g_dac_pid) {
		g_notify_stop = 0;
		signal(SIGTERM, supervisor_sigstop);
		signal(SIGALRM, supervisor_sigrestart);
		while (0 == g_notify_stop) {
			g_supervised_process = fork();
			if (0 == g_supervised_process) {
				chdir(CORE_MAIN_DIR);
				if (execve("./dac", args, NULL) == -1) {
					exit(EXIT_FAILURE);
				}
			} else if (g_supervised_process > 0) {
				waitpid(g_supervised_process, &status, 0);
			}
			sleep(1);
		}
		exit(EXIT_SUCCESS);
	}
}

/*
 *  start zcore service
 */
void start_zcore()
{
	int status;
	char temp_path[256];
	struct stat node_stat;
	char *args[] = {"./zcore", "../config/zcore.cfg", NULL};

	g_zcore_pid = fork();
	if (g_zcore_pid < 0) {
		return;
	} else if (0 == g_zcore_pid) {
		g_notify_stop = 0;
		signal(SIGTERM, supervisor_sigstop);
		signal(SIGALRM, supervisor_sigrestart);
		while (0 == g_notify_stop) {
			g_supervised_process = fork();
			if (0 == g_supervised_process) {
				chdir(CORE_MAIN_DIR);
				if (execve("./zcore", args, NULL) == -1) {
					exit(EXIT_FAILURE);
				}
			} else if (g_supervised_process > 0) {
				waitpid(g_supervised_process, &status, 0);
			}
			sleep(1);
		}
		exit(EXIT_SUCCESS);
	}
}

/*
 *	start http ... services
 */
void start_service()
{
	time_t now_time;
	struct tm *ptm;
	pid_t pid, sid; /* process ID and session ID */
	int fd, ctrl_id;
	char str[16];
	key_t k_ctrl;
	long ctrl_type;


	pid = fork();
	if (pid < 0) {
		printf("fail to fork the child process\n");
        exit(EXIT_FAILURE);
	}
	if (pid > 0) {
		exit(EXIT_SUCCESS);
	}
	/* child (daemon) continues */
	/* obtain a new process group */
	sid = setsid();
	if (sid < 0) {
        printf("fail to create new session ID\n");
        exit(EXIT_FAILURE);
    }
	for (fd=getdtablesize(); fd>=0; fd--) {
		if (STDOUT_FILENO != fd) {
			close(fd); /* close all descriptors */
		}
	}
	/* check if the process is running uniquely */
	g_token_fd = open(PID_LOCK_FILE, O_RDWR|O_CREAT, 0640);
	/* can not open */
	if (g_token_fd < 0) {
		printf("cannot open the %s\n", PID_LOCK_FILE);
		exit(EXIT_FAILURE);
	}
	/* can not lock */
	if (lockf(g_token_fd, F_TLOCK, 0) < 0 ) {
		printf("there's another instance is running in system\n");
		close(g_token_fd);
		exit(EXIT_FAILURE);
	}
	/* first instance continues */
	sprintf(str,"%d\n",getpid());
	write(g_token_fd, str, strlen(str));
	/* record pid to lockfile */
	/* handle standart I/O */

	/* close the STDOUT_FILENO */
	close(STDOUT_FILENO);
	fd = open("/dev/null",O_RDWR);
	dup(fd);
	dup(fd);
	/* change the file mode mask */
	umask(0);
	/* change running directory */
	chdir("/tmp");
	signal(SIGTSTP, SIG_IGN); /* ignore tty signals */
	signal(SIGTTOU, SIG_IGN); /* ignore tty signals */
	signal(SIGTTIN, SIG_IGN); /* ignore tty signals */
	signal(SIGTERM, daemon_sigstop);    /* catch term signal */

	start_http();
	start_midb();
	start_dac();
	start_zcore();
	
	while (0 == g_notify_stop) {
		sleep(1);	
	}
	lockf(g_token_fd, F_ULOCK, 0);
	close(g_token_fd);
	remove(PID_LOCK_FILE);
	exit(EXIT_SUCCESS);
}

/*
 *	stop the daemon
 */
void stop_service()
{
	int fd;
    pid_t pid;
    char str[32];

	memset(str, 0, 32);
	fd = open(PID_LOCK_FILE, O_RDONLY);
	if (fd < 0) {
		printf("core is not running\n");
		exit(EXIT_SUCCESS);
	}
	read(fd, str, 16);
	close(fd);
	pid = atoi(str);
	if (0 == pid) {
		printf("fail to get core's pid\n");
		exit(EXIT_FAILURE);
	}
	kill(pid, SIGTERM);
	exit(EXIT_SUCCESS);
}

/*
 *	print the daemon status
 */
void status_service()
{
	int fd;
    pid_t pid;
    char str[32];
    struct stat node_stat;

	if (0 != stat(PID_LOCK_FILE, &node_stat)) {
		printf("core is not running\n");
		exit(EXIT_SUCCESS);
	}
	fd = open(PID_LOCK_FILE, O_RDONLY);
	read(fd, str, 16);
	close(fd);
	pid = atoi(str);
	if (0 == pid) {
		printf("core is not running\n");
		exit(EXIT_SUCCESS);
	}
	sprintf(str, "ps -p %d > /dev/null", pid);
	if (0 == WEXITSTATUS(system(str))) {
		printf("core (pid: %d) is running\n", pid);
	} else {
		printf("core is not running\n");
	}
	exit(EXIT_SUCCESS);
}

/*
 *	restart the daemon
 */
void restart_service()
{
	int i, fd;
    pid_t pid;
    char str[32];

	fd = open(PID_LOCK_FILE, O_RDONLY);
	if (fd < 0) {
		printf("core is not running\n");
		exit(EXIT_FAILURE);
	}
	read(fd, str, 16);
	close(fd);
	pid = atoi(str);
	if (0 == pid) {
		printf("core is not running\n");
		exit(EXIT_FAILURE);
	}
	kill(pid, SIGTERM);
	sprintf(str, "ps -p %d > /dev/null", pid);
	for (i=0; i<20; i++) {
	    if (0 != WEXITSTATUS(system(str))) {
			break;
		}
		sleep(3);
	}
	if (20 == i) {
		printf("cannot stop core\n");
		exit(EXIT_FAILURE);
	}
	sleep(3);
	start_service();
}

int main(int argc, char **argv)
{
	int fd;
	pid_t pid;
	char str[32];
	struct stat node_stat;

	if (2 == argc && 0 == strcmp(argv[1], "--help")) {
		printf("usage: %s start|stop|restart|status\n", argv[0]);
		exit(EXIT_SUCCESS);
	}
	if (3 != argc) {
		printf("usage: %s path start|stop|restart|status\n", argv[0]);
		exit(EXIT_FAILURE);
	}
	
	sprintf(PID_LOCK_FILE, "%s/token/token.pid", argv[1]);
	sprintf(HTTP_LOG_FILE, "%s/logs/http_running.log", argv[1]);
	sprintf(CORE_MAIN_DIR, "%s/bin", argv[1]);
	if (0 == strcmp(argv[2], "start")) {
		start_service();
	} else if (0 == strcmp(argv[2], "stop")) {
		stop_service();
	} else if (0 == strcmp(argv[2], "restart")) {
		restart_service();
	} else if (0 == strcmp(argv[2], "status")) {
		status_service();
	} else {
		printf("unknown option %s\n", argv[1]);
		exit(EXIT_FAILURE);
	}
}
