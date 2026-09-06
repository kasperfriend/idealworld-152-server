#include "world.h"

// See objmanager.h: the heartbeat loop of obj_manager<> reaches into world
// (plane->w_activestate) and gobject_imp (DispatchMessage), which are only
// forward declared where the template is defined.
bool objman_plane_inactive(gobject * obj)
{
	return obj->plane->w_activestate != 1;
}

int objman_dispatch(gobject * obj, MSG & msg)
{
	return obj->imp->DispatchMessage(obj->plane, msg);
}

bool 
extern_object_manager::Init()
{
	int rst = SetTimer(g_timer,20*15,0);
	ASSERT(rst >=0);
	return true;

}

void 
extern_object_manager::Run()
{
	//每10秒一次heartbeat
	//估计一个服务的量为1000左右
	ONET::Thread::Mutex::Scoped keeper(_lock);
	OBJECT_MAP::iterator it = _map.begin();
	for(;it != _map.end(); )
	{
		object_entry & ent = it->second;
		if(--ent.ttl <= 0) 
		{
			__PRINTF("object %x removed\n",ent.id);
			_map.erase(it++); 
		}
		else
		{
			++it;
		}
	}
}

void 
extern_object_manager::OnTimer(int index,int rtimes)
{
	ONET::Thread::Pool::AddTask(this);
}

