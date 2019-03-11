#include <sys/types.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/wait.h>
#include <string.h>
#include <signal.h>
#include <assert.h>
#include <fcntl.h>
#include <errno.h>

// Global Variable: BGAllowed: int. 1 means background processes allowed. 0 means not allowed.
int BGAllowed = 1;

/************************************************************
 * struct CommandLine
 * Description: a dynamic array of char strings
 * Attributes: capacity: int, number of char* allocated in memory
 * 	       size: int, number of strings stored
 * 	       maxCommandLength: int, the length of the longest command
 * 	       bg: int, 1 means backgroun process, 0 means foreground
 * 	       arr: a pointer to a dynamic array of strings
 * *********************************************************/
struct CommandLine
{
	int capacity;
	int size;
	int bg;
	char* inputFile;
	char* outputFile;
	char **arr;
};

/*************************************************************
 * Function: initCommandLine
 * Description: Initialize the CommandLine
 * Arguments: r: a pointer to struct CommandLine
 * 	      capacity: int, the specified capacity of CommandLine
 * Precondition: A struct CommandLine is declared
 * Postcondition: capacity is set to the argument passed in,
 * 		  size is 0, and an array of char* of size
 * 		  capacity is allocated in memory.
 * Return value: N/A
 * **********************************************************/
void initCommandLine(struct CommandLine *r, int capacity)
{
	r->arr = (char**)malloc(capacity * sizeof(char*));
	assert(r->arr);
	int i;
	for (i=0; i<capacity; i++)
		r->arr[i] = NULL;
	r->capacity = capacity;
	r->size = 0;
	r->bg = 0;
	r->inputFile = NULL;
	r->outputFile = NULL;
}

/**********************************************************************
 * Function: addCommandLine
 * Description: add the string to the CommandLine
 * Arguments: r: a pointer to CommandLine
 * 	      str: a pointer to a string
 * Precondition: r has been initialized. str is valid
 * Postcondition: str is added to r. The size of r is incremented. If 
 * 		  the capacity of r was reached, then a new memory with
 * 		  twice the original capacity is allocated.
 * Return value: N/A
 * *********************************************************************/
void addCommandLine(struct CommandLine *r, char *str)
{
	if (r->size == r->capacity-1)
	{
		char  **temp = (char**)calloc(2 *r->capacity, sizeof(char*));
		assert(temp);
		int i;
		for (i=0; i< r->size; i++)
		{
			temp[i] = (char*)calloc(strlen(r->arr[i])+1,sizeof(char));
			assert(temp[i]);
			strcpy(temp[i], r->arr[i]);
			free(r->arr[i]);
		}
		free(r->arr);
		r->arr = temp;
		r->capacity *= 2;
	}
	
	r->arr[r->size] = (char*)calloc(strlen(str)+1, sizeof(char));
	assert(r->arr[r->size]);
	strcpy(r->arr[r->size], str);
	r->size++;
}

/*********************************************************************
 * Function: freeCommandLine
 * Description: this function frees the memory dynamically allocated
 * 		for the attributes in CommandLine
 * Precondition: N/A
 * Postcondition: the memory for arr, inputFile, and outputFile is freed
 * *******************************************************************/
void freeCommandLine(struct CommandLine *r)
{
	int i=0;
	for (i=0; i< r->size; i++)
	{
		free(r->arr[i]);
		r->arr[i] = NULL;
	}
	free(r->arr);
	r->arr = NULL;
	free(r->inputFile);
	r->inputFile = NULL;
	free(r->outputFile);
	r->outputFile = NULL;
	r->size = 0;
	r->capacity = 0;
	r->bg = 0;
}


/**********************************************************************
 * struct Link
 * Description: this struct stores the information for a pid
 * Attributes: pidNo: pid_t, pid of the process
 * 	       command: a pointer to a struct CommandLine that has the
 * 	       		information for the process
 * 	       builtIn: int. 1 means the process is a built-in bash
 * 	       		function, 0 otherwise.
 * 	       next: a pointer to a struct Link
 * *******************************************************************/
struct Link
{
	pid_t pidNo;
	struct CommandLine* command;
	int builtIn;
	struct Link* next;
};

/*************************************************************************
 * Function initLink
 * Description: Initialize a struct Link
 * Argument: l: a pointer to a struct Link
 * 	     p: pid_t, a process id
 * 	     c: a pointer to a struct CommandLine
 * 	     b: int, indicating whether the process is a built-in function
 * Precondition: l has been declared
 * Postcondition: pidNo is set to l, command is set to c, builtIn is set 
 * 		  to b, and next is set to NULL.
 * ***********************************************************************/
void initLink(struct Link* l, pid_t p, struct CommandLine* c, int b)
{
	l->pidNo = p;
	l->command = c;
	l->builtIn = b;
	l->next = NULL;
}

/*************************************************************************
 * Function: freeLink
 * Description: The attribute command is dynamically allocated elsewhere
 * 		in the shell function. This function frees the memory of
 * 		command. It also frees the memory for struct Link.
 * Argument: l: a pointer to a struct Link
 * Precondition: N/A
 * Postcondition: The memory dynamically allocated to the attributes of
 * 		  l and l is both freed.
 * ***********************************************************************/
void freeLink(struct Link *l)
{
	freeCommandLine(l->command);
	free(l->command);
	free(l);
}

/*********************************************************************
 * struct ChildrenPids
 * Description: this struct maintains a linked list of struct Link.
 * Attributes: size: the number of pid stored in the linked list
 *	       list: a pointer to the first element of the linked list
 *********************************************************************/		
struct ChildrenPids
{
	int size;
	struct Link* list; 
};

/*************************************************************
 * Function: initChildrenPids
 * Description: Initialize the struct childrenPids
 * Arguments: children: a pointer to struct ChildrenPids
 * Precondition: A struct ChildrenPids is declared and allocated
 * Postcondition: size is set to 0, and list is set to NULL.
 * Return value: N/A
 * **********************************************************/
void initChildrenPids(struct ChildrenPids* children)
{
	children->size = 0;
	children->list = NULL;
}

/**********************************************************************
 * Function: addChildrenPids
 * Description: construct a struct Link from the arguments and add it
 * 		to the struct ChildrenPids
 * Arguments: children: a pointer to struct ChildrenPids
 * 	      p: pid_t, pid to be added
 * 	      c: a pointer to a struct CommandLine
 * 	      b: int, 1 means build-in process, 0 means not built-in 
 * Precondition: children has been initialized.
 * Postcondition: a struct Link is constructed from pidNo, command, and 
 * 		  builtIn. And it is added to the linked list maintained by 
 * 		  struct ChildrenPids
 * *******************************************************************/
void addChildrenPids(struct ChildrenPids* children, pid_t pidNo, struct CommandLine* command, int builtIn)
{
	struct Link *l = (struct Link*)malloc(sizeof(struct Link));
	assert(l);
	initLink(l, pidNo, command, builtIn);
	if (children->size == 0)
		children->list = l;
	else
	{
		struct Link *temp;
		temp = children->list;
		while(temp->next)
			temp = temp->next;
		temp->next = l;
	}
	children->size++;
}

/**********************************************************************
 * Function: inChildrenPids
 * Description: the function searches the linked list in struct childrenPids
 * 		for the given pid. If the pid is in the array, then
 * 		the function returns 1. 0 otherwise.
 * Arguments: children: a pointer to the struct childrenPids
 * 	      num: pid_t, the pid that is searched for in children
 * Precondition: N/A
 * Postcondition: N/A
 * Return values: 1 if num is in children. 0 otherwise.
 **********************************************************************/ 	   
int inChildrenPids(struct ChildrenPids* children, pid_t num)
{
	if (children->size == 0)
		return 0;
	
	struct Link *temp = children->list;
	while(temp)
	{
		if (temp->pidNo == num)
			return 1;
		temp = temp->next;
	}
	return 0;
}

/*********************************************************************
 * Function: deleteChildrenPids
 * Description: if the pid is in the argument struct childrenPids, then 
 * 		the struct Link that stores is is deleted, and the function 
 * 		returns 1. If the pid is not stored, then 0 is returned.
 * Arguments: children: a pointer to the struct childrenPids
 * 	      num: pid_t, the pid that should be deleted from children
 * Precondition: N/A
 * Postcondition: if num is in children, then the Link  is deleted, and the size
 * 		  of children is decremented. If num is not in children,
 * 		  children is not changed.
 * Return values: 1 if num was in children and deleted. 0 otherwise.
 **********************************************************************/ 	   
int deleteChildrenPids(struct ChildrenPids* children, pid_t num)
{
	int in = inChildrenPids(children, num);
	if (in == 0) //num not in children
		return 0;
	struct Link *temp, *junk;
	if (children->list->pidNo == num)  //if it's the first element in the linked list 
	{
		junk = children->list;
		children->list = junk->next;
	}
	else
	{
		temp = children->list;
		while(temp->next)
		{
			if (temp->next->pidNo == num)
			{
				junk = temp->next;
				temp->next = junk->next;
				break;
			}
			temp = temp->next;
		}
	}
	freeLink(junk);
	children->size--;
	return 1;
}

/*********************************************************************
 * Function: freeChildrenPids
 * Description: this function frees the memory dynamically allocated
 * 		for the linked list
 * Precondition: N/A
 * Postcondition: the memory for linked list is freed.
 * *******************************************************************/
void freeChildrenPids(struct ChildrenPids* children)
{
	struct Link *temp, *junk;
	temp = children->list;
	while (temp)
	{
		junk = temp;
		temp = temp->next;
		freeLink(junk);
	}
	children->list = NULL;
	children->size = 0;
}



/*****************************************************************************
 * Function: readline
 * Description: read a line of user input. It will read a line despite signal
 * 		interruptions
 * Argument: line: char*
 * Precondition: char* is NULL or not initialized.
 * Postcondition: char* is allocated and filled with user input
 * Return value: the size of the input
 * ***************************************************************************/
int readLine(char **line)
{
	*line = NULL;
	int numCharsEntered = -5;
	size_t bufferSize = 0;
	while (1)
	{
		numCharsEntered = getline(line, &bufferSize, stdin);
		if (numCharsEntered == -1)
			clearerr(stdin);
		else
			break;  //exit loop. The input is valid.
	}

	return numCharsEntered;
}

/***********************************************************************************
 * Function: expandShellPid
 * Description: this function replaces "$$" in the string to the pid of the current
 * 		process.
 * Argument: str, char*
 * Precondition: str has "$$"
 * Postcondition: "$$" is replaced with pid
 * Return value: a char* pointed to a string that has pid
 * ********************************************************************************/
char* expandShellPid(char* str)
{
	char* pos = strstr(str, "$$");
	char* newStr = (char*)calloc(strlen(str)+10, sizeof(char)); //return string
	char pidStr[8]; //char string of pid
	memset(pidStr, '\0', 8);
	sprintf(pidStr, "%d", getpid());
	char* i;
	int j, k;
	for (i = str, j=0; i < pos; i++, j++) //copy over every char of str before $$
		newStr[j] = *i;
	for (k=0; k<strlen(pidStr); k++, j++) //copy over pid
		newStr[j] = pidStr[k];
	i += 2;  //copy over every char of str after $$
	for ( ;i < str+strlen(str); i++, j++)
		newStr[j] = *i;
	return newStr;
}


/*******************************************************************************
 * Function: parseLine
 * Description: parse the input line into words and store the words in struct
 * 		CommandLine
 * Argument: line: a char* to be parsed
 * Precondition: line is composed of words separated by spaces. It doesn't have an
 * 		ending '\n'.
 * Postcondition: line is altered by strtok
 * Return value: a pointer to a dynamically allocated strct CommandLine that stores
 * 		 the words of the line, the input and output, and whether or not
 * 		 background process
 * *****************************************************************************/
struct CommandLine*  parseLine(char* line)
{	
	char* token = NULL; // set null pointer
	char* rest = line;

	struct CommandLine* commands = (struct CommandLine*)malloc(sizeof(struct CommandLine));
	assert(commands);
	initCommandLine(commands,10);
	while ((token = strtok_r(rest, " ", &rest)))
	{	
		if (strcmp(token, "<") == 0)  //set the inputFile
		{
			token = strtok_r(rest, " ", &rest);
			commands->inputFile = (char*)calloc(strlen(token)+1, sizeof(char));
			assert(commands->inputFile);
			strcpy(commands->inputFile, token);
			continue;
		}	
		if (strcmp(token, ">") == 0)  //set the outputFile
		{
			token = strtok_r(rest, " ", &rest);
			commands->outputFile = (char*)calloc(strlen(token)+1, sizeof(char));
			assert(commands->outputFile);
			strcpy(commands->outputFile, token);
			continue;
		}
		if (strstr(token, "$$") != 0)  //expand "$$" and add the command to the struct
		{
			char *expanded = expandShellPid(token);
			while (strstr(expanded, "$$") != 0)
			{
				char *temp = expandShellPid(expanded);
				free(expanded);
				expanded = temp;
			}
			addCommandLine(commands, expanded);
			free(expanded);
			continue;
		}
		addCommandLine(commands,token);
	}
	
	// Set up for the background process
	if (commands->size > 0)
	{
		if(strcmp("&", commands->arr[commands->size - 1]) == 0)
		{
			commands->bg = 1;
			free(commands->arr[commands->size-1]);
			commands->arr[commands->size-1] = NULL;
			commands->size--;
			// if no input or output is provided, then set them to /dev/null
			if (commands->inputFile == NULL)
			{
				commands->inputFile = (char*)calloc(10, sizeof(char));
				assert(commands->inputFile);
				strcpy(commands->inputFile, "/dev/null");
			}
			if (commands->outputFile == NULL)
			{
				commands->outputFile = (char*)calloc(10, sizeof(char));
				assert(commands->outputFile);
				strcpy(commands->outputFile, "/dev/null");
			}

		}
	}
	return commands;
}



/*******************************************************************
 * Function: cdHandle
 * Description: built-in shell function. If the user type in cd,
 * 		then this function is invoked. If the user typed 
 * 		in the directory, then the current working directory
 * 		is changed to it. Otherwise, the directory is changed
 * 		to the environment variable HOME. The exit value is 0
 * 		if the directory is successfully changed. Otherwise
 * 		the exit value is 1.
 * Argument: c: a pointer to a struct CommandLine that has the info
 * 		for cd command.
 * Precondition: N/A
 * Postcondition: the current working directory is changed to either
 * 		  the one specified by the user, or HOME.
 * ******************************************************************/
void cdHandle(struct CommandLine *c)
{
	int result;
	char path[1024];
	memset(path, '\0', 1024);
	if (c->size == 1)   //If there is only one command cd
	{	//get $HOME
		sprintf(path, "%s", getenv("HOME"));
	}
	else if (c->size >= 2)
	{
		strcpy(path, c->arr[1]);
	}
	

	result = chdir(path);
	if (result == -1)
	{
		perror("cd error");
	}

}

/*******************************************************************************
 * Function: exitHandle
 * Description: This is a built-in shell function. When the user typed in exit,
 * 		then this function is invoked. It kills all the processes or jobs the 
 * 		shell has not finished.
 * Argument: children, a pointer to a struct ChildrenPids that stores all the
 * 	     pids of the children process that are still running.
 * Precondition: N/A
 * Postcondition: all the children processes are killed.
 * ****************************************************************************/
void exitHandle(struct ChildrenPids *children)
{	
	struct Link* temp;
	int result;
	int i=0;
	while (children->size != 0 && i < 50)
	{
		temp = children->list;
		result = kill(temp->pidNo, SIGTERM);
		if (result == 0)
		{
			printf("Process %d terminated by SIGTERM\n", temp->pidNo);
			fflush(stdout);
			int deleted = deleteChildrenPids(children, temp->pidNo);
			if (deleted == 0)
			{
				printf("Sucessfully deleted info for this process\n");
				fflush(stdout);
			}
			else
			{
				printf("Error deleting info for this process\n");
				fflush(stdout);
			}

		}
		else
		{
			perror("Exit Failure: ");
		}
		i++;
	}
}

/*******************************************************************************************
 * Function: statusHandle
 * Description: This function prints to the terminal the exit status or the terminating signal
 * 		of the last foreground process ran by the shell.
 * Argument: exitMethod: int, the exit method of the last foreground process
 * Precondition: N/A
 * Postcondition: The exit status or terminating signal is printed to the terminal.
 * ******************************************************************************************/
void statusHandle(int exitMethod)
{
	if (WIFEXITED(exitMethod) != 0)   //child exited normally
	{
		int exitStatus = WEXITSTATUS(exitMethod);
		if (exitStatus != 0)
			exitStatus = 1;
		printf("exit value %d\n",exitStatus);
		fflush(stdout);
	}		
	else if (WIFSIGNALED(exitMethod) != 0) //child terminated by a signal
	{	
		printf("terminated by signal %d\n", WTERMSIG(exitMethod));
		fflush(stdout);
	}
}


/*********************************************************************************
 * Function: execHandle
 * Dexcription: This function performs redirection of stdin and stdout if needed,
 * 		then calls execvp with the information in the struct CommandLine
 * Argument: c: a pointer to a struct Commandline that stores the information for
 * 		redirection and execvp
 * Precondition: N/A
 * Postcondition: The execvp runs successfully. Or the error message is output to
 * 		  the terminal and exit with 1.
 * Return value: N/A
 * ******************************************************************************/
void execHandle(struct CommandLine *c)
{
	//redirecting stdin and stdout
	int sourceFD = 0, targetFD = 1, result;
	if (c->inputFile)
	{
		sourceFD = open(c->inputFile, O_RDONLY);
		if (sourceFD == -1)
		{
//			perror("source open()");
			exit(errno);
		}
		result = dup2(sourceFD, 0);
		if (result == -1)
		{
//			perror("source dup2()"); 
			exit(errno);
		}
	}

	if (c->outputFile)
	{
		targetFD = open(c->outputFile, O_WRONLY | O_CREAT | O_TRUNC, 0644);
		if (targetFD == -1)
		{
//			perror("target open()");
			exit(errno);
		}
		result = dup2(targetFD, 1);
		if (result == -1)
		{
//			perror("target dup2()");
			exit(errno);
		}
	}
	
	// redirecting stdin and stdout
	if (execvp(c->arr[0], c->arr) < 0)
	{
//		perror("No such command");
//		printf("errno is %d: %s\n",  errno, strerror(errno));
//		fflush(stdout);
		exit(errno);
	}
}


void parentCatchSIGTSTP(int signo)
{
//	char* message = "Parent Caught SIGTSTP\n";
//	write(STDOUT_FILENO, message, 22);
	if (BGAllowed == 1)
	{
		write(STDOUT_FILENO, "Enter foreground only mode (& is ignored)\n: ",45);
		BGAllowed = 0;
	}
	else
	{
		write(STDOUT_FILENO, "Exit foreground only mode\n: ", 28);
		BGAllowed = 1;
	}

}

/********************************************************************************************************
 * Function: decipherExitStatus
 * Description: determine the exit status of the passed in childExitMethod, and then store
 * 		appropriate values in the pointed int values;
 * Arguments: childExitMethod: int, the code returned by the terminated child process
 * 	      exited: int*, the int pointed to will be 1 if the child exited normally. 0 otherwise
 * 	      exitStatus: int*, the int pointed to will be the exit status if childe exited normally.
 * 	      signaled: int*, the int pointed to will be 1 if the child exited upon a signal. 0 otherwise.
 * 	      termSignal: in*, the int pointed to will be the signal that killed the child process.
 * Precondition: all the input are valid.
 * Postcondition: the deciphered information are stored in the addresses that the pointers point to.
 * ******************************************************************************************************/
void decipherExitStatus(int childExitMethod, int* exited, int* exitStatus, int* signaled, int* termSignal)
{

	if (WIFEXITED(childExitMethod) != 0)   //child terminated normally
	{
		*exited = 1;
		*exitStatus = WEXITSTATUS(childExitMethod);
		*signaled = 0;
		*termSignal = 0;
	}		
	else if (WIFSIGNALED(childExitMethod) != 0) //child terminated by a signal
	{	
		*signaled = 1;
		*termSignal = WTERMSIG(childExitMethod);
		*exited = 0;
		*exitStatus = 0;
	}					
}

/**********************************************************************************************
 * Function: checkBGChildren
 * Description: This function checks whether the background child processes have finished. If
 * 		yes, then it cleans them up.
 * Argument: children, a pointer to a struct ChildrenPids, which stores the child processes
 * Precondition: children has at least 1 element
 * Postcondition: If the child process in children has finished, then it is cleaned up and removed
 * 		  from children
 * Return value: N/A
 * **********************************************************************************************/
void checkBGChildren(struct ChildrenPids *children)
{		
	//use an array to store the pid of the background processes
	assert(children->size);
	pid_t *bgChildren=(pid_t*)calloc(children->size, sizeof(pid_t));
	int i=0;
	struct Link* temp = children->list;
	while (temp)   //copy the pids in the linked list to the array
	{
		bgChildren[i] = temp->pidNo;
		temp = temp->next;
		i++;
	}
	
	//Check each pid. If it's finished, clean up.
	int childExitMethod,exited, exitStatus, signaled, termSignal;
	pid_t currPid;
	for (i=0; i < children->size; i++)
	{
		if ((currPid = waitpid(bgChildren[i], &childExitMethod, WNOHANG)) != 0)
		{	
			deleteChildrenPids(children, currPid);
			
			printf("Background process %d has finished: ", currPid);
			fflush(stdout);
			//Decipher the type of termination
			decipherExitStatus(childExitMethod, &exited, &exitStatus, &signaled, &termSignal);
			if (exited)
			{
				if (exitStatus == 0)
				{
					printf("exit value 0\n");
					fflush(stdout);
				}
				if (exitStatus != 0)
				{
					printf("exited value %d\n", exitStatus); 
					fflush(stdout);
				}
			}
			else if (signaled)
			{
				printf("terminated by signal %d\n",  termSignal);
				fflush(stdout);
			}
			
		}
	}
	free(bgChildren);	
}

int main()
{
	const int MAXCHILDREN = 50; // the maximum number of children that can be spawned
	struct ChildrenPids children;
	initChildrenPids(&children);
	int lastFGExitMethod = 0;

	// Set up the signals
	struct sigaction pSIGTSTP_action = {{0}}, ignore_action = {{0}}, default_action = {{0}};

	ignore_action.sa_handler = SIG_IGN;
	ignore_action.sa_flags = SA_RESTART;

	sigaction(SIGINT, &ignore_action, NULL);  //setting parent SIGINT 
	
	default_action.sa_handler = SIG_DFL;
	sigfillset(&default_action.sa_mask);
	default_action.sa_flags = SA_RESTART;
	
	pSIGTSTP_action.sa_handler = parentCatchSIGTSTP;
	sigfillset(&pSIGTSTP_action.sa_mask);
	pSIGTSTP_action.sa_flags = SA_RESTART;
	sigaction(SIGTSTP, &pSIGTSTP_action, NULL);//setting parent SIGTSTP


	int exited, exitStatus, signaled, termSignal;
	int keepGoing = 1; //variable to tell the parent to keep taking commands
	// keep getting command line from user
	while (keepGoing)
	{
		printf(": ");
		fflush(stdout);	
		char *line = NULL;
		int sizeLine = readLine(&line);
		//remove the ending newline character
		line[sizeLine - 1]='\0';
		//commands will be freed after child process finishes
		struct CommandLine *commands = parseLine(line);
		//for debugging
		if (commands->size == 0 ||  commands->arr[0][0] == '#')
		{
			free(line);
			line = NULL;
			freeCommandLine(commands);
			free(commands);
			continue;
		}
		pid_t spawnPid = -5;
		int childExitMethod = -5;
		int procType = 0; // 0 is non built-in; 1 is cd; 2 is status; 3 is exit
		//assign the procType
		if (strcmp(commands->arr[0], "cd")==0)
		{
			procType = 1;
			cdHandle(commands);
		}
		else if (strcmp(commands->arr[0], "status") == 0)
		{
			procType = 2;
			statusHandle(lastFGExitMethod);
		}
		else if (strcmp(commands->arr[0], "exit") == 0)
		{
			procType = 3;
			keepGoing = 0;
			exitHandle(&children);
		}
		if (procType == 1 || procType == 2 || procType == 3)
		{
			free(line);
			line = NULL;
			freeCommandLine(commands);
			free(commands);
			continue;
		}
	
		/*decide whether the background processing is allowed
		the built-in functions, status, exit, and cd, run in foreground only
		the other functions will can run in background if the global variable BGAllowed
		is 1. If BGAllowed is 0, then run all the processes in foreground.*/
		if (procType != 0)
		{
			commands->bg = 0;
		}
		else if (BGAllowed == 0)
		{
			commands->bg = 0;
		}

		spawnPid = fork();		
		

		if (spawnPid == -1)  //fork error
		{	
			perror("Hull Breach!");
			exit(1);
		}
		else if (spawnPid == 0) //child process
		{
			
			if (commands->bg == 0)  //a foreground process
			{		
				//Will be terminated by SIGINT signal
				sigaction(SIGINT, &default_action, NULL);
			}
			
			//All processe will ignore SIGTSTP signal
			sigaction(SIGTSTP, &ignore_action, NULL);
		
			execHandle(commands);	
		}
		else  //parent process  
		{	
			addChildrenPids(&children,spawnPid,commands, commands->bg);		
			// if too many children are running at the same time, then abort
			if (children.size == MAXCHILDREN) 				
				abort();
			// let the user know that a background process has started
			if (commands->bg == 1)
			{
				printf("Background process %d starts\n", spawnPid);
				fflush(stdout);
			}		
		
			if (commands->bg == 0) // a foreground process
			{
				//block the SIGTSTP signal while waiting for the child process
				sigset_t toBlock;
				if (sigemptyset(&toBlock) == -1)
					perror("Failed to set sigset_t toBlock");
				if (sigaddset(&toBlock, SIGTSTP) == -1)
					perror("Failed to add SIGTSTP to sigset_t toBlock");
				if (sigprocmask(SIG_BLOCK, &toBlock, NULL) != 0)
					perror("SIGTSTP is not blocked in foreground child process");
				
				//Wait for the child to finish
				waitpid(spawnPid,&childExitMethod, 0);

				//unblock the SIGTSTP signal
				if (sigprocmask(SIG_UNBLOCK, &toBlock, NULL) != 0)
					perror("SIGTSTP is not unblocked");
	
				//clear up the command and pid saved for the child process
				deleteChildrenPids(&children, spawnPid);
			
				//Decipher the type of termination
//				printf("childExitMethod is %d\n", childExitMethod);
//				fflush(stdout);
				decipherExitStatus(childExitMethod, &exited, &exitStatus, &signaled, &termSignal);
				lastFGExitMethod = childExitMethod;
				if (exited)
				{
		//			lastFGExitMethod = 0;
					if (exitStatus != 0)
					{
		//				lastFGExitMethod = 1; 
						printf("Error: %s\n", strerror(exitStatus));
						fflush(stdout);
					}
				}
				else if (signaled)
				{
		//			lastFGExitMethod = childExitMethod;
					printf("Foreground process %d is terminated by signal %d\n", spawnPid, termSignal);
					fflush(stdout);
				}
			}
		}
		//check if any of the background child process has finished
		if (children.size != 0)
			checkBGChildren(&children);		

		free(line);
		line = NULL;
	}

	return 0;
}
