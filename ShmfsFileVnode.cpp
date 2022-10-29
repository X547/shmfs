#include "Shmfs.h"

#include <KernelExport.h>
#include <NodeMonitor.h>

#include <util/AutoLock.h>

#include <new>
#include <algorithm>


status_t ShmfsFileVnode::EnsureSize(uint64 size)
{
	if (fDataAllocSize < size) {
		uint64 newAllocSize = size + size/2;
		ArrayDeleter<uint8> newData(new(std::nothrow) uint8[newAllocSize]);
		if (!newData.IsSet())
			return B_NO_MEMORY;
		memcpy(&newData[0], &fData[0], fDataSize);
		fData.SetTo(newData.Detach());
		fDataAllocSize = newAllocSize;
	}
	return B_OK;
}


//#pragma mark - VFS interface

status_t ShmfsFileVnode::ReadStat(struct stat &stat)
{
	RecursiveLocker lock(Volume()->Lock());
	CHECK_RET(ShmfsVnode::ReadStat(stat));
	stat.st_mode |= S_IFREG;
	stat.st_size = fDataSize;
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

status_t ShmfsFileVnode::Read(ShmfsFileCookie* cookie, off_t pos, void* buffer, size_t &length)
{
	RecursiveLocker lock(Volume()->Lock());

	if (pos < 0)
		return B_BAD_VALUE;

	if (pos + length > fDataSize)
		length = std::max<size_t>((off_t)fDataSize - pos, 0);

	CHECK_RET(user_memcpy(buffer, &fData[pos], length));

	return B_OK;
}

status_t ShmfsFileVnode::Write(ShmfsFileCookie* cookie, off_t pos, const void* buffer, size_t &length)
{
	RecursiveLocker lock(Volume()->Lock());

	if (pos < 0)
		return B_BAD_VALUE;

	off_t newSize = pos + length;
	if (newSize <= 0) {
		length = 0;
		return B_OK;
	}
	if (newSize > fDataSize) {
		CHECK_RET(EnsureSize(newSize));
		fDataSize = newSize;
	}
	CHECK_RET(user_memcpy(&fData[pos], buffer, length));

	return B_OK;
}
