#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/wait.h>
#include <errno.h>
#include <time.h>
#include <linux/limits.h>
#include "array_list.h"
#include <readline/readline.h>
#include <readline/history.h>

#define DELIMITER " \n\t\a\r"
#define LEFT_HOINKY '<'
#define RIGHT_HOINKY '>'
#define PIPE '|'

#define MAX_ARGS 1024
#define MAX_FILENAME 100

int SPID = -1;
int total_jobs = 0;
pid_t shell_pgid;
int shell_terminal = STDIN_FILENO;
time_t shell_starting_time;

/* A single prcess like 'ls' */
typedef struct process{
	/* The next process in the pipeline */
	struct process *next; 
	/* All the arguments of the process */
	arraylist* arguments;
	/* The in and out file for this process */
	int in_file;
	int out_file;
	int status;
	/* The group id of this process */
	pid_t pgid;
	/* The pid of this process */
	pid_t pid;
	int finished;
	int stopped;
}process;

/* A job is a pipeline of the processes */
typedef struct job{
	/* The next job */
	struct job *next;
	/* The group id of this job */
	pid_t pgid;
	/* Track the time */
	time_t starting_time;
	char *original_process;
	process *head_process;
	int total_processes;
	/* Flag for background jobs */
	int is_background;
	int stopped;
}job;

/* Keep track of the jobs list by pointing to the head*/
job *jobs_list_head = NULL;
int job_head_id = -1;
int current_jobs = 0;

#define ANSI_COLOR_RED     "\x1b[31m"
#define ANSI_COLOR_BLUE    "\x1b[34m"
#define ANSI_COLOR_GREEN   "\x1b[32m"
#define ANSI_COLOR_YELLOW  "\x1b[33m"
#define ANSI_COLOR_CYAN    "\x1b[36m"
#define ANSI_COLOR_MAGENTA "\x1b[35m"
#define ANSI_COLOR_BLACK "\x1b[30m"
#define ANSI_COLOR_WHITE "\x1b[37m"

#define ANSI_COLOR_RED_BOLD     "\x1b[1;31m"
#define ANSI_COLOR_BLUE_BOLD    "\x1b[1;34m"
#define ANSI_COLOR_GREEN_BOLD   "\x1b[1;32m"
#define ANSI_COLOR_YELLOW_BOLD  "\x1b[1;33m"
#define ANSI_COLOR_CYAN_BOLD    "\x1b[1;36m"
#define ANSI_COLOR_MAGENTA_BOLD "\x1b[1;35m"
#define ANSI_COLOR_BLACK_BOLD "\x1b[1;30m"
#define ANSI_COLOR_WHITE_BOLD "\x1b[1;37m"

#define ANSI_COLOR_RESET   "\x1b[0m"

#define NUM_COLORS 8

const char *colors[NUM_COLORS] = {
	"red",
	"blue",
	"green",
	"yellow",
	"cyan",
	"magenta",
	"black",
	"white"
};

const char *color_codes[NUM_COLORS] = {
	ANSI_COLOR_RED,
	ANSI_COLOR_BLUE,
	ANSI_COLOR_GREEN,
	ANSI_COLOR_YELLOW,
	ANSI_COLOR_CYAN,
	ANSI_COLOR_MAGENTA,
	ANSI_COLOR_BLACK,
	ANSI_COLOR_WHITE
};

const char *color_codes_bold[NUM_COLORS] = {
	ANSI_COLOR_RED_BOLD,
	ANSI_COLOR_BLUE_BOLD,
	ANSI_COLOR_GREEN_BOLD,
	ANSI_COLOR_YELLOW_BOLD,
	ANSI_COLOR_CYAN_BOLD,
	ANSI_COLOR_MAGENTA_BOLD,
	ANSI_COLOR_BLACK_BOLD,
	ANSI_COLOR_WHITE_BOLD
};

#define PROMPT_SIZE 1024

/*The command prompt to be shown to the user */
char sfish_prompt[PROMPT_SIZE];

/*The toggle settings */
int user_toggle = 1;
int machine_toggle = 1;

/* The color toggle settings */
int user_color_toggle = 0;
int user_color_bold_toggle = 0;
int machine_color_toggle = 0;
int machine_color_bold_toggle = 0;
int user_color = -1;
int machine_color = -1;



/* To keep track of the previous directory */
char prev_dir[PATH_MAX];
int changed_dir = 0;


/*Keep track of return codes */
int status;

#define NUM_BUILTINS 13
#define HELP_MENU_LENGTH 14

/** The help statement. */
const char *HELP[HELP_MENU_LENGTH] = { 
"Welcome to sfish. These are the currently supported builtins:\n\n",
"help: Prints this menu to show all the builtins\n\n",
"exit: Exit the shell\n",
"cd: Change the current working directory\n",
"pwd: Prints the absolute path of the current working directory\n",
"prt: Prints the return code of the command that was last executed\n\n",
"chpmt: Change prompt settings \t Usage: chpmt SETTING TOGGLE \t Valid values for settings: 1) user"
": The field in the prompt 2) machine: The context field in the prompt \t Valid values for toggle: 0) DISABLED 1) ENABLED\n",
"chclr: Change prompt colors\tUsage: chpmt SETTING TOGGLE\n\n",
"jobs: List all jobs running in the background, their name, PID, job number with their status (running or suspended).\n",
"fg: Make the specified job number or ID go to foreground\n",
"bg: Make the specified job number or ID go to background\n",
"kill: Send a signal to a specified job. SIGTERM is sent by default\n",
"disown: Disown a child \n"
};

/* All the built in commands */
int sfish_help(process *process_given, int is_background);
int sfish_exit(process *process_given, int is_background);
int sfish_cd(process *process_given, int is_background);
int sfish_pwd(process *process_given, int is_background);
int sfish_prt(process *process_given, int is_background);
int sfish_cpmt(process *process_given, int is_background);
int sfish_chclr(process *process_given, int is_background);
int sfish_echo(process *process_given, int is_background);
int sfish_jobs(process *process_given, int is_background);
int sfish_fg(process *process_given, int is_background);
int sfish_bg(process *process_given, int is_background);
int sfish_disown(process *process_given, int is_background);
int sfish_kill(process *process_given, int is_background);



/* All the builtins */
const char *builtin_functions[NUM_BUILTINS] = {
	"help",
	"exit",
	"cd",
	"pwd",
	"prt",
	"chpmt",
	"chclr",
	"quit",
	"jobs",
	"fg",
	"bg",
	"disown",
	"kill"
};

/*The functions to execute all the builtins */
int (*builtin_function_exec[])(process *, int) = {
	&sfish_help,
	&sfish_exit,
	&sfish_cd,
	&sfish_pwd,
	&sfish_prt,
	&sfish_cpmt,
	&sfish_chclr,
	&sfish_exit,
	&sfish_jobs,
	&sfish_fg,
	&sfish_bg,
	&sfish_disown,
	&sfish_kill
};

char* check_exec(char* command);
job* parse_job(char* process);
void split_processes(job* job_to_return);
int sfish_execute(arraylist* commands);
void split_commands(process* process_to_return);
void set_io_pipes(job* job_to_run);
int execute_job(job *job_to_run);
void launch_process(process *process_to_run);
void Dup2( int old_fd, int new_fd );

void put_job_to_foreground(job *job_to_run, int continue_flag);
void put_job_to_background(job *job_to_run, int continue_flag);
int job_is_stopped(job *job_to_check);
int job_is_finished(job *job_to_check);

job *get_from_jobs_list(pid_t pgid, int remove_flag);
job *get_jid_from_jobs_list(pid_t job_id, int remove_flag);
