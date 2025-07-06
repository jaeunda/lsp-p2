#include <fcntl.h>
#include <time.h>
#include <utime.h>
#include <dirent.h>
#include <signal.h>
#include <stdbool.h>
#include <limits.h>
#include <errno.h>
#include <ctype.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>

#ifndef _HEADER_H_
#define _HEADER_H_


// define
#define PATH_MAX 4096
#define NAME_MAX 255
#define EXT_MAX 32
#define BUFFER_SIZE 4096 

#define true 1
#define false 0

// config
struct config {
	char monitoring_path[PATH_MAX]; 
	pid_t pid; // 0: error
	time_t start_time; // 0: error
	char output_path[PATH_MAX]; 
	time_t time_interval; // default: 10
	int max_log_lines; // default: none(-1)
	char **exclude_path; // default: none
	char **extension; // default: all
	int mode; // default: 1
};

// LIST
typedef struct _node{
	char name[NAME_MAX];
	char path[PATH_MAX];
	char extension[EXT_MAX];
	time_t mtime;
	struct _node *next;
} Node;
typedef struct _list{
	Node *head;
	unsigned size;
} List;
#endif

// FUNCTIONS
// ssu_cleanupd.c

void cmd_show(int argc, char **argv);
void cmd_add(int argc, char **argv); 
void cmd_modify(int argc, char **argv);
void cmd_remove(int argc, char **argv);

void init();
bool prompt();
bool show_prompt();
int set_option(struct config *conf, int argc, char **argv);
int set_config(const char *config_path, const struct config *conf);
void arrange(const char *exec_path, const char *config_path);
void handle_signal();

int get_list(bool print); 
int search_list(const char *monitoring_path);
int show_information(int line_num);
int add_list(const char *monitoring_path);
int remove_list(const char *monitoring_path);

// util.c
int load_config(const char *config_path, struct config *conf);
int init_config(struct config *conf, int argc, char **argv);
void free_config(struct config *conf);
int lock_file(int fd, int type);
int make_file(const char *file_path);

int to_realpath(char *relative_path);

bool is_directory(const char *path);
char **divide_line(char *str, int *argc, char *del);
bool check_instruction(char *instruction);
void print_usage();

// arrange.c
int scan_target_dir(const char *cur_path);
int scan_output_dir(const char *cur_path);
int compare_and_sync(); 
void print_log(const char *source, const char *dest);

int get_extension(const char *filename, char *extension);
int add_files(List *list, Node *new_node);
int copy_file(const char *source, const char *dest);
int remove_file(const char *path);
int remove_empty_dir(const char *path);

bool is_excluded(const char *path);
bool is_ext_specified(const char *extension);

void free_list(List *list);
