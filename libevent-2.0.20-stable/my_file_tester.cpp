#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <string>
#include <vector>
#include <unordered_map>
#include <iostream>
using namespace std;

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
	char * error_description = (char *)malloc(4096 * sizeof(char));
	error_description[0] = 0;

	// Parse request
	char * param_type = NULL, * param_key = NULL, * param_value = NULL, * param_ttl = NULL;
	parse_request(request, &param_type, &param_key, &param_value, &param_ttl, &error_code, &error_description);
	if (error_code != 0) {
		finalize_request(param_type, param_key, param_value, param_ttl, error_description, response);
		return;
	}
	// TODO
	finalize_request(param_type, param_key, param_value, param_ttl, error_description, response);
	return;
}

int main() {
	freopen("my_input.txt", "r", stdin);
	freopen("my_output.txt", "w", stdout);
	while(1) {
		string tmp;
		getline(cin, tmp);
		if (tmp == "") {
			break;		
		}
		char * request = (char *)malloc(tmp.length() + 1);
		for(int i = 0; i < (int)tmp.length(); i++) {
			request[i] = tmp[i];
		}
		request[tmp.length()] = 0;
		char * response = NULL;
		process_request(request, &response);
		printf("%s", response);
		free(request);
		free(response);
	}
}
