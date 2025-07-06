#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "header.h"

char home_path[PATH_MAX];
char list_path[PATH_MAX];

int main(void){
	init();
	while (prompt());
	exit(0);
}
void init(){
	// home_path
	if (snprintf(home_path, (size_t)PATH_MAX, "%s", getenv("HOME")) >= (size_t)PATH_MAX){
		exit(1); // error 하지만 그럴 리 없음
	}
	// make file: ~/.ssu_cleanupd/current_daemon_list
	char dir_path[PATH_MAX];
	if (snprintf(dir_path, (size_t)PATH_MAX, "%s/.ssu_cleanupd", home_path) >= (size_t)PATH_MAX){
		exit(1); // error 
	}
	if (!is_directory(dir_path)){
		mkdir(dir_path, 0755);
	}
	
	if (snprintf(list_path, (size_t)PATH_MAX, "%s/current_daemon_list", dir_path) >= (size_t)PATH_MAX){
		exit(1);
	}
	if (make_file(list_path) < 0){
		exit(1);
	}

	return;
}
bool prompt(){
	char input[BUFFER_SIZE];	
	char *instruction;

	int argc;
	char **argv = NULL;

	printf("20232372> ");
	fgets(input, BUFFER_SIZE, stdin);
	input[strlen(input)-1] = '\0';

	// 입력이 없을 경우 프롬프트 재출력
	if (!strcmp(input, "")) return true;
	
	// input string: 공백을 기준으로 token 분리
	if ((argv = divide_line(input, &argc, " \t")) == NULL){
		// argv가 NULL
		return false;
	}

	// 지원하지 않는 명령어 -> usage 출력
	instruction = argv[0];

	if (!check_instruction(instruction)){
		print_usage();
		return true;
	}

	// 명령어 실행: exit
	if (!strcmp(instruction, "exit")) exit(0); // 프로그램 종료
	
	// 명령어 실행: help
	if (!strcmp(instruction, "help")){
		print_usage();
		return true;
	}
	// 명령어 실행 함수: cmd_show()
	if (!strcmp(instruction, "show")){
		cmd_show(argc, argv);
		return true;
	}
	// 명령어 실행 함수: cmd_add()
	if (!strcmp(instruction, "add")){
		cmd_add(argc, argv);
		return true;
	}
	// 명령어 실행 함수: cmd_modify()
	if (!strcmp(instruction, "modify")){
		cmd_modify(argc, argv);
		return true;
	}
	// 명령어 실행 함수: cmd_remove()
	if (!strcmp(instruction, "remove")){
		cmd_remove(argc, argv);
		return true;
	}

	return false; // else
}
void cmd_show(int argc, char **argv){
	printf("Current working daemon process list\n\n");
	while (show_prompt());
	return;
}
void cmd_add(int argc, char **argv){
	struct config conf;
	char config_path[PATH_MAX];
	char log_path[PATH_MAX];
	char exec_path[PATH_MAX];

	// set_option
	if (init_config(&conf, argc, argv) < 0){
		return;
	}
	if (set_option(&conf, argc, argv) < 0){
		return;
	}
	// search the list
	if (search_list(conf.monitoring_path) >= 0){
		fprintf(stderr, "Error: The path already being monitored\n");
		free_config(&conf);
		return;
	}

	// set file path
	if (snprintf(config_path, (size_t)PATH_MAX, "%s/ssu_cleanupd.config", conf.monitoring_path) >= (size_t)PATH_MAX){
		fprintf(stderr, "Error: Path too long\n");
		free_config(&conf);
		return;
	}
	if (snprintf(log_path, (size_t)PATH_MAX, "%s/ssu_cleanupd.log", conf.monitoring_path) >= (size_t)PATH_MAX){
		fprintf(stderr, "Error: Path too long\n");
		free_config(&conf);
		return;
	}
	

	// make file
	if (make_file(config_path) < 0 || make_file(log_path) < 0){
		free_config(&conf);
		return;
	}
	
	realpath("./arrange", exec_path);

		
	// close all open file descriptors except 0, 1, 2
	int maxfd = getdtablesize();
	for (int fd = 3; fd < maxfd; fd++)
		close(fd);

	// reset all signal handlers to their default
	struct sigaction sa;
	sa.sa_handler = SIG_DFL;
	
	for (int sig = 1; sig < _NSIG; sig++){
		if (sig == SIGKILL || sig == SIGSTOP) continue;
		sigaction(sig, &sa, NULL);
	}

	// fork
	pid_t pid;
	if ((pid = fork()) < 0){
		free_config(&conf);
		return;
	} else if (pid != 0) { // parent
		free_config(&conf);
		return; // prompt
	} // child: background process
	
	// setsid
	setsid();

	// double fork
	if ((pid = fork()) < 0){
		exit(1);
	} else if (pid != 0){ // child process
		exit(0);
	}
	
	// daemon process

	// ignore terminal signal
	signal(SIGTTIN, SIG_IGN);
	signal(SIGTTOU, SIG_IGN);
	signal(SIGTSTP, SIG_IGN);

	// connect /dev/null to standard input, output and error
	int fd;
	if ((fd = open("/dev/null", O_RDWR)) < 0){
		exit(1);
	}
	dup2(fd, STDIN_FILENO);
	dup2(fd, STDOUT_FILENO);
	dup2(fd, STDERR_FILENO);

	if (fd > STDERR_FILENO) 
		close(fd);

	// reset the umask
	umask(0);

	// change the directory
	chdir("/");

	pid = getpid();

	conf.pid = pid;
	conf.start_time = time(NULL);
	
	// set_config
	if (set_config(config_path, &conf) < 0){
		exit(1);
	}

	// add list
	if (add_list(conf.monitoring_path) < 0){
		exit(1);
	}
	
	// loop
	while (1){
		arrange(exec_path, config_path);
		sleep(conf.time_interval);
	}

	return;
}
void cmd_modify(int argc, char **argv){
	struct config conf;
	char config_path[PATH_MAX];
	char monitoring_path[PATH_MAX];

	// argv[1]: monitoring_path
	if (snprintf(monitoring_path, (size_t)PATH_MAX, "%s", argv[1]) >= (size_t)PATH_MAX){
		fprintf(stderr, "Error: Path too long\n");
		return;
	}
	if (to_realpath(monitoring_path) < 0){
		// <PATH> is outside the home directory
		// Invalid Path
		return;
	}
	// search the list
	if (search_list(monitoring_path) <= 0){
		fprintf(stderr, "Error: Daemon process does not exist\n");
		return;
	}
	
	// load config
	if (snprintf(config_path, (size_t)PATH_MAX, "%s/ssu_cleanupd.config", monitoring_path) >= (size_t)PATH_MAX){
		fprintf(stderr, "Error: Path too long\n");
		free_config(&conf);
		return;
	}
	if (load_config(config_path, &conf) < 0){
		return;
	}

	// set_option
	if (set_option(&conf, argc, argv) < 0){
		return;
	}
	// set_config
	if (set_config(config_path, &conf) < 0){
		free_config(&conf);
		return;
	}
	// free config
	free_config(&conf);

	return;
}
void cmd_remove(int argc, char **argv){
	if (argc != 2){
		print_usage();
		return;
	}

	pid_t pid;
	char monitoring_path[PATH_MAX];
	// argv[1]: monitoring_path
	if (snprintf(monitoring_path, (size_t)PATH_MAX, "%s", argv[1]) >= (size_t)PATH_MAX){
		fprintf(stderr, "Error: Path too long\n");
		return;
	}
	if (to_realpath(monitoring_path) < 0){
		// <PATH> is outside the home directory
		// Invalid Path
		return;
	}
	// search the list
	if (search_list(monitoring_path) <= 0){
		fprintf(stderr, "Error: Daemon process does not exist\n");
		return;
	}

	// get pid
	struct config conf;
	char config_path[PATH_MAX];
	if (snprintf(config_path, (size_t)PATH_MAX, "%s/ssu_cleanupd.config", monitoring_path) >= (size_t)PATH_MAX){
		return;
	}
	if (load_config(config_path, &conf) < 0){
		return;
	}
	pid = conf.pid;

	// kill
	if (kill(pid, SIGTERM) < 0){
		free_config(&conf);
		return;
	}
	// remove path from the list
	if (remove_list(monitoring_path) < 0){
		fprintf(stderr, "remove list error\n");
		free_config(&conf);
		return;
	}

	return;
}
void arrange(const char *exec_path, const char *config_path){
	pid_t pid;
	char *argv[] = {"arrange", (char *)config_path, NULL};

	if ((pid = fork()) < 0){
		fprintf(stderr, "fork error\n");
		return;
	} else if (pid == 0){
		execv(exec_path, argv);
		exit(1); // 실행되지 않음
	} else {
		wait(NULL);
	}
	
	return;
}
void handle_signal(){
	wait(NULL); // 자식 프로세스가 종료될 때까지 대기
	// 자식 프로세스가 존재하지 않으면 데몬 프로세스 즉시 종료
	exit(0);
}
bool show_prompt(){
	printf("0. exit\n");

	int line_num;
	if ((line_num = get_list(true)) < 0){
		return false;
	}

	printf("\nSelect one to see process info: ");
	
	// 사용자 입력
	char input[128];
	if (fgets(input, sizeof(input), stdin) == NULL){
		fprintf(stderr, "Please check your input is valid\n");
		return true;
	}
	input[strlen(input)-1] = '\0';
	// exit
	if (!strcmp(input, "0")) return false;

	
	// is valid input
	int input_num;
	if ((input_num = atoi(input)) > 0 && input_num <= line_num){
		if (show_information(input_num) < 0){
			fprintf(stderr, "show information error\n");
		}
		return false;
	} else {
		fprintf(stderr, "Please check your input is valid\n");
		return true;
	}
	
	return false; // 도달 x
}
int show_information(int line_num){
	int count = 0;
	char buffer[BUFFER_SIZE];
	char path[PATH_MAX];
	int fd;

	if ((fd = open(list_path, O_RDONLY)) < 0){
		return -1;
	}
	if (lock_file(fd, F_RDLCK) < 0){
		close(fd);
		return -1;
	}
	FILE *fp;
	fp = fdopen(fd, "r");
	while (fgets(buffer, (size_t)BUFFER_SIZE, fp)){
		count++;
		if (count == line_num) break;
	}
	strcpy(path, buffer);
	path[strlen(path)-1] = '\0';
	fclose(fp);
	
	int config_fd, log_fd;
	FILE *config_fp, *log_fp;
	char config_path[PATH_MAX], log_path[PATH_MAX];
	// set file path
	if (snprintf(config_path, (size_t)PATH_MAX, "%s/ssu_cleanupd.config", path) >= (size_t)PATH_MAX){
		fprintf(stderr, "Error: Path too long\n");
		return -1;
	}
	if (snprintf(log_path, (size_t)PATH_MAX, "%s/ssu_cleanupd.log", path) >= (size_t)PATH_MAX){
		fprintf(stderr, "Error: Path too long\n");
		return -1;
	}
	// config
	if ((config_fd = open(config_path, O_RDONLY)) < 0){
		return -1;
	}
	if (lock_file(config_fd, F_RDLCK) < 0){
		close(config_fd);
		return -1;
	}

	if ((config_fp = fdopen(config_fd, "r")) == NULL){
		fprintf(stderr, "config open error\n");
		close(config_fd);
		return -1;
	}
	fprintf(stdout, "\n\n1. config detail\n\n");
	while (fgets(buffer, (size_t)BUFFER_SIZE, config_fp)){
		fputs(buffer, stdout);
	}
	fprintf(stdout, "\n\n");
	fclose(config_fp);
	
	// log
	int line_cnt = 0;
	int cur_line = 0;
	if ((log_fd = open(log_path, O_RDONLY)) < 0){
		return -1;
	}
	if (lock_file(log_fd, F_RDLCK) < 0){
		close(log_fd);
		return -1;
	}

	if ((log_fp = fdopen(log_fd, "r")) ==  NULL){
		fprintf(stderr, "log open error\n");
		close(log_fd);
		return -1;
	}
	// line count
	while (fgets(buffer, (size_t)BUFFER_SIZE, log_fp)){
		line_cnt++;
	}
	fseek(log_fp, (off_t)0, SEEK_SET);

	fprintf(stdout, "2. log detail\n\n");
	while (fgets(buffer, (size_t)BUFFER_SIZE, log_fp)){
		cur_line++;
		if (cur_line > line_cnt - 10)
			fputs(buffer, stdout);
	}
	fprintf(stdout, "\n\n");
	fclose(log_fp);

	return 0;
}
int set_option(struct config *conf, int argc, char **argv){
	// option flag
	int opt_d = 0;
	int opt_i = 0;
	int opt_l = 0;
	int opt_x = 0;
	int opt_e = 0;
	int opt_m = 0;
	
	// get option
	int opt;
	optind = 2;
	while ((opt = getopt(argc, argv, "d:i:l:x:e:m:")) != -1){
		switch (opt){
			case 'd':
			{	if (opt_d){
					print_usage();
					return -1;
				} else opt_d = 1;
				// 옵션의 인자가 두 개 이상 존재하면 에러
				if (optind < argc && argv[optind][0] != '-'){ 
					print_usage();
					return -1;
				}
				
				char temp[PATH_MAX];
				if (snprintf(temp, (size_t)PATH_MAX, "%s", optarg) >= (size_t)PATH_MAX){
					fprintf(stderr, "Error: Path too long\n");
					return -1;
				}
				if (to_realpath(temp) < 0) {
					return -1;
				}
				if (strlen(temp) >= strlen(conf->monitoring_path) && !strncmp(temp, conf->monitoring_path, strlen(conf->monitoring_path))){
					if (temp[strlen(conf->monitoring_path)] == '/'){
						fprintf(stderr, "Error: Output path includes the monitoring path\n");
						return -1;
					} else if (temp[strlen(conf->monitoring_path)] == '\0'){
						fprintf(stderr, "Error: Output path (-d) cannot be same as monitoring path\n");
						return -1;
					}
				}
				strcpy(conf->output_path, temp);
				break;
			}
			case 'i':
			{	if (opt_i){
					return -1;
					print_usage();
				} else opt_i = 1;
				// 옵션의 인자가 두 개 이상 존재하면 에러
				if (optind < argc && argv[optind][0] != '-'){
					print_usage();
					return -1;
				}
				// 인자가 정수로 변환할 수 없는 문자열이면 에러
				size_t len = strlen(optarg);
				for (int i=0; i<len; i++){
					if (!isdigit(optarg[i])){
						fprintf(stderr, "Error: Argument must be natural number\n");
						return -1;
					}
				}
				if (atoi(optarg) <= 0){
					fprintf(stderr, "Error: Argument must be natural number\n");
					return -1;
				}
				conf->time_interval = (time_t) atoi(optarg);
				break;
			}
			case 'l':
			{	if (opt_l){
					print_usage();
					return -1;
				} else opt_l = 1;
				// 옵션의 인자가 두 개 이상 존재하면 에러
				if (optind < argc && argv[optind][0] != '-'){
					print_usage();
					return -1;
				}
				// 인자가 정수로 변환할 수 없는 문자열이면 에러
				size_t len = strlen(optarg);
				for (int i=0; i<len; i++){
					if (!isdigit(optarg[i])){
						fprintf(stderr, "Error: Argument must be natural number\n");
						return -1;
					}
				}
				if (atoi(optarg) <= 0){
					fprintf(stderr, "Error: Argument must be natural number\n");
					return -1;
				}
				conf->max_log_lines = atoi(optarg);
				break;
			}
			case 'x':
			{	
				if (opt_x){
					print_usage();
					return -1;
				} else opt_x=1;
				char temp[PATH_MAX];

				// count
				int count = 0;
				int index = optind-1;
				int start = index;
				while (index < argc && argv[index][0] != '-'){
					if (snprintf(temp, (size_t)PATH_MAX, "%s", argv[index]) >= (size_t)PATH_MAX){
						fprintf(stderr, "Error: Path too long\n");
						return -1;
					}
					if (to_realpath(temp) < 0){
						return -1;
					}
					if (strlen(temp) >= strlen(conf->monitoring_path) && !strncmp(temp, conf->monitoring_path, strlen(conf->monitoring_path))){
						if (temp[strlen(conf->monitoring_path)] != '/'){
							fprintf(stderr, "Error: Exclude path (-x) is outside the monitoring path\n");
							return -1;
						} else if (temp[strlen(conf->monitoring_path)] == '\0'){
							fprintf(stderr, "Error: Exclude path (-x) cannot be same as monitoring path\n");
							return -1;
						}
					}
					else if (strlen(temp) >= strlen(conf->monitoring_path) && strncmp(temp, conf->monitoring_path, strlen(conf->monitoring_path)) != 0){
						fprintf(stderr, "Error: Exclude path (-x) is outside the monitoring path\n");
						return -1;
					} else if (strlen(temp) < strlen(conf->monitoring_path)){
						fprintf(stderr, "Error: Exclude path (-x) is outside the monitoring path\n");
						return -1;
					}
					count++;
					index++;
				}
				optind = index;
				
				// set exclude_path
				conf->exclude_path = (char **)calloc((size_t)count+1, sizeof(char *));
				for (int i=0; i<count; i++){
					conf->exclude_path[i] = strdup(argv[start+i]);
					if (to_realpath(conf->exclude_path[i]) < 0){
						for (int k=0; k<=i; k++) free(conf->exclude_path[k]);
						return -1;
					}
					for (int l=0; l<i; l++){
						size_t len = (strlen(conf->exclude_path[i]) <= strlen(conf->exclude_path[l])) ? strlen(conf->exclude_path[i]) : strlen(conf->exclude_path[l]);
						if (!strncmp(conf->exclude_path[i], conf->exclude_path[l], len)){
							if (conf->exclude_path[i][len] == '/' || conf->exclude_path[l][len] == '/'){
								fprintf(stderr, "Error: Exclude path (-x) cannot overlap\n");
								for (int k=0; k<=i; k++) free(conf->exclude_path[k]);
								return -1;
							} else if (conf->exclude_path[i][len] == '\0' && conf->exclude_path[l][len] == '\0'){
								fprintf(stderr, "Error: Exclude path (-x) cannot overlap\n");
								for (int k=0; k<=i; k++) free(conf->exclude_path[k]);
								return -1;
							}
						}
					}
				}
				conf->exclude_path[count]=NULL;
				break;
			}
			case 'e':
			{	if (opt_e){
					print_usage();
					return -1;
				} else opt_e=1;
				char temp[EXT_MAX];

				// count
				int count = 0;
				int index = optind-1;
				int start = index;

				while (index < argc && argv[index][0] != '-'){
					if (snprintf(temp, (size_t)EXT_MAX, "%s", argv[index]) >= (size_t)EXT_MAX){
						return -1;
					}
					count++;
					index++;
				}
				optind = index;

				// set extension
				conf->extension = (char **)calloc((size_t)count+1, sizeof(char *));
				for (int i=0; i<count; i++){
					conf->extension[i]=strdup(argv[start+i]);
				}
				conf->extension[count]=NULL;
				break;
			}
			case 'm':
				if (opt_m){
					print_usage();
					return -1;
				} else opt_m = 1;

				if (optind < argc && argv[optind][0] != '-'){
					print_usage();
					return -1;
				}
				
				size_t len = strlen(optarg);
				for (int i=0; i<len; i++){
					if (!isdigit(optarg[i])){
						fprintf(stderr, "Error: Argument must be natural number\n");
						return -1;
					}
				}
				if (atoi(optarg) <= 0 || atoi(optarg) > 3){
					fprintf(stderr, "Error: Invalid <MODE> value\n");
					return -1;
				}
				conf->mode = atoi(optarg);
				break;
			case '?':
				print_usage();
				break;

		}
	}
	return 0;
}
int set_config(const char *config_path, const struct config *conf){
	int config_fd;
	FILE *config_fp;
	if ((config_fd = open(config_path, O_WRONLY | O_CREAT, 0644)) < 0){
		return -1;
	}
	if (lock_file(config_fd, F_WRLCK) < 0){
		close(config_fd);
		return -1;
	}
	config_fp = fdopen(config_fd, "w");
	// monitoring path
	fprintf(config_fp, "monitoring_path: %s\n", conf->monitoring_path);
	// pid
	if (conf->pid != (pid_t)0){
		fprintf(config_fp, "pid: %ld\n", (long)conf->pid);
	} else {
		ftruncate(config_fd, (off_t)0);
		fclose(config_fp);
		return -1;
	}
	// start time
	if (conf->start_time > (time_t) 0){
		static char timestamp[64];
		struct tm *tm_start = localtime(&conf->start_time);
		strftime(timestamp, sizeof(timestamp), "%Y-%m-%d %H:%M:%S", tm_start);

		fprintf(config_fp, "start_time: %s\n", timestamp);
	} else {
		ftruncate(config_fd, (off_t)0);
		fclose(config_fp);
		return -1;
	}
	// output path
	fprintf(config_fp, "output_path: %s\n", conf->output_path);
	// time_interval
	fprintf(config_fp, "time_interval: %ld\n", (long)conf->time_interval);
	// max_log_lines
	if (conf->max_log_lines == (-1)){
		fprintf(config_fp, "max_log_lines: none\n");
	} else {
		fprintf(config_fp, "max_log_lines: %d\n", conf->max_log_lines);
	}
	// exclude_path
	if (conf->exclude_path == NULL){
		fprintf(config_fp, "exclude_path: none\n");
	} else {
		fprintf(config_fp, "exclude_path: ");
		for (int i=0; conf->exclude_path[i] != NULL; i++){
			if (i)
				fprintf(config_fp, ",");
			fprintf(config_fp, "%s", conf->exclude_path[i]);
		}
		fprintf(config_fp, "\n");
	}
	// extension
	if (conf->extension == NULL){
		fprintf(config_fp, "extension: all\n");
	} else {
		fprintf(config_fp, "extension: ");
		for (int i=0; conf->extension[i] != NULL; i++){
			if (i)
				fprintf(config_fp, ",");
			fprintf(config_fp, "%s", conf->extension[i]);
		}
		fprintf(config_fp, "\n");
	}
	// mode
	fprintf(config_fp, "mode: %d\n", conf->mode);
	fclose(config_fp);
	return 0;
}
int get_list(bool print){
	// RETURN VALUE
		// success: line count
		// error: -1
	int count=0;
	int fd;
	char buffer[BUFFER_SIZE];

	if ((fd = open(list_path, O_RDONLY)) < 0){
		return -1;
	}
	if (lock_file(fd, F_RDLCK) < 0){
		close(fd);
		return -1;
	}

	FILE *fp;
	fp = fdopen(fd, "r");
	while (fgets(buffer, (size_t)BUFFER_SIZE, fp)){
		count++;
		if (print){
			fprintf(stdout, "%d. ", count);
			fprintf(stdout, "%s", buffer);
		}
	}
	fprintf(stdout, "\n");
	fclose(fp);
	return count;
}
int search_list(const char *monitoring_path){
	// RETURN VALUE
		// 		 -1: does not exist or error
		// 	      0: child or parent
		// line_num: does exist
	int list_fd;
	int line_cnt = 0;
	char buffer[BUFFER_SIZE] = {0};
	FILE *list_fp;

	if ((list_fd = open(list_path, O_RDONLY)) < 0){
		return -1;
	}
	if (lock_file(list_fd, F_RDLCK) < 0){
		close(list_fd);
		return -1;
	}

	list_fp = fdopen(list_fd, "r");
	while (fgets(buffer, (size_t)BUFFER_SIZE, list_fp)){
		line_cnt++;
		buffer[strlen(buffer)-1] = '\0';
		if (strlen(buffer) > strlen(monitoring_path)){
			if (!strncmp(buffer, monitoring_path, strlen(monitoring_path))){
				fclose(list_fp);
				return 0;
			}
		} else if (strlen(buffer) < strlen(monitoring_path)){
			if (!strncmp(buffer, monitoring_path, strlen(buffer))){
				fclose(list_fp);
				return 0;
			}
		} else { // strlen(buffer) == strlen(monitoring_path)
			if (!strcmp(buffer, monitoring_path)){
				fclose(list_fp);
				return line_cnt;
			}
		}
	}
	fclose(list_fp);
	return -1;
}
int add_list(const char *monitoring_path){
	// RETURN VALUE
		// -1: error
		//  0: success
	int list_fd;
	FILE *list_fp;

	if ((list_fd = open(list_path, O_WRONLY)) < 0){
		return -1;
	}
	if (lock_file(list_fd, F_WRLCK) < 0){
		close(list_fd);
		return -1;
	}

	list_fp = fdopen(list_fd, "a");
	fseek(list_fp, (off_t)0, SEEK_END);
	fprintf(list_fp, "%s\n", monitoring_path);

	fclose(list_fp);
	return 0;
}
int remove_list(const char *monitoring_path){
	// RETURN VALUE
		// -1: error
		//  0: success
	int list_fd;
	int total_line, line_num;
	char **cp_list = NULL;
	char buffer[BUFFER_SIZE] = {0};
	FILE *list_fp;

	// get total line number
	if ((total_line = get_list(false)) < 0){
		return -1;
	}
	
	// get line number
	if ((line_num = search_list(monitoring_path)) <= 0){
		return -1;
	}
	
	// read (except target path)
	int cnt = 0;
	int index = 0;
	cp_list = (char **)calloc(total_line, sizeof(char *));

	if ((list_fd = open(list_path, O_RDWR)) < 0){
		free(cp_list);
		return -1;
	}
	if (lock_file(list_fd, F_WRLCK) < 0){
		close(list_fd);
		free(cp_list);
		return -1;
	}

	list_fp = fdopen(list_fd, "r+");
	while (fgets(buffer, (size_t)BUFFER_SIZE, list_fp)){
		cnt++;
		if (cnt == line_num){
			continue;
		}
		cp_list[index++] = strdup(buffer);
	}
	cp_list[index] = NULL;

	// truncate
	ftruncate(list_fd, (off_t)0);
	rewind(list_fp);

	// write 
	for (int i=0; i<total_line; i++){
		if (cp_list[i]){
			fputs(cp_list[i], list_fp);
			free(cp_list[i]);
		}
	}
	free(cp_list);
	fclose(list_fp);

	return 0;
}
