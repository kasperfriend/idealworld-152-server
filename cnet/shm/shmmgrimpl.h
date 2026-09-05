#ifndef    _SHMMGRIMPL_H_
#define    _SHMMGRIMPL_H_
#include "shmcom.h"
#include <sys/shm.h>
#include <sys/types.h>
#include <sys/ipc.h>

namespace pwrdshmm
{

/******************************************** struct ********************************************/
#pragma pack(1)
typedef struct shmmm_t
{
        enum
	{
		SHMMM_SIZE = 2*1024*1024,
	};
        u16 _ver;
        u16 _reserve;
}shmmm_t;
#pragma pack()
/******************************************** end ********************************************/


class CShmMgrImpl
{
public:
        static CShmMgrImpl& GetInstance();
        bool Init(int create, int shmkey, char *shm_root, unsigned long long size);

        inline char* GetShmRoot() const
        {
                return _shm_root;
        }
        inline int GetShmKey() const
        {
                return _shm_key;
        }
        inline unsigned long long GetShmSize() const
        {
                return _shm_size;
        }
        int Shmdt(void* addr);
        void *Shmat(int shmid, const void* addr, int flag);
        int Shmctl(int shmid, int cmd, struct shmid_ds *buf);
        int Shmget(key_t key, size_t size, int flag);
        void *ShmMatch(int shmkey, size_t size);
        void *ShmReMatch(int shmkey, void*addr, size_t size);
        void RemoveShmkey(int shmkey, int size);
public:
        ~CShmMgrImpl(){};
private:
        CShmMgrImpl();
private:
        int _shm_key;
        char* _shm_root;
        unsigned long long _shm_size;

};
};

#endif




