#include "Shmfs.h"

#include <fs_info.h>

#include <util/AutoLock.h>

#include <new>


//#pragma mark - ShmfsVolume

ShmfsVolume::ShmfsVolume()
{
	fIdPool.Register(1, 0x7fffffff);
}

void ShmfsVolume::ListVnodes()
{
	dprintf("ListVnodes()\n");
	for (ShmfsVnode *vnode = fIds.LeftMost(); vnode != NULL; vnode = fIds.Next(vnode)) {
		dprintf("  %" B_PRId64 ": ShmfsVnode(name: \"%s\", adr: %p)\n", vnode->Id(), vnode->Name(), &vnode);
	}
}

status_t ShmfsVolume::RegisterVnode(ShmfsVnode *vnode)
{
	RecursiveLocker lock(Lock());

	uint64_t id;
	if (!fIdPool.Alloc(id, 1))
		return B_NO_MEMORY;

	vnode->fVolume = this;
	vnode->fId = id;
	fIds.Insert(vnode);

	dprintf("+ShmfsVnode(%" B_PRId64 ", \"%s\"), adr: %p\n", vnode->fId, vnode->Name(), vnode);
	dprintf("  &fIdNode: %p\n", &vnode->fIdNode);

	return B_OK;
}

ShmfsVnode *ShmfsVolume::LookupVnode(ino_t id)
{
	RecursiveLocker lock(Lock());
	dprintf("ShmfsVolume::LookupVnode: %" B_PRId64 "\n", id);
	return fIds.Find(id);
}


status_t ShmfsVolume::Mount(ShmfsVolume* &volume, fs_volume *base, const char* device, uint32 flags, const char* args, ino_t &rootVnodeID)
{
	ObjectDeleter<ShmfsVolume> vol(new(std::nothrow) ShmfsVolume());
	if (!vol.IsSet())
		return B_NO_MEMORY;
	RecursiveLocker lock(vol->Lock());
	vol->fBase = base;
	volume = vol.Get();

	vol->fRootVnode.SetTo(new(std::nothrow) ShmfsDirectoryVnode(), true);
	CHECK_RET(vol->RegisterVnode(vol->fRootVnode));
	vol->ListVnodes();
	rootVnodeID = vol->fRootVnode->Id();
	CHECK_RET(get_vnode(vol->Base(), rootVnodeID, NULL));

	vol.Detach();
	return B_OK;
}

status_t ShmfsVolume::Unmount()
{
	{
		RecursiveLocker lock(Lock());
		fRootVnode.Unset();
	}
	delete this;
	return B_OK;
}

status_t ShmfsVolume::ReadFsInfo(struct fs_info &info)
{
	RecursiveLocker lock(Lock());
	info = {
		.dev = Id(),
		.root = fRootVnode->Id(),
		.block_size = B_PAGE_SIZE,
		.io_size = B_PAGE_SIZE,
		.total_blocks = 0,
		.free_blocks = INT64_MAX / B_PAGE_SIZE,
		.total_nodes = 0,
		.free_nodes = INT64_MAX,
	};
	return B_OK;
}

status_t ShmfsVolume::GetVnode(ino_t id, ShmfsVnode* &vnode, int &type, uint32 &flags, bool reenter)
{
	dprintf("ShmfsVolume::GetVnode(%" B_PRId64 ")\n", id);
	dprintf("  &vnode: %p\n", &vnode);
	dprintf("  &type: %p\n", &type);
	dprintf("  &flags: %p\n", &flags);

	RecursiveLocker lock(Lock());

	vnode = LookupVnode(id);
	if (vnode == NULL)
		return ENOENT;

	vnode->AcquireReference();
	struct stat stat;
	CHECK_RET(vnode->ReadStat(stat));
	type = stat.st_mode;
	flags = 0;

	return B_OK;
}
