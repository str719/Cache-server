#include <event2/listener.h>
#include <event2/bufferevent.h>
#include <event2/buffer.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#define UNIX_SOCK_PATH "/tmp/mysocket"

void set_error(int *error_code, char **error_description, int error_code_value, const char *error_description_value) {
	*error_code = error_code_value;
	strcpy(*error_description, error_description_value);
}

static void get_parameter(char * params, int *offset, 
	const char* expected_param_name, char **param_value, int *error_code, char **error_description) {
	// Already error - do nothing
	if ((*error_code) != 0) {
		return;
	}

	//printf("%s: ", expected_param_name);

	// Non-empty expected parameter name - process the name
	if (expected_param_name[0] != 0) {
		// Check parameter name	
		for(int i = 0; expected_param_name[i]; i++) {
			if (params[(*offset)] == 0 || params[(*offset)] != expected_param_name[i]) {
				// Invalid parameter name - error code 1
				set_error(error_code, error_description, 1, "Error - incorrect parameter names or ordering.\n");
				return;
			}
			(*offset)++;	
		}
		// Delimeter of param name and value should be '='
		if (params[(*offset)] != '=') {
			// Invalid delimeter - error code 2
			set_error(error_code, error_description, 2, "Error - incorrect parameter name and value delimeter.\n");
			return;
		}
		(*offset)++;
	}

	// Find the length of parameter value (from offset to delimeter
	// which is symbol with code less or equal to 32 (space, EOL, EOF, \t, ...))
	int param_value_length;
	for(param_value_length = 0; params[(*offset) + param_value_length] > 32; param_value_length++);
	// Set param value
	*param_value = (char *)malloc(param_value_length + 1);
	for(int i = 0; i < param_value_length; i++) {
		(*param_value)[i] = params[(*offset) + i];	
	}
	(*param_value)[param_value_length] = 0;
	(*offset) += param_value_length;
	// Process additional symbol if necessary
	if (params[(*offset)] != 0 && params[(*offset)] != EOF) {
		(*offset)++;
	}

	//printf("%s\n", *param_value);
	return;
}

static void parse_request(char * request, char ** param_type, char ** param_key, char ** param_value, char ** param_ttl, 
	int *error_code, char **error_description) {
	int offset = 0;
	// Type of request (GET | PUT)
	get_parameter(request, &offset, "", param_type, error_code, error_description);
	if (!(strcmp(*param_type, "GET") == 0 || strcmp(*param_type, "PUT") == 0)){
		set_error(error_code, error_description, 3, "Error - incorrect request type (must be GET or PUT).\n");
		return;
	}
	// Key
	get_parameter(request, &offset, "KEY", param_key, error_code, error_description);
	if (strcmp(*param_type, "PUT") == 0) {
		// Value
		get_parameter(request, &offset, "VALUE", param_value, error_code, error_description);
		// time to live
		get_parameter(request, &offset, "TTL", param_ttl, error_code, error_description);
		
	}
	if ((*error_code) != 0) {
		return;
	}
	if (request[offset] != 0) {
		set_error(error_code, error_description, 4, "Error - extra symbols in request.\n");
		return;
	}
	return;
}

static void finalize_request(char * param_type, char * param_key, char * param_value, char * param_ttl, 
	char * error_description, char ** response) {
	if (error_description[0] != 0) {
		(*response) = (char *)malloc(strlen(error_description) * sizeof(char));
		strcpy(*response, error_description);	
	} else {
		(*response) = (char *)malloc(4 * sizeof(char));
		strcpy(*response, "OK\n");
	}

	free(param_type);
	free(param_key);
	free(param_value);
	free(param_ttl);
	free(error_description);
}

static void process_request(char * request, char ** response) {
	// Set error fields
	int error_code = 0;
	// 4096 - maximal possible error description size
	char * error_description = (char *)malloc(4096 * sizeof(char));
	error_description[0] = 0;

	// Parse request
	char * param_type = NULL, * param_key = NULL, * param_value = NULL, * param_ttl = NULL;
	parse_request(request, &param_type, &param_key, &param_value, &param_ttl, &error_code, &error_description);
	if (error_code != 0) {
		finalize_request(param_type, param_key, param_value, param_ttl, error_description, response);
		return;
	}
	// TODO - process request after parsing parameters

	finalize_request(param_type, param_key, param_value, param_ttl, error_description, response);
	return;
}

static void echo_read_cb(struct bufferevent *bev, void *ctx) {
	/* This callback is invoked when there is data to read on bev. */
	struct evbuffer *input = bufferevent_get_input(bev);
	struct evbuffer *output = bufferevent_get_output(bev);
	size_t len = evbuffer_get_length(input);
	char *data;
	//data = malloc(len);
	data = (char*)malloc(len + 1);
	data[len] = 0;
	evbuffer_copyout(input, data, len);

	char *response = NULL;

	printf("we got some data: %s\n", data);
	process_request(data, &response);

	/* Copy all the data from the input buffer to the output buffer. */
	//evbuffer_add_buffer(output, input);
	evbuffer_add(output, response, strlen(response));
	free(data);
	free(response);
}

static void echo_event_cb(struct bufferevent *bev, short events, void *ctx) {
	if (events & BEV_EVENT_ERROR)
		perror("Error from bufferevent");
	if (events & (BEV_EVENT_EOF | BEV_EVENT_ERROR)) {
		bufferevent_free(bev);
	}
}

static void
accept_conn_cb(struct evconnlistener *listener, evutil_socket_t fd, struct sockaddr *address, int socklen, void *ctx) {
	/* We got a new connection! Set up a bufferevent for it. */
	struct event_base *base = evconnlistener_get_base(listener);
	struct bufferevent *bev = bufferevent_socket_new(base, fd, BEV_OPT_CLOSE_ON_FREE);
	bufferevent_setcb(bev, echo_read_cb, NULL, echo_event_cb, NULL);

	bufferevent_enable(bev, EV_READ | EV_WRITE);
}

static void accept_error_cb(struct evconnlistener *listener, void *ctx) {
	struct event_base *base = evconnlistener_get_base(listener);
	int err = EVUTIL_SOCKET_ERROR();
	fprintf(stderr, "Got an error %d (%s) on the listener. "
		"Shutting down.\n", err, evutil_socket_error_to_string(err));

	event_base_loopexit(base, NULL);
}

int main(int argc, char **argv) {
	struct event_base *base;
	struct evconnlistener *listener;
	struct sockaddr_un sin;
	/* Create new event base */
	base = event_base_new();
	if (!base) {
		puts("Couldn't open event base");
		return 1;
	}

	/* Clear the sockaddr before using it, in case there are extra
	 * platform-specific fields that can mess us up. */
	memset(&sin, 0, sizeof(sin));
	sin.sun_family = AF_LOCAL;
	strcpy(sin.sun_path, UNIX_SOCK_PATH);

	/* Create a new listener */
	listener = evconnlistener_new_bind(base, accept_conn_cb, NULL,
                                       LEV_OPT_CLOSE_ON_FREE | LEV_OPT_REUSEABLE, -1,
                                       (struct sockaddr *) &sin, sizeof(sin));
	if (!listener) {
		perror("Couldn't create listener");
		return 1;
	}
	evconnlistener_set_error_cb(listener, accept_error_cb);

	/* Lets rock */
	event_base_dispatch(base);
	return 0;
}
