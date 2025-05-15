#include "shell.h"
#include <iostream>
#include <map>
#include <sstream>
#include <csignal>
#include <cstdlib>
#include <vector>
#include <string>

using namespace std;

map<string, string> alias_map;
extern vector<string> command_history;

//Declare interruption flag
volatile sig_atomic_t interrupted = 0;

void signal_handler(int signal) {
    if (signal == SIGINT) {
        interrupted = 1;
        cout << "\nInterrupt received (Ctrl+C)\n";
    }
}

void init_signals() {
    signal(SIGINT, signal_handler);
}

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
