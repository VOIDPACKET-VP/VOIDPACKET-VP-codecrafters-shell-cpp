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
	  std::cout << command << ": command not found";
  }

}
