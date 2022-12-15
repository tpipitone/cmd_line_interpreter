/*  THOMAS PIPITONE, RID 824506781, CSSC0066
 *  Program 4
 *  CS480
 *  Professor Carroll
 *  This program is a simple UNIX command line interpreter that 
 *  handles redirection, as well as simple command processing and horizontal piping 
*/

#include "p2.h"
#include "getword.h"

#include <stdio.h>
#include <string.h>
#include <stdlib.h> 
#include <signal.h>
#include <unistd.h> 
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <errno.h>
#include <sys/wait.h>
#include <stdbool.h>
#include "CHK.h" 
 
#define _GNU_SOURCE
#define TRUE 1
#define FALSE 0 

#define STDIN_FD 0 
#define STDOUT_FD 1
#define STDERR_FD 2

#define STARTFLAG 10  
#define MAXHISTORY 10

void signalhandle(); 
 
char s[STORAGE * MAXITEM];  
char comment_hold[STORAGE * MAXITEM]; 
char metachars[STORAGE * MAXITEM]; 
 
char* newargv[STORAGE*MAXITEM];
char** input_history;

char* lastWord_hold; // last word processed
char* lastWord; 
 
char* out_filename;
char* in_filename; 
char* newargv_pipe[STORAGE*MAXITEM]; 

int pipefds[2]; // file discriptor arr for our pipe, 0 = reading, 1 = writing

int newargIndex;
int newarg_pipeIndex; 
 
int pipeIndex; 
int historyIndex; 
int line_num; 
int parseFlag; // used for our switch statement to decide what to do


bool redirect_read;
bool redirect_write;
bool redirect_append; 
bool redirect_ampersand; 
bool trailing_ampersand;
bool arg_present; 
bool comment;
bool invalid;
bool piped; 

int ampersand_; 


int main(int argc, char *argv[]){ 
	int init;   // used for initilizing MAXHISTORY
	int newfd;  // file descriptor for use with dup and dup2
	int end;    // checking for EOF
	int i;      // for loops
	pid_t pid;  //process id
	pid_t pid2; 

	invalid     = FALSE; 
	arg_present = FALSE; 
	comment     = FALSE; 
	trailing_ampersand = FALSE; 
	ampersand_  = FALSE; 
	parseFlag   = STARTFLAG; 
	line_num    = 1; 
	newargIndex = 0;
	newarg_pipeIndex = 0; 
	historyIndex = 0; 
	lastWord = ""; 

	setpgid(0,0); 
	signal(SIGTERM, signalhandle); 

	//malloc() allowed only for history mechanism
	input_history = malloc(MAXHISTORY * sizeof(char*)); // history holds up to ten previous inputs 

	for(init = 0; init < MAXHISTORY; init++){ // initilize each array element for maxhistory
		input_history[init] = malloc((STORAGE*MAXITEM) * sizeof(char*)); 	
	}

	if(argv[1]){ // if we have an argv[1], we change our standard input to the file specified
		if((newfd = open(argv[1], O_RDONLY, 0 )) < 0){
			perror(argv[1]);
			exit(-1); 
		}
		arg_present = TRUE; 
		dup2(newfd, STDIN_FD);
	}
	
	for(;;){
		START:

		/* error protection for !! if all files are under 10 lines	
		if(historyIndex == MAXHISTORY){
			fprintf(stderr, "ERROR: max allowed inputs is 10.\n");
			exit(1); 
		}
		*/
 
		if(!arg_present) // only print prompt if its stdinput
			printf("%s%d%s" , "%", line_num, "% ");
			
		newargIndex = 0; 
		newarg_pipeIndex = 0;

		invalid = FALSE; 
		piped   = FALSE; 
		redirect_write  = FALSE;
		redirect_read   = FALSE; 
		redirect_append = FALSE; 
		redirect_ampersand = FALSE; 

		end = parse();
		 
		if(invalid){ // if an imput is invlid, this flag will reset it, even after parse() called
			newargIndex = 0; 
			newarg_pipeIndex = 0;
			line_num--; 
			if(!arg_present){printf("%s%d%s" ,  "%", line_num, "% ");} 	
			parse(); 
		}
		
		trailing_ampersand = FALSE;
		
		//checks for trailing '&' ,, if found, it is disregarded, and flag is set to true; 
		if(strcmp(lastWord, "&") == 0 && newargIndex > 0){ 
			trailing_ampersand = TRUE;
		}
		
		//throws error and jumps to start if a pipe is provided with no argument
		if(piped && newargv_pipe[0] == NULL) {
			fprintf(stderr, "ERROR: Invalid pipe operation\n");
			trailing_ampersand = FALSE;
			parseFlag = STARTFLAG; 
			goto START;  
		}

		newargv[newargIndex] = NULL; // assigns last element to null so we can call exevp()  
		newargv_pipe[newarg_pipeIndex] = NULL;  //same as above, but for pipe arg
 
		if(FALSE){ // this is for testing purposes only, set to TRUE to print arrays being used
			for(i = 0; i <= newargIndex; i++){
				printf("%s%d%s%s\n", "Newargv ", i, ": ", newargv[i]); 
			}			
			for( i = 0; i <= newarg_pipeIndex; i++){
				printf("%s%d%s%s\n" , "Pipe arg " , i , ": ", newargv_pipe[i]); 
			}
			for(i = 0; i < historyIndex; i++){
				printf("%s%d%s%s\n", "History index ", i,": " ,  input_history[i]); 
			}
		}
		
		switch(parseFlag){
			case 1: //'cd'
				if( !newargv[1] ){ 
					if( chdir(getenv("HOME")) < 0 ){ 
						perror( getenv("HOME") ); 
						exit(-1); 
					}	 
				} else if (newargv[2]){
					fprintf(stderr,"%s\n","ERROR cd: Too many arguments"); 
						
				} else {
					if(chdir(newargv[1]) < 0){
						perror(newargv[1]); 
						exit(-1); 
					}
				} 
				parseFlag = STARTFLAG;  
				break; 
			case 2: // '!!'
				parseFlag = STARTFLAG;
				break;
			case 4: // newline empty
				break; 
			case 5:  // pipe '|'

				fflush(stdin); 
				fflush(stderr);
				fflush(stdout); // fflush() before calling fork to avoid duplicate propmts etc.  
 				 
				pid = fork();  //now we call fork same as case 10
				if(pid == -1){ 
					perror("ERROR IN FORKING CHILD 1:");
					exit(1); 
				} else if (pid == 0 ) { // child prog, this child will exec process 1, and fork another child to exec process 2
										 	
					CHK(pipe(pipefds)); // creating the pipeline
					
					fflush(stdin); 
					fflush(stderr); 
					fflush(stdout); 	 
					
					pid2 = fork(); 

					if(pid2 == -1){
						perror("ERROR IN FORKING CHILD 2:"); 
						exit(1); 
					} else if (pid2 == 0){ // grandchild aka child 2 program (this child will write to the pipe)
						CHK(close(pipefds[0]));  // close fds[0] because we are not reading anything from the pipe 
						CHK(dup2(pipefds[1], STDOUT_FD)); // stdout now writes to pipe 
						CHK(execvp(newargv[0], newargv));  // running the first command, whose output will then be written to the pipe instead of stdout	
						CHK(close(pipefds[1])); // close pipe once writing is finished 
					} else { // child aka child 1 program (this child will read from pipe
						CHK(close(pipefds[1]));  // close fds[1] because this child is not writing to the pipe
						CHK(dup2(pipefds[0], STDIN_FD)); // takes stdin from read end of pipe, we are essentially reading what child 1 wrote to the pipe

						if(redirect_write){ // if > detected					 
							if(( newfd = open(out_filename, O_CREAT|O_EXCL|O_WRONLY, 0644)) < 0){
								perror(out_filename); 
								break; 
							}
							if(redirect_ampersand){dup2(newfd,STDERR_FD); redirect_ampersand = FALSE;} //if ampersand, we also redirect stderr
							dup2(newfd, STDOUT_FD); // replace stdout with given file
							close(newfd);
			  				redirect_write = FALSE;
						}

						if(redirect_append){
							if((newfd = open(out_filename, O_APPEND|O_WRONLY)) < 0){
								perror(out_filename); 
								break; 
							}
							if(redirect_ampersand){ dup2(newfd, STDERR_FD); redirect_ampersand = FALSE; } //if ampersand, we redirect to stderr
							dup2(newfd, STDOUT_FD); 
							close(newfd); 
							redirect_append = FALSE; 
						} 
						 
						if(redirect_read){ //if < detected
							if(( newfd = open(in_filename, O_RDONLY, 0)) < 0){
								perror(in_filename);
								exit(-1);
							}	
							dup2(newfd, STDIN_FD);
							close(newfd);
							redirect_read = FALSE; 
						}
						
						CHK(execvp(newargv_pipe[0], newargv_pipe)); // running whatever command is written after "|" ( "|" is at newargv[newargIndex] )
						CHK(close(pipefds[0])); //close pipe after done reading

						if(trailing_ampersand){
							printf("%s%s%d%s\n" , newargv_pipe[0], "[ " , pid2, "]"); 
						} else { continue;  } 
					}


				} else { // parent prog (this)
					if(trailing_ampersand){
						printf("%s%s%d%s\n", newargv[0], " [", pid, "]" ); // if trailing &, we do not wait for child to finish, we just execute
					} else { wait(NULL); }
				}
				
				parseFlag = STARTFLAG; 
				break; 	
	
			case 10: // deals with non piped input

				fflush(stdout); // flush before creating child to avoid repeat output
				fflush(stdin);
				fflush(stderr); 
				pid = fork(); // fork() returns 0 if child is successfully created
				if(pid == -1){
					perror("ERROR:"); 
					exit(-1); 
				} else if (pid == 0){ // child prog
					if(redirect_write){ // if > detected
						if(( newfd = open(out_filename, O_CREAT|O_EXCL|O_WRONLY, 0644)) < 0){
							perror(out_filename); 
							break;  
						}
						if(redirect_ampersand){dup2(newfd, STDERR_FD); redirect_ampersand = FALSE;} 
						dup2(newfd, STDOUT_FD); // replace stdout with given file
						close(newfd);
						redirect_write = FALSE;
					} 
					if(redirect_append){ // if >> detected
						if(( newfd = open(out_filename, O_APPEND|O_WRONLY )) < 0 ){
							perror(out_filename);
							break;  
						}
						if(redirect_ampersand){dup2(newfd, STDERR_FD); redirect_ampersand = FALSE; }
						dup2(newfd, STDOUT_FD);
						close(newfd); 
						redirect_append = FALSE; 
					}
					

					if(redirect_read){ //if < detected
						if(( newfd = open(in_filename, O_RDONLY, 0)) < 0){
							perror(in_filename);
							break; 
						}
						dup2(newfd, STDIN_FD); //replace stdin with our given file
						close(newfd);
						redirect_read = FALSE; 
					}
			 
					if(execvp(newargv[0], newargv) < 0){ // calling exevp on the first argument in newargv 
						perror(newargv[0]); 
					}/// nothing is accessed below here since execvp does not return if it correctly runs 
					
				} else { // this prog
					if(trailing_ampersand){ // if we have a trailing '&', we print the PID of the chid assgned at fork()
						printf("%s%s%d%s\n", newargv[0] , " [" , pid, "]");  
					} else { wait(NULL);}
					 			
				} 
				break; 		
		}
	
		if( end == -1 ) { // break at newline
			break; 
		}
	} 

	killpg(getpgrp(), SIGTERM);
	if(!arg_present){printf("p2 terminated. \n");} // only give termination message if user input or redirection
	exit(0);  
}

/* int parse():
 * parse contiunually calls getword() until EOF or metachar is detected
 * parse sets flags based on each word given by getword
 * these flags are then used in the switch statement in main()
 * returns the value returned by getword(), word length or -1 for EOF
*/

int parse(){
/* SWITCH CASES:
 * 	1 - "cd"
 * 	2 - "!!"
 * 	3 - "end."
 * 	4 - no input
 * 	10 - all other 
 */
	int j; // for loop var declarationin
	int d; // getword comment length
	int c; //getword length
	int lineRepIndex; // index of line to be repeated with #! calls 
	bool line_inc;  
	bool special_cmd; // true if $!
	int callBackIndex; // this is the line to be repeated if !!
	char* history_ln; // history line str used for maintaining an array of past inputs
	char* exclamation; // the char array that occurs after ! calls, so we can atoi to see if valid 
	
	d = -2; //initial vals, getchar() will never return -2
	c = -2; 

	line_inc = TRUE; // makes it so we cannot increment our line counter more than once per parse call;   
	special_cmd = FALSE; // for detecting '!$' 

	for(;;){			
		REPEATLINE: // used after ungetc() in order to repeat parse() when !! called
 
		if(!comment){ 
			c = getword(s); 
	  	} else { // if comment char present, the rest of the line goes into a holding arr
			d = getword(comment_hold); 
		}
		
		if(strcmp(s, "end.") == 0  && newargIndex == 0  ){ // exit prog if end. is first word on line
			exit(0); 
		}

		if(strcmp(s, "!!") != 0 && strcmp(s,"!$") != 0){ // we do not want to add !! to our history array to avoid infinite loops
			history_ln = strdup(s);
			if(strcmp(s, "") != 0 && strcmp(s,">>") != 0 ){ lastWord_hold = strdup(s);}
			strcat(history_ln, " " );
			strcat(input_history[historyIndex], strdup(history_ln)); 
		}
		
		if(d == -1 || d == 0){ 
			comment = FALSE;
			historyIndex++; 
			break ; 
		} // comment arr, not held in newargv

		if(c == 0 || c == -1){
			lastWord = lastWord_hold;
			historyIndex++; 
			break;
		} 

		if(c != 0 && line_inc){ 
			line_num++;
			line_inc = FALSE; // set our incrementing var to f so we dont inc the line counter more than once per parse() call  
		}  

		if(strcmp(s,"#") == 0 && arg_present){ // if we are reading from a file, and there is a comment char present
			comment = TRUE; 
			break; 
		}
		
		if(strcmp(s,"<") == 0){  // if metachar detected, set appropriate flag to be worked on in case 10 of swith above		
			redirect_read = TRUE;
			c=getword(s); 
			in_filename = strdup(s);
			if(strcmp(in_filename, "") == 0){
				redirect_read = FALSE; 
				fprintf(stderr, "ERROR: No file provided for redirect\n"); // invalid line if no filename is given
				invalid = TRUE; 
				break; 		
			}
			if(!piped){parseFlag = 10;} // parseflag will have been set to 5 already if a pipe was found 
		}
		
		if(strcmp(s, ">>") == 0 ){
			redirect_append = TRUE; 
			getword(metachars); // metchars array holds whatever occurs after the metachar is detected 
			if(strcmp(metachars, "!$") == 0){
				out_filename = lastWord; 
			} else { 
				out_filename = strdup(metachars);
			}
			lastWord= out_filename; 
			strcat(input_history[historyIndex], strdup(out_filename));
			if(!piped){parseFlag = 10;}
			break; 
		
		}
		if(strcmp(s, ">>&") == 0){
			redirect_ampersand = TRUE;
			redirect_append = TRUE; 
			getword(metachars);
			out_filename = strdup(metachars); 
			if(!piped){ parseFlag = 10;}
			break; 

		}
		
		if(strcmp(s,">&") == 0 ){
			redirect_ampersand = TRUE;
			redirect_write = TRUE;  
			getword(metachars); 
			out_filename = strdup(metachars); 
			if(!piped){ parseFlag = 10; }
			break; 
		}

		if(strcmp(s,">") == 0){  // if metachar detected, set appropriate flag, this is worked on in case 10 of switch above
			redirect_write = TRUE;
			getword(metachars); 
			out_filename = strdup(metachars);  
			if(!piped){parseFlag = 10; }
			getword(s); // add any other words to newargv taht might occur after the filename
			if(strcmp(s, "") != 0){
				newargv[newargIndex] = strdup(s); 
				newargIndex++;	
			} 	  
			break; 	
		}

		if(strcmp(s, "!$") == 0){ // if !$ deteted, we sub the '!$' part of our current input with lastWord, and then repeatline
			strcat(input_history[historyIndex], lastWord);
			newargv[newargIndex] = lastWord; 
			newargIndex++;
			goto REPEATLINE;
		}
		
		if( (exclamation = strchr(s, '!')) != NULL && (strcmp(s,"!$")  != 0) && strcmp(exclamation, "") != 0 ){
		 	
			exclamation++;  // removing the ! from our exclamation string, so we can access the int  
	
			if(strcmp(s, "!!") == 0 ){ // repeat last line  
				lineRepIndex = historyIndex - 1; 
			} else if( (callBackIndex = atoi(exclamation) ) != 0){ // repeat line @ callBackIndex
				lineRepIndex = callBackIndex-1; 
				if(lineRepIndex >= historyIndex ){ 
					fprintf(stderr, "ERROR: Invalid Line Index\n" );
					break;	
				} // index out of bounds 

			} else { // invalid input, or '!$' ,
				break; 
 			}
 
			for( ( j = strlen(input_history[lineRepIndex]) - 1 ); j >= 0; j--){ // put all the chars of the desired line to be repeated back onto stdin
				ungetc(input_history[lineRepIndex][j], stdin);  
			}

			goto REPEATLINE; // restart parse with a new stdin
		}
		
		if(strcmp(s,"|") == 0){  
			piped = TRUE; // we set this to true after finding a pipe, the rest of the input will go into newargv2 
			parseFlag = 5;
		}

		if(strcmp(s,"&") == 0){ ampersand_ = TRUE;} 
		
		if(!(strcmp(s, "<")== 0) && strcmp(s, "&") != 0){ 			
			if(strcmp(s,"cd") == 0){ parseFlag = 1;}
			if(strcmp(s,"") == 0){ parseFlag = 4;}  
	
			if(!piped){
				newargv[newargIndex] = strdup(s); // adds s to our arry before any pipe detected 
				newargIndex++; 
				
			} else if (piped && (strcmp(s, "|") != 0) ) { // adds all s (after |) to our "after pipe" arr after "|" detected 
				newargv_pipe[newarg_pipeIndex] = strdup(s); 
			
				newarg_pipeIndex++; 
			}
		}
	}
	return c;
}

void signalhandle(){
	//empty because we discard the termination message provided by kill()	
}
