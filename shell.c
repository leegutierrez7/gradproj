// Add your code here
#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <sys/types.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/wait.h>
#include <ctype.h>
#include <sys/stat.h>

int num_args = 0;
int size_of_command_list = 0;
int list_frequency[1024];
int index_of_args_list = 0;
char error_message[30] = "An error has occurred\n";
int error_message_length = 22;
int len = 128;

char **parse_input(char *input, size_t len)
{

    char *end;
    int space_flag = 0;
    int copy_done = 0;
    char buffer[1024] = {0};
    char *insert_point = &buffer[0];
    const char *tmp = input;
    size_t needle_len = strlen("&");
    size_t repl_len = strlen(" & ");

    while (1)
    {
        const char *p = strstr(tmp, "&");
        if (p != NULL && isspace(*(p - 1)) && ((isspace(*(p + 1)) || *(p + 1) == '\0')))
        {
            tmp = p + needle_len;
            continue;
        }
        else if (p == NULL)
        {
            space_flag = 1;
            strcpy(insert_point, tmp);
            break;
        }
        else
        {
            // walked past last occurrence of needle; copy remaining part
            if (p == NULL)
            {
                strcpy(insert_point, tmp);
                break;
            }

            // copy part before needle
            copy_done = 1;
            memcpy(insert_point, tmp, p - tmp);
            insert_point += p - tmp;

            // copy replacement string
            memcpy(insert_point, " & ", repl_len);
            insert_point += repl_len;

            // adjust pointers, move on
            tmp = p + needle_len;
        }
    }

    // write altered string back to target
    if (!space_flag || copy_done)
    {
        strcpy(input, buffer);
    }

    // Trim trailing space
    end = input + strlen(input) - 1;
    while (end > input && isspace((unsigned char)*end))
    {
        end--;
    }
    // Write new null terminator character
    end[1] = '\0';

    char *token = NULL;
    char *input_copy = NULL;
    num_args = 0;

    input_copy = malloc(len * sizeof(char));

    memcpy(input_copy, input, len);

    while ((token = strsep(&input, " ")) != NULL)
    {
        num_args++;
    }

    char **args = malloc(sizeof(char *) * (num_args - 1));

    for (int i = 0; i < num_args; i++)
    {
        if ((token = strsep(&input_copy, " ")) != NULL)
        {

            args[i] = malloc(strlen(token) * sizeof(char));
            strcpy(args[i], token);
        }
        else
        {
            break;
        }
    }

    free(input_copy);
    return args;
}

char ***commands(char **input)
{
    // ls > test.txt & ls > test1.txt
    // ls > test.txt list[0]
    // ls > test1.txt list[1]
    // num of args = 2 -> spawn 2 children

    // ls > test.txt -> num_of_args = 1 child_count = 1
    /*for(int i =0; i < child_count; ++i)
        fork()
        if (list[i+1] != '&'){
            wait;
        } else {i++;}
    */

    size_of_command_list = 1; // create size of list
    int num_of_args = 0;
    index_of_args_list = 0;
    for (int i = 0; i < num_args; i++)
    {
        if (num_args > 1 && strcmp(input[i], "&") == 0)
        {
            list_frequency[index_of_args_list] = num_of_args;
            index_of_args_list++;
            num_of_args = 1;
            list_frequency[index_of_args_list] = num_of_args;
            size_of_command_list++;
            index_of_args_list++;
        }
        else if (num_args == 1 && strcmp(input[i], "&") == 0)
        {
            num_of_args++;
            list_frequency[index_of_args_list] = num_args;
        }
        else if (i > 1 && strcmp(input[i - 1], "&") == 0)
        {
            list_frequency[index_of_args_list] = num_of_args;
            size_of_command_list++;
        }
        else if (i == num_args - 1)
        {
            num_of_args++;
            list_frequency[index_of_args_list] = num_of_args;
        }
        else
        {
            num_of_args++;
        }
    }
    char ***list = malloc(sizeof(char **) * size_of_command_list); // 2Dimensional Array of Strings
    for (int i = 0; i < size_of_command_list; i++)
    {
        list[i] = (char **)malloc(sizeof(char *) * list_frequency[i] + 1);
        for (int j = 0; j < list_frequency[i]; j++)
        {
            list[i][j] = (char *)malloc(sizeof(char) * size_of_command_list);
        }
    }

    int i = 0; // Counter for injection to 2d array
    int k = 0; // Counter for moving list array
    for (int j = 0; j < num_args; j++)
    {
        if (num_args > 1 && strcmp(input[j], "&") == 0)
        {
            ++i; // increment i if parallel command is found
            k = 0;
            list[i][k] = input[j];
            ++i;
        }
        else if (num_args == 1 && strcmp(input[j], "&") == 0)
        {
            list[i][k] = input[j];
            k++;
        }
        else
        {
            if (strchr(input[j], '>') && strcmp(input[j], ">") != 0)
            {
                char *split;
                char *temp = input[j];
                int is_split = 0;
                while ((split = strtok_r(temp, ">", &temp)))
                {

                    list[i][k] = split;
                    k++;
                    if (!is_split)
                    {
                        list[i][k] = ">";
                        k++;
                        is_split = 1;
                    }
                }
                list_frequency[j - 1]++;
            }
            else
            {
                list[i][k] = input[j];
                k++;
            }
        }
    }

    return list;
}

/*
if (i + 1 <= index_of_args_list && strcmp(list[i + 1][0], "&") == 0)
            {
                i++;
                continue;
            }

*/

void execute(char ***list, char *path[])
{

    pid_t id;
    int redirection_location = 0;
    for (int i = 0; i <= index_of_args_list; i++)
    {

        int redirect_no_args = 0;

        if (list[i] == NULL)
        {
            break;
        }
        for (int j = 1; j < list_frequency[i]; j++)
        {

            if (list[i][j] != NULL)
            {
                if ((strcmp(list[i][j], ">") == 0))
                {
                    if (list[i][j + 2] != NULL)
                    {
                        write(STDERR_FILENO, error_message, error_message_length);
                        redirect_no_args = 1;
                        break;
                    }
                    else
                    {
                        break;
                    }
                }
            }
        }

        if (redirect_no_args)
        {
            break;
        }

        // start of arguments for single list with no redirection
        if (strcmp(list[i][0], "cd") == 0)
        {

            if (list_frequency[i] == 2)
            {

                if (chdir(list[i][1]) != 0)
                {
                    write(STDERR_FILENO, error_message, error_message_length);
                }
                break;
            }
            else
            {
                write(STDERR_FILENO, error_message, error_message_length);
                break;
            }
        }
        if (strcmp(list[i][0], "path") == 0)
        {
            int j = 0;
            while (path[j] != NULL)
            {
                // free(path[j]);
                path[j] = NULL;
                ++j;
            }

            char current_dir[100];
            getcwd(current_dir, 100);
            strcat(current_dir, "/");
            // new paths
            j = 1;
            while (list[i][j] != NULL)
            {
                if (list[i][j][0] == '/')
                {
                    path[j - 1] = list[i][j];
                }
                else
                {
                    strcat(current_dir, list[i][j]);
                    char *temp_path = malloc(sizeof(char *) * 100);
                    memcpy(temp_path, current_dir, 100);
                    path[j - 1] = temp_path;
                }
                ++j;
            }

            break;
        }
        if (strcmp(list[i][0], "exit") == 0)
        {
            if (list_frequency[i] != 1)
            {
                write(STDERR_FILENO, error_message, error_message_length);
                break;
            }
            exit(0);
        }

        int counter = 0;
        char valid_path[100];
        char *home = malloc(sizeof(char) * 100);
        getcwd(home, 100);
        if (path[counter] == NULL)
        {
            write(STDERR_FILENO, error_message, error_message_length);
            break;
        }
        int sucessful = 0;
        while (path[counter] != NULL)
        {
            strcpy(valid_path, path[counter]);
            strcat(valid_path, "/");
            if (strcmp(list[i][0], "&") != 0)
            {
                strcat(valid_path, list[i][0]);
            }
            if (access(valid_path, X_OK) == 0)
            {
                sucessful = 1;
                if (!(id = fork()))
                {
                    for (int k = 0; k < list_frequency[i]; k++)
                    {
                        if (strcmp(list[i][k], ">") == 0) // file needs to be redirecteed
                        {

                            redirection_location = k;
                            if ((list[i][k + 1][0]) == '/')
                            {
                                home[0] = '\0';
                                strcat(home, list[i][k + 1]);
                            }
                            else
                            {
                                strcat(home, list[i][k + 1]);
                            }

                            char *create_file = malloc(sizeof(char) * 100);
                            memcpy(create_file, home, 100);
                            char *create_directory = &home[strlen(home)];
                            while (create_directory > home && *create_directory != '/')
                            {
                                create_directory--;
                            }
                            if (*create_directory == '/')
                            {
                                *create_directory = '\0';
                            }
                            if (path[counter] == NULL)
                            {
                                write(STDERR_FILENO, error_message, error_message_length);
                                break;
                            }
                            strcat(home, "/");
                            int second_try = 0;
                            int fd_output = open(home, O_CREAT | O_WRONLY | O_TRUNC, 0777);

                            if (fd_output == -1)
                            {

                                mkdir(home, 0777);
                                second_try = open(create_file, O_CREAT | O_WRONLY | O_TRUNC, 0777);
                                close(1);
                                dup2(second_try, STDOUT_FILENO); // overwriting stdout with the new file
                                close(second_try);
                                list[i][redirection_location] = NULL;
                                free(create_file);
                            }
                            else
                            {
                                close(1);
                                dup2(fd_output, STDOUT_FILENO); // overwriting stdout with the new file
                                close(fd_output);
                                list[i][redirection_location] = NULL;
                            }
                        }
                    }

                    execv(valid_path, list[i]);
                }

                else
                {

                    if (i + 1 <= index_of_args_list && strcmp(list[i + 1][0], "&") == 0)
                    {
                        i++;
                        if (i + 1 == index_of_args_list)
                        {
                            waitpid(id, NULL, WNOHANG);
                        }
                        else
                        {
                            wait(0);
                        }                        
                    }
                    else
                    {
                        wait(0);
                    }
                }
            }
            ++counter;
        }
        if (sucessful == 0)
        {
            write(STDERR_FILENO, error_message, error_message_length);
        }

        free(home);
    }
    wait(0);
}

int main(int argc, char *argv[])
{

    int file_or_std = argc;

    if (argc > 2)
    {
        write(STDERR_FILENO, error_message, error_message_length);
        exit(1);
    }

    // File Input
    FILE *fptr;

    if (argc == 2)
    {
        if ((fptr = fopen(argv[1], "r")) == NULL)
        {
            write(STDERR_FILENO, error_message, error_message_length);
            exit(1);
        }
    }

    char *path[len];
    path[0] = "/bin";
    for (int i = 1; i < len; i++)
    {
        path[i] = NULL;
    }

    while (1)
    {
        char *input = NULL;
        size_t len = 0;
        int num_args = 0;
        if (file_or_std == 1)
        {
            printf("wish> ");
            getline(&input, &len, stdin); // Read in Data
        }
        else
        {
            if (getline(&input, &len, fptr) <= -1)
            {
                break;
            }
        }

        int index, i;

        index = 0;

        /* Find last index of whitespace character */
        while (input[index] == ' ' || input[index] == '\t')
        {
            index++;
        }

        if (index != 0)
        {
            /* Shit all trailing characters to its left */
            i = 0;
            while (input[i + index] != '\0')
            {
                input[i] = input[i + index];
                i++;
            }
            input[i] = '\0'; // Make sure that string is NULL terminated
        }
        strtok(input, "\n"); // Remove \n

        if (strcmp(input, "\n") == 0)
        {
            continue;
        }

        char **args = parse_input(input, len); // Tokenize

        char ***command_list = commands(args); // Super Tokenize for Redirection and Parallel

        execute(command_list, path);

        free(input);
        free(args);
        for (int i = 0; i < num_args; i++)
        {
            free(args[i]);
        }
        for (int i = 0; i < num_args; i++)
        {
            for (int j = 0; j < num_args; j++)
            {
                free(command_list[i][j]);
            }
            free(command_list[i]);
        }
        free(command_list);
    }

    return 0;
}