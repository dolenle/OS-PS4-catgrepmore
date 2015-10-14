/*
	ECE357 Operating Systems
	Dolen Le
	PS 4 Cat piped into Grep piped into More (CGM)
	This is not a virus
	Prof. Hakner
*/

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <signal.h>

void exitHandler();

int buffer_size = 128;
void *buffer;
unsigned long byteCounter = 0;
unsigned int fileCounter = 0;

char* pattern;
int pipe1[2]; //pipe between cgm and grep
int pipe2[2]; //pipe between grep and more

const struct sigaction ignoreSig = {
	.sa_handler = SIG_IGN,
	.sa_mask = 0,
	.sa_flags = 0
};

const struct sigaction interruptSig = {
	.sa_handler = exitHandler,
	.sa_mask = 0,
	.sa_flags = 0
};

void exitHandler() {
	printf("%d files, %lu bytes read\n", fileCounter, byteCounter);
	exit(0);
}

int main(int argc, char *argv[]) {
	if(argc < 3 || !argv[1] || strlen(argv[1]) == 0) {
		fprintf(stderr, "Usage: %s pattern infile1 [infile2 ...]\n", argv[0]);
		exit(-1);
	}

	pattern = argv[1];
	sigaction(SIGPIPE, &ignoreSig, NULL);
	sigaction(SIGINT, &interruptSig, NULL);
	if((buffer = (void*) malloc(buffer_size)) == NULL) {
		fprintf(stderr, "Error: Unable to allocate buffer.\n");
		return -1;
	}

	int i, infile;
	for(i = 2; i<argc; i++) {
		if(access(argv[i], R_OK) == 0) { //make sure it exists first
			
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
				if(dup2(pipe2[0], STDIN_FILENO) < 0) {
					perror("Could not redirect stdin for more");
					exit(-1);
				}
				execlp("more", "more", NULL);
				fprintf(stderr, "You must halt. And catch fire.\n");
				exit(-1);
			} else if(more > 0) {
				if(close(pipe2[0]) < 0) {
					perror("Cannot close pipe");
					exit(-1);
				}
				if((infile = open(argv[i], O_RDONLY)) < 0) {
					perror("Cannot open input file");
					fprintf(stderr, "File: %s\n", argv[i]);
					exit(-1);
				}
				fileCounter++;
				int bytes, grepStat, moreStat;
				while(1) {
					byteCounter += bytes = read(infile, buffer, buffer_size);
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
							if(close(pipe1[1]) < 0) {
								perror("Cannot close write pipe");
							}
							break;
						}
					}
				}
				waitpid(grep, &grepStat, 0);
				waitpid(more, &moreStat, 0);
				if(close(infile) < 0) {
					perror("Could not close input file");
					fprintf(stderr, "File: %s\n", argv[i]);
					exit(-1);
				}
			} else {
				perror("Could not fork process for more");
				exit(-1);
			}
		} else {
			perror("Cannot access input file");
		}
	}
	printf("Done...");
	exitHandler();	
}