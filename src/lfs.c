/*
 * The little filesystem
 *
 * Copyright (c) 2017, Arm Limited. All rights reserved.
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * Modified by Torsten Wolf, 2023 - replace lfs implementation by native calls, remove readonly defines, added mock support
 * (find original version in "framework-arduinoespressif8266/libraries/LittleFS/lib/littlefs")
 */

#include <io.h>
#include <stdio.h>
#include <errno.h>
#include <sys/stat.h>
#include "lfs.h"

/*
 * On windows open files in binary mode to prevent confusions with EOL
 * use always \n only instead of translated \r\n
 */
#if defined(_WIN32)
    #define OM_APPEND_ONLY "ab"
    #define OM_APPEND_READ "ab+"
    #define OM_READ_ONLY   "rb"
    #define OM_READ_WRITE  "rb+"
    #define OM_WRITE_ONLY  "wb"
    #define OM_WRITE_READ  "wb+"
#else
    #define OM_APPEND_ONLY "a"
    #define OM_APPEND_READ "a+"
    #define OM_READ_ONLY   "r"
    #define OM_READ_WRITE  "r+"
    #define OM_WRITE_ONLY  "w"
    #define OM_WRITE_READ  "w+"
#endif

char path_buffer [512];
const char* patch_path(lfs_t *lfs, const char* path)
{
    if ( strncmp(lfs->test_dir, path, strlen(lfs->test_dir)) == 0)
        // already patched
        return path;

    strcpy(path_buffer, lfs->test_dir);
    strcat(path_buffer, path);
    return path_buffer;
}

int lfs_format(lfs_t *lfs, const struct lfs_config *config)
{
    return 0;
}
int lfs_mount(lfs_t *lfs, const struct lfs_config *config)
{
    return 0;
}
int lfs_unmount(lfs_t *lfs)
{
    return 0;
}

int lfs_remove(lfs_t *lfs, const char *path)
{
    path = patch_path(lfs, path);
    int rc = remove(path);
    if (rc == -1) {
        rc = errno;
        if (rc == EACCES)
            return rmdir(path);
        return rc;
    }
    return 0;
}
int lfs_rename(lfs_t *lfs, const char *oldpath, const char *newpath)
{
    char opp[512];
    strcpy(opp, patch_path(lfs, oldpath));
    newpath = patch_path(lfs, newpath);
    return rename(opp, newpath);
}
int lfs_stat(lfs_t *lfs, const char *path, struct lfs_info *info)
{
    path = patch_path(lfs, path);

    struct stat buffer;
    
    int rc = stat(path, &buffer);
    if(rc == 0)
    {
        if (S_ISDIR(buffer.st_mode))
            info->type = LFS_TYPE_DIR;
        else
            info->type = LFS_TYPE_REG;
    }
    return rc;
}
lfs_ssize_t lfs_getattr(lfs_t *lfs, const char *path, uint8_t type, void *buffer, lfs_size_t size)
{
    return 0;
}

int lfs_setattr(lfs_t *lfs, const char *path, uint8_t type, const void *buffer, lfs_size_t size)
{
    return 0;
}

int lfs_removeattr(lfs_t *lfs, const char *path, uint8_t type)
{
    return 0;
}

int lfs_file_open(lfs_t *lfs, lfs_file_t *file, const char *path, int flags)
{
    path = patch_path(lfs, path);
    /*
    LFS_O_RDONLY = 1,         // Open a file as read only
    LFS_O_WRONLY = 2,         // Open a file as write only
    LFS_O_RDWR   = 3,         // Open a file as read and write
    LFS_O_CREAT  = 0x0100,    // Create a file if it does not exist
    LFS_O_EXCL   = 0x0200,    // Fail if a file already exists
    LFS_O_TRUNC  = 0x0400,    // Truncate the existing file to zero size
    LFS_O_APPEND = 0x0800,    // Move to end of file on every write
     */
    const char *type;
    if ((flags & LFS_O_RDWR) == LFS_O_RDWR)
    {
        if (flags & LFS_O_APPEND)
            type = OM_APPEND_READ;
        else
        if (flags & LFS_O_CREAT)
            type = OM_WRITE_READ;
        else
            type = OM_READ_WRITE;
    }
    else if ((flags & LFS_O_RDONLY) == LFS_O_RDONLY)
    {
        type = OM_READ_ONLY;
    }
    else if ((flags & LFS_O_WRONLY) == LFS_O_WRONLY)
    {
        if (flags & LFS_O_APPEND)
            type = OM_APPEND_ONLY;
        else
            type = OM_WRITE_ONLY;
    }
    file->pFile = fopen(path, type);
    return file->pFile != NULL ? 0 : -1;
}

int lfs_file_opencfg(lfs_t *lfs, lfs_file_t *file, const char *path, int flags, const struct lfs_file_config *config)
{
    path = patch_path(lfs, path);
    return lfs_file_open(lfs, file, path, flags);
}

int lfs_file_close(lfs_t *lfs, lfs_file_t *file)
{
    FILE *pFile = file->pFile;
    file->pFile = NULL;
    return fclose(pFile);
}

int lfs_file_sync(lfs_t *lfs, lfs_file_t *file)
{
    return fflush(file->pFile);
}

lfs_ssize_t lfs_file_read(lfs_t *lfs, lfs_file_t *file, void *buffer, lfs_size_t size)
{
    return fread(buffer, 1, size, file->pFile);
}

lfs_ssize_t lfs_file_write(lfs_t *lfs, lfs_file_t *file, const void *buffer, lfs_size_t size)
{
    return fwrite(buffer, 1, size, file->pFile);
}

lfs_soff_t lfs_file_seek(lfs_t *lfs, lfs_file_t *file, lfs_soff_t off, int whence)
{
    return fseek(file->pFile, off, whence);
}

int lfs_file_truncate(lfs_t *lfs, lfs_file_t *file, lfs_off_t size)
{
    return _chsize(_fileno(file->pFile), size);
}

lfs_soff_t lfs_file_tell(lfs_t *lfs, lfs_file_t *file)
{
    return ftell(file->pFile);
}

int lfs_file_rewind(lfs_t *lfs, lfs_file_t *file)
{
    rewind(file->pFile);
    return 0;
}

lfs_soff_t lfs_file_size(lfs_t *lfs, lfs_file_t *file)
{
    int prev = ftell(file->pFile);
    fseek(file->pFile, 0L, SEEK_END);
    int sz = ftell(file->pFile);
    fseek(file->pFile, prev, SEEK_SET); //go back to where we were
    return sz;
}

/// Directory operations ///

int lfs_mkdir(lfs_t *lfs, const char *path)
{
    path = patch_path(lfs, path);
    return mkdir(path);
}

int lfs_dir_open(lfs_t *lfs, lfs_dir_t *dir, const char *path)
{
    path = patch_path(lfs, path);
    strcpy(dir->path, path);
    dir->pDir = opendir(path);
    return dir->pDir != NULL ? 0 : -1;
}

int lfs_dir_close(lfs_t *lfs, lfs_dir_t *dir)
{
    DIR *pDir = dir->pDir;
    dir->pDir = NULL;
    return closedir(pDir);
}

int lfs_dir_read(lfs_t *lfs, lfs_dir_t *dir, struct lfs_info *info)
{
    struct dirent* pEntry = readdir(dir->pDir);
    if (pEntry != NULL)
    {
        strcpy(info->name, pEntry->d_name);
        if (strcmp(pEntry->d_name, ".") == 0 || strcmp(pEntry->d_name, "..") == 0)
        {
            info->type = LFS_TYPE_DIR;
            info->size = 0;
            return true;
        }
        struct stat buffer;
        char path[300];
        strcpy(path, dir->path);
        strcat(path, "/");
        strcat(path, pEntry->d_name);
        if (stat(path, &buffer) == 0)
        {
            if (S_ISDIR(buffer.st_mode))
            {
                info->type = LFS_TYPE_DIR;
                info->size = 0;
                return true;
            }
            else if (S_ISREG(buffer.st_mode))
            {
                info->type = LFS_TYPE_REG;
                info->size = buffer.st_size;
                return true;
            }
        }
    }
    return false;
}

int lfs_dir_seek(lfs_t *lfs, lfs_dir_t *dir, lfs_off_t off)
{
    return 0;
}

lfs_soff_t lfs_dir_tell(lfs_t *lfs, lfs_dir_t *dir)
{
    return 0;
}

int lfs_dir_rewind(lfs_t *lfs, lfs_dir_t *dir)
{
    rewinddir(dir->pDir);
    return 0;
}

/// Filesystem-level filesystem operations ///
int internal_is_dir(const char * path)
{
    struct stat path_stat;
    stat(path, &path_stat);
    return !(S_ISREG(path_stat.st_mode));
}

int internal_size(const char * name)
{
    int dir_size = 0;
    struct dirent * pDirent;
    DIR * pDir = opendir(name);
    while ((pDirent = readdir(pDir)) != NULL)
    {
        if ( strcmp(pDirent->d_name, ".") != 0 && strcmp(pDirent->d_name, "..") != 0 )
        {
            char buf [512];
            strcpy(buf, name);
            strcat(buf, pDirent->d_name);
            if (internal_is_dir(buf))
            {
                strcat(buf, "/");
                dir_size += internal_size(buf);
            }
            else
            {
                struct stat st;
                stat(buf, &st);
                int sz = st.st_size;
                dir_size += sz;
            }
        }
    }
    return dir_size;
}

lfs_ssize_t lfs_fs_size(lfs_t *lfs)
{
    return internal_size(lfs->test_dir);
}

int lfs_fs_traverse(lfs_t *lfs, int (*cb)(void *, lfs_block_t), void *data)
{
    return 0;
}
