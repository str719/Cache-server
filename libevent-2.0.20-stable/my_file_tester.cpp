#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <string>
#include <vector>
#include <unordered_map>
#include <iostream>
using namespace std;

// Hash size
const int MODULO = 239017;

// Base for polynomial hashing
const int P = 997;

// Hashmap
char hash_used[MODULO];
char *cached_value[MODULO];
int time_to_live[MODULO];

static void alloc_and_copy(char **dest, const char *source) {
	if (*dest != NULL) {
		free(*dest);
	}
	*dest = (char*)malloc((strlen(source) + 1) * sizeof(char));
	strcpy(*dest, source);
}

static void init_hashmap() {
	for(int i = 0; i < MODULO; i++){
		hash_used[i] = 0;
		cached_value[i] = NULL;
		time_to_live[i] = 0;
	}
}

static void clear_hashmap() {
	for(int i = 0; i < MODULO; i++){
		if (hash_used[i]) {
			free(cached_value[i]);		
		}
	}
}

static int get_hash(char * str) {
	int hash = 0;
	for(int i = 0; str[i]; i++) {
		hash = (hash * P + str[i]) % MODULO;	
	}
	return hash;
}

static void get_value(char * key, char **value, int *result) {
	// TODO - add time_to_live
	int hash = get_hash(key);
	if (!hash_used[hash]) {
		*result = 0;
		return;
	}
	*result = 1;
	alloc_and_copy(value, cached_value[hash]);
}

static void set_value(char * key, char *value, int ttl) {
	// TODO - add time_to_live
	// TODO - add mutex
	int hash = get_hash(key);
	if (hash_used[hash]) {
		free(cached_value[hash]);
	}
	hash_used[hash] = 1;
	alloc_and_copy(&cached_value[hash], value);
}

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
	int error_code, char * error_description, char ** response_header, char ** response_description, int exit_flag) {
	if (error_code != 0) {
		alloc_and_copy(response_header, "ERROR");
		alloc_and_copy(response_description, error_description);	
	} else if (exit_flag) {
		alloc_and_copy(response_header, "EXIT");
		alloc_and_copy(response_description, "OK");
	} else {
		//alloc_and_copy(response_header, param_type);
		//alloc_and_copy(response_description, "OK");
	}

	free(param_type);
	free(param_key);
	free(param_value);
	free(param_ttl);
	free(error_description);
}

static void process_request(char * request, char ** response_header, char ** response_description, int *exit_flag) {
	// Set error fields
	int error_code = 0;
	char * error_description = (char *)malloc(4096 * sizeof(char));
	
	// Parse request
	char * param_type = NULL, * param_key = NULL, * param_value = NULL, * param_ttl = NULL;
	int ttl = 0;
	parse_request(request, &param_type, &param_key, &param_value, &param_ttl, &ttl, &error_code, &error_description);
	if (error_code != 0) {
		finalize_request(param_type, param_key, param_value, param_ttl, 
			error_code, error_description, response_header, response_description, *exit_flag);
		return;
	}
	// Special exit request (just for finishng work)
	*exit_flag = 0;
	if (strcmp(param_type, "EXIT") == 0) {
		*exit_flag = 1;
		finalize_request(param_type, param_key, param_value, param_ttl, 
			error_code, error_description, response_header, response_description, *exit_flag);
		return;
	}
	
	if (strcmp(param_type, "GET") == 0) {
		int result = 0;
		get_value(param_key, &param_value, &result);
		if (result == 0) {
			alloc_and_copy(response_header, "GET FAILURE");
			alloc_and_copy(response_description, "Value does not exist.");
		} else {
			alloc_and_copy(response_header, "GET SUCCESS");
			alloc_and_copy(response_description, param_value);
		}
	} else if (strcmp(param_type, "PUT") == 0) {
		set_value(param_key, param_value, ttl);
		alloc_and_copy(response_header, "PUT SUCCESS");
		alloc_and_copy(response_description, "Value successfully saved.");
	}

	finalize_request(param_type, param_key, param_value, param_ttl, 
		error_code, error_description, response_header, response_description, *exit_flag);
	return;
}

int main() {
	freopen("my_input.txt", "r", stdin);
	freopen("my_output.txt", "w", stdout);

	init_hashmap();

	while(1) {
		string tmp;
		getline(cin, tmp);
		if (tmp == "") {
			break;		
		}
		char * request = (char *)malloc((tmp.length() + 1) * sizeof(char));
		for(int i = 0; i < (int)tmp.length(); i++) {
			request[i] = tmp[i];
		}
		request[tmp.length()] = 0;
		char * response_header = NULL;
		char * response_description = NULL;
		int exit_flag = 0;
		process_request(request, &response_header, &response_description, &exit_flag);
		printf("%s - %s\n", response_header, response_description);
		free(request);
		free(response_header);
		free(response_description);
		if (exit_flag) {
			clear_hashmap();
			exit(0);
		}
	}
}
