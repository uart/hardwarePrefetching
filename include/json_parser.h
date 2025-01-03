#ifndef __JSON_PARSER_H
#define __JSON_PARSER_H


#define JSON_BUFFER_SIZE (1024)
#define JSON_MAX_ARG (50)

int json_init(char ***json_argv);
int json_parse(const char *filename, char *argv[], char *json_argv[]);
int json_deinit(char *json_argv[]);

#endif