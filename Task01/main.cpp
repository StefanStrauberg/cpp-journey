#include <iostream>
#include <print>

int main() {
    int age;
    std::cout << "Hell, old are oyu? ";

    if (std::cin >> age) {
        std::println("After 10 years you will be {} years old.", age + 10);
    } else {
        std::cout << "You entered not a number." << std::endl;
    }

    return 0;
}