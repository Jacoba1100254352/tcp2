#include "tcp_client.h"
#include "log.h"
#include <ctype.h>

int verbose_flag = 0;  // Global variable for the verbose flag

// Global counters for sent and received messages
static size_t messages_sent = 0;
static size_t messages_received = 0;

// Function to print usage instructions when --help is used or invalid arguments are provided
static void printHelpOption(char *argv[]) {
    fprintf(stderr, "Usage: %s [--help] [-v] [-h HOST] [-p PORT] FILE\n"
                    "\nArguments:\n"
                    "  FILE   A file name containing actions and messages to\n"
                    "         send to the server. If \"-\" is provided, stdin will\n"
                    "         be read.\n"
                    "\nOptions:\n"
                    "  --help\n"
                    "  -v, --verbose\n"
                    "  --host HOSTNAME, -h HOSTNAME\n"
                    "  --port PORT, -p PORT\n", argv[0]);
}

int handle_response(char *response) {
    printf("%s\n", response);
    messages_received++;
    if(messages_sent > messages_received)
        return EXIT_SUCCESS;
    else return EXIT_FAILURE;
}

int main(int argc, char *argv[]) {
    Config config = {
            .host = TCP_CLIENT_DEFAULT_HOST,
            .port = TCP_CLIENT_DEFAULT_PORT
    };

    if (tcp_client_parse_arguments(argc, argv, &config)) {
        printHelpOption(argv);
        exit(EXIT_FAILURE);
    }

    int sockfd = tcp_client_connect(config);
    if (sockfd == TCP_CLIENT_BAD_SOCKET) {
        log_warn("Unable to connect to a socket, exiting program");
        exit(EXIT_FAILURE);
    }

    if (verbose_flag)
        log_log(LOG_DEBUG, __FILE__, __LINE__, "Connected to %s:%s", config.host, config.port);

    FILE *fp;
    if (strcmp(config.file, "-") == 0) {
        fp = stdin;
    } else if (!(fp = tcp_client_open_file(config.file))) {
        log_error("There was an error trying to open the file.");
        tcp_client_close(sockfd);
        return EXIT_FAILURE;
    }

    char *action = NULL;
    char *message = NULL;
    while (tcp_client_get_line(fp, &action, &message) == EXIT_SUCCESS) {
        if (tcp_client_send_request(sockfd, action, message) != EXIT_SUCCESS) {
            free(action);
            free(message);
            if (fp != stdin) tcp_client_close_file(fp);
            tcp_client_close(sockfd);
            return EXIT_FAILURE;
        }
        messages_sent++;
        free(action);
        free(message);
    }

    log_info("Messages sent: %zu, messages received: %zu.", messages_sent, messages_received);

    if (fp != stdin)
        tcp_client_close_file(fp);

    exit((tcp_client_receive_response(sockfd, handle_response) || tcp_client_close(sockfd)) ? EXIT_FAILURE : EXIT_SUCCESS);
}
