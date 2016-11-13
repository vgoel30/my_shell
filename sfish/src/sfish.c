#include "../include/sfish.h"

/* All the SIG handlers */
void sigchild_handler(int sig){
    /* Block all them signals */
    sigset_t mask, prev_mask;
    sigfillset(&mask);
    sigprocmask(SIG_BLOCK, &mask, &prev_mask);


    pid_t pid;

    while((pid = waitpid(-1, NULL, WNOHANG)) > 0){
        pid_t matched_pdig = -1;
        job *current_job = jobs_list_head;
        while(current_job != NULL){
            process *current_process = current_job -> head_process;
            while(current_process != NULL){
                if(current_process -> pid == pid){
                    matched_pdig = current_job -> pgid;
                    break;
                }
                current_process = current_process -> next;
            }
            if(matched_pdig != -1)
                break;
            current_job = current_job -> next;
        }

        if(matched_pdig != -1){
            /* Now check to see if the process is completely dead or not */
            if(kill(-matched_pdig, 0) != 0 && errno == ESRCH){
                printf("Job \"%s\" with process ID : %d has ended\n", current_job -> original_process, matched_pdig);
                get_from_jobs_list(matched_pdig, 1);
            }    
        }
        
    }
    /* unblock all the signals */
    sigprocmask(SIG_SETMASK, &prev_mask, NULL);
    return;
}

void sigint_handler(int sig){
    pid_t pid = getpid();
    if(pid != shell_pgid){
        kill(pid, 2);
        tcsetpgrp(shell_terminal, shell_pgid);
    }
    tcsetpgrp(shell_terminal, shell_pgid);
}

job *get_jid_from_jobs_list(pid_t job_id, int remove_flag){
    /* Block all the signals */
    sigset_t mask, prev_mask;
    sigfillset(&mask);
    sigprocmask(SIG_BLOCK, &mask, &prev_mask);

    int counter = 1;

    job *current_job = jobs_list_head;

    if(current_job == NULL){
        sigprocmask(SIG_SETMASK, &prev_mask, NULL);
        return NULL;
    }

    if(job_id == 1){
        if(remove_flag)
            jobs_list_head = jobs_list_head -> next;
        sigprocmask(SIG_SETMASK, &prev_mask, NULL);
        return current_job;
    }
    while(current_job != NULL && current_job -> next != NULL){
        if(counter == job_id - 1){
            job *to_return = current_job -> next;
            if(remove_flag)
                current_job -> next = current_job -> next -> next;
            sigprocmask(SIG_SETMASK, &prev_mask, NULL);
            return to_return;
        }
        current_job = current_job -> next;
        counter++;
    }
    sigprocmask(SIG_SETMASK, &prev_mask, NULL);
    return NULL;
}

job *get_from_jobs_list(pid_t pgid, int remove_flag){
    /* NBlock all the signals */
    sigset_t mask, prev_mask;
    sigfillset(&mask);
    sigprocmask(SIG_BLOCK, &mask, &prev_mask);

    job *current_job = jobs_list_head;

    if(current_job == NULL){
        sigprocmask(SIG_SETMASK, &prev_mask, NULL);
        return NULL;
    }

    if(current_job -> pgid == pgid){
        if(remove_flag)
            jobs_list_head = jobs_list_head -> next;
        sigprocmask(SIG_SETMASK, &prev_mask, NULL);
        return current_job;
    }

    while(current_job != NULL && current_job -> next != NULL){
        if(current_job -> next -> pgid == pgid){
            job *to_return = current_job -> next;
            if(remove_flag)
                current_job -> next = current_job -> next -> next;
            sigprocmask(SIG_SETMASK, &prev_mask, NULL);
            return to_return;
            //break;
        }
        else{
            current_job = current_job -> next;
        }
    }
    sigprocmask(SIG_SETMASK, &prev_mask, NULL);
    return NULL;
}

int job_is_stopped(job *job_to_check){
    process *current_process = job_to_check -> head_process;
    while(current_process != NULL){
        if(!current_process -> finished && !current_process -> stopped)
            return 1;
        current_process = current_process -> next;
    }
    return 0;
}

int job_is_finished(job *job_to_check){
    process *current_process = job_to_check -> head_process;
    while(current_process != NULL){
        if(!current_process -> finished)
            return 1;
        current_process = current_process -> next;
    }
    return 0;
}

int set_process_status(pid_t pid, int status){
    job *current_job;
    process *current_process;

    if(pid > 0){
        current_job = jobs_list_head;
        while(current_job != NULL){
            current_process = current_job -> head_process;
            while(current_process != NULL){
                if(current_process -> pid == pid){
                    current_process -> status = status;
                    if(WIFSTOPPED(status)){
                        current_process -> stopped = 1;
                        current_job -> stopped = 1;
                    }
                    else{
                        current_process -> finished = 1;
                    }
                    return 0;
                }
                current_process = current_process -> next;
            }
            current_job = current_job -> next;
        }
    }
    return -1;
}

void job_wait(job *job_to_wait_for){
    int status;
    pid_t pid;
    /* Check for all the children */
    do{
        pid = waitpid(-1, &status, WUNTRACED);
    }while(!set_process_status(pid, status) && !job_is_stopped(job_to_wait_for) && !job_is_finished(job_to_wait_for));

    /* Handle SIGSTP */
    if(WIFSTOPPED(status) != 0){
        /* Grab control of the shell */
        tcsetpgrp(shell_terminal, shell_pgid);
        /* Insert the job into the jobs list */
        job_to_wait_for -> stopped = 1;
        if(jobs_list_head == NULL){
            job_to_wait_for -> next = NULL;
            jobs_list_head = job_to_wait_for;
        }
        else{
            /* insert it elsewhere */
            job *current_job = jobs_list_head;
            while(current_job -> next != NULL){
                current_job = current_job -> next;
            }
            current_job -> next = job_to_wait_for;
            job_to_wait_for -> next = NULL;

        }
    }
}


void put_job_to_foreground(job *job_to_run, int continue_flag){
    job_to_run -> stopped = 0;
    /* Give the job the control */
    tcsetpgrp(shell_terminal, job_to_run -> pgid);
    /* Check to see if SIGCONT is to be sent */
    if(continue_flag){
        if(kill(-job_to_run -> pgid, SIGCONT) < 0)
            perror("SIGCONT error");
    }
    job_wait(job_to_run);
    /* GIve the control back to the shell */
    tcsetpgrp(shell_terminal, shell_pgid);
}

void put_job_to_background(job *job_to_run, int continue_flag){
    job_to_run -> stopped = 0;
    /* Check to see if SIGCONT is to be sent */
    if(continue_flag){
        if(kill(-job_to_run -> pgid, SIGCONT) < 0)
            perror("SIGCONT error");
    }
}

pid_t Fork(){
    pid_t pid = fork();
    if(pid < 0){
        perror("Couldn't fork");
        exit(1);
    }
    return pid;
}

void Pipe(int *array){
    if(pipe(array) < 0){
        perror("Couldn't pipe");
        exit(1);
    }
}

void Dup2(int old_fd, int new_fd){
    if(dup2(old_fd, new_fd) == -1){
        fprintf(stderr,"Error in dup2 with old fd: %d and new fd: %d\n", old_fd, new_fd);
        _exit(0);
    }
}

int sfish_help(process *process_given, int is_background){
    pid_t pid;
    pid = Fork();

    /* Child job */
    if(pid == 0){
        Dup2(process_given -> in_file, STDIN_FILENO);
        Dup2(process_given -> out_file, STDOUT_FILENO);

        if(process_given -> in_file != STDIN_FILENO){
            close(process_given -> in_file);
        }
        if(process_given -> out_file != STDOUT_FILENO){
            close(process_given -> out_file);
        }

        for(int i = 0; i < HELP_MENU_LENGTH - 1; i++){
            fprintf(stdout, "%s", HELP[i]);
        }
        exit(0);
    }
    /* Parent job */
    else{
        /* Wait for the child job and then reap it */
        if(!is_background)
            waitpid(-1, NULL, 0);
    }
    return 0;
}

/* when user types exit */
int sfish_exit(process *process_given, int is_background){
    clear_list(process_given -> arguments);
    free(process_given -> arguments -> data);
    free(process_given -> arguments);
    exit(EXIT_SUCCESS);
}

int sfish_cd(process *process_given, int is_background){
    changed_dir = 1;
    #ifdef DEBUG
    printf("OLD directory: %s\n",prev_dir);
    #endif

    if(process_given -> arguments -> current_elements == 1  || strcmp(get_element(process_given -> arguments, 1), "-") != 0){
        getcwd(prev_dir, sizeof(char) * PATH_MAX);
    }

    /* cd equates to changing to home directory */
    if(process_given -> arguments -> current_elements == 1){
        if(getenv("HOME")){
            chdir(getenv("HOME"));
        }
        else{
            perror("HOME environment variable not set!");
            return 1;
        }
    }

    /* If more than one arguments are there */
    else{
        /* Go to the last working directory the user was in */
        if(strcmp(get_element(process_given -> arguments, 1), "-") == 0){
            char temp[PATH_MAX];
            getcwd(temp, sizeof(char) * PATH_MAX);
            chdir(prev_dir);
            strcpy(prev_dir, temp); 
        }
        else if(chdir(get_element(process_given -> arguments, 1)) != 0){
            perror("Not a directory");
            return 1;
        }    
    }

    return 0;
}

int sfish_pwd(process *process_given, int is_background){
    pid_t pid;
    pid = Fork();

    if(pid == 0){
        Dup2(process_given -> in_file, STDIN_FILENO);
        Dup2(process_given -> out_file, STDOUT_FILENO);

        if(process_given -> in_file != STDIN_FILENO){
            close(process_given -> in_file);
        }
        if(process_given -> out_file != STDOUT_FILENO){
            close(process_given -> out_file);
        }

        char *cwd_buffer;
        /* Get the current working directory */
        if((cwd_buffer = getcwd(NULL, 0))){
            fprintf(stdout, "%s\n", cwd_buffer);
        }
        free(cwd_buffer);
        exit(0);
    }
    /* Parent job */
    else{
        /* Wait for the child job and then reap it */
        if(!is_background)
            waitpid(-1, NULL, 0);
    }

    return 0;
}

/* Print the last return code */
int sfish_prt(process *process_given, int is_background){
    pid_t pid;
    pid = Fork();

    if(pid == 0){
        Dup2(process_given -> in_file, STDIN_FILENO);
        Dup2(process_given -> out_file, STDOUT_FILENO);

        if(process_given -> in_file != STDIN_FILENO){
            close(process_given -> in_file);
        }
        if(process_given -> out_file != STDOUT_FILENO){
            close(process_given -> out_file);
        }

        fprintf(stdout, "%d\n",status);
        exit(0);
    }
    else{
        if(!is_background)
            waitpid(-1, NULL, 0);
    }
    return 0;
}

/*Change the process prompt settings */
int sfish_cpmt(process *process_given, int is_background){
    /* Proceed iff the process is of the form < cpmt SETTING TOGGLE > */
    if(process_given -> arguments -> current_elements >= 3){
        /* If the user wants to toggle user display settings */
        if(strcmp(get_element(process_given -> arguments, 1), "user") == 0){
            if(strcmp(get_element(process_given -> arguments, 2), "0") == 0){
                user_toggle = 0;
                return 0;
            }
            else if(strcmp(get_element(process_given -> arguments, 2), "1") == 0){
                user_toggle = 1;
                return 0;
            }
        }

        /* If the user wants to toggle machine display settings */
        else if(strcmp(get_element(process_given -> arguments, 1), "machine") == 0){
            if(strcmp(get_element(process_given -> arguments, 2), "0") == 0){
                machine_toggle = 0;
                return 0;
            }
            else if(strcmp(get_element(process_given -> arguments, 2), "1") == 0){
                machine_toggle = 1;
                return 0;
            }
        }
    }

    return 1;
}

/* Change the color and stuff of your process prompt */
int sfish_chclr(process *process_given, int is_background){
    /* Proceed forward iff the process is of the form < chlcr SETTING COLOR BOLD > */
    if(process_given -> arguments -> current_elements >= 4){
        /* If the user wants to toggle user display settings */
        if(strcmp(get_element(process_given -> arguments, 1), "user") == 0){
            int color_index = -1;
            /* Go through all the colors to check if there is a hit */
            for(int i = 0; i < NUM_COLORS; i++){
                if(strcmp(get_element(process_given -> arguments, 2), colors[i]) == 0){
                    user_color_toggle = 1;
                    color_index = i;
                    break;
                }
            }
            /* If a legit color wasn't found */
            if(color_index == -1)
                return 1;
            /* Set the user prompt color to the color index that matched */
            user_color = color_index;

            /* Now check for the bold color toggle */
            if(strcmp(get_element(process_given -> arguments, 3), "0") == 0){
                user_color_bold_toggle = 0;
                return 0;
            }
            else if(strcmp(get_element(process_given -> arguments, 3), "1") == 0){
                user_color_bold_toggle = 1;
                return 0;
            }
        }
        /* If the user wants to edit machine display settings */
        else if(strcmp(get_element(process_given -> arguments, 1), "machine") == 0){
            int color_index = -1;
            /* Go through all the colors to check if there is a hit */
            for(int i = 0; i < NUM_COLORS; i++){
                if(strcmp(get_element(process_given -> arguments, 2), colors[i]) == 0){
                    user_color_toggle = 1;
                    color_index = i;
                    break;
                }
            }
            /* If a legit color wasn't found */
            if(color_index == -1)
                return 1;
            /* Set the user prompt color to the color index that matched */
            machine_color = color_index;

            /* Now check for the bold color toggle */
            if(strcmp(get_element(process_given -> arguments, 3), "0") == 0){
                machine_color_bold_toggle = 0;
                return 0;
            }
            else if(strcmp(get_element(process_given -> arguments, 3), "1") == 0){
                machine_color_bold_toggle = 1;
                return 0;
            }
        }
    }
    return 1;
}

int sfish_jobs(process *process_given, int is_background){
    pid_t pid;
    pid = Fork();

    /* Child */
    if(pid == 0){
        #ifdef DEBUG
        printf("IN FILE FOR GIVEN PROCESS: %d\n",process_given -> in_file);
        printf("OUT FILE FOR GIVEN PROCESS: %d\n",process_given -> out_file);
        #endif
        Dup2(process_given -> in_file, STDIN_FILENO);
        Dup2(process_given -> out_file, STDOUT_FILENO);

        if(process_given -> in_file != STDIN_FILENO){
            close(process_given -> in_file);
        }
        if(process_given -> out_file != STDOUT_FILENO){
            close(process_given -> out_file);
        }

        int current_job_index = 0;
        if(jobs_list_head == NULL){
            fprintf(stdout, "%s\n","No jobs!");
        }
        job *current_job = jobs_list_head;
        /* go through all the jobs */
        while(current_job != NULL){
            current_job_index++;
            fprintf(stdout, "[%d]\t",current_job_index);
            int status;
            waitpid(-current_job -> pgid, &status, WNOHANG);
            /* Now check the status */
            if(current_job -> stopped){
                fprintf(stdout, "%s\t","Stopped");
            }
            else{
                fprintf(stdout, "%s\t","Running");
            }
            fprintf(stdout, "\t%d\t",current_job -> pgid);
            fprintf(stdout, "%s\n",current_job -> original_process);

            current_job = current_job -> next;
        }
        exit(0);    
    }
    else
        waitpid(-1, NULL, 0);
    return 0;
}

int sfish_fg(process *process_given, int is_background){
    if(process_given -> arguments -> current_elements != 2){
        perror("Incorrect usage for fg");
        return 1;
    }
    char *ptr;
    /* Find if a job ID has been given */
    if(strchr(get_element(process_given -> arguments, 1), '%') != NULL){
        if(strlen(get_element(process_given -> arguments, 1)) == 1){
            fprintf(stderr, "%s\n", "Please provide valid job ID!");
            return 1;
        }
        char *jid = strchr(get_element(process_given -> arguments, 1), '%') + 1;
        #ifdef DEBUG
        printf("jid: %s\n", jid);
        #endif
        pid_t job_id = strtol(jid, &ptr, 10);
        /* strol fails */
        if(errno == ERANGE){
           printf("%s\n", "Invalid ID!");
            return 1; 
        }
        if(job_id == 0)
            return 1;
        
        job *job_to_fg = get_jid_from_jobs_list(job_id, 1);

        if(job_to_fg != NULL){
            put_job_to_foreground(job_to_fg, 1);
            return 0;
        }
        else{
            printf("%s\n", "Invalid ID!");
        } 
    }

    else{
        pid_t pgid = strtol(get_element(process_given -> arguments, 1), &ptr, 10);
        /* strol fails */
        if(errno == ERANGE){
            printf("%s\n", "Invalid ID!");
            return 1;
        }
        if(pgid == 0)
            return 1;
        job *job_to_fg = get_from_jobs_list(pgid, 1);
        
        if(job_to_fg != NULL){
            put_job_to_foreground(job_to_fg, 1);
            return 0;
        }
        else{
            printf("%s\n", "Invalid ID!");
        }   
    }
    
    return 1;
}

int sfish_bg(process *process_given, int is_background){
    printf("%s\n","GOING TO BG");
    if(process_given -> arguments -> current_elements != 2){
        perror("Incorrect usage for bg");
        return 1;
    }
    char *ptr;
    /* Find if a job ID has been given */
    if(strchr(get_element(process_given -> arguments, 1), '%') != NULL){
        if(strlen(get_element(process_given -> arguments, 1)) == 1){
            fprintf(stderr, "%s\n", "Please provide valid job ID!");
            return 1;
        }
        char *jid = strchr(get_element(process_given -> arguments, 1), '%') + 1;
        #ifdef DEBUG
        printf("jid: %s\n", jid);
        #endif
        pid_t job_id = strtol(jid, &ptr, 10);
        /* strol fails */
        if(errno == ERANGE){
           printf("%s\n", "Invalid ID!");
            return 1; 
        }
        if(job_id == 0)
            return 1;
        
        job *job_to_bg = get_jid_from_jobs_list(job_id, 0);

        if(job_to_bg != NULL){
            put_job_to_background(job_to_bg, 1);
            return 0;
        }
        else{
            printf("%s\n", "Invalid ID!");
        } 
    }

    else{
        pid_t pgid = strtol(get_element(process_given -> arguments, 1), &ptr, 10);
        /* strol fails */
        if(errno == ERANGE){
            printf("%s\n", "Invalid ID!");
            return 1;
        }
        if(pgid == 0)
            return 1;
        job *job_to_bg = get_from_jobs_list(pgid, 0);
        
        if(job_to_bg != NULL){
            put_job_to_background(job_to_bg, 1);
            return 0;
        }
        else{
            printf("%s\n", "Invalid ID!");
        }   
    }
    
    return 1;  
}

int sfish_disown(process *process_given, int is_background){
    if(process_given -> arguments -> current_elements == 1){
        jobs_list_head = NULL;
        return 0;
    }

    if(process_given -> arguments -> current_elements > 2){
        printf("Incorrect usage for disown");
        return 1;
    }
    char *ptr;
    /* Find if a job ID has been given */
    if(strchr(get_element(process_given -> arguments, 1), '%') != NULL){
        if(strlen(get_element(process_given -> arguments, 1)) == 1){
            fprintf(stderr, "%s\n", "Please provide valid job ID!");
            return 1;
        }
        char *jid = strchr(get_element(process_given -> arguments, 1), '%') + 1;
        #ifdef DEBUG
        printf("jid: %s\n", jid);
        #endif
        pid_t job_id = strtol(jid, &ptr, 10);
        /* strol fails */
        if(errno == ERANGE){
           printf("%s\n", "Invalid ID!");
            return 1; 
        }
        if(job_id == 0)
            return 1;
        
        job *job_to_fg = get_jid_from_jobs_list(job_id, 1);

        if(job_to_fg != NULL){
            return 0;
        }
        else{
            printf("%s\n", "Invalid ID!");
        } 
    }

    else{
        pid_t pgid = strtol(get_element(process_given -> arguments, 1), &ptr, 10);
        /* strol fails */
        if(errno == ERANGE){
            printf("%s\n", "Invalid ID!");
            return 1;
        }
        if(pgid == 0)
            return 1;
        job *job_to_fg = get_from_jobs_list(pgid, 1);
        
        if(job_to_fg != NULL){
            put_job_to_foreground(job_to_fg, 1);
            return 0;
        }
        else{
            printf("%s\n", "Invalid ID!");
        }   
    }
    
    return 1;

}

int sfish_kill(process *process_given, int is_background){
    if(process_given -> arguments -> current_elements != 2 && process_given -> arguments -> current_elements != 3){
        printf("%s\n","Invalid usage of kill");
        return 1;
    }
    /* kill along with the ID */
    if(process_given -> arguments -> current_elements == 2){
        char *ptr;
        /* Find if a job ID has been given */
        if(strchr(get_element(process_given -> arguments, 1), '%') != NULL){
            if(strlen(get_element(process_given -> arguments, 1)) == 1){
                fprintf(stderr, "%s\n", "Please provide valid job ID!");
                return 1;
            }
            char *jid = strchr(get_element(process_given -> arguments, 1), '%') + 1;
            #ifdef DEBUG
            printf("jid: %s\n", jid);
            #endif
            pid_t job_id = strtol(jid, &ptr, 10);
            /* strol fails */
            if(errno == ERANGE){
               printf("%s\n", "Invalid ID!");
                return 1; 
            }
            if(job_id == 0)
                return 1;
            
            job *job_to_fg = get_jid_from_jobs_list(job_id, 1);

            if(job_to_fg != NULL){
                kill(-job_to_fg -> pgid, SIGTERM);
                return 0;
            }
            else{
                printf("%s\n", "Invalid ID!");
            } 
        }

        else{
            pid_t pid = strtol(get_element(process_given -> arguments, 1), &ptr, 10);
            /* strol fails */
            if(errno == ERANGE){
                printf("%s\n", "Invalid ID!");
                return 1;
            }
            if(pid == 0)
                return 1;

            pid_t matched_pdig = -1;
            job *current_job = jobs_list_head;

            /*Go through all the jobs to find the particular process */
            while(current_job != NULL){
                process *current_process = current_job -> head_process;
                while(current_process != NULL){
                    if(current_process -> pid == pid){
                        matched_pdig = current_job -> pgid;
                        break;
                    }
                    current_process = current_process -> next;
                }
                if(matched_pdig != -1)
                    break;
                current_job = current_job -> next;
            }

            if(matched_pdig == -1){
                printf("%s\n","Invalid PID!");
                return 1;
            }

            job *job_to_fg = current_job;
            
            if(job_to_fg != NULL){
                kill(-job_to_fg -> pgid, SIGTERM);
                return 0;
            }
            else{
                printf("%s\n", "Invalid ID!");
            }   
        }
    }
    /* Format: kill SIGNAL ID */
    else if(process_given -> arguments -> current_elements == 3){
        char *ptr;
        /* Find if a job ID has been given */
        if(strchr(get_element(process_given -> arguments, 2), '%') != NULL){
            if(strlen(get_element(process_given -> arguments, 2)) == 1){
                fprintf(stderr, "%s\n", "Please provide valid job ID!");
                return 1;
            }
            char *jid = strchr(get_element(process_given -> arguments, 2), '%') + 1;
            #ifdef DEBUG
            printf("jid: %s\n", jid);
            #endif
            pid_t job_id = strtol(jid, &ptr, 10);
            /* strol fails */
            if(errno == ERANGE){
               printf("%s\n", "Invalid ID!");
                return 1; 
            }
            if(job_id == 0)
                return 1;

            /* Now we need to figure out what signal number */
            char *signal_string = get_element(process_given -> arguments, 1);
            int signal_number = strtol(signal_string, &ptr, 10);

            int need_to_stop_job = (signal_number == SIGSTOP) || (signal_number == SIGTSTP) || (signal_number == SIGTTIN) || (signal_number == SIGTTOU);
            
            job *job_to_fg;
            
            /* check if we need to stop the job */ 
            if(!need_to_stop_job){
                job_to_fg = get_jid_from_jobs_list(job_id, 1);    
            }
            
            else{
                job_to_fg = get_jid_from_jobs_list(job_id, 0); 
                job_to_fg -> stopped = 1;  
            }
            
            if(job_to_fg != NULL){
                kill(-job_to_fg -> pgid, signal_number);
                return 0;
            }
            else{
                printf("%s\n", "Invalid ID!");
            } 
        }

        else{
            pid_t pid = strtol(get_element(process_given -> arguments, 2), &ptr, 10);
            /* strol fails */
            if(errno == ERANGE){
                printf("%s\n", "Invalid ID!");
                return 1;
            }
            if(pid == 0)
                return 1;

            pid_t matched_pdig = -1;
            job *current_job = jobs_list_head;

            /*Go through all the jobs to find the particular process */
            while(current_job != NULL){
                process *current_process = current_job -> head_process;
                while(current_process != NULL){
                    if(current_process -> pid == pid){
                        matched_pdig = current_job -> pgid;
                        break;
                    }
                    current_process = current_process -> next;
                }
                if(matched_pdig != -1)
                    break;
                current_job = current_job -> next;
            }

            if(matched_pdig == -1){
                printf("%s\n","Invalid PID!");
                return 1;
            }

            job *job_to_fg = current_job;

            /* Now we need to figure out what signal number */
            char *signal_string = get_element(process_given -> arguments, 1);
            int signal_number = strtol(signal_string, &ptr, 10);

            int need_to_stop_job = (signal_number == SIGSTOP) || (signal_number == SIGTSTP) || (signal_number == SIGTTIN) || (signal_number == SIGTTOU);

            /* check if we need to stop the job */ 
            if(!need_to_stop_job){
                job_to_fg -> stopped = 0;    
            }
            
            else{
                job_to_fg -> stopped = 1;  
            }
            
            if(job_to_fg != NULL){
                kill(-job_to_fg -> pgid, signal_number);
                return 0;
            }
            else{
                printf("%s\n", "Invalid ID!");
            }   
        }
    }
    return 1;
}

/* checks for the executable that the user has given */
char* check_exec(char* command){
    char *executable_path = NULL;
    int is_absolute_path = (*command == '.') || (*command == '/');
    /* If it is not the absolute path */
    if(!is_absolute_path){
        char *original_path;
        /* Get the path environment variable */
        original_path = strdup(getenv("PATH"));
        /*For individual paths */
        char *path;
        /* Split on ":"" to get all the avaiable paths */
        while((path = strsep(&original_path, ":")) != NULL){
            /* Now we need to obtain the full path of the executable */
            char *full_path = calloc(PATH_MAX, sizeof(char));
            strcpy(full_path, path);
            strcat(full_path, "/");
            strcat(full_path, command);
            /* After the full path has been obtained, check to see if it is actually executable */
            if(access(full_path, F_OK) != -1){
                executable_path = strdup(full_path);
                /* Free all the memory we allocated and return the executable path*/
                free(full_path);
                return executable_path;
            }
            free(full_path);
        }
    }
    /* If it is an absolute path, we can just return the executable path after a check */
    else{
        if(access(command, F_OK) != -1){
            executable_path = strdup(command);
            return executable_path;
        }
    }
    /* If we got till here, it was an invalid command */
    fprintf(stderr, "%s\n", "Invalid Command!");
    return executable_path;
}

int execute_job(job *job_to_run){
    /* Get the first process that is to be run */
    char *first_process_first_arg =  get_element(job_to_run -> head_process -> arguments, 0);
        
    /* Check if it is one of the builtins */
    for(int i = 0; i < NUM_BUILTINS; i++){
        if(strcmp(first_process_first_arg, builtin_functions[i]) == 0){
            #ifdef DEBUG
            printf("MATCHED BUILTIN: %s\n",builtin_functions[i]);
            #endif
            /* return the function pointer */
            return (*builtin_function_exec[i])(job_to_run -> head_process, job_to_run -> is_background);
        }
    }

    /* Get the first process in the pipeline */
    process *current_process = job_to_run -> head_process;

    /* Set the I/O redirection pipes */
    set_io_pipes(job_to_run);

    int group_id = -1;

    /* Get the next process */
    //current_process = current_process -> next;
    /* Go thorugh all the processes in the pipeline */

    sigset_t mask;

    while(current_process != NULL){
        sigemptyset(&mask);
        sigaddset(&mask, SIGCHLD);
        sigaddset(&mask, SIGTSTP);
        sigprocmask(SIG_BLOCK, &mask, NULL);

        pid_t pid = Fork();

        /* child */
        if(pid == 0){
            /* unblock */
            sigprocmask(SIG_UNBLOCK, &mask, NULL);
            /* launch the process */
            launch_process(current_process);
            _exit(0);
        }

        /* parent */
        else{
            /* close the file descriptors */
            if(current_process -> in_file != STDIN_FILENO)
                close(current_process -> in_file);
            if(current_process -> out_file != STDOUT_FILENO)
                close(current_process -> out_file);
            /* set the process ID of the process */
            current_process -> pid = pid;
            /* Set the group ID */
            if(group_id == -1){
                /* Irrespective of a bg or fg job, the PGID will be the PID of the first process */
                group_id = pid;
                job_to_run -> pgid = group_id;
                /* set the process's group ID */
                setpgid(pid, group_id);
            }
            else{
                /* if the group_id has been set, just use that */

                /* If the process who's ID we were going to use at the group ID has been reaped */
                if(setpgid(pid, job_to_run -> pgid) == -1){
                    /* set the current process's ID as the PGID to use now */
                    setpgid(pid, pid);
                    group_id = pid;
                    job_to_run -> pgid = group_id;
                }
                else{
                    /* set the process's group ID */
                    setpgid(pid, group_id); 
                    job_to_run -> pgid = group_id;  
                }
            }

            #ifdef DEBUG
            printf("Child's PID: %d\n", pid);
            printf("Child's GPID: %d\n",getpgid(pid));
            #endif

            /* If it is the last process and we can wait on it, do that */
            if(current_process -> next == NULL){
                /* unblock */
                sigprocmask(SIG_UNBLOCK, &mask, NULL);
                if(!job_to_run -> is_background){
                    put_job_to_foreground(job_to_run, 0);

                }
            }
            usleep(10000);
   
        }
        current_process = current_process -> next;
    }
    return 1;
}

void launch_process(process *process_to_launch){

    Dup2(process_to_launch -> in_file, STDIN_FILENO);
    Dup2(process_to_launch -> out_file, STDOUT_FILENO);

    if(process_to_launch -> in_file != STDIN_FILENO){
        close(process_to_launch -> in_file);
    }
    if(process_to_launch -> out_file != STDOUT_FILENO){
        close(process_to_launch -> out_file);
    }

    /*Get the program name */
    char *program_name = get_element(process_to_launch -> arguments, 0);

    /* check for this exec to see if it exists */
    char *executable_path = check_exec(program_name);
    #ifdef DEBUG
    printf("PROGRAM PATH TO EXEC: %s\n",executable_path);
    #endif

    /*If there is a file to execute, do that now */
    if(executable_path){
        #ifdef DEBUG
        printf("%s\n","EXECUTABLE WAS FOUND");
        #endif
        /* Build the argv array for exec */
        char *argv[process_to_launch -> arguments -> current_elements + 1];
        for(int i = 0; i < process_to_launch -> arguments -> current_elements; i++){
            argv[i] = get_element(process_to_launch -> arguments, i);
        }
        argv[process_to_launch -> arguments -> current_elements] = 0;

        /* execute the executable */
        execvp(executable_path,argv);
         
    }
    free(executable_path);
}


/* Sets the piping FDs for a job */
void set_io_pipes(job* job_to_run){
    /* If there are no pipes */
    if(job_to_run -> total_processes == 1)
        return;
    /* The total size for the array of fd numbers to be passed to pipe */
    int pipe_size = ((job_to_run -> total_processes) - 1) * 2;
    /* Declare the array to be passed to pipe */
    int pipes[pipe_size];

    /*Initialize the pipes array for safety */
    for(int i = 0; i < pipe_size; i++){
        pipes[i] = -1;
    }

    /*Perform piping to get all the file descriptors*/
    for(int i = 0; i < pipe_size; i += 2){
        Pipe(pipes + i);
    }

    /* Get the head process to iterate over all the processes in the job pipeline */
    process *current_process = job_to_run -> head_process;

    int *pipes_temp = pipes;

    /* The write end of the pipe (the 2nd element) becomes the out file for the first process of the job */
    current_process -> out_file = pipes_temp[1];

    /* Now go through all the processes in the job pipeline */
    while(current_process -> next){
        current_process = current_process -> next;
        /* Set the read end of the pipe (the 1st element) as the in file for the process */
        current_process -> in_file = pipes_temp[0];

        if(current_process -> next == NULL)
            break;

        /* Get the next set of file descriptors that were returned from pipe */
        pipes_temp += 2;

        /* set the out file for this process */
        current_process -> out_file = pipes_temp[1];
    }
}

job* parse_job(char* process){
    /* DO NOT INITALIZE WITH 0: WILL CAUSE INVALID READ SIZE ISSUES */
    job *to_return = calloc(1, sizeof(job));

    to_return -> original_process = process;
    split_processes(to_return);

    return to_return;
}

/* Split the processes to make the final job */
void split_processes(job* job_to_return){
    /* Temp var to tokenize on */
    char *temp, *to_free;
    /* Copy the original process into the temp variable to tokenize it */
    temp = strdup(job_to_return -> original_process);
    to_free = temp;
    int total_processes = 0;
    char total_arguments = 0;
    char *individual_process;
    char *individual_argument;

    /* Check if it is a background job by checking for the occurences of the & */
    if(strchr(temp, '&')){
        job_to_return -> is_background = 1;
        temp = strsep(&temp, "&");
    }
    else{
        job_to_return -> is_background = 0;
    }   

    while((individual_process = strsep(&temp, "|"))){
        //printf("individual_process for pipe: %s\n",individual_process);
        process *current_process = calloc(1, sizeof(process));

        current_process -> in_file = fileno(stdin);
        current_process -> out_file = fileno(stdout);
        /* Init the arraylist of arguments */
        current_process -> arguments = malloc(sizeof(arraylist));
        init_list(current_process -> arguments);

        /* Pointers to check for the out and in file */
        char *out_file_ptr, *in_file_ptr;

        /* Check for all instances of '>' */
        if((out_file_ptr = strchr(individual_process, RIGHT_HOINKY))){
            /* Check to see if there is a numerical file descriptor in front of the > */
            int in_file_fd = -1;
            /* Check to see if there is a different fd than there needs to be (the regaular out stream is STDOUT) */
            if(strlen(individual_process) - strlen(out_file_ptr) >= 2){
                if(*(out_file_ptr -1) == '2')
                    in_file_fd = 2;
                else if(*(out_file_ptr -1) == '0')
                    in_file_fd = 0;
            }
            /* Set the in file if it was changed */
            if(in_file_fd != -1){
                current_process -> in_file = in_file_fd;
            }
            char *temp_ptr = out_file_ptr;
            /* Get the char next to the > */
            temp_ptr += 1;
            char filename[MAX_FILENAME];
            memset(filename, '\0', MAX_FILENAME);
            int counter = 0;
            /* Obtain the out file name*/
            while(*temp_ptr != '\0' && *temp_ptr != RIGHT_HOINKY && *temp_ptr != PIPE && *temp_ptr != LEFT_HOINKY){
                /* Should not be a space character */
                if(*temp_ptr != 32){
                    filename[counter] = *temp_ptr;
                    counter++;
                }
                *temp_ptr = '\0';
                temp_ptr += 1;
            }

            FILE *out_file_stream = fopen(filename, "w");
            /* Check for out files */
            if(out_file_stream){
                current_process -> out_file = fileno(fopen(filename, "w"));
                fclose(out_file_stream);    
            }
            else{
                current_process -> out_file = -1;
            }
            
            /* check if the out file can be opened or not for writing stuff*/
            if(current_process -> out_file == -1){
                perror("File could not be opened for writing");
                return;
            }
        }

        /* Check for all instances of '<' */
        if((in_file_ptr = strchr(individual_process, LEFT_HOINKY))){
            char *temp_ptr = in_file_ptr;
            temp_ptr += 1;
            char filename[MAX_FILENAME];
            memset(filename, '\0', MAX_FILENAME);
            int counter = 0;
            /*Obtain the in file name */
            while(*temp_ptr != '\0' && *temp_ptr != LEFT_HOINKY && *temp_ptr != PIPE && *temp_ptr != RIGHT_HOINKY){
                /* Should not be a space character */
                if(*temp_ptr != 32){
                    filename[counter] = *temp_ptr;
                    counter++;
                }
                *temp_ptr = '\0';
                temp_ptr += 1;
            }

            FILE *in_file_stream = fopen(filename, "r");
            if(in_file_stream){
                current_process -> in_file = fileno(fopen(filename, "r"));
                fclose(in_file_stream);
            }
            else{
                current_process -> in_file = -1;
            }

            /* check if the in file can be opened or not for reading stuff*/
            if(current_process -> in_file == -1){
                perror("Input file doesn't exist");
                return;
            }
        }

        /* Now we have completed all the stuff for in and out file */
        while((individual_argument = strsep(&individual_process, DELIMITER))){
            /*Check for the case if there is no space between the filename and the hoinkies */    
            if(strchr(individual_argument, RIGHT_HOINKY)){
                individual_argument[strlen(individual_argument) - 1] = '\0';
            }

            else if(strchr(individual_argument, LEFT_HOINKY)){
                individual_argument[strlen(individual_argument) - 1] = '\0';
            }

            /* Insert the argument into the list of arguments for the process */
            if(*individual_argument != 0 && *individual_argument != 32 && *individual_argument != LEFT_HOINKY && *individual_argument != RIGHT_HOINKY){
                insert_element(current_process -> arguments, individual_argument);   
            }

            total_arguments++;
        }
        #ifdef DEBUG
        printf("\n");
        #endif

        /* Insert the process into the 'linked list' of jobes */
        if(job_to_return -> head_process == NULL){
            job_to_return -> head_process = current_process;
        }
        else{
            process *iterator = job_to_return -> head_process;
            while(iterator -> next){
                iterator = iterator -> next;
            }
            iterator -> next = current_process;
        }

        total_processes++;
    }
    free(to_free);
    job_to_return -> total_processes = total_processes;
}



int readline_binding_function(int count, int key){
    if(key == 2){
        if(jobs_list_head == NULL)
            SPID = -1;
        else
            SPID = jobs_list_head -> pgid;
    }

    if(key == 7){
        if(SPID != -1){
            job *job_to_kill = get_from_jobs_list(SPID, 0);

            if(job_to_kill != NULL){
                kill(-job_to_kill -> pgid, SIGTERM);
            }
            else
                fprintf(stderr, "%s\n", "SPID has not been set");
        }
        else{
        fprintf(stderr, "%s\n", "SPID has not been set");
        }
    }

    /* The help menu shall be called */
    if(key == 8){
        for(int i = 0; i < HELP_MENU_LENGTH - 1; i++){
            fprintf(stdout, "%s", HELP[i]);
        }
    }

    if(key == 16){
        printf("\n%s\n", "----Info----");
        printf("%s\n", "help");
        printf("%s\n", "prt");
        printf("\n%s\n", "----CTRL----");
        printf("%s\n", "cd");
        printf("%s\n", "chclr");
        printf("%s\n", "cpmt");
        printf("%s\n", "pwd");
        printf("%s\n", "exit");
        printf("\n%s\n", "----Job Control----");
        printf("%s\n", "bg");
        printf("%s\n", "fg");
        printf("%s\n", "disown");
        printf("%s\n", "jobs");
        printf("\n%s\n", "----Number of commands run----");
        printf("%d\n",total_jobs);
        printf("\n%s\n", "----Process Table----");
        printf("PGID\tPID\tTime\t\tcmd\n");
        job *current_job = jobs_list_head;
        while(current_job != NULL){
            long time_difference = (current_job -> starting_time) - shell_starting_time;
            if(time_difference < 60)
                printf("%d\t%d\t%ld seconds\t%s\n", current_job -> pgid, current_job -> pgid, time_difference, current_job -> original_process);
            else{
                time_difference /= 60;
                printf("%d\t%d\t%ld minute(s)\t%s\n", current_job -> pgid, current_job -> pgid, time_difference, current_job -> original_process);
            }
            current_job = current_job -> next;
        }
    }

    rl_on_new_line();
    return 0;
}

int main(int argc, char** argv) {
    //DO NOT MODIFY THIS. If you do you will get a ZERO.
    rl_catch_signals = 0;
    //This is disable readline's default signal handlers, since you are going
    //to install your own.
    //
    //
    
    /* Loop until we are in the foreground.  */
      // while (tcgetpgrp (shell_terminal) != (shell_pgid = getpgrp ()))
      //   kill (- shell_pgid, SIGTTIN);
    
    /* Install all the sig handlers */

    /* Ctrl + B  -> store pid */ 
    rl_bind_key(2, readline_binding_function);
    /* Ctrl + G -> get pid */
    rl_bind_key(7, readline_binding_function);
    /* Ctrl + H -> help menu */
    rl_bind_key(8, readline_binding_function);
    rl_bind_key(16, readline_binding_function);

    signal(SIGCHLD, sigchild_handler);
    signal(SIGINT, sigint_handler);
    signal(SIGTTOU, SIG_IGN);

    /* Put ourselves in our own process group.  */
      shell_pgid = getpid ();
      if (setpgid (shell_pgid, shell_pgid) < 0)
        {
          perror ("Couldn't put the shell in its own process group");
          exit (1);
        }

      /* Grab control of the terminal.  */
    tcsetpgrp(shell_terminal, shell_pgid);
    /* Get the starting time of the shell */
    shell_starting_time = time(NULL);

    /* Modify the process prompt */
    strcpy(sfish_prompt, "sfish-");
    strcat(sfish_prompt, getenv("USER"));
    strcat(sfish_prompt, "@");
    /* Get the host name and append it to the prompt */
    char *hostname = malloc(128 * sizeof(char));
    gethostname(hostname,sizeof(hostname));
    strcat(sfish_prompt,hostname);
    free(hostname);
    strcat(sfish_prompt, "[");
    char *cwd = getcwd(NULL, 0);
    if(strstr(cwd, getenv("HOME")) && (cwd == strstr(cwd, getenv("HOME")))){
        strcat(sfish_prompt, "~");
        strcat(sfish_prompt, cwd + strlen(getenv("HOME")));
    }
    else{
        strcat(sfish_prompt, cwd);
    }
    free(cwd);
    strcat(sfish_prompt, "]> ");


    /* The process that the user has given */
    char *cmd;

    status = 0;

    /*Inititalize the list for processes */
    arraylist *cmds_list = malloc(sizeof(arraylist));
    init_list(cmds_list);

    /* The current directory will be the previous directory for the next call */
    getcwd(prev_dir, sizeof(char) * PATH_MAX);

    shell_pgid = getpid();

    while((cmd = readline(sfish_prompt)) != NULL && status >= 0) {

        #ifdef DEBUG
        printf("SHELL PID: %d\n",getpid());
        #endif


        /* The job that is to be run */
        job *job_to_run;
        /* Get the job that is to be run */
        job_to_run = parse_job(cmd);
        /* If it has atleast one process, proceed forward */
        if(job_to_run -> head_process && get_element(job_to_run -> head_process -> arguments, 0)){ 
            total_jobs++;
            /* Get the starting time */
            job_to_run -> starting_time = time(NULL);
            #ifdef DEBUG
            printf("job_to_run is background: %d\n", job_to_run -> is_background);
            #endif
            /* If it is a background process, insert into jobs_list */
            if(job_to_run -> is_background){
                if(jobs_list_head == NULL){
                    job_to_run -> next = NULL;
                    jobs_list_head = job_to_run;
                }
                else{
                    /* insert it elsewhere */
                    job *current_job = jobs_list_head;
                    while(current_job -> next != NULL){
                        current_job = current_job -> next;
                    }
                    current_job -> next = job_to_run;
                    job_to_run -> next = NULL;

                }
            }

            /* If it is an exit or quit process */
            if(strcmp(get_element(job_to_run -> head_process -> arguments, 0), "exit") == 0 || strcmp(get_element(job_to_run -> head_process -> arguments, 0), "quit") == 0){
                free(cmd);
            } 
            status = execute_job(job_to_run);
            //sfish_jobs(job_to_run -> head_process, 0);

            #ifdef DEBUG
            int counter = 0;
            /* insert it elsewhere */
            job *current_job = jobs_list_head;
            while(current_job != NULL){
                current_job = current_job -> next;
                counter++;
            }
            printf("TOTAL JOBS CURRENTLY: %d\n",counter);
            #endif

        }  
       
        /* Clear the commad prompt to potentially update it */
        memset(sfish_prompt, '\0', PROMPT_SIZE);

        /* Modify the process prompt */
        strcpy(sfish_prompt, "sfish");

        /* display the - if either on of the toggles is on */
        if(machine_toggle || user_toggle){
            strcat(sfish_prompt, "-");
        }
        /* Show the user or not */
        if(user_toggle){
            if(user_color != -1){
                user_color_bold_toggle == 0 ? strcat(sfish_prompt, color_codes[user_color]) : strcat(sfish_prompt, color_codes_bold[user_color]);
            }
            strcat(sfish_prompt, getenv("USER"));
            if(user_color != -1){
                /* We need to reset the ansi exacpe sequence to get default colors back */
                strcat(sfish_prompt, ANSI_COLOR_RESET); 
            }
        }
        /* display the user @ iff both toggles are enabled */
        if(machine_toggle && user_toggle){
            strcat(sfish_prompt, "@");
        }

        if(machine_toggle){
            /* Get the host name and append it to the prompt */
            char *hostname = malloc(128 * sizeof(char));
            gethostname(hostname,sizeof(hostname));
            if(machine_color != -1){
                machine_color_bold_toggle == 0 ? strcat(sfish_prompt, color_codes[machine_color]) : strcat(sfish_prompt, color_codes_bold[machine_color]);
            }
            strcat(sfish_prompt,hostname);
            if(machine_color != -1){
                strcat(sfish_prompt, ANSI_COLOR_RESET); 
            }
            free(hostname);
        }
        
        strcat(sfish_prompt, ":[");
        char *cwd = getcwd(NULL, 0);
        if(strstr(cwd, getenv("HOME")) && (cwd == strstr(cwd, getenv("HOME")))){
            strcat(sfish_prompt, "~");
            strcat(sfish_prompt, cwd + strlen(getenv("HOME")));
        }
        else{
            strcat(sfish_prompt, cwd);
        }
        free(cwd);
        strcat(sfish_prompt, "]> ");
    }

    //Don't forget to free allocated memory, and close file descriptors.
    free(cmd);
    //WE WILL CHECK VALGRIND!
    clear_list(cmds_list);
    free(cmds_list -> data);
    free(cmds_list);
    return EXIT_SUCCESS;
}
