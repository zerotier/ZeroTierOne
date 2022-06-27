// Test for gcc-style "pure" attribute.
int __attribute__((pure)) f()
{
  return 0;
}

int main()
{
  return f();
}
