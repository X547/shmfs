#include "Shmfs.h"


status_t ShmfsAttribute::SetName(const char* name)
{
	size_t len = strlen(name) + 1;
	ArrayDeleter<char> newName(new(std::nothrow) char[len]);
	if (!newName.IsSet())
		return B_NO_MEMORY;
	memcpy(&newName[0], name, len);
	fName.SetTo(newName.Detach());
	return B_OK;
}


status_t ShmfsAttribute::Read(off_t pos, void* buffer, size_t &length)
{
	return ENOSYS;
}

status_t ShmfsAttribute::Write(off_t pos, const void* buffer, size_t &length)
{
	return ENOSYS;
}

status_t ShmfsAttribute::ReadStat(struct stat &stat)
{
	return ENOSYS;
}

status_t ShmfsAttribute::WriteStat(const struct stat &stat, int statMask)
{
	return ENOSYS;
}
