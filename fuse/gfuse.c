#define FUSE_USE_VERSION 31

#include <fuse.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>
#include <time.h>
#include <string.h>
#include <stdlib.h>
#include <pthread.h>
#include <errno.h>

// #include "mylist.h"
// #include <linux/rbtree.h>
// #include "list.h"

#define MAX_NMAE_LEN 255
#define MAX_FILE_NUM 1024
#define MAX_DIR_NMU 1024
#define BLOCK_SIZE 4096
#define _GNU_SOURCE

struct memfs_file
{
    char *id;
    char *content;
    // int real_size;
    int content_size;
    struct stat fstat;
};

struct memfs_dir
{
    char *id;
    struct memfs_file *files[MAX_FILE_NUM];
    int file_num;
    struct memfs_dir *dirs[MAX_DIR_NMU];
    int dir_num;
    struct stat dstat;
};

void init_memfs_dir(struct memfs_dir *dir, char *dir_id)
{
    dir->id = dir_id;
    dir->file_num = 0;
    dir->dir_num = 0;
}

void init_memfs_file(struct memfs_file *file, char *file_id)
{
    file->id = file_id;
    file->content = "";
    file->content_size = 0;
    // file->real_size = 0;
}

int add_file_to_dir(struct memfs_dir *dir, struct memfs_file *file)
{
    if (dir->file_num >= MAX_FILE_NUM)
    {
        printf("file num is full\n");
        return -1;
    }
    dir->files[dir->file_num] = file;
    dir->file_num++;
    return 0;
}

void remove_file_from_dir(struct memfs_dir *dir, struct memfs_file *file)
{
    int i = 0;
    for (; i < dir->file_num; i++)
    {
        if (strcmp(dir->files[i]->id, file->id) == 0)
        {
            break;
        }
    }
    if (i == dir->file_num)
    {
        printf("file not found\n");
        return;
    }
    for (; i < dir->file_num - 1; i++)
    {
        dir->files[i] = dir->files[i + 1];
    }
    dir->file_num--;
}

void clear_file_of_dir(struct memfs_dir *dir)
{
    dir->file_num = 0;
}

int add_dir_to_dir(struct memfs_dir *dir, struct memfs_dir *sub_dir)
{
    if (dir->dir_num >= MAX_DIR_NMU)
    {
        printf("dir num is full\n");
        return -1;
    }
    dir->dirs[dir->dir_num] = sub_dir;
    dir->dir_num++;
    return 0;
}

void remove_dir_from_dir(struct memfs_dir *dir, struct memfs_dir *sub_dir)
{
    int i = 0;
    for (; i < dir->dir_num; i++)
    {
        if (strcmp(dir->dirs[i]->id, sub_dir->id) == 0)
        {
            break;
        }
    }
    if (i == dir->dir_num)
    {
        printf("dir not found\n");
        return;
    }
    for (; i < dir->dir_num - 1; i++)
    {
        dir->dirs[i] = dir->dirs[i + 1];
    }
    dir->dir_num--;
}

void clear_dir_of_dir(struct memfs_dir *dir)
{
    dir->dir_num = 0;
}

struct memfs
{
    struct memfs_dir *root;
};

struct memfs *gfs;

void init_memfs(struct memfs *memfs)
{
    memfs->root = (struct memfs_dir *)malloc(sizeof(struct memfs_dir));
    init_memfs_dir(memfs->root, "/");
}

struct path
{
    char *split_path[MAX_NMAE_LEN];
    int split_num;
};

struct path *split_path(const char *raw_path)
{
    char *raw_path_copy = (char *)malloc(sizeof(char) * MAX_NMAE_LEN);
    strcpy(raw_path_copy, raw_path);
    struct path *p = (struct path *)malloc(sizeof(struct path));
    char *token = strtok(raw_path_copy, "/");
    int i = 0;
    while (token != NULL)
    {
        p->split_path[i] = token;
        token = strtok(NULL, "/");
        i++;
    }
    p->split_num = i;
    return p;
}

struct path *get_parent_path(struct path *path)
{
    struct path *parent_path = (struct path *)malloc(sizeof(struct path));
    parent_path->split_num = path->split_num - 1;
    for (int i = 0; i < parent_path->split_num; i++)
    {
        parent_path->split_path[i] = path->split_path[i];
    }
    return parent_path;
}

struct memfs_dir *find_dir(struct path *dir_path)
{
    struct memfs_dir *dir = gfs->root;
    for (int i = 0; i < dir_path->split_num; i++)
    {
        int flag = 0;
        for (int j = 0; j < dir->dir_num; j++)
        {
            if (strcmp(dir->dirs[j]->id, dir_path->split_path[i]) == 0)
            {
                flag = 1;
                dir = dir->dirs[j];
                break;
            }
        }
        if (flag == 0)
        {
            return NULL;
        }
    }
    return dir;
}

struct memfs_file *find_file(struct path *file_path)
{
    printf("[find_file] enter find file \n");
    struct path *dir_path = get_parent_path(file_path);
    if (file_path->split_num < 1)
    {
        // printf("[find_file] split_num == 0 \n");
        return NULL;
    }
    char *file_name = file_path->split_path[file_path->split_num - 1];

    struct memfs_dir *dir = find_dir(dir_path);
    // printf("[find_file]=====\n");
    // printf("[find_file] file_num = %d \n", dir->file_num);
    if (dir == NULL)
    {
        return NULL;
    }
    for (int i = 0; i < dir->file_num; i++)
    {
        // printf("[find_file] file id = %s; filename = %s \n", dir->files[i]->id, file_name);
        if (dir->files[i] == NULL)
        {
            printf("[find_file] file is null \n");
        }
        else if (dir->files[i]->id == NULL)
        {
            printf("[find_file] file id is null \n");
        }
        else if (strcmp(dir->files[i]->id, file_name) == 0)
        {
            printf("[find_file] file found \n");
            return dir->files[i];
        }
    }
    printf("[find_file] file not found \n");
    return NULL;
}

static int gfs_getattr(const char *path, struct stat *stbuf, struct fuse_file_info *fi)
{
    // int res = 0;
    printf("[getattr]: entered, path = %s\n", path);
    // memset(stbuf, 0, sizeof(struct stat));

    stbuf->st_uid = getuid();
    stbuf->st_gid = getgid();
    stbuf->st_atime = time(NULL);
    stbuf->st_mtime = time(NULL);

    struct path *s_path = split_path(path);
    struct memfs_dir *dir = find_dir(s_path);
    printf("[getattr]: ====\n");
    struct memfs_file *file = find_file(s_path);
    printf("[getattr]: ####\n");
    if (dir != NULL)
    {
        stbuf->st_mode = S_IFDIR | 0755;
        stbuf->st_nlink = 2;
        stbuf->st_size = 0;
    }
    else if (file != NULL)
    {
        stbuf->st_mode = S_IFREG | 0644;
        stbuf->st_nlink = 1;
        stbuf->st_size = file->content_size;
    }
    else
    {
        return -ENOENT; // no such file or directory
    }
    return 0;
};

//--------cd-----------
static int gfs_access(const char *path, int mask)
{
    printf("[access]: entered\n");
    struct path *s_path = split_path(path);
    if (find_dir(s_path) != NULL || find_file(s_path) != NULL)
    {
        return 0;
    }
    else
    {
        return -ENOENT;
    }
};
//--------ls-----------
static int gfs_readdir(const char *path, void *buffer, fuse_fill_dir_t filler, off_t offset,
                       struct fuse_file_info *fi, enum fuse_readdir_flags flags)
{
    filler(buffer, ".", NULL, 0, 0);
    if (strcmp(path, "/") != 0)
    {
        filler(buffer, "..", NULL, 0, 0);
    }
    struct path *s_path = split_path(path);
    struct memfs_dir *dir = find_dir(s_path);
    if (dir == NULL)
    {
        return -ENOENT;
    }
    for (int i = 0; i < dir->dir_num; i++)
    {
        if (dir->dirs[i]->id == NULL)
        {
            printf("======================dir NULL=====================\n");
        }
        filler(buffer, dir->dirs[i]->id, NULL, 0, 0);
    }
    for (int i = 0; i < dir->file_num; i++)
    {
        if (dir->files[i] == NULL)
        {
            printf("======================file NULL=====================\n");
        }
        filler(buffer, dir->files[i]->id, NULL, 0, 0);
    }
    return 0;
};
//--------mkdir----------
static int gfs_mkdir(const char *path, mode_t mode)
{
    printf("%s: %s\n", __FUNCTION__, path);
    struct path *s_path = split_path(path);
    struct path *dir_path = (struct path *)malloc(sizeof(struct path));
    for (int i = 0; i < s_path->split_num - 1; i++)
    {
        dir_path->split_path[i] = s_path->split_path[i];
    }
    char *dir_name = s_path->split_path[s_path->split_num - 1];
    struct memfs_dir *dir = find_dir(dir_path);
    if (dir == NULL)
    {
        return -ENOENT;
    }
    else if (find_dir(s_path) != NULL)
    {
        return -EEXIST;
    }
    else
    {
        struct memfs_dir *new_dir = (struct memfs_dir *)malloc(sizeof(struct memfs_dir));
        init_memfs_dir(new_dir, dir_name);
        add_dir_to_dir(dir, new_dir);
        return 0;
    }
}
//--------touch----------
static int gfs_create(const char *path, mode_t mode, struct fuse_file_info *fi)
{
    printf("%s: %s\n", __FUNCTION__, path);
    struct path *s_path = split_path(path);
    struct path *dir_path = get_parent_path(s_path);
    char *file_name = s_path->split_path[s_path->split_num - 1];
    struct memfs_dir *dir = find_dir(dir_path);
    if (dir == NULL)
    {
        return -ENOENT;
    }
    else if (find_file(s_path) != NULL)
    {
        return -EEXIST;
    }
    else
    {
        struct memfs_file *new_file = (struct memfs_file *)malloc(sizeof(struct memfs_file));
        init_memfs_file(new_file, file_name);
        add_file_to_dir(dir, new_file);
        return 0;
    }
}

static int utimens(const char *path, const struct timespec tv[2], struct fuse_file_info *fi)
{
    printf("%s: %s\n", __FUNCTION__, path);
    struct path *s_path = split_path(path);
    struct memfs_file *file = find_file(s_path);
    if (file == NULL)
    {
        return -ENOENT;
    }
    else
    {
        file->fstat.st_atime = tv[0].tv_sec;
        file->fstat.st_mtime = tv[1].tv_sec;
        return 0;
    }
}

static int gfs_mknod(const char *path, mode_t mode, dev_t rdev)
{
    printf("%s: %s\n", __FUNCTION__, path);
    struct path *s_path = split_path(path);
    struct path *dir_path = get_parent_path(s_path);
    char *file_name = s_path->split_path[s_path->split_num - 1];
    struct memfs_dir *dir = find_dir(dir_path);
    if (dir == NULL)
    {
        return -ENOENT;
    }
    else if (find_file(s_path) != NULL)
    {
        return -EEXIST;
    }
    else
    {
        struct memfs_file *new_file = (struct memfs_file *)malloc(sizeof(struct memfs_file));
        init_memfs_file(new_file, file_name);
        add_file_to_dir(dir, new_file);
        return 0;
    }
}
//--------echo------------
static int gfs_write(const char *path, const char *buf, size_t size, off_t offset,
                     struct fuse_file_info *fi)
{
    printf("%s: %s\n", __FUNCTION__, path);
    printf("###########################################[write] buf = %s; size = %ld; offset = %ld \n", buf, size, offset);
    struct path *s_path = split_path(path);
    struct memfs_file *file = find_file(s_path);
    if (file == NULL)
    {
        gfs_create(path, 0, NULL);
        file = find_file(s_path);
    }
    int req_size = ((offset + size) / BLOCK_SIZE + 1) * BLOCK_SIZE;
    if (file->content_size == 0)
    {
        printf("###########################################[write] case1 \n");
        file->content = (char *)malloc(req_size);
        file->content_size = req_size;
        memcpy(file->content + offset, buf, size);
        printf("########(1)###### %s\n", file->content);
    }
    else if (req_size > file->content_size)
    {
        printf("###########################################[write] case2 \n");
        file->content = (char *)realloc(file->content, req_size);
        file->content_size = req_size;
        memcpy(file->content + offset, buf, size);
        printf("########(2)###### %s\n", file->content);
    }
    else
    {
        printf("###########################################[write] case3 \n");
        memcpy(file->content + offset, buf, size);
        printf("########(3)###### %s\n", file->content);
    }
    printf("[write] %s\n", file->id);

    return size;
}

// FLUSH,GETXATTR

static int gfs_flush(const char *path, struct fuse_file_info *fi)
{
    printf("%s: %s\n", __FUNCTION__, path);
    return 0;
}

static int gfs_getxattr(const char *path, const char *name, char *value, size_t size)
{
    printf("%s: %s\n", __FUNCTION__, path);
    return 0;
}

// RELEASE
static int gfs_release(const char *path, struct fuse_file_info *fi)
{
    printf("%s: %s\n", __FUNCTION__, path);
    return 0;
}
// READ
static int gfs_read(const char *path, char *buf, size_t size, off_t offset,
                    struct fuse_file_info *fi)
{
    printf("%s: %s\n", __FUNCTION__, path);
    printf("###########################################[read] buf = %s; size = %ld; offset = %ld \n", buf, size, offset);
    struct path *s_path = split_path(path);
    struct memfs_file *file = find_file(s_path);
    if (file == NULL)
    {
        return -ENOENT;
    }
    else
    {
        printf("[read] ###### %s \n", file->content);
        int file_size = file->content_size;
        if (offset >= file_size)
        {
            return 0;
        }
        int rsize = (size < file_size - offset) ? size : (file_size - offset);
       
        memcpy(buf, file->content + offset, rsize);
        printf("[read]exit ############ %s\n", buf);
        return rsize;
    }
}

//----------rm-----------
static int gfs_unlink(const char *path)
{
    printf("%s: %s\n", __FUNCTION__, path);
    struct path *s_path = split_path(path);
    struct path *dir_path = get_parent_path(s_path);
    struct memfs_dir *dir = find_dir(dir_path);
    if (dir == NULL || find_file(s_path) == NULL)
    {
        return -ENOENT;
    }
    else
    {
        struct memfs_file *file = find_file(s_path);
        remove_file_from_dir(dir, file);
        return 0;
    }
}

//----------rmdir--------
static int gfs_rmdir(const char *path)
{
    printf("%s: %s\n", __FUNCTION__, path);
    struct path *self_path = split_path(path);
    struct path *parent_path = (struct path *)malloc(sizeof(struct path));
    for (int i = 0; i < self_path->split_num - 1; i++)
    {
        parent_path->split_path[i] = self_path->split_path[i];
    }
    struct memfs_dir *parent_dir = find_dir(parent_path);
    struct memfs_dir *self_dir = find_dir(self_path);
    if (parent_dir == NULL || self_dir == NULL)
    {
        return -ENOENT;
    }
    else
    {
        clear_file_of_dir(self_dir);
        clear_dir_of_dir(self_dir);
        remove_dir_from_dir(parent_dir, self_dir);
        return 0;
    }
}

static struct fuse_operations memfs_oper = {
    .getattr = gfs_getattr,
    .access = gfs_access,
    .readdir = gfs_readdir,
    .mkdir = gfs_mkdir,
    .create = gfs_create,
    .mknod = gfs_mknod,
    .write = gfs_write,
    .flush = gfs_flush,
    .getxattr = gfs_getxattr,
    .release = gfs_release,
    .read = gfs_read,
    .unlink = gfs_unlink,
    .utimens = utimens,
    .rmdir = gfs_rmdir,
};

int main(int argc, char *argv[])
{
    int ret;
    struct fuse_args args = FUSE_ARGS_INIT(argc, argv);

    gfs = (struct memfs *)malloc(sizeof(struct memfs));
    init_memfs(gfs);

    ret = fuse_main(args.argc, args.argv, &memfs_oper, NULL);
    fuse_opt_free_args(&args);
    return ret;
}
