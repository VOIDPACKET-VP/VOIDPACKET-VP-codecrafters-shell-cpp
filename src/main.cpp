#include <iostream>
#include <string>
#include <cstdlib> // for std::getenv
#include <filesystem> // for std::filesystem and std::path etc.
#include <sstream> // for std::istringstream
#include <vector>
#include <stdlib.h>

// For the processes : we need to know how to spawn on, differes between OS
#if defined(_WIN32)
	#include <windows.h>
#else 
	#include <unistd.h>
	#include <sys/wait.h>
#endif



void spawnProcess(const std::string& pathToExe, std::string& commandName) {
#if defined(_WIN32)
	STARTUPINFO si = { sizeof(si) };
	PROCESS_INFORMATION pi;

	// Combine them into a single command line string with quotes around the path : to avoid the case where the path has spaces
	std::string fullCommandPath = "\"" + pathToExe + "\" " + commandName;

	if (CreateProcessA(NULL, fullCommandPath.data(), NULL, NULL, FALSE, 0, NULL, NULL, &si, &pi)) { // fullCommandLine.data() because it has to be a pointer : char*
		// Tells our shell to wait till the program finishes then show that $ sign  
		WaitForSingleObject(pi.hProcess, INFINITE);

		CloseHandle(pi.hProcess);
		CloseHandle(pi.hThread);
	}

#else 
	// In POSIX we have to split (tokenize) the string by spaces into an array of strings before passing it to execvp.
	// We can do that by using good old std::istringstream
	std::vector<std::string> tokens;
	std::istringstream ss(commandName);
	std::string temp;

	while (ss >> temp) tokens.push_back(temp);

	// we need to convert to a vector of raw char* for execvp
	std::vector<char*> args;
	for (auto& s : tokens) { args.push_back(s.data()); } // .data() gives us the raw char* pointer
	args.push_back(nullptr); // execvp needs a NULL terminator

	pid_t pid = fork();
	if (pid == 0) {
		execv(pathToExe.c_str(), args.data()); // pathToExe.c_str() is the program name (added c_str() because it expects a char*) , args.data() converts the vector into a raw array pointer (char**)

		std::perror("Exec failed");
		std::exit(1);
	} else if (pid > 0) {
		wait(nullptr);
	} else {
		std::perror("Fork failed");
	}
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
		target += ".exe"; // windows executables must end with .exe
		isWindows = true;
	}
	while (std::getline(stream, temp_part, delimiter)) {
		std::filesystem::path full_path = std::filesystem::path(temp_part) / target;  // the / is not divide, it's an operator of std::filesystem::path : it automatically handles inserting the correct directory separator (\ on Windows, / on Linux)
		
		// we get the exact path to the executable
		if (std::filesystem::exists(full_path)) {
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




int main() {
  // Flush after every std::cout / std:cerr
  std::cout << std::unitbuf;
  std::cerr << std::unitbuf;

  bool terminate = true;


  // HOME dir path for the cd command
  std::filesystem::path homeDir = std::filesystem::current_path();


  while (terminate) {
	  std::string command;
	  std::cout << "$ ";

	  /// Get the paths (std::getenv)
	  const char* path_value = std::getenv("PATH");
	  
	  if (!std::getline(std::cin, command)) break;
	  
	  // exit command
	  if (command == "exit") terminate = false;

	  // echo command
	  else if (command.find("echo ") == 0) {
		  std::string toPrint = command.substr(5);
		  if (!toPrint.empty()) std::cout << toPrint << "\n";
		  else std::cout << "\n";
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

		  //
		  if (toGoTo == "~") {
			  std::filesystem::current_path(homeDir);
		  }
		  else {
			  // check if it exists
			  bool newDirExists = std::filesystem::is_directory(newDir);

			  // execute the cd command
			  if (newDirExists) std::filesystem::current_path(toGoTo);
			  else std::cout << "cd: " << toGoTo << ": No such file or directory\n";
		  }
	  }

	  // External program option
	  else {
		  // Get the first word of the command : which is the executable
		  std::istringstream iss(command);
		  std::string exe_name;
		  iss >> exe_name;
		  
		  // check if it exists
		  std::string pathToExecutable = find_executable(exe_name, path_value).string();

		  if (!pathToExecutable.empty()) {
			  spawnProcess(pathToExecutable, command);
		  } else {
			  std::cout << exe_name << ": command not found\n";
		  }

	  }
  
  }

}
