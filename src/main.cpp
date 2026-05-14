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
	  std::cin >> command;
	  if (command == "exit") terminate = false;
	  else if (command.find("echo") == 0) { 
		  std::string toPrint = command.substr(4);
		  std::cout << toPrint << "\n";
	  }
	  else if (command == "echo") std::cout << "\n";
	  else std::cout << command << ": command not found\n";
  }

}
