/*
 * _DoCacheIO, _GetPages, _PutPages are based on Haiku `ramfs` code under following license:
 *
 * Copyright 2007, Ingo Weinhold, ingo_weinhold@gmx.de.
 * Copyright 2019, Haiku, Inc.
 * All rights reserved. Distributed under the terms of the MIT license.
 */


#include "Shmfs.h"

#include <KernelExport.h>
#include <NodeMonitor.h>

#include <vfs.h>
#include <vm/VMCache.h>
#include <vm/vm_page.h>

#include <util/AutoLock.h>

#include <new>
#include <algorithm>


class ShmfsFileCookie {
public:
	bool isAppend = false;
};


status_t ShmfsFileVnode::Init()
{
	return B_OK;
}

ShmfsFileVnode::~ShmfsFileVnode()
{
}


status_t ShmfsFileVnode::EnsureSize(uint32 size)
{
	if (size > fDataAllocSize) {
		uint32 newSize = size + size/2;
		ArrayDeleter<uint8> newData(new(std::nothrow) uint8[newSize]);
		if (!newData.IsSet())
			return B_NO_MEMORY;
		memcpy(&newData[0], &fData[0], fDataSize);
		fData.SetTo(newData.Detach());
		fDataAllocSize = newSize;
	}
	return B_OK;
}


//#pragma mark - VFS interface

bool ShmfsFileVnode::CanPage(ShmfsFileCookie* cookie)
{
	return true;
}

status_t ShmfsFileVnode::ReadPages(ShmfsFileCookie* cookie, off_t pos, const iovec* vecs, size_t count, size_t& numBytes)
{
	RecursiveLocker lock(Volume()->Lock());

	if (pos < 0 || pos >= fDataSize)
		return B_BAD_VALUE;

	size_t bytesLeft = std::min<size_t>(numBytes, size_t(fDataSize - pos));
	numBytes = 0;
	for (size_t i = 0; i < count && bytesLeft > 0; i++) {
		const size_t ioSize = std::min<size_t>(bytesLeft, vecs[i].iov_len);
		memcpy((char*)vecs[i].iov_base, &fData[pos], ioSize);

		pos += ioSize;
		numBytes += ioSize;
		bytesLeft -= ioSize;
	}

	return B_OK;
}

status_t ShmfsFileVnode::WritePages(ShmfsFileCookie* cookie, off_t pos, const iovec* vecs, size_t count, size_t& numBytes)
{
	RecursiveLocker lock(Volume()->Lock());

	if (pos < 0 || pos >= fDataSize)
		return B_BAD_VALUE;

	size_t bytesLeft = std::min<size_t>(numBytes, size_t(fDataSize - pos));
	numBytes = 0;
	for (size_t i = 0; i < count && bytesLeft > 0; i++) {
		const size_t ioSize = std::min<size_t>(bytesLeft, vecs[i].iov_len);
		memcpy(&fData[pos], (char*)vecs[i].iov_base, ioSize);

		pos += ioSize;
		numBytes += ioSize;
		bytesLeft -= ioSize;
	}

	return B_OK;
}

status_t ShmfsFileVnode::SetFlags(ShmfsFileCookie* cookie, int flags)
{
	RecursiveLocker lock(Volume()->Lock());
	TRACE("#%" B_PRId64 ".FileVnode::SetFlags(%p, %x)\n", Id(), cookie, flags);
	cookie->isAppend = (flags & O_APPEND) != 0;
	return B_OK;
}

status_t ShmfsFileVnode::ReadStat(struct stat &stat)
{
	RecursiveLocker lock(Volume()->Lock());
	CHECK_RET(ShmfsVnode::ReadStat(stat));
	stat.st_mode |= S_IFREG;
	stat.st_size = fDataSize;
	stat.st_blocks = (fDataSize + (512 - 1)) / 512;
	return B_OK;
}

status_t ShmfsFileVnode::WriteStat(const struct stat &stat, uint32 statMask)
{
	RecursiveLocker lock(Volume()->Lock());

	if ((statMask & B_STAT_SIZE) != 0) {
		CHECK_RET(EnsureSize(stat.st_size));
		if (stat.st_size > fDataSize)
			memset(&fData[fDataSize], 0, stat.st_size - fDataSize);
		fDataSize = stat.st_size;
	}
	return ShmfsVnode::WriteStat(stat, statMask);
}

status_t ShmfsFileVnode::Open(int openMode, ShmfsFileCookie* &outCookie)
{
	TRACE("#%" B_PRId64 ".FileVnode::Open(%x, &cookie: %p)\n", Id(), openMode, &cookie);
	ObjectDeleter<ShmfsFileCookie> cookie(new(std::nothrow) ShmfsFileCookie());
	if (!cookie.IsSet())
		return B_NO_MEMORY;
	cookie->isAppend = (openMode & O_APPEND) != 0;
	if (fDataSize > 0 && (O_TRUNC & openMode) != 0)
		CHECK_RET(WriteStat({.st_size = 0}, B_STAT_SIZE));
	outCookie = cookie.Detach();
	return B_OK;
}

status_t ShmfsFileVnode::FreeCookie(ShmfsFileCookie* cookie)
{
	TRACE("#%" B_PRId64 ".FileVnode::FreeCookie(%p)\n", Id(), cookie);
	delete cookie;
	return B_OK;
}

status_t ShmfsFileVnode::Read(ShmfsFileCookie* cookie, off_t pos, void* buffer, size_t &outLength)
{
	ino_t dirId;
	{
	RecursiveLocker lock(Volume()->Lock());
	TRACE("#%" B_PRId64 ".FileVnode::Read(%p, %" B_PRId64 ")\n", Id(), cookie, pos);

	if (pos < 0)
		return B_BAD_VALUE;

	pos = std::min<off_t>(pos, fDataSize);
	size_t length = std::min<size_t>(outLength, size_t(fDataSize - pos));

	struct timespec time;
	GetCurrentTime(time);
	fAccessTime = time;

	dirId = fParent == NULL ? 0 : fParent->Id();

	memcpy(buffer, &fData[pos], length);
	}
	notify_stat_changed(Volume()->Id(), dirId, Id(), B_STAT_ACCESS_TIME);
	return B_OK;
}

status_t ShmfsFileVnode::Write(ShmfsFileCookie* cookie, off_t pos, const void* buffer, size_t &outLength)
{
	ino_t dirId;
	{
	RecursiveLocker lock(Volume()->Lock());
	TRACE("#%" B_PRId64 ".FileVnode::Write(%p, %" B_PRId64 ")\n", Id(), cookie, pos);

	if (cookie->isAppend)
		pos = fDataSize;

	if (pos < 0)
		return B_BAD_VALUE;

	size_t length = outLength;
	off_t newSize = pos + length;
	if (newSize <= 0) {
		length = 0;
		return B_OK;
	}
	if (newSize > fDataSize) {
		CHECK_RET(WriteStat({.st_size = newSize}, B_STAT_SIZE));
	}

	struct timespec time;
	GetCurrentTime(time);
	fAccessTime = time;
	fModifyTime = time;

	dirId = fParent == NULL ? 0 : fParent->Id();

	memcpy(&fData[pos], buffer, length);
	}
	notify_stat_changed(Volume()->Id(), dirId, Id(), B_STAT_ACCESS_TIME | B_STAT_MODIFICATION_TIME);
	return B_OK;
}
