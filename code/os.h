#if !defined(OS_H_)
#define OS_H_

#if defined(__cplusplus)
extern "C" {
#endif

#if !defined(CORE_H_)
    #error "'os.h' relies on 'core.h'. please include it before including this file"
#endif

typedef struct OS_Handle OS_Handle;
struct OS_Handle {
    U64 v;
};

//
// --------------------------------------------------------------------------------
// :File_System
// --------------------------------------------------------------------------------
//
// The user facing api calls that take or deal with paths expect UTF-8 encoded strings
//

typedef U32 OS_FileProperties;
enum {
    OS_FILE_PROPERTY_DIRECTORY = (1 << 0),
    OS_FILE_PROPERTY_HIDDEN    = (1 << 1)
};

typedef struct OS_FileInfo OS_FileInfo;
struct OS_FileInfo {
    OS_FileInfo *next;

    Str8 name;

    U64 size;

    U64 last_write_time;
    U64 creation_time;

    OS_FileProperties props;
};

typedef struct OS_FileList OS_FileList;
struct OS_FileList {
    U32 count;

    OS_FileInfo *first;
    OS_FileInfo *last;
};

typedef U32 OS_FileAccess;
enum {
    OS_FILE_ACCESS_READ  = (1 << 0),
    OS_FILE_ACCESS_WRITE = (1 << 1)
};

Func OS_Handle OS_FileOpen(Str8 path, OS_FileAccess access);
Func void OS_FileClose(OS_Handle file);

Func void OS_FileRead(OS_Handle file, void *data, U64 offset, U64 size);
Func void OS_FileWrite(OS_Handle file, void *data, U64 offset, U64 size);

Func B32  OS_FileExists(Str8 path);
Func void OS_FileCreate(Str8 path);
Func void OS_FileDelete(Str8 path);

Func B32  OS_DirectoryExists(Str8 path);
Func void OS_DirectoryCreate(Str8 path);
Func void OS_DirectoryDelete(Str8 path);

Func OS_FileInfo OS_FileInfoFromPath(Arena *arena, Str8 path);
Func OS_FileInfo OS_FileInfoFromHandle(Arena *arena, OS_Handle file);

typedef U32 OS_FileIterFlags;
enum {
    OS_FILE_ITER_SKIP_DIRECTORIES = (1 << 0),
    OS_FILE_ITER_SKIP_FILES       = (1 << 1),
    OS_FILE_ITER_INCLUDE_HIDDEN   = (1 << 2) // skip hidden files by default
};

// Non-recursive list of all found entries in the directory specified by path, filtered
// by the flags provided. Final list will not include '.' and '..' relative entries regardless of
// inclusion of hidden directories
//
Func OS_FileList OS_DirectoryList(Arena *arena, Str8 path, OS_FileIterFlags flags);

typedef U32 OS_PathType;
enum {
    OS_PATH_EXECUTABLE = 0,
    OS_PATH_WORKING,
    OS_PATH_TEMP,
    OS_PATH_USER
};

// All of these paths will not contain a trailing path separator
//
Func Str8 OS_PathGet(Arena *arena, OS_PathType type);

//
// --------------------------------------------------------------------------------
// :Library_Loading
// --------------------------------------------------------------------------------
//

typedef void VoidProc(void);

Func OS_Handle  OS_LibraryOpen(Str8 path);
Func void       OS_LibraryClose(OS_Handle handle);
Func VoidProc  *OS_LibraryProcLoad(OS_Handle handle, Str8 name);

#if defined(__cplusplus)
}
#endif

#endif  // OS_H_

#if defined(OS_IMPL)

#if OS_WINDOWS

#if !defined(WIN32_LEAN_AND_MEAN)
    #define WIN32_LEAN_AND_MEAN 1
#endif

#include <windows.h>
#include <shlobj.h>

#pragma comment(lib, "kernel32.lib")
#pragma comment(lib, "user32.lib")
#pragma comment(lib, "shell32.lib")

//
// --------------------------------------------------------------------------------
// :Win32_Utilities
// --------------------------------------------------------------------------------
//

FileScope Str8 Win32_Str8ConvertToStr16(Arena *arena, Str8 str) {
    Str8 result = { 0 };

    // :note this is a platform specific version for now, if we feel we need it to be
    // platform-agnostic it can be changed later
    //
    // Don't think we need to though because Win32 is the only OS API using UTF-16 and
    // all of the user facing APIs expect UTF-8
    //
    // +1 for null-termination char as we assume all counted strings have unknown termination and
    // that the byte count doesn't include the null-terminating byte anyway (i.e. string literals)
    //
    S32 num_bytes = cast(int) str.count;
    U32 num_chars = MultiByteToWideChar(CP_UTF8, 0, cast(LPCCH) str.data, num_bytes, 0, 0) + 1;

    result.count = num_chars << 1; // in bytes
    result.data  = ArenaPush(arena, U8, result.count);

    MultiByteToWideChar(CP_UTF8, 0, cast(LPCCH) str.data, num_bytes, cast(LPWSTR) result.data, num_chars - 1);

    return result;
}

FileScope Str8 Win32_Str16ConvertToStr8(Arena *arena, Str8 str) {
    Str8 result;

    S32 num_chars = cast(S32) (str.count >> 1);
    WCHAR *chars  = cast(WCHAR *) str.data;

    result.count = WideCharToMultiByte(CP_UTF8, 0, chars, num_chars, 0, 0, 0, 0);
    result.data  = ArenaPush(arena, U8, result.count);

    WideCharToMultiByte(CP_UTF8, 0, chars, num_chars, cast(LPSTR) result.data, cast(S32) result.count, 0, 0);

    return result;
}

FileScope Str8 Win32_Str16WrapNullTerminated(LPCWSTR str) {
    Str8 result;

    S64 num_chars = 0;
    while (str[num_chars] != 0) {
        num_chars += 1;
    }

    result.count = (num_chars << 1); // in bytes
    result.data  = cast(U8 *) str;

    return result;
}

//
// --------------------------------------------------------------------------------
// :Win32_File_System
// --------------------------------------------------------------------------------
//

OS_Handle OS_FileOpen(Str8 path, OS_FileAccess access) {
    OS_Handle result = { 0 };

    TempArena temp = TempGet(0, 0);

    // Will be null-terminated
    //
    Str8 wpath = Win32_Str8ConvertToStr16(temp.arena, path);

    DWORD desired_access = 0;
    DWORD share_mode     = FILE_SHARE_READ;
    DWORD creation_type  = OPEN_EXISTING;

    if (access & OS_FILE_ACCESS_READ) { desired_access |= GENERIC_READ; }
    if (access & OS_FILE_ACCESS_WRITE) {
        desired_access |= GENERIC_WRITE;
        creation_type   = OPEN_ALWAYS;
    }

    HANDLE handle = CreateFileW((LPCWSTR) wpath.data, desired_access, share_mode, 0, creation_type, 0, 0);

    // can be INVALID_HANDLE_VALUE
    //
    result.v = cast(U64) handle;

    TempRelease(&temp);

    return result;
}

void OS_FileClose(OS_Handle file) {
    HANDLE handle = cast(HANDLE) file.v;
    if (handle != INVALID_HANDLE_VALUE) { CloseHandle(handle); }
}

void OS_FileRead(OS_Handle file, void *data, U64 offset, U64 size) {
    HANDLE handle = cast(HANDLE) file.v;
    if (handle != INVALID_HANDLE_VALUE) {
        U64 current_offset = offset;
        U64 size_remaining = size;

        U8 *data_at = cast(U8 *) data;

        while (size_remaining != 0) {
            OVERLAPPED ov = { 0 };
            ov.Offset     = cast(DWORD) (current_offset >>  0);
            ov.OffsetHigh = cast(DWORD) (current_offset >> 32);

            // @todo: make some sort of cast saturate function
            //
            DWORD to_read = ((size_remaining > U32_MAX) ? U32_MAX : (DWORD) size_remaining);
            DWORD nread   = 0;
            if (!ReadFile(handle, data_at, to_read, &nread, &ov)) {
                // Failed to read so bail
                //
                break;
            }

            Assert(nread <= size_remaining);

            size_remaining -= nread;
            current_offset += nread;
            data_at        += nread;
        }
    }
}

void OS_FileWrite(OS_Handle file, void *data, U64 offset, U64 size) {
    HANDLE handle = cast(HANDLE) file.v;
    if (handle != INVALID_HANDLE_VALUE) {
        U64 current_offset = offset;
        U64 size_remaining = size;

        U8 *data_at = cast(U8 *) data;

        while (size_remaining != 0) {
            OVERLAPPED ov = { 0 };
            ov.Offset     = cast(DWORD) (current_offset >>  0);
            ov.OffsetHigh = cast(DWORD) (current_offset >> 32);

            DWORD to_write = ((size_remaining > U32_MAX) ? U32_MAX : (DWORD) size_remaining);
            DWORD nwritten = 0;
            if (!WriteFile(handle, data_at, to_write, &nwritten, &ov)) {
                // Failed to read so bail
                //
                break;
            }

            Assert(nwritten <= size_remaining);

            size_remaining -= nwritten;
            current_offset += nwritten;
            data_at        += nwritten;
        }
    }
}

B32 OS_FileExists(Str8 path) {
    B32 result = false;

    TempArena temp = TempGet(0, 0);
    Str8 wpath     = Win32_Str8ConvertToStr16(temp.arena, path);

    DWORD attrs = GetFileAttributesW((LPCWSTR) wpath.data);

    TempRelease(&temp);

    // Exists and isn't a directory, assume file
    //
    // @todo: test this, Windows likes to mark normal files as ARCHIVE for some reason
    //
    result = (attrs != INVALID_FILE_ATTRIBUTES) && ((attrs & FILE_ATTRIBUTE_DIRECTORY) == 0);
    return result;
}

void OS_FileCreate(Str8 path) {
    TempArena temp = TempGet(0, 0);
    Str8 wpath     = Win32_Str8ConvertToStr16(temp.arena, path);

    DWORD access = GENERIC_WRITE;
    DWORD share  = FILE_SHARE_READ | FILE_SHARE_WRITE;
    DWORD create = OPEN_ALWAYS; // Could use CREATE_ALWAYS instead if we want to truncate existing files

    HANDLE handle = CreateFileW((LPCWSTR) wpath.data, access, share, 0, create, 0, 0);
    CloseHandle(handle);

    TempRelease(&temp);
}

void OS_FileDelete(Str8 path) {
    TempArena temp = TempGet(0, 0);
    Str8 wpath     = Win32_Str8ConvertToStr16(temp.arena, path);

    DeleteFileW((LPCWSTR) wpath.data);

    TempRelease(&temp);
}

B32 OS_DirectoryExists(Str8 path) {
    B32 result;

    TempArena temp = TempGet(0, 0);
    Str8 wpath     = Win32_Str8ConvertToStr16(temp.arena, path);

    DWORD attrs = GetFileAttributesW((LPCWSTR) wpath.data);

    TempRelease(&temp);

    result = (attrs != INVALID_FILE_ATTRIBUTES) && ((attrs & FILE_ATTRIBUTE_DIRECTORY) != 0);
    return result;
}

void OS_DirectoryCreate(Str8 path) {
    TempArena temp = TempGet(0, 0);
    Str8 wpath     = Win32_Str8ConvertToStr16(temp.arena, path);

    // This allows creates all the way down to the sub-directory even if the parent directories don't
    // exist
    //
    WCHAR *data = cast(WCHAR *) wpath.data;
    for (U32 it = 0; data[it] != 0; it += 1) {
        if (it && (data[it] == L'/') || (data[it] == L'\\')) {
            WCHAR sep = data[it];
            data[it]  = 0;

            if (!CreateDirectoryW(data, 0)) {
                DWORD err = GetLastError();
                if (err != ERROR_ALREADY_EXISTS) {
                    break;
                }
            }

            data[it] = sep;
        }
    }

    CreateDirectoryW(data, 0);

    TempRelease(&temp);
}

void OS_DirectoryDelete(Str8 path) {
    TempArena temp = TempGet(0, 0);
    Str8 wpath     = Win32_Str8ConvertToStr16(temp.arena, path);

    RemoveDirectoryW((LPCWSTR) wpath.data);

    TempRelease(&temp);
}

OS_FileInfo OS_FileInfoFromPath(Arena *arena, Str8 path) {
    OS_FileInfo result = { 0 };

    TempArena temp = TempGet(1, &arena);
    Str8 wpath     = Win32_Str8ConvertToStr16(temp.arena, path);

    WIN32_FILE_ATTRIBUTE_DATA data;
    if (GetFileAttributesExW((LPCWSTR) wpath.data, GetFileExInfoStandard, &data)) {
        FILETIME mtime = data.ftLastWriteTime;
        FILETIME ctime = data.ftCreationTime;

        Str8 filename = Str8PathBasename(path);
        result.name   = Str8PushCopy(arena, filename);

        if (data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) { result.props |= OS_FILE_PROPERTY_DIRECTORY; }
        if (data.dwFileAttributes & FILE_ATTRIBUTE_HIDDEN)    { result.props |= OS_FILE_PROPERTY_HIDDEN;    }
        if (filename.count && filename.data[0] == '.')        { result.props |= OS_FILE_PROPERTY_HIDDEN;    }

        result.size            = cast(U64) data.nFileSizeHigh   << 32 | cast(U64) data.nFileSizeLow;
        result.last_write_time = cast(U64) mtime.dwHighDateTime << 32 | cast(U64) mtime.dwLowDateTime;
        result.creation_time   = cast(U64) ctime.dwHighDateTime << 32 | cast(U64) ctime.dwLowDateTime;
    }

    TempRelease(&temp);

    return result;
}

OS_FileInfo OS_FileInfoFromHandle(Arena *arena, OS_Handle file) {
    OS_FileInfo result = { 0 };

    HANDLE hFile = cast(HANDLE) file.v;
    if (hFile != INVALID_HANDLE_VALUE) {
        BY_HANDLE_FILE_INFORMATION info;
        if (GetFileInformationByHandle(hFile, &info)) {
            FILETIME mtime = info.ftLastWriteTime;
            FILETIME ctime = info.ftCreationTime;

            result.size            = cast(U64) info.nFileSizeHigh   << 32 | cast(U64) info.nFileSizeLow;
            result.last_write_time = cast(U64) mtime.dwHighDateTime << 32 | cast(U64) mtime.dwLowDateTime;
            result.creation_time   = cast(U64) ctime.dwHighDateTime << 32 | cast(U64) ctime.dwLowDateTime;

            TempArena temp = TempGet(1, &arena);

            U32 buffer_limit = cast(U32) ((KB(32) << 1) + sizeof(FILE_NAME_INFO));
            WCHAR *buffer    = cast(WCHAR *) ArenaPush(temp.arena, U8, buffer_limit, ARENA_FLAG_NO_ZERO);

            if (GetFileInformationByHandleEx(hFile, FileNameInfo, buffer, buffer_limit)) {
                FILE_NAME_INFO *name_info = cast(FILE_NAME_INFO *) buffer;

                Str8 wpath = Str8WrapCount((U8 *) name_info->FileName, name_info->FileNameLength);
                Str8 path  = Win32_Str16ConvertToStr8(temp.arena, wpath);

                Str8 filename = Str8PathBasename(path);
                result.name   = Str8PushCopy(arena, filename);
            }

            // Directory should be false as it is an opened file
            //
            if (info.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) { result.props |= OS_FILE_PROPERTY_DIRECTORY; }
            if (info.dwFileAttributes & FILE_ATTRIBUTE_HIDDEN)    { result.props |= OS_FILE_PROPERTY_HIDDEN;    }
            if (result.name.count && result.name.data[0] == '.')  { result.props |= OS_FILE_PROPERTY_HIDDEN;    }

            TempRelease(&temp);
        }
    }

    return result;
}

OS_FileList OS_DirectoryList(Arena *arena, Str8 path, OS_FileIterFlags flags) {
    OS_FileList result = { 0 };

    TempArena temp = TempGet(1, &arena);

    Str8 search_path = Str8Format(temp.arena, Str8Literal("%.*s\\*"), Str8Arg(path));
    Str8 wpath       = Win32_Str8ConvertToStr16(temp.arena, search_path);

    B32 skip_files  = (flags & OS_FILE_ITER_SKIP_FILES)       != 0;
    B32 skip_dirs   = (flags & OS_FILE_ITER_SKIP_DIRECTORIES) != 0;
    B32 skip_hidden = (flags & OS_FILE_ITER_INCLUDE_HIDDEN)   == 0;

    WIN32_FIND_DATAW data;
    HANDLE handle = FindFirstFileW((LPCWSTR) wpath.data, &data);
    while (handle != INVALID_HANDLE_VALUE) {
        B32 is_dir    = (data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY);
        B32 is_hidden = (data.dwFileAttributes & FILE_ATTRIBUTE_HIDDEN) || (data.cFileName[0] == L'.');

        // We do not include relative entries in the list because they are ubiquitous on any platform
        // we are currently supporting so the user can just assume they are available for use and thus
        // there isn't really any point
        //
        B32 is_relative =
            (data.cFileName[0] == L'.' && data.cFileName[1] == 0) ||
            (data.cFileName[0] == L'.' && data.cFileName[1] == L'.' && data.cFileName[2] == 0);

        // If the user has requested to skip files/directories/hidden and this file matches any of those
        // types we skip and don't add them to the list
        //
        // @todo: this can probably be compressed down to a single statement
        //
        B32 valid = !is_relative;

        if (skip_files  && !is_dir)    { valid = false; }
        if (skip_dirs   &&  is_dir)    { valid = false; }
        if (skip_hidden &&  is_hidden) { valid = false; }

        if (valid) {
            // Push a new file info and fill it out appending it to the end of the list
            //
            OS_FileInfo *info = ArenaPush(arena, OS_FileInfo);

            FILETIME mtime = data.ftLastWriteTime;
            FILETIME ctime = data.ftCreationTime;

            Str8 wfilename = Win32_Str16WrapNullTerminated(data.cFileName);
            Str8 filename  = Win32_Str16ConvertToStr8(arena, wfilename);

            info->name            = filename;
            info->size            = cast(U64) data.nFileSizeHigh   << 32 | data.nFileSizeLow;
            info->last_write_time = cast(U64) mtime.dwHighDateTime << 32 | mtime.dwLowDateTime;
            info->creation_time   = cast(U64) ctime.dwHighDateTime << 32 | ctime.dwLowDateTime;

            if (data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) { info->props |= OS_FILE_PROPERTY_DIRECTORY; }
            if (data.dwFileAttributes & FILE_ATTRIBUTE_HIDDEN)    { info->props |= OS_FILE_PROPERTY_HIDDEN;    }
            if (filename.count && filename.data[0] == '.')        { info->props |= OS_FILE_PROPERTY_HIDDEN;    }

            QueuePush(result.first, result.last, info);

            result.count += 1;
        }

        if (!FindNextFileW(handle, &data)) {
            FindClose(handle);
            break;
        }
    }

    TempRelease(&temp);

    return result;
}

Str8 OS_PathGet(Arena *arena, OS_PathType type) {
    Str8 result = { 0 };

    TempArena temp = TempGet(1, &arena);

    // @todo: For now we are just getting the paths each time this is called, we will likley require
    // an 'OS_Init' call at some point so we can just cache the static paths when we call that and
    // just return copies directly
    //
    // :os_init
    //

    switch (type) {
        case OS_PATH_EXECUTABLE: {
            DWORD  limit  = KB(32);
            WCHAR *buffer = ArenaPush(temp.arena, WCHAR, limit, ARENA_FLAG_NO_ZERO);

            DWORD num_chars = GetModuleFileNameW(0, buffer, limit);

            // Remove the last section of the path as this returns the full filename path of the
            // executable, not the directory it is contained in
            //
            while (num_chars && buffer[num_chars] != L'\\') { num_chars -= 1; }

            Str8 wpath;
            wpath.count = (num_chars << 1);
            wpath.data  = cast(U8 *) buffer;

            result = Win32_Str16ConvertToStr8(arena, wpath);
        }
        break;
        case OS_PATH_WORKING: {
            DWORD num_chars = GetCurrentDirectoryW(0, 0);
            if (num_chars > 0) {
                Str8 wpath;
                wpath.count = (num_chars - 1) << 1; // we don't care about the null terminator
                wpath.data  = cast(U8 *) ArenaPush(temp.arena, WCHAR, num_chars, ARENA_FLAG_NO_ZERO);

                GetCurrentDirectoryW(num_chars, cast(LPWSTR) wpath.data);

                result = Win32_Str16ConvertToStr8(arena, wpath);
            }
        }
        break;
        case OS_PATH_TEMP: {
            // @todo: WinAPI docs recommend using GetTempPath2W however that is only available
            // on relatively new iterations of Windows 10, so we should attempt to load the
            // new function from kernel32.dll and fallback to GetTempPathW if it isn't available
            //
            // again.. :os_init
            //
            // For now I am just going to use GetTempPathW as there doesn't seem to be too
            // much of a difference between the two calls
            //
            DWORD  limit  = MAX_PATH + 1; // Max possible length as per docs
            WCHAR *buffer = ArenaPush(temp.arena, WCHAR, limit, ARENA_FLAG_NO_ZERO);

            DWORD num_chars = GetTempPathW(limit, buffer);
            if (num_chars > 0) {
                num_chars -= 1; // The resulting string has a trailing backslash so remove

                Str8 wpath;
                wpath.count = (num_chars << 1);
                wpath.data  = cast(U8 *) buffer;

                result = Win32_Str16ConvertToStr8(arena, wpath);
            }
        }
        break;
        case OS_PATH_USER: {
            DWORD  limit  = KB(32);
            WCHAR *buffer = ArenaPush(temp.arena, WCHAR, limit, ARENA_FLAG_NO_ZERO);

            // ... sigh, I wanted to use the recommended SHGetKnownFolderPath here but because
            // Microsoft and the C++ committee the first argument is a reference in C++ and
            // a pointer in C, this means without randomly ifdef-ing around it you can't compile
            // for both
            //
            // again.. showing that C and C++ are not actually interoperable
            //
            if (SHGetFolderPathW(0, CSIDL_APPDATA, 0, 0, buffer) == S_OK) {
                Str8 wpath = Win32_Str16WrapNullTerminated(buffer);
                result     = Win32_Str16ConvertToStr8(arena, wpath);
            }
        }
        break;
    }

    TempRelease(&temp);

    return result;
}

//
// --------------------------------------------------------------------------------
// :Win32_Library_Loading
// --------------------------------------------------------------------------------
//

OS_Handle OS_LibraryOpen(Str8 path) {
    OS_Handle result;

    TempArena temp = TempGet(0, 0);
    Str8 wpath = Win32_Str8ConvertToStr16(temp.arena, path);

    HMODULE library = LoadLibraryW((LPCWSTR) wpath.data);
    result.v = cast(U64) library;

    TempRelease(&temp);

    return result;
}

void OS_LibraryClose(OS_Handle handle) {
    HMODULE library = cast(HMODULE) handle.v;
    if (library) {
        FreeLibrary(library);
    }
}

VoidProc *OS_LibraryProcLoad(OS_Handle handle, Str8 name) {
    VoidProc *result = 0;

    HMODULE library = cast(HMODULE) handle.v;
    if (library) {
        TempArena temp   = TempGet(0, 0);
        LPCSTR proc_name = Str8PushCopyNullTerminated(temp.arena, name);

        result = cast(VoidProc *) GetProcAddress(library, proc_name);
        TempRelease(&temp);
    }

    return result;
}

#elif OS_LINUX

#elif OS_SWITCH

#endif

#endif
