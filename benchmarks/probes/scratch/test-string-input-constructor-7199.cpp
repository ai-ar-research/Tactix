#include <cassert>
#include <cstring>
#include <iterator>
#include <new>
#include <sstream>
#include <string>
int main(int argc, char **) {
  alignas(std::string) unsigned char storage[sizeof(std::string)];
  std::memset(storage, 0xff, sizeof storage);
  std::istringstream stream("012345678901234567890123456789");
  if (argc == 1) {
    auto *text = new (storage) std::string(std::istreambuf_iterator<char>(stream), {});
    assert(text->size() == 30);
    text->~basic_string();
  } else {
    auto *text = new (storage) std::string;
    for (char byte; stream.get(byte);) text->push_back(byte);
    assert(*text == "012345678901234567890123456789");
    text->~basic_string();
  }
}
