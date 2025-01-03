#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "cJSON.h"
#include "log.h"
#include "json_parser.h"

#define TAG "JSON_PARSER"

//This dynamically allocates memory to *json_argv[]
int json_init(char ***json_argv)
{
	*json_argv = malloc(sizeof(char *) * JSON_MAX_ARG);
	if (*json_argv == NULL) {
		loge(TAG, "Failed to allocate memory for json_argv\n");
		return -1;
	}

	memset(*json_argv, 0, sizeof(char *) * JSON_MAX_ARG);
	return 0;
}

//this parses all the keys and values config,json to be used in json_argv
int json_parse(const char *filename, char *argv[], char *json_argv[])
{
	FILE *fp;
	cJSON *json = NULL;
	char buffer[JSON_BUFFER_SIZE];
	int i = 0;
	int json_argc = 0;

	// Open the JSON file
	fp = fopen(filename, "r");
	if (fp == NULL) {
		loge(TAG, "Unable to open the file.\n");
		return -1;
	}

	// Read the file contents into a buffer
	size_t bytes_read = fread(buffer, 1, sizeof(buffer) - 1, fp);
	buffer[bytes_read] = '\0'; // Null-terminate the buffer
	fclose(fp);

	// Parse the JSON data
	json = cJSON_Parse(buffer);
	if (json == NULL) {
		const char *error_ptr = cJSON_GetErrorPtr();
		if (error_ptr != NULL) {
			loge(TAG, "Error: %s\n", error_ptr);
		}

		return -1;
	}

        // Allocate memory for the program name in json_argv
        json_argv[0] = malloc(JSON_BUFFER_SIZE);
        if (json_argv[0] == NULL) {
                cJSON_Delete(json);
                return -1;
        }
	
	snprintf(json_argv[0], JSON_BUFFER_SIZE, "%s", argv[0]);

	// Iterate through the JSON object and extract keys and values
	cJSON *item = NULL;
	cJSON_ArrayForEach(item, json) { // Correct iteration macro for cJSON

		if (i >= JSON_MAX_ARG - 1) {
			break;
		}

		// Allocate memory for the key and value
		json_argv[2 * i + 1] = malloc(JSON_BUFFER_SIZE);
		json_argv[2 * i + 2] = malloc(JSON_BUFFER_SIZE);
		if (json_argv[2 * i + 1] == NULL || json_argv[2 * i + 2] ==
							NULL) {

			cJSON_Delete(json);

			return -1;
		}

		// Copy key and value into json_argv
		snprintf(json_argv[2 * i + 1], JSON_BUFFER_SIZE, "%s",
			 cJSON_GetObjectItem(json, item->string)->string);

		if (cJSON_IsString(item)) {
			// Remove quotes from string values
			char *value = item->valuestring;
			if (value[0] == '\"' && value[strlen(value) - 1] ==
						    '\"') {
				value[strlen(value) - 1] = '\0';
				value++;
			}
			snprintf(json_argv[2 * i + 2], JSON_BUFFER_SIZE, "%s",
				 value);
		} else if (cJSON_IsNumber(item)) {
			// Format numbers as strings
			snprintf(json_argv[2 * i + 2], JSON_BUFFER_SIZE, "%g",
				 item->valuedouble);

		} else if (cJSON_IsBool(item)) {
			// Format boolean values as "true" or "false"
			snprintf(json_argv[2 * i + 2], JSON_BUFFER_SIZE, "%s",
				 item->valueint ? "true" : "false");

		} else if (cJSON_IsArray(item)) {
			// Format arrays as comma-separated strings without
			// brackets
			char *array_str = cJSON_Print(item);

			if (array_str != NULL) {
				// Remove brackets from array strings
				// Skip the opening bracket
				char *src = array_str + 1;

				char *dst = json_argv[2 * i + 2];

				while (*src && dst - json_argv[2 * i + 2] < JSON_BUFFER_SIZE - 1) {
					if (*src != '[' && *src != ']') {
						*dst++ = *src;
					}
					src++;
				}

				*dst = '\0'; // Ensure null-termination
				free(array_str);
			}

		} else {
			// For other types, use cJSON_Print to format the value
			char *value_str = cJSON_Print(item);

			if (value_str != NULL) {
				snprintf(json_argv[2 * i + 2],
					 JSON_BUFFER_SIZE, "%s", value_str);
				free(value_str);
			}
		}

		i++;
	}

	// Null-terminate the argument list
	json_argv[2 * i + 1] = NULL;

	// Delete the JSON object
	cJSON_Delete(json);

	json_argc = 2 * i + 1;

	return json_argc;
}

int json_deinit(char *json_argv[])
{
	// this frees the inidividual strings within the json_argv array
	if (json_argv != NULL) {
		for (int i = 0; json_argv[i] != NULL; i++) {
			free(json_argv[i]);
		}
		// this frees the initially dynamically allocated json_argv
		free(json_argv);
	}

	return 0;
}