#include <time.h>
#include <stdio.h>
#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <sys/file.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <sys/shm.h>

#define INTERVAL_BEFORE_MIDNIGHT	5
#define TOKEN_SESSION				1
#define TOKEN_CONTROL				100
#define CTRL_RESTART_MONTOR			1
#define CTRL_RESTART_SUPERVISOR		2
#define CTRL_RESTART_WEBADAPTOR		3
#define CTRL_RESTART_SYNCHRONIZER	4
#define CTRL_RESTART_CDNER			5
#define SHARE_MEMORY_SIZE			1024*(32+sizeof(time_t)+16+256)

static char PID_LOCK_FILE[256];
static char ATHENA_MAIN_DIR[256];
static char CONTROL_TOKEN_FILE[256];
static char SESSION_TOKEN_FILE[256];

static int g_token_fd;
static int g_notify_stop;
static pid_t g_cdner_pid;
static pid_t g_process_id;
static pid_t g_monitor_pid;
static pid_t g_adaptor_pid;
static pid_t g_supervisor_pid;
static pid_t g_synchronizer_pid;
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
	if (g_monitor_pid > 0) {
		kill(g_monitor_pid, SIGTERM);
	}
	if (g_supervisor_pid > 0) {
		kill(g_supervisor_pid, SIGTERM);
	}
	if (g_adaptor_pid > 0) {
		kill(g_adaptor_pid, SIGTERM);
	}
	if (g_synchronizer_pid > 0) {
		kill(g_synchronizer_pid, SIGTERM);
	}
	if (g_cdner_pid > 0) {
		kill(g_cdner_pid, SIGTERM);
	}
}

void start_analyzer()
{
	pid_t pid;
	int len, status;
	char temp_str[32];
	char temp_path[256];
	char *args[] = {"./daemon", "../config/athena.cfg", NULL};
	struct stat node_stat;
	
	sprintf(temp_path, "%s/daemon", ATHENA_MAIN_DIR);
	if (0 != stat(temp_path, &node_stat)) {
		return;
	}
	pid = fork();
	if (0 == pid) {
		chdir(ATHENA_MAIN_DIR);
		if (execve("./daemon", args, NULL) == -1) {
			exit(EXIT_FAILURE);
		}
	} else if (pid > 0) {
		lseek(g_token_fd, SEEK_SET, 0);	
		write(g_token_fd, "-1\n", 3);
		fsync(g_token_fd);
		waitpid(pid, &status, 0);
		lseek(g_token_fd, SEEK_SET, 0);	
		len = sprintf(temp_str, "%d\n", getpid());
		write(g_token_fd, temp_str, len);
		fsync(g_token_fd);
	}
}

/*
 *  start monitor service
 */
void start_monitor()
{
	int status;
	struct stat node_stat;
	char temp_path[256];
	char *args[] = {"./monitor", "../config/athena.cfg", NULL};

	sprintf(temp_path, "%s/monitor", ATHENA_MAIN_DIR);
	if (0 != stat(temp_path, &node_stat)) {
		g_monitor_pid = -1;
		return;
	}
	g_monitor_pid = fork();
	if (g_monitor_pid < 0) {
		return;
	} else if (0 == g_monitor_pid) {
		g_notify_stop = 0;
		signal(SIGTERM, supervisor_sigstop);
		signal(SIGALRM, supervisor_sigrestart);
		while (0 == g_notify_stop) {
			g_supervised_process = fork();
			if (0 == g_supervised_process) {
				chdir(ATHENA_MAIN_DIR);
				if (execve("./monitor", args, NULL) == -1) {
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
 *  start supervisor service
 */
void start_supervisor()
{
	int status;
	struct stat node_stat;
	char temp_path[256];
	char *args[] = {"./supervisor", "../config/athena.cfg", NULL};

	sprintf(temp_path, "%s/supervisor", ATHENA_MAIN_DIR);
	if (0 != stat(temp_path, &node_stat)) {
		g_supervisor_pid = -1;
		return;
	}
	g_supervisor_pid = fork();
	if (g_supervisor_pid < 0) {
		return;
	} else if (0 == g_supervisor_pid) {
		g_notify_stop = 0;
		signal(SIGTERM, supervisor_sigstop);
		signal(SIGALRM, supervisor_sigrestart);
		while (0 == g_notify_stop) {
			g_supervised_process = fork();
			if (0 == g_supervised_process) {
				chdir(ATHENA_MAIN_DIR);
				if (execve("./supervisor", args, NULL) == -1) {
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
 *  start web adaptor service
 */
void start_adaptor()
{
	int status;
	struct stat node_stat;
	char temp_path[256];
	char *args[] = {"./web_adaptor", "../config/athena.cfg", NULL};

	sprintf(temp_path, "%s/web_adaptor", ATHENA_MAIN_DIR);
	if (0 != stat(temp_path, &node_stat)) {
		g_adaptor_pid = -1;
		return;
	}
	g_adaptor_pid = fork();
	if (g_adaptor_pid < 0) {
		return;
	} else if (0 == g_adaptor_pid) {
		g_notify_stop = 0;
		signal(SIGTERM, supervisor_sigstop);
		signal(SIGALRM, supervisor_sigrestart);
		while (0 == g_notify_stop) {
			g_supervised_process = fork();
			if (0 == g_supervised_process) {
				chdir(ATHENA_MAIN_DIR);
				if (execve("./web_adaptor", args, NULL) == -1) {
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
 *  start synchronizer service
 */
void start_synchronizer()
{
	int status;
	struct stat node_stat;
	char temp_path[256];
	char *args[] = {"./synchronizer", "../config/athena.cfg", NULL};

	sprintf(temp_path, "%s/synchronizer", ATHENA_MAIN_DIR);
	if (0 != stat(temp_path, &node_stat)) {
		g_synchronizer_pid = -1;
		return;
	}
	g_synchronizer_pid = fork();
	if (g_synchronizer_pid < 0) {
		return;
	} else if (0 == g_synchronizer_pid) {
		g_notify_stop = 0;
		signal(SIGTERM, supervisor_sigstop);
		signal(SIGALRM, supervisor_sigrestart);
		while (0 == g_notify_stop) {
			g_supervised_process = fork();
			if (0 == g_supervised_process) {
				chdir(ATHENA_MAIN_DIR);
				if (execve("./synchronizer", args, NULL) == -1) {
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
 *  start cdner service
 */
void start_cdner()
{
	int status;
	struct stat node_stat;
	char temp_path[256];
	char *args[] = {"./cdner", "../config/athena.cfg", NULL};

	sprintf(temp_path, "%s/cdner", ATHENA_MAIN_DIR);
	if (0 != stat(temp_path, &node_stat)) {
		g_cdner_pid = -1;
		return;
	}
	g_cdner_pid = fork();
	if (g_cdner_pid < 0) {
		return;
	} else if (0 == g_cdner_pid) {
		g_notify_stop = 0;
		signal(SIGTERM, supervisor_sigstop);
		signal(SIGALRM, supervisor_sigrestart);
		while (0 == g_notify_stop) {
			g_supervised_process = fork();
			if (0 == g_supervised_process) {
				chdir(ATHENA_MAIN_DIR);
				if (execve("./cdner", args, NULL) == -1) {
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
 *	start smtp and delivery services
 */
void start_service()
{
	time_t now_time;
	struct tm *ptm;
	pid_t pid, sid; /* process ID and session ID */
	int ctrl_id;
	int fd, shm_id;
	char *shm_begin;
	char str[16];
	key_t k_shm;
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

	start_monitor();
	start_supervisor();
	start_adaptor();
	start_synchronizer();
	start_cdner();
	
	k_shm = ftok(SESSION_TOKEN_FILE, TOKEN_SESSION);
	if (-1 != k_shm) {
		shm_id = shmget(k_shm, SHARE_MEMORY_SIZE, 0666);
		if (-1 == shm_id) {
			shm_id = shmget(k_shm, SHARE_MEMORY_SIZE, 0666|IPC_CREAT);
		}
		if (-1 != shm_id) {
			shm_begin = shmat(shm_id, NULL, 0);
			if (NULL != shm_begin) {
				memset(shm_begin, 0, SHARE_MEMORY_SIZE);
				shmdt(shm_begin);
			}
		}
	}


	k_ctrl = ftok(CONTROL_TOKEN_FILE, TOKEN_CONTROL);
	if (-1 == k_ctrl) {
		ctrl_id = -1;
	} else {
		ctrl_id = msgget(k_ctrl, 0666|IPC_CREAT);
	}
	
	while (0 == g_notify_stop) {
		if (-1 != ctrl_id && -1 != msgrcv(ctrl_id, &ctrl_type, 0, 0,
			IPC_NOWAIT)) {
			switch (ctrl_type) {
			case CTRL_RESTART_MONTOR:
				if (g_monitor_pid > 0) {
					kill(g_monitor_pid, SIGALRM);
				}
				break;
			case CTRL_RESTART_SUPERVISOR:
				if (g_supervisor_pid > 0) {
					kill(g_supervisor_pid, SIGALRM);
				}
				break;
			case CTRL_RESTART_WEBADAPTOR:
				if (g_adaptor_pid > 0) {
					kill(g_adaptor_pid, SIGALRM);
				}
				break;
			case CTRL_RESTART_SYNCHRONIZER:
				if (g_synchronizer_pid > 0) {
					kill(g_synchronizer_pid, SIGALRM);
				}
				break;
			case CTRL_RESTART_CDNER:
				if (g_cdner_pid > 0) {
					kill(g_cdner_pid, SIGALRM);
				}
				break;
			}
		}
		time(&now_time);
		ptm = localtime(&now_time);
		if (24*60*60 - ptm->tm_sec - 60*ptm->tm_min - 
			60*60*ptm->tm_hour < INTERVAL_BEFORE_MIDNIGHT) {
			start_analyzer();
			sleep(INTERVAL_BEFORE_MIDNIGHT);
		}
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
		printf("athena is not running\n");
		exit(EXIT_SUCCESS);
	}
	read(fd, str, 16);
	close(fd);
	if (0 == strcmp(str, "-1\n")) {
		printf("daemon analyzer is now running, please stop athena later\n");
		exit(EXIT_FAILURE);	
	}
	pid = atoi(str);
	if (0 == pid) {
		printf("fail to get athena's pid\n");
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
		printf("athena is not running\n");
		exit(EXIT_SUCCESS);
	}
	fd = open(PID_LOCK_FILE, O_RDONLY);
	read(fd, str, 16);
	close(fd);
	pid = atoi(str);
	if (0 == pid) {
		printf("athena is not running\n");
		exit(EXIT_SUCCESS);
	}
	sprintf(str, "ps -p %d > /dev/null", pid);
	if (0 == WEXITSTATUS(system(str))) {
		printf("athena (pid: %d) is running\n", pid);
	} else {
		printf("athena is not running\n");
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
		printf("athena is not running\n");
		exit(EXIT_FAILURE);
	}
	read(fd, str, 16);
	close(fd);
	pid = atoi(str);
	if (0 == pid) {
		printf("athena is not running\n");
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
		printf("cannot stop athena\n");
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
	sprintf(CONTROL_TOKEN_FILE, "%s/token/control.msg", argv[1]);
	sprintf(SESSION_TOKEN_FILE, "%s/token/session.shm", argv[1]);
	sprintf(ATHENA_MAIN_DIR, "%s/bin", argv[1]);
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

