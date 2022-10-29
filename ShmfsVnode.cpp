#include "Shmfs.h"

#include <NodeMonitor.h>

#include <util/AutoLock.h>

#include <new>


void GetCurrentTime(struct timespec &outTime)
{
	bigtime_t now = real_time_clock_usecs();
	outTime.tv_sec = now / 1000000LL;
	outTime.tv_nsec = (now % 1000000LL) * 1000;
}


//#pragma mark - ShmfsVnode

ShmfsVnode::~ShmfsVnode()
{
	RecursiveLocker lock(Volume()->Lock());
	TRACE("-ShmfsVnode(%" B_PRId64 ", \"%s\")\n", fId, Name());
	if (fId != 0) {
		fVolume->fIds.Remove(this);
		fVolume->fIdPool.Free(fId);
	}
}

status_t ShmfsVnode::SetName(const char *name)
{
	size_t len = strlen(name) + 1;
	ArrayDeleter<char> newName(new(std::nothrow) char[len]);
	if (!newName.IsSet())
		return B_NO_MEMORY;
	memcpy(&newName[0], name, len);
	fName.SetTo(newName.Detach());
	return B_OK;
}

status_t ShmfsVnode::Lookup(const char* name, ino_t &id)
{
	RecursiveLocker lock(Volume()->Lock());
	TRACE("ShmfsVnode::Lookup()\n");
	ShmfsVnode *vnode = fNames.Find(name);
	if (vnode != NULL) {
		id = vnode->Id();
		return B_OK;
	}
	return ENOENT;
}

status_t ShmfsVnode::GetVnodeName(char* buffer, size_t bufferSize)
{
	RecursiveLocker lock(Volume()->Lock());
	TRACE("ShmfsVnode::GetVnodeName()\n");
	strlcpy(buffer, &fName[0], bufferSize);
	return B_OK;
}

status_t ShmfsVnode::PutVnode(bool reenter)
{
	TRACE("ShmfsVnode::PutVnode()\n");
	ReleaseReference();
	return B_OK;
}

status_t ShmfsVnode::Fsync()
{
	return B_OK;
}

status_t ShmfsVnode::ReadSymlink(char* buffer, size_t &bufferSize)
{
	TRACE("ShmfsVnode::ReadSymlink(%p, %p)\n", buffer, &bufferSize);
	return ENOSYS;
}

status_t ShmfsVnode::CreateSymlink(const char* name, const char* path, int mode)
{
	TRACE("ShmfsVnode::CreateSymlink(\"%s\", \"%s\")\n", name, path);
	return B_NOT_A_DIRECTORY;
}

status_t ShmfsVnode::Unlink(const char* name)
{
	TRACE("ShmfsVnode::Unlink(\"%s\")\n", name);
	return B_NOT_A_DIRECTORY;
}

status_t ShmfsVnode::Rename(const char* fromName, ShmfsVnode* toDir, const char* toName)
{
	TRACE("ShmfsVnode::Rename()\n");
	return B_NOT_A_DIRECTORY;
}

status_t ShmfsVnode::Access(int mode)
{
	TRACE("ShmfsVnode::Access()\n");
	return B_OK;
}

status_t ShmfsVnode::ReadStat(struct stat &stat)
{
	RecursiveLocker lock(Volume()->Lock());
	TRACE("ShmfsVnode::ReadStat()\n");

	stat = {
		.st_ino = fId,
		.st_mode = fMode,
		.st_nlink = 1,
		.st_uid = fUid,
		.st_gid = fGid,
		.st_atim = fAccessTime,
		.st_mtim = fModifyTime,
		.st_ctim = fChangeTime,
		.st_crtim = fCreateTime,
	};
	return B_OK;
}

status_t ShmfsVnode::WriteStat(const struct stat &stat, uint32 statMask)
{
	ino_t dirId;
	{
	RecursiveLocker lock(Volume()->Lock());
	TRACE("ShmfsVnode::WriteStat()\n");

	if ((statMask & B_STAT_MODE) != 0)
		fMode = stat.st_mode & S_IUMSK;
	if ((statMask & B_STAT_UID) != 0)
		fUid = stat.st_uid;
	if ((statMask & B_STAT_GID) != 0)
		fGid = stat.st_gid;
	if ((statMask & B_STAT_ACCESS_TIME) != 0)
		fAccessTime = stat.st_atim;
	if ((statMask & B_STAT_MODIFICATION_TIME) != 0)
		fModifyTime = stat.st_mtim;
	if ((statMask & B_STAT_CHANGE_TIME) != 0)
		fChangeTime = stat.st_ctim;
	else if (statMask != 0)
		GetCurrentTime(fChangeTime);
	if ((statMask & B_STAT_CREATION_TIME) != 0)
		fCreateTime = stat.st_crtim;

	dirId = fParent == NULL ? 0 : fParent->Id();
	}
	notify_stat_changed(Volume()->Id(), dirId, Id(), statMask);

	return B_OK;
}

status_t ShmfsVnode::Create(const char* name, int openMode, int perms, ino_t &newVnodeID)
{
	TRACE("ShmfsVnode::Create()\n");
	return B_NOT_A_DIRECTORY;
}

status_t ShmfsVnode::Open(int openMode, ShmfsFileCookie* &cookie)
{
	TRACE("ShmfsVnode::Open()\n");
	cookie = NULL;
	return B_OK;
}

status_t ShmfsVnode::Close(ShmfsFileCookie* cookie)
{
	TRACE("ShmfsVnode::Close()\n");
	return B_OK;
}

status_t ShmfsVnode::FreeCookie(ShmfsFileCookie* cookie)
{
	TRACE("ShmfsVnode::FreeCookie()\n");
	return B_OK;
}

status_t ShmfsVnode::Read(ShmfsFileCookie* cookie, off_t pos, void* buffer, size_t &length)
{
	TRACE("ShmfsVnode::Read()\n");
	return B_IS_A_DIRECTORY;
}

status_t ShmfsVnode::Write(ShmfsFileCookie* cookie, off_t pos, const void* buffer, size_t &length)
{
	TRACE("ShmfsVnode::Write()\n");
	return B_IS_A_DIRECTORY;
}

status_t ShmfsVnode::CreateDir(const char* name, int perms)
{
	TRACE("ShmfsVnode::CreateDir()\n");
	return B_NOT_A_DIRECTORY;
}

status_t ShmfsVnode::RemoveDir(const char* name)
{
	TRACE("ShmfsVnode::RemoveDir()\n");
	return B_NOT_A_DIRECTORY;
}

status_t ShmfsVnode::OpenDir(ShmfsDirIterator* &cookie)
{
	TRACE("ShmfsVnode::OpenDir()\n");
	return B_NOT_A_DIRECTORY;
}

status_t ShmfsVnode::CloseDir(ShmfsDirIterator* cookie)
{
	TRACE("ShmfsVnode::CloseDir()\n");
	return B_NOT_A_DIRECTORY;
}

status_t ShmfsVnode::FreeDirCookie(ShmfsDirIterator* cookie)
{
	TRACE("ShmfsVnode::FreeDirCookie()\n");
	return B_NOT_A_DIRECTORY;
}

status_t ShmfsVnode::ReadDir(ShmfsDirIterator* cookie, struct dirent* buffer, size_t bufferSize, uint32 &num)
{
	TRACE("ShmfsVnode::ReadDir()\n");
	return B_NOT_A_DIRECTORY;
}

status_t ShmfsVnode::RewindDir(ShmfsDirIterator* cookie)
{
	TRACE("ShmfsVnode::RewindDir()\n");
	return B_NOT_A_DIRECTORY;
}
