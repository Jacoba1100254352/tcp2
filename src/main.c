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
    while (*ptr != '\0') { // Traverse until the end of the string
        // Extract the length of the response
        char *endptr;
        long len = strtol(ptr, &endptr, 10);
        if (endptr == ptr || len < 0) { // Check if there are no digits or the length is negative
            fprintf(stderr, "Malformed response received: %s\n", ptr);
            return EXIT_FAILURE;
        }

        // Move the pointer to the start of the response message
        ptr = endptr;
        if (*ptr == '\0' || (size_t)len > strlen(ptr)) {
            fprintf(stderr, "Incomplete response received: %s\n", ptr);
            return EXIT_FAILURE;
        }

        // Print the response message
        printf("%.*s\n", (int)len, ptr); // Print len characters from ptr

        // Move the pointer to the next response
        ptr += len;
    }

    return EXIT_SUCCESS;
}


int process_file(int sockfd, Config *config) {
    FILE *fp;
    if (strcmp(config->file, "-") == 0) {
        fp = stdin;
    } else {
        fp = tcp_client_open_file(config->file);
        if (fp == NULL) {
            log_error("Could not open file: %s", config->file);
            return EXIT_FAILURE;
        }
    }

    char *line = NULL;
    size_t len = 0;
    ssize_t read;
    while ((read = getline(&line, &len, fp)) != -1) {
        char *action = strtok(line, " ");
        char *message = strtok(NULL, "\n");

        // Skip malformed lines
        if (action == NULL || message == NULL) continue;

        // Send Request
        if (tcp_client_send_request(sockfd, action, message) != EXIT_SUCCESS) {
            log_error("Failed to send request for action: %s, message: %s", action, message);
            free(line);
            if (fp != stdin) tcp_client_close_file(fp);
            return EXIT_FAILURE;
        }
    }

    free(line);
    if (fp != stdin) tcp_client_close_file(fp);
    return EXIT_SUCCESS;
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
    if (sockfd == TCP_CLIENT_BAD_SOCKET) exit(EXIT_FAILURE);

    if (verbose_flag)
        log_log(LOG_INFO, __FILE__, __LINE__, "Connected to %s:%s", config.host, config.port);

    // Process the provided file or stdin and send requests
    if (process_file(sockfd, &config) != EXIT_SUCCESS) {
        // Handle Error
        tcp_client_close(sockfd);
        return EXIT_FAILURE;
    }

    if (tcp_client_receive_response(sockfd, handle_response)) {
        tcp_client_close(sockfd);
        exit(EXIT_FAILURE);
    }

    if (tcp_client_close(sockfd)) exit(EXIT_FAILURE);
    exit(EXIT_SUCCESS);
}
