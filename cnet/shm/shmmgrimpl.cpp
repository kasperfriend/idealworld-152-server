#include "shmmgrimpl.h"
#include <stdio.h> //test
namespace pwrdshmm
{


CShmMgrImpl::CShmMgrImpl()
{
}

CShmMgrImpl& CShmMgrImpl::GetInstance()
{
        static CShmMgrImpl inst;
        return inst;
}

bool CShmMgrImpl::Init(int create, int shmkey, char *shm_root, unsigned long long size)
{
        shmkey = shmkey<<16;
        _shm_key = shmkey;
        int shmid = -1;
        if (create)
        {
                //shmid = Shmget(shmkey, size, IPC_CREAT|IPC_EXCL|0600);
                shmid = Shmget(shmkey, size, IPC_CREAT|0600);
        }
        else
        {
                shmid = Shmget(shmkey, size, IPC_CREAT|0600);
        }

        if (shmid == -1)
        {
                return false;
        }
        _shm_root = shm_root;
        _shm_size = size;
        char* root_addr = (char*)Shmat(shmid, _shm_root, 0);
        if (root_addr != _shm_root)
        {
                if ((long)root_addr != -1)
                {
                        Shmdt(root_addr);
                }
                return false;
        }

        return true;
}


int CShmMgrImpl::Shmget(key_t key, size_t size, int flag)
{
        return shmget(key, size, flag);
}

int CShmMgrImpl::Shmctl(int shmid, int cmd, struct shmid_ds *buf)
{
        return shmctl(shmid, cmd, buf);
}

void *CShmMgrImpl::Shmat(int shmid, const void* addr, int flag)
{
        return shmat(shmid, addr, flag);
}

void *CShmMgrImpl::ShmMatch(int shmkey, size_t size)
{
        int shmid = Shmget(shmkey, size, IPC_CREAT|0600);
        if (shmid == -1)
        {
                return 0;
        }
        void * addr = Shmat(shmid, 0, 0);
        if ((long)addr == -1)
        {
                return 0;
        }
        //printf("shm_key %d addr 0x%p size %ld\n",shmkey, addr, size);
        return addr;
}

void *CShmMgrImpl::ShmReMatch(int shmkey, void*addr, size_t size)
{
        int shmid = Shmget(shmkey, size, IPC_CREAT|0600);
        if (shmid == -1)
        {
                return 0;
        }
        void * get_addr = Shmat(shmid, addr, 0);
        if ((long)addr == -1)
        {
                return 0;
        }
        if (get_addr != addr)
        {
                Shmdt(get_addr);
                return 0;
        }
        //printf("shm_key %d addr 0x%p size %ld\n",shmkey, addr, size);
        return addr;
}


int CShmMgrImpl::Shmdt(void* addr)
{
        //printf("shmdt addr 0x%p\n",addr);
        return shmdt(addr);
}

void CShmMgrImpl::RemoveShmkey(int shmkey, int size)
{
        int shmid = Shmget(shmkey, size, IPC_EXCL|0600);
        if (shmid == -1)
        {
                return;
        }

        Shmctl(shmid, IPC_RMID, NULL);
}
};


