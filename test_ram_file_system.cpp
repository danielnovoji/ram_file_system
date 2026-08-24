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

    RamFileSystem fs2(1000);
    assert(fs2.Mount("/ram"));

    // Path canonicalization
    assert(fs2.CreateDirectory("/a"));
    assert(fs2.CreateFile("/a/f.txt"));
    assert(fs2.WriteFile("/a/f.txt", "hello"));
    assert(fs2.ReadFile("/a/./f.txt") == "hello");
    assert(fs2.ReadFile("/a/../a/f.txt") == "hello");
    assert(fs2.FileExists("/a//f.txt"));
    assert(!fs2.FileExists("/.."));
    assert(fs2.DirectoryExists("/a/.."));

    // CopyFile
    assert(fs2.CopyFile("/a/f.txt", "/a/f_copy.txt"));
    assert(fs2.ReadFile("/a/f_copy.txt") == "hello");
    assert(!fs2.CopyFile("/a/f.txt", "/a/f_copy.txt"));
    assert(!fs2.CopyFile("/a/missing.txt", "/a/x.txt"));
    assert(fs2.WriteFile("/a/f_copy.txt", "world"));
    assert(fs2.ReadFile("/a/f.txt") == "hello");

    // MoveDirectory
    assert(fs2.CreateDirectory("/a/sub"));
    assert(fs2.CreateFile("/a/sub/nested.txt"));
    assert(fs2.WriteFile("/a/sub/nested.txt", "nested-data"));
    assert(fs2.MoveDirectory("/a/sub", "/b"));
    assert(fs2.DirectoryExists("/b"));
    assert(!fs2.DirectoryExists("/a/sub"));
    assert(fs2.ReadFile("/b/nested.txt") == "nested-data");
    assert(fs2.CreateDirectory("/c"));
    assert(!fs2.MoveDirectory("/c", "/c/inner"));
    assert(!fs2.MoveDirectory("/", "/root2"));
    assert(fs2.CreateDirectory("/d"));
    assert(!fs2.MoveDirectory("/c", "/d"));

    std::cout << "All tests passed\n";

    return 0;
}