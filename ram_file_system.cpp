#include <algorithm>

#include "ram_file_system.hpp"

/*********************** Private Helper Functions ***************************/
std::string RamFileSystem::GetParentPath(const std::string& path) const
{
    const std::size_t slash_position = path.find_last_of('/');

    if(slash_position == 0)
    {
        return "/";
    }

    return path.substr(0, slash_position);
}  

bool RamFileSystem::IsValidPath(const std::string& path) const
{
    if(path.empty() || path[0] != '/')
    {
        return false;
    }

    if(path.size() > 1 && path.back() == '/')
    {
        return false;
    }

    if(path.find("//") != std::string::npos)
    {
        return false;
    }

    return true;
}

/*********************************** API Implementation ****************************************/

RamFileSystem::RamFileSystem(std::size_t capacity) : m_is_mounted(false) ,m_capacity(capacity),
 m_used_space(0), m_next_handle(1)
{
    m_directories.emplace("/");
}

bool RamFileSystem::Mount(const std::string& mount_point)
{
    if(IsMounted())
    {
        return false;
    }

    if(mount_point.empty())
    {
        return false;
    }

    m_mount_point = mount_point;
    m_is_mounted = true;

    return true;
}

bool RamFileSystem::Unmount()
{
    if(!IsMounted())
    {
        return false;
    }

    m_mount_point.clear();
    m_is_mounted = false;

    return true;
}

bool RamFileSystem::CreateFile(const std::string& path)
{
    if(!IsMounted()|| !IsValidPath(path))
    {
        return false;
    }

    if(m_files.find(path) != m_files.end())
    {
        return false;
    }

    const std::string parent_path = GetParentPath(path);

    if(!DirectoryExists(parent_path))
    {
        return false;
    }

    m_files.emplace(path, std::make_shared<File>());

    return true;
}

bool RamFileSystem::WriteFile(const std::string& path, const std::string& data)
{
    if(!IsMounted() || path.empty())
    {
        return false;
    }

    auto file_it = m_files.find(path);

    if(file_it == m_files.end())
    {
        return false;
    }

    const std::size_t old_size = file_it->second->data.size();
    const std::size_t new_used_space = m_used_space - old_size + data.size();

    if(new_used_space > m_capacity)
    {
        return false;
    }

    file_it->second->data = data;

    m_used_space = new_used_space;

    return true;
}

std::optional<std::string> RamFileSystem::ReadFile(const std::string& path) const
{   
    if(!IsMounted() || path.empty())
    {
        return std::nullopt;
    }

    auto file_it = m_files.find(path);

    if(file_it == m_files.end())
    {
        return std::nullopt;
    }

    return file_it->second->data;
}

bool RamFileSystem::DeleteFile(const std::string& path)
{
    if (!IsMounted() || path.empty())
    {
        return false;
    }

    auto file_it = m_files.find(path);

    if (file_it == m_files.end())
    {
        return false;
    }

    for (const auto& open_entry : m_open_files)
    {
        if (open_entry.second.file == file_it->second)
        {
            return false;
        }
    }

    m_used_space -= file_it->second->data.size();
    m_files.erase(file_it);

    return true;
}

bool RamFileSystem::IsMounted() const
{
    return m_is_mounted;
}

std::size_t RamFileSystem::GetUsedSpace() const
{
    return m_used_space;
}   

std::size_t RamFileSystem::GetAvailableSpace() const
{
    return m_capacity - m_used_space;
}

bool RamFileSystem::AppendFile(const std::string& path, const std::string& data)
{
    if(!IsMounted() || path.empty())
    {
        return false;
    }

    auto file_it = m_files.find(path);
    if(file_it == m_files.end())
    {
        return false;
    }

    if(data.size() > m_capacity - m_used_space)
    {
        return false;
    }
    
    file_it->second->data += data;

    m_used_space += data.size();

    return true;
}

bool RamFileSystem::FileExists(const std::string& path) const
{
    if(!IsMounted() || path.empty())
    {
        return false;
    }

    return m_files.find(path) != m_files.end();
}

std::optional<std::size_t> RamFileSystem::GetFileSize(const std::string& path) const
{
    if(!IsMounted() || path.empty())
    {
        return std::nullopt;
    }

    const auto file_it = m_files.find(path);
    if(file_it == m_files.end())
    {
        return std::nullopt;
    }

    return file_it->second->data.size();
}

bool RamFileSystem::RenameFile(const std::string& old_path, const std::string& new_path)
{
    if(!IsMounted() || !IsValidPath(old_path) || !IsValidPath(new_path))
    {
        return false;
    }

    auto old_path_it = m_files.find(old_path);
    if(old_path_it == m_files.end())
    {
        return false;
    }

    auto new_path_it = m_files.find(new_path);
    if(new_path_it != m_files.end())
    {
        return false;
    }

    if(!DirectoryExists(GetParentPath(new_path)))
    {
        return false;
    }

    auto file_node = m_files.extract(old_path_it);
    file_node.key() = new_path;
    m_files.insert(std::move(file_node));
    
    return true;
}


bool RamFileSystem::ClearFile(const std::string& path)
{
    if(!IsMounted() || path.empty())
    {
        return false;
    }

    auto file_it = m_files.find(path);
    if(file_it == m_files.end())
    {
        return false;
    }

    m_used_space -= file_it->second->data.size();
    file_it->second->data.clear();
    
    return true;
}

bool RamFileSystem::CreateDirectory(const std::string& path)
{
    if(!IsMounted() || !IsValidPath(path))
    {
        return false;
    }

    if(m_directories.find(path) != m_directories.end())
    {
        return false;
    }

    const std::string parent_path = GetParentPath(path);

    if(m_directories.find(parent_path) == m_directories.end())
    {
        return false;
    }

    m_directories.emplace(path);
    
    return true;
}

bool RamFileSystem::DirectoryExists(const std::string& path) const
{
    if(!IsMounted() || path.empty())
    {
        return false;
    }

    return m_directories.find(path) != m_directories.end();
}

bool RamFileSystem::DeleteDirectory(const std::string& path)
{
    if(!IsMounted() || path.empty() || path == "/")
    {
        return false;
    }

    auto path_iter = m_directories.find(path);
    if(path_iter == m_directories.end())
    {
        return false;
    }

    const std::string child_prefix = path + "/";

    for(const auto& file_entry : m_files)
    {
        if(file_entry.first.compare(0, child_prefix.size(), child_prefix) == 0)
        {
            return false;
        }
    }

    for(const auto& directory : m_directories)
    {
        if(directory.compare(0, child_prefix.size(), child_prefix) == 0)
        {
            return false;
        }
    }

    m_directories.erase(path_iter);

    return true;
}

std::optional<std::vector<RamFileSystem::DirectoryEntry>> RamFileSystem::ListDirectory(const std::string& path) const
{
    if(!IsMounted() || path.empty())
    {
        return std::nullopt;
    }

    if(!DirectoryExists(path))
    {
        return std::nullopt;
    }

    std::vector<DirectoryEntry> result;
    const std::string prefix = path == "/" ? "/" : path + "/";

    for(const auto& file_entry : m_files)
    {
        const std::string& file_path = file_entry.first;

        if(file_path.compare(0, prefix.size(), prefix) != 0)
        {
            continue;
        }

        const std::string remaining = file_path.substr(prefix.size());
        if(!remaining.empty() && remaining.find('/') == std::string::npos)
        {
            result.push_back({remaining, false});
        }
    }

    for(const auto& directory : m_directories)
    {
        if(directory.compare(0, prefix.size(), prefix) != 0)
        {
            continue;
        }

        const std::string remaining = directory.substr(prefix.size());
        if(!remaining.empty() && remaining.find('/') == std::string::npos)
        {
            result.push_back({remaining, true});
        }

    }
    std::sort(result.begin(), result.end(), [](const DirectoryEntry& lhs, const DirectoryEntry& rhs)    
    {
        if(lhs.is_directory != rhs.is_directory)
        {
            return lhs.is_directory > rhs.is_directory;
        }

        return lhs.name < rhs.name;
    });

    return result;
}

bool RamFileSystem::DeleteDirectoryRecursive(const std::string& path)
{
    if(!IsMounted() || path.empty() || path == "/")
    {
        return false;
    }

    const std::string prefix = path + "/";

    for(auto it = m_files.begin(); it != m_files.end(); )
    {
        if(it->first.compare(0, prefix.size(), prefix) == 0)
        {
            m_used_space -= it->second->data.size();
            it = m_files.erase(it);
        }
        else
        {
            ++it;
        }
    }

    for(auto it = m_directories.begin(); it != m_directories.end(); )
    {
        if(it->compare(0, prefix.size(), prefix) == 0)
        {
            it = m_directories.erase(it);
        }
        else
        {
            ++it;
        }
    }

    m_directories.erase(path);

    return true;
}

std::optional<FileHandle> RamFileSystem::OpenFile(const std::string& path, OpenMode mode)
{
    if(!IsMounted() || path.empty())
    {
        return std::nullopt;
    }

    auto file_it = m_files.find(path);
    if(file_it == m_files.end())
    {
        return std::nullopt;
    }

    const FileHandle handle = m_next_handle++;

    std::size_t initial_offset = 0;
    
    if(mode == OpenMode::Append)
    {
        initial_offset = file_it->second->data.size();
    }

    m_open_files.emplace(handle, OpenFileEntry{
        file_it->second,
        initial_offset,
        mode
    });

    return handle;
}

bool RamFileSystem::CloseFile(FileHandle handle)
{
    if(!IsMounted())
    {
        return false;
    }

    auto handle_it = m_open_files.find(handle);

    if(handle_it == m_open_files.end())
    {
        return false;
    }

    m_open_files.erase(handle_it);

    return true;
}

std::optional<std::string>
RamFileSystem::ReadOpenFile(FileHandle handle, std::size_t count)
{
    if (!IsMounted())
    {
        return std::nullopt;
    }

    const auto handle_it = m_open_files.find(handle);

    if (handle_it == m_open_files.end() || handle_it->second.mode == OpenMode::WriteOnly)
    {
        return std::nullopt;
    }

    const std::string& data = handle_it->second.file->data;
    const std::size_t offset = handle_it->second.offset;

    if(offset >= data.size())
    {
        return std::string{};
    }

    std::string result = data.substr(offset, count);
    handle_it->second.offset += result.size();

    return result;
}

bool RamFileSystem::WriteOpenFile(FileHandle handle, const std::string& data)
{
    if (!IsMounted())
    {
        return false;
    }

    auto handle_it = m_open_files.find(handle);

    if (handle_it == m_open_files.end() || handle_it->second.mode == OpenMode::ReadOnly)
    {
        return false;
    }

    std::shared_ptr<File>& file = handle_it->second.file;
    const std::size_t offset = handle_it->second.mode == OpenMode::Append ? file->data.size() : handle_it->second.offset;
    const std::size_t old_size = file->data.size();
    const std::size_t new_file_size = std::max(old_size, offset + data.size());
    const std::size_t used_without_file = m_used_space - old_size;

    if (new_file_size > m_capacity - used_without_file)
    {
        return false;
    }

    if (new_file_size > old_size)
    {
        file->data.resize(new_file_size);
    }

    file->data.replace(offset, data.size(), data);

    handle_it->second.offset = data.size();

    m_used_space = used_without_file + new_file_size;

    return true;
}

bool RamFileSystem::SeekFile(FileHandle handle, std::size_t offset)
{
    if(!IsMounted())
    {
        return false;
    }

    auto handle_it = m_open_files.find(handle);

    if(handle_it == m_open_files.end())
    {
        return false;
    }

    const std::size_t file_size = handle_it->second.file->data.size();

    if(offset > file_size)
    {
        return false;
    }

    handle_it->second.offset = offset;

    return true;
}

std::optional<std::size_t> RamFileSystem::GetFileOffset(FileHandle handle) const
{
    if(!IsMounted())
    {
        return std::nullopt;
    }

    const auto handle_it = m_open_files.find(handle);
    if(handle_it == m_open_files.end())
    {
        return std::nullopt;
    }

    return handle_it->second.offset;
}