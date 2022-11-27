/*
 * mcu-app - Microcontroller application framework
 * Copyright 2022  Simon Arlott
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */
#ifdef ENV_NATIVE

#include <Arduino.h>
#include <FS.h>
#include <assert.h>
#include <errno.h>
#include <libgen.h>
#include <stdio.h>
#include <string>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

#include <algorithm>

#include "../util.h"

static const std::string FS_PREFIX = ".pio/fs/";

namespace fs {

File::~File() {
	close();
}

size_t File::write(uint8_t c) {
	return write(&c, 1);
}

size_t File::write(const uint8_t *buf, size_t size) {
	if (f_ == nullptr)
		return 0;

	return ::fwrite(buf, 1, size, f_);
}

int File::available() {
	if (f_ == nullptr)
		return 0;

	long current = ::ftell(f_);

	::fseek(f_, 0, SEEK_END);

	long end = ::ftell(f_);

	::fseek(f_, current, SEEK_SET);

	return (peek_ != -1 ? 1 : 0) + (end - current);
}

int File::read() {
	if (f_ == nullptr)
		return -1;

	if (peek_ != -1) {
		uint8_t c = peek_;
		peek_ = -1;
		return c;
	}

	uint8_t c = 0;
	size_t ret = ::fread(&c, 1, 1, f_);
	if (ret == 1)
		return c;

	return -1;
}

int File::peek() {
	if (f_ == nullptr)
		return -1;

	if (peek_ != -1) {
		uint8_t c = peek_;
		peek_ = -1;
		return c;
	}

	peek_ = read();
	return peek_;
}

void File::flush() {}

size_t File::read(uint8_t* buf, size_t size) {
	if (f_ == nullptr)
		return 0;

	if (size == 0)
		return 0;

	if (peek_ != -1) {
		uint8_t c = peek_;
		peek_ = -1;
		*(buf++) = c;
		size--;
	}

	return ::fread(buf, 1, size, f_);
}

bool File::seek(uint32_t pos, SeekMode mode) {
	if (f_ == nullptr)
		return false;

	switch (mode) {
	case SeekSet:
		return ::fseek(f_, pos, SEEK_SET) == 0;

	case SeekCur:
		return ::fseek(f_, pos, SEEK_CUR) == 0;

	case SeekEnd:
		return ::fseek(f_, pos, SEEK_END) == 0;

	default:
		return false;
	}
}

size_t File::position() const {
	if (f_ == nullptr)
		return 0;

	return ::ftell(f_);
}

size_t File::size() const {
	if (f_ == nullptr)
		return 0;

	long current = ::ftell(f_);

	::fseek(f_, 0, SEEK_END);

	long end = ::ftell(f_);

	::fseek(f_, current, SEEK_SET);

	return end;
}

//bool File::setBufferSize(size_t size) {}

void File::close() {
	if (f_)
		::fclose(f_);
	f_ = nullptr;

	if (d_)
		::closedir(d_);
	d_ = nullptr;
}

File::operator bool() const {
	return f_ != nullptr || d_ != nullptr;
}

time_t File::getLastWrite() {
	struct stat st;

	if (f_ != nullptr && ::fstat(fileno(f_), &st) == 0)
		return st.st_mtime;

	if (d_ != nullptr && ::fstat(dirfd(d_), &st) == 0)
		return st.st_mtime;

	return 0;
}
const char* File::path() const { return path_.c_str(); }
//const char* File::name() const {}

boolean File::isDirectory(void) { return d_ != nullptr; }

File File::openNextFile(const char *mode) {
	static const std::string this_dir{"."};
	static const std::string parent_dir{".."};
	struct dirent *dirp;

	do {
		dirp = ::readdir(d_);
		if (dirp != nullptr) {
			if (dirp->d_type != DT_REG && dirp->d_type != DT_DIR)
				continue;

			if (dirp->d_name == this_dir || dirp->d_name == parent_dir)
				continue;

			break;
		}
	} while (dirp != nullptr);

	if (dirp == nullptr)
		return File(fs_, (FILE*)nullptr, "");

	std::string slash = "/";

	if (!path_.empty() && path_.back() == '/')
		slash = "";

	return fs_.open((path_ + slash + dirp->d_name).c_str(), mode);
}

void File::rewindDirectory(void) {
	::rewinddir(d_);
}

static std::string resolve_filename(const std::string &filename) {
	return FS_PREFIX + app::normalise_filename(filename);
}

static bool valid_filename(const std::string &filename) {
	std::string normalised_filename = app::normalise_filename(filename);
	return !normalised_filename.empty() && normalised_filename.front() == '/';
}

File FS::open(const char* path, const char* mode, const bool create) {
	std::string filename = resolve_filename(path);
	struct stat st;

	if (!valid_filename(path))
		return File(*this, (FILE*)nullptr, "");

	::mkdir(FS_PREFIX.c_str(), 0777);

	if (create) {
		// mkdir();
	}

	if (::stat(filename.c_str(), &st) != 0 && mode == std::string{"r"})
		return File(*this, (FILE*)nullptr, "");

	if (S_ISDIR(st.st_mode)) {
		return File(*this, ::opendir(filename.c_str()), path);
	} else if (S_ISREG(st.st_mode) || mode != std::string{"r"}) {
		return File(*this, ::fopen(filename.c_str(), mode), path);
	} else {
		return File(*this, (FILE*)nullptr, "");
	}
}

bool FS::exists(const char* path) {
	std::string filename = resolve_filename(path);
	struct stat st;

	if (!valid_filename(path))
		return false;

	return ::stat(filename.c_str(), &st) == 0;
}

bool FS::remove(const char* path) {
	std::string filename = resolve_filename(path);

	if (!valid_filename(path))
		return false;

	return ::unlink(filename.c_str()) == 0;
}

bool FS::rename(const char* pathFrom, const char* pathTo) {
	std::string filenameFrom = resolve_filename(pathFrom);
	std::string filenameTo = resolve_filename(pathTo);

	if (!valid_filename(pathFrom))
		return false;

	if (!valid_filename(pathTo))
		return false;

	return ::rename(filenameFrom.c_str(), filenameTo.c_str()) == 0;
}

bool FS::mkdir(const char *path) {
	std::string filename = resolve_filename(path);

	if (!valid_filename(path))
		return false;

	::mkdir(FS_PREFIX.c_str(), 0777);
	int ret = ::mkdir(filename.c_str(), 0777);
	return ret == 0 || errno == EEXIST;
}

bool FS::rmdir(const char *path) {
	std::string filename = resolve_filename(path);

	if (!valid_filename(path))
		return false;

	return ::rmdir(filename.c_str()) == 0;
}

} // namespace fs

#endif
