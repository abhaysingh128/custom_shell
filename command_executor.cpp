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
         << "  ll [dir]   - List directory contents (long format with details)\n"
         << "  dir [dir]  - List directory contents (Windows style)\n"
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
         << "  alias [name='command'] - Create or list aliases\n"
         << "  exit       - Exit the shell\n"
         << "\nSpecial Features:\n"
         << "  cmd1 | cmd2 - Pipe output of cmd1 to cmd2\n"
         << "  cmd > file - Redirect output to file\n"
         << "  cmd < file - Take input from file\n"
         << "  cmd &      - Run command in background\n";
}

void execute_command(const vector<string>& args) {
    if (args.empty()) return;

    string command = args[0];
    
    // Handle built-in commands
    if (command == "cd") {
        if (args.size() < 2) {
            char current_dir[MAX_PATH];
            if (GetCurrentDirectoryA(MAX_PATH, current_dir)) {
                cout << current_dir << endl;
            }
        } else {
            if (_chdir(args[1].c_str()) != 0) {
                cerr << "Error changing directory" << endl;
            }
        }
    }
    else if (command == "clear") {
        system("cls");  // Windows clear screen command
    }
    else if (command == "pwd") {
        char cwd[1024];
        if (_getcwd(cwd, sizeof(cwd))) {
            cout << cwd << endl;
        }
    }
    else if (command == "history") {
        cout << "Command history (last 10 commands):\n";
        int start = (command_history.size() > 10) ? command_history.size() - 10 : 0;
        for (size_t i = start; i < command_history.size(); i++) {
            cout << "  " << i + 1 << ": " << command_history[i] << endl;
        }
    }
    else if (command == "ls" || command == "ll") {
        WIN32_FIND_DATAA findData;
        HANDLE hFind;
        string path = args.size() > 1 ? args[1] : ".";
        bool long_format = (command == "ll");
        
        hFind = FindFirstFileA((path + "\\*").c_str(), &findData);
        if (hFind != INVALID_HANDLE_VALUE) {
            do {
                if (strcmp(findData.cFileName, ".") == 0 || strcmp(findData.cFileName, "..") == 0)
                    continue;
                    
                if (long_format) {
                    // Get file size
                    ULARGE_INTEGER fileSize;
                    fileSize.LowPart = findData.nFileSizeLow;
                    fileSize.HighPart = findData.nFileSizeHigh;
                    
                    // Get file attributes
                    string attributes;
                    if (findData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
                        attributes += "d";
                    if (findData.dwFileAttributes & FILE_ATTRIBUTE_READONLY)
                        attributes += "r";
                    if (findData.dwFileAttributes & FILE_ATTRIBUTE_HIDDEN)
                        attributes += "h";
                    if (findData.dwFileAttributes & FILE_ATTRIBUTE_SYSTEM)
                        attributes += "s";
                    
                    // Get last write time
                    SYSTEMTIME st;
                    FileTimeToSystemTime(&findData.ftLastWriteTime, &st);
                    
                    cout << setw(10) << left << attributes
                         << setw(12) << right << fileSize.QuadPart
                         << " " << setw(2) << st.wMonth << "/" 
                         << setw(2) << st.wDay << "/" 
                         << setw(4) << st.wYear << " "
                         << setw(2) << st.wHour << ":"
                         << setw(2) << st.wMinute << " "
                         << findData.cFileName << endl;
                } else {
                    cout << findData.cFileName << endl;
                }
            } while (FindNextFileA(hFind, &findData));
            FindClose(hFind);
        }
    }
    else if (command == "alias") {
        handle_alias_command(args);
    }
    else if (command == "mkdir") {
        if (args.size() < 2) {
            cerr << "Error: mkdir requires a directory name" << endl;
        } else {
            if (_mkdir(args[1].c_str()) == 0) {
                cout << "Directory created successfully" << endl;
            } else {
                cerr << "Error creating directory" << endl;
            }
        }
    }
    else if (command == "touch") {
        if (args.size() < 2) {
            cerr << "Error: touch requires a filename" << endl;
        } else {
            ofstream file(args[1], ios::app);
            if (file) {
                file.close();
                cout << "File created/updated successfully" << endl;
            } else {
                cerr << "Error creating/updating file" << endl;
            }
        }
    }
    else if (command == "rm") {
        if (args.size() < 2) {
            cerr << "Error: rm requires a filename" << endl;
        } else {
            if (remove(args[1].c_str()) == 0) {
                cout << "File deleted successfully" << endl;
            } else {
                cerr << "Error deleting file" << endl;
            }
        }
    }
    else if (command == "cat") {
        if (args.size() < 2) {
            cerr << "Error: cat requires a filename" << endl;
        } else {
            ifstream file(args[1]);
            if (file) {
                string line;
                while (getline(file, line)) {
                    cout << line << endl;
                }
                file.close();
            } else {
                cerr << "Error reading file" << endl;
            }
        }
    }
    else if (command == "help") {
        print_help();
    }
    else if (command == "jobs") {
        listJobs();
    }
    else if (command == "fg") {
        if (args.size() < 2) {
            cout << "Usage: fg <job_id>" << endl;
            return;
        }
        fg(stoi(args[1]));
    }
    else if (command == "kill") {
        if (args.size() < 2) {
            cout << "Usage: kill <job_id>" << endl;
            return;
        }
        killJob(stoi(args[1]));
    }
    else {
        // Handle external commands
        string fullCommand;
        for (const auto& arg : args) {
            fullCommand += arg + " ";
        }
        
        // Check for background execution
        if (fullCommand.back() == '&') {
            fullCommand.pop_back(); // Remove the &
            launchBackgroundProcess(fullCommand);
            return;
        }
        
        // Check for piping
        if (fullCommand.find('|') != string::npos) {
            runPipedCommand(fullCommand);
            return;
        }
        
        // Check for redirection
        if (fullCommand.find('>') != string::npos || fullCommand.find('<') != string::npos) {
            runWithRedirection(fullCommand);
            return;
        }
        
        // Regular command execution
        STARTUPINFOA si;
        PROCESS_INFORMATION pi;
        ZeroMemory(&si, sizeof(si));
        si.cb = sizeof(si);
        ZeroMemory(&pi, sizeof(pi));
        
        // Use CreateProcess directly for better control
        string cmdLine = fullCommand;
        char* cmd = _strdup(cmdLine.c_str());
        
        BOOL success = CreateProcessA(
            NULL,           // No module name (use command line)
            cmd,            // Command line
            NULL,           // Process handle not inheritable
            NULL,           // Thread handle not inheritable
            FALSE,          // Set handle inheritance to FALSE
            CREATE_NEW_CONSOLE, // Create new console window
            NULL,           // Use parent's environment block
            NULL,           // Use parent's starting directory
            &si,            // Pointer to STARTUPINFO structure
            &pi             // Pointer to PROCESS_INFORMATION structure
        );
        
        if (success) {
            // Wait for the process to complete
            WaitForSingleObject(pi.hProcess, INFINITE);
            
            // Close process and thread handles
            CloseHandle(pi.hProcess);
            CloseHandle(pi.hThread);
        } else {
            cerr << "Failed to execute command: " << fullCommand << endl;
        }
        
        free(cmd);
    }
}