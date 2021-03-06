/*
    Distrotech Solutions wxWidgets Gui Interface
    Copyright (C) 2013 Gregory Hinton Nietsky <gregory@distrotech.co.za>

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

/** @file
  * @ingroup LIB-FILE
  * @brief File utilities to test files (fstat)
  * @addtogroup LIB-FILE
  * @{*/

#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <fcntl.h>
#include <ctype.h>
#ifdef __WIN32
#else
#include <grp.h>
#endif

/** @brief Determine if a file exists.
  * @param path Filename.
  * @returns 1 if the file exists 0 othewise.*/
extern int is_file(const char *path) {
	struct stat sr;
	if (!stat(path, &sr)) {
		return 1;
	} else {
		return 0;
	}
}

/** @brief Determine if a path is a directory
  * @param path Path of directory to check.
  * @returns 1 if the path exists and is a directory 0 othewise.*/
extern int is_dir(const char *path) {
	struct stat sr;
	if (!stat(path, &sr) && S_ISDIR(sr.st_mode)) {
		return 1;
	} else {
		return 0;
	}
}

/** @brief Determine if a file is executable
  * @param path Path of file to check.
  * @returns 1 if the path exists and is executable 0 othewise.*/
extern int is_exec(const char *path) {
	struct stat sr;
	if (!stat(path, &sr) && (S_IXUSR & sr.st_mode)) {
		return 1;
	} else {
		return 0;
	}
}

/** @brief Create a directory
  *
  * On *NIX systems a mode, uid and gid can be used to set initial permisions.
  * @param dir Directory to create.
  * @param mode Initial mode to set.
  * @param user Initial UID.
  * @param group Initial GID.
  * @returns non 0 on success on failure the directory may be created but no ownership not set.*/
#ifdef __WIN32
extern int mk_dir(const char *dir) {
#else
extern int mk_dir(const char *dir, mode_t mode, uid_t user, gid_t group) {
#endif
	struct stat sr;

#ifdef __WIN32
	if ((stat(dir, &sr) && (errno == ENOENT)) && !mkdir(dir)) {
#else
	if ((stat(dir, &sr) && (errno == ENOENT)) && !mkdir(dir, mode) && !chown(dir, user, group)) {
#endif
		return 0;
	}
	return -1;
}

/** @}*/
