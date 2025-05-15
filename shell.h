#ifndef SHELL_H
#define SHELL_H

#include <string>
#include <vector>
#include <csignal>

void init_signals();
bool handle_alias_command(const std::vector<std::string>& args);
void execute_command(const std::vector<std::string>& args);
std::string get_input_with_features();
std::string resolve_alias(const std::string& input);

//external flag to detect Ctrl+C interrupt
extern volatile sig_atomic_t interrupted;

#endif
