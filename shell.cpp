#include <iostream>
#include <vector>
#include <windows.h>
#include <string>
#include <sstream>
#include <fstream>
#include <direct.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <cstdio>
#include <iomanip>
#include <locale>
#include <codecvt>
#include <map>
#include <csignal>
#include <conio.h>
#include <algorithm>
#include <io.h>

using namespace std;

// Global variables
const string shellPassword = "1234";
const string notesFile = "shell_notes.txt";
vector<string> command_history;
map<string, string> alias_map;
volatile sig_atomic_t interrupted = 0;
int history_index = -1;

// Forward declarations
string resolve_alias(const string& input);
void execute_command(const vector<string>& args);
void print_help();
void autocomplete(string& input);
string get_input_with_features();
void signal_handler(int signal);
void init_signals();

// Signal handler
void signal_handler(int signal) {
    if (signal == SIGINT) {
        interrupted = 1;
        cout << "\nInterrupt received (Ctrl+C)\n";
    }
}

void init_signals() {
    signal(SIGINT, signal_handler);
}

// Alias handling
string resolve_alias(const string& input) {
    istringstream iss(input);
    string first_word;
    iss >> first_word;

    if (alias_map.count(first_word)) {
        string replaced = alias_map[first_word] + input.substr(first_word.length());
        return replaced;
    }
    return input;
}

bool handle_alias_command(const vector<string>& args) {
    if (args.empty() || args[0] != "alias") return false;

    string full_command;
    for (const auto& arg : args) full_command += arg + " ";
    if (!full_command.empty()) full_command.pop_back(); 

    if (args.size() == 1) {
        for (const auto& pair : alias_map) {
            cout << "alias " << pair.first << "='" << pair.second << "'\n";
        }
    } else {
        string rest = full_command.substr(6);
        size_t eq_pos = rest.find('=');
        if (eq_pos == string::npos) {
            cerr << "Invalid alias format. Use: alias name='command'\n";
            return true;
        }

        string key = rest.substr(0, eq_pos);
        string val = rest.substr(eq_pos + 1);

        if ((val.front() == '\'' && val.back() == '\'') || (val.front() == '"' && val.back() == '"')) {
            val = val.substr(1, val.size() - 2);
        }

        alias_map[key] = val;
    }

    return true;
}

// Input handling
string wide_to_narrow(const wchar_t* wide) {
    std::wstring_convert<std::codecvt_utf8<wchar_t>> converter;
    return converter.to_bytes(wide);
}

void autocomplete(string& input) {
    size_t last_space = input.find_last_of(" ");
    string prefix = (last_space == string::npos) ? input : input.substr(last_space + 1);
    string base = (last_space == string::npos) ? "" : input.substr(0, last_space + 1);

    vector<string> suggestions;

    WIN32_FIND_DATA findFileData;
    HANDLE hFind = FindFirstFile("*.*", &findFileData);
    
    if (hFind != INVALID_HANDLE_VALUE) {
        do {
            string name = findFileData.cFileName;
            if (name != "." && name != ".." && name.rfind(prefix, 0) == 0) {
                suggestions.push_back(name);
            }
        } while (FindNextFile(hFind, &findFileData));
        FindClose(hFind);
    }

    vector<string> commands = {
        "help", "cd", "pwd", "clear", "history", "ls", "ll", "mkdir",
        "touch", "rm", "cat", "cp", "mv", "time", "exit", "jobs", "fg",
        "kill", "run", "ping", "schedule", "note", "lock"
    };

    for (const auto& cmd : commands) {
        if (cmd.rfind(prefix, 0) == 0) {
            suggestions.push_back(cmd);
        }
    }

    if (!suggestions.empty()) {
        input = base + suggestions[0];
        cout << "\r" << input << string(50, ' ');
    }
}

string get_input_with_features() {
    string input;
    char ch;

    while (true) {
        ch = _getch();
        if (ch == '\r') {
            cout << endl;
            break;
        } else if (ch == '\b') {
            if (!input.empty()) {
                input.pop_back();
                cout << "\b \b";
            }
        } else if (ch == 0 || ch == -32) {
            ch = _getch();

            if (ch == 72) {
                if (!command_history.empty() && history_index > 0)
                    history_index--;
                else if (history_index == -1 && !command_history.empty())
                    history_index = (int)command_history.size() - 1;

                if (history_index >= 0 && history_index < (int)command_history.size()) {
                    input = command_history[history_index];
                    cout << "\r" << input << string(50, ' ');
                }
            } else if (ch == 80) {
                if (!command_history.empty() && history_index < (int)command_history.size() - 1)
                    history_index++;
                else
                    history_index = -1;

                if (history_index >= 0 && history_index < (int)command_history.size()) {
                    input = command_history[history_index];
                    cout << "\r" << input << string(50, ' ');
                } else {
                    input.clear();
                    cout << "\r" << string(50, ' ');
                }
            }
        } else if (ch == '\t') {
            autocomplete(input);
        } else {
            input += ch;
            cout << ch;
        }
    }

    if (!input.empty()) {
        command_history.push_back(input);
        history_index = -1;
    }

    return resolve_alias(input);
}

// Command execution
void execute_command(const vector<string>& args) {
    if (handle_alias_command(args)) return;

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
         << "  time       - Show current time\n"
         << "  exit       - Exit the shell\n"
         << "  cat <file>   - Display contents of a file\n"
         << "  cp <src> <dst>- Copy file from src to dst\n"
         << "  mv <src> <dst>- Move (rename) file from src to dst\n"
         << "  jobs       - List background jobs\n"
         << "  fg <job>   - Bring job to foreground\n"
         << "  kill <job> - Kill a background job\n"
         << "  run <cmd>  - Run command in background\n"
         << "  ping <host>- Ping a host\n"
         << "  schedule <cmd> at <sec>- Schedule a command\n"
         << "  note add <text>- Add a note\n"
         << "  note view  - View notes\n"
         << "  lock      - Lock the shell\n"
         << "  alias     - Manage command aliases\n";
}

struct Job {
    int id;
    HANDLE hProcess;
    DWORD pid;
    string command;
    bool isRunning;
};

vector<Job> jobList;
int jobCounter = 1;

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
        bool stillRunning = (exitCode == STILL_ACTIVE);
        string status = stillRunning ? "Running" : "Exited";
        cout << "[" << job.id << "] PID: " << job.pid << " Command: " << job.command << " Status: " << status << endl;
    }
}

void fg(int jobId) {
    for (auto it = jobList.begin(); it != jobList.end(); ++it) {
        if (it->id == jobId) {
            cout << "Bringing job [" << it->id << "] to foreground...\n";
            WaitForSingleObject(it->hProcess, INFINITE);
            CloseHandle(it->hProcess);
            jobList.erase(it);
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
    char* cmd = _strdup(cmdLine.c_str());

    BOOL success = CreateProcess(NULL, cmd, NULL, NULL, FALSE, CREATE_NO_WINDOW, NULL, NULL, &si, &pi);
    if (success) {
        addJob(pi.hProcess, pi.dwProcessId, command);
        CloseHandle(pi.hThread);
    } else {
        cerr << "Failed to launch process: " << command << endl;
    }
    free(cmd);
}

vector<string> split(const string &str, char delimiter) {
    vector<string> tokens;
    stringstream ss(str);
    string token;
    while (getline(ss, token, delimiter)) {
        tokens.push_back(token);
    }
    return tokens;
}

void runPipedCommand(const string &command) {
    vector<string> commands = split(command, '|');
    if (commands.size() != 2) {
        cout << "Error: Only single pipe supported.\n";
        return;
    }

    string cmd1 = "cmd.exe /C " + commands[0];
    string cmd2 = "cmd.exe /C " + commands[1];

    HANDLE readPipe, writePipe;
    SECURITY_ATTRIBUTES sa = { sizeof(SECURITY_ATTRIBUTES), NULL, TRUE };

    if (!CreatePipe(&readPipe, &writePipe, &sa, 0)) {
        cerr << "Error creating pipe.\n";
        return;
    }

    STARTUPINFOA si1 = {};
    PROCESS_INFORMATION pi1 = {};
    si1.cb = sizeof(si1);
    si1.dwFlags = STARTF_USESTDHANDLES;
    si1.hStdOutput = writePipe;
    si1.hStdError = GetStdHandle(STD_ERROR_HANDLE);

    SetHandleInformation(writePipe, HANDLE_FLAG_INHERIT, HANDLE_FLAG_INHERIT);

    char* cmdline1 = _strdup(cmd1.c_str());
    if (!CreateProcessA(NULL, cmdline1, NULL, NULL, TRUE, 0, NULL, NULL, &si1, &pi1)) {
        cerr << "Error launching first command.\n";
        CloseHandle(writePipe);
        CloseHandle(readPipe);
        free(cmdline1);
        return;
    }
    free(cmdline1);
    CloseHandle(writePipe);

    STARTUPINFOA si2 = {};
    PROCESS_INFORMATION pi2 = {};
    si2.cb = sizeof(si2);
    si2.dwFlags = STARTF_USESTDHANDLES;
    si2.hStdInput = readPipe;
    si2.hStdError = GetStdHandle(STD_ERROR_HANDLE);
    si2.hStdOutput = GetStdHandle(STD_OUTPUT_HANDLE);

    SetHandleInformation(readPipe, HANDLE_FLAG_INHERIT, HANDLE_FLAG_INHERIT);

    char* cmdline2 = _strdup(cmd2.c_str());
    if (!CreateProcessA(NULL, cmdline2, NULL, NULL, TRUE, 0, NULL, NULL, &si2, &pi2)) {
        cerr << "Error launching second command.\n";
        CloseHandle(readPipe);
        free(cmdline2);
        return;
    }
    free(cmdline2);
    CloseHandle(readPipe);

    WaitForSingleObject(pi1.hProcess, INFINITE);
    WaitForSingleObject(pi2.hProcess, INFINITE);

    CloseHandle(pi1.hProcess);
    CloseHandle(pi1.hThread);
    CloseHandle(pi2.hProcess);
    CloseHandle(pi2.hThread);
}

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
    cmd.erase(cmd.find_last_not_of(" \n\r\t")+1);
    filename.erase(0, filename.find_first_not_of(" \n\r\t"));

    HANDLE hFile = NULL;
    if (redirectOutput) {
        hFile = CreateFileA(filename.c_str(), GENERIC_WRITE, FILE_SHARE_WRITE, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    } else if (redirectInput) {
        hFile = CreateFileA(filename.c_str(), GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    }
    if (hFile == INVALID_HANDLE_VALUE) {
        cerr << "Failed to open file: " << filename << endl;
        return;
    }

    STARTUPINFOA si = {};
    PROCESS_INFORMATION pi = {};
    si.cb = sizeof(si);
    si.dwFlags |= STARTF_USESTDHANDLES;
    if (redirectOutput) si.hStdOutput = hFile;
    if (redirectInput) si.hStdInput = hFile;
    si.hStdError = GetStdHandle(STD_ERROR_HANDLE);
    SetHandleInformation(hFile, HANDLE_FLAG_INHERIT, HANDLE_FLAG_INHERIT);

    string fullCmd = "cmd.exe /C " + cmd;
    char* cmdline = _strdup(fullCmd.c_str());
    BOOL success = CreateProcessA(NULL, cmdline, NULL, NULL, TRUE, 0, NULL, NULL, &si, &pi);

    if (!success) cerr << "CreateProcess failed.\n";
    else {
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

void scheduleCommand(const string& command, int delaySeconds) {
    cout << "Scheduling command: \"" << command << "\" to run after " << delaySeconds << " seconds.\n";
    Sleep(delaySeconds * 1000);
    system(command.c_str());
}

void addNote(const string& note) {
    ofstream out(notesFile, ios::app);
    if (!out) {
        cerr << "Unable to open notes file.\n";
        return;
    }
    out << note << endl;
    cout << "Note added.\n";
}

void viewNotes() {
    ifstream in(notesFile);
    string line;
    cout << "Shell Notes:\n";
    while (getline(in, line)) {
        cout << "- " << line << endl;
    }
}

bool authenticateShell() {
    string pass;
    cout << "Enter shell password: ";
    getline(cin, pass);
    return pass == shellPassword;
}

void lockShell() {
    string pass;
    cout << "Shell locked. Enter password to unlock:\n";
    do {
        cout << "Password: ";
        getline(cin, pass);
        if (pass != shellPassword) cout << "Incorrect password. Try again.\n";
    } while (pass != shellPassword);
    cout << "Shell unlocked.\n";
}

int main() {
    init_signals();

    if (!authenticateShell()) {
        cout << "Incorrect password. Exiting shell.\n";
        return 1;
    }

    while (true) {
        cout << "\nShell> ";
        string input = get_input_with_features();

        if (input == "exit") break;
        
        vector<string> args;
        istringstream iss(input);
        string arg;
        while (iss >> arg) {
            args.push_back(arg);
        }

        if (!args.empty()) {
            if (args[0] == "jobs") listJobs();
            else if (args[0] == "fg" && args.size() > 1) fg(stoi(args[1]));
            else if (args[0] == "kill" && args.size() > 1) killJob(stoi(args[1]));
            else if (args[0] == "run" && args.size() > 1) {
                string cmd = input.substr(4);
                launchBackgroundProcess(cmd);
            }
            else if (args[0] == "ping" && args.size() > 1) runPingCommand(args[1]);
            else if (args[0] == "schedule" && args.size() > 3 && args[args.size()-2] == "at") {
                string cmd = input.substr(9, input.find(" at ") - 9);
                int delay = stoi(args.back());
                scheduleCommand(cmd, delay);
            }
            else if (args[0] == "note") {
                if (args.size() > 2 && args[1] == "add") {
                    string note = input.substr(9);
                    addNote(note);
                }
                else if (args.size() > 1 && args[1] == "view") viewNotes();
            }
            else if (args[0] == "lock") lockShell();
            else execute_command(args);
        }
    }

    return 0;
}
