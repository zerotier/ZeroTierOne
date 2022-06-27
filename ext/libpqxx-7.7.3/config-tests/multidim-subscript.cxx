// Test for multidimensional subscript operator support.
// Proposed for C++23: P2128R6.
struct table
{
  int width = 100;

  int operator[](int x, int y) const { return x + width * y; }
};


int main()
{
  return table{}[0, 0];
}
