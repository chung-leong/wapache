#include "stdafx.h"
#include "WapacheLock.h"
#define LOCK_MAX_WAIT INFINITE

inline bool WaitForObject(HANDLE h1, DWORD msec) {
	return WaitForSingleObject(h1, msec) != WAIT_TIMEOUT;
}

inline bool WaitForObjects(HANDLE h1, HANDLE h2, DWORD msec) {
	HANDLE h[2] = { h1, h2 };
	return WaitForMultipleObjects(2, h, TRUE, msec) != WAIT_TIMEOUT;
}

inline bool WaitForObjects(HANDLE h1, HANDLE h2, HANDLE h3, DWORD msec) {
	HANDLE h[3] = { h1, h2, h3 };
	return WaitForMultipleObjects(3, h, TRUE, msec) != WAIT_TIMEOUT;
}

WapacheLock::WapacheLock(void) {
	ReadLocks = 0;
	WriteLocks = 0;
	AccessMutex = CreateMutex(NULL, FALSE, NULL);
	WriteMutex = CreateMutex(NULL, FALSE, NULL);
	WritableEvent = CreateEvent(NULL, TRUE, TRUE, NULL);
	ReadableEvent = CreateEvent(NULL, TRUE, TRUE, NULL);
}

WapacheLock::~WapacheLock(void) {
	CloseHandle(AccessMutex);
	CloseHandle(WriteMutex);
	CloseHandle(WritableEvent);
	CloseHandle(ReadableEvent);
}

bool WapacheLock::Lock(int lock_type) {
	if(lock_type == WRITE_LOCK) {
		if(WaitForObjects(AccessMutex, WriteMutex, WritableEvent, LOCK_MAX_WAIT)) {
			ResetEvent(ReadableEvent);
			WriteLocks++;
			ReleaseMutex(AccessMutex);
			return true;
		}
	}
	else {
		if(WaitForObjects(AccessMutex, ReadableEvent, LOCK_MAX_WAIT)) {
			if(ReadLocks == 0) {
				ResetEvent(WritableEvent);
			}
			ReadLocks++;
			ReleaseMutex(AccessMutex);
			return true;
		}
	}
	return false;
}

bool WapacheLock::Unlock(int lock_type) {
	if(lock_type == WRITE_LOCK) {
		if(WaitForObject(AccessMutex, LOCK_MAX_WAIT)) {
			WriteLocks--;
			if(WriteLocks == 0) {
				SetEvent(ReadableEvent);
			}
			ReleaseMutex(WriteMutex);
			ReleaseMutex(AccessMutex);
			return true;
		}
	}
	else {
		if(WaitForObject(AccessMutex, LOCK_MAX_WAIT)) {
			ReadLocks--;
			if(ReadLocks == 0) {
				SetEvent(WritableEvent);
			}
			ReleaseMutex(AccessMutex);
			return true;
		}
	}
	return false;
}
