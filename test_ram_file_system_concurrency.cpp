#include "ram_file_system.hpp"

#include <iostream>
#include <random>
#include <string>
#include <thread>
#include <vector>

int main()
{
    constexpr std::size_t kCapacity = 1'000'000;
    RamFileSystem fs(kCapacity);

    if(!fs.Mount("/ram"))
    {
        std::cerr << "mount failed\n";
        return 1;
    }

    constexpr int kNumDirs = 4;
    constexpr int kNumFiles = 5;

    for(int d = 0; d < kNumDirs; ++d)
    {
        fs.CreateDirectory("/dir" + std::to_string(d));
    }

    constexpr int kNumThreads = 8;
    constexpr int kIterationsPerThread = 5000;

    auto worker = [&](int thread_id)
    {
        std::mt19937 rng(thread_id * 7919u + 12345u);
        std::uniform_int_distribution<int> op_dist(0, 15);
        std::uniform_int_distribution<int> dir_dist(0, kNumDirs - 1);
        std::uniform_int_distribution<int> file_dist(0, kNumFiles - 1);

        for(int i = 0; i < kIterationsPerThread; ++i)
        {
            const int dir = dir_dist(rng);
            const int file_a = file_dist(rng);
            const int file_b = file_dist(rng);
            const std::string dir_path = "/dir" + std::to_string(dir);
            const std::string path_a = dir_path + "/file" + std::to_string(file_a) + ".txt";
            const std::string path_b = dir_path + "/file" + std::to_string(file_b) + ".txt";
            const std::string data = "thread" + std::to_string(thread_id) + "-iter" + std::to_string(i);

            switch(op_dist(rng))
            {
                case 0:
                    fs.CreateFile(path_a);
                    break;
                case 1:
                    fs.WriteFile(path_a, data);
                    break;
                case 2:
                {
                    auto result = fs.ReadFile(path_a);
                    (void)result;
                    break;
                }
                case 3:
                    fs.AppendFile(path_a, data);
                    break;
                case 4:
                    fs.DeleteFile(path_a);
                    break;
                case 5:
                {
                    auto result = fs.FileExists(path_a);
                    (void)result;
                    break;
                }
                case 6:
                {
                    auto result = fs.GetFileSize(path_a);
                    (void)result;
                    break;
                }
                case 7:
                    fs.RenameFile(path_a, path_b);
                    break;
                case 8:
                    fs.ClearFile(path_a);
                    break;
                case 9:
                {
                    auto result = fs.ListDirectory(dir_path);
                    (void)result;
                    break;
                }
                case 10:
                    fs.SetFileReadable(path_a, (i % 2) == 0);
                    break;
                case 11:
                    fs.SetFileWritable(path_a, (i % 3) != 0);
                    break;
                case 12:
                {
                    auto result = fs.GetFileMetadata(path_a);
                    (void)result;
                    break;
                }
                case 13:
                {
                    auto handle = fs.OpenFile(path_a, RamFileSystem::OpenMode::ReadWrite);
                    if(handle)
                    {
                        fs.WriteOpenFile(*handle, data);
                        fs.SeekFile(*handle, 0);
                        auto result = fs.ReadOpenFile(*handle, 4);
                        (void)result;
                        fs.CloseFile(*handle);
                    }
                    break;
                }
                case 14:
                {
                    auto used = fs.GetUsedSpace();
                    auto available = fs.GetAvailableSpace();
                    (void)used;
                    (void)available;
                    break;
                }
                default:
                {
                    auto result = fs.IsMounted();
                    (void)result;
                    break;
                }
            }
        }
    };

    std::vector<std::thread> threads;
    threads.reserve(kNumThreads);

    for(int t = 0; t < kNumThreads; ++t)
    {
        threads.emplace_back(worker, t);
    }

    for(auto& t : threads)
    {
        t.join();
    }

    std::cout << "Concurrency stress test completed without crashing. Used space: "
              << fs.GetUsedSpace() << "\n";

    return 0;
}
