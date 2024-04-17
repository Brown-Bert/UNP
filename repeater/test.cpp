#include <cstdio>
#include <cstring>
#include <iostream>

void test(char *s) {
  s[0] = '6';
  std::cout << s[2] << std::endl;
  return;
}

int main() {
  char buf[4] = {'1', '\0', '2', '\0'};
  test(buf);
  return 0;
}