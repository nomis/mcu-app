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

static const std::string FS_PREFIX = ".pio/fs/";

namespace fs {

File::~File() {
	close();
}

size_t File::write(uint8_t c) {
	return write(&c, 1);
}

size_t File::write(const uint8_t *buf, size_t size) {
	return ::fwrite(buf, size, 1, f_);
}

int File::available() {
	long current = ::ftell(f_);

	::fseek(f_, 0, SEEK_END);

	long end = ::ftell(f_);

	::fseek(f_, current, SEEK_SET);

	return (peek_ != -1 ? 1 : 0) + (end - current);
}

int File::read() {
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
	if (size == 0)
		return 0;

	if (peek_ != -1) {
		uint8_t c = peek_;
		peek_ = -1;
		*(buf++) = c;
		size--;
	}

	return ::fread(buf, size, 1, f_);
}

bool File::seek(uint32_t pos, SeekMode mode) {
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
	return ::ftell(f_);
}

size_t File::size() const {
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
}

File::operator bool() const { return f_ != nullptr; }
time_t File::getLastWrite() { return 0; }
//const char* File::path() const {}
//const char* File::name() const {}

//boolean File::isDirectory(void) {}
//File File::openNextFile(const char* mode = FILE_READ) {}
//void File::rewindDirectory(void) {}

static void validate_filename(const std::string &filename) {
	assert(filename.find("/..") == std::string::npos);
	assert(filename.find("../") == std::string::npos);
	assert(filename != "..");
}

File FS::open(const char* path, const char* mode, const bool create) {
	std::string filename = path;

	validate_filename(filename);

	filename = FS_PREFIX + path;

	::mkdir(FS_PREFIX.c_str(), 0777);
	return File(::fopen(filename.c_str(), mode));
}

bool FS::exists(const char* path) {
	std::string filename = path;
	struct stat st;

	validate_filename(filename);

	filename = FS_PREFIX + path;

	return ::stat(filename.c_str(), &st) == 0;
}

bool FS::remove(const char* path) {
	std::string filename = path;

	validate_filename(filename);

	filename = FS_PREFIX + path;

	return ::unlink(filename.c_str()) == 0;
}

bool FS::rename(const char* pathFrom, const char* pathTo) {
	std::string filenameFrom = pathFrom;
	std::string filenameTo = pathTo;

	validate_filename(filenameFrom);
	validate_filename(filenameTo);

	filenameFrom = FS_PREFIX + pathFrom;
	filenameTo = FS_PREFIX + pathTo;

	return ::rename(filenameFrom.c_str(), filenameTo.c_str()) == 0;
}

bool FS::mkdir(const char *path) {
	std::string filename = path;

	validate_filename(filename);

	filename = FS_PREFIX + path;

	::mkdir(FS_PREFIX.c_str(), 0777);
	int ret = ::mkdir(filename.c_str(), 0777);
	return ret == 0 || errno == EEXIST;
}

bool FS::rmdir(const char *path) {
	std::string filename = path;

	validate_filename(filename);

	filename = FS_PREFIX + path;

	return ::rmdir(filename.c_str()) == 0;
}

} // namespace fs

#endif
