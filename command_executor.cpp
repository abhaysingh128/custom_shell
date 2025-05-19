#include <iostream>
#include <vector>
#include <string>
#include <windows.h>
#include <direct.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fstream>
#include <cstdio>
#include <iomanip>
#include <locale>
#include <codecvt>
#include <sstream>
#include "shell.h"
// Commented out shell.h since we're missing implementation for handle_alias_command
// #include "shell.h" 
using namespace std;

vector<string> command_history;

// Job management structures and functions
struct Job {
    int id;
    HANDLE hProcess;
    DWORD pid;
    string command;
    bool isRunning;
};

vector<Job> jobList;
int jobCounter = 1;

// Helper functions
string wide_to_narrow(const wchar_t* wide) {
    std::wstring_convert<std::codecvt_utf8<wchar_t>> converter;
    return converter.to_bytes(wide);
}

vector<string> split(const string &str, char delimiter) {
    vector<string> tokens;
    stringstream ss(str);  // Create a stringstream from the input string
    string token;

    // Use getline to split the string into tokens by the delimiter
    while (getline(ss, token, delimiter)) {
        tokens.push_back(token);
    }

    return tokens;
}

// Job management functions
void addJob(HANDLE hProcess, DWORD pid, const string& command) {
    Job newJob = { jobCounter++, hProcess, pid, command, true };
    jobList.push_back(newJob);
    cout << "[" << newJob.id << "] " << pid << " started in background\n";
}

void listJobs() {
    cout << "Active Background Jobs:\n";
    for (auto& job : jobList) {
        DWORD exitCode;
        GetExitCodeProcess(job.hProcess, &exitCode);

        // If process is still active, mark as Running
        bool stillRunning = (exitCode == STILL_ACTIVE);
        string status = stillRunning ? "Running" : "Exited";

        cout << "[" << job.id << "] PID: " << job.pid
             << " Command: " << job.command
             << " Status: " << status << endl;
    }
}

void fg(int jobId) {
    for (auto &job : jobList) {
        if (job.id == jobId) {
            cout << "Bringing job [" << job.id << "] to foreground...\n";
            WaitForSingleObject(job.hProcess, INFINITE);  // Wait for completion
            // Optional: Add timeout here to break after certain time.
            CloseHandle(job.hProcess);
            job.isRunning = false;
            return;
        }
    }
    cout << "Error: Job ID not found.\n";
}

void killJob(int jobId) {
    for (auto it = jobList.begin(); it != jobList.end(); ++it) {
        if (it->id == jobId) {
            cout << "Killing job [" << it->id << "]...\n";
            TerminateProcess(it->hProcess, 0);
            CloseHandle(it->hProcess);
            jobList.erase(it);
            return;
        }
    }
    cout << "Error: Job ID not found.\n";
}

void launchBackgroundProcess(const string& command) {
    STARTUPINFO si = { sizeof(si) };
    PROCESS_INFORMATION pi;
    string cmdLine = "cmd.exe /C " + command;

    // CreateProcess requires a writable char*
    char* cmd = _strdup(cmdLine.c_str());

    BOOL success = CreateProcess(
        NULL,         // No module name (use command line)
        cmd,          // Command line
        NULL,         // Process handle not inheritable
        NULL,         // Thread handle not inheritable
        FALSE,        // Set handle inheritance to FALSE
        CREATE_NO_WINDOW, // Don't create console window (set to 0 if you want one)
        NULL,         // Use parent's environment block
        NULL,         // Use parent's starting directory 
        &si,          // Pointer to STARTUPINFO structure
        &pi           // Pointer to PROCESS_INFORMATION structure
    );

    if (success) {
        addJob(pi.hProcess, pi.dwProcessId, command);
        // Don't close hProcess here — it's needed for job control
        CloseHandle(pi.hThread);
    } else {
        cerr << "Failed to launch process: " << command << endl;
    }

    free(cmd); // Free duplicated string
}

// Piping function
void runPipedCommand(const string &command) {
    vector<string> commands = split(command, '|'); // Split the command by '|'

    if (commands.size() != 2) {
        cout << "Error: Invalid command syntax for piping.\n";
        return;
    }

    string cmd1 = commands[0];
    string cmd2 = commands[1];

    // Create a pipe
    HANDLE readPipe, writePipe;
    if (!CreatePipe(&readPipe, &writePipe, NULL, 0)) {
        cerr << "Error creating pipe.\n";
        return;
    }

    // Execute the first command
    STARTUPINFO si1 = {0};
    PROCESS_INFORMATION pi1;
    si1.dwFlags = STARTF_USESTDHANDLES;
    si1.hStdOutput = writePipe;

    if (!CreateProcess(NULL, (LPSTR)cmd1.c_str(), NULL, NULL, TRUE, 0, NULL, NULL, &si1, &pi1)) {
        cerr << "Error executing command 1: " << cmd1 << "\n";
        return;
    }

    // Close write end of the pipe in the parent process
    CloseHandle(writePipe);

    // Execute the second command
    STARTUPINFO si2 = {0};
    PROCESS_INFORMATION pi2;
    si2.dwFlags = STARTF_USESTDHANDLES;
    si2.hStdInput = readPipe;

    if (!CreateProcess(NULL, (LPSTR)cmd2.c_str(), NULL, NULL, TRUE, 0, NULL, NULL, &si2, &pi2)) {
        cerr << "Error executing command 2: " << cmd2 << "\n";
        return;
    }

    // Close the read end of the pipe in the parent process
    CloseHandle(readPipe);

    // Wait for both processes to finish
    WaitForSingleObject(pi1.hProcess, INFINITE);
    WaitForSingleObject(pi2.hProcess, INFINITE);

    // Clean up
    CloseHandle(pi1.hProcess);
    CloseHandle(pi2.hProcess);
}

// File I/O redirection function
void runWithRedirection(const string &command) {
    string cmd, filename;
    bool redirectOutput = false, redirectInput = false;

    size_t pos;
    if ((pos = command.find('>')) != string::npos) {
        cmd = command.substr(0, pos);
        filename = command.substr(pos + 1);
        redirectOutput = true;
    } else if ((pos = command.find('<')) != string::npos) {
        cmd = command.substr(0, pos);
        filename = command.substr(pos + 1);
        redirectInput = true;
    } else {
        cmd = command;
    }

    // Trim spaces
    cmd.erase(cmd.find_last_not_of(" \n\r\t")+1);
    filename.erase(0, filename.find_first_not_of(" \n\r\t"));

    HANDLE hFile = NULL;

    if (redirectOutput) {
        hFile = CreateFileA(
            filename.c_str(),
            GENERIC_WRITE,
            FILE_SHARE_WRITE,
            NULL,
            CREATE_ALWAYS,
            FILE_ATTRIBUTE_NORMAL,
            NULL
        );
    } else if (redirectInput) {
        hFile = CreateFileA(
            filename.c_str(),
            GENERIC_READ,
            FILE_SHARE_READ,
            NULL,
            OPEN_EXISTING,
            FILE_ATTRIBUTE_NORMAL,
            NULL
        );
    }

    if (hFile == INVALID_HANDLE_VALUE) {
        cerr << "Failed to open file: " << filename << endl;
        return;
    }

    STARTUPINFOA si;
    PROCESS_INFORMATION pi;
    ZeroMemory(&si, sizeof(si));
    si.cb = sizeof(si);
    si.dwFlags |= STARTF_USESTDHANDLES;

    if (redirectOutput) si.hStdOutput = hFile;
    if (redirectInput) si.hStdInput = hFile;

    // Duplicate handles so child can use them
    SetHandleInformation(hFile, HANDLE_FLAG_INHERIT, HANDLE_FLAG_INHERIT);

    ZeroMemory(&pi, sizeof(pi));

    string fullCmd = "cmd.exe /C " + cmd;
    char* cmdline = _strdup(fullCmd.c_str());

    BOOL success = CreateProcessA(
        NULL,
        cmdline,
        NULL, NULL, TRUE,
        0, NULL, NULL,
        &si, &pi
    );

    if (!success) {
        cerr << "CreateProcess failed.\n";
    } else {
        WaitForSingleObject(pi.hProcess, INFINITE);
        CloseHandle(pi.hProcess);
        CloseHandle(pi.hThread);
    }

    CloseHandle(hFile);
    free(cmdline);
}

void runPingCommand(const string &host) {
    string command = "ping " + host;
    system(command.c_str());
}

void print_help() {
    cout << "Custom Shell Help:\n"
         << "  help       - Show this help message\n"
         << "  cd <dir>   - Change directory\n"
         << "  pwd        - Print working directory\n"
         << "  clear      - Clear the screen\n"
         << "  history    - Show command history\n"
         << "  ls [dir]   - List directory contents (short format)\n"
         << "  ll [dir]   - List directory contents (long format)\n"
         << "  mkdir <dir>- Create a directory\n"
         << "  touch <file>- Create/update a file\n"
         << "  rm <file>  - Delete a file\n"
         << "  cat <file> - Display contents of a file\n"
         << "  cp <src> <dst>- Copy file from src to dst\n"
         << "  mv <src> <dst>- Move (rename) file from src to dst\n"
         << "  time       - Show current time\n"
         << "  jobs       - List all background jobs\n"
         << "  fg <jobid> - Bring background job to foreground\n"
         << "  kill <jobid> - Kill a background job\n"
         << "  run <cmd>  - Run a command in the background\n"
         << "  ping <host>- Ping a host\n"
         << "  exit       - Exit the shell\n"
         << "  cmd1 | cmd2 - Pipe output of cmd1 to cmd2\n"
         << "  cmd > file - Redirect output to file\n"
         << "  cmd < file - Take input from file\n";
}

// Integrate the execute_command function with new capabilities
void execute_command(const vector<string>& args, const string& fullCommand) {
    if (args.empty()) return;

    command_history.push_back(args[0]);
  
    if (args[0] == "help") {
        print_help();
    }

    else if (args[0] == "cd") {
        if (args.size() < 2) {
            cerr << "Error: cd command requires a directory name.\n";
        } else {
            if (_chdir(args[1].c_str()) != 0) {
                cerr << "Error: Could not change directory to '" << args[1] << "'.\n";
                if (errno == ENOENT) {
                    cerr << "Directory does not exist.\n";
                }
            }
        }
    }

    else if (args[0] == "pwd") {
        char cwd[1024];
        if (_getcwd(cwd, sizeof(cwd))) {
            cout << "Current directory: " << cwd << endl;
        } else {
            cerr << "Error: Unable to get current directory.\n";
        }
    }

    else if (args[0] == "clear") {
        system("cls");
    }

    else if (args[0] == "history") {
        cout << "Command history (last 10 commands):\n";
        int start = (command_history.size() > 10) ? command_history.size() - 10 : 0;
        for (size_t i = start; i < command_history.size(); i++) {
            cout << "  " << i + 1 << ": " << command_history[i] << endl;
        }
    }

    else if (args[0] == "ls" || args[0] == "ll") {
        string path = (args.size() > 1) ? args[1] : ".";
        bool long_format = (args[0] == "ll");

        wstring wpath;
        if (path.back() != '\\' && path.back() != '/') {
            wpath = wstring(path.begin(), path.end()) + L"\\*";
        } else {
            wpath = wstring(path.begin(), path.end()) + L"*";
        }

        WIN32_FIND_DATAW findFileData;
        HANDLE hFind = FindFirstFileW(wpath.c_str(), &findFileData);

        if (hFind == INVALID_HANDLE_VALUE) {
            DWORD err = GetLastError();
            if (err != ERROR_FILE_NOT_FOUND) {
                cerr << "Error: Cannot access directory (code " << err << ").\n";
            }
            return;
        }

        do {
            string filename = wide_to_narrow(findFileData.cFileName);
            if (filename == "." || filename == "..") continue;

            if (long_format) {
                cout << ((findFileData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) ? "[D] " : "[F] ");
                cout << setw(30) << left << filename;
                
                if (!(findFileData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)) {
                    cout << " " << setw(10) << right 
                         << (findFileData.nFileSizeLow / 1024) << " KB";
                }
                cout << endl;
            } else {
                cout << filename << "  ";
            }
        } while (FindNextFileW(hFind, &findFileData) != 0);

        FindClose(hFind);
        if (!long_format) cout << endl;
    }

    else if (args[0] == "mkdir") {
        if (args.size() < 2) {
            cerr << "Error: mkdir requires a directory name.\n";
        } else {
            if (_mkdir(args[1].c_str()) == 0) {
                cout << "Directory '" << args[1] << "' created successfully.\n";
            } else {
                cerr << "Error: Could not create directory '" << args[1] << "'.\n";
                if (errno == EEXIST) {
                    cerr << "Directory already exists.\n";
                }
            }
        }
    }

    else if (args[0] == "touch") {
        if (args.size() < 2) {
            cerr << "Error: touch requires a filename.\n";
        } else {
            ofstream file(args[1], ios::app);
            if (file) {
                file.close();
                cout << "File '" << args[1] << "' created/updated successfully.\n";
            } else {
                cerr << "Error: Failed to create or modify file '" << args[1] << "'.\n";
            }
        }
    }

    else if (args[0] == "rm") {
        if (args.size() < 2) {
            cerr << "Error: rm requires a filename.\n";
        } else {
            if (remove(args[1].c_str()) == 0) {
                cout << "File '" << args[1] << "' deleted successfully.\n";
            } else {
                cerr << "Error: Could not delete file '" << args[1] << "'.\n";
                if (errno == ENOENT) {
                    cerr << "File does not exist.\n";
                }
            }
        }
    }

    else if (args[0] == "cat") {
        if (args.size() < 2) {
            cerr << "Error: cat requires a filename.\n";
        } else {
            ifstream file(args[1]);
            if (!file) {
                cerr << "Error: Cannot open file '" << args[1] << "'.\n";
            } else {
                string line;
                while (getline(file, line)) {
                    cout << line << endl;
                }
                file.close();
            }
        }
    }

    else if (args[0] == "cp") {
        if (args.size() < 3) {
            cerr << "Error: cp requires source and destination filenames.\n";
        } else {
            ifstream src(args[1], ios::binary);
            ofstream dst(args[2], ios::binary);
            if (!src || !dst) {
                cerr << "Error: Could not copy file.\n";
            } else {
                dst << src.rdbuf();
                cout << "File copied from '" << args[1] << "' to '" << args[2] << "'.\n";
            }
        }
    }

    else if (args[0] == "mv") {
        if (args.size() < 3) {
            cerr << "Error: mv requires source and destination filenames.\n";
        } else {
            if (rename(args[1].c_str(), args[2].c_str()) == 0) {
                cout << "File moved from '" << args[1] << "' to '" << args[2] << "'.\n";
            } else {
                cerr << "Error: Could not move file.\n";
            }
        }
    }

    else if (args[0] == "time") {
        SYSTEMTIME st;
        GetLocalTime(&st);
        printf("Current time: %02d:%02d:%02d\n", st.wHour, st.wMinute, st.wSecond);
    }

    // Program Manager Commands
    else if (args[0] == "jobs") {
        listJobs();
    }
    
    else if (args[0] == "fg" && args.size() > 1) {
        fg(stoi(args[1]));
    }
    
    else if (args[0] == "kill" && args.size() > 1) {
        killJob(stoi(args[1]));
    }
    
    else if (args[0] == "run" && args.size() > 1) {
        string cmd = fullCommand.substr(4); // Remove "run " prefix
        launchBackgroundProcess(cmd);
    }
    
    else if (args[0] == "ping" && args.size() > 1) {
        runPingCommand(args[1]);
    }
    
    // Handle piping and redirection based on the full command
    else if (fullCommand.find('|') != string::npos) {
        runPipedCommand(fullCommand);
    }
    
    else if (fullCommand.find('>') != string::npos || fullCommand.find('<') != string::npos) {
        runWithRedirection(fullCommand);
    }

    else {
        string cmd;
        for (const auto& arg : args) cmd += arg + " ";

        if (cmd.find(".exe") != string::npos || args[0] == "notepad" || args[0] == "calc") {
            system(("start \"\" " + cmd).c_str());
        } else {
            system(cmd.c_str());
        }
    }
}

// Updated main function integrating both functionalities
int main() {
    cout << "Integrated Shell v1.0\n";
    cout << "Type 'help' for available commands, 'exit' to quit.\n";
    
    while (true) {
        char cwd[1024];
        if (_getcwd(cwd, sizeof(cwd))) {
            cout << "\n" << cwd << " > ";
        } else {
            cout << "\nShell > ";
        }
        
        string input;
        getline(cin, input);

        if (input == "exit") break;
        
        // Check if we need to handle aliases - commented out until alias function is implemented
        // if (handle_alias_command(split(input, ' '))) continue;
        
        // Check if we need special handling for piping or redirection
        if (input.find('|') != string::npos) {
            runPipedCommand(input);
        }
        else if (input.find('>') != string::npos || input.find('<') != string::npos) {
            runWithRedirection(input);
        }
        else {
            // Split the command into arguments for regular processing
            vector<string> args;
            string arg;
            istringstream iss(input);
            while (iss >> arg) {
                args.push_back(arg);
            }
            
            execute_command(args, input);
        }
    }

    return 0;
}