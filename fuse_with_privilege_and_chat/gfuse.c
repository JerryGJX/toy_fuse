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

#define MAX_NAME_LEN 255
#define MAX_FILE_NUM 1024
#define MAX_DIR_NUM 1024
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
    struct memfs_dir *dirs[MAX_DIR_NUM];
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
    if (dir->dir_num >= MAX_DIR_NUM)
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
    memfs->root->dstat.st_mode = S_IFDIR | 0777;
    memfs->root->dstat.st_atime = time(NULL);
    memfs->root->dstat.st_mtime = time(NULL);
    memfs->root->dstat.st_ctime = time(NULL);
    memfs->root->dstat.st_uid = getuid();
    memfs->root->dstat.st_gid = getgid();
}

struct path
{
    char *split_path[MAX_NAME_LEN];
    int split_num;
};

struct path *split_path(const char *raw_path)
{
    char *raw_path_copy = (char *)malloc(sizeof(char) * MAX_NAME_LEN);
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

char *merge_path(struct path *path)
{
    char *raw_path = (char *)malloc(sizeof(char) * MAX_NAME_LEN);
    strcpy(raw_path, "/");
    for (int i = 0; i < path->split_num; i++)
    {
        strcat(raw_path, path->split_path[i]);
        strcat(raw_path, "/");
    }
    return raw_path;
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

    struct path *s_path = split_path(path);
    struct memfs_dir *dir = find_dir(s_path);
    printf("[getattr]: ====\n");
    struct memfs_file *file = find_file(s_path);
    printf("[getattr]: ####\n");
    if (dir != NULL)
    {
        stbuf->st_mode = S_IFDIR | dir->dstat.st_mode;
        stbuf->st_nlink = 2;
        stbuf->st_size = 0;
        stbuf->st_uid = dir->dstat.st_uid;
        stbuf->st_gid = dir->dstat.st_gid;
        stbuf->st_atime = dir->dstat.st_atime;
        stbuf->st_mtime = dir->dstat.st_mtime;
    }
    else if (file != NULL)
    {
        stbuf->st_mode = S_IFREG | file->fstat.st_mode;
        stbuf->st_nlink = 1;
        stbuf->st_size = file->content_size;
        stbuf->st_uid = file->fstat.st_uid;
        stbuf->st_gid = file->fstat.st_gid;
        stbuf->st_atime = file->fstat.st_atime;
        stbuf->st_mtime = file->fstat.st_mtime;
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
    printf("[access]: entered, path = %s\n", path);
    struct path *s_path = split_path(path);
    struct memfs_dir *dir = find_dir(s_path);
    struct memfs_file *file = find_file(s_path);
    if (dir != NULL)
    {
        if ((dir->dstat.st_mode & mask) == mask)
        {
            return 0;
        }
        else
        {
            return -EACCES;
        }
    }
    else if (file != NULL)
    {
        if ((file->fstat.st_mode & mask) == mask)
        {
            return 0;
        }
        else
        {
            return -EACCES;
        }
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
        new_dir->dstat.st_mode = S_IFDIR | mode;
        struct fuse_context *cxt = fuse_get_context();
        if (cxt)
        {
            new_dir->dstat.st_uid = cxt->uid;
            new_dir->dstat.st_gid = cxt->gid;
        }
        else
        {
            new_dir->dstat.st_uid = getuid();
            new_dir->dstat.st_gid = getgid();
        }

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
        new_file->fstat.st_mode = S_IFREG | mode;
        struct fuse_context *cxt = fuse_get_context();
        if (cxt)
        {
            new_file->fstat.st_uid = cxt->uid;
            new_file->fstat.st_gid = cxt->gid;
        }
        else
        {
            new_file->fstat.st_uid = getuid();
            new_file->fstat.st_gid = getgid();
        }
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
        new_file->fstat.st_mode = S_IFREG | mode;
        struct fuse_context *cxt = fuse_get_context();
        if (cxt)
        {
            new_file->fstat.st_uid = cxt->uid;
            new_file->fstat.st_gid = cxt->gid;
        }
        else
        {
            new_file->fstat.st_uid = getuid();
            new_file->fstat.st_gid = getgid();
        }
        return 0;
    }
}
//--------echo------------
static int real_write(const char *path, const char *buf, size_t size, off_t offset,
                      struct fuse_file_info *fi)
{
    struct path *s_path = split_path(path);
    struct memfs_file *file = find_file(s_path);

    if (file == NULL)
    {
        gfs_create(path, 0755, NULL);
        file = find_file(s_path);
    }
    int req_size = ((offset + size) / BLOCK_SIZE + 1) * BLOCK_SIZE;
    if (file->content_size == 0)
    {
        file->content = (char *)malloc(req_size);
        file->content_size = req_size;
        memcpy(file->content + offset, buf, size);
    }
    else if (req_size > file->content_size)
    {
        file->content = (char *)realloc(file->content, req_size);
        file->content_size = req_size;
        memcpy(file->content + offset, buf, size);
    }
    else
    {
        memcpy(file->content + offset, buf, size);
    }
    printf("[write] %s\n", file->id);

    return size;
}

static int gfs_write(const char *path, const char *buf, size_t size, off_t offset,
                     struct fuse_file_info *fi)
{
    printf("%s: %s\n", __FUNCTION__, path);
    // printf("###########################################[write] buf = %s; size = %ld; offset = %ld \n", buf, size, offset);
    struct path *s_path = split_path(path);
    printf("###########################################[write] s_path = %s\n", s_path->split_path[0]);
    if (s_path->split_num == 2 && strcmp(s_path->split_path[0], "bot1") == 0)
    {
        printf("###########################################[write] in 1\n");
        struct path *bot2_path = (struct path *)malloc(sizeof(struct path));
        bot2_path->split_num = 1;
        bot2_path->split_path[0] = "bot2";
        // bot2_path->split_path[1] = s_path->split_path[1];
        if (find_dir(bot2_path) == NULL)
        {
            printf("bot2 not found\n");
            return -ENOENT;
        }
        struct path *bot2_file_path = (struct path *)malloc(sizeof(struct path));
        bot2_file_path->split_num = 2;
        bot2_file_path->split_path[0] = "bot2";
        bot2_file_path->split_path[1] = s_path->split_path[1];
        char *new_path = merge_path(bot2_file_path);
        return real_write(new_path, buf, size, offset, fi);
    }
    else if (s_path->split_num == 2 && strcmp(s_path->split_path[0], "bot2") == 0)
    {
        printf("###########################################[write] in 2\n");
        struct path *bot1_path = (struct path *)malloc(sizeof(struct path));
        bot1_path->split_num = 1;
        bot1_path->split_path[0] = "bot1";
        // bot1_path->split_path[1] = s_path->split_path[1];
        if (find_dir(bot1_path) == NULL)
        {
            printf("bot1 not found\n");
            return -ENOENT;
        }
        struct path *bot1_file_path = (struct path *)malloc(sizeof(struct path));
        bot1_file_path->split_num = 2;
        bot1_file_path->split_path[0] = "bot1";
        bot1_file_path->split_path[1] = s_path->split_path[1];
        char *new_path = merge_path(bot1_file_path);
        return real_write(new_path, buf, size, offset, fi);
    }

    return real_write(path, buf, size, offset, fi);
}

// FLUSH,GETXATTR

static int gfs_flush(const char *path, struct fuse_file_info *fi)
{
    printf("%s: %s\n", __FUNCTION__, path);
    return 0;
}

// static int gfs_getxattr(const char *path, const char *name, char *value, size_t size)
// {
//     printf("%s: %s\n", __FUNCTION__, path);
//     return 0;
// }

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

static int gfs_chmod(const char *path, mode_t mode, struct fuse_file_info *fi)
{
    printf("%s: %s\n", __FUNCTION__, path);
    struct path *s_path = split_path(path);
    struct memfs_dir *dir = find_dir(s_path);
    struct memfs_file *file = find_file(s_path);
    if (file != NULL)
    {
        file->fstat.st_mode = S_IFREG | mode;
        return 0;
    }
    else if (dir != NULL)
    {
        dir->dstat.st_mode = S_IFDIR | mode;
        return 0;
    }
    else
    {
        return -ENOENT;
    }
}

static int gfs_chown(const char *path, uid_t uid, gid_t gid, struct fuse_file_info *fi)
{
    printf("%s: %s\n", __FUNCTION__, path);
    struct path *s_path = split_path(path);
    struct memfs_dir *dir = find_dir(s_path);
    struct memfs_file *file = find_file(s_path);
    if (file != NULL)
    {
        file->fstat.st_uid = uid;
        file->fstat.st_gid = gid;
        return 0;
    }
    else if (dir != NULL)
    {
        dir->dstat.st_uid = uid;
        dir->dstat.st_gid = gid;
        return 0;
    }
    else
    {
        return -ENOENT;
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
    // .getxattr = gfs_getxattr,
    .release = gfs_release,
    .read = gfs_read,
    .unlink = gfs_unlink,
    .utimens = utimens,
    .rmdir = gfs_rmdir,
    .chmod = gfs_chmod,
    .chown = gfs_chown,
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
