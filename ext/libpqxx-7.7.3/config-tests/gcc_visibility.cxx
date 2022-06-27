// Test for gcc-style "visibility" attribute.
struct __attribute__((visibility("hidden"))) D
{
  D() {}
  int f() { return 0; }
};

int main()
{
  D d;
  return d.f();
}
