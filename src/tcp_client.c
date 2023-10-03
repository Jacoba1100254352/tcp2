#include "tcp_client.h"
#include "log.h"
#include <string.h>
#include <unistd.h>
#include <netinet/in.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <getopt.h>

#define NUM_VALID_ACTIONS 5

extern int verbose_flag;  // External reference to the global verbose flag declared in main.c

static void printHelpOption(char *argv[]) {
    fprintf(stderr, "Usage: %s [--help] [-v] [-h HOST] [-p PORT] ACTION MESSAGE\n"
                    "\nArguments:\n"
                    "  ACTION   Must be uppercase, lowercase, reverse,\n"
                    "           shuffle, or random.\n"
                    "  MESSAGE  Message to send to the server\n"
                    "\nOptions:\n"
                    "\t--help\n"
                    "\t-v, --verbose\n"
                    "\t--host HOSTNAME, -h HOSTNAME\n"
                    "\t--port PORT, -p PORT\n", argv[0]);
}

// Parses the commandline arguments and options given to the program.
int tcp_client_parse_arguments(int argc, char *argv[], Config *config) {
    int opt;
    static struct option long_options[] = {
            {"help",    no_argument,       0, 0},
            {"host",    required_argument, 0, 'h'},
            {"port",    required_argument, 0, 'p'},
            {"verbose", no_argument,       0, 'v'},
            {0, 0,                         0, 0}
    };

    // Loop over all the options
    int long_index = 0;
    while ((opt = getopt_long(argc, argv, "h:p:v", long_options, &long_index)) != -1) {
        switch (opt) {
            case 0: // --help
                printHelpOption(argv);
                exit(EXIT_SUCCESS);
            case 'h':  // --host or -h
                config->host = optarg;
                break;
            case 'p':  // --port or -p
            {
                long port;
                char *endptr;
                port = strtol(optarg, &endptr, 10);
                if (*endptr != '\0' || port <= 0 || port > 65535) {
                    log_error("Invalid port number provided. Port must be a number between 1 and 65535.");
                    return EXIT_FAILURE;
                }
                config->port = optarg;
            }
                break;
            case 'v':  // --verbose or -v
                verbose_flag = 1;
                break;
            default: // An unrecognized option
                log_error("Invalid arguments provided.");
                return EXIT_FAILURE;
        }
    }

    // Update to parse file name instead of action and message
    if (optind < argc) config->file = argv[optind++];

    // Check if file argument is provided
    if (config->file == NULL) {
        log_error("Required argument not provided. Need FILE.");
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}


// Creates a TCP socket and connects it to the specified host and port.
int tcp_client_connect(Config config) {
    int sockfd;
    struct sockaddr_in server_address;
    struct hostent *server;

    if (verbose_flag)
        log_log(LOG_DEBUG, __FILE__, __LINE__, "Connecting to %s:%s", config.host, config.port);

    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0) {
        log_log(LOG_ERROR, __FILE__, __LINE__, "Could not create socket");
        return TCP_CLIENT_BAD_SOCKET;
    }

    server = gethostbyname(config.host);
    if (server == NULL) {
        log_log(LOG_ERROR, __FILE__, __LINE__, "No such host");
        return TCP_CLIENT_BAD_SOCKET;
    }

    memset(&server_address, 0, sizeof(server_address));
    server_address.sin_family = AF_INET;
    memcpy(&server_address.sin_addr.s_addr, server->h_addr, server->h_length);

    char *endptr; // Pointer to the end of the parsed string
    long port = strtol(config.port, &endptr, 10); // Convert string to long integer

    // Check if the entire string was parsed
    if (*endptr != '\0' || port <= 0 || port > 65535) {
        log_error("Invalid port number provided. Port must be a number between 1 and 65535.");
        return TCP_CLIENT_BAD_SOCKET; // or some other error handling
    }

    server_address.sin_port = htons((uint16_t) port); // Convert to appropriate type and byte order


    if (connect(sockfd, (struct sockaddr *) &server_address, sizeof(server_address)) < 0) {
        log_log(LOG_ERROR, __FILE__, __LINE__, "Could not connect");
        return TCP_CLIENT_BAD_SOCKET;
    }

    if (verbose_flag)
        log_log(LOG_DEBUG, __FILE__, __LINE__, "Connected to server!");

    return sockfd;  // Return the socket descriptor
}

// Creates and sends a request to the server using the socket and configuration.
int tcp_client_send_request(int sockfd, char *action, char *message) {
    // Initialize and display payload
    char payload[TCP_CLIENT_MAX_INPUT_SIZE];
    snprintf(payload, TCP_CLIENT_MAX_INPUT_SIZE, "%s %lu %s", action, strlen(message), message);

    // Inform of sending status
    if (verbose_flag)
        log_log(LOG_DEBUG, __FILE__, __LINE__, "Sending: %s %lu %s", action, strlen(message), message);

    //
    int payloadLength = (int) strlen(payload);
    ssize_t bytesSent, totalBytesSent = 0;

    // Send bytes to the server until done
    while (totalBytesSent < payloadLength) {
        bytesSent = send(sockfd, payload + totalBytesSent, payloadLength - totalBytesSent, 0);
        if (bytesSent == -1) {
            log_error("Sent failed");
            return EXIT_FAILURE;
        }
        totalBytesSent += bytesSent;
    }

    // Inform of bytes sent
    if (verbose_flag)
        log_log(LOG_DEBUG, __FILE__, __LINE__, "Bytes sent: %lu (%lu/%lu)", strlen(message), strlen(message), strlen(message));

    return EXIT_SUCCESS;
}

// Receives the response from the server. The caller must provide a callback function to handle the response.
int tcp_client_receive_response(int sockfd, int (*handle_response)(char *)) {
    char buf[TCP_CLIENT_MAX_INPUT_SIZE];
    ssize_t totalBytesReceived = 0;

    while (1) {
        // Receive data
        ssize_t bytesReadInCurrentIteration = recv(sockfd, buf + totalBytesReceived, sizeof(buf) - totalBytesReceived - 1, 0);
        if (bytesReadInCurrentIteration <= 0) {
            break;
        }
        totalBytesReceived += bytesReadInCurrentIteration;

        // Process any complete messages in the buffer
        char *ptr = buf;
        while (ptr - buf < totalBytesReceived) {
            char *endptr;
            long len = strtol(ptr, &endptr, 10);
            if (endptr == ptr || len <= 0) {
                break;  // Malformed message, break out and wait for more data
            }

            // Check if the entire message is in the buffer
            if (endptr + len > buf + totalBytesReceived) {
                break;  // Not a complete message, break out and wait for more data
            }

            // Process the message
            if (handle_response(endptr) != 0) {
                log_error("Handling response failed");
                return EXIT_FAILURE;
            }

            ptr = endptr + len;  // Move to the next message in the buffer
        }

        // If we processed some messages, shift the remaining data to the front of the buffer
        if (ptr != buf) {
            totalBytesReceived -= (ptr - buf);
            memmove(buf, ptr, totalBytesReceived);
        }
    }

    return EXIT_SUCCESS;
}


// Closes the given socket.
int tcp_client_close(int sockfd) {
    if (verbose_flag)
        log_log(LOG_DEBUG, __FILE__, __LINE__, "Reached?");

    if (close(sockfd) < 0) {
        log_error("Could not close socket");
        return EXIT_FAILURE;
    }

    if (verbose_flag)
        log_log(LOG_DEBUG, __FILE__, __LINE__, "Client socked closed");

    return EXIT_SUCCESS;
}

// Opens a file.
FILE *tcp_client_open_file(char *file_name) {
    FILE *fileData = fopen(file_name, "r");
    if (fileData == NULL)
        log_error("Could not open file");
    else if (verbose_flag)
        log_log(LOG_DEBUG, __FILE__, __LINE__, "File Opened");

    return fileData;
}

// Check if the provided action is valid
static int is_valid_action(const char *action) {
    static char *validActions[NUM_VALID_ACTIONS] = {
            "uppercase",
            "lowercase",
            "reverse",
            "shuffle",
            "random"
    };

    for (int i = 0; i < NUM_VALID_ACTIONS; i++)
        if (strcmp(action, validActions[i]) == 0)
            return EXIT_SUCCESS; // True: Valid action

    return EXIT_FAILURE; // False: Invalid action
}

// Gets the next line of a file, filling in action and message.
int tcp_client_get_line(FILE *fd, char **action, char **message) {
    if (fd == NULL || action == NULL || message == NULL)
        return EXIT_FAILURE;

    char *line = NULL;
    size_t len = 0;

    if (getline(&line, &len, fd) == -1) {
        free(line);
        return EXIT_FAILURE;
    }

    *action = strtok(line, " ");

    if (is_valid_action(*action) == EXIT_FAILURE) {
        log_error("Invalid Action provided: %s", *action);
        free(line);
        return EXIT_FAILURE;
    }

    *message = strtok(NULL, "\n");

    // Skip malformed lines
    if (*action == NULL || *message == NULL) {
        free(line);
        return EXIT_FAILURE;
    }

    // Duplicate the strings since we will free the line
    *action = strdup(*action);
    *message = strdup(*message);

    free(line);

    if (verbose_flag)
        log_log(LOG_DEBUG, __FILE__, __LINE__, "Action: %s, Message: %s", *action, *message);

    return EXIT_SUCCESS;
}


// Closes a file.
int tcp_client_close_file(FILE *fd) {
    if (fclose(fd) == EOF) {
        log_error("Could not close file");
        return EXIT_FAILURE;
    }

    if (verbose_flag)
        log_log(LOG_DEBUG, __FILE__, __LINE__, "File Closed");

    return EXIT_SUCCESS;
}
