#ifndef RAM_FILE_SYSTEM_HPP
#define RAM_FILE_SYSTEM_HPP

#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>
#include <optional>
#include <memory>
#include <ctime>
#include <mutex>

using FileHandle = std::size_t;

class RamFileSystem
{
    public:
        struct DirectoryEntry
            {
                std::string name;
                bool is_directory;
            };

        struct FileMetadata
        {
            std::size_t size;
            bool readable;
            bool writable;
            std::time_t creation_time;
            std::time_t modification_time;
        };
        

        enum class OpenMode
        {
            ReadOnly,
            WriteOnly,
            ReadWrite,
            Append
        }; 
        explicit RamFileSystem(std::size_t capacity);

        bool Mount(const std::string& mount_point);
        bool Unmount();

        bool CreateFile(const std::string& path);
        bool WriteFile(const std::string& path, const std::string& data);
        std::optional<std::string> ReadFile(const std::string& path) const;
        bool DeleteFile(const std::string& path);

        bool IsMounted() const;
        std::size_t GetUsedSpace() const;
        std::size_t GetAvailableSpace() const;
        
        /* File Functions */
        bool AppendFile(const std::string& path, const std::string& data);
        bool FileExists(const std::string& path) const;
        std::optional<std::size_t> GetFileSize(const std::string& path) const;
        bool RenameFile(const std::string& old_path, const std::string& new_path);
        bool ClearFile(const std::string& path);
        std::optional<FileHandle> OpenFile(const std::string& path, OpenMode mode);
        bool CloseFile(FileHandle handle);
        std::optional<std::string> ReadOpenFile(FileHandle handle, std::size_t count);
        bool WriteOpenFile(FileHandle handle, const std::string& data);
        
        /* File Position Functions */
        bool SeekFile(FileHandle handle, std::size_t offset);
        std::optional<std::size_t> GetFileOffset(FileHandle handle) const;

        /* Directory Functions */
        bool CreateDirectory(const std::string& path);
        bool DirectoryExists(const std::string& path) const;
        bool DeleteDirectory(const std::string& path);
        bool DeleteDirectoryRecursive(const std::string& path);
        std::optional<std::vector<DirectoryEntry>> ListDirectory(const std::string& path) const;

        /* File Permission Functions */
        bool SetFileReadable(const std::string& path, bool readable);
        bool SetFileWritable(const std::string& path, bool writable);

        std::optional<bool> IsFileReadable(const std::string& path) const;
        std::optional<bool> IsFileWritable(const std::string& path) const;

        /* File Metadata Functions */
        std::optional<FileMetadata> GetFileMetadata(const std::string& path) const;

    private:
        struct File
        {
            std::string data;

            bool readable = true;
            bool writable = true;
            std::time_t creation_time;
            std::time_t modification_time;
        };
        
        struct OpenFileEntry
        {
            std::shared_ptr<File> file;
            std::size_t offset;
            OpenMode mode;
        }; 

        bool m_is_mounted;
        std::size_t m_capacity;
        std::size_t m_used_space;
        std::string m_mount_point;
        
        std::unordered_map<std::string, std::shared_ptr<File>> m_files;
        std::unordered_set<std::string> m_directories;
        std::unordered_map<FileHandle, OpenFileEntry> m_open_files;
        FileHandle m_next_handle;

        mutable std::mutex m_mutex; // Mutex for thread safety

        /* Private Helper Functions */
        std::string GetParentPath(const std::string& path) const;
        bool IsValidPath(const std::string& path) const;
        bool IsFileOpen(const std::shared_ptr<File>& file) const;
};


#endif // RAM_FILE_SYSTEM_HPP