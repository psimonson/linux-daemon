#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <syslog.h>
#include <signal.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/un.h>

#define DAEMON_NAME "mydaemon"
#define PID_FILE "/var/run/mydaemon.pid"
#define SOCKET_PATH "/tmp/mydaemon.sock"

// Signal handler for termination and reload signals
void signal_handler(int sig) {
    switch(sig) {
        case SIGHUP:
            syslog(LOG_INFO, "Received SIGHUP signal, reloading configuration.");
            // Add code here to reload configuration if needed
            break;
        case SIGTERM:
            syslog(LOG_INFO, "Received SIGTERM signal, exiting.");
            unlink(PID_FILE); // Remove the PID file
            unlink(SOCKET_PATH); // Remove the socket file
            exit(EXIT_SUCCESS);
            break;
        default:
            syslog(LOG_INFO, "Received unexpected signal.");
            break;
    }
}

// Function to send a signal to the daemon based on PID
void send_signal(int sig) {
    FILE *pid_file = fopen(PID_FILE, "r");
    if (!pid_file) {
        fprintf(stderr, "Daemon is not running.\n");
        exit(EXIT_FAILURE);
    }

    pid_t pid;
    if (fscanf(pid_file, "%d", &pid) != 1) {
        fprintf(stderr, "Failed to read PID from file.\n");
        fclose(pid_file);
        exit(EXIT_FAILURE);
    }
    fclose(pid_file);

    if (kill(pid, sig) == -1) {
        perror("Failed to send signal");
        exit(EXIT_FAILURE);
    }
}

// Function to start the daemon
void start_daemon() {
    pid_t pid, sid;
    int fd;

    // Fork the parent process
    pid = fork();
    if (pid < 0) {
        exit(EXIT_FAILURE);
    }
    if (pid > 0) {
        // Parent process, exit
        exit(EXIT_SUCCESS);
    }

    // Change the file mode mask
    umask(0);

    // Create a new SID for the child process
    sid = setsid();
    if (sid < 0) {
        exit(EXIT_FAILURE);
    }

    // Change the current working directory
    if ((chdir("/")) < 0) {
        exit(EXIT_FAILURE);
    }

    // Redirect standard files to /dev/null
    fd = open("/dev/null", O_RDWR);
    if (fd == -1) {
        exit(EXIT_FAILURE);
    }
    dup2(fd, STDIN_FILENO);
    dup2(fd, STDOUT_FILENO);
    dup2(fd, STDERR_FILENO);
    close(fd);

    // Write PID file
    fd = open(PID_FILE, O_RDWR | O_CREAT, 0600);
    if (fd == -1) {
        syslog(LOG_ERR, "Could not open PID file %s, exiting.", PID_FILE);
        exit(EXIT_FAILURE);
    }
    if (lockf(fd, F_TLOCK, 0) == -1) {
        syslog(LOG_ERR, "Could not lock PID file %s, exiting.", PID_FILE);
        exit(EXIT_FAILURE);
    }
    char str[10];
    sprintf(str, "%d\n", getpid());
    write(fd, str, strlen(str));
    close(fd);

    // Setup signal handling
    signal(SIGTERM, signal_handler);
    signal(SIGHUP, signal_handler);
}

// Handle client commands from the socket
void handle_client(int client_socket) {
    char buffer[256];
    int bytes_read = read(client_socket, buffer, sizeof(buffer) - 1);
    if (bytes_read > 0) {
        buffer[bytes_read] = '\0';
        syslog(LOG_INFO, "Received command: %s", buffer);
        if (strcmp(buffer, "status") == 0) {
            write(client_socket, "Daemon is running\n", 18);
        } else if (strcmp(buffer, "reload") == 0) {
            raise(SIGHUP); // Send SIGHUP signal to self to reload
            write(client_socket, "Daemon configuration reloaded\n", 30);
        } else {
            write(client_socket, "Unknown command\n", 16);
        }
    }
    close(client_socket);
}

// Main daemon process with socket handling
void daemon_process() {
    int server_socket, client_socket;
    struct sockaddr_un server_addr;

    // Create a UNIX domain socket
    server_socket = socket(AF_UNIX, SOCK_STREAM, 0);
    if (server_socket < 0) {
        syslog(LOG_ERR, "Failed to create socket");
        exit(EXIT_FAILURE);
    }

    // Bind the socket to a file path
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sun_family = AF_UNIX;
    strncpy(server_addr.sun_path, SOCKET_PATH, sizeof(server_addr.sun_path) - 1);
    unlink(SOCKET_PATH); // Remove existing socket file
    if (bind(server_socket, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        syslog(LOG_ERR, "Failed to bind socket");
        exit(EXIT_FAILURE);
    }

    // Listen for incoming connections
    if (listen(server_socket, 5) < 0) {
        syslog(LOG_ERR, "Failed to listen on socket");
        exit(EXIT_FAILURE);
    }

    syslog(LOG_INFO, "Daemon is running and listening on socket.");

    while (1) {
        client_socket = accept(server_socket, NULL, NULL);
        if (client_socket < 0) {
            syslog(LOG_ERR, "Failed to accept client connection");
            continue;
        }
        handle_client(client_socket);
    }
    close(server_socket);
}

int main(int argc, char *argv[]) {
	if (argc == 2) {
		if (!strcmp(argv[1], "reload")) {
			send_signal(SIGHUP);
			printf("Sent SIGHUP to daemon for reload.\n");
			exit(EXIT_SUCCESS);
		} else if (!strcmp(argv[1], "kill")) {
			send_signal(SIGTERM);
			printf("Sent SIGTERM to daemon to kill it.\n");
			exit(EXIT_SUCCESS);
		} else {
			// Invalid argument
			fprintf(stderr, "Unknown argument: %s\n", argv[1]);
			exit(EXIT_FAILURE);
		}
	} else if (argc != 1) {
		fprintf(stderr, "Usage: %s [reload|kill]\n", argv[0]);
		exit(EXIT_FAILURE);
	}

    // Open syslog
    openlog(DAEMON_NAME, LOG_PID | LOG_CONS, LOG_DAEMON);
    syslog(LOG_INFO, "Starting daemon.");

    // Start the daemon
    start_daemon();

    // Run the daemon process
    daemon_process();

    // Close syslog
    closelog();

    return EXIT_SUCCESS;
}

