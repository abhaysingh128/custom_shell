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
#include <cctype>
#include <cmath>

using namespace std;

// Global variables
const string shellPassword = "1234";
const string notesFile = "shell_notes.txt";
vector<string> command_history;
map<string, string> alias_map;
volatile sig_atomic_t interrupted = 0;
int history_index = -1;

struct Job {
    int id;
    HANDLE hProcess;
    DWORD pid;
    string command;
    bool isRunning;
};

vector<Job> jobList;
int jobCounter = 1;

// Forward declarations
string resolve_alias(const string& input);
void execute_command(const vector<string>& args);
void print_help();
void autocomplete(string& input);
string get_input_with_features();
void signal_handler(int signal);
void init_signals();
void count_word_in_file(const string& filename, const string& word);
void word_frequency(const string& filename);
void calculator(const string& num1_str, const string& op, const string& num2_str);
void addJob(HANDLE hProcess, DWORD pid, const string& command);
void listJobs();
void fg(int jobId);
void killJob(int jobId);
void launchBackgroundProcess(const string& command);
void runPipedCommand(const string& command);
void runWithRedirection(const string& command);
void runPingCommand(const string& host);
void scheduleCommand(const string& command, int delaySeconds);
void addNote(const string& note);
void viewNotes();
bool authenticateShell();
void lockShell();

// Helper function to convert string to lowercase
string to_lower(string str) {
    transform(str.begin(), str.end(), str.begin(), ::tolower);
    return str;
}

// Helper function to split string into words
vector<string> split_words(const string& text) {
    vector<string> words;
    istringstream iss(text);
    string word;
    while (iss >> word) {
        // Remove punctuation
        word.erase(remove_if(word.begin(), word.end(), ::ispunct), word.end());
        if (!word.empty()) {
            words.push_back(to_lower(word));
        }
    }
    return words;
}

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

// String conversion helper
string wide_to_narrow(const wchar_t* wide) {
    wstring_convert<codecvt_utf8<wchar_t>> converter;
    return converter.to_bytes(wide);
}

void autocomplete(string& input) {
    size_t last_space = input.find_last_of(" ");
    string prefix = (last_space == string::npos) ? input : input.substr(last_space + 1);
    string base = (last_space == string::npos) ? "" : input.substr(0, last_space + 1);

    vector<string> suggestions;

    WIN32_FIND_DATAA findFileData;
    HANDLE hFind = FindFirstFileA("*.*", &findFileData);

    if (hFind != INVALID_HANDLE_VALUE) {
        do {
            string name = findFileData.cFileName;
            if (name != "." && name != ".." && name.rfind(prefix, 0) == 0) {
                suggestions.push_back(name);
            }
        } while (FindNextFileA(hFind, &findFileData));
        FindClose(hFind);
    }

    vector<string> commands = {
        "help", "cd", "pwd", "clear", "history", "ls", "ll", "mkdir",
        "touch", "rm", "cat", "cp", "mv", "time", "exit", 
        "banao", "hatao", "dikhhao", "badlo", "count", "wordfreq", "calc"
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

            if (ch == 72) { // Up arrow
                if (!command_history.empty() && history_index > 0)
                    history_index--;
                else if (history_index == -1 && !command_history.empty())
                    history_index = (int)command_history.size() - 1;

                if (history_index >= 0 && history_index < (int)command_history.size()) {
                    input = command_history[history_index];
                    cout << "\r" << input << string(50, ' ');
                }
            } else if (ch == 80) { // Down arrow
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

void count_word_in_file(const string& filename, const string& word) {
    ifstream file(filename);
    if (!file) {
        cerr << "Error: Cannot open file '" << filename << "'" << endl;
        return;
    }
    
    string search_word = to_lower(word);
    string line;
    int count = 0;
    
    while (getline(file, line)) {
        vector<string> words = split_words(line);
        for (const string& w : words) {
            if (w == search_word) {
                count++;
            }
        }
    }
    
    cout << "Word '" << word << "' appears " << count << " times in '" << filename << "'" << endl;
    file.close();
}

void word_frequency(const string& filename) {
    ifstream file(filename);
    if (!file) {
        cerr << "Error: Cannot open file '" << filename << "'" << endl;
        return;
    }
    
    map<string, int> word_count;
    string line;
    
    while (getline(file, line)) {
        vector<string> words = split_words(line);
        for (const string& word : words) {
            if (!word.empty()) {
                word_count[word]++;
            }
        }
    }
    
    vector<pair<string, int>> freq_pairs(word_count.begin(), word_count.end());
    
    sort(freq_pairs.begin(), freq_pairs.end(), 
         [](const pair<string, int>& a, const pair<string, int>& b) {
             return a.second > b.second;
         });
    
    cout << "Top 10 most frequent words in '" << filename << "':" << endl;
    int display_count = min(10, (int)freq_pairs.size());
    for (int i = 0; i < display_count; i++) {
        cout << setw(15) << left << freq_pairs[i].first 
             << ": " << freq_pairs[i].second << endl;
    }
    
    file.close();
}

void calculator(const string& num1_str, const string& op, const string& num2_str) {
    try {
        double num1 = stod(num1_str);
        double num2 = stod(num2_str);
        double result = 0;
        bool valid_op = true;
        
        if (op == "+") {
            result = num1 + num2;
        } else if (op == "-") {
            result = num1 - num2;
        } else if (op == "*" || op == "x") {
            result = num1 * num2;
        } else if (op == "/") {
            if (num2 == 0) {
                cerr << "Error: Division by zero" << endl;
                return;
            }
            result = num1 / num2;
        } else if (op == "%") {
            if (num2 == 0) {
                cerr << "Error: Division by zero" << endl;
                return;
            }
            result = fmod(num1, num2);
        } else if (op == "^" || op == "**") {
            result = pow(num1, num2);
        } else {
            valid_op = false;
            cerr << "Error: Invalid operator '" << op << "'" << endl;
            cerr << "Supported operators: +, -, *, /, %, ^ (or **)" << endl;
        }
        
        if (valid_op) {
            cout << num1 << " " << op << " " << num2 << " = " << result << endl;
        }
    } catch (const invalid_argument& e) {
        cerr << "Error: Invalid number format" << endl;
    } catch (const out_of_range& e) {
        cerr << "Error: Number out of range" << endl;
    }
}

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
         << "\nCustom Commands:\n"
         << "  count <file> <word>    - Count occurrences of word in file\n"
         << "  wordfreq <file>        - Show word frequency in file (top 10)\n"
         << "  calc <num1> <op> <num2>- Calculator (+, -, *, /, %, ^)\n"
         << "\nHindi Commands:\n"
         << "  banao <file>     - Create/update a file\n"
         << "  hatao <file>     - Delete a file\n"
         << "  dikhhao <file>   - Display file contents\n"
         << "  badlo <old> <new>- Rename file\n";
}

void execute_command(const vector<string>& args) {
    if (args.empty()) return;
    string command = args[0];
    command.erase(0, command.find_first_not_of(" \n\r\t"));
    command.erase(command.find_last_not_of(" \n\r\t") + 1);
    for (auto &c : command) c = tolower(c);
    cout << "[DEBUG] Running command: '" << command << "'" << endl;
    
    if (command == "jobs") {
        listJobs();
        return;
    }
    else if (command == "fg" && args.size() > 1) {
        fg(stoi(args[1]));
        return;
    }
    else if (command == "kill" && args.size() > 1) {
        killJob(stoi(args[1]));
        return;
    }
    else if (command == "run" && args.size() > 1) {
        string cmd;
        for (size_t i = 1; i < args.size(); ++i) {
            cmd += args[i] + " ";
        }
        launchBackgroundProcess(cmd);
        return;
    }
    else if (command == "ping" && args.size() > 1) {
        runPingCommand(args[1]);
        return;
    }
    else if (command == "schedule" && args.size() > 3 && args[args.size()-2] == "at") {
        string cmd;
        for (size_t i = 1; i < args.size() - 2; ++i) {
            cmd += args[i] + " ";
        }
        int delay = stoi(args.back());
        scheduleCommand(cmd, delay);
        return;
    }
    else if (command == "note") {
        if (args.size() > 2 && args[1] == "add") {
            string note;
            for (size_t i = 2; i < args.size(); ++i) {
                note += args[i] + " ";
            }
            addNote(note);
        }
        else if (args.size() > 1 && args[1] == "view") {
            viewNotes();
        }
        return;
    }
    else if (command == "lock") {
        lockShell();
        return;
    }
    else if (command == "count") {
        cout << "[DEBUG] Entered 'count' handler\n";
        if (args.size() < 3) {
            cerr << "Usage: count <filename> <word>" << endl;
            cerr << "Example: count myfile.txt hello" << endl;
        } else {
            count_word_in_file(args[1], args[2]);
        }
        return;
    }
    else if (command == "wordfreq") {
        if (args.size() < 2) {
            cerr << "Usage: wordfreq <filename>" << endl;
            cerr << "Example: wordfreq myfile.txt" << endl;
        } else {
            word_frequency(args[1]);
        }
        return;
    }
    else if (command == "calc") {
        if (args.size() < 4) {
            cerr << "Usage: calc <num1> <operator> <num2>" << endl;
            cerr << "Example: calc 10 + 5" << endl;
            cerr << "Operators: +, -, *, /, %, ^ (or **)" << endl;
        } else {
            calculator(args[1], args[2], args[3]);
        }
        return;
    }
    else if (command == "cd") {
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
        return;
    }
    else if (command == "hatao") {
        if (args.size() < 2) {
            cerr << "Error: hatao requires a filename" << endl;
        } else {
            if (remove(args[1].c_str()) == 0) {
                cout << "File deleted successfully (via hatao)" << endl;
            } else {
                cerr << "Error deleting file (via hatao)" << endl;
            }
        }
        return;
    }
    else if (command == "banao") {
        if (args.size() < 2) {
            cerr << "Error: banao requires a filename" << endl;
        } else {
            ofstream file(args[1], ios::app);
            if (file) {
                file.close();
                cout << "File created/updated successfully (via banao)" << endl;
            } else {
                cerr << "Error creating/updating file (via banao)" << endl;
            }
        }
        return;
    }
    else if (command == "dikhhao") {
        if (args.size() < 2) {
            cerr << "Error: dikhhao requires a filename" << endl;
        } else {
            ifstream file(args[1]);
            if (file) {
                string line;
                while (getline(file, line)) {
                    cout << line << endl;
                }
                file.close();
            } else {
                cerr << "Error reading file (via dikhhao)" << endl;
            }
        }
        return;
    }
    else if (command == "badlo") {
        if (args.size() < 3) {
            cerr << "Error: badlo requires source and destination filenames" << endl;
        } else {
            if (rename(args[1].c_str(), args[2].c_str()) == 0) {
                cout << "File renamed from '" << args[1] << "' to '" << args[2] << "' (via badlo)" << endl;
            } else {
                cerr << "Error renaming file (via badlo)" << endl;
            }
        }
        return;
    }
    else if (command == "clear") {
        system("cls");
        return;
    }
    else if (command == "pwd") {
        char cwd[1024];
        if (_getcwd(cwd, sizeof(cwd))) {
            cout << cwd << endl;
        }
        return;
    }
    else if (command == "history") {
        cout << "Command history (last 10 commands):\n";
        int start = (command_history.size() > 10) ? command_history.size() - 10 : 0;
        for (size_t i = start; i < command_history.size(); i++) {
            cout << "  " << i + 1 << ": " << command_history[i] << endl;
        }
        return;
    }
    else if (command == "help") {
        print_help();
        return;
    }
    else if (command == "ls" || command == "ll") {
        string path = (args.size() > 1) ? args[1] : ".";
        bool long_format = (command == "ll");

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
        return;
    }
    else if (command == "mkdir") {
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
        return;
    }
    else if (command == "touch") {
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
        return;
    }
    else if (command == "rm") {
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
        return;
    }
    else if (command == "cat") {
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
        return;
    }
    else if (command == "cp") {
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
        return;
    }
    else if (command == "mv") {
        if (args.size() < 3) {
            cerr << "Error: mv requires source and destination filenames.\n";
        } else {
            if (rename(args[1].c_str(), args[2].c_str()) == 0) {
                cout << "File moved from '" << args[1] << "' to '" << args[2] << "'.\n";
            } else {
                cerr << "Error: Could not move file.\n";
            }
        }
        return;
    }
    else if (command == "time") {
        SYSTEMTIME st;
        GetLocalTime(&st);
        printf("Current time: %02d:%02d:%02d\n", st.wHour, st.wMinute, st.wSecond);
        return;
    }
    else {
        string cmd;
        for (const auto& arg : args) cmd += arg + " ";

        if (cmd.find(".exe") != string::npos || command == "notepad" || command == "calc") {
            system(("start \"\" " + cmd).c_str());
        } else {
            system(cmd.c_str());
        }
    }
}

int main() {
    string input;
    vector<string> args;

    init_signals();

    if (!authenticateShell()) {
        cout << "Incorrect password. Exiting shell.\n";
        return 1;
    }

    cout << "Custom Shell (type 'help' for commands)\n";

    while (true) {
        char cwd[MAX_PATH];
        if (_getcwd(cwd, MAX_PATH)) {
            cout << "\n" << cwd << " ";
        }

        input = get_input_with_features();
        if (input.empty()) continue;
        if (input == "exit") break;

        // Check for pipe or redirection
        if (input.find('|') != string::npos) {
            runPipedCommand(input);
            continue;
        }
        if (input.find('>') != string::npos || input.find('<') != string::npos) {
            runWithRedirection(input);
            continue;
        }

        args.clear();
        bool in_quotes = false;
        string current;
        for (char c : input) {
            if (c == '"') {
                in_quotes = !in_quotes;
                continue;
            }
            if (isspace(c) && !in_quotes) {
                if (!current.empty()) {
                    args.push_back(current);
                    current.clear();
                }
            } else {
                current += c;
            }
        }
        if (!current.empty()) args.push_back(current);

        execute_command(args);
    }

    return 0;
} 