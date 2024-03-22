#include "dirsize.h"
#include "good_morning.h"
#include "hexdump.h"
#include <errno.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <dirent.h>
#include <sys/stat.h>
#include <termios.h> // termios, TCSANOW, ECHO, ICANON
#include <unistd.h>
#include <fcntl.h>
const char *sysname = "Shellect";

// struct to store aliases
struct aliases_struct {
    char *alias_name;
    char *command_string;
    struct aliases_struct *next;
};
struct aliases_struct *aliases = NULL;

void add_alias(const char *alias_name, const char *command_string);
const char *search_alias(const char *alias_name);
void save_aliases();
void load_aliases();
void find_string_in_all_files(const char *search_string);

enum return_codes {
	SUCCESS = 0,
	EXIT = 1,
	UNKNOWN = 2,
};

struct command_t {
	char *name;
	bool background;
	bool auto_complete;
	int arg_count;
	char **args;
	char *redirects[3]; // in/out redirection
	struct command_t *next; // for piping
};

/**
 * Prints a command struct
 * @param struct command_t *
 */
void print_command(struct command_t *command) {
	int i = 0;
	printf("Command: <%s>\n", command->name);
	printf("\tIs Background: %s\n", command->background ? "yes" : "no");
	printf("\tNeeds Auto-complete: %s\n",
		   command->auto_complete ? "yes" : "no");
	printf("\tRedirects:\n");

	for (i = 0; i < 3; i++) {
		printf("\t\t%d: %s\n", i,
			   command->redirects[i] ? command->redirects[i] : "N/A");
	}

	printf("\tArguments (%d):\n", command->arg_count);

	for (i = 0; i < command->arg_count; ++i) {
		printf("\t\tArg %d: %s\n", i, command->args[i]);
	}

	if (command->next) {
		printf("\tPiped to:\n");
		print_command(command->next);
	}
}

/**
 * Release allocated memory of a command
 * @param  command [description]
 * @return         [description]
 */
int free_command(struct command_t *command) {
	if (command->arg_count) {
		for (int i = 0; i < command->arg_count; ++i)
			free(command->args[i]);
		free(command->args);
	}

	for (int i = 0; i < 3; ++i) {
		if (command->redirects[i])
			free(command->redirects[i]);
	}

	if (command->next) {
		free_command(command->next);
		command->next = NULL;
	}

	free(command->name);
	free(command);
	return 0;
}

/**
 * Show the command prompt
 * @return [description]
 */
int show_prompt() {
	char cwd[1024], hostname[1024];
	gethostname(hostname, sizeof(hostname));
	getcwd(cwd, sizeof(cwd));
	printf("%s@%s:%s %s$ ", getenv("USER"), hostname, cwd, sysname);
	return 0;
}

/**
 * Parse a command string into a command struct
 * @param  buf     [description]
 * @param  command [description]
 * @return         0
 */
int parse_command(char *buf, struct command_t *command) {
	const char *splitters = " \t"; // split at whitespace
	int index, len;
	len = strlen(buf);

	// trim left whitespace
	while (len > 0 && strchr(splitters, buf[0]) != NULL) {
		buf++;
		len--;
	}

	while (len > 0 && strchr(splitters, buf[len - 1]) != NULL) {
		// trim right whitespace
		buf[--len] = 0;
	}

	// auto-complete
	if (len > 0 && buf[len - 1] == '?') {
		command->auto_complete = true;
	}

	// background
	if (len > 0 && buf[len - 1] == '&') {
		command->background = true;
	}

	char *pch = strtok(buf, splitters);
	if (pch == NULL) {
		command->name = (char *)malloc(1);
		command->name[0] = 0;
	} else {
		command->name = (char *)malloc(strlen(pch) + 1);
		strcpy(command->name, pch);
	}

	command->args = (char **)malloc(sizeof(char *));

	int redirect_index;
	int arg_index = 0;
	char temp_buf[1024], *arg;

	while (1) {
		// tokenize input on splitters
		pch = strtok(NULL, splitters);
		if (!pch)
			break;
		arg = temp_buf;
		strcpy(arg, pch);
		len = strlen(arg);

		// empty arg, go for next
		if (len == 0) {
			continue;
		}

		// trim left whitespace
		while (len > 0 && strchr(splitters, arg[0]) != NULL) {
			arg++;
			len--;
		}

		// trim right whitespace
		while (len > 0 && strchr(splitters, arg[len - 1]) != NULL) {
			arg[--len] = 0;
		}

		// empty arg, go for next
		if (len == 0) {
			continue;
		}

		// piping to another command
		if (strcmp(arg, "|") == 0) {
			struct command_t *c = malloc(sizeof(struct command_t));
			int l = strlen(pch);
			pch[l] = splitters[0]; // restore strtok termination
			index = 1;
			while (pch[index] == ' ' || pch[index] == '\t')
				index++; // skip whitespaces

			parse_command(pch + index, c);
			pch[l] = 0; // put back strtok termination
			command->next = c;
			continue;
		}

		// background process
		if (strcmp(arg, "&") == 0) {
			// handled before
			continue;
		}

		// handle input redirection
		redirect_index = -1;
		if (arg[0] == '<') {
			redirect_index = 0;
		}

		if (arg[0] == '>') {
			if (len > 1 && arg[1] == '>') {
				redirect_index = 2;
				arg++;
				len--;
			} else {
				redirect_index = 1;
			}
		}

		if (redirect_index != -1) {
			command->redirects[redirect_index] = malloc(len);
			strcpy(command->redirects[redirect_index], arg + 1);
			continue;
		}

		// normal arguments
		if (len > 2 &&
			((arg[0] == '"' && arg[len - 1] == '"') ||
			 (arg[0] == '\'' && arg[len - 1] == '\''))) // quote wrapped arg
		{
			arg[--len] = 0;
			arg++;
		}

		command->args =
			(char **)realloc(command->args, sizeof(char *) * (arg_index + 1));

		command->args[arg_index] = (char *)malloc(len + 1);
		strcpy(command->args[arg_index++], arg);
	}
	command->arg_count = arg_index;

	// increase args size by 2
	command->args = (char **)realloc(
		command->args, sizeof(char *) * (command->arg_count += 2));

	// shift everything forward by 1
	for (int i = command->arg_count - 2; i > 0; --i) {
		command->args[i] = command->args[i - 1];
	}

	// set args[0] as a copy of name
	command->args[0] = strdup(command->name);

	// set args[arg_count-1] (last) to NULL
	command->args[command->arg_count - 1] = NULL;

	return 0;
}

void prompt_backspace() {
	putchar(8); // go back 1
	putchar(' '); // write empty over
	putchar(8); // go back 1 again
}

/**
 * Prompt a command from the user
 * @param  buf      [description]
 * @param  buf_size [description]
 * @return          [description]
 */
int prompt(struct command_t *command) {
	size_t index = 0;
	char c;
	char buf[4096];
	static char oldbuf[4096];

	// tcgetattr gets the parameters of the current terminal
	// STDIN_FILENO will tell tcgetattr that it should write the settings
	// of stdin to oldt
	static struct termios backup_termios, new_termios;
	tcgetattr(STDIN_FILENO, &backup_termios);
	new_termios = backup_termios;
	// ICANON normally takes care that one line at a time will be processed
	// that means it will return if it sees a "\n" or an EOF or an EOL
	new_termios.c_lflag &=
		~(ICANON |
		  ECHO); // Also disable automatic echo. We manually echo each char.
	// Those new settings will be set to STDIN
	// TCSANOW tells tcsetattr to change attributes immediately.
	tcsetattr(STDIN_FILENO, TCSANOW, &new_termios);

	show_prompt();
	buf[0] = 0;

	while (1) {
		c = getchar();
		// printf("Keycode: %u\n", c); // DEBUG: uncomment for debugging

		// handle tab
		if (c == 9) {
			buf[index++] = '?'; // autocomplete
			break;
		}

		// handle backspace
		if (c == 127) {
			if (index > 0) {
				prompt_backspace();
				index--;
			}
			continue;
		}

		if (c == 27 || c == 91 || c == 66 || c == 67 || c == 68) {
			continue;
		}

		// up arrow
		if (c == 65) {
			while (index > 0) {
				prompt_backspace();
				index--;
			}

			char tmpbuf[4096];
			printf("%s", oldbuf);
			strcpy(tmpbuf, buf);
			strcpy(buf, oldbuf);
			strcpy(oldbuf, tmpbuf);
			index += strlen(buf);
			continue;
		}

		putchar(c); // echo the character
		buf[index++] = c;
		if (index >= sizeof(buf) - 1)
			break;
		if (c == '\n') // enter key
			break;
		if (c == 4) // Ctrl+D
			return EXIT;
	}

	// trim newline from the end
	if (index > 0 && buf[index - 1] == '\n') {
		index--;
	}

	// null terminate string
	buf[index++] = '\0';

	strcpy(oldbuf, buf);

	parse_command(buf, command);

	// print_command(command); // DEBUG: uncomment for debugging

	// restore the old settings
	tcsetattr(STDIN_FILENO, TCSANOW, &backup_termios);
	return SUCCESS;
}

int process_command(struct command_t *command);

int main() {
	// Load aliases before getting commands
	load_aliases();

	while (1) {
		struct command_t *command = malloc(sizeof(struct command_t));

		// set all bytes to 0
		memset(command, 0, sizeof(struct command_t));

		int code;
		code = prompt(command);
		if (code == EXIT) {
			break;
		}

		code = process_command(command);
		if (code == EXIT) {
			break;
		}

		free_command(command);
	}
	// Save aliases before exiting
    save_aliases();
	printf("\n");
	return 0;
}

int process_command(struct command_t *command) {
	int r;

	const char *alias_command = search_alias(command->name);
    if (alias_command) {
        // If alias found, change the alias to its real command name
        command->name = strdup(alias_command);
    }

	if (strcmp(command->name, "") == 0) {
		return SUCCESS;
	}

	if (strcmp(command->name, "exit") == 0) {
		return EXIT;
	}

	if (strcmp(command->name, "cd") == 0) {
		if (command->arg_count > 0) {
			r = chdir(command->args[0]);
			if (r == -1) {
				printf("-%s: %s: %s\n", sysname, command->name,
					   strerror(errno));
			}

			return SUCCESS;
		}
	}

	
	if (strcmp(command->name, "alias") == 0 && command->arg_count == 4) {
		add_alias(command->args[1], command->args[2]);
     	return SUCCESS;
	}

	// This custom command finds the first occurence of a string in all ".txt" files in the current directory.
	// If the given string is found in the txt file, it returns the line number. If not, returns "not found" string.
	if (strcmp(command->name, "findstringinall") == 0) {
        if (command->arg_count > 0) {
            find_string_in_all_files(command->args[1]);
            return SUCCESS;
        } else {
            printf("This command needs an string to search as an argument.");
            return SUCCESS;
        }
    }

	pid_t pid = fork();
	// child
	if (pid == 0) {

		// Operator >
        if (command->redirects[1]) {
            int fd = open(command->redirects[1], O_WRONLY | O_CREAT | O_TRUNC, 0666);
            if (fd < 0) {
                printf("Error while doing operator > \n");
                exit(1);
            }
            dup2(fd, STDOUT_FILENO);
            close(fd);
        }

		// Operator >>
        if (command->redirects[2]) {
            int fd = open(command->redirects[2], O_WRONLY | O_CREAT | O_APPEND, 0666);
            if (fd < 0) {
                printf("Error while doing operator >> \n");
                exit(1);
            }
            dup2(fd, STDOUT_FILENO);
            close(fd);
        }

		// Operator <
        if (command->redirects[0]) {
            int fd = open(command->redirects[0], O_RDONLY);
            if (fd < 0) {
                printf("Error while doing operator < \n");
                exit(1);
            }
            dup2(fd, STDIN_FILENO);
            close(fd);
        }

		

		if (command->background) {
            // Detach the child process if it's a background process
            setsid();
        }

		execvp(command->name, command->args); // exec+args+path
	 	perror("execv"); // execv returns only if an error occurs
        exit(EXIT_FAILURE);
	} else {
		 if (!command->background) {
            // If not a background process, wait for the child to finish
            waitpid(pid, NULL, 0);
        }
		return SUCCESS;
	}


    if (strcmp(command->name, "hexdump") == 0) {
        // Parse arguments and set up the config structure
        HexdumpConfig config;
        config.group_size = 1;  // Default group size
        config.filename = NULL; // Default to NULL (STDIN)

        // Check for the presence of '-g' option and filename
        for (int i = 1; i < command->arg_count; i++) {
            if (strcmp(command->args[i], "-g") == 0 && i + 1 < command->arg_count) {
                config.group_size = atoi(command->args[++i]);
                if (config.group_size <= 0 || config.group_size > 16 || (config.group_size & (config.group_size - 1)) != 0) {
                    fprintf(stderr, "Invalid group size. Must be a power of 2 and not larger than 16.\n");
                    return UNKNOWN;
                }
            } else {
                config.filename = command->args[i]; // Assume the last argument is the filename
            }
        }

        hexdump(&config);
        return SUCCESS;
    }
  
 
    if (strcmp(command->name, "good_morning") == 0) {
        if (command->arg_count != 3) {
            fprintf(stderr, "Usage: good_morning <minutes> <path/to/audio>\n");
            return UNKNOWN;
        }

        GoodMorningConfig gm_config;
        gm_config.minutes = atoi(command->args[1]);
        gm_config.audio_path = command->args[2];

        if (gm_config.minutes <= 0) {
            fprintf(stderr, "Invalid number of minutes. Must be greater than 0.\n");
            return UNKNOWN;
        }

        schedule_audio_playback(&gm_config);
        printf("Alarm set for %d minutes from now.\n", gm_config.minutes);
        return SUCCESS;
    }


	if (strcmp(command->name, "dirsize") == 0) {
        DirSizeOptions options = {.path = ".", .recursive = 0};
        int foundPath = 0;

        for (int i = 1; i < command->arg_count; ++i) {
            if (strcmp(command->args[i], "-r") == 0) {
                options.recursive = 1;
            } else {
                if (foundPath) {
                    fprintf(stderr, "Usage: dirsize [-r] [path]\n");
                    return UNKNOWN;
                }
                options.path = command->args[i];
                foundPath = 1;
            }
        }

        calculate_dir_size(&options);
        return SUCCESS;
    }


	printf("-%s: %s: command not found\n", sysname, command->name);
	return UNKNOWN;
}

void add_alias(const char *alias_name, const char *command_string) {
    struct aliases_struct *new_alias = (struct aliases_struct *) malloc(sizeof(struct aliases_struct));
    new_alias->alias_name = strdup(alias_name);
    new_alias->command_string = strdup(command_string);
    new_alias->next = aliases;
    aliases = new_alias;
}

const char *search_alias(const char *alias_name) {
    struct aliases_struct *current_aliases = aliases;
    while (current_aliases) {
        if (strcmp(current_aliases->alias_name, alias_name) == 0) {
            return current_aliases->command_string;
        }
        current_aliases = current_aliases->next;
    }
	// If there is not alias found, it returns null
    return NULL;
}

// Save aliases to a file to be able to store and make the aliases live across the shell sessions
void save_aliases() {
    FILE *file = fopen("aliases.txt", "w");
    if (file == NULL) {
        perror("Error opening file for writing aliases");
        return;
    }

    struct aliases_struct *current_aliases = aliases;
    while (current_aliases) {
        fprintf(file, "alias %s %s\n", current_aliases->alias_name, current_aliases->command_string);
        current_aliases = current_aliases->next;
    }
    fclose(file);
}

// Load aliases from a file in the beginning
void load_aliases() {
    FILE *file = fopen("aliases.txt", "r");
    if (file == NULL) {
        //If no alias file is found
        return;
    }

    char line[300];
    while (fgets(line, sizeof(line), file)) {
        if (line[0] == '\n') {
			// To avoid empty lines
            continue;
        }
        char alias_name[100];
        char command_string[100];
        if (sscanf(line, "alias %s %s", alias_name, command_string) == 2) {
			// add previous aliases to the current aliases list
            add_alias(alias_name, command_string);
        }
    }
    fclose(file);
}

void find_string_in_all_files(const char *search_string) {
    struct dirent *entry;
    DIR *dp = opendir(".");

    if (dp == NULL) {
        perror("opendir");
        return;
    }

    while ((entry = readdir(dp))) {
		struct stat entry_stat;
        // Check if the entry is a regular file and has a .txt extension
		if (stat(entry->d_name, &entry_stat) == 0) {
			if (S_ISREG(entry_stat.st_mode) && strstr(entry->d_name, ".txt") != NULL) {
				char file_name[256];
            	sprintf(file_name, "%s", entry->d_name);

            	FILE *file = fopen(entry->d_name, "r");
            	if (file == NULL) {
            	    perror("fopen");
            	    continue;
            	}

            	char line[300];
            	int line_number = 1;
            	int found = 0;

            	// Search the string in the file
            	while (fgets(line, sizeof(line), file) != NULL) {
            	    if (strstr(line, search_string) != NULL) {
                	    // String found
                	    printf("%s\tString found in %s at line %d\n", file_name, entry->d_name, line_number);
            		   	found = 1;
                	    break;
                	}
                	line_number++;
            	}

            	if (!found) {
                	printf("%s\tString not found in %s\n", file_name, entry->d_name);
           		}

            	fclose(file);

			}

		}
        
    }

    closedir(dp);
}
