/*
	ECE357 Operating Systems
	Dolen Le
	PS 4 Cat piped into Grep piped into More (or Less)
	Prof. Hakner
*/

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>

int doCopy(int src);

int buffer_size = 128;
void *buffer;
int outfile = 1;
int out_spec = 0;

char* pattern;
int pipe1[2];
int pipe2[2];



int main(int argc, char *argv[]) {
	if(argc < 3 || !argv[1] || strlen(argv[1]) == 0) {
		fprintf(stderr, "Usage: %s pattern infile1 [infile2 ...]\n", argv[0]);
		exit(-1);
	}

	pattern = argv[1];
	if((buffer = (void*) malloc(buffer_size)) == NULL) {
		fprintf(stderr, "Error: Unable to allocate buffer.\n");
		return -1;
	}

	int i, infile;
	for(i = 2; i<argc; i++) {
		if((infile = open(argv[i], O_RDONLY)) >= 0) {
			
			if(pipe(pipe1) < 0) {
				perror("Could not open pipe to grep");
			}
			if(pipe(pipe2) < 0) {
				perror("Could not open pipe to more");
			}

			int grep = fork();
			if(grep == 0) { //inside grep child
				if(close(pipe1[1]) < 0 || close(pipe2[0]) < 0) {
					perror("Can't find the right size of pipe cap");
					exit(-1);
				}
				//dup pipe1 read to stdin and pipe2 write to stdout
				if(dup2(pipe1[0], STDIN_FILENO) < 0 || dup2(pipe2[1], STDOUT_FILENO) < 0) {
					perror("The pipes are all mixed up");
					exit(-1);
				}
				execlp("grep", "grep", pattern, NULL);
				fprintf(stderr, "If you see this, grep failed to run.\n");
				exit(-1);
			} else if(grep > 0) { //inside parent
				if(close(pipe1[0]) < 0 || close(pipe2[1]) < 0) {
					perror("Could not close unused pipes");
					exit(-1);
				}
			} else {
				perror("Could not fork process for grep");
				exit(-1);
			}

			int more = fork();
			if(more == 0) {
				//dup pipe2 read to stdin
				if(close(pipe1[1]) < 0) {
					perror("Pipes are leaking everywhere");
					exit(-1);
				}
				dup2(pipe2[0], STDIN_FILENO);
				execlp("more", "more", NULL);
			} else if(more > 0) {
				close(pipe2[0]);
				int bytes, grepStat, moreStat;
				while(1) {
					bytes = read(infile, buffer, buffer_size);
					if(bytes < 0) {
						perror("Error reading input");
						exit(-1);
					} else {
						int wbytes = write(pipe1[1], buffer, bytes);
						if(wbytes < 0) {
							perror("Error writing to output");
							exit(-1);
						}
						if(bytes < buffer_size) { //reached EOF
							close(pipe1[1]);
							break;
						}
					}
				}
				printf("waiting for grep (%d)...\n", grep);
				waitpid(grep, &grepStat, 0);
				printf("waiting for more (%d)...\n", more);
				waitpid(more, &moreStat, 0);
			} else {
				perror("Could not fork process for more");
				exit(-1);
			}


		} else {
			perror("Could not open input file");
		}
	}
}