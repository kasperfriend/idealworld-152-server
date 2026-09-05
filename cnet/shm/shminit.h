#ifndef _SHM_INIT_
#define _SHM_INIT_
#include "lock.h"
namespace pwrdshmm
{
class CShmInit
{
public:
        enum
        {
                INIT_PAEG_SIZE = 1024*1024,
        };

        enum
        {
                INIT_GS_KEY = 1,
                INIT_DELIVERY_KEY = 32,
                INIT_GAMEDBD_KEY = 33,
        };

        enum
        {
                INIT_TYPE_GS = 0,
                INIT_TYPE_DELIVERY = 1,
                INIT_TYPE_GAMEDBD = 2,
        };
        enum
        {
                ST_SHM_RESTART = 0,
                ST_SHM_STATIC = 1,
                ST_SHM_DYNAMIC = 2,
                ST_NORMAL = 3,
        };
public:
        static const unsigned long long INIT_FIRST_SHM_SIZE = 0x40000000ull;
        static CShmInit& GetInstance();
        int StartShmInit();
        inline int GetStartType()
        {
                return _start_type;
        }
        void RemoveShmkey();
private:
        CShmInit();
        CShmInit(const CShmInit&);
        CShmInit& operator = (const CShmInit&);
private:
        bool ReadConf();
        bool SetInitData();
private:
        unsigned long long _shm_root;
        unsigned long long _shm_size;
        int _shm_key;
        int _zone_num;
        int _start_type;
        CPthreadMutex _mutex;
};


int start_shm_init();

};




#endif
