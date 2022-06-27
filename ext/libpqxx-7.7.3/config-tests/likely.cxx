// Test for C++20 [[likely]] and [[unlikely]] attributes.

int main(int argc, char **)
{
#if __cplusplus < 202002L
  deliberately_fail(because, older, C++, standard);
#endif

  int x = 0;
  if (argc == 1) [[likely]]
    x = 0;
  else
    x = 1;
  return x;
}
