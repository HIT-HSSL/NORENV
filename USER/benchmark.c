#include "nfvfs.h"
#include "lfs.h"
#include "delay.h"

void basic_storage_test(const char *fsname, int loop)
{
    int fd;
    int i, ret;
    struct nfvfs *fs;
    int boot_count = 0;
    
    fs = get_nfvfs(fsname);
    if (!fs) {
        printf("\r\nFailed to get %s, making sure you have register it\r\n", fsname);
        return;
    }

    for (i = 0; i < loop; i++) {
        nfvfs_mount(fs);
        
        fd = nfvfs_open(fs, "test.txt", O_RDWR | O_CREAT, S_ISREG);
        if (fd < 0) {
            printf("open file failed: %d\r\n", fd);
        } else {
            printf("open file success: %d\r\n", fd);
        }

        ret = nfvfs_read(fs, fd, &boot_count, sizeof(boot_count));
        if (ret < 0) {
            printf("read file failed: %d\r\n", ret);
        } else {
            printf("read file success: %d\r\n", fd);
        }
        
        boot_count++;
        
        ret = nfvfs_lseek(fs, fd, 0, NFVFS_SEEK_SET);
        if (ret < 0) {
            printf("lseek file failed: %d\r\n", ret);
        } else {
            printf("seek file success: %d\r\n", fd);
        }

        ret = nfvfs_write(fs, fd, &boot_count, sizeof(boot_count));
        if (ret < 0) {
            printf("write file failed: %d\r\n", ret);
        } else {
            printf("write file success: %d\r\n", fd);
        }
        
        nfvfs_close(fs, fd);
        nfvfs_umount(fs);

        printf("boot_count: %d\r\n", boot_count);

        delay_ms(2000);
    }
}
