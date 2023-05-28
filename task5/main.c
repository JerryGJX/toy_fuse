#include <fcntl.h>
#include <sys/types.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
int main()
{

    pid_t pid;
    // OPEN FILES
    int fd;
    fd = open("test.txt", O_RDWR | O_CREAT | O_TRUNC);
    if (fd == -1)
    {
        /* code */
        fprintf(stderr, "fail on file open %s\n", "test.txt");
    }
    // write 'hello fcntl!' to file

    /* code */
    char *str = "hello fcntl!";
    int write_bytes = write(fd, str, 12);
    if (write_bytes == -1)
    {
        /* code */
        fprintf(stderr, "fail on file write %s\n", "test.txt");
    }

    // DUPLICATE FD

    /* code */
    int fd2 = fcntl(fd, F_DUPFD, 0);
    if (fd2 == -1)
    {
        /* code */
        fprintf(stderr, "fail on file dup %s\n", "test.txt");
    }
    // FORK

    pid = fork();

    if (pid < 0)
    {
        // FAILS
        printf("error in fork");
        return 1;
    }

    struct flock fl;
    fl.l_type = F_WRLCK;
    fl.l_whence = SEEK_SET;
    fl.l_start = 0;
    fl.l_len = 0;
    fl.l_pid = getpid();

    if (pid > 0)
    {
        // PARENT PROCESS
        // set the lock
        if (flock(fd, LOCK_EX) == 0)
        {
            printf("[parent] the file was locked\n");
        }
        else
        {
            printf("[parent] the file was not locked\n");
        }

        // append 'b'
        write(fd, "b", 1);
        // unlock
        if(flock(fd, LOCK_UN)==0){
            printf("[parent] the file was unlocked\n");
        }
        else{
            printf("[parent] the file was not unlocked\n");
        };
        sleep(3);
        // printf("%s", str); the feedback should be 'hello fcntl!ba'
        exit(0);
    }
    else
    {
        // CHILD PROCESS
        sleep(2);
        // get the lock
        if (flock(fd2, LOCK_EX) == 0)
        {
            printf("[child] the file was locked\n");
        }
        else
        {
            printf("[child] the file was not locked\n");
        }

        // append 'a'
        write(fd2, "a", 1);

        exit(0);
    }
    close(fd);
    return 0;
}