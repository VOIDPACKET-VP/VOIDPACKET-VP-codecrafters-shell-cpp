#include <iostream>
#include <string>
#include <cstdlib> // for std::getenv
#include <filesystem> // for std::filesystem and std::path etc.
#include <sstream> // for std::istringstream
#include <vector>
#include <stdlib.h>

/// For Redirection section
#include <fstream> 
#include <cerrno>  // for errno
#include <cstring> // for std::strerror
#include <fcntl.h>    // For open, O_WRONLY, O_CREAT, O_TRUNC
#include <unistd.h>   // For dup2, close



// For the processes : we need to know how to spawn on, differes between OS
#if defined(_WIN32)
	#include <windows.h>
#else 
	#include <unistd.h>
	#include <sys/wait.h>
#endif


void spawnProcess(const std::string& pathToExe, const std::vector<std::string>& arguments) {
#if defined(_WIN32)
	STARTUPINFO si = { sizeof(si) };
	PROCESS_INFORMATION pi;

	// Combine them into a single command line string with quotes around the path : to avoid the case where the path has spaces
	std::string fullCommandLine = "\"" + pathToExe + "\"";

	for (size_t i = 1; i < arguments.size(); ++i) {
		fullCommandLine += " ";
		// If the argument contains spaces, wrap it in quotes so Windows programs parse it as one argument
		if (arguments[i].find(' ') != std::string::npos) {
			fullCommandLine += "\"" + arguments[i] + "\"";
		}
		else {
			fullCommandLine += arguments[i];
		}
	}

	std::vector<char> commandLineBuffer(fullCommandLine.begin(), fullCommandLine.end());
	commandLineBuffer.push_back('\0'); // Always ensure Win32 strings have a null terminator

	if (CreateProcessA(NULL, CommandLineBuffer.data(), NULL, NULL, FALSE, 0, NULL, NULL, &si, &pi)) { // fullCommandLine.data() because it has to be a pointer : char*
		// Tells our shell to wait till the program finishes then show that $ sign  
		WaitForSingleObject(pi.hProcess, INFINITE);

		CloseHandle(pi.hProcess);
		CloseHandle(pi.hThread);
	}

#else 
	// we need to convert to a vector of raw char* for execvp
	std::vector<char*> args;

	// We use const_cast because execv expects char* instead of const char*,
	// but it is safe because execv guarantees it won't mutate the data.
	for (auto& s : arguments) { args.push_back(const_cast<char*>(s.data())); } // .data() gives us the raw char* pointer
	args.push_back(nullptr); // execvp needs a NULL terminator

	pid_t pid = fork();
	if (pid == 0) {
		if (hasRedirection && !redirectLocation.empty()) {
			// 1. Fix the "No such file or directory" error by making parent directories
			//std::filesystem::path p(redirectLocation);
			//if (p.has_parent_path()) {
			//	std::filesystem::create_directories(p.parent_path());
			//}

			// Open the file (Write Only, Create if missing, Truncate/Wipe if exists)
			// Permissions: 0644 (Read/Write for owner, Read for others)
			int fd = open(redirectLocation.c_str(), O_WRONLY | O_CREAT | O_TRUNC, 0644);
			if (fd < 0) {
				std::perror("Failed to open redirection file");
				std::exit(1);
			}

			// Duplicate fd to STDOUT (Redirect standard output to our file descriptor)
			if (dup2(fd, STDOUT_FILENO) < 0) {
				std::perror("dup2 failed");
				std::exit(1);
			}

			// Close the original file descriptor descriptor since STDOUT now points to it
			close(fd);
		}

		execv(pathToExe.c_str(), args.data());
		std::perror("Exec failed");
		std::exit(1);
	}
	else if (pid > 0) wait(nullptr);
	else std::perror("Fork failed");
#endif
}


// what is the preferred delimiter based on what's the preferred separator
constexpr char path_list_delimiter() {
	return (std::filesystem::path::preferred_separator == '\\') ? ';' : ':';
}

std::filesystem::path find_executable(const std::string& command_name, const char* path_env) {
	/// Split those paths and append the wanted file at the end
	std::istringstream stream(path_env);
	std::string temp_part;
	constexpr char delimiter = path_list_delimiter(); // we stored it to avoid repeatedly calling it in the loop
	std::filesystem::path pathToFile;
	bool isWindows = false;

	std::string target = command_name; // because we declared it as a const in the function params
	if constexpr (delimiter == ';') {
		isWindows = true;
		target += ".exe"; // windows executables must end with .exe
	}
	while (std::getline(stream, temp_part, delimiter)) {
		std::filesystem::path full_path = std::filesystem::path(temp_part) / target;  // the / is not divide, it's an operator of std::filesystem::path : it automatically handles inserting the correct directory separator (\ on Windows, / on Linux)
		
		// we get the exact path to the executable
		if (std::filesystem::is_regular_file(full_path)) {
			if (isWindows) {
				pathToFile = full_path;
				break;
			} else {
				// For windows .exe is all we need, for linux and mac we need to check permission 
				std::filesystem::perms p = std::filesystem::status(full_path).permissions();
				bool isExecutable = ((p & std::filesystem::perms::owner_exec) != std::filesystem::perms::none) ||
									((p & std::filesystem::perms::group_exec) != std::filesystem::perms::none) ||
									((p & std::filesystem::perms::others_exec) != std::filesystem::perms::none);
				if (isExecutable) {
					pathToFile = full_path;
					break;
				}
			}
		}
	}
	return pathToFile;
}




// Get the HOME DIRECTORY based on the OS used
std::filesystem::path get_home_dir() {
	const char* homeDir = nullptr;
#if defined(_WIN32)
	homeDir = std::getenv("USERPROFILE");
	/// FOR FUTURE VOIDPACKET : Add a Fallback for older or specific Windows setups
#else 
	homeDir = std::getenv("HOME");
#endif
	if (homeDir) return std::filesystem::path(homeDir);
	
	// return an empty path if nothing is found
	return std::filesystem::path();
}


/// Quoting
std::vector<std::string> handlQuotes(std::string &toPrint) {
	std::vector<std::string> arguments;
	std::string currentArgument;
	bool insideSingleQuotes = false;
	bool insideDoubleQuotes = false;

	for (int i = 0; i < toPrint.length(); i++) {

		// if we are inside a single quote, double quotes are treated as normal text, and vice versa.
		if (insideSingleQuotes) {
			if (toPrint[i] == '\'') insideSingleQuotes = false;
			else currentArgument += toPrint[i];
		}
		else if (insideDoubleQuotes) {
			if (toPrint[i] == '"') insideDoubleQuotes = false;

			else if (toPrint[i] == '\\' && i + 1 < toPrint.length() && (toPrint[i + 1] == '"' || toPrint[i + 1] == '\\')) {
				currentArgument += toPrint[i + 1];
				i++; // Skip the escaped character
			}
			else currentArgument += toPrint[i];
		}
		else if (toPrint[i] == '\\') {
			// Check bounds BEFORE looking ahead to prevent memory crashes
			if (i + 1 < toPrint.length()) {
				currentArgument += toPrint[i + 1];
				i++; // So that we pass the next character (the escaped one)
			}
		}
		else {
			if (toPrint[i] == '\'') insideSingleQuotes = true;
			else if (toPrint[i] == '"') insideDoubleQuotes = true;
			else if (toPrint[i] == ' ') {
				if (!currentArgument.empty()) {
					arguments.push_back(currentArgument);
					currentArgument.clear();
				}
			}
			else currentArgument += toPrint[i];
		}
	}
	if (!currentArgument.empty()) arguments.push_back(currentArgument);
	return arguments;
}

/// Redirection
void redirect_output(const std::string& fileName, const std::string& text) {
	std::ofstream outFile(fileName, std::ios::out);

	if (outFile.is_open()) {
		outFile << text << "\n";
		outFile.close();
	}
	else {
		std::cerr << "shell: " << fileName << ": " << std::strerror(errno) << std::endl;
	}
}



int main() {
  // Flush after every std::cout / std:cerr
  std::cout << std::unitbuf;
  std::cerr << std::unitbuf;

  bool terminate = true;

  while (terminate) {
	  std::string command;
	  std::cout << "$ ";

	  std::filesystem::path homeDir = get_home_dir();

	  /// Get the paths (std::getenv)
	  const char* path_value = std::getenv("PATH");
	  
	  if (!std::getline(std::cin, command)) break;
	  
	  // exit command
	  if (command == "exit") terminate = false;


	  /// Finding the ">" operator
	  std::string redirectLocation = "";
	  bool hasRedirection = false;
	  size_t redirPos = command.find('>');
	  if (redirPos != std::string::npos) {
		  hasRedirection = true;
		  std::string filePart = command.substr(redirPos + 1);
		  command = command.substr(0, redirPos); // Keep only the command part

		  // Clean up spaces from the filename
		  size_t firstNonSpace = filePart.find_first_not_of(" \t");
		  size_t lastNonSpace = filePart.find_last_not_of(" \t\r\n");
		  if (firstNonSpace != std::string::npos && lastNonSpace != std::string::npos) {
			  redirectLocation = filePart.substr(firstNonSpace, lastNonSpace - firstNonSpace + 1);
		  }
	  }


	  // echo command
	  else if (command.find("echo ") == 0) {
		  std::string toPrint = command.substr(5);
		  std::vector<std::string> arguments = handlQuotes(toPrint);
		  for (size_t i = 0; i < arguments.size(); i++) {
			  std::cout << arguments[i];
			  if (i < arguments.size() - 1) {
				  std::cout << " ";
			  }
		  }
		  std::cout << "\n";
	  }

	  // type command
	  else if (command.find("type ") == 0) {
		  std::string toFind = command.substr(5);

		  if (toFind == "echo" || toFind == "exit" || toFind == "type" || toFind == "pwd") {
			  std::cout << toFind << " is a shell builtin" << "\n";
		  }
		  else {
			  if (path_value) {
				  std::string pathToExecutable = find_executable(toFind, path_value).string();
				  if (!pathToExecutable.empty()) std::cout << toFind << " is " << pathToExecutable << "\n";
				  else std::cout << toFind << ": not found" << "\n";
			  }
		  }
	  }

	  // pwd command
	  else if (command == "pwd") {
		  std::filesystem::path cwd_object = std::filesystem::current_path();
		  std::string cwd_string = cwd_object.string();
		  std::cout << cwd_string << "\n";
	  }

	  // cd command
	  else if (command.find("cd ") == 0) {
		  std::string toGoTo = command.substr(3);

		  // Make it a path
		  std::filesystem::path newDir = toGoTo;

		  // HOME Dir
		  if (toGoTo == "~") {
			  std::filesystem::current_path(homeDir);
		  } else { 
			  // check if it exists
			  bool newDirExists = std::filesystem::is_directory(newDir);

			  // execute the cd command
			  if (newDirExists) std::filesystem::current_path(toGoTo);
			  else std::cout << "cd: " << toGoTo << ": No such file or directory\n";
		  }
	  }

	  // External program option
	  else {
		  std::vector<std::string> args = handlQuotes(command);
		  
		  if (!args.empty()) {
			  std::string exe_name = args[0];

			  // check if it exists
			  std::string pathToExecutable = find_executable(exe_name, path_value).string();

			  if (!pathToExecutable.empty()) {
				  spawnProcess(pathToExecutable, args);
			  }
			  else {
				  std::cout << exe_name << ": command not found\n";
			  }
		  }

	  }
  
  }

}
