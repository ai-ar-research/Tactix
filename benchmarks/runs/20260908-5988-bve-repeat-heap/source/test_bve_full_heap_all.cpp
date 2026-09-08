// Exercise repeated-pop selection through the same real fixed API fixture.
#define main first_pop_fixture_main
#include "test_bve_full_heap.cpp"
#undef main
int main() { return first_pop_fixture_main(2, nullptr); }
