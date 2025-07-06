#define _XOPEN_SOURCE 700

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
#include "header.h"


extern char home_path[PATH_MAX];

int load_config(const char *config_path, struct config *conf){
	int fd;
	if ((fd = open(config_path, O_RDONLY)) < 0){
		return -1;
	}
	// file lock
	if (lock_file(fd, F_RDLCK) < 0){
		close(fd);
		return -1;
	}

	FILE *fp = fdopen(fd, "r");
	char buf[BUFFER_SIZE];
	while (fgets(buf, sizeof(buf), fp)){
		int cnt;
		char **list;

		if (!(list = divide_line(buf, &cnt, ":, \n"))) 
			continue;

		if (!strcmp(list[0], "monitoring_path")){
			strcpy(conf->monitoring_path, list[1]);
		}
		else if (!strcmp(list[0], "pid")){
			conf->pid = atoi(list[1]);
		}
		else if (!strcmp(list[0], "start_time")){
			char datetime[32];
			if (snprintf(datetime, sizeof(datetime), "%s %s:%s:%s", list[1], list[2], list[3], list[4]) >= sizeof(datetime)){
				return -1;
			}
			struct tm tm_time;
			memset(&tm_time, 0, sizeof(struct tm));
			if (strptime(datetime, "%Y-%m-%d %H:%M:%S", &tm_time) == NULL){
				return -1;
			}
			conf->start_time = mktime(&tm_time);
		}
		else if (!strcmp(list[0], "output_path")){
			strcpy(conf->output_path, list[1]);
		}
		else if (!strcmp(list[0], "time_interval")){
			conf->time_interval = (time_t)atoi(list[1]);
		}
		else if (!strcmp(list[0], "max_log_lines")){
			if (!strcmp(list[1], "none"))
				conf->max_log_lines=-1;
			else
				conf->max_log_lines=atoi(list[1]);
		}
		else if (!strcmp(list[0], "exclude_path")){
			if (!strcmp(list[1], "none")){
				conf->exclude_path=NULL;
			}
			else {
				conf->exclude_path = (char **)calloc((size_t)cnt, sizeof(char *));
				for (int i=1; i<cnt; i++){
					conf->exclude_path[i-1] = strdup(list[i]);
				}
				conf->exclude_path[cnt-1] = NULL;
			}
		}
		else if (!strcmp(list[0], "extension")){
			if (!strcmp(list[1], "all"))
				conf->extension = NULL;
			else {
				conf->extension=(char **)calloc((size_t)cnt, sizeof(char *));
				for (int i=1; i<cnt; i++){
					conf->extension[i-1] = strdup(list[i]);
				}
				conf->extension[cnt-1] = NULL;
			}
		}
		else if (!strcmp(list[0], "mode")){
			conf->mode = atoi(list[1]);
		}
		free(list);
		continue;
	}
	fclose(fp); // fd도 같이 close
	return 0;
}
int init_config(struct config *conf, int argc, char **argv){
	// monitoring path
	char monitoring_path[PATH_MAX];
	if (snprintf(monitoring_path, (size_t)PATH_MAX, "%s", argv[1]) >= (size_t)PATH_MAX){
		fprintf(stderr, "Error: Path too long\n");
		return -1;
	}
	if (to_realpath(monitoring_path) < 0){
		return -1;
	}

	// output path
	char output_path[PATH_MAX];
	if (snprintf(output_path, (size_t)PATH_MAX, "%s_arranged", monitoring_path) >= (size_t)PATH_MAX){
		fprintf(stderr, "Error: Path too long\n");
		return -1;
	}
	if (to_realpath(monitoring_path) < 0){
		return -1;
	}

	// default
	strcpy(conf->monitoring_path, monitoring_path);
	conf->pid = (pid_t)0;
	conf->start_time = (time_t) 0;
	strcpy(conf->output_path, output_path);
	conf->time_interval = (time_t)10;
	conf->max_log_lines = -1;
	conf->exclude_path = NULL;
	conf->extension = NULL;
	conf->mode = 1;

	return 0;
}
void free_config(struct config *conf){
	if (conf->exclude_path != NULL){
		for (int i=0; conf->exclude_path[i]!=NULL; i++){
			free(conf->exclude_path[i]);
		}
		free(conf->exclude_path);
		conf->exclude_path=NULL;
	}
	if (conf->extension != NULL){
		for (int i=0; conf->extension[i]!=NULL ; i++){
			free(conf->extension[i]);
		}
		free(conf->extension);
		conf->extension=NULL;
	}
}
int lock_file(int fd, int type){
	struct flock fl;
	fl.l_type = type; // F_RDLCK or F_WRLCK
	fl.l_whence = SEEK_SET;
	fl.l_start = 0;
	fl.l_len = 0;

	return fcntl(fd, F_SETLKW, &fl);
}
int make_file(const char *file_path){
	int fd;
	if ((fd = open(file_path, O_WRONLY | O_CREAT, 0644)) < 0){
		return -1;
	} else close(fd);
	return 0;
}
bool is_directory(const char *path){
	struct stat statbuf;
	return (stat(path, &statbuf) == 0 && S_ISDIR(statbuf.st_mode));
}
int to_realpath(char *relative_path){
	// RETURN VALUE
		//	0: success 
		// -1: error
	char temp_path[PATH_MAX];
	char real_path[PATH_MAX];
	
	if (snprintf(temp_path, (size_t)PATH_MAX,"%s", relative_path) >= (size_t)PATH_MAX){
		fprintf(stderr, "Path too long\n");
		return -1;
	}
	// $HOME, ~
	if (temp_path[0] == '~'){
		if (snprintf(real_path, (size_t)PATH_MAX, "%s%s", home_path, temp_path+1) >= (size_t)PATH_MAX){
			fprintf(stderr, "Error: Path too long\n");
			return -1;
		}
	} 
	// to realpath
	else if (realpath(temp_path, real_path) != NULL){
		// home directory
		if (strncmp(real_path, home_path, strlen(home_path))){
			fprintf(stderr, "Error: %s is outside the home directory\n", temp_path);
			return -1;
		}
	}
	else { // errno
		int err = errno;
		if (err == EACCES) fprintf(stderr, "Error: Permission denied\n");
		else if (err == EINVAL) fprintf(stderr, "Error: Path is NULL\n");
		else if (err == ENAMETOOLONG) fprintf(stderr, "Error: Path too long\n");
		else if (err == ENOENT) fprintf(stderr, "Error: Path does not exist\n");
		else if (err == ENOTDIR) fprintf(stderr, "Error: Invalid path\n");
		else fprintf(stderr, "Error: Invalid path\n");

		return -1;
	}
	// directory
	struct stat sb;
	if (stat(real_path, &sb) < 0){
		// errno
		int err = errno;
		if (err == EACCES) fprintf(stderr, "Error: Permission denied\n");
		else if (err == ENAMETOOLONG) fprintf(stderr, "Error: Path too long\n");
		else if (err == ENOENT) fprintf(stderr, "Error: Path does not exist\n");
		else if (err == ENOTDIR) fprintf(stderr, "Error: Invalid path\n");
		else fprintf(stderr, "Error: Invalid path\n");
		
		return -1;
	}
	if (!S_ISDIR(sb.st_mode)){
		fprintf(stderr, "The path is not a directory\n");
		return -1;
	}
	
	strcpy(relative_path, real_path);
	return 0;
}
// ssu_cleanupd.c
char **divide_line(char *str, int *argc, char *del){
	*argc = 0;
	char *token = strtok(str, del);
	if (!token) return NULL;

	char **argv = (char **)calloc(128, sizeof(char *));
	while (token != NULL){
		argv[*argc] = strdup(token);
		(*argc)++;
		token = strtok(NULL, del);
	}
	argv[*argc] = NULL;
	return argv;
}
bool check_instruction(char *instruction){
	char *instruction_list[] = {
		"show", "add", "modify", "remove", "help", "exit"};
	for (int i=0; i<6; i++){
		if (!strcmp(instruction_list[i], instruction))
			return true;
	}
	return false; // instruction list에 없는 명령어
}
void print_usage(){
	printf("Usage:\n");
	printf("  > show\n");
	printf("	<none>: show monitoring daemon process info.\n");
	printf("  > add <DIR_PATH> [OPTION] ...\n");
	printf("	<none>: add daemon process monitoring the <DIR_PATH> directory.\n");
	printf("	-d <OUTPUT_PATH>: Specify the output directory <OUTPUT_PATH> where <DIR_PATH> will be arranged.\n");
	printf("	-i <TIME_INTERVAL>: Set the time interval for the daemon process to monitor in seconds.\n");
	printf("	-l <MAX_LOG_LINES>: Set the maximum number of log lines the daemon process will record.\n");
	printf("	-x <EXCLUDE_PATH1, EXCLUDE_PATH2, ...>: Exclude all subfiles in the specified directories.\n");
	printf("  > modify <DIR_PATH> [OPTION] ...\n");
	printf("  > remove <DIR_PATH>\n");
	printf("  > help\n");
	printf("  > exit\n");
}
