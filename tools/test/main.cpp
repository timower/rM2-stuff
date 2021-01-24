#include <Canvas.h>
#include <iostream>

int
main() {
  const auto print_char = [](char c) {
    std::string s(1, c);
    auto size = rmlib::Canvas::getTextSize(s, 32);
    std::cout << "Size of: " << c << " " << size << std::endl;
  };
  const auto print_str = [](std::string_view s) {
    auto size = rmlib::Canvas::getTextSize(s, 32);
    std::cout << "Size of: '" << s << "' " << size << std::endl;
  };

  print_char('a');
  print_char('A');
  print_char('.');
  print_char('!');
  print_char('^');
  print_str("　");
  print_char('7');

  return 0;
}
