#ifndef TCP_CLIENT_H_
#define TCP_CLIENT_H_

#include <arpa/inet.h>
#include <errno.h>
#include <getopt.h>
#include <netdb.h>
#include <netinet/in.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

#define TCP_CLIENT_BAD_SOCKET -1
#define TCP_CLIENT_DEFAULT_PORT "8081"
#define TCP_CLIENT_DEFAULT_HOST "lundrigan.byu.edu" //"localhost"
#define TCP_CLIENT_MAX_INPUT_SIZE 1024

/**
 * @brief Contains all of the information needed to connect to the server and send it a message.
 */
typedef struct Config {
    char *port;
    char *host;
    char *file;
} Config;

/**
 * @brief Parses the command-line arguments and options given to the program.
 *
 * @param argc The number of arguments provided to the program (provided by the main function).
 * @param argv The array of arguments provided to the program (provided by the main function).
 * @param config An empty Config struct that will be filled in by this function.
 * @return Returns 0 on success, 1 on failure.
 */
int tcp_client_parse_arguments(int argc, char *argv[], Config *config);

///////////////////////////////////////////////////////////////////////
/////////////////////// SOCKET RELATED FUNCTIONS //////////////////////
///////////////////////////////////////////////////////////////////////

/**
 * @brief Creates a TCP socket and connects it to the specified host and port.
 *
 * @param config A config struct with the necessary information.
 * @return Returns the socket file descriptor or TCP_CLIENT_BAD_SOCKET if an error occurs.
 */
int tcp_client_connect(Config config);

/**
 * @brief Creates and sends a request to the server using the socket and configuration.
 *
 * @param sockfd Socket file descriptor.
 * @param action The action that will be sent.
 * @param message The message that will be sent.
 * @return Returns 0 on success, 1 on failure.
 */
int tcp_client_send_request(int sockfd, char *action, char *message);

/**
 * @brief Receives the response from the server. The caller must provide a function pointer that handles
 * the response and returns a true value if all responses have been handled, otherwise it returns a false value.
 *
 * After the response is handled by the handle_response function pointer, the response data can be safely deleted.
 * The string passed to the function pointer must be null terminated.
 *
 * @param sockfd Socket file descriptor.
 * @param handle_response A callback function that handles a response.
 * @return Returns 0 on success, 1 on failure.
 */
int tcp_client_receive_response(int sockfd, int (*handle_response)(char *));

/**
 * @brief Closes the given socket.
 *
 * @param sockfd Socket file descriptor.
 * @return Returns 0 on success, 1 on failure.
 */
int tcp_client_close(int sockfd);

///////////////////////////////////////////////////////////////////////
//////////////////////// FILE RELATED FUNCTIONS ///////////////////////
///////////////////////////////////////////////////////////////////////

/**
 * @brief Opens a file.
 *
 * @param file_name The name of the file to open.
 * @return Returns a FILE pointer on success, NULL on failure.
 */
FILE *tcp_client_open_file(char *file_name);

/**
 * @brief Gets the next line of a file, filling in action and message.
 *
 * *action and message must be allocated by the function and freed by the caller.
 * When this function is called, action must point to the action string and message must point to the message string.
 *
 * @param fd The file pointer to read from.
 * @param action A pointer to the action that was read in.
 * @param message A pointer to the message that was read in.
 * @return Returns the number of characters read on success, -1 on failure.
 */
int tcp_client_get_line(FILE *fd, char **action, char **message);

/**
 * @brief Closes a file.
 *
 * @param fd The file pointer to close.
 * @return Returns 0 on success, 1 on failure.
 */
int tcp_client_close_file(FILE *fd);

#endif
