#include <stdio.h>
#include <sys/stat.h>
#include "hash.h"
#include "ftree.h"
#include <stdlib.h>
#include <dirent.h>
#include <string.h>
#include <libgen.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

/*
 * Helper to get the file permissions.
 */
int get_mode(const char *path) {
	struct stat s;
	if (stat(path, &s) == -1) {
		perror("get_mode lstat");
	}
	
	return (s.st_mode & S_IRUSR) | (s.st_mode & S_IWUSR) | (s.st_mode & S_IXUSR) | 
	(s.st_mode & S_IRGRP) | (s.st_mode & S_IWGRP) | (s.st_mode & S_IXGRP) | 
	(s.st_mode & S_IROTH) | (s.st_mode & S_IWOTH) | (s.st_mode & S_IXOTH);
}

/*
 * Copy the file tree from source to destination.
 */
int copy_ftree(const char *src, const char *dest) {
	
	int count = 1;
	// Check if src and dest are the same.
	if (src == dest) {
		printf("Source and destination should not be the same!\n");
		exit(-1);
	}	
	
	// Get stat of src.
	struct stat s;
	int check_src = lstat(src, &s);
	if (check_src == -1){
		perror("src lstat");
		exit(-1);
	}
	
	// Get stat of dest.
	struct stat d;
	int check_dest = lstat(dest, &d);
	if (check_dest == -1){
		perror("dest lstat");
		exit(-1);
	}
	
	// Check if the provided destination is a directory.
	if (!S_ISDIR(d.st_mode)) {
		printf("Destination should be a directory!\n");
		exit(-1);
	}
	
	const char *src_name = basename((char *)src);

	// Create full path of the file to be updated.
	char *dest_path = malloc(512);
	strcpy(dest_path, (char *)dest);
	strcat(dest_path, "/");
	
	char *src_path = malloc(512);
	strcpy(src_path, (char *)src);
	strcat(src_path, "/");
	
	// When src is a file.
	if (S_ISREG(s.st_mode) == 1) {
		FILE *sf;
		char buffer[512];
		strcpy(buffer, dest_path);
		strcat(buffer, (char *)src_name);
		
		// Avoid overwriting.
		DIR *guard = opendir(buffer);
		if (guard != NULL) {
			closedir(guard);
			printf("Cannot overwrite existing directory %s with %s!\n", buffer, src_path);
			exit(-1);
		}
		
		sf = fopen(buffer, "r");
		// Destination does not contain it.
		if (sf == NULL) {
			FILE *src_file = fopen(src, "r");
			FILE *sub_file = fopen(buffer, "w");
			char input_char[1];
			while (fread(input_char, 1, 1, src_file) == 1) {
				if (fwrite(input_char, 1, 1, sub_file) != 1) {
					printf("Unable to write.\n");
					exit(-1);
				}
			}
			fclose(src_file);
			fclose(sub_file);
		}
		// When destination contains the file.
		else {
			fclose(sf);
			struct stat dest_file;
			int check_dest_file = lstat(buffer, &dest_file);
			if (check_dest_file == -1){
				perror("dest_file lstat");
				exit(-1);
			}
			
			// Check size.
			if (dest_file.st_size == s.st_size) {
				FILE *src_file = fopen(src, "r");
				FILE *sub_file = fopen(buffer, "r");
				
				// When they differ in hash value, copy.
				char *src_hash = hash(src_file);
				char *sub_hash = hash(sub_file);
				fclose(sub_file);
				fclose(src_file);
				if (strcmp(src_hash, sub_hash) != 0) {
					src_file = fopen(src, "r");
					sub_file = fopen(buffer, "w");
					char input_char[1];
					rewind(src_file);
					rewind(sub_file);
					while (fread(&input_char, 1, 1, src_file) == 1) {
						if (fwrite(&input_char, 1, 1, sub_file) != 1) {
							printf("Unable to write.\n");
							exit(-1);
						}
					}
					fclose(src_file);
					fclose(sub_file);
				}
				// Update the file permissions.
				if (get_mode(src) != get_mode(buffer)) {
					if (chmod(buffer, get_mode(src)) < 0) {
						printf("Unable to chmod!");
					}
				}

			}
			// When they differ in size, copy.
			else {
				FILE *src_file = fopen(src, "r");
				FILE *sub_file = fopen(buffer, "w");
				
				char input_char[1];
				rewind(src_file);
				rewind(sub_file);
				while (fread(&input_char, 1, 1, src_file) == 1) {
					if (fwrite(&input_char, 1, 1, sub_file) != 1) {
						printf("Unable to write.\n");
						exit(-1);
					}
				}
				fclose(src_file);
				fclose(sub_file);
				// Update file permissions.
				if (get_mode(src) != get_mode(buffer)) {
					if (chmod(buffer, get_mode(src)) < 0) {
						printf("Unable to chmod!");
					}
				}
				
			}
		}
	}
	
	// When src is a directory and destination does not contain it.
	else if (S_ISDIR(s.st_mode) == 1) {
		char buffer[512];
		strcpy(buffer, dest_path);
		
		if (strcmp(basename((char *)dest), basename((char *)src)) != 0) {
			strcat(buffer, "/");
			strcat(buffer, (char *)src_name);
		}
		
		DIR *d_dirp = opendir(buffer);
		
		// When dest does not contain src directory, mkdir.
		if (d_dirp == NULL) {
			int mode = get_mode(src_path);
			// Make the directory of the same name under destination.
			if (mkdir(buffer, mode) == -1) {
				perror("mkdir");
				exit(-1);
			}
		}
		
		// When dest contains the src directory.
		else {
			closedir(d_dirp);
			// Update the permissions.
			if (get_mode(src_path) != get_mode(buffer)) {
				if (chmod(buffer, get_mode(src_path)) < 0) {
					printf("Unable to chmod!");
				}
			}
			
		}
		
		DIR *src_dirp;
		src_dirp = opendir(src);
		if (src_dirp == NULL) {
			perror("opendir");
			exit(-1);
		}
		struct dirent *src_dp;
		src_dp = readdir(src_dirp);
	
	while (src_dp != NULL) {
		// When destination contains the file under source.
		char path_buffer[512];
		strcpy(path_buffer, buffer);
		
		// Make valid path.
		if ((strcmp(src_dp->d_name, "..") != 0) && (strcmp(src_dp->d_name, ".") != 0)) {
			strcat(path_buffer, "/");
			strcat(path_buffer, src_dp->d_name);
			strcpy(src_path, src);
			strcat(src_path, "/");
			strcat(src_path, src_dp->d_name);
		} else {
			// Omit "." or ".."
			src_dp = readdir(src_dirp);
			continue;
			}
		
		struct stat s_sub;
		int check_s_sub = lstat(src_path, &s_sub);
		if (check_s_sub == -1) {
			perror("lstat src_path");
			exit(-1);
		}
		
		DIR *d1 = opendir(path_buffer);
		FILE *f1 = fopen(path_buffer, "r");
		
		if ((d1 != NULL) || (f1 != NULL)) {
			if (d1 != NULL)
				closedir(d1);
			if (f1 != NULL)
				fclose(f1);
			struct stat d_sub;
			int check_d_sub = lstat(path_buffer, &d_sub);
			if (check_d_sub == -1) {
				perror("lstat");
				exit(-1);
			}
			
			// Prevent overwriting.
			if ((S_ISDIR(s_sub.st_mode) && (!S_ISDIR(d_sub.st_mode)))
			|| ((!S_ISDIR(s_sub.st_mode)) && (S_ISDIR(d_sub.st_mode)))) {
				printf("This will overwrite!\n");
				exit(-1);			
			}
			
			// When the current file under src is a file.
			if (S_ISREG(s_sub.st_mode) == 1) {
				FILE *src_file = fopen(src_path, "r");
				FILE *sub_file = fopen(path_buffer, "r");
				if ((src_file == NULL) || (sub_file == NULL)) {
					printf("Unable to open source/destination file.\n");
					exit(-1);
				}

				// When src and file with same name under dest have the same size.
				if (d_sub.st_size == s_sub.st_size) {
					// When they differ in hash value, copy.
					char *src_hash = hash(src_file);
					char *sub_hash = hash(sub_file);
					fclose(sub_file);
					if (strcmp(src_hash, sub_hash) != 0) {
						sub_file = fopen(path_buffer, "w");
						char input_char[1];
						rewind(src_file);
						rewind(sub_file);
						while (fread(&input_char, 1, 1, src_file) == 1) {
							if (fwrite(&input_char, 1, 1, sub_file) != 1) {
								printf("Unable to write.\n");
								exit(-1);
							}
						}
						fclose(src_file);
						fclose(sub_file);
					}
					
					// Update permissions.
					if (get_mode(src_path) != get_mode(path_buffer)) {
						if (chmod(path_buffer, get_mode(src_path)) < 0) {
							printf("Unable to chmod!");
						}
					}
				}
				
				// When they differ in size, copy.
				else {
					char input_char[1];
					fclose(sub_file);
					sub_file = fopen(path_buffer, "w");
					while (fread(input_char, 1, 1, src_file) == 1) {
						if (fwrite(input_char, 1, 1, sub_file) != 1) {
							printf("Unable to write.\n");
							exit(-1);
						}
					}
					fclose(src_file);
					fclose(sub_file);
					
					// Check and change the permissions of the file tree.
					if (get_mode(src_path) != get_mode(path_buffer)) {
						if (chmod(path_buffer, get_mode(src_path)) < 0) {
							printf("Unable to chmod!");
						}
					}
				}
			}
			
			// When the current file under src is a directory.
			else if (S_ISDIR(s_sub.st_mode) == 1) {
				char *sub_src = malloc(512);
				strcpy(sub_src, src);
				strcat(sub_src, "/");
				strcat(sub_src, src_dp->d_name);
				int status, r;
				r = fork();
				
				// Parent process: copy the directory on the current depth.
				if (r > 0) {
					// Make directory if not existed. Update permissions otherwise.
					DIR *child_dir = opendir(path_buffer);
					if (child_dir == NULL) {
						int sub_mode = get_mode(sub_src);
						// Make the directory of the same name under destination.
						if (mkdir(path_buffer, sub_mode) == -1) {
							perror("mkdir");
							exit(-1);
						}
					} else {
						closedir(child_dir);
						if (get_mode(sub_src) != get_mode(path_buffer)) {
							if (chmod(path_buffer, get_mode(sub_src)) < 0) {
								printf("Unable to chmod!");
							}
						}
					}
					
					// Wait and get exit value of its child.
					if (wait(&status) != -1) {
            		if (WIFEXITED(status)) {
            			if (WEXITSTATUS(status) >= 0) {
            				count += WEXITSTATUS(status);
            			} else {
			            	printf("Error in the child process!");
            			}
            		}
            	}
				}
				
				// Child process: recursion.
				else if (r == 0) {
					exit(copy_ftree((const char *)src_path, (const char *)path_buffer));
				}
			}
		}
		
		// Deal with when the file/directory that does not exist under destination.
		else {
			// When the current file under src is a file.
			if (S_ISREG(s_sub.st_mode) == 1) {
				FILE *src_file = fopen(src_path, "r");
				FILE *sub_file = fopen(path_buffer, "w");
				char input_char[1];
				rewind(src_file);
				rewind(sub_file);
				while (fread(&input_char, 1, 1, src_file) == 1) {
					if (fwrite(&input_char, 1, 1, sub_file) != 1) {
						printf("Unable to write.\n");
						exit(-1);
					}
				}
				fclose(src_file);
				fclose(sub_file);
			}
			
			// Deal with folder under src.
			else if (S_ISDIR(s_sub.st_mode) == 1) {
				int mode = get_mode(src_path);
				// Make the directory of the same name under destination.
				if (mkdir(path_buffer, mode) == -1) {
					perror("mkdir");
					exit(-1);
				}

				// Fork a child process to deal with the folder.
				int status, r;
				r = fork();
				
				// Parent process: copy the directory on the current depth.
				if (r > 0) {
					// Wait and get exit value of its child.
					if (wait(&status) != -1) {
            		if (WIFEXITED(status)) {
            			if (WEXITSTATUS(status) >= 0) {
            				count += WEXITSTATUS(status);
            			} else {
			            	printf("Error in the child process!");
            			}
            		}
            	}
				}
				
				// Child process: recursion.
				else if (r == 0) {
					exit(copy_ftree((const char *)src_path, dirname(path_buffer)));
				}
			}
		}
		src_dp = readdir(src_dirp);
		}
		if (closedir(src_dirp) == -1) {
			perror("closedir");
		}
}
return count;
}