#include "nfvfs.h"
#include "lfs.h"
#include "delay.h"
#include "FreeRTOS.h"
#include "task.h"
#include "w25qxx.h"
#include "benchmark.h"

#include "ff.h"
#include "exfuns.h"

// It's used to checkout the number of files in busybox
int cnt = 0;

uint32_t total_time = 0;
uint32_t total_mount = 0;
uint32_t total_unmount = 0;
uint32_t total_open = 0;
uint32_t total_write = 0;
uint32_t total_read = 0;
uint32_t total_close = 0;
uint32_t total_lseek = 0;
uint32_t total_readdir = 0;
uint32_t total_fssize = 0;
uint32_t total_delete = 0;

void total_time_reset()
{
  total_time = 0;
  total_mount = 0;
  total_unmount = 0;
  total_open = 0;
  total_write = 0;
  total_read = 0;
  total_close = 0;
  total_lseek = 0;
  total_readdir = 0;
  total_fssize = 0;
  total_delete = 0;
}

void total_time_print()
{
  printf("\r\n\r\n\r\n");
  printf("total time is         	%u\r\n", total_time);
  printf("total mount time is   	%u\r\n", total_mount);
  printf("total unmount time is 	%u\r\n", total_unmount);
  printf("total open time is    	%u\r\n", total_open);
  printf("total write time is   	%u\r\n", total_write);
  printf("total read time is    	%u\r\n", total_read);
  printf("total close time is   	%u\r\n", total_close);
  printf("total lseek time is   	%u\r\n", total_lseek);
  printf("total readdir time is   %u\r\n", total_readdir);
  printf("total fs size time is   %u\r\n", total_fssize);
  printf("total delete time is    %u\r\n", total_delete);
  printf("\r\n\r\n\r\n");
}

/**
 * -----------------------------------------------------------------------------------------------------------------------------------------------
 * -------------------------------------------------------------    Raw IO speed    --------------------------------------------------------------
 * -----------------------------------------------------------------------------------------------------------------------------------------------
 */

void raw_sctor_test(int loop, uint32_t size)
{
  uint32_t start = 0;
  uint32_t end = 0;
  uint8_t temp_buffer[size];

  start = (uint32_t)xTaskGetTickCount();
  W25QXX_Erase_Sector(8190);
  end = (uint32_t)xTaskGetTickCount();
  printf("Erase sector's time is %d\r\n", end - start);

  start = (uint32_t)xTaskGetTickCount();
  for (int i = 0; i < loop; i++)
    W25QXX_Read(temp_buffer, 20 * 4096, size);
  end = (uint32_t)xTaskGetTickCount();
  printf("Loop Read bytes time is %d\r\n", end - start);

  memset(temp_buffer, -1, size);
  start = (uint32_t)xTaskGetTickCount();
  for (int i = 0; i < loop; i++)
    W25QXX_Write(temp_buffer, 20 * 4096, size);
  end = (uint32_t)xTaskGetTickCount();
  printf("Loop Write bytes time is %d\r\n", end - start);

  memset(temp_buffer, -1, size);
  start = (uint32_t)xTaskGetTickCount();
  for (int i = 0; i < loop; i++)
    W25QXX_Write_NoCheck(temp_buffer, 20 * 4096, size);
  end = (uint32_t)xTaskGetTickCount();
  printf("Loop Write bytes time is %d\r\n", end - start);

  memset(temp_buffer, -1, size);
  start = (uint32_t)xTaskGetTickCount();
  for (int i = 0; i < loop; i++)
    W25QXX_Write(temp_buffer, 20 * 4096, size);
  end = (uint32_t)xTaskGetTickCount();
  printf("Loop Write bytes with data time is %d\r\n", end - start);
}

/**
 * -----------------------------------------------------------------------------------------------------------------------------------------------
 * -------------------------------------------------------------    Basic IO Operations    --------------------------------------------------------------
 * -----------------------------------------------------------------------------------------------------------------------------------------------
 */

void raw_mount(struct nfvfs *fs)
{
  uint32_t start = (uint32_t)xTaskGetTickCount();
  nfvfs_mount(fs);
  uint32_t end = (uint32_t)xTaskGetTickCount();
  // printf("Mount time is %u, %u, %u\r\n", end - start, start, end);
  total_time += end - start;
  total_mount += end - start;
  return;
}

int raw_open(struct nfvfs *fs, char *path, int flags, int type)
{
  uint32_t start = (uint32_t)xTaskGetTickCount();
  int fd = nfvfs_open(fs, path, flags, type);
  if (fd < 0)
  {
    printf("open file failed: %d\r\n", fd);
  }
  else
  {
    // printf("open file success: %d\r\n", fd);
  }
  uint32_t end = (uint32_t)xTaskGetTickCount();
  // printf("Open file time is %u, %u, %u\r\n", end - start, start, end);
  total_time += end - start;
  total_open += end - start;
  return fd;
}

void raw_write(struct nfvfs *fs, int fd, int size)
{
  char test_buffer[size > 4096 ? 4096 : size];
  // char test_buffer[4096];

  uint32_t start = (uint32_t)xTaskGetTickCount();
  while (size > 0)
  {
    int min = (size > 4096) ? 4096 : size;
    int ret = nfvfs_write(fs, fd, test_buffer, min);
    if (ret < 0)
    {
      printf("write file failed: %d\r\n", ret);
    }
    else
    {
      // printf("write file success: %d\r\n", fd);
    }
    size -= min;
  }
  // if (fs->name[0] != 'e' && fs->name[1] != 'H')
  // nfvfs_fsync(fs, fd);
  uint32_t end = (uint32_t)xTaskGetTickCount();
  // printf("write file time is %u, %u, %u\r\n", end - start, start, end);
  total_time += end - start;
  total_write += end - start;
  return;
}

void raw_write_fsync(struct nfvfs *fs, int fd, int size)
{
  char test_buffer[size > 4096 ? 4096 : size];
  // char test_buffer[4096];

  uint32_t start = (uint32_t)xTaskGetTickCount();
  while (size > 0)
  {
    int min = (size > 4096) ? 4096 : size;
    int ret = nfvfs_write(fs, fd, test_buffer, min);
    if (ret < 0)
    {
      printf("write file failed: %d\r\n", ret);
    }
    else
    {
      // printf("write file success: %d\r\n", fd);
    }
    size -= min;
  }
  // if (fs->name[0] != 'e' && fs->name[1] != 'H')
  nfvfs_fsync(fs, fd);
  uint32_t end = (uint32_t)xTaskGetTickCount();
  // printf("write file time is %u, %u, %u\r\n", end - start, start, end);
  total_time += end - start;
  total_write += end - start;
  return;
}

void raw_lseek(struct nfvfs *fs, int fd, int off)
{
  uint32_t start = (uint32_t)xTaskGetTickCount();
  int ret = nfvfs_lseek(fs, fd, off, NFVFS_SEEK_SET);
  if (ret < 0)
  {
    printf("lseek file failed: %d\r\n", ret);
  }
  else
  {
    // printf("seek file success: %d\r\n", fd);
  }
  // if (fs->name[0] != 'e' && fs->name[1] != 'H')
  // nfvfs_fsync(fs, fd);
  uint32_t end = (uint32_t)xTaskGetTickCount();
  // printf("lseek file time is %u, %u, %u\r\n", end - start, start, end);
  total_time += end - start;
  total_lseek += end - start;
  return;
}

void raw_lseek_fsync(struct nfvfs *fs, int fd, int off)
{
  uint32_t start = (uint32_t)xTaskGetTickCount();
  int ret = nfvfs_lseek(fs, fd, off, NFVFS_SEEK_SET);
  if (ret < 0)
  {
    printf("lseek file failed: %d\r\n", ret);
  }
  else
  {
    // printf("seek file success: %d\r\n", fd);
  }
  // if (fs->name[0] != 'e' && fs->name[1] != 'H')
  nfvfs_fsync(fs, fd);
  uint32_t end = (uint32_t)xTaskGetTickCount();
  // printf("lseek file time is %u, %u, %u\r\n", end - start, start, end);
  total_time += end - start;
  total_lseek += end - start;
  return;
}

void raw_read(struct nfvfs *fs, int fd, int size)
{
  char test_buffer[size > 4096 ? 4096 : size];
  // char test_buffer[4096];

  uint32_t start = (uint32_t)xTaskGetTickCount();
  while (size > 0)
  {
    int min = (size > 4096) ? 4096 : size;
    int ret = nfvfs_read(fs, fd, test_buffer, min);
    if (ret < 0)
    {
      printf("read file failed: %d\r\n", ret);
    }
    else
    {
      // printf("read file success: %d\r\n", fd);
    }
    size -= min;
  }
  // if (fs->name[0] != 'e' && fs->name[1] != 'H')
  // nfvfs_fsync(fs, fd);
  uint32_t end = (uint32_t)xTaskGetTickCount();
  // printf("read file time is %u, %u, %u\r\n", end - start, start, end);
  total_time += end - start;
  total_read += end - start;
  return;
}

void raw_read_fsync(struct nfvfs *fs, int fd, int size)
{
  char test_buffer[size > 4096 ? 4096 : size];
  // char test_buffer[4096];

  uint32_t start = (uint32_t)xTaskGetTickCount();
  while (size > 0)
  {
    int min = (size > 4096) ? 4096 : size;
    int ret = nfvfs_read(fs, fd, test_buffer, min);
    if (ret < 0)
    {
      printf("read file failed: %d\r\n", ret);
    }
    else
    {
      // printf("read file success: %d\r\n", fd);
    }
    size -= min;
  }
  // if (fs->name[0] != 'e' && fs->name[1] != 'H')
  nfvfs_fsync(fs, fd);
  uint32_t end = (uint32_t)xTaskGetTickCount();
  // printf("read file time is %u, %u, %u\r\n", end - start, start, end);
  total_time += end - start;
  total_read += end - start;
  return;
}

void raw_close(struct nfvfs *fs, int fd)
{
  uint32_t start = (uint32_t)xTaskGetTickCount();
  int err = nfvfs_close(fs, fd);
  if (err < 0)
  {
    printf("close err, err is %d\r\n", err);
  }
  uint32_t end = (uint32_t)xTaskGetTickCount();
  // printf("close file time is %u, %u, %u\r\n", end - start, start, end);
  total_time += end - start;
  total_close += end - start;
  return;
}

void raw_unmount(struct nfvfs *fs)
{
  uint32_t start = (uint32_t)xTaskGetTickCount();
  int err = nfvfs_umount(fs);
  if (err < 0)
  {
    printf("unmount err, err is %d\r\n", err);
  }
  uint32_t end = (uint32_t)xTaskGetTickCount();
  // printf("umount time is %u, %u, %u\r\n", end - start, start, end);
  total_time += end - start;
  total_unmount += end - start;
  return;
}

void raw_fssize(struct nfvfs *fs)
{
  uint32_t start = (uint32_t)xTaskGetTickCount();
  int fs_size = nfvfs_fssize(fs);
  uint32_t end = (uint32_t)xTaskGetTickCount();
  printf("fs size is %d\r\n", fs_size);
  // printf("cal fs_size time is %u, %u, %u\r\n", end - start, start, end);
  total_time += end - start;
  total_fssize += end - start;
}

void raw_readdir(struct nfvfs *fs, int fd, struct nfvfs_dentry *info)
{
  int err = 0;
  uint32_t start = (uint32_t)xTaskGetTickCount();
  err = nfvfs_readdir(fs, fd, info);
  if (err < 0)
  {
    printf("read dir entry error, error is %d\r\n", err);
  }
  uint32_t end = (uint32_t)xTaskGetTickCount();
  // printf("Read dir time is %u, %u, %u\r\n", end - start, start, end);
  total_time += end - start;
  total_readdir += end - start;
}

void raw_delete(struct nfvfs *fs, int fd, char *path, int mode, int type)
{
  int err = 0;
  uint32_t start = (uint32_t)xTaskGetTickCount();
  err = nfvfs_remove(fs, fd, path, mode, type);
  if (err < 0)
  {
    printf("remove path %s error, error is %d\r\n", path, err);
  }
  uint32_t end = (uint32_t)xTaskGetTickCount();
  // printf("delete time is %u, %u, %u\r\n", end - start, start, end);
  total_time += end - start;
  total_delete += end - start;
}

void raw_basic_test(struct nfvfs *fs, char *path, int len, int loop)
{
  int fd;
  printf("\r\n\r\n\r\n");
  printf("raw_basic_test\r\n");
  fd = raw_open(fs, path, O_RDWR | O_CREAT, S_ISREG);

  uint32_t start = (uint32_t)xTaskGetTickCount();
  for (int i = 0; i < loop; i++)
  {
    raw_lseek_fsync(fs, fd, 0);
    raw_write_fsync(fs, fd, len);
  }
  uint32_t end = (uint32_t)xTaskGetTickCount();
  printf("Sequential wirte time is %u\r\n", end - start);

  start = (uint32_t)xTaskGetTickCount();
  for (int i = 0; i < loop; i++)
  {
    raw_lseek_fsync(fs, fd, 0);
    raw_read_fsync(fs, fd, len);
  }
  end = (uint32_t)xTaskGetTickCount();
  printf("Sequential read time is %u\r\n", end - start);

  raw_close(fs, fd);
}

// Sequential and randow r/w test.
void raw_sr_test(struct nfvfs *fs, char *path, int slen, int off, int rlen)
{
  int fd;
  printf("raw_sr_test\r\n");
  fd = raw_open(fs, path, O_RDWR | O_CREAT, S_ISREG);

  uint32_t start = (uint32_t)xTaskGetTickCount();
  raw_lseek(fs, fd, 0);
  raw_write(fs, fd, slen);
  uint32_t end = (uint32_t)xTaskGetTickCount();
  printf("Sequential large wirte time is %u\r\n", end - start);

  start = (uint32_t)xTaskGetTickCount();
  raw_lseek(fs, fd, 0);
  raw_read(fs, fd, slen);
  end = (uint32_t)xTaskGetTickCount();
  printf("Sequential large read time is %u\r\n", end - start);

  raw_lseek(fs, fd, slen - 1);

  start = (uint32_t)xTaskGetTickCount();
  raw_lseek(fs, fd, off);
  raw_write(fs, fd, rlen);
  end = (uint32_t)xTaskGetTickCount();
  printf("Random wirte time is %u\r\n", end - start);

  raw_lseek(fs, fd, slen - 1);

  start = (uint32_t)xTaskGetTickCount();
  raw_lseek(fs, fd, off);
  raw_read(fs, fd, rlen);
  end = (uint32_t)xTaskGetTickCount();
  printf("Random read time is %u\r\n", end - start);

  // start = (uint32_t)xTaskGetTickCount();
  raw_close(fs, fd);
  // end = (uint32_t)xTaskGetTickCount();
  // printf("Close file time is %u\r\n", end - start);
}

void raw_total_sr_test(struct nfvfs *fs, char *path, int tail, char *my_cnt, int off)
{
  printf("\r\n\r\n\r\n");
  printf("raw_total_sr_test\r\n");
  // path[tail] = *my_cnt;
  // raw_sr_test(fs, path, 2 * 1024 * 1024, off, 8);
  // *my_cnt += 1;

  path[tail] = *my_cnt;
  raw_sr_test(fs, path, 2 * 1024 * 1024, off, 1024);
  *my_cnt += 1;

  // path[tail] = *my_cnt;
  // raw_sr_test(fs, path, 40960, off, 4096);
  // *my_cnt += 1;
}

/**
 * -----------------------------------------------------------------------------------------------------------------------------------------------
 * -------------------------------------------------------------    Basic fs test    -------------------------------------------------------------
 * -----------------------------------------------------------------------------------------------------------------------------------------------
 */

void file_prog_test(const char *fsname, int loop)
{
  struct nfvfs *fs;

  printf("begin\r\n");
  fs = get_nfvfs(fsname);
  if (!fs)
  {
    printf("\r\nFailed to get %s, making sure you have register it\r\n", fsname);
    return;
  }

  uint32_t total_start = (uint32_t)xTaskGetTickCount();

  raw_mount(fs);
  char path[64];
  memset(path, 0, 64);
  if (fsname[0] == 'e' && fsname[1] == 'H')
    strcpy(path, "1:/test1.txt");
  else
    strcpy(path, "/test1.txt");
  int tail = strlen(path) - 1;
  char my_cnt1 = 'A';

  int size = 8;
  nor_flash_message_reset();
  for (int i = 0; i < 5; i++)
  {
    path[tail] = my_cnt1++;
    raw_basic_test(fs, path, size, loop);
    size = size * 4;
  }
  nor_flash_message_print();

  nor_flash_message_reset();
  size = 8 * 1024;
  for (int i = 0; i < 5; i++)
  {
    path[tail] = my_cnt1++;
    // How to circulate?
    raw_basic_test(fs, path, size, 1);
    size = size * 4;
  }
  nor_flash_message_print();

  // int rate = 0;
  // for (int i = 0; i < 5; i++)
  // {
  //   raw_total_sr_test(fs, path, tail, &my_cnt, 512 * 1024 * rate);
  //   rate += 1;
  // }

  printf("\r\n\r\n\r\n");
  raw_fssize(fs);
  raw_unmount(fs);

  uint32_t total_end = (uint32_t)xTaskGetTickCount();
  printf("\r\n\r\n\r\n");
  printf("Total r/w time is %u\r\n", total_end - total_start);
}

/**
 * -----------------------------------------------------------------------------------------------------------------------------------------------
 * -------------------------------------------------------------    Busybox test    --------------------------------------------------------------
 * -----------------------------------------------------------------------------------------------------------------------------------------------
 */

FRESULT write_busybox_test(
    char *path, /* Start node to be scanned (***also used as work area***) */
    struct nfvfs *dst_fs,
    const char *fsname)
{
  FRESULT res;
  DIR dir;
  // FIL file;
  UINT i;
  static FILINFO fno;

  char dst_path[256];
  memset(dst_path, 0, 256);

  // Debug
  // For eHNFFS
  //  strcpy(dst_path, path);
  //  dst_path[0] = '1';

  // For normal
  strcpy(dst_path, &path[2]);

  if (fsname[0] == 'e' && fsname[1] == 'H')
  {
    strcpy(dst_path, path);
    dst_path[0] = '1';
  }
  else
    strcpy(dst_path, &path[2]);

  int dst_i = strlen(dst_path);
  int fd = raw_open(dst_fs, dst_path, LFS_O_CREAT, S_ISDIR);
  printf("dir path is %s\r\n", dst_path);

  res = f_opendir(&dir, path); /* Open the directory */
  if (res == FR_OK)
  {
    for (;;)
    {
      res = f_readdir(&dir, &fno); /* Read a directory item */
      if (res != FR_OK || fno.fname[0] == 0)
        break; /* Break on error or end of dir */
      if (fno.fattrib & AM_DIR)
      { /* It is a directory */
        i = strlen(path);
        sprintf(&path[i], "/%s", fno.fname);
        res = write_busybox_test(path, dst_fs, fsname); /* Enter the directory */
        if (res != FR_OK)
          break;
        path[i] = 0;
      }
      else
      { /* It is a file. */
        sprintf(&dst_path[dst_i], "/%s", fno.fname);
        int fd2 = raw_open(dst_fs, dst_path, O_RDWR | O_CREAT, S_ISREG);
        int size = fno.fsize;
        while (size > 0)
        {
          int len = (size > 4096) ? 4096 : size;
          size -= len;
          raw_write_fsync(dst_fs, fd2, len);
        }
        raw_close(dst_fs, fd2);
      }
    }
    f_closedir(&dir);
  }

  raw_close(dst_fs, fd);
  return res;
}

int update_busybox_test(char *path, struct nfvfs *dst_fs)
{
  int err = 0;

  int fd = raw_open(dst_fs, path, O_RDWR | O_CREAT, S_ISREG);
  int my_size = 2675216;
  // int test_size = 10 * 1024;
  // while (test_size > 0)
  // {
  //   int min = (test_size > 1024) ? 1024 : test_size;
  //   raw_lseek(dst_fs, fd, rand() % (my_size - 1024));
  //   raw_write(dst_fs, fd, min);
  //   my_size += min;
  //   test_size -= min;
  // }
  int test_size = 15 * 1024;
  while (test_size > 0)
  {
    int min = (test_size > 1024) ? 1024 : test_size;
    raw_lseek_fsync(dst_fs, fd, rand() % (my_size));
    raw_write_fsync(dst_fs, fd, min);
    my_size += min;
    test_size -= min;
  }
  raw_close(dst_fs, fd);
  return err;
}

int read_busybox_test(char *path, struct nfvfs *dst_fs)
{
  int err = 0;

  // Debug
  // printf("dir path is %s\r\n", path);

  int fd = nfvfs_open(dst_fs, path, O_APPEND, S_ISDIR);
  // int fd = raw_open(dst_fs, path, O_RDWR | O_CREAT, S_ISDIR);
  if (fd < 0)
  {
    printf("open dir error: %s, %d\r\n", path, fd);
    return err;
  }

  struct nfvfs_dentry info;
  info.name = pvPortMalloc(256);
  if (info.name == NULL)
  {
    printf("There is no enough memory\r\n");
    return LFS_ERR_NOMEM;
  }

  while (true)
  {
    raw_readdir(dst_fs, fd, &info);
    // printf("Info message %s, %d\r\n", info.name, info.type);

    if (info.type == NFVFS_TYPE_REG)
    {
      cnt++;
      // printf("%s\r\n", info.name);
    }
    else if (info.type == NFVFS_TYPE_DIR)
    {

      if (info.name[0] == '.')
        continue;

      char temp[256];
      memset(temp, 0, 256);
      strcpy(temp, path);
      int len = strlen(temp);
      temp[len] = '/';
      strcpy(&temp[len + 1], info.name);

      err = read_busybox_test(temp, dst_fs);
      if (err)
      {
        goto cleanup;
      }
    }
    else if (info.type == NFVFS_TYPE_END)
    {
      printf("file number is %d\r\n", cnt);
      goto cleanup;
    }
    else
    {
      printf("something is error in read busybox, %d\r\n", info.type);
      err = -1;
      goto cleanup;
    }
  }

cleanup:
  vPortFree(info.name);
  return err;
}

void busybox_test(const char *fsname)
{
  uint32_t start = (uint32_t)xTaskGetTickCount();

  struct nfvfs *dst_fs;
  dst_fs = get_nfvfs(fsname);
  if (!dst_fs)
  {
    printf("\r\nFailed to get %s, making sure you have register it\r\n", fsname);
    return;
  }

  char buff[256];
  FRESULT res = f_mount(fs[2], "2:", 1);
  if (res == 0X0D)
  {
    printf("Flash Disk Formatting...\r\n");
    res = f_mkfs("1:", FM_ANY, 0, fatbuf, FF_MAX_SS);
    if (res == 0)
    {
      f_setlabel((const TCHAR *)"1:ALIENTEK");
      printf("Flash Disk Formatting...\r\n");
    }
    else
      printf("Flash Disk Format Error \r\n");
    delay_ms(1000);
  }
  else if (res == FR_OK)
  {
    printf("fatfs mount success!\r\n");
  }

  printf("Write busybox test\r\n");
  nor_flash_message_reset();
  total_time_reset();
  raw_mount(dst_fs);
  strcpy(buff, "2:/busybox");
  res = write_busybox_test(buff, dst_fs, fsname);
  total_time_print();
  nor_flash_message_print();
  printf("\r\n\r\n");

  printf("Update busybox test\r\n");
  nor_flash_message_reset();
  total_time_reset();
  memset(buff, 0, 256);
  // TODO, update busybox test.
  if (fsname[0] == 'e' && fsname[1] == 'H')
    strcpy(buff, "1:/busybox/bin/busybox");
  else
    strcpy(buff, "/busybox/bin/busybox");
  int my_res = update_busybox_test(buff, dst_fs);
  if (my_res < 0)
  {
    printf("update error,%d\r\n", my_res);
  }
  total_time_print();
  nor_flash_message_print();
  printf("\r\n\r\n");

  printf("traverse busybox test\r\n");
  nor_flash_message_reset();
  total_time_reset();
  memset(buff, 0, 256);
  if (fsname[0] == 'e' && fsname[1] == 'H')
    strcpy(buff, "1:/busybox");
  else
    strcpy(buff, "/busybox");
  read_busybox_test(buff, dst_fs);
  total_time_print();
  nor_flash_message_print();

  // uint32_t start = (uint32_t)xTaskGetTickCount();
  // if (fsname[0] == 'e' && fsname[1] == 'H')
  //   strcpy(buff, "1:/busybox/bin/busybox");
  // else
  //   strcpy(buff, "/busybox/bin/busybox");
  // int fd = raw_open(dst_fs, buff, O_RDWR | O_CREAT, S_ISREG);
  // raw_lseek(dst_fs, fd, 0);
  // int my_size = 2675216;
  // while (my_size > 0)
  // {
  //   int size = (my_size > 4096) ? 4096 : my_size;
  //   raw_read(dst_fs, fd, size);
  //   my_size -= size;
  // }
  // raw_close(dst_fs, fd);
  // uint32_t end = (uint32_t)xTaskGetTickCount();
  // printf("Total read binary time is %d, %d, %d\r\n", end - start, start, end);

  // raw_fssize(dst_fs);
  raw_unmount(dst_fs);
  uint32_t end = (uint32_t)xTaskGetTickCount();
  printf("Total project time is %d, %d, %d\r\n", end - start, start, end);
}

/**
 * -----------------------------------------------------------------------------------------------------------------------------------------------
 * ----------------------------------------------------------    New operation test    -----------------------------------------------------------
 * -----------------------------------------------------------------------------------------------------------------------------------------------
 */

void new_operation_test(const char *fsname)
{
  uint32_t start = (uint32_t)xTaskGetTickCount();

  struct nfvfs *dst_fs;
  dst_fs = get_nfvfs(fsname);
  if (!dst_fs)
  {
    printf("\r\nFailed to get %s, making sure you have register it\r\n", fsname);
    return;
  }

  raw_mount(dst_fs);

  char buff[128];
  memset(buff, 0, 128);
  if (fsname[0] == 'e' && fsname[1] == 'H')
    strcpy(buff, "1:/");
  else
    strcpy(buff, "/");

  // for (int i = 0; i < 4; i++)
  // {
  //   printf("dir layer %d\r\n", i + 1);
  //   layer_test(dst_fs, buff);
  // }

  int len = strlen(buff);
  buff[len] = 'a';
  buff[len + 1] = 'a';
  int fd = raw_open(dst_fs, buff, O_RDWR | O_CREAT, S_ISREG);
  raw_write(dst_fs, fd, 12 * 1024 * 1024);
  raw_close(dst_fs, fd);

  memset(buff, 0, 128);
  if (fsname[0] == 'e' && fsname[1] == 'H')
    strcpy(buff, "1:/");
  else
    strcpy(buff, "/");
  layer_test(dst_fs, buff);
  len = strlen(buff);
  burn_test(dst_fs, buff, len);
  read_test(dst_fs, buff, len);
  remove_test(dst_fs, buff, len);

  raw_unmount(dst_fs);

  uint32_t end = (uint32_t)xTaskGetTickCount();
  printf("Total project time is %d\r\n", end - start);
}

void layer_test(struct nfvfs *dst_fs, char *path)
{
  int len = strlen(path);
  // burn_test(dst_fs, path, len);
  // read_test(dst_fs, path, len);
  // // update_test(dst_fs, path, len);
  // remove_test(dst_fs, path, len);
  strcpy(&path[len], "layer");
  int fd = raw_open(dst_fs, path, LFS_O_CREAT, S_ISDIR);
  len = strlen(path);
  path[len] = '/';
  raw_close(dst_fs, fd);
}

void burn_test(struct nfvfs *dst_fs, char *path, int len)
{
  char a = 'a';
  char b = 'a';
  int fd;

  printf("\r\n\r\n\r\ncollect test begin\r\n");
  total_time_reset();
  nor_flash_message_reset();
  uint32_t start = (uint32_t)xTaskGetTickCount();

  // uint32_t temp_begin = (uint32_t)xTaskGetTickCount();
  int read_size = 0;
  for (int i = 0; i < 10; i++)
  {
    read_size += 8;
    path[len] = a;
    if (i == 0)
      b = 'b';
    else
      b = 'a';
    for (int j = 0; j < 10; j++)
    {
      path[len + 1] = b;
      // printf("Before path is %s\r\n", path);
      fd = raw_open(dst_fs, path, O_RDWR | O_CREAT, S_ISREG);
      // printf("After path is %s\r\n", path);
      raw_write_fsync(dst_fs, fd, read_size);
      // raw_lseek(dst_fs, fd, 0);
      // raw_read(dst_fs, fd, 4);
      raw_close(dst_fs, fd);
      b++;
    }
    a++;
  }

  uint32_t end = (uint32_t)xTaskGetTickCount();
  printf("Test collect time is %d\r\n", end - start);
  total_time_print();
  nor_flash_message_print();
}

void read_test(struct nfvfs *dst_fs, char *path, int len)
{
  printf("\r\n\r\n\r\nread test begin\r\n");
  nor_flash_message_reset();
  total_time_reset();
  uint32_t start = (uint32_t)xTaskGetTickCount();

  char a = 'a';
  char b = 'a';
  int fd;

  // uint32_t temp_begin = (uint32_t)xTaskGetTickCount();
  int read_size = 0;
  for (int i = 0; i < 10; i++)
  {
    read_size += 8;
    path[len] = a;
    if (i == 0)
      b = 'b';
    else
      b = 'a';
    for (int j = 0; j < 10; j++)
    {
      path[len + 1] = b;
      fd = raw_open(dst_fs, path, O_RDWR | O_CREAT, S_ISREG);
      raw_lseek_fsync(dst_fs, fd, 0);
      raw_read_fsync(dst_fs, fd, read_size);
      raw_close(dst_fs, fd);
      b++;

      // nfvfs_sync(dst_fs);
    }
    a++;
  }

  uint32_t end = (uint32_t)xTaskGetTickCount();
  printf("Test read time is %d\r\n", end - start);
  total_time_print();
  nor_flash_message_print();
}

void update_test(struct nfvfs *dst_fs, char *path, int len)
{
  printf("\r\n\r\n\r\nupdate test begin\r\n");
  nor_flash_message_reset();
  total_time_reset();
  uint32_t start = (uint32_t)xTaskGetTickCount();

  char a = 'a';
  char b = 'a';
  int fd;

  uint32_t temp_begin = (uint32_t)xTaskGetTickCount();
  for (int i = 0; i < 10; i++)
  {
    path[len] = a;
    b = 'a';
    for (int j = 0; j < 10; j++)
    {
      path[len + 1] = b;
      // printf("Before path is %s\r\n", path);
      fd = raw_open(dst_fs, path, O_RDWR | O_CREAT, S_ISREG);
      // printf("After path is %s\r\n", path);
      raw_write(dst_fs, fd, 4);
      raw_lseek(dst_fs, fd, 0);
      raw_read(dst_fs, fd, 4);
      raw_close(dst_fs, fd);
      b++;
    }
    a++;
  }
  uint32_t temp_end = (uint32_t)xTaskGetTickCount();
  printf("Update small file time is %d\r\n", temp_end - temp_begin);

  path[len] = a;
  path[len + 1] = b;
  fd = raw_open(dst_fs, path, O_RDWR | O_CREAT, S_ISREG);
  int file_size = 2 * 1024 * 1024;
  int test_size = 5 * 1024;
  while (test_size > 0)
  {
    int min = (test_size > 1024) ? 1024 : test_size;
    // raw_lseek(dst_fs, fd, rand() % file_size);
    raw_lseek(dst_fs, fd, rand() % (file_size - 1024));
    raw_write(dst_fs, fd, min);
    file_size += min;
    test_size -= min;
  }
  raw_close(dst_fs, fd);
  // int off = 0;
  // for (int i = 0; i < 5; i++)
  // {
  //   raw_lseek(dst_fs, fd, off);
  //   raw_write(dst_fs, fd, 50);
  //   off += 512 * 1024;
  // }
  // raw_close(dst_fs, fd);

  uint32_t end = (uint32_t)xTaskGetTickCount();
  printf("Test update time is %d\r\n", end - start);
  total_time_print();
  nor_flash_message_print();
}

void remove_test(struct nfvfs *dst_fs, char *path, int len)
{
  printf("\r\n\r\n\r\nremove test begin\r\n");
  nor_flash_message_reset();
  total_time_reset();
  uint32_t start = (uint32_t)xTaskGetTickCount();

  char a = 'a';
  char b = 'b';
  int fd;

  int type = (dst_fs->name[0] == 'l' && dst_fs->name[1] == 'i')
                 ? NFVFS_REMOVE_PATH
                 : NFVFS_REMOVE_FHANDLER;

  // uint32_t temp_begin = (uint32_t)xTaskGetTickCount();
  for (int i = 0; i < 10; i++)
  {
    path[len] = a;
    if (i == 0)
      b = 'b';
    else
      b = 'a';
    for (int j = 0; j < 10; j++)
    {
      path[len + 1] = b;
      fd = raw_open(dst_fs, path, O_RDWR | O_CREAT, S_ISREG);
      // raw_lseek(dst_fs, fd, 0);
      // raw_read(dst_fs, fd, 4);

      // TODO，Used for littlefs
      if (type == NFVFS_REMOVE_PATH)
      {
        raw_close(dst_fs, fd);
      }

      raw_delete(dst_fs, fd, path, S_ISREG, type);
      b++;

      // nfvfs_sync(dst_fs);
    }
    a++;
  }
  // uint32_t temp_end = (uint32_t)xTaskGetTickCount();
  // printf("remove small file time is %d\r\n", temp_end - temp_begin);

  uint32_t end = (uint32_t)xTaskGetTickCount();
  printf("Test remove time is %d\r\n", end - start);
  total_time_print();
  nor_flash_message_print();
}

void mount_test(const char *fsname)
{
  printf("\r\n\r\n\r\nremove test begin\r\n");

  struct nfvfs *dst_fs;
  dst_fs = get_nfvfs(fsname);
  if (!dst_fs)
  {
    printf("\r\nFailed to get %s, making sure you have register it\r\n", fsname);
    return;
  }

  uint32_t start = (uint32_t)xTaskGetTickCount();
  raw_mount(dst_fs);
  uint32_t end = (uint32_t)xTaskGetTickCount();
  printf("Test format time is %d\r\n", end - start);
  raw_unmount(dst_fs);

  start = (uint32_t)xTaskGetTickCount();
  for (int i = 0; i < 100; i++)
  {
    raw_mount(dst_fs);
    raw_unmount(dst_fs);
  }
  end = (uint32_t)xTaskGetTickCount();
  printf("Test mount & unmount time is %d\r\n", end - start);
}

void random_write_test(const char *fsname)
{
  nor_flash_message_reset();
  struct nfvfs *dst_fs;
  dst_fs = get_nfvfs(fsname);
  if (!dst_fs)
  {
    printf("\r\nFailed to get %s, making sure you have register it\r\n", fsname);
    return;
  }

  raw_mount(dst_fs);

  char path[64];
  memset(path, 0, 64);
  if (fsname[0] == 'e' && fsname[1] == 'H')
    strcpy(path, "1:/test1.txt");
  else
    strcpy(path, "/test1.txt");
  int tail = strlen(path) - 1;
  char my_cnt = 'A';

  for (int i = 0; i < 1; i++)
  {
    // printf("loop %d\r\n", i);
    int fd = raw_open(dst_fs, path, O_RDWR | O_CREAT, S_ISREG);
    if (fd < 0)
      printf("open error is %d\r\n", fd);
    my_cnt++;
    path[tail] = my_cnt;

    // uint32_t start = (uint32_t)xTaskGetTickCount();
    int file_size = 2 * 1024 * 1024;
    raw_write(dst_fs, fd, file_size);
    // uint32_t end = (uint32_t)xTaskGetTickCount();
    // printf("Big file write time is %d\r\n", end - start);

    uint32_t start = (uint32_t)xTaskGetTickCount();
    int test_size = 20 * 1024;
    while (test_size > 0)
    {
      int min = (test_size > 1024) ? 1024 : test_size;
      // raw_lseek(dst_fs, fd, rand() % file_size);
      raw_lseek(dst_fs, fd, rand() % (file_size - 1024));
      raw_write(dst_fs, fd, min);
      file_size += min;
      test_size -= min;

      if ((test_size / 1024) % 2 == 0)
      {
        uint32_t end = (uint32_t)xTaskGetTickCount();
        printf("Random write %d time is %d\r\n", 20 - test_size / 1024, end - start);
      }
    }

    raw_close(dst_fs, fd);
  }

  raw_unmount(dst_fs);
  nor_flash_message_print();
}

void random_read_test(const char *fsname)
{
  nor_flash_message_reset();
  struct nfvfs *dst_fs;
  dst_fs = get_nfvfs(fsname);
  if (!dst_fs)
  {
    printf("\r\nFailed to get %s, making sure you have register it\r\n", fsname);
    return;
  }

  raw_mount(dst_fs);

  char path[64];
  memset(path, 0, 64);
  if (fsname[0] == 'e' && fsname[1] == 'H')
    strcpy(path, "1:/test1.txt");
  else
    strcpy(path, "/test1.txt");
  int tail = strlen(path) - 1;
  char my_cnt = 'A';

  for (int i = 0; i < 1; i++)
  {
    int num = 0;
    int temp_arr[20];
    int file_size = 2 * 1024 * 1024;
    for (num = 0; num < 20; num++)
    {
      temp_arr[num] = rand() % (file_size - 1024);
    }

    // printf("loop %d\r\n", i);
    int fd = raw_open(dst_fs, path, O_RDWR | O_CREAT, S_ISREG);
    if (fd < 0)
      printf("open error is %d\r\n", fd);
    my_cnt++;
    path[tail] = my_cnt;

    // uint32_t start = (uint32_t)xTaskGetTickCount();
    raw_write(dst_fs, fd, file_size);
    // uint32_t end = (uint32_t)xTaskGetTickCount();
    // printf("Big file write time is %d\r\n", end - start);

    // start = (uint32_t)xTaskGetTickCount();
    // for (int j = 0; j < 50; j++)
    // {
    //   for (int k = 0; k < num; k++)
    //   {
    //     raw_lseek(dst_fs, fd, temp_arr[k]);
    //     raw_read(dst_fs, fd, 1024);
    //   }
    // }
    // end = (uint32_t)xTaskGetTickCount();
    // printf("Random read time is %d\r\n", end - start);

    uint32_t start = (uint32_t)xTaskGetTickCount();
    for (int k = 0; k < num; k++)
    {
      raw_lseek(dst_fs, fd, temp_arr[k]);
      raw_read(dst_fs, fd, 1024);
      if (k % 2 == 1)
      {
        uint32_t end = (uint32_t)xTaskGetTickCount();
        printf("Random read %d time is %d\r\n", k + 1, end - start);
      }
    }

    raw_close(dst_fs, fd);
  }

  raw_unmount(dst_fs);
  nor_flash_message_print();
}

/**
 * -----------------------------------------------------------------------------------------------------------------------------------------------
 * ------------------------------------------------------------    GC Module test    -------------------------------------------------------------
 * -----------------------------------------------------------------------------------------------------------------------------------------------
 */

void gc_test(const char *fsname)
{
  // Get fs in nfvfs by fsname.
  struct nfvfs *dst_fs;
  dst_fs = get_nfvfs(fsname);
  if (!dst_fs)
  {
    printf("\r\nFailed to get %s, making sure you have register it\r\n", fsname);
    return;
  }

  // int my_total_create = 0;

  // Mount function
  raw_mount(dst_fs);

  // File's path
  char path[64];
  memset(path, 0, 64);
  if (fsname[0] == 'e' && fsname[1] == 'H')
    strcpy(path, "1:/test1.?");
  else
    strcpy(path, "/test1.?");
  int tail = strlen(path);
  char my_cnt1 = 'A';
  char my_cnt2 = 'A';

  uint32_t start;

  start = (uint32_t)xTaskGetTickCount();
  char temp_cnt1 = my_cnt1;
  char temp_cnt2 = my_cnt2;
  for (int i = 0; i < 2000; i++)
  {
    // Open and then change the path.
    path[tail - 1] = my_cnt1;
    path[tail] = my_cnt2;
    // uint32_t start = (uint32_t)xTaskGetTickCount();
    int fd = raw_open(dst_fs, path, O_RDWR | O_CREAT, S_ISREG);
    // uint32_t end = (uint32_t)xTaskGetTickCount();
    // my_total_create += end - start;
    if (fd < 0)
      printf("open error is %d\n", fd);
    raw_write(dst_fs, fd, 4);
    raw_close(dst_fs, fd);
    my_cnt1++;
    if (my_cnt1 == 'z')
    {
      my_cnt1 = 'A';
      my_cnt2 += 1;
    }

    if (i % 2 == 0)
      continue;

    path[tail - 1] = temp_cnt1;
    path[tail] = temp_cnt2;
    // For eHNFFS
    if (fsname[0] == 'e' && fsname[1] == 'H')
    {
      fd = raw_open(dst_fs, path, O_RDWR | O_CREAT, S_ISREG);
      if (fd < 0)
        printf("open error is %d\n", fd);
    }
    int type = (dst_fs->name[0] == 'e' && dst_fs->name[1] == 'H')
                   ? NFVFS_REMOVE_FHANDLER
                   : NFVFS_REMOVE_PATH;
    raw_delete(dst_fs, fd, path, S_ISREG, type);
    temp_cnt1 += 2;
    if (temp_cnt1 >= 'z')
    {
      temp_cnt1 = (temp_cnt1 == 'z') ? 'A' : 'B';
      temp_cnt2 += 1;
    }

    // if (i % 500 == 499)
    // {
    //   uint32_t end = (uint32_t)xTaskGetTickCount();
    //   printf("small file %d: %d\r\n", i + 1, end - start);
    // }
  }

  // // TODO
  // // Big file's random write and gc
  // for (int i = 0; i < 1; i++)
  // {
  //   // Open and then change the path.
  //   path[tail - 1] = my_cnt1;
  //   path[tail] = my_cnt2;

  //   // No need to open for eHNFFS and littlefs
  //   // uint32_t start = (uint32_t)xTaskGetTickCount();
  //   int fd = raw_open(dst_fs, path, O_RDWR | O_CREAT, S_ISREG);
  //   // uint32_t end = (uint32_t)xTaskGetTickCount();
  //   // my_total_create += end - start;

  //   if (fd < 0)
  //     printf("open error is %d\n", fd);
  //   my_cnt1++;
  //   if (my_cnt1 == 'z')
  //   {
  //     my_cnt1 = 'A';
  //     my_cnt2 += 1;
  //   }

  //   int file_size = 2 * 1024 * 1024;
  //   raw_write(dst_fs, fd, file_size);

  //   start = (uint32_t)xTaskGetTickCount();
  //   int test_size = 10 * 1024;
  //   while (test_size > 0)
  //   {
  //     int min = (test_size > 1024) ? 1024 : test_size;
  //     int temp = rand() % file_size;
  //     temp = (temp > 1024) ? (temp - 1024) : 1024;
  //     raw_lseek(dst_fs, fd, temp);

  //     // uint32_t start = (uint32_t)xTaskGetTickCount();
  //     raw_write(dst_fs, fd, min);
  //     // uint32_t end = (uint32_t)xTaskGetTickCount();
  //     // my_total_create += (end - start);
  //     // printf("time is %d\r\n", my_total_create);

  //     test_size -= min;
  //     // if ((test_size / 1024) % 10 == 0)
  //     // {
  //     //   uint32_t end = (uint32_t)xTaskGetTickCount();
  //     //   printf("large file %d: %d\r\n", 40 - test_size / 1024, end - start);
  //     // }
  //   }

  //   raw_close(dst_fs, fd);
  // }

  // For SPIFFS
  uint32_t end = (uint32_t)xTaskGetTickCount();
  printf("prepare phase time is %d\r\n", end - start);
  int err = nfvfs_gc(dst_fs);
  end = (uint32_t)xTaskGetTickCount();
  printf("gc return is %d, time is %d\r\n", err, end - start);

  // Unmount fs
  raw_unmount(dst_fs);
}

void corrupt_test(void)
{
  int off = 0;
  uint32_t head;
  for (int i = 0; i < 8192; i++)
  {
    off = i * 4096;
    W25QXX_Read((uint8_t *)&head, off, 4);
  }

  uint8_t buffer[4096];
  // sector map.
  W25QXX_Write_NoCheck(buffer, 0, 1024);
  // region map.
  W25QXX_Write_NoCheck(buffer, 0, 2 * 128 / 8);
  // wl array.
  W25QXX_Write_NoCheck(buffer, 0, 4 + 8 * 128);
  for (int i = 0; i < 10; i++)
    W25QXX_Write_NoCheck(buffer, 0, 4);
}
