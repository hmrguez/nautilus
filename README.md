# Nautilus Shell
Nautilus is a Unix shell written in C that allows users to interact with the operating system by typing commands. It includes several features that are commonly found in Unix shells, such as input/output redirection, append redirection, and multipiping commands. This was a part of our 3rd year CS course about Operating Systems.
## Features
- Input redirection: allows the user to redirect standard input from a file instead of the keyboard (<).
- Output redirection: allows the user to redirect standard output to a file instead of the screen (>).
- Append redirection: allows the user to append standard output to a file instead of overwriting it (>>).
- Multipiping: allows the user to chain multiple commands together using the pipe operator (|).
- cd command: allows the user to change the working directory.
- help command: displays a help message with information about the built-in commands.
- exit command: terminates the shell.
- history command: displays a list of the most recently executed commands.
- again command: executes the indexed command in the history list.
- Ctrl + C command: kills the current process being executed using signals.

## Usage
To use Nautilus, simply download the source code and compile it using a C compiler. Once compiled, run the executable to start the shell. The shell will display a prompt ($) and wait for the user to enter a command. To execute a command, type it at the prompt and press Enter.

For detailed information about each command, type help at the prompt.

## Contribution
If you would like to contribute to Nautilus, please submit a pull request with your changes. We welcome contributions from anyone, regardless of their level of experience with C or Unix shells.
