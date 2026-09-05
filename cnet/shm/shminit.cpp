#include "kmallocator.h"
#include "shmmgrimpl.h"
#include "ptmalloctor.h"
#include "kmalloc.h"
#include "ptmalloc.h"
#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include "shminit.h"


namespace pwrdshmm
{

	//É½Õ¯°æ ½«¾ÍÓÃµ½
bool CShmInit::ReadConf()
{
        char file_name[100] = {0};
        char buffer[101] = {0};
        pid_t pid = getpid();
        snprintf(file_name,100,"/proc/%d/cmdline",pid);

        int file_handle = open(file_name,O_RDONLY);
        if (file_handle == -1)
        {
                return false;
        }

        int buffer_len = read(file_handle,buffer,100);
        if (buffer_len <= 0)
        {
                close(file_handle);
                return false;
        }
        buffer[buffer_len] = 0;
        close(file_handle);

        int init_type = -1;
        int gs_num = 0;

        char* ppro_name_start = 0;
        char* pbuff = buffer;
        while (*pbuff)
        {
                if (*pbuff == '/')
                {
                        ppro_name_start = pbuff+1;
                }
                pbuff++;
        }

        pbuff++;
        if (ppro_name_start[0] == 'g'
            && ppro_name_start[1] == 's'
            )
        {
                for (int ix = 0; ix < 2; ix++)
                {
                        while (*pbuff++);
                }
                sscanf(pbuff,"gsalias%d.conf",&gs_num);
		if (gs_num == 0)
		{
			if (!strcmp(pbuff,"gsalias.conf"))
			{
				gs_num = 0;
			}
			else if (!strcmp(pbuff,"gsalias_base.conf"))
			{
				gs_num = 16;
			}
			else if (!strcmp(pbuff,"gsalias_kbattle.conf"))
			{
				gs_num = 17;
			}
			else if (!strcmp(pbuff,"gsalias_instance.conf"))
			{
				gs_num = 18;
			}
			else if (!strcmp(pbuff,"gsalias_ins1.conf"))
			{
				gs_num = 19;
			}
			else if (!strcmp(pbuff,"gsalias_ins2.conf"))
			{
				gs_num = 20;
			}
			else if (!strcmp(pbuff,"gsalias_ins3.conf"))
			{
				gs_num = 21;
			}
			else if (!strcmp(pbuff,"gsalias_ins4.conf"))
			{
				gs_num = 22;
			}
			else if (!strcmp(pbuff,"gsalias_solo1.conf"))
			{
				gs_num = 23;
			}
			else if (!strcmp(pbuff,"gsalias_solo2.conf"))
			{
				gs_num = 24;
			}
			else if (!strcmp(pbuff,"gsalias_solo3.conf"))
			{
				gs_num = 25;
			}
			else if (!strcmp(pbuff,"gsalias_solo4.conf"))
			{
				gs_num = 26;
			}
			else
			{
				return false;
			}
		}
                if (gs_num <0 || gs_num >= INIT_DELIVERY_KEY)
                {
                        return false;
                }
		init_type = INIT_TYPE_GS;
        }
        else if(ppro_name_start[0] == 'g'
                && ppro_name_start[1] == 'd'
                && ppro_name_start[2] == 'e')
        {
                init_type = INIT_TYPE_DELIVERY;
        }
        else if(ppro_name_start[0] == 'g'
                && ppro_name_start[1] == 'a'
                && ppro_name_start[2] == 'm')
        {
                init_type = INIT_TYPE_GAMEDBD;
        }


        if (init_type == INIT_TYPE_GS)
        {
                _shm_key = INIT_GS_KEY + gs_num;
        }
        else if (init_type == INIT_TYPE_DELIVERY)
        {
                _shm_key = INIT_DELIVERY_KEY;
        }
        else if (init_type == INIT_TYPE_GAMEDBD)
        {
                _shm_key = INIT_GAMEDBD_KEY;
        }
        else
        {
                return false;
        }
        while (*pbuff++);
        if (pbuff >= buffer+buffer_len)
        {
                _start_type = ST_NORMAL;
        }
        else
        {
                if (!strcmp(pbuff,"shm_static"))
		{
		        _start_type = ST_SHM_STATIC;
		}
                else if (!strcmp(pbuff,"shm_dynamic"))
                {
                        _start_type = ST_SHM_DYNAMIC;
                }
                else if (!strcmp(pbuff,"shm_restart"))
                {
                        _start_type = ST_SHM_RESTART;
                }
                else
                {
                        _start_type = ST_NORMAL;
                }
        }

        return true;
}

bool CShmInit::SetInitData()
{

#ifdef  _SHM_32_
        _shm_root = 0x50000000;
        _shm_size = 0xb8000000;
        _zone_num = 8;
#else
        _shm_root = 0x400000000000ull;
        _shm_size = 0x200000000ull;
        _zone_num = 10;
#endif
	 _start_type = ST_NORMAL;
        return true;
}


CShmInit::CShmInit()
{
        SHM_ASSERT(_mutex.Init());
}


CShmInit& CShmInit::GetInstance()
{
        static CShmInit inst;
        return inst;
}

int CShmInit::StartShmInit()
{
        static bool init = false;
        if (init)
        {
                return _start_type;
        }
        {
                SHM_MT(SHM_GUARD(&_mutex, obj, SHM_ASSERT(0)));
                if (init)
                {
                        return _start_type;
                }
                init = true;

                if (!SetInitData())
                {
                        SHM_ASSERT(0);
                        return -1;
                }
                if (!ReadConf())
                {
                        SHM_ASSERT(0);
                        return -1;
                }

                if (_start_type == ST_NORMAL)
                {
                        RemoveShmkey();
                        return _start_type;
                }
                int creat = -1;
                if (_start_type == ST_SHM_RESTART)
                {
                        creat = kmalloc_t::CREATE_TYPE_USED;
                }
                else if (_start_type == ST_SHM_STATIC)
                {
                        creat = kmalloc_t::CREATE_TYPE_STATIC;
                }
                else if (_start_type == ST_SHM_DYNAMIC)
                {
                        creat = kmalloc_t::CREATE_TYPE_DYNAMIC;
                }
                else
                {
                        SHM_ASSERT(0);
                }
                unsigned long long shm_root = _shm_root - shmmm_t::SHMMM_SIZE - kmalloc_t::KMALLOC_SIZE;
                unsigned long long shm_size = _shm_size + shmmm_t::SHMMM_SIZE + kmalloc_t::KMALLOC_SIZE;
                unsigned long long first_shm_size = INIT_FIRST_SHM_SIZE + shmmm_t::SHMMM_SIZE + kmalloc_t::KMALLOC_SIZE;

                if (!CShmMgrImpl::GetInstance().Init(creat, _shm_key, (char *)shm_root, first_shm_size))
                {
                        printf("shmmgrimpl fail\n");
                        SHM_ASSERT(0);
                        return -1;
                }
	        if (!CKernelMallocator::GetInstance().Init(creat, _shm_key ,shm_root + shmmm_t::SHMMM_SIZE, shm_size - shmmm_t::SHMMM_SIZE, _zone_num, INIT_PAEG_SIZE ))
	        {
                        printf("kmalloc fail\n");
                        SHM_ASSERT(0);
                        return -1;
	        }
                unsigned short used_page,free_page,unmem_page;
                if (!CKernelMallocator::GetInstance().TestMemoryPage(free_page,used_page,unmem_page))
	        {
                        printf("TestMemoryPage fail\n");
                        SHM_ASSERT(0);
                        return -1;
	        }else
	        {
		        printf("init ok free = %d used = %d\n",free_page, used_page);
	        }
                //CKernelMallocator::GetInstance().PrintZoneInfo();
        }
        return _start_type;
}

void CShmInit::RemoveShmkey()
{
        int shmkey = _shm_key;
        unsigned long long first_shm_size = INIT_FIRST_SHM_SIZE + shmmm_t::SHMMM_SIZE + kmalloc_t::KMALLOC_SIZE;
        CShmMgrImpl::GetInstance().RemoveShmkey(shmkey<<16, first_shm_size);
	printf("shmkey = 0x%x size = %d\n",shmkey,first_shm_size);
        int pagebegin = INIT_FIRST_SHM_SIZE/INIT_PAEG_SIZE;
        int pageend = _shm_size/INIT_PAEG_SIZE;
        int pageoffset = 1<<(_zone_num-1);
        int shmsize = (pageoffset+1)*INIT_PAEG_SIZE;
        for (int pageid = pagebegin; pageid < pageend; pageid += pageoffset)
        {
                shmkey = _shm_key<<16 | pageid;
                CShmMgrImpl::GetInstance().RemoveShmkey(shmkey, shmsize);
		printf("shmkey = 0x%x size = %d\n",shmkey,shmsize);
        }
}
int start_shm_init()
{
        return CShmInit::GetInstance().StartShmInit();
}


};


