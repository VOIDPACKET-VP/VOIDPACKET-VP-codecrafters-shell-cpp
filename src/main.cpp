#include <iostream>
#include <string>
#include <cstdlib> // for std::getenv
#include <filesystem> // for std::filesystem and std::path etc.
#include <sstream> // for std::istringstream
#include <vector>

// what is the preferred delimiter based on what's the preferred separator
constexpr char path_list_delimiter() {
	return (std::filesystem::path::preferred_separator == '\\') ? ';' : ':';
}


int main() {
  // Flush after every std::cout / std:cerr
  std::cout << std::unitbuf;
  std::cerr << std::unitbuf;

  bool terminate = true;

  while (terminate) {
	  std::string command;
	  std::cout << "$ ";

	  /// Get the paths (std::getenv)
	  const char* path_value = std::getenv("PATH");
	  
	  if (!std::getline(std::cin, command)) break;

	  if (command == "exit") terminate = false;
	  
	  else if (command.find("echo ") == 0) { 
		  std::string toPrint = command.substr(5);
		  std::cout << toPrint << "\n";
	  }
	  
	  else if (command == "echo") std::cout << "\n";
	  
	  else if (command.find("type ") == 0) {
		  std::string toFind = command.substr(5);
		  
		  if (toFind == "echo" || toFind == "exit" || toFind == "type") {
			  std::cout << toFind << " is a shell builtin" << "\n";
		  } else {
			  if (path_value) {
				  
				  /// Split them and append the wanted file at the 
				  std::istringstream stream(path_value);
				  std::string temp_part;
				  constexpr char delimiter = path_list_delimiter(); // we stored it to avoid repeatedly calling it in the loop
				  std::string pathToFile;

				  if constexpr (delimiter == ';') toFind += ".exe"; // windows executables must end with .exe

				  while (std::getline(stream, temp_part, delimiter)) {
					  std::filesystem::path full_path = std::filesystem::path(temp_part) / toFind;  // the / is not divide, it's an operator of std::filesystem::path : it automatically handles inserting the correct directory separator (\ on Windows, / on Linux)
					  
					  /// Use std::filesystem::exists()
					  if (std::filesystem::exists(path_str)) {
						  pathToFile = full_path.string();
						  break;
					  }
				  }

				  std::cout << toFind << " is " << pathToFile << "\n";
			  } 
			  else std::cout << toFind << ": not found" << "\n";
			  
		  }
	  }
	  
	  else std::cout << command << ": command not found\n";
  }

}
