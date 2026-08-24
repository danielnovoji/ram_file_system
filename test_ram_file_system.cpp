#include "ram_file_system.hpp"

#include <cassert>
#include <iostream>

int main()
{
    RamFileSystem fs(10);

    assert(!fs.IsMounted());
    assert(!fs.CreateFile("/a.txt"));

    assert(fs.Mount("/ram"));
    assert(!fs.Mount("/other"));

    assert(fs.CreateFile("/a.txt"));
    assert(!fs.CreateFile("/a.txt"));

    assert(fs.WriteFile("/a.txt", "hello"));
    assert(fs.GetUsedSpace() == 5);
    assert(fs.GetAvailableSpace() == 5);
    assert(fs.ReadFile("/a.txt") == "hello");

    assert(fs.WriteFile("/a.txt", "12345678"));
    assert(fs.GetUsedSpace() == 8);

    assert(!fs.WriteFile("/a.txt", "12345678901"));
    assert(fs.ReadFile("/a.txt") == "12345678");
    assert(fs.GetUsedSpace() == 8);

    assert(fs.DeleteFile("/a.txt"));
    assert(fs.GetUsedSpace() == 0);
    assert(!fs.DeleteFile("/a.txt"));

    assert(fs.Unmount());
    assert(!fs.Unmount());

    std::cout << "All tests passed\n";

    return 0;
}