#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <dirent.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <unistd.h>
#include <time.h>
#include <signal.h>

#define MAX_LENGTH 2049
#define MAX_ARGUMENTS 513

void ignoreSIGINT();
void catchSIGINT();
void ignoreSIGTSTP();
void catchSIGTSTP();
void handleSIGTSTP(int signo);
bool ignoreBackground;


/*
Struct stores a command list of individual commands as well as 
other control variables that help indetify and process commands.
*/
struct commands
{
    char command[MAX_ARGUMENTS][256];
    int numberOfCommands; 
    bool redirectInput;
    int inputFileIndex;
    bool redirectOutput;
    int outputFileIndex;
    bool background;
    bool blankLine;
    
};


/*
Prints the exit status of the last foreground process. 
*/
void displayStatus(int childStatus)
{
    // exit value of last process
    if(WIFEXITED(childStatus))
    {
		printf("exit value %d\n", WEXITSTATUS(childStatus));
        fflush(stdout);
	} 
    // if process was terminated by signal, show signal value.
    else
    {
		printf("terminated by signal %d\n",  WTERMSIG(childStatus));
        fflush(stdout);
	}
}

/*
If the user doesn't redirect the standard input for a background command, 
then standard input should be redirected to /dev/null.
*/
void backgroundNoRedirectInput()
{
// redirect to /dev/null.
    int sourceFD = open("/dev/null", O_RDONLY);
    if (sourceFD == -1) 
    { 
        perror("cannot open file for input"); 
        exit(1); 
    }
    // Redirect stdin 
    int result = dup2(sourceFD, 0);
    if (result == -1) { 
        perror("source dup2()"); 
        exit(2); 
    }
}

/*
If the user doesn't redirect the standard output for a background command, 
then standard output should be redirected to /dev/null.
*/
void backgroundNoRedirectOuput()
{
    // redirect to /dev/null.
    int targetFD = open("/dev/null", O_WRONLY | O_CREAT | O_TRUNC);
    if (targetFD == -1) 
    { 
        perror("cannot open file for output"); 
        exit(1); 
    }
    // Redirect stdout
    int result = dup2(targetFD, 1);
    if (result == -1) { 
        perror("target dup2()"); 
        exit(2); 
    }
}


/*
Executes commands by forking a child process and running execvp. 
Redirection of input and output is dealt within the child process. 
Foreground and background process are handled in the parent process after forking. 
*/
int executeCommands(struct commands *output, char *argv[], int exitStatus, pid_t pidArray[])
{
    int childStatus;
	// Fork a new process
	pid_t spawnPid = fork();

    // save child's pid into a pidArray so it can be used to store ongoing
    // background pids and to kill all ongoing processes before the program exits.  
    for(int i=0; i < MAX_ARGUMENTS; i++ )
    {
        if(pidArray[i] == 0)
        {
            pidArray[i] = spawnPid;
            break;
        } 
    }

	switch(spawnPid)
    {
        case -1:
            perror("fork()\n");
            exit(1);
            break;
        case 0: // In the child process
            
            // ignore SIGTSTP signal for both foreground and background process.
            ignoreSIGTSTP();

            // if it is a background process
            if(output->background == true && ignoreBackground == false)
            {
                // ignore SIGINT signal
                ignoreSIGINT();
            }
            // if it is a foreground process
            else
            {   // receive SIGINT signal 
                catchSIGINT();
            }
            
            // input file redirected via stdin.
            if(output->redirectInput == true)
            {
                // open source file
                int sourceFD = open(argv[output->inputFileIndex], O_RDONLY);
                if (sourceFD == -1) 
                {   
                    perror("cannot open file for input"); 
                    exit(1); 
	            }
                // Redirect stdin to source file
                int result = dup2(sourceFD, 0);
                if (result == -1) { 
                    perror("source dup2()"); 
                    exit(2); 
                }
            }

            // output file redirected via stdout.
            if(output->redirectOutput == true)
            {
                // Open target file
                int targetFD = open(argv[output->outputFileIndex], O_WRONLY | O_CREAT | O_TRUNC, 0644);
                if (targetFD == -1) 
                { 
                    perror("cannot open file for output"); 
                    exit(1); 
                }
                // Redirect stdout to target file
                int result = dup2(targetFD, 1);
                if (result == -1) { 
                    perror("target dup2()"); 
                    exit(2); 
                }
            }

            // if input redirection is not specified in the background command
            if(output->redirectInput == false && output->background == true && ignoreBackground == false)
            {
                backgroundNoRedirectInput();
            }

            // if output redirection is not specified in the background command
            if(output->redirectOutput == false && output->background == true && ignoreBackground == false)
            {
                backgroundNoRedirectOuput();
            }
            
            // pass commands from list argv
            execvp(argv[0], argv);
            // exec only returns if there is an error
            perror("cannot find command to run");
            exit(1);
            break;
            
        default: // In the parent process
            
            // runs as a background process
            if(output->background == true && ignoreBackground == false)
            {
                // use flag WNOHANG to non-block process. 
                pid_t childPid = waitpid(spawnPid, &childStatus, WNOHANG);
                // if a child happens to be terminated, remove pid from array.
                if(childPid != 0)
                {
                    for(int i=0; i < MAX_ARGUMENTS; i++ )
                    {
                        if(pidArray[i] == spawnPid)
                        {
                            pidArray[i] = 0;
                            break;
                        } 
                    }
                }
                printf("background pid is %d\n", spawnPid);
                fflush(stdout);
            }
            else
            {   
                // runs as a foreground process, wait for child to terminate
                pid_t childPid = waitpid(spawnPid, &childStatus, 0);
                // remove terminated child's pid from array
                for(int i=0; i < MAX_ARGUMENTS; i++ )
                {
                    if(pidArray[i] == spawnPid)
                    {
                        pidArray[i] = 0;
                        break;
                    }
                }
                // if child was terminated by a signal, print the respective signal. 
                if (WIFSIGNALED(childStatus))
                {
                    printf("terminated by signal %d\n",  WTERMSIG(childStatus));
                    fflush(stdout);
                }
            }
            break;
	}
    
    // return the child status, so it can be saved in exitStatus variable. 
    return childStatus;
}

/*
Checks all children PIDs that have not yet been terminated/completed, 
if a child has been terminated, it cleans up the process and prints the 
the child process ID
*/
void checkChildTermination(pid_t pidArray[])
{
    int childStatus;
    for(int i=0; i < MAX_ARGUMENTS; i++)
    {
        // check if child pid from array has been terminated. 
        pid_t childPid = waitpid(pidArray[i], &childStatus, WNOHANG);
        if(childPid > 0)
        {
            printf("background pid %d is done: ", pidArray[i]);
            fflush(stdout);
            displayStatus(childStatus);
            // remove child pid from array since it has been terminated.
            pidArray[i] = 0; 
        }
    }
}

/*
Function process all the commands, it distinguishes and runs built-in 
commands (exit, cd, status) as well as indetifies redirections and 
background commands. 
*/
int processCommands(struct commands *output, char *args[], int exitStatus, pid_t pidArray[])
{
    for (int i=0; output->command[i] != 0; i++)
    {   
        // if command was a blank line, return to next input. 
        if(output->blankLine == true)
        {
            return 0;
        }

        // reached the end of the command list.
        if(strcmp(output->command[i], "\0") == 0)
        {   
            args[i] = NULL;
            break;
        }

        // if there is input redirection in the command.
        if(strcmp(output->command[i], "<") == 0)
        {
            // there is a Input redirect
            output->redirectInput = true;
            // save file index for later use
            output->inputFileIndex = i + 1;
            continue;
        }

        // if there is output redirection in the command.
        if(strcmp(output->command[i], ">") == 0)
        {
            // there is an Output redirect
            output->redirectOutput = true;
            // save file index for later use
            output->outputFileIndex = i + 1 ;
            continue;
        }

        // identifies if command will run as a background process. 
        // Character & has to be the last and only character in the command list.
        if(output->command[i][0] == '&' && output->command[i][1] == 0 && i == (output->numberOfCommands - 1))
        {
            output->background = true;
            continue;
        }

        // comment line is ignored.
        else if(output->command[i][0] == '#')
        {
            return 0;
        }

        // displays current exit status
        else if(strcmp(output->command[i], "status") == 0 && i == 0 )
        {
            displayStatus(exitStatus);
            return 0;
        }

        // exit command kills all ongoing foreground and background processes
        // and signals 1 to exit in main. 
        else if(strcmp(output->command[i], "exit") == 0 && i == 0)
        {
            for(int i=0; i < MAX_ARGUMENTS; i++ )
            {
                if(pidArray[i] == 0)
                {
                    break;
                }
                else
                {   // if pid is in the array, kill it
                    kill(pidArray[i], SIGKILL);
                }
            }
            return 1;
        }

        // changes the directory
        else if(strcmp(output->command[i], "cd") == 0 && i == 0)
        {
            // if there is an argument after cd.
            if(strlen(output->command[i+1]) != 0)
            {
                chdir(output->command[i+1]);
            }
            //if there is only cd and no argument, 
            else
            {
                chdir(getenv("HOME"));
            }
            return 0;
        }
        else
        {  
            // add command to args which will be later used in execvp().
            args[i] = output->command[i];
        }    
    }
}

/*
Tokenizes command input string and saves the commands to a 2D list.
A struct commands is created to store the 2D list.
Token is analyzed to find and convert expansion variables. 
*/
struct commands *processInput(char *commandInput)
{
    // create struct to contain command input and other control variables.
    struct commands *currCommand = malloc(sizeof(struct commands));

    // initialize bool values to false
    currCommand->redirectInput = false;
    currCommand->redirectOutput = false; 
    currCommand->background = false;
    currCommand->blankLine = false;

    char *saveptr;
    char *token = strtok_r(commandInput, " \n", &saveptr);

    //blank lines are ingnored.
    if(token == 0)
    {
        currCommand->blankLine = true;
        return currCommand;
    }

    // tokenize the input in order to extract individual commands. 
    strcpy(currCommand->command[0], token);
    for (int i=1; i<MAX_ARGUMENTS; i++)
        {    
            token = strtok_r(NULL, " \n", &saveptr);
                // if reaches the end of the string.
                if(token == 0)
                {   
                    // save the number of commands that were passed. 
                    currCommand->numberOfCommands = i;
                    break;
                }
                else
                {
                    // each token's character is analyzed in order to find the characters '$$' (variable expansion)
                    int j = 0;
                    while(token[j] != '\0')
                    {
                        if(token[j] == '$')
                        {   // if there are two $$ characters, they are replace with current process ID
                            if(token[j+1] ==  '$')
                            {   
                                sprintf(currCommand->command[i], "%s%d", currCommand->command[i], getpid());
                                j += 2;
                            }
                            // if not, the single $ charater is added to the string
                            else
                            {
                                strncat(currCommand->command[i], &token[j], 1);
                                j++;
                            }
                        }
                        // if there are no $ charaters, the current character is added to the string
                        else
                        {
                            strncat(currCommand->command[i], &token[j], 1);
                            j++;
                        }
                    }
                }
        } 
    return currCommand; 
}

/*
Receives command string and stores it at commandInput array.
*/
void receiveInput(char *commandInput)
{
    printf(":");
    fflush(stdout);
    fgets(commandInput, MAX_LENGTH, stdin);
}

/*
Sets SIGINT signal to be ignored.
*/
void ignoreSIGINT() 
{
         // Initialize SIGINT_action struct to be empty
        struct sigaction SIGINT_action = {{0}};
        // Register SIG_IGN as the signal handler
        SIGINT_action.sa_handler = SIG_IGN;
        // Block all catchable signals while handle_SIGINT is running
        sigfillset(&SIGINT_action.sa_mask);
        // No flags set
        SIGINT_action.sa_flags = 0;
        // Install our signal handler
	    sigaction(SIGINT, &SIGINT_action, NULL);
}

/*
Catches SIGINT signal and let default signal action to occur.
*/
void catchSIGINT()
{
         // Initialize SIGINT_action struct to be empty
        struct sigaction SIGINT_action = {{0}};
        // Fill out the SIGINT_action struct
        // Register SIG_IGN as the signal handler
        SIGINT_action.sa_handler = SIG_DFL;
        // Block all catchable signals
        sigfillset(&SIGINT_action.sa_mask);
        // No flags set
        SIGINT_action.sa_flags = 0;
        // Install our signal handler
	    sigaction(SIGINT, &SIGINT_action, NULL);
}

/*
Set SIGTSTP signal to be ignored.
*/
void ignoreSIGTSTP()    
{
        // Initialize SIGTSTP_action struct to be empty
        struct sigaction SIGTSTP_action = {{0}};
        // Register SIGTSTP_action as the signal handler
        SIGTSTP_action.sa_handler = SIG_IGN;
        // Block all catchable signals
        sigfillset(&SIGTSTP_action.sa_mask);
        // No flags set
        SIGTSTP_action.sa_flags = 0;
        // Install our signal handler
	    sigaction(SIGTSTP, &SIGTSTP_action, NULL);
}

/*
Catches SIGTSTP signal and passes through a signal handler.
*/
void catchSIGTSTP() 
{
        // Initialize SIGTSTP_action struct to be empty
        struct sigaction SIGTSTP_action = {{0}};
        // Register SIGTSTP_action as the signal handler
        SIGTSTP_action.sa_handler = handleSIGTSTP;
        // Block all catchable signals
        sigfillset(&SIGTSTP_action.sa_mask);
        // No flags set
        SIGTSTP_action.sa_flags = SA_RESTART;
        // Install our signal handler
	    sigaction(SIGTSTP, &SIGTSTP_action, NULL);
}

/*
SIGTSTP signal handler, enables and disables background mode.
*/
void handleSIGTSTP(int signo)
{
    // background processes wont be allowed to run.
    if(ignoreBackground == false)
    {
        ignoreBackground = true;
        char *message = " Entering foreground-only mode (& is now ignored)\n:";
        write(STDOUT_FILENO, message, 53);
    }
     // background processes are allowed to be run again.
    else
    {
        ignoreBackground = false;
        char *message = " Exiting foreground-only mode\n:";
        write(STDOUT_FILENO, message, 33);
    }
}


/*
Receives input string, process input into commands, 
sorts command types, runs built in commands and execute commands.
*/
int main()
{
    // variable used by SIGTSTP signal handler to control background processes.
    ignoreBackground = false;

    // holds the PIDs of all ongoing foreground and background processes. 
    pid_t pidArray[MAX_ARGUMENTS] = {0};
    int exitStatus;

    while(true)
    {   
        // parent process ignores SIGINT signal. 
        ignoreSIGINT();
        // parent process catches SIGTSTP signal
        catchSIGTSTP();
        
        char commandInput[MAX_LENGTH];
 
        // receive command input string.
        receiveInput(commandInput);
   
        // process command input into individual commands and saves them to a struct
        struct commands *output = processInput(commandInput);

        // create a new list (args) that will be later used in execvp().
        char *args[output->numberOfCommands + 1];
        // all values in args are set to NULL.
        for(int i=0; i<output->numberOfCommands + 1; i++)
        {
            args[i] = NULL;
        }

        // sorts out the commands, and place commands that will be executed in args list. 
        int response = processCommands(output, args, exitStatus, pidArray);
    
        // if command exit is requested, program exits. 
        if(response == 1)
        {   
            free(output);
            exit(0);
            break;
        }

        if(response != 0)
        {
            // execute non built in commands
            exitStatus = executeCommands(output, args, exitStatus, pidArray);
            // checks if child process have been terminated and cleans them up.
            checkChildTermination(pidArray);
        }
    }
}