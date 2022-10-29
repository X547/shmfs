#include "Shmfs.h"


static status_t shmfs_std_ops(int32 op, ...)
{
	switch (op) {
		case B_MODULE_INIT: {
			return B_OK;
		}
		case B_MODULE_UNINIT: {
			return B_OK;
		}
	}
	return B_ERROR;
}


static fs_vnode_ops sVnodeOps = {
	.lookup = [](fs_volume* volume, fs_vnode* dir, const char* name, ino_t* id) {
		return static_cast<ShmfsVnode*>(dir->private_node)->Lookup(name, *id);
	},
	.get_vnode_name = [](fs_volume* volume, fs_vnode* vnode, char* buffer, size_t bufferSize) {
		return static_cast<ShmfsVnode*>(vnode->private_node)->GetVnodeName(buffer, bufferSize);
	},
	.put_vnode = [](fs_volume* volume, fs_vnode* vnode, bool reenter) {
		return static_cast<ShmfsVnode*>(vnode->private_node)->PutVnode(reenter);
	},
	.read_symlink = [](fs_volume* volume, fs_vnode* link, char* buffer, size_t* bufferSize) {
		return static_cast<ShmfsVnode*>(link->private_node)->ReadSymlink(buffer, *bufferSize);
	},
	.create_symlink = [](fs_volume* volume, fs_vnode* dir, const char* name, const char* path, int mode) {
		return static_cast<ShmfsVnode*>(dir->private_node)->CreateSymlink(name, path, mode);
	},
	.unlink = [](fs_volume* volume, fs_vnode* dir, const char* name) {
		return static_cast<ShmfsVnode*>(dir->private_node)->Unlink(name);
	},
	.rename = [](fs_volume* volume, fs_vnode* fromDir, const char* fromName, fs_vnode* toDir, const char* toName) {
		return static_cast<ShmfsVnode*>(fromDir->private_node)->Rename(fromName, static_cast<ShmfsVnode*>(toDir->private_node), toName);
	},
	.access = [](fs_volume* volume, fs_vnode* vnode, int mode) {
		return static_cast<ShmfsVnode*>(vnode->private_node)->Access(mode);
	},
	.read_stat = [](fs_volume* volume, fs_vnode* vnode, struct stat* stat) {
		return static_cast<ShmfsVnode*>(vnode->private_node)->ReadStat(*stat);
	},
	.write_stat = [](fs_volume* volume, fs_vnode* vnode, const struct stat* stat, uint32 statMask) {
		return static_cast<ShmfsVnode*>(vnode->private_node)->WriteStat(*stat, statMask);
	},
	.create = [](fs_volume* volume, fs_vnode* dir, const char* name, int openMode, int perms, void** cookie, ino_t* newVnodeID) {
		return static_cast<ShmfsVnode*>(dir->private_node)->Create(name, openMode, perms, *newVnodeID);
	},
	.open = [](fs_volume* volume, fs_vnode* vnode, int openMode, void** cookie) {
		return static_cast<ShmfsVnode*>(vnode->private_node)->Open(openMode, *(ShmfsFileCookie**)&cookie);
	},
	.close = [](fs_volume* volume, fs_vnode* vnode, void* cookie) {
		return static_cast<ShmfsVnode*>(vnode->private_node)->Close((ShmfsFileCookie*)cookie);
	},
	.free_cookie = [](fs_volume* volume, fs_vnode* vnode, void* cookie) {
		return static_cast<ShmfsVnode*>(vnode->private_node)->FreeCookie((ShmfsFileCookie*)cookie);
	},
	.read = [](fs_volume* volume, fs_vnode* vnode, void* cookie, off_t pos, void* buffer, size_t* length) {
		return static_cast<ShmfsVnode*>(vnode->private_node)->Read((ShmfsFileCookie*)cookie, pos, buffer, *length);
	},
	.write = [](fs_volume* volume, fs_vnode* vnode, void* cookie, off_t pos, const void* buffer, size_t* length) {
		return static_cast<ShmfsVnode*>(vnode->private_node)->Write((ShmfsFileCookie*)cookie, pos, buffer, *length);
	},
	.create_dir = [](fs_volume* volume, fs_vnode* parent, const char* name, int perms) {
		return static_cast<ShmfsVnode*>(parent->private_node)->CreateDir(name, perms);
	},
	.remove_dir = [](fs_volume* volume, fs_vnode* parent, const char* name) {
		return static_cast<ShmfsVnode*>(parent->private_node)->RemoveDir(name);
	},
	.open_dir = [](fs_volume* volume, fs_vnode* vnode, void** cookie) {
		return static_cast<ShmfsVnode*>(vnode->private_node)->OpenDir(*(ShmfsDirIterator**)cookie);
	},
	.close_dir = [](fs_volume* volume, fs_vnode* vnode, void* cookie) {
		return static_cast<ShmfsVnode*>(vnode->private_node)->CloseDir((ShmfsDirIterator*)cookie);
	},
	.free_dir_cookie = [](fs_volume* volume, fs_vnode* vnode, void* cookie) {
		return static_cast<ShmfsVnode*>(vnode->private_node)->FreeDirCookie((ShmfsDirIterator*)cookie);
	},
	.read_dir = [](fs_volume* volume, fs_vnode* vnode, void* cookie, struct dirent* buffer, size_t bufferSize, uint32* num) {
		return static_cast<ShmfsVnode*>(vnode->private_node)->ReadDir((ShmfsDirIterator*)cookie, buffer, bufferSize, *num);
	},
	.rewind_dir = [](fs_volume* volume, fs_vnode* vnode, void* cookie) {
		return static_cast<ShmfsVnode*>(vnode->private_node)->RewindDir((ShmfsDirIterator*)cookie);
	},
};

static fs_volume_ops sVolumeOps = {
	.unmount = [](fs_volume* volume) {
		return static_cast<ShmfsVolume*>(volume->private_volume)->Unmount();
	},
	.read_fs_info = [](fs_volume* volume, struct fs_info* info) {
		return static_cast<ShmfsVolume*>(volume->private_volume)->ReadFsInfo(*info);
	},
	.get_vnode = [](fs_volume* volume, ino_t id, fs_vnode* vnode, int* type, uint32* flags, bool reenter) {
		vnode->ops = &sVnodeOps;
		return static_cast<ShmfsVolume*>(volume->private_volume)->GetVnode(id, *(ShmfsVnode**)&vnode->private_node, *type, *flags, reenter);
	},
};

static file_system_module_info sModule = {
	.info = {
		.name = "file_systems/shmfs" B_CURRENT_FS_API_VERSION,
		.std_ops = shmfs_std_ops,
	},
	.short_name = "shmfs",
	.pretty_name = "Shared Memory File System",
	.flags = B_DISK_SYSTEM_SUPPORTS_WRITING,
	.mount = [](fs_volume* volume, const char* device, uint32 flags, const char* args, ino_t* rootVnodeID) {
		volume->ops = &sVolumeOps;
		return ShmfsVolume::Mount(*(ShmfsVolume**)&volume->private_volume, volume, device, flags, args, *rootVnodeID);
	},
};

_EXPORT module_info* modules[] = {
	(module_info* )&sModule,
	NULL
};
