#include <iostream>
#include <string>

int main() {
  // Flush after every std::cout / std:cerr
  std::cout << std::unitbuf;
  std::cerr << std::unitbuf;

  bool terminate = true;

  while (terminate) {
	  std::string command;
	  std::cout << "$ ";
	  
	  if (!std::getline(std::cin, command)) break;

	  if (command == "exit") terminate = false;
	  
	  else if (command.find("echo ")) { 
		  std::string toPrint = command.substr(5);
		  std::cout << toPrint << "\n";
	  }
	  
	  else if (command == "echo") std::cout << "\n";
	  
	  else std::cout << command << ": command not found\n";
  }

}
