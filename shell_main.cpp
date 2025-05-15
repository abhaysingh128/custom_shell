#include "shell.h"
#include <iostream>
#include <string>
#include <vector>
#include <windows.h>
#include <direct.h>
#include <sstream>

using namespace std;

extern void init_signals();
extern void execute_command(const vector<string>& args);
extern string get_input_with_features();

int main() {
    string input;
    vector<string> args;

    init_signals();

    cout << "Custom Shell (type 'help' for commands)\n";

    while (true) {
        // Display custom prompt
        cout << "\nsoul_shell> ";

        // Get user input with autocomplete/history support
        input = get_input_with_features();
        if (input.empty()) continue;

        // Exit command
        if (input == "exit") break;

        // Tokenize input into arguments
        args.clear();
        stringstream ss(input);
        string token;
        while (ss >> token) args.push_back(token);

        // Execute the command
        execute_command(args);
    }

    return 0;
}
