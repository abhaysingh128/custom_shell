#include "shell.h"
#include <windows.h>
#include <iostream>
#include <vector>
#include <string>
#include <conio.h>
#include <algorithm>
#include <direct.h>
#include <io.h>

using namespace std;

extern vector<string> command_history;
extern string resolve_alias(const string& input);

int history_index = -1;

void autocomplete(string& input) {
    size_t last_space = input.find_last_of(" ");
    string prefix = (last_space == string::npos) ? input : input.substr(last_space + 1);
    string base = (last_space == string::npos) ? "" : input.substr(0, last_space + 1);

    vector<string> suggestions;

    // Use Windows API to list directory contents
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

    // Add built-in commands
    vector<string> commands = {
        "help", "cd", "pwd", "clear", "history", "ls", "ll", "mkdir",
        "touch", "rm", "cat", "cp", "mv", "time", "exit"
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
        if (ch == '\r') {  //Enter
            cout << endl;
            break;
        } else if (ch == '\b') {  //Backspace
            if (!input.empty()) {
                input.pop_back();
                cout << "\b \b";
            }
        } else if (ch == 0 || ch == -32) {  //Special key
            ch = _getch();  //Fetch actual key

            if (ch == 72) { //Up arrow
                if (!command_history.empty() && history_index > 0)
                    history_index--;
                else if (history_index == -1 && !command_history.empty())
                    history_index = (int)command_history.size() - 1;

                if (history_index >= 0 && history_index < (int)command_history.size()) {
                    input = command_history[history_index];
                    cout << "\r" << input << string(50, ' ');
                }
            } else if (ch == 80) { //Down arrow
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
        } else if (ch == '\t') { //Tab
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
