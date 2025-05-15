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
#include "shell.h" 
using namespace std;

vector<string> command_history;


string wide_to_narrow(const wchar_t* wide) {
    std::wstring_convert<std::codecvt_utf8<wchar_t>> converter;
    return converter.to_bytes(wide);
}

void execute_command(const vector<string>& args);
void print_help();

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

    //Show file content (`cat`)
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

    //Copy file (`cp`)
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

    //Move file (`mv`)
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
         << "  mv <src> <dst>- Move (rename) file from src to dst\n";

}