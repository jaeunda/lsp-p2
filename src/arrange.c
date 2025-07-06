#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "header.h"

List targetList = {NULL, 0};
List outputList = {NULL, 0};

struct config conf;
char home_path[PATH_MAX];
char log_path[PATH_MAX];

int main(int argc, char **argv){ 

	// load config file
	if (load_config(argv[1], &conf) <0){
		exit(1);
	}
	// set path
	if (snprintf(home_path, (size_t)PATH_MAX, "%s", getenv("HOME")) >= (size_t)PATH_MAX){
		exit(1);
	}
	if (snprintf(log_path, (size_t)PATH_MAX, "%s/ssu_cleanupd.log", conf.monitoring_path) >= (size_t)PATH_MAX){
		exit(1);
	}
	/* ARRANGE */
	// scan directory
	if (scan_target_dir(conf.monitoring_path)<0){
		exit(1);
	}
	if (scan_output_dir(conf.output_path)<0){
		exit(1);
	}
	// copy or remove the directory
	if (compare_and_sync()<0){ 
		exit(1);
	}
	
	// free
	free_list(&targetList);
	free_list(&outputList);
	free_config(&conf);
	exit(0);
}
int scan_target_dir(const char *cur_path){
	// RETURN VALUE
	//	성공 시 0, 에러 시 -1

	struct dirent **namelist;
	int n=-1;
	
	// scan directory
	if ((n=scandir(cur_path, &namelist, NULL, alphasort)) < 0){
		// error
		return -1;
	}
	// namelist loop
	for (int i=0; i<n; i++){
		// 현재 디렉토리와 부모 디렉토리 제외
		if (!strcmp(namelist[i]->d_name, ".") || !strcmp(namelist[i]->d_name, "..")){
			continue;
		}
		// 숨김 파일 제외
		if (namelist[i]->d_name[0] == '.')
			continue;
		// config, log 파일 제외
		if (!strcmp(namelist[i]->d_name, "ssu_cleanupd.config") || !strcmp(namelist[i]->d_name, "ssu_cleanupd.log"))
			continue;
		struct stat sb; // file stat
		char child_path[PATH_MAX]; // path 설정
		if (snprintf(child_path, (size_t)PATH_MAX, "%s/%s", cur_path, namelist[i]->d_name) >= (size_t)PATH_MAX){
			return -1;
		}

		if (stat(child_path, &sb) < 0){ // stat()
			return -1;
		}

		// is directory
		if (is_directory(child_path)){
			// option: 제외되는 경로일 경우 
			if (is_excluded(child_path)){
				continue;
			}
			// 디렉토리일 경우 재귀 탐색
			if (scan_target_dir(child_path)<0){
				return -1;
			}
			continue; // linked list에 추가하지 않음.
		} else {	// file: linked list에 추가
			char extension[EXT_MAX];
			
			// filename에서 extension 분리
			if (get_extension(namelist[i]->d_name, extension)<0){
				return -1;
			}
			
			// option: extension이 포함되어 있지 않을 경우
			if (!is_ext_specified(extension)){
				continue;
			}

		// 디렉토리 구조: Linked List로 관리
			// targetList: node 초기화
			Node *new_node=(Node *)calloc((size_t)1, sizeof(Node));
			strcpy(new_node->name, namelist[i]->d_name);
			strcpy(new_node->path, child_path);
			strcpy(new_node->extension, extension);
			new_node->mtime = sb.st_mtime;
			new_node->next = NULL;
			
			// List에 Node 추가
			if (add_files(&targetList, new_node) < 0){
				return -1;
			}
		} 
	} 
	
	// 탐색 종료 후 namelist 할당 해제
	for (int i=0; i<n; i++)
		free(namelist[i]);
	namelist = NULL;
	return 0;
}
int scan_output_dir(const char *cur_path){
	// RETURN VALUE
	//	성공 시 0, 에러 시 -1
	
	struct dirent **namelist;
	int n=-1;
	if (!is_directory(cur_path)){
		return 0;
	}
	if ((n=scandir(cur_path, &namelist, NULL, alphasort)) < 0){
		return -1;
	}

	for (int i=0; i<n; i++){
		// 현재 디렉토리와 부모 디렉토리 제외
		if (!strcmp(namelist[i]->d_name, ".") || !strcmp(namelist[i]->d_name, ".")){
			continue;
		}
		// 숨김 파일 제외
		if (namelist[i]->d_name[0] == '.')
			continue;
		
		struct stat sb; // file stat
		char child_path[PATH_MAX];
		if (snprintf(child_path, (size_t)PATH_MAX, "%s/%s", cur_path, namelist[i]->d_name) >= (size_t)PATH_MAX){
			return -1;
		}

		if (stat(child_path, &sb) < 0){
			return -1;
		}
		if (is_directory(child_path)){
			// 재귀 탐색
			if (scan_output_dir(child_path)<0){
				return -1;
			}
			continue; // linked list에 추가하지 않음.
		} else {
			// file: linked list에 추가
			char extension[EXT_MAX];
			
			// filename에서 extension 분리
			if (get_extension(namelist[i]->d_name, extension)<0){
				return -1;
			}

		// 디렉토리 구조: Linked List로 관리
			// outputList: node 추가
			Node *new_node=(Node *)calloc((size_t)1, sizeof(Node));
			strcpy(new_node->name, namelist[i]->d_name);
			strcpy(new_node->path, child_path);
			strcpy(new_node->extension, extension);
			new_node->mtime = sb.st_mtime;
			new_node->next = NULL;
			
			// List에 Node 추가
			if (add_files(&outputList, new_node) < 0){
				return -1;
			}
		} 
	} 
	
	// 탐색 종료 후 namelist 할당 해제
	for (int i=0; i<n; i++)
		free(namelist[i]);
	namelist = NULL;
	return 0;

}
int compare_and_sync(){
	Node *target_p = targetList.head;
	Node *output_p = outputList.head;

	if (!is_directory(conf.output_path)){
		mkdir(conf.output_path, 0755);
	}

	while (target_p != NULL && output_p != NULL){
		char dir_path[PATH_MAX];
		char file_path[PATH_MAX];
		if (!strcmp(target_p->name, output_p->name)){
			switch (conf.mode){
				case 1: // 최신 파일만 정리
					if (target_p->mtime > output_p->mtime){
						copy_file(target_p->path, output_p->path);
						// LOG
						print_log(target_p->path, output_p->path);
					}
					break;
				case 2: // 오래된 파일만 정리
					if (target_p->mtime < output_p->mtime){
						copy_file(target_p->path, output_p->path);
						//LOG
						print_log(target_p->path, output_p->path);
					}
					break;
				case 3: // 정리하지 않음
					break;
			}
			target_p = target_p -> next;
			output_p = output_p -> next;
		} else if (strcmp(target_p->name, output_p->name) < 0){
			// ASCII 기준 target_p->name이 먼저
				// target에는 존재하는 파일이 output에는 존재하지 않음
			
			// directory path 설정
			if (snprintf(dir_path, (size_t)PATH_MAX, "%s/%s", conf.output_path, target_p->extension) >= (size_t)PATH_MAX){
				return -1;
			}
			// directory가 존재하지 않으면 생성
			if (!is_directory(dir_path)){
				mkdir(dir_path, 0755);
			}

			// file path 설정
			if (snprintf(file_path, (size_t)PATH_MAX, "%s/%s", dir_path, target_p->name) >= (size_t)PATH_MAX){
				return -1;
			}
			// target_p가 가리키는 노드에 해당하는 파일 생성/복사
			copy_file(target_p->path, file_path);
			// LOG
			print_log(target_p->path, file_path);

			// 포인터 이동
			target_p = target_p -> next;
		} else {
			// ASCII 기준 output_p->name이 먼저
				// target에 존재하지 않는 파일이 output에 존재
			// output_p가 가리키는 노드에 해당하는 파일 삭제
			remove_file(output_p->path);
			output_p = output_p -> next;
		}
	}

	// 루프 종료
	// targetList: 남아있으면 전부 복사
	while (target_p != NULL){
		char dir_path[PATH_MAX];
		char file_path[PATH_MAX];
			// directory path 설정
		if (snprintf(dir_path, (size_t)PATH_MAX, "%s/%s", conf.output_path, target_p->extension) >= (size_t)PATH_MAX){
			return -1;
		}
		// directory가 존재하지 않으면 생성
		if (!is_directory(dir_path)){
			mkdir(dir_path, 0755);
		}

		// file path 설정
		if (snprintf(file_path, (size_t)PATH_MAX, "%s/%s", dir_path, target_p->name) >= (size_t)PATH_MAX){
			return -1;
		}
		// target_p가 가리키는 노드에 해당하는 파일 생성/복사
		copy_file(target_p->path, file_path);
		// LOG
		print_log(target_p->path, file_path);

		target_p = target_p -> next;
	}

	// outputList: 남아있으면 전부 삭제
	while (output_p != NULL){
		// remove
		remove_file(output_p->path);
		output_p = output_p -> next;
	}
	
	if (remove_empty_dir(conf.output_path)<0){
		return -1;
	}
	return 0;
}
int get_extension(const char *filename, char *extension){
	// filename = name.extension
	// 확장자가 존재하지 않는 경우: no_extension
	char temp[NAME_MAX];
	strcpy(temp, filename);
	char *token = strtok(temp, "."); // name
	token = strtok(NULL, "."); // extension
	if (token == NULL){ // 확장자가 존재하지 않는 경우
		strcpy(extension, "no_extension");
		return 0;
	} else {
		strcpy(extension, token);
		return 0;
	}
}
int add_files(List *list, Node *new_node){
	// 리스트가 비어있을 경우
	if (list->size == 0){
		list->head = new_node;
		list->size++;
		return 0;
	}
	// if (size==1 && 파일 중복)
	if (!strcmp(new_node->name, list->head->name)){
		// mode
		switch (conf.mode){
			case 1: // default: 최신 파일만 정리
				if (new_node->mtime > list->head->mtime){
					strcpy(list->head->path, new_node->path);
					list->head->mtime = new_node->mtime;
				}
				break;
			case 2: // 오래된 파일만 정리
				if (new_node->mtime < list->head->mtime){
					strcpy(list->head->path, new_node->path);
					list->head->mtime = new_node->mtime;
				}
				break;
			case 3: // 정리하지 않음
				Node *delete_node;
				delete_node = list->head;
				list->head = list->head->next; // list->head = NULL;
				free (delete_node);

				list->size--;
				break;
		}
		free(new_node);
		return 0;
	}
	// 맨 앞 삽입
	if (strcmp(new_node->name, list->head->name) <0){
		new_node->next = list->head;
		list->head=new_node;
		list->size++;
		return 0;
	}
	// 중간 삽입
	Node *temp = list -> head;
	while (temp -> next != NULL){
		if (strcmp(new_node->name, temp->next->name)<0){
			// insert
			new_node->next = temp->next;
			temp->next = new_node;
			
			list->size++;
			return 0;
		} else if (!strcmp(new_node->name, temp->next->name)){ // 중복
			// mode
			switch (conf.mode){
				case 1: // default: 최신 파일만 정리
					if (new_node->mtime > temp->next->mtime){
						strcpy(temp->next->path, new_node->path);
						temp->next->mtime = new_node->mtime;
					}
					break;
				case 2: // 오래된 파일만 정리
					if (new_node->mtime < temp->next->mtime){
						strcpy(temp->next->path, new_node->path);
						temp->next->mtime = new_node->mtime;
					}
					break;
				case 3: // do nothing: delete temp->next
					Node *delete_node;
					delete_node = temp->next;
					temp->next = temp->next->next;
					free(delete_node);

					list->size--;
					break;
			}
			free(new_node);
			return 0;
		} else
			temp=temp->next;
	}
	temp->next = new_node; // append
	list->size++;
	
	return 0;
}
int copy_file(const char *source, const char *dest){
	int src_fd, dst_fd;
	// source
	if ((src_fd=open(source, O_RDONLY))<0){
		return -1;
	}
	// file lock
	if (lock_file(src_fd, F_RDLCK) < 0){
		close(src_fd);
		return -1;
	}
	// dest
	if  ((dst_fd=open(dest, O_WRONLY | O_CREAT | O_TRUNC, 00644))<0){
		return -1;
	}
	// file lock
	if (lock_file(dst_fd, F_WRLCK) < 0){
		close(dst_fd);
		return -1;
	}

	while (1){
		char buffer[BUFFER_SIZE];
		int read_bytes, write_bytes;

		if ((read_bytes=read(src_fd, buffer, (size_t)BUFFER_SIZE))<0){
			return -1;
		}
		if ((write_bytes=write(dst_fd, buffer, (size_t)read_bytes))!=read_bytes){
			return -1;
		}
		if (read_bytes < BUFFER_SIZE) break;
	}
	close(src_fd);
	close(dst_fd);

	// mode 2: 오래된 파일만 정리되므로 복사한 파일의 mtime 수정
		// utime(): 원본 파일의 mtime 적용
	struct stat sb;
	if (stat(source, &sb) < 0){
		return -1;
	}
	struct utimbuf times;
	times.actime = sb.st_atime;
	times.modtime = sb.st_mtime;
	if (utime(dest, &times) < 0){
		return -1;
	}

	return 0;
}
int remove_file(const char *path){
	struct stat sb;
	if (stat(path, &sb) != 0){
		return -1;
	}
	if (!S_ISDIR(sb.st_mode))
		unlink(path);
	return 0;
}
int remove_empty_dir(const char *path){
	struct dirent **namelist;
	int n=-1;

	if ((n=scandir(path, &namelist, NULL, alphasort))<0){
		return -1;
	}
	for (int i=0; i<n; i++){
		if (!strcmp(namelist[i]->d_name, ".") || !strcmp(namelist[i]->d_name, "..")){
			continue;
		}
		if (namelist[i]->d_name[0] == '.'){
			continue;
		}

		// path
		char dir_path[PATH_MAX];
		if (snprintf(dir_path, (size_t)PATH_MAX, "%s/%s", path, namelist[i]->d_name) >= (size_t)PATH_MAX) {
			return -1;
		}
		// extension 확인
		if (is_directory(dir_path) && !is_ext_specified(namelist[i]->d_name)){
			if (rmdir(dir_path)<0)
				continue;
		}
	}
	for (int i=0;i<n;i++)
		free(namelist[i]);
	namelist = NULL;
	return 0;
}
void print_log(const char *source, const char *dest){
	// log
	static char timestamp[16];
	time_t now = time(NULL);
	struct tm *tm_now = localtime(&now);
	strftime(timestamp, sizeof(timestamp), "%H:%M:%S", tm_now);

	int log_fd;
	FILE *log_fp;

	if ((log_fd = open(log_path, O_RDWR)) < 0){
		return;
	}
	if (lock_file(log_fd, F_WRLCK) < 0){
		return;
	}
	log_fp = fdopen(log_fd, "a+");
	fprintf(log_fp, "[%s][%d][%s][%s]\n", timestamp, (int)conf.pid, source, dest);

	// truncate
	if (conf.max_log_lines < 0) {
		fclose(log_fp);
		return;
	}
	int total_line = 0;
	char buffer[BUFFER_SIZE];
	fseek(log_fp, (off_t)0, SEEK_SET);
	while (fgets(buffer, (size_t)BUFFER_SIZE, log_fp)){
		int len = strlen(buffer);
		if (len <=0) break;
		if (buffer[len-1] == '\n') total_line++;
	}
	
	if (total_line <= conf.max_log_lines){
		fclose(log_fp);
		return;
	}

	fseek(log_fp, (off_t)0, SEEK_SET);
	// tmp
	char tmp_path[PATH_MAX];
	if (snprintf(tmp_path, (size_t)PATH_MAX, "%s.tmp", log_path) >= (size_t)PATH_MAX)
		return;
	
	FILE *tmp_fp;
	if ((tmp_fp = fopen(tmp_path, "w")) == NULL){
		fclose(log_fp);
		return;
	}
	
	int cnt = 0;
	while (fgets(buffer, (size_t)BUFFER_SIZE, log_fp)){
		int len = strlen(buffer);
		if (len <= 0) break;
		if (buffer[len-1] == '\n') cnt++;
		if (cnt > total_line - conf.max_log_lines){
			fputs(buffer, tmp_fp);
		}
	}
	fclose(tmp_fp);
	fclose(log_fp);

	rename(tmp_path, log_path);
}
bool is_excluded(const char *path){
	if (conf.exclude_path == NULL)
		return false;
	for (int i=0; conf.exclude_path[i] != NULL; i++){
		if (!strcmp(path, conf.exclude_path[i])){
			return true;
		}
	}
	return false;
}
bool is_ext_specified(const char *extension){
	if (conf.extension == NULL) // default
		return true;
	for (int i=0; conf.extension[i] != NULL; i++){
		if (!strcmp(extension, conf.extension[i]))
			return true; // 목록에 존재
	}
	return false;
}
void free_list(List *list){
	Node *temp = list -> head;
	while (temp != NULL){
		Node *next_node = temp -> next;
		free(temp);
		temp = next_node;
	}
	list -> head = NULL;
	list -> size = 0;
}
