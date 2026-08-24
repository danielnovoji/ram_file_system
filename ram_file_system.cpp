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

std::optional<std::string> RamFileSystem::CanonicalizePath(const std::string& path) const
{
    if(path.empty() || path[0] != '/')
    {
        return std::nullopt;
    }

    std::vector<std::string> segments;
    std::size_t i = 1;

    while(i <= path.size())
    {
        std::size_t next_slash = path.find('/', i);

        if(next_slash == std::string::npos)
        {
            next_slash = path.size();
        }

        const std::string segment = path.substr(i, next_slash - i);

        if(segment.empty() || segment == ".")
        {
            // no-op: repeated slashes and "." both refer to the current directory
        }
        else if(segment == "..")
        {
            if(segments.empty())
            {
                return std::nullopt;
            }

            segments.pop_back();
        }
        else
        {
            segments.push_back(segment);
        }

        i = next_slash + 1;
    }

    if(segments.empty())
    {
        return std::string{"/"};
    }

    std::string result;

    for(const auto& segment : segments)
    {
        result += "/";
        result += segment;
    }

    return result;
}

bool RamFileSystem::IsFileOpen(const std::shared_ptr<File>& file) const
{
    for(const auto& open_entry : m_open_files)
    {
        if(open_entry.second.file == file)
        {
            return true;
        }
    }

    return false;
}

/*************** Unlocked Implementations (caller must hold m_mutex) ****************/

bool RamFileSystem::IsMountedUnlocked() const
{
    return m_is_mounted;
}

bool RamFileSystem::MountUnlocked(const std::string& mount_point)
{
    if(IsMountedUnlocked())
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

bool RamFileSystem::UnmountUnlocked()
{
    if(!IsMountedUnlocked())
    {
        return false;
    }

    m_mount_point.clear();
    m_is_mounted = false;

    return true;
}

bool RamFileSystem::CreateFileUnlocked(const std::string& path)
{
    if(!IsMountedUnlocked())
    {
        return false;
    }

    const auto canonical_path = CanonicalizePath(path);
    if(!canonical_path)
    {
        return false;
    }

    if(m_files.find(*canonical_path) != m_files.end())
    {
        return false;
    }

    const std::string parent_path = GetParentPath(*canonical_path);

    if(!DirectoryExistsUnlocked(parent_path))
    {
        return false;
    }

    const std::time_t now = std::time(nullptr);
    auto file = std::make_shared<File>();

    file->creation_time = now;
    file->modification_time = now;
    m_files.emplace(*canonical_path, file);

    return true;
}

bool RamFileSystem::WriteFileUnlocked(const std::string& path, const std::string& data)
{
    if(!IsMountedUnlocked())
    {
        return false;
    }

    const auto canonical_path = CanonicalizePath(path);
    if(!canonical_path)
    {
        return false;
    }

    auto file_it = m_files.find(*canonical_path);

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
    file_it->second->modification_time = std::time(nullptr);
    m_used_space = new_used_space;

    return true;
}

std::optional<std::string> RamFileSystem::ReadFileUnlocked(const std::string& path) const
{
    if(!IsMountedUnlocked())
    {
        return std::nullopt;
    }

    const auto canonical_path = CanonicalizePath(path);
    if(!canonical_path)
    {
        return std::nullopt;
    }

    auto file_it = m_files.find(*canonical_path);

    if(file_it == m_files.end())
    {
        return std::nullopt;
    }

    return file_it->second->data;
}

bool RamFileSystem::DeleteFileUnlocked(const std::string& path)
{
    if (!IsMountedUnlocked())
    {
        return false;
    }

    const auto canonical_path = CanonicalizePath(path);
    if(!canonical_path)
    {
        return false;
    }

    auto file_it = m_files.find(*canonical_path);

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

std::size_t RamFileSystem::GetUsedSpaceUnlocked() const
{
    return m_used_space;
}

std::size_t RamFileSystem::GetAvailableSpaceUnlocked() const
{
    return m_capacity - m_used_space;
}

bool RamFileSystem::AppendFileUnlocked(const std::string& path, const std::string& data)
{
    if(!IsMountedUnlocked())
    {
        return false;
    }

    const auto canonical_path = CanonicalizePath(path);
    if(!canonical_path)
    {
        return false;
    }

    auto file_it = m_files.find(*canonical_path);
    if(file_it == m_files.end())
    {
        return false;
    }

    if(data.size() > m_capacity - m_used_space)
    {
        return false;
    }

    file_it->second->data += data;
    file_it->second->modification_time = std::time(nullptr);
    m_used_space += data.size();

    return true;
}

bool RamFileSystem::FileExistsUnlocked(const std::string& path) const
{
    if(!IsMountedUnlocked())
    {
        return false;
    }

    const auto canonical_path = CanonicalizePath(path);
    if(!canonical_path)
    {
        return false;
    }

    return m_files.find(*canonical_path) != m_files.end();
}

std::optional<std::size_t> RamFileSystem::GetFileSizeUnlocked(const std::string& path) const
{
    if(!IsMountedUnlocked())
    {
        return std::nullopt;
    }

    const auto canonical_path = CanonicalizePath(path);
    if(!canonical_path)
    {
        return std::nullopt;
    }

    const auto file_it = m_files.find(*canonical_path);
    if(file_it == m_files.end())
    {
        return std::nullopt;
    }

    return file_it->second->data.size();
}

bool RamFileSystem::RenameFileUnlocked(const std::string& old_path, const std::string& new_path)
{
    if(!IsMountedUnlocked())
    {
        return false;
    }

    const auto old_canonical = CanonicalizePath(old_path);
    const auto new_canonical = CanonicalizePath(new_path);

    if(!old_canonical || !new_canonical)
    {
        return false;
    }

    auto old_path_it = m_files.find(*old_canonical);
    if(old_path_it == m_files.end())
    {
        return false;
    }

    auto new_path_it = m_files.find(*new_canonical);
    if(new_path_it != m_files.end())
    {
        return false;
    }

    if(!DirectoryExistsUnlocked(GetParentPath(*new_canonical)))
    {
        return false;
    }

    auto file_node = m_files.extract(old_path_it);
    file_node.key() = *new_canonical;
    m_files.insert(std::move(file_node));

    return true;
}

bool RamFileSystem::CopyFileUnlocked(const std::string& source_path, const std::string& destination_path)
{
    if(!IsMountedUnlocked())
    {
        return false;
    }

    const auto source_canonical = CanonicalizePath(source_path);
    const auto destination_canonical = CanonicalizePath(destination_path);

    if(!source_canonical || !destination_canonical)
    {
        return false;
    }

    auto source_it = m_files.find(*source_canonical);
    if(source_it == m_files.end())
    {
        return false;
    }

    if(m_files.find(*destination_canonical) != m_files.end())
    {
        return false;
    }

    if(!DirectoryExistsUnlocked(GetParentPath(*destination_canonical)))
    {
        return false;
    }

    const std::size_t new_used_space = m_used_space + source_it->second->data.size();

    if(new_used_space > m_capacity)
    {
        return false;
    }

    auto new_file = std::make_shared<File>();
    const std::time_t now = std::time(nullptr);

    new_file->data = source_it->second->data;
    new_file->readable = source_it->second->readable;
    new_file->writable = source_it->second->writable;
    new_file->creation_time = now;
    new_file->modification_time = now;

    m_files.emplace(*destination_canonical, new_file);
    m_used_space = new_used_space;

    return true;
}

bool RamFileSystem::ClearFileUnlocked(const std::string& path)
{
    if(!IsMountedUnlocked())
    {
        return false;
    }

    const auto canonical_path = CanonicalizePath(path);
    if(!canonical_path)
    {
        return false;
    }

    auto file_it = m_files.find(*canonical_path);
    if(file_it == m_files.end())
    {
        return false;
    }

    m_used_space -= file_it->second->data.size();
    file_it->second->data.clear();
    file_it->second->modification_time = std::time(nullptr);

    return true;
}

bool RamFileSystem::CreateDirectoryUnlocked(const std::string& path)
{
    if(!IsMountedUnlocked())
    {
        return false;
    }

    const auto canonical_path = CanonicalizePath(path);
    if(!canonical_path)
    {
        return false;
    }

    if(m_directories.find(*canonical_path) != m_directories.end())
    {
        return false;
    }

    const std::string parent_path = GetParentPath(*canonical_path);

    if(m_directories.find(parent_path) == m_directories.end())
    {
        return false;
    }

    m_directories.emplace(*canonical_path);

    return true;
}

bool RamFileSystem::DirectoryExistsUnlocked(const std::string& path) const
{
    if(!IsMountedUnlocked())
    {
        return false;
    }

    const auto canonical_path = CanonicalizePath(path);
    if(!canonical_path)
    {
        return false;
    }

    return m_directories.find(*canonical_path) != m_directories.end();
}

bool RamFileSystem::DeleteDirectoryUnlocked(const std::string& path)
{
    if(!IsMountedUnlocked())
    {
        return false;
    }

    const auto canonical_path = CanonicalizePath(path);
    if(!canonical_path || *canonical_path == "/")
    {
        return false;
    }

    auto path_iter = m_directories.find(*canonical_path);
    if(path_iter == m_directories.end())
    {
        return false;
    }

    const std::string child_prefix = *canonical_path + "/";

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

std::optional<std::vector<RamFileSystem::DirectoryEntry>> RamFileSystem::ListDirectoryUnlocked(const std::string& path) const
{
    if(!IsMountedUnlocked())
    {
        return std::nullopt;
    }

    const auto canonical_path = CanonicalizePath(path);
    if(!canonical_path)
    {
        return std::nullopt;
    }

    if(!DirectoryExistsUnlocked(*canonical_path))
    {
        return std::nullopt;
    }

    std::vector<DirectoryEntry> result;
    const std::string prefix = *canonical_path == "/" ? "/" : *canonical_path + "/";

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

bool RamFileSystem::DeleteDirectoryRecursiveUnlocked(const std::string& path)
{
    if(!IsMountedUnlocked())
    {
        return false;
    }

    const auto canonical_path = CanonicalizePath(path);
    if(!canonical_path || *canonical_path == "/")
    {
        return false;
    }

    if (!DirectoryExistsUnlocked(*canonical_path))
    {
        return false;
    }

    const std::string prefix = *canonical_path + "/";

    for (const auto& file_entry : m_files)
    {
        if (file_entry.first.compare(0, prefix.size(), prefix) == 0)
        {
            if (IsFileOpen(file_entry.second))
            {
                return false;
            }
        }
    }

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

    m_directories.erase(*canonical_path);

    return true;
}

bool RamFileSystem::MoveDirectoryUnlocked(const std::string& old_path, const std::string& new_path)
{
    if(!IsMountedUnlocked())
    {
        return false;
    }

    const auto old_canonical = CanonicalizePath(old_path);
    const auto new_canonical = CanonicalizePath(new_path);

    if(!old_canonical || !new_canonical || *old_canonical == "/")
    {
        return false;
    }

    const std::string& old_dir = *old_canonical;
    const std::string& new_dir = *new_canonical;

    if(m_directories.find(old_dir) == m_directories.end())
    {
        return false;
    }

    if(m_directories.find(new_dir) != m_directories.end() || m_files.find(new_dir) != m_files.end())
    {
        return false;
    }

    const std::string old_prefix = old_dir + "/";

    if(new_dir.compare(0, old_prefix.size(), old_prefix) == 0)
    {
        return false;
    }

    if(!DirectoryExistsUnlocked(GetParentPath(new_dir)))
    {
        return false;
    }

    std::vector<std::string> directories_to_move;
    for(const auto& directory : m_directories)
    {
        if(directory == old_dir || directory.compare(0, old_prefix.size(), old_prefix) == 0)
        {
            directories_to_move.push_back(directory);
        }
    }

    std::vector<std::string> files_to_move;
    for(const auto& file_entry : m_files)
    {
        if(file_entry.first.compare(0, old_prefix.size(), old_prefix) == 0)
        {
            files_to_move.push_back(file_entry.first);
        }
    }

    const std::string new_prefix = new_dir + "/";

    for(const auto& directory : directories_to_move)
    {
        const std::string relocated = directory == old_dir ? new_dir : new_prefix + directory.substr(old_prefix.size());
        if(m_directories.find(relocated) != m_directories.end())
        {
            return false;
        }
    }

    for(const auto& file_path : files_to_move)
    {
        const std::string relocated = new_prefix + file_path.substr(old_prefix.size());
        if(m_files.find(relocated) != m_files.end())
        {
            return false;
        }
    }

    for(const auto& directory : directories_to_move)
    {
        const std::string relocated = directory == old_dir ? new_dir : new_prefix + directory.substr(old_prefix.size());
        m_directories.erase(directory);
        m_directories.emplace(relocated);
    }

    for(const auto& file_path : files_to_move)
    {
        const std::string relocated = new_prefix + file_path.substr(old_prefix.size());
        auto file_node = m_files.extract(file_path);
        file_node.key() = relocated;
        m_files.insert(std::move(file_node));
    }

    return true;
}

std::optional<FileHandle> RamFileSystem::OpenFileUnlocked(const std::string& path, OpenMode mode)
{
    if (!IsMountedUnlocked())
    {
        return std::nullopt;
    }

    const auto canonical_path = CanonicalizePath(path);
    if(!canonical_path)
    {
        return std::nullopt;
    }

    auto file_it = m_files.find(*canonical_path);

    if (file_it == m_files.end())
    {
        return std::nullopt;
    }

    const std::shared_ptr<File>& file = file_it->second;

    if (mode == OpenMode::ReadOnly && !file->readable)
    {
        return std::nullopt;
    }

    if ((mode == OpenMode::WriteOnly ||
         mode == OpenMode::Append) &&
        !file->writable)
    {
        return std::nullopt;
    }

    if (mode == OpenMode::ReadWrite &&
        (!file->readable || !file->writable))
    {
        return std::nullopt;
    }

    const FileHandle handle = m_next_handle++;

    std::size_t initial_offset = 0;

    if (mode == OpenMode::Append)
    {
        initial_offset = file->data.size();
    }

    m_open_files.emplace(
        handle,
        OpenFileEntry{
            file,
            initial_offset,
            mode
        }
    );

    return handle;
}

bool RamFileSystem::CloseFileUnlocked(FileHandle handle)
{
    if(!IsMountedUnlocked())
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
RamFileSystem::ReadOpenFileUnlocked(FileHandle handle, std::size_t count)
{
    if (!IsMountedUnlocked())
    {
        return std::nullopt;
    }

    const auto handle_it = m_open_files.find(handle);

    if (handle_it == m_open_files.end())
    {
        return std::nullopt;
    }

    if (handle_it->second.mode == OpenMode::WriteOnly || handle_it->second.mode == OpenMode::Append)
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

bool RamFileSystem::WriteOpenFileUnlocked(FileHandle handle, const std::string& data)
{
    if (!IsMountedUnlocked())
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

    handle_it->second.offset = offset + data.size();

    m_used_space = used_without_file + new_file_size;
    file->modification_time = std::time(nullptr);

    return true;
}

bool RamFileSystem::SeekFileUnlocked(FileHandle handle, std::size_t offset)
{
    if(!IsMountedUnlocked())
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

std::optional<std::size_t> RamFileSystem::GetFileOffsetUnlocked(FileHandle handle) const
{
    if(!IsMountedUnlocked())
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

bool RamFileSystem::SetFileReadableUnlocked(const std::string& path, bool readable)
{
    if(!IsMountedUnlocked())
    {
        return false;
    }

    const auto canonical_path = CanonicalizePath(path);
    if(!canonical_path)
    {
        return false;
    }

    auto file_it = m_files.find(*canonical_path);
    if(file_it == m_files.end())
    {
        return false;
    }

    file_it->second->readable = readable;

    return true;
}

bool RamFileSystem::SetFileWritableUnlocked(const std::string& path, bool writable)
{
    if(!IsMountedUnlocked())
    {
        return false;
    }

    const auto canonical_path = CanonicalizePath(path);
    if(!canonical_path)
    {
        return false;
    }

    auto file_it = m_files.find(*canonical_path);
    if(file_it == m_files.end())
    {
        return false;
    }

    file_it->second->writable = writable;

    return true;
}

std::optional<bool> RamFileSystem::IsFileReadableUnlocked(const std::string& path) const
{
    if(!IsMountedUnlocked())
    {
        return std::nullopt;
    }

    const auto canonical_path = CanonicalizePath(path);
    if(!canonical_path)
    {
        return std::nullopt;
    }

    auto file_it = m_files.find(*canonical_path);
    if(file_it == m_files.end())
    {
        return std::nullopt;
    }

    return file_it->second->readable;
}

std::optional<bool> RamFileSystem::IsFileWritableUnlocked(const std::string& path) const
{
    if(!IsMountedUnlocked())
    {
        return std::nullopt;
    }

    const auto canonical_path = CanonicalizePath(path);
    if(!canonical_path)
    {
        return std::nullopt;
    }

    auto file_it = m_files.find(*canonical_path);
    if(file_it == m_files.end())
    {
        return std::nullopt;
    }

    return file_it->second->writable;
}

std::optional<RamFileSystem::FileMetadata> RamFileSystem::GetFileMetadataUnlocked(const std::string& path) const
{
    if(!IsMountedUnlocked())
    {
        return std::nullopt;
    }

    const auto canonical_path = CanonicalizePath(path);
    if(!canonical_path)
    {
        return std::nullopt;
    }

    auto file_it = m_files.find(*canonical_path);
    if(file_it == m_files.end())
    {
        return std::nullopt;
    }

    const auto& file = file_it->second;

    FileMetadata metadata;
    metadata.size = file->data.size();
    metadata.readable = file->readable;
    metadata.writable = file->writable;
    metadata.creation_time = file->creation_time;
    metadata.modification_time = file->modification_time;

    return metadata;
}

/*********************************** Public API (locking) ****************************************/

RamFileSystem::RamFileSystem(std::size_t capacity) : m_is_mounted(false) ,m_capacity(capacity),
 m_used_space(0), m_next_handle(1)
{
    m_directories.emplace("/");
}

bool RamFileSystem::Mount(const std::string& mount_point)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    return MountUnlocked(mount_point);
}

bool RamFileSystem::Unmount()
{
    std::lock_guard<std::mutex> lock(m_mutex);
    return UnmountUnlocked();
}

bool RamFileSystem::CreateFile(const std::string& path)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    return CreateFileUnlocked(path);
}

bool RamFileSystem::WriteFile(const std::string& path, const std::string& data)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    return WriteFileUnlocked(path, data);
}

std::optional<std::string> RamFileSystem::ReadFile(const std::string& path) const
{
    std::lock_guard<std::mutex> lock(m_mutex);
    return ReadFileUnlocked(path);
}

bool RamFileSystem::DeleteFile(const std::string& path)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    return DeleteFileUnlocked(path);
}

bool RamFileSystem::IsMounted() const
{
    std::lock_guard<std::mutex> lock(m_mutex);
    return IsMountedUnlocked();
}

std::size_t RamFileSystem::GetUsedSpace() const
{
    std::lock_guard<std::mutex> lock(m_mutex);
    return GetUsedSpaceUnlocked();
}

std::size_t RamFileSystem::GetAvailableSpace() const
{
    std::lock_guard<std::mutex> lock(m_mutex);
    return GetAvailableSpaceUnlocked();
}

bool RamFileSystem::AppendFile(const std::string& path, const std::string& data)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    return AppendFileUnlocked(path, data);
}

bool RamFileSystem::FileExists(const std::string& path) const
{
    std::lock_guard<std::mutex> lock(m_mutex);
    return FileExistsUnlocked(path);
}

std::optional<std::size_t> RamFileSystem::GetFileSize(const std::string& path) const
{
    std::lock_guard<std::mutex> lock(m_mutex);
    return GetFileSizeUnlocked(path);
}

bool RamFileSystem::RenameFile(const std::string& old_path, const std::string& new_path)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    return RenameFileUnlocked(old_path, new_path);
}

bool RamFileSystem::CopyFile(const std::string& source_path, const std::string& destination_path)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    return CopyFileUnlocked(source_path, destination_path);
}

bool RamFileSystem::ClearFile(const std::string& path)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    return ClearFileUnlocked(path);
}

bool RamFileSystem::CreateDirectory(const std::string& path)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    return CreateDirectoryUnlocked(path);
}

bool RamFileSystem::DirectoryExists(const std::string& path) const
{
    std::lock_guard<std::mutex> lock(m_mutex);
    return DirectoryExistsUnlocked(path);
}

bool RamFileSystem::DeleteDirectory(const std::string& path)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    return DeleteDirectoryUnlocked(path);
}

std::optional<std::vector<RamFileSystem::DirectoryEntry>> RamFileSystem::ListDirectory(const std::string& path) const
{
    std::lock_guard<std::mutex> lock(m_mutex);
    return ListDirectoryUnlocked(path);
}

bool RamFileSystem::DeleteDirectoryRecursive(const std::string& path)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    return DeleteDirectoryRecursiveUnlocked(path);
}

bool RamFileSystem::MoveDirectory(const std::string& old_path, const std::string& new_path)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    return MoveDirectoryUnlocked(old_path, new_path);
}

std::optional<FileHandle> RamFileSystem::OpenFile(const std::string& path, OpenMode mode)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    return OpenFileUnlocked(path, mode);
}

bool RamFileSystem::CloseFile(FileHandle handle)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    return CloseFileUnlocked(handle);
}

std::optional<std::string> RamFileSystem::ReadOpenFile(FileHandle handle, std::size_t count)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    return ReadOpenFileUnlocked(handle, count);
}

bool RamFileSystem::WriteOpenFile(FileHandle handle, const std::string& data)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    return WriteOpenFileUnlocked(handle, data);
}

bool RamFileSystem::SeekFile(FileHandle handle, std::size_t offset)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    return SeekFileUnlocked(handle, offset);
}

std::optional<std::size_t> RamFileSystem::GetFileOffset(FileHandle handle) const
{
    std::lock_guard<std::mutex> lock(m_mutex);
    return GetFileOffsetUnlocked(handle);
}

bool RamFileSystem::SetFileReadable(const std::string& path, bool readable)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    return SetFileReadableUnlocked(path, readable);
}

bool RamFileSystem::SetFileWritable(const std::string& path, bool writable)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    return SetFileWritableUnlocked(path, writable);
}

std::optional<bool> RamFileSystem::IsFileReadable(const std::string& path) const
{
    std::lock_guard<std::mutex> lock(m_mutex);
    return IsFileReadableUnlocked(path);
}

std::optional<bool> RamFileSystem::IsFileWritable(const std::string& path) const
{
    std::lock_guard<std::mutex> lock(m_mutex);
    return IsFileWritableUnlocked(path);
}

std::optional<RamFileSystem::FileMetadata> RamFileSystem::GetFileMetadata(const std::string& path) const
{
    std::lock_guard<std::mutex> lock(m_mutex);
    return GetFileMetadataUnlocked(path);
}
