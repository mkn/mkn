#include <cstdlib>
#include <iostream>

int main() {
  char const* const v = std::getenv("MKN_SP");
  std::cout << "MKN_SP=" << (v ? v : "(unset)") << std::endl;
  return 0;
}
