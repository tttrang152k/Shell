#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h> 
#include <ctype.h>
#include <signal.h>
#include <fcntl.h>
#include <errno.h>

#define MAX_LINE 80     //given in assignment details
#define MAX_ARGC 80     //given in assignment details
#define MAX_JOB 5       //given in assignment details


int g_job_id = 1;       // global assigned job_id 
int needs_In_N_Out = 0; //global bool for if there is both input and output. 0 for False and 1 for True

enum job_Status {
    AVAILABLE,
    FOREGROUND, // foreground_running
    BACKGROUND, // background_running
    STOPPED
};     

struct job_Info { 
    pid_t pid;
    enum job_Status status;
    int job_id;
    char cmd[MAX_LINE];
} jobList[MAX_JOB]; // global job list

typedef enum working_Space {
    WP_FOREGROUND,
    WP_BACKGROUND,
    WP_REDIRECT_INPUT,
    WP_REDIRECT_OUTPUT,
    WP_APPEND_OUTPUT,
    WP_IN_OUT,
    WP_IN_OUT_APPEND
} working_Space;

void addJob(pid_t pid, enum job_Status status,char* cmdLine) {
    for (int i = 0; i < MAX_JOB; i++) {
        if (jobList[i].status == AVAILABLE) {
            jobList[i].pid = pid;
            jobList[i].status = status;
            jobList[i].job_id = g_job_id;
            strcpy(jobList[i].cmd,cmdLine);
            g_job_id++;
            break;
        }
    }
    return;
}

void constructJobs() {   // Construct a default list of jobs
    for (int i = 0; i < MAX_JOB; i++) {
        jobList[i].pid = 0;
        jobList[i].status = AVAILABLE;
        jobList[i].job_id = 0;
    }
}

void printBgJobs( working_Space space, char* outputFile){
    static const char *STATUS_STRING[] = {"Running", "Stopped"};
    
    for(int i = 0; i < MAX_JOB; i++){
        if(jobList[i].status == BACKGROUND || jobList[i].status == STOPPED){
            if(jobList[i].pid != 0){
                printf("[%d] (%d) %s %s", jobList[i].job_id, jobList[i].pid, STATUS_STRING[jobList[i].status-2], jobList[i].cmd);
            }
            else{
                printf("The job is added with the wrong pid.\n");
            }
        }
    }
    
}

void deleteJob(pid_t pid) {
    for (int i = 0; i < MAX_JOB; i++) {
        if (jobList[i].pid == pid) {
            jobList[i].pid = 0;
            jobList[i].job_id = 0;
            jobList[i].status = AVAILABLE;
            jobList[i].cmd[0] = '\0';
        }
    }
}

pid_t getPIDByJID(int jid){
    pid_t pid = -1;
    for(int i = 0; i < MAX_JOB; i ++){
        if(jobList[i].job_id == jid){
            return jobList[i].pid;
        }
    }
    return pid;
}
void changeJobStatus(pid_t pid, enum job_Status newStatus) {
    for (int i = 0; i < MAX_JOB; i++) {
        if (jobList[i].pid == pid) {
            jobList[i].status = newStatus;
        }
    }
}

void switchWorkingSpace(int argc, char** argv, enum working_Space space, pid_t pid){ 
    // need to put the job back into running status
    if (kill(pid, SIGCONT) < 0) {           // fail to continue a job
        printf("Job with pid %d fails to continue\n", pid);
        return;
    }
    if (strcmp(argv[0], "fg") == 0) {
        changeJobStatus(pid, FOREGROUND);
        pause();        // pause for running foreground job
    }
    else{
        changeJobStatus(pid,BACKGROUND);
    }
}
   
void killJob(pid_t pid) {

    // send kill SIGKILL signal to terminate job
    kill(pid, SIGKILL);

    // manually delete terminated job in jobList
    deleteJob(pid);

}

void printAllCurrentJobs() {
    for (int i = 0; i < MAX_JOB; i++) {
        printf("[%d] (%d) ", jobList[i].job_id, jobList[i].pid);
        switch(jobList[i].status) {
            case FOREGROUND:
            printf("Foreground ");
            break;
            case BACKGROUND:
            printf("Background ");
            break;
            case STOPPED:
            printf("Stopped ");
            break;
            case AVAILABLE:
            printf("Available");
            break;
        }
        printf("%s\n", jobList[i].cmd);
    }
}
void distributeInput(char* input, int* argc, char** argv) {
    char* token;        
    const char* delims = " \t\n";
    token = strtok(input, delims);      // first token is the command
    while (token != NULL) {             // getting next arguments in to argv
        argv[(*argc)++] = token;
        token = strtok(NULL, delims);
    }
}

void eval(char **argv, int argc, working_Space space, char* cmdLine, char* inputFile, char* outputFile){

    char cwd[MAX_LINE];     // store current working directory path

    // Built-in commands
    if (strcmp(argv[0], "cd") == 0 || strcmp(argv[0], "pwd") == 0 || strcmp(argv[0], "quit") == 0 || (strcmp(argv[0], "jobs") == 0 ||
        strcmp(argv[0], "list") == 0) || strcmp(argv[0], "fg") == 0 || strcmp(argv[0], "bg") == 0 || strcmp(argv[0], "kill") == 0 ) {
        if (strcmp(argv[0], "cd") == 0) {
            chdir(argv[1]);
        }
        else if (strcmp(argv[0], "pwd") == 0) {
            printf("%s\n", getcwd(cwd,sizeof(cwd)));
        }
        else if (strcmp(argv[0], "quit") == 0) {
            exit(0);
        }
        else if (strcmp(argv[0], "jobs") == 0) { 
            printBgJobs(space,outputFile);
        }
        // added for testing
        else if(strcmp(argv[0], "list") == 0) {
            printAllCurrentJobs();
        }
        // fg bg and kill are merged into one else if condition
        else if (strcmp(argv[0], "fg") == 0 || strcmp(argv[0], "bg") == 0 || strcmp(argv[0], "kill") == 0){  // switch job's working spaces: fg -> bg or bg -> fg 
            pid_t pid;
            // get pid directly or from job id
            if(argv[1][0] == '%'){ 
                memmove(argv[1], argv[1]+1, strlen(argv[1]));  // remove % sign 
                int jid = atoi(argv[1]);
                pid = getPIDByJID(jid);         
            }
            else{
                pid = atoi(argv[1]);        // get pid in case argument is pid
            }
            if (strcmp(argv[0],"kill") == 0) {
                killJob(pid);
            }                                                                                
            else {      // these jobs are already stopped before switching
                switchWorkingSpace(argc, argv, space, pid);     // just need to put in either jobID or pid and then switchWorkingSpace() will handle it 
            }
        }
    }
    // not a built-in command
    else {     // run as an executable command
        
        pid_t pid;       
        char* newArgv[MAX_LINE]; 

        argv[argc] = NULL;
        if ((pid = fork()) == 0) {      // child process is successfully spawned. child's pid to the parent process  

            // following the discussion sample
            int inFile = -1;     // input file descriptor
            int outFile = -1;    // output file descriptor
            mode_t mode = S_IRWXU | S_IRWXG | S_IRWXO;  // mode fore setting permission bits

            if (space == WP_REDIRECT_INPUT) {       // redirect standard input from a file
                inFile = open(inputFile, O_RDONLY, mode);   // open the inputFile
                dup2(inFile, STDIN_FILENO);         // dup the file descriptor to point to STDIN to the input file
                close(inFile);           
            }

            else if (space == WP_REDIRECT_OUTPUT || space == WP_APPEND_OUTPUT) {      // redirect + overwrite standard output to a file
                if (space == WP_REDIRECT_OUTPUT) {
                    outFile = open(outputFile, O_CREAT|O_WRONLY|O_TRUNC, mode);
                }
                else { // append output 
                    outFile = open(outputFile, O_CREAT|O_APPEND|O_WRONLY,mode);
                }
                dup2(outFile, STDOUT_FILENO);
            }
            
            else if(space == WP_IN_OUT || space == WP_IN_OUT_APPEND){
                inFile = open(inputFile, O_RDONLY, mode);   // open the inputFile
                dup2(inFile, STDIN_FILENO);         
                close(inFile);    
                if (space == WP_IN_OUT) {
                    outFile = open(outputFile, O_CREAT|O_WRONLY|O_TRUNC, mode);
                }
                else {
                    outFile = open(outputFile, O_CREAT|O_APPEND|O_WRONLY,mode); 
                }
                dup2(outFile, STDOUT_FILENO);
            }

            if (strcmp("ls", argv[0]) == 0 || strcmp("sort", argv[0]) == 0 || strcmp("cat", argv[0]) == 0) {    // for basic command and redirect input
                newArgv[0] = argv[0];
                newArgv[1] = NULL;
                if (execvp(newArgv[0], newArgv) < 0) {
                    printf("%s: command not found.\n", argv[0]);
                    exit(0);
                }
            }
            else {
            // excecute the executables
                if(execv(argv[0], argv) < 0){       // Negative value means it didn't work - try execv first
                    if(execvp(argv[0],argv) < 0){    // Otherwise, try with execvp
                        printf("%s: Command not found.\n",argv[0]);
                        exit(0);
                    }
                }
            }
        }
        else if (pid < 0) {
            printf("Spawning child process unsuccessful! \n");
        }
        else {      // parent process
            if(space == WP_BACKGROUND){             // add new background job 
                    addJob(pid, BACKGROUND,cmdLine);   
            }
            else{
                // add new foreground job
                addJob(pid, FOREGROUND,cmdLine);
            }
            if (space == WP_FOREGROUND) {           // pause if foreground otherwise said for sigchld
                pause(); 
            }
        } 
    }
}


pid_t currentFGJobPID() {
    pid_t pid = 0;
    for (int i = 0; i < MAX_JOB; i++) {
        if (jobList[i].status == FOREGROUND) {
            pid = jobList[i].pid;
        }
    }
    return pid;
}


void interruptHandler(int signalNum) {        // only interrupts current FG job

    pid_t pid = currentFGJobPID();

    if (pid > 0) {
        kill(-pid, SIGINT);
        deleteJob(pid);
    } 

}

void stopHandler(int signalNum) {               // only stops current FG job

    pid_t pid = currentFGJobPID();
    
    if (pid > 0) {
        kill(pid, SIGTSTP);
        changeJobStatus(pid, STOPPED);
    } 
}

void sigchdHandler(int signalNum) {
    int status = 0;
    pid_t pid = 0;
    while ((pid = waitpid(currentFGJobPID(), &status, WNOHANG | WUNTRACED)) > 0) {
        
        if (WIFSIGNALED(status)) {      // child process has terminated by a signal. Ctrl + C
            interruptHandler(SIGINT);
        }
        else if (WIFSTOPPED(status)) {  // child process has been stopped by delivery of a signal. Ctrl + Z
            stopHandler(SIGTSTP);
        }
        else if (WIFEXITED(status)) {   // child process has been terminated normally
            // for terminated jobs, need to manually delete them
            deleteJob(pid);
        }
    }
}


working_Space checkInput(int* argc, char **argv) {
    working_Space space = WP_FOREGROUND;
    int input_check = 0;    // for checking if we need to work with infile and outfile

    for (int i = 0; i < *argc; i++) {
        if (strcmp(argv[i], "&") == 0) {     // Background space
            space = WP_BACKGROUND;
        }  
        else if (strcmp(argv[i], "<") == 0) {    // redirect input
            input_check = 1;
            space = WP_REDIRECT_INPUT;
        }
        else if (strcmp(argv[i], ">") == 0) {      // reidrect output
            if(input_check == 1){
                space = WP_IN_OUT;
            }
            else{
                space = WP_REDIRECT_OUTPUT;
            }
        }
        else if (strcmp(argv[i], ">>") == 0) {      // use to displace stuff by job ID
            if(input_check == 1){
                space = WP_IN_OUT_APPEND;
            }
            else{
                space = WP_APPEND_OUTPUT;
            }
            
        }
    }

    return space;
}

void fileScanner(int argc, char** argv, char* inputFile, char* outputFile) {        
    for (int i = argc - 1; i >= 0; i--) {   // loop backward to get file
        if (strcmp(argv[i], "<") == 0) {
            if (i + 1 < argc) {     // ... < inFile. < is at argv[i]. inFile will be at argv[i + 1].
                strcpy(inputFile, argv[i + 1]);
            }
        }
        else if (strstr(argv[i], ">") != NULL) {    // for either > or >>. outputFile should be saved 
            if (i + 1 < argc) {
                strcpy(outputFile, argv[i + 1]);
            }
        }
    }
}

int main() {
     
    char input[MAX_LINE];   // Input from user. Each argument is seperated by a space or tab character 
    char cmdLine[MAX_LINE]; // Copy of input since input gets changed to strtok
    int argc;               // Number of arguments from the input
    char* argv[MAX_LINE];   // List of arguments. First argument would be the command
    char inputFile[MAX_LINE];   // input file for reading from a file
    char outputFile[MAX_LINE];  // output file for writing to a file
    
    constructJobs();        // fill the job list with empty jobs 
    
    signal(SIGINT, interruptHandler);   // When user type in Ctrl+C, interrupt signal handler will be called
    signal(SIGTSTP, stopHandler);       //deals with stop signal 
    signal(SIGCHLD, sigchdHandler);     // for the bg process, cleans up zombies
    
    while(1) //loop until quit is entered
    {
        fflush(stdin);
        fflush(stdout);

        argc = 0;   // reset number of arguments every time getting a new input
        printf("prompt >");

        fgets(input, MAX_LINE, stdin);          // Get user input
        strcpy(cmdLine,input);  //  make a copy & store in cmdLine
        distributeInput(input, &argc, argv);    // Distribute arguments from user input
        needs_In_N_Out = 0;     //  reset to 0 everytime

        // check input to see if in bg, or on shell or to redicted files
        working_Space space = checkInput(&argc, argv);
        
        // Scan file names
        fileScanner(argc, argv, inputFile, outputFile);

        if(feof(stdin)){
            exit(0);
        }

        eval(argv, argc, space, cmdLine, inputFile, outputFile);     // evaluate the list of arguments
        
        fflush(stdin);
        fflush(stdout);

        
    }

    return(0);

}
