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

#include <mutex>
#include <thread>
#include <chrono> 
using namespace std;

// Hash size
const int MODULO = 239017;

// Base for polynomial hashing
const int P = 997;

// Hashmap
char hash_used[MODULO];
char *cached_value[MODULO];
int time_to_live[MODULO];

// Mutex array size
const int MUTEX_ARRAY_SIZE = 16;

// Mutex array
mutex mtx[MUTEX_ARRAY_SIZE];

// Global exit flag
int exit_flag;

// Start time
time_t start_time;

// Alloc memory and set dest = source
static void alloc_and_copy(char **dest, const char *source) {
	// Delete old value if necessary
	if (*dest != NULL) {
		free(*dest);
	}
	// Alloc memory
	*dest = (char*)malloc((strlen(source) + 1) * sizeof(char));
	// Copy to dest
	strcpy(*dest, source);
}

// alloc memory and set dest = source1 + delimeter + source2
static void alloc_and_concat(char **dest, const char * source1, const char * delimeter, const char *source2) {
	// Delete old value if necessary
	if (*dest != NULL) {
		free(*dest);
	}
	// Alloc memory
	*dest = (char*)malloc((strlen(source1) + strlen(delimeter) + strlen(source2) + 1) * sizeof(char));
	// Copy to dest
	int index = 0;
	for(int i = 0; source1[i]; i++) {
		(*dest)[index++] = source1[i];
	}
	for(int i = 0; delimeter[i]; i++) {
		(*dest)[index++] = delimeter[i];
	}
	for(int i = 0; source2[i]; i++) {
		(*dest)[index++] = source2[i];
	}
	(*dest)[index] = 0;
}

// Initialize cached values and auxiliary data
static void init_hashmap() {
	for(int i = 0; i < MODULO; i++){
		hash_used[i] = 0;
		cached_value[i] = NULL;
		time_to_live[i] = 0;
	}
}

static int get_mutexed_segment_size() {
	return (MODULO + MUTEX_ARRAY_SIZE - 1) / MUTEX_ARRAY_SIZE;
}

// Get index in mutex array by hash index
static int get_mutex_index(int hash) {
	return hash / get_mutexed_segment_size();
}

// Delete all cached values
static void clear_hashmap() {
	// Clear segments one by one
	int segment_size = get_mutexed_segment_size();
	for(int i = 0; i < MUTEX_ARRAY_SIZE; i++) {
		int l = i * segment_size;
		int r = min((i + 1) * segment_size, MODULO);

		// Lock mutex and clesar all values in corresponding segment
		//cout << "Clear hashmap - trying to lock mutex #" << i << "...\n";
		while(!mtx[i].try_lock());
		//cout << "Clear hashmap - mutex #" << i << " locked\n";
		for(int j = l; j < r; j++) {
			if (hash_used[j]) {
				free(cached_value[j]);
				cached_value[j] = NULL;		
			}
		}
		//cout << "Clear hashmap - trying to unlock mutex #" << i << "...\n";
		mtx[i].unlock();
		//cout << "Clear hashmap - mutex #" << i << " unlocked\n";
	}
}

// Get hash = (str[n - 1] + str[n - 2] * P + str[n - 3] * P^2 + ... + str[0] * P^(n-1)) % MODULO
static int get_hash(char * str) {
	int hash = 0;
	for(int i = 0; str[i]; i++) {
		hash = (hash * P + str[i]) % MODULO;	
	}
	return hash;
}

static void get_value(char * key, char **value, int *result) {
	int hash = get_hash(key);

	// Lock corresponding mutex
	int mutex_index = get_mutex_index(hash);
	//cout << "Get value - trying to lock mutex #" << mutex_index << "...\n";
	while(!mtx[mutex_index].try_lock());
	//cout << "Get value - mutex #" << mutex_index << " locked\n";

	// Check if key exists
	if (!hash_used[hash]) {
		*result = 0;

		// Unlock mutex
		//cout << "Get value - trying to unlock mutex #" << mutex_index << "...\n";
		mtx[mutex_index].unlock();
		//cout << "Get value - mutex #" << mutex_index << " unlocked\n";
		
		return;
	}

	// Check if time to live suits 
	time_t current_time = time(NULL);
	if (time_to_live[hash] < current_time) {
		// Delete old value and return 0
		hash_used[hash] = 0;
		free(cached_value[hash]);
		cached_value[hash] = NULL;
		time_to_live[hash] = 0;
		*result = 0;

		// Unlock mutex
		//cout << "Get value - trying to unlock mutex #" << mutex_index << "...\n";
		mtx[mutex_index].unlock();
		//cout << "Get value - mutex #" << mutex_index << " unlocked\n";		
		
		return;	
	}

	// Get value
	*result = 1;
	alloc_and_copy(value, cached_value[hash]);

	// Unlock mutex
	//cout << "Get value - trying to unlock mutex #" << mutex_index << "...\n";
	mtx[mutex_index].unlock();
	//cout << "Get value - mutex #" << mutex_index << " unlocked\n";
}

static void set_value(char * key, char *value, int ttl) {
	int hash = get_hash(key);

	// Lock corresponding mutex
	int mutex_index = get_mutex_index(hash);
	//cout << "Set value - trying to lock mutex #" << mutex_index << "...\n";
	while(!mtx[mutex_index].try_lock());
	//cout << "Set value - mutex #" << mutex_index << " locked\n";

	// Delete old value if necessary
	if (hash_used[hash]) {
		free(cached_value[hash]);
		cached_value[hash] = NULL;
	}

	// Set value
	hash_used[hash] = 1;
	alloc_and_copy(&cached_value[hash], value);
	time_t current_time = time(NULL);
	time_to_live[hash] = ((int)current_time) + ttl;

	// Unlock mutex
	//cout << "Set value - trying to unlock mutex #" << mutex_index << "...\n";
	mtx[mutex_index].unlock();
	//cout << "Set value - mutex #" << mutex_index << " unlocked\n";
}

// Set error code and error description
static void set_error(int *error_code, char **error_description, int error_code_value, const char *error_description_value) {
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
				set_error(error_code, error_description, 1, "Incorrect parameter names or ordering.");
				return;
			}
			(*offset)++;	
		}
		// Delimeter of param name and value should be '='
		if (params[(*offset)] != '=') {
			// Invalid delimeter - error code 2
			set_error(error_code, error_description, 2, "Incorrect parameter name and value delimeter.");
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
	int *param_ttl_value, int *error_code, char **error_description) {
	int offset = 0;
	
	get_parameter(request, &offset, "", param_type, error_code, error_description);
	// Type of request (GET | PUT | EXIT)
	if (!(strcmp(*param_type, "GET") == 0 || strcmp(*param_type, "PUT") == 0 || strcmp(*param_type, "EXIT") == 0)){
		set_error(error_code, error_description, 3, "Incorrect request type (must be GET, PUT or EXIT).");
		return;
	}
	if (strcmp(*param_type, "GET") == 0 || strcmp(*param_type, "PUT") == 0) {
		// Key
		get_parameter(request, &offset, "KEY", param_key, error_code, error_description);
		if (strcmp(*param_type, "PUT") == 0) {
			// Value
			get_parameter(request, &offset, "VALUE", param_value, error_code, error_description);
			// time to live
			get_parameter(request, &offset, "TTL", param_ttl, error_code, error_description);
		}
	}
	if ((*error_code) != 0) {
		return;
	}
	if (request[offset] != 0) {
		set_error(error_code, error_description, 4, "Extra symbols in request.");
		return;
	}
	*param_ttl_value = 0;
	if (strcmp(*param_type, "PUT") == 0) {
		for(int i = 0; (*param_ttl)[i]; i++) {
			if ((*param_ttl)[i] < '0' || (*param_ttl)[i] > '9') {
				set_error(error_code, error_description, 5, "Invalid ttl format.");
				return;
			}
			*param_ttl_value = *param_ttl_value * 10 + ((*param_ttl)[i] - '0');
		}
	}
	return;
}

static void finalize_request(char * param_type, char * param_key, char * param_value, char * param_ttl, 
	int error_code, char * error_description, char ** response_header, char ** response_description, char ** response) {
	if (error_code != 0) {
		alloc_and_copy(response_header, "ERROR");
		alloc_and_copy(response_description, error_description);	
	} else if (strcmp(param_type, "EXIT") == 0) {
		alloc_and_copy(response_header, "EXIT");
		alloc_and_copy(response_description, "OK");
	} else {
		//alloc_and_copy(response_header, param_type);
		//alloc_and_copy(response_description, "OK");
	}

	alloc_and_concat(response, *response_header, "|", *response_description);

	free(param_type);
	free(param_key);
	free(param_value);
	free(param_ttl);
	free(error_description);

	free(*response_header);
	free(*response_description);
}

static void process_request(char * request, char ** response) {
	// Just for testing
	std::this_thread::sleep_for(std::chrono::seconds(1));

	// Set error fields
	int error_code = 0;
	char * error_description = (char *)malloc(4096 * sizeof(char));
	
	// Parse request
	char * param_type = NULL, * param_key = NULL, * param_value = NULL, * param_ttl = NULL;
	int ttl = 0;
	char * response_header = NULL, * response_description = NULL;
	parse_request(request, &param_type, &param_key, &param_value, &param_ttl, &ttl, &error_code, &error_description);
	if (error_code != 0) {
		finalize_request(param_type, param_key, param_value, param_ttl, 
			error_code, error_description, &response_header, &response_description, response);
		return;
	}
	// Special exit request (just for finishng work)
	if (strcmp(param_type, "EXIT") == 0) {
		exit_flag = 1;
		finalize_request(param_type, param_key, param_value, param_ttl, 
			error_code, error_description, &response_header, &response_description, response);
		return;
	}
	
	if (strcmp(param_type, "GET") == 0) {
		int result = 0;
		get_value(param_key, &param_value, &result);
		if (result == 0) {
			alloc_and_copy(&response_header, "GET FAILURE");
			alloc_and_copy(&response_description, "Value does not exist.");
		} else {
			alloc_and_copy(&response_header, "GET SUCCESS");
			alloc_and_copy(&response_description, param_value);
		}
	} else if (strcmp(param_type, "PUT") == 0) {
		set_value(param_key, param_value, ttl);
		alloc_and_copy(&response_header, "PUT SUCCESS");
		alloc_and_copy(&response_description, "Value successfully saved.");
	}

	finalize_request(param_type, param_key, param_value, param_ttl, 
		error_code, error_description, &response_header, &response_description, response);
	return;
}

/*static void process_request_in_thread(string tmp) {
	char * request = (char *)malloc((tmp.length() + 1) * sizeof(char));
	for(int i = 0; i < (int)tmp.length(); i++) {
		request[i] = tmp[i];
	}
	request[tmp.length()] = 0;

	//printf("%s\n", request);

	char * response = NULL;

	process_request(request, &response);

	printf("%s\n", request);
	printf("%s\n", response);

	free(request);
	free(response);
}

static void finalize_all() {
	std::this_thread::sleep_for(std::chrono::seconds(30));
	clear_hashmap();
	exit(0);
}*/

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

	printf("we got some data: %s\n", data);

	char *response = NULL;
	
	
	process_request(data, &response);

	/* Copy all the data from the input buffer to the output buffer. */
	//evbuffer_add_buffer(output, input);
	evbuffer_add(output, response, strlen(response));
	free(data);
	free(response);

	if (exit_flag) {
		clear_hashmap();
		exit(0);
	}
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
	// MY CODE BEGIN
	// Init hashmap
	init_hashmap();

	// Refresh global exit flag	
	exit_flag = 0;
	// MY CODE END

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
