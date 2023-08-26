// Copyright (C) 2022 Deadpool
//
// Nor Flash File System Benchmark
//
// NORENV is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 2 of the License, or
// (at your option) any later version.
//
// NORENV is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with NORENV.  If not, see <http://www.gnu.org/licenses/>.

#ifndef __BENCHMARK_H
#define __BENCHMARK_H

/**
 * -----------------------------------------------------------------------------------------------------------------------------------------------
 * -------------------------------------------------------------    Raw IO speed    --------------------------------------------------------------
 * -----------------------------------------------------------------------------------------------------------------------------------------------
 */

void raw_sctor_test(int loop, uint32_t size);

/**
 * -----------------------------------------------------------------------------------------------------------------------------------------------
 * -------------------------------------------------------------    Basic fs test    -------------------------------------------------------------
 * -----------------------------------------------------------------------------------------------------------------------------------------------
 */

// 0
void file_prog_test(const char *fsname, int loop);

// void basic_storage_test(const char *fsname, int loop);

// void fatfs_test(void);

// 1
void busybox_test(const char *fsname);

// void operation_test(const char *fsname);

// 2
void new_operation_test(const char *fsname);

void layer_test(struct nfvfs *dst_fs, char *path);

void burn_test(struct nfvfs *dst_fs, char *path, int len);

void read_test(struct nfvfs *dst_fs, char *path, int len);

void update_test(struct nfvfs *dst_fs, char *path, int len);

void remove_test(struct nfvfs *dst_fs, char *path, int len);

void mount_test(const char *fsname);

// 3
void random_write_test(const char *fsname);

// 4
void random_read_test(const char *fsname);

// 5
void gc_test(const char *fsname);

void corrupt_test(void);

#endif /* __BENCHMARK_H */
