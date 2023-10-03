#include "tcp_client.h"
#include "log.h"

int verbose_flag = 0;  // Global variable for the verbose flag

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
    char *ptr = response;

    if (verbose_flag)
        log_log(LOG_DEBUG, __FILE__, __LINE__, "Response received: %s", response);

    while (*ptr != '\0') { // Traverse until the end of the string
        // Extract the length of the response
        char *endptr;
        long len = strtol(ptr, &endptr, 10);

        // Move the pointer to the start of the response message
        ptr = endptr;
        if (*ptr == '\0' || (size_t) len > strlen(ptr)) {
            fprintf(stderr, "Incomplete response received: %s\n", ptr);
            return EXIT_FAILURE;
        }

        // Print the response message
        printf("%.*s\n", (int) len, ptr); // Print len characters from ptr

        // Move the pointer to the next response
        ptr += len;
    }

    return EXIT_SUCCESS;
}

int main(int argc, char *argv[]) {
    printf("%s\n%s\n", argv[0], argv[1]);
    Config config = {
            .host = TCP_CLIENT_DEFAULT_HOST,
            .port = TCP_CLIENT_DEFAULT_PORT
    };
    if (tcp_client_parse_arguments(argc, argv, &config)) {
        printHelpOption(argv);
        exit(EXIT_FAILURE);
    }

    int sockfd = tcp_client_connect(config);
    if (sockfd == TCP_CLIENT_BAD_SOCKET) exit(EXIT_FAILURE);

    if (verbose_flag)
        log_log(LOG_DEBUG, __FILE__, __LINE__, "Connected to %s:%s", config.host, config.port);


    // Open the specified file or use stdin
    FILE *fp;
    if (strcmp(config.file, "-") == 0)
        fp = stdin;
    else if (!(fp = tcp_client_open_file(config.file))) {
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
        free(action);
        free(message);
    }

    if (fp != stdin)
        tcp_client_close_file(fp);

    exit((tcp_client_receive_response(sockfd, handle_response) || tcp_client_close(sockfd)) ? EXIT_FAILURE : EXIT_SUCCESS);
}

