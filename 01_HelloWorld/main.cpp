#include <iostream>
#include <string>
#include <format>

int main() {
  std::string greeting;
  std::string name;
  int age;
  
  std::cout << "Hello, enter your name: "; 
  std::getline(std::cin, name);
  
  std::cout << "Enter your age: "; 
  if(!(std::cin >> age)) {
    std::cerr << "Error: a number was expected.\n";
    return 1;
  } else if (age < 0 || age > 150)
  {
    std::cerr << "Error: a number should be greater than 0 and less than 150.\n";
    return 1;
  }


  if (age < 18) {
    greeting = std::format("Hello young {}! You are {}.", name, age);
  } else if (age < 65) {
    greeting = std::format("Good day, {}! You are {} years old.", name, age);
  } else {
    greeting = std::format("Greetings, {}! You are wise at {} years old.", name, age);
  }

  std::cout << greeting << '\n';

  return 0;
}