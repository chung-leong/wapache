#ifndef __WAPACHE_LOCK_H
#define __WAPACHE_LOCK_H

class WapacheLock {
public:
	WapacheLock(void);
	~WapacheLock(void);

	bool Lock(int type);
	bool Unlock(int type);

private:
	LONG ReadLocks;
	LONG WriteLocks;
	HANDLE AccessMutex;
	HANDLE WriteMutex;
	HANDLE ReadableEvent;
	HANDLE WritableEvent;
};

#define READ_LOCK		0
#define WRITE_LOCK		1

#endif