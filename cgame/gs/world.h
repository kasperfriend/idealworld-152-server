#ifndef __ONLINEGAME_GS_WORLD_H__
#define __ONLINEGAME_GS_WORLD_H__

#include <map>
#include <hashtab.h>
#include <amemory.h>
#include <timer.h>
#include <threadpool.h>
#include <common/types.h>
#include <set>
#include <glog.h>

#include "gimp.h"
#include "grid.h"
#include "msgqueue.h"
#include "io/msgio.h"
#include "terrain.h"
#include "template/itemdataman.h"
#include "worldmanager.h"
#include "npcgenerator.h"
#include "commondata.h"
#include "usermsg.h"
#include "staticmap.h"

namespace NPCMoveMap
{
	class CMap;
}
class trace_manager2;

class MsgDispatcher;
class GSvrPool;
class CNPCGenMan;

class  world_data_ctrl
{
//Õâ¸öÀàÓÃÓÚÊÀ½çµÄÊı¾İ¿ØÖÆ£¬»áÌí¼ÓÒ»ÏµÁĞµÄĞéº¯Êı
	public:
	virtual ~world_data_ctrl() {}
	virtual world_data_ctrl * Clone() = 0;
	virtual void Reset() = 0;
	virtual void Tick(world * pPlane) = 0;
	virtual void BattleFactionSay(int faction, const void * buf, size_t size, char emote_id, const void * aux_data, size_t dsize, int self_id, int self_level){}
	virtual void BattleSay(const void * buf, size_t size){};
	virtual void OnSetCommonValue(int key, int value){}
	virtual void OnTriggerSpawn(int controller_id){}
	virtual void OnClearSpawn(int controller_id){}
	virtual void OnServerShutDown(){}
	//°ïÅÉ»ùµØÏà¹Ø
	virtual int GetFactionId(){ return 0; }
	virtual bool LevelUp(){ return false; }
	virtual bool SetTechPoint(size_t tech_index){ return false; }
	virtual bool ResetTechPoint(world * pPlane, size_t tech_index){ return false; }
	virtual bool Construct(world * pPlane, int id, int accelerate){ return false; }
	virtual bool HandInMaterial(int id, size_t count){ return false; }
	virtual bool HandInContrib(int contrib){ return false; }
	virtual bool MaterialExchange(size_t src_index,size_t dst_index,int material){ return false; }
	virtual bool Dismantle(world * pPlane, int id){ return false; }
	virtual bool GetInfo(int roleid, int cs_index, int cs_sid){ return false; }
	//¹úÕ½Õ½³¡Ïà¹Ø
	virtual void UpdatePersonalScore(bool offense, int roleid, int combat_time, int attend_time, int dmg_output, int dmg_output_weighted, int dmg_endure, int dmg_output_npc){}
	virtual void OnPlayerDeath(gplayer * pPlayer, const XID & killer, int player_soulpower, const A3DVECTOR & pos){}
	virtual bool PickUpFlag(gplayer * pPlayer){ return false;}
	virtual bool HandInFlag(gplayer * pPlayer){ return false;}
	virtual void UpdateFlagCarrier(int roleid, const A3DVECTOR & pos){}
	virtual void OnTowerDestroyed(world * pPlane, bool offense, int tid){}
	virtual void OccupyStrongHold(int mine_tid, gplayer* pPlayer){};
	virtual bool GetStrongholdNearby(bool offense, const A3DVECTOR &opos, A3DVECTOR &pos, int & tag){return false;}
	virtual bool GetPersonalScore(bool offense, int roleid, int& combat_time, int& attend_time, int& kill_count, int& death_count, int& country_kill_count,int& country_death_count){ return false; }
	virtual bool GetCountryBattleInfo(int & attacker_count, int & defender_count){ return false; }
	virtual void GetStongholdState(int roleid, int cs_index, int cs_sid){}
	virtual bool GetLiveShowResult(int roleid, int cs_index, int cs_sid, world* pPlane){ return false; }

	//Õ½³µÕ½³¡Ïà¹Ø
	virtual void UpdatePersonalScore(int roleid, int kill, int death, int score){}
	virtual void AddChariot(int type, int chariot) {}
	virtual void DelChariot(int type, int chariot) {}
	virtual void GetChariots(int type, abase::hash_map<int, int> & chariot_map) {}
};

class world
{
	typedef abase::hashtab<int,int,abase::_hash_function,abase::fast_alloc<> >	query_map;//ÓÃ»§µÄ²éÑ¯±í
	extern_object_manager w_ext_man;	//±£´æÆäËû·şÎñÆ÷ÉÏ¶ÔÏóĞÅÏ¢µÄ¹ÜÀíÆ÷

	grid 	w_grid;
	int 	w_index;	//ÊÀ½çÇøÓòµÄË÷Òı
	query_map w_player_map;	//Íæ¼ÒµÄ²éÕÒ±í
	query_map w_npc_map;	//ÔÚ±¾µØµÄÍâ²¿npc²éÑ¯±í
	int	w_pmap_lock;	//player µÄuserid -> index idµÄmapËø
	int	w_nmap_lock;	//npc Íâ²¿npc-->±¾µØÁ¬½ÓµÄ²éÑ¯±í
	int 	w_message_counter[GM_MSG_MAX];	//ÏûÏ¢ÊıÄ¿µÄ¼ÆÊı±í
	npc_generator 	w_npc_gen; //¹ÖÎïÉú³É¹ÜÀíÆ÷
	
	map_generator * w_map_generator;
	CTerrain * w_terrain;
	NPCMoveMap::CMap * w_movemap;
	trace_manager2 * w_traceman;
public:
	struct off_node_t{
		int idx_off;
		int x_off;
		int z_off;
		off_node_t(grid & grid,int offset_x,int offset_y):x_off(offset_x),z_off(offset_y)
		{
			idx_off = offset_y*grid.reg_column + offset_x;
		}
		bool operator==(const off_node_t & rhs) const 
		{
			return rhs.idx_off == idx_off;
		}
	};
	abase::vector<off_node_t,abase::fast_alloc<> > w_off_list;
	int	w_near_vision;		//ÔİÊ±Ã»ÓĞÊ¹ÓÃ£¬½üµãµÄË÷Òı
	int	w_far_vision;		//×îÔ¶µÄ¾àÀëËùº­¸ÇµÄ·¶Î§
	int	w_true_vision;		//ÍêÈ«¿ÉÊÓµÄ·¶Î§,ÔİÊ±Ã»ÓĞÊ¹ÓÃ
	int	w_plane_index;		//ÔÚÊÀ½çÖĞµÄÎ»ÃæË÷Òı
	int	w_player_count;		//±¾ÊÀ½çÖĞÍæ¼ÒµÄÊıÄ¿
	float	w_vision;		//ÊÓÒ°·¶Î§£¬ºÍfar_vision¶ÔÓ¦µÄ¾àÀë
	world_manager * w_world_man;
	int 	w_obsolete;		//ÓÉmanagerÊ¹ÓÃÕâ¸ö±äÁ¿
	int 	w_life_time;	//worldÊ£ÓàÉú´æÊ±¼ä,-1´ú±íÎŞÏŞ,0´ú±íÒÑ¾­¹ıÆÚ,ÓÉmanagerÊ¹ÓÃÕâ¸ö±äÁ¿
	instance_hash_key w_ins_key;	//¸±±¾Ê¹ÓÃµÄinstance_key £¬ ÓÉmanagerÊ¹ÓÃÕâ¸ö±äÁ¿
	int	w_activestate;		//¼¤»î×´Ì¬ 0:Î´¼¤»î 1:¼¤»î 2:ÀäÈ´  ÓÉmanagerÀ´¿ØÖÆ
	int	w_index_in_man;		//ÔÚ¹ÜÀíÆ÷ÖĞµÄË÷Òı£¬ÎªMsgQueue2ºÍmanagerËùÊ¹ÓÃ
	int	w_create_timestamp;	//´´½¨µÄÊ±¼ä´Á£¬ÓÉmanagerÊ¹ÓÃ 
	int	w_destroy_timestamp;	//É¾³ıµÄÊ±¼ä´Á£¬ÓÉmanagerÊ¹ÓÃ ²»ÊÇËùÓĞµÄ¸±±¾Õâ¸ö¶¼ÓĞĞ§µÄ
	int	w_ins_kick;		//ÊÇ·ñÌß³öinstance key²»·ûºÏµÄÍæ¼Ò
	int	w_battle_result;	//¸øÕ½³¡ÓÃµÄ Õ½³¡½á¹û
	int	w_offense_goal;		//Õ½³¡¹¥·½Ä¿±ê
	int	w_offense_cur_score;	//Õ½³¡¹¥·½µÃ·Ö
	int	w_defence_goal;		//Õ½³¡ÊØ·½Ä¿±ê
	int	w_defence_cur_score;	//Õ½³¡ÊØ·½µÃ·Ö
	int 	w_end_timestamp; 	//Õ½³¡½áÊøÊ±¼ä£¬µ½´ËÊ±¼äÊ±£¬ËùÓĞÍæ¼Ò¶¼½«±»×Ô¶¯Ìß³ö 
					//Ö»ÓĞbattle_resultÓĞĞ§ºó,´ËÖµ²Å»á±»Ê¹ÓÃ

	int 	w_player_node_lock;	//Íæ¼ÒËøÊı¾İ£¬ÓÃÓÚ¸üĞÂÊÀ½çÖĞµÄÍæ¼ÒÁĞ±íÊ±µÄËø
	cs_user_map w_player_node_list;	//Íæ¼ÒÊı¾İ
	common_data w_common_data;	//±¾ÊÀ½çÍ¨ÓÃÊı¾İ
	abase::vector<char>	w_collision_flags;	//npc dyn mine Åö×²ÊÇ·ñ¼¤»îµÄ±êÖ¾
	int w_scene_service_npcs_lock;
	abase::static_multimap<int, int> w_scene_service_npcs;	//È«³¡¾°·şÎñnpc£¬Ê¹ÓÃ·şÎñÊ±ÎŞ¾àÀëÏŞÖÆ
public:
	void SetCommonValue(int key,int value, bool notify_world_ctrl = true);
	int GetCommonValue(int key);
	int ModifyCommonValue(int key, int offset);
	void AddPlayerNode(gplayer * pPlayer);
	void DelPlayerNode(gplayer * pPlayer);
	void SyncPlayerWorldGen(gplayer* pPlayer);
	void AddSceneServiceNpc(int tid, int id);
	void DelSceneServiceNpc(int tid, int id);
	void GetSceneServiceNpc(abase::vector<int> & list);
private:	
	void CommonDataNotify(int key, int value);
public:
//³õÊ¼»¯º¯Êı
	world();
	~world();
	bool 	Init(int world_index);
	void 	InitManager(world_manager * man) { w_world_man = man;}
	bool 	InitNPCGenerator(CNPCGenMan & npcgen);
	bool    InitNPCGenerator(CNPCGenMan & ctrldata, npcgen_data_list& npcgen_list);
	bool	TriggerSpawn(int condition, bool notify_world_ctrl = true);
	bool 	ClearSpawn(int condition, bool notify_world_ctrl = true);
	void 	InitTimerTick();
	bool 	CreateGrid(int row,int column,float step,float startX,float startY);
	int	BuildSliceMask(float near,float far);			//´´½¨¾àÀë²éÑ¯ËùĞèÒªµÄmask

	void 	DuplicateWorld(world * dest) const;	//¸´ÖÆÊÀ½ç£¬³ıÁËNPCÉú³ÉÆ÷ .....
	
	inline world_manager * GetWorldManager() { return w_world_man;}
	//·ÖÅäÒ»¸öNPCÊı¾İ£¬·µ»ØÒ»¸öÉÏÁËËøÁËNPC½á¹¹
	inline gnpc 	*AllocNPC() 
	{ 
		gnpc *pNPC = w_world_man->AllocNPC(); 
		if(pNPC) pNPC->plane = this;
		return pNPC;
	}
	inline void 	FreeNPC(gnpc* pNPC) 
	{ 
		ASSERT(pNPC->plane == this);
		pNPC->plane = NULL;
		return w_world_man->FreeNPC(pNPC); 
	}

	inline bool CheckPlayerDropCondition()
	{
		return w_world_man->CheckPlayerDropCondition();
	}

	//ÉèÖÃ´ËnpcÎªÍâ²¿npc
	inline void 	SetNPCExtern(gnpc * pNPC)
	{
		spin_autolock alock(w_nmap_lock); 
		w_npc_map.put(pNPC->ID.id,GetNPCIndex(pNPC));
	}

	inline int 	GetNPCExternID(int id)
	{
		spin_autolock alock(w_nmap_lock);
		query_map::pair_type p = w_npc_map.get(id);
		if(!p.second) return -1;
		return *p.first;
	}

	inline void 	EraseExternNPC(int id)
	{
		spin_autolock alock(w_nmap_lock);
		w_npc_map.erase(id);
	}


	//·ÖÅäÒ»¸öMatterÊı¾İ£¬·µ»ØÒ»¸öÉÏÁËËøµÄMatter½á¹¹
	inline gmatter *AllocMatter() 
	{ 
		gmatter * pMatter = w_world_man->AllocMatter(); 
		if(pMatter) pMatter->plane = this;
		return pMatter;
	}
	inline void 	FreeMatter(gmatter *pMatter) 
	{ 
		ASSERT(pMatter->plane == this);
		pMatter->plane = NULL;
		return w_world_man->FreeMatter(pMatter); 
	}

	//	Ôö¼Ó/É¾³ıÍæ¼ÒºÍ¶ÔÏóµÄº¯Êı
	//·ÖÅäÒ»¸öÍæ¼ÒÊı¾İ£¬²¢·µ»ØÒ»¸öËø¶¨µÄÍæ¼Ò½á¹¹
	inline gplayer *AllocPlayer() 
	{ 
		gplayer * pPlayer = w_world_man->AllocPlayer(); 
		if(pPlayer) 
		{
			pPlayer->plane = this;
			interlocked_increment(&w_player_count);
		}
		return pPlayer;
	}

	inline void 	FreePlayer(gplayer * pPlayer)
	{
		w_world_man->PlayerLeaveThisWorld(w_plane_index,pPlayer->ID.id);
		ASSERT(pPlayer->plane == this);
		interlocked_decrement(&w_player_count);
		pPlayer->plane = NULL;
		return w_world_man->FreePlayer(pPlayer);
	}

	//AttachPlayer,DetachPlayer ÓÃÓÚÍæ¼ÒÔÚ±¾gs½øĞĞ»»Ïß²Ù×÷Ê±Ê¹ÓÃ
	inline void AttachPlayer(gplayer * pPlayer)
	{
		ASSERT(pPlayer->plane == NULL);
		pPlayer->plane = this;
		interlocked_increment(&w_player_count);
	}

	inline void DetachPlayer(gplayer * pPlayer)
	{
		w_world_man->PlayerLeaveThisWorld(w_plane_index,pPlayer->ID.id);
		ASSERT(pPlayer->plane == this);
		interlocked_decrement(&w_player_count);
		pPlayer->plane = NULL;
	}

	inline int GetPlayerInWorld() 
	{
		return w_player_count; 
	}

	inline void InsertPlayerToMan(gplayer *pPlayer) 
	{ 
		w_world_man->InsertPlayerToMan(pPlayer);
	}
	inline void RemovePlayerToMan(gplayer *pPlayer) 
	{ 	
		w_world_man->RemovePlayerToMan(pPlayer);
	}

	int InsertPlayer(gplayer *);		//¸ù¾İÎ»ÖÃ£¬²åÈëÒ»¸ö¶ÔÏóµ½ÊÀ½çÖĞ£¬·µ»Ø²åÈëµÄÇøÓòË÷ÒıºÅ
	int InsertNPC(gnpc*);			//¸ù¾İÎ»ÖÃ£¬²åÈëÒ»¸ö¶ÔÏóµ½ÊÀ½çÖĞ£¬·µ»Ø²åÈëµÄÇøÓòË÷ÒıºÅ
	int InsertMatter(gmatter *);		//¸ù¾İÎ»ÖÃ£¬²åÈëÒ»¸ö¶ÔÏóµ½ÊÀ½çÖĞ£¬·µ»Ø²åÈëµÄÇøÓòË÷ÒıºÅ
	
	void RemovePlayer(gplayer *pPlayer); 	//´ÓÊÀ½çÖĞÒÆ³öÒ»¸ö¶ÔÏó£¬²»free
	void RemoveNPC(gnpc *pNPC);		//´ÓÊÀ½çÖĞÒÆ³öÒ»¸ö¶ÔÏó£¬²»free
	void RemoveMatter(gmatter *pMatter);	//´ÓÊÀ½çÖĞÒÆ³öÒ»¸ö¶ÔÏó£¬²»free

	//´Ó¹ÜÀíÆ÷ÖĞÒÆ³öNPC£¬ÓÃÓÚ²»ÔÚ³¡¾°ÖĞµÄnpc
	inline void RemoveNPCFromMan(gnpc * pNPC)
	{
		w_world_man->RemoveNPCFromMan(pNPC);
	}

	inline void RemoveMatterFromMan(gmatter * pMatter)
	{
		w_world_man->RemoveMatterFromMan(pMatter);
	}

	bool IsPlayerExist(int player_id);	//²éÑ¯Íæ¼ÒÊÇ·ñÔÚÊÀ½çÖĞ£¨»òÕßÔÚÆäËû·şÎñÆ÷ÖĞ£©

	void Release();

public:
//	È¡µÃÊôĞÔµÄinlineº¯Êı
	inline gmatter * GetMatterByIndex(size_t index) const  { return w_world_man->GetMatterByIndex(index);}
	inline gplayer*  GetPlayerByIndex(size_t index) const   {return w_world_man->GetPlayerByIndex(index);}
	inline gnpc* 	 GetNPCByIndex(size_t index) const   { return w_world_man->GetNPCByIndex(index);}
	inline size_t GetPlayerIndex(gplayer *pPlayer)  const  { return w_world_man->GetPlayerIndex(pPlayer);}
	inline size_t GetMatterIndex(gmatter *pMatter)  const  { return w_world_man->GetMatterIndex(pMatter);}
	inline size_t GetNPCIndex(gnpc *pNPC)  const  { return w_world_man->GetNPCIndex(pNPC);}
	inline grid&	 GetGrid() { return w_grid;}
	inline extern_object_manager & GetExtObjMan() { return  w_ext_man;}
	inline const rect & GetLocalWorld() { return w_grid.local_region;}
	inline bool PosInWorld(const A3DVECTOR & pos)
	{
		return w_grid.IsLocal(pos.x,pos.z);
	}

	inline bool MapPlayer(int uid,int index) { 
		spin_autolock alock(w_pmap_lock); 
		return w_player_map.put(uid,index);
	}
	
	inline int UnmapPlayer(int uid) {
		spin_autolock alock(w_pmap_lock);
		return w_player_map.erase(uid);
	}
	
	inline int FindPlayer(int uid) {
		spin_autolock alock(w_pmap_lock);
		query_map::pair_type p = w_player_map.get(uid);
		if(!p.second) return -1;
		return *p.first;
	}

	inline gplayer * GetPlayerByID(int uid)
	{
		int index = FindPlayer(uid);
		if(index < 0) return NULL;
		return GetPlayerByIndex(index);
	}

	inline void ExtManRefresh(int id, const A3DVECTOR &pos, const extern_object_manager::object_appear & obj)
	{
		w_ext_man.Refresh(id,pos,obj);
	}

	inline void ExtManRefreshHP(int id, const A3DVECTOR &pos, int hp)
	{
		w_ext_man.RefreshHP(id,pos,hp);
	}

	inline void ExtManRemoveObject(int id)
	{
		w_ext_man.RemoveObject(id);
	}
	
	inline int GetPlayerCount()
	{
		spin_autolock alock(w_pmap_lock); 
		return w_player_map.size();
	}

	//
	enum
	{
		QUERY_OBJECT_STATE_ACTIVE = 0x01,
		QUERY_OBJECT_STATE_ZOMBIE = 0x02,
		QUERY_OBJECT_STATE_DISCONNECT = 0x04,
	};
	struct object_info
	{
		int state;
		A3DVECTOR pos;
		float body_size;
		int race;
		int faction;
		int level;
		int hp;
		int mp;
		int max_hp;
		int invisible_degree;
		int anti_invisible_degree;
		unsigned int object_state;		//½öplayer npcÓĞĞ§
		unsigned int object_state2;		//½öplayer npcÓĞĞ§
		int mafia_id;
	};

	bool QueryObject(const XID & id,object_info & info);	//²éÑ¯Ò»¸öÆäËû¶ÔÏóµÄ×´Ì¬

public:
	int RebuildMapRes();
	inline float GetHeightAt(float x, float z)
	{
		if(w_terrain) return w_terrain->GetHeightAt(x, z);
		return w_world_man->GetMapRes().GetUniqueTerrain()->GetHeightAt(x, z);	
	}

	inline NPCMoveMap::CMap * GetMoveMap()
	{
		if(w_movemap) return w_movemap;
		return w_world_man->GetMapRes().GetUniqueMoveMap();
	}

	inline trace_manager2 * GetTraceMan()
	{
		if(w_traceman) return w_traceman;
		return w_world_man->GetMapRes().GetUniqueTraceMan();
	}
	
	inline const map_generator* GetMapGen() const { return w_map_generator; }

	inline int GetBlockID(float x, float z) const {	return w_map_generator ? w_map_generator->GetBlockID(x,z) : 0;}
	inline int GetRoomIndex(float x, float z) const { return w_map_generator ? w_map_generator->GetRoomIndex(x,z) : 0;}
	inline bool GetTownPosition(gplayer_imp *pImp, const A3DVECTOR &opos, A3DVECTOR &pos, int & tag) const { return w_map_generator ? w_map_generator->GetTownPosition(pImp,opos,pos,tag) : false; }
	inline bool SetIncomingPlayerPos(gplayer * pPlayer, const A3DVECTOR & origin_pos) const { return w_map_generator ? w_map_generator->SetIncomingPlayerPos(pPlayer,origin_pos) : false; }	
private:
	//´¦ÀíÏûÏ¢µÄÄÚ²¿º¯Êı
	gobject * locate_object_from_msg(const MSG & msg);		//¸ù¾İÏûÏ¢¶¨Î»¶ÔÏó
	void 	try_dispatch_extern_msg(const MSG & msg);
public:
	void RunTick();		//ÓÉmanager¿ØÖÆµ÷ÓÃ»òÕßÄÚ²¿×Ô¶¯µ÷ÓÃ
	void ResetWorld();	//ÖØÖÃÊÀ½ç£¬Ö»ÓĞ¸±±¾²Å»áµ÷ÓÃÕâ¸ö£¨Õâ¸ö²Ù×÷»áÖØÉúËùÓĞµÄ¹ÖÎïµÈ£©
	void DumpMessageCount();

	void SetWorldCtrl(world_data_ctrl * ctrl);
	world_data_ctrl * w_ctrl;
private:
	friend class MsgQueue;
public:

//·¢ËÍÏûÏ¢µÄº¯Êı
	/*
	 *	Ö¸¶¨Ä¿±êµÄ·¢ËÍÏûÏ¢£¬ÒÑ¾­Íê³ÉÁË¶ÔÏóÈ·¶¨£¬Òò´Ë²»ÔÙĞèÒªÔÙ´Î½øĞĞ²éÕÒ
	 */
	int DispatchMessage(gobject * obj, const MSG & message);

	/*
	 *	ÑÓÊ±·¢ËÍÒ»ÌõÏûÏ¢·¢ËÍÒ»ÌõÏûÏ¢
	 *	delay_tickÒÔ50msÎªµ¥Î»
	 *	ÔÚconfig.hÀï¶¨ÒåÁË×î´óµÄdelay_tick MAX_MESSAGE_DELAY
	 *	PostMessageQueue ÊÇÓÃSendLazyMessage(message,0)À´ÊµÏÖµÄ
	 *	Èç¹ûdelay_tick¹ı´ó£¬ÄÇÃ´ºÎÊ±·¢ËÍ¸ÃÏûÏ¢ÊÇÎ´¶¨ÒåµÄ
	 */
	inline void PostLazyMessage(const MSG & message, size_t delay_tick)
	{
		w_world_man->PostMessage(this,message,delay_tick);
	}

	/*
	 *	·¢ËÍÒ»ÌõÏûÏ¢£¬¿ÉÄÜ»áÑÓ³Ù·¢ËÍ£¬Ò²¿ÉÄÜ»á½Ï¿ìµÄ·¢ËÍ
	 *	ÔÚÃ¿´ÎSendMessageÖ®ºó£¬¶¼»á¼ì²âÊÇ·ñÓĞÏûÏ¢µÈ´ı·¢ËÍ
	 */
	inline void PostLazyMessage(const MSG & message)
	{
		w_world_man->PostMessage(this,message);
	}

	/*
	 *	¸øÒ»¶Ñplayer·¢ËÍÏûÏ¢
	 */
	inline void SendPlayerMessage(size_t count, int * player_list, const MSG & msg)
	{
		w_world_man->PostPlayerMessage(this,player_list,count,msg);
	}

	/*
	 *	¸øÒ»¶ÑID·¢ËÍÏûÏ¢
	 */
	void SendMessage(const XID * first, const XID * last, const MSG & msg)
	{
		w_world_man->PostMessage(this,first,last,msg);
	}

	/*
	 *	·¢ËÍÒ»ÌõÏûÏ¢µ½Ô¶³Ì·şÎñÆ÷
	 *	·Ö·¢²Ù×÷ÓÉÔ¶³Ì·şÎñÆ÷Íê³É
	 */
	void SendRemoteMessage(int id,const MSG & msg)
	{
		w_world_man->SendRemoteMessage(id, msg);
	}

	/*
	
	*  »ñÈ¡player_listµÄ´óĞ¡£¬Ò²¾ÍÊÇ±»¹ã²¥µÄÈËÊı
	*/
	int GetSpherePlayerListSize(const A3DVECTOR& target,float fRadius);
	
	/*
	 *	½«Ò»ÌõÏûÏ¢·¢ËÍµ½ºÍ¸ø³öµÄrect²ÎÊıÏà½»µÄÔ¶³Ì·şÎñÆ÷ÉÏ
	 */
	int BroadcastSvrMessage(const rect & rt,const MSG & message,float extend_size)
	{
		return w_world_man->BroadcastSvrMessage(rt,message,extend_size);
	}

	/*
	 *	¹ã²¥ÏûÏ¢£¬°´ÕÕ¾àÀë½«°ü×ª·¢¸øÖÜÎ§µÄËùÓĞ¶ÔÏ
	 *	msg.target µÄÀàĞÍ¾ö¶¨ÁËÊÕµ½Õß£¬msg.targetµÄID±ØĞëÎª-1
	 *	maskÓÃÓÚ¹ıÂËÏûÏ¢½ÓÊÕ¶ÔÏó
	 *	ÔÚ×ùÕâ¸ö¹ã²¥Ê±£¬»á×Ô¶¯ÅĞ¶ÏÊÇ·ñÒª×ª·¢µ½ÆäËûµÄ·şÎñÆ÷ÉÏ
	 *	msg.source²»»áÊÕµ½Õâ¸ö¹ã²¥ÏûÏ¢
	 */
	int BroadcastMessage(const MSG & message,float fRadius,int mask); 		

	/*
	 *	¹ã²¥ºĞĞÎÏûÏ¢£¬°´ÕÕ¾àÀë½«°ü×ª·¢¸øÖÜÎ§µÄËùÓĞ¶ÔÏó
	 *	msg.target µÄÀàĞÍ¾ö¶¨ÁËÊÕµ½Õß£¬msg.targetµÄID±ØĞëÎª-1
	 *	msg.source²»»áÊÕµ½Õâ¸ö¹ã²¥ÏûÏ¢
	 */
	int BroadcastLocalBoxMessage(const MSG & message,const rect & rt);

	/*
	 *	¹ã²¥ÇòĞÎÏûÏ¢£¬°´ÕÕ¾àÀë½«°ü×ª·¢¸øÖÜÎ§µÄËùÓĞ¶ÔÏó
	 *	msg.target µÄÀàĞÍ¾ö¶¨ÁËÊÕµ½Õß£¬msg.targetµÄID±ØĞëÎª-1
	 *	maskÓÃÓÚ¹ıÂËÏûÏ¢½ÓÊÕ¶ÔÏó(ÒÑ¾­±»È¡Ïû)
	 *	ÔÚ×ùÕâ¸ö¹ã²¥Ê±£¬»á×Ô¶¯ÅĞ¶ÏÊÇ·ñÒª×ª·¢µ½ÆäËûµÄ·şÎñÆ÷ÉÏ
	 *	msg.source²»»áÊÕµ½Õâ¸ö¹ã²¥ÏûÏ¢
	 */
	int BroadcastSphereMessage(const MSG & message,const A3DVECTOR & target, float fRadius);

	/*
	 *	¹ã²¥ÖùĞÎÏûÏ¢£¬°´ÕÕ¾àÀë½«°ü×ª·¢¸øÖÜÎ§µÄËùÓĞ¶ÔÏó,¸Ã¶ÔÏó±ØĞëÔÚÖùÖĞ
	 *	ÖùµÄÆğÊ¼×ø±êÔÚmessageÖĞ£¬ÖÕµãÊÇtarget
	 *	msg.target µÄÀàĞÍ¾ö¶¨ÁËÊÕµ½Õß£¬msg.targetµÄID±ØĞëÎª-1
	 *	maskÓÃÓÚ¹ıÂËÏûÏ¢½ÓÊÕ¶ÔÏó
	 *	ÔÚ×ùÕâ¸ö¹ã²¥Ê±£¬»á×Ô¶¯ÅĞ¶ÏÊÇ·ñÒª×ª·¢µ½ÆäËûµÄ·şÎñÆ÷ÉÏ
	 *	msg.source²»»áÊÕµ½Õâ¸ö¹ã²¥ÏûÏ¢
	 */
	int BroadcastCylinderMessage(const MSG & message,const A3DVECTOR & target, float fRadius);

	/*
	 *	¹ã²¥×µĞÎÏûÏ¢£¬ÔÚ×µÖĞµÄ¶ÔÏó»áÊÕµ½Õâ¸öÏûÏ¢
	 *	Ô²×¶µÄÔ²ĞÄ¼´ÎªÏûÏ¢µÄ·¢³öµã
	 *	msg.target µÄÀàĞÍ¾ö¶¨ÁËÊÕµ½Õß£¬msg.targetµÄID±ØĞëÎª-1
	 *	maskÓÃÓÚ¹ıÂËÏûÏ¢½ÓÊÕ¶ÔÏó
	 *	ÔÚ×ùÕâ¸ö¹ã²¥Ê±£¬»á×Ô¶¯ÅĞ¶ÏÊÇ·ñÒª×ª·¢µ½ÆäËûµÄ·şÎñÆ÷ÉÏ
	 *	msg.source²»»áÊÕµ½Õâ¸ö¹ã²¥ÏûÏ¢
	 */
	int BroadcastTaperMessage(const MSG & message,const A3DVECTOR & target,float fRadius,float cos_halfangle);


	/*
	 *	Í¬BroadcastMessage£¬Î¨Ò»µÄÇø±ğÊÇÖ»ÔÚ±¾µØ×öÏàÓ¦µÄ×ª·¢²Ù×÷
	 *	BroadcastMessageµÄ±¾µØ·¢ËÍÊÇÍ¨¹ıµ÷ÓÃ±¾º¯ÊıÍê³ÉµÄ
	 */
	int BroadcastLocalMessage(const MSG & message,float fRadius,int mask);

	/*
	 *	¹ã²¥ÇòĞÎÏûÏ¢£¬°´ÕÕ¾àÀë½«°ü×ª·¢¸øÖÜÎ§µÄËùÓĞ¶ÔÏó
	 *	msg.target µÄÀàĞÍ¾ö¶¨ÁËÊÕµ½Õß£¬msg.targetµÄID±ØĞëÎª-1
	 *	maskÓÃÓÚ¹ıÂËÏûÏ¢½ÓÊÕ¶ÔÏó
	 *	ÔÚ×ùÕâ¸ö¹ã²¥Ê±£¬»á×Ô¶¯ÅĞ¶ÏÊÇ·ñÒª×ª·¢µ½ÆäËûµÄ·şÎñÆ÷ÉÏ
	 *	msg.source²»»áÊÕµ½Õâ¸ö¹ã²¥ÏûÏ¢
	 */
	int BroadcastLocalSphereMessage(const MSG & message,const A3DVECTOR & target, float fRadius);

	/*
	 *	¹ã²¥ÖùĞÎÏûÏ¢£¬°´ÕÕ¾àÀë½«°ü×ª·¢¸øÖÜÎ§µÄËùÓĞ¶ÔÏó,¸Ã¶ÔÏó±ØĞëÔÚÖùÖĞ
	 *	ÖùµÄÆğÊ¼×ø±êÔÚmessageÖĞ£¬ÖÕµãÊÇtarget
	 *	msg.target µÄÀàĞÍ¾ö¶¨ÁËÊÕµ½Õß£¬msg.targetµÄID±ØĞëÎª-1
	 *	maskÓÃÓÚ¹ıÂËÏûÏ¢½ÓÊÕ¶ÔÏó
	 *	ÔÚ×ùÕâ¸ö¹ã²¥Ê±£¬»á×Ô¶¯ÅĞ¶ÏÊÇ·ñÒª×ª·¢µ½ÆäËûµÄ·şÎñÆ÷ÉÏ
	 *	msg.source²»»áÊÕµ½Õâ¸ö¹ã²¥ÏûÏ¢
	 */
	int BroadcastLocalCylinderMessage(const MSG & message,const A3DVECTOR & target, float fRadius);

	/*
	 *	¹ã²¥×µĞÎÏûÏ¢£¬ÔÚ×µÖĞµÄ¶ÔÏó»áÊÕµ½Õâ¸öÏûÏ¢
	 *	Ô²×¶µÄÔ²ĞÄ¼´ÎªÏûÏ¢µÄ·¢³öµã
	 *	msg.target µÄÀàĞÍ¾ö¶¨ÁËÊÕµ½Õß£¬msg.targetµÄID±ØĞëÎª-1
	 *	maskÓÃÓÚ¹ıÂËÏûÏ¢½ÓÊÕ¶ÔÏó
	 *	ÔÚ×ùÕâ¸ö¹ã²¥Ê±£¬»á×Ô¶¯ÅĞ¶ÏÊÇ·ñÒª×ª·¢µ½ÆäËûµÄ·şÎñÆ÷ÉÏ
	 *	msg.source²»»áÊÕµ½Õâ¸ö¹ã²¥ÏûÏ¢
	 */
	int BroadcastLocalTaperMessage(const MSG & message,const A3DVECTOR & target,float fRadius,float cos_halfangle);

	/*
	 *	·Ö·¢ÏûÏ¢£¬Õâ¸ö²Ù×÷²»Ó¦¸ÃÓÉÓÃ»§µ÷ÓÃ
	 *	·Ö·¢ÏûÏ¢µÄ²Ù×÷ÔÚSendMessageÀï±»×Ô¶¯µ÷ÓÃ
	 */
	int DispatchMessage(const MSG & message);

	/*
	 *	¼ÆËãÄ³¸öÎ»ÖÃÓ¦¸ÃÊôÓÚÄÄ¸ö·şÎñÆ÷ ,·µ»ØÕÒµ½µÄµÚÒ»¸ö·şÎñÆ÷
	 *	·µ»Ø-1±íÊ¾Ã»ÓĞÕÒµ½
	 */
	int GetSvrNear(const A3DVECTOR & pos) const
	{
		return w_world_man->GetServerNear(pos);
	}

	/*
	 *	¼ÆËãÄ³¸öÎ»ÖÃÓ¦¸ÃÊôÓÚ´óµØÍ¼ÖĞµÄÄÇ¸ö·şÎñÆ÷£¬¸±±¾·şÎñÆ÷²»ÔÚ´ËÁĞ
	 *	·µ»Ø-1±íÊ¾Ã»ÓĞÕÒµ½
	 */
	int GetGlobalServer(const A3DVECTOR & pos) const
	{
		return w_world_man->GetServerGlobal(pos);
	}
	
	/*
	 *	¼ì²éÒ»¸ö×ø±ê¼ÓÖµºóÊÇ·ñÈÔÈ»ÔÚÕıÈ··¶Î§ÄÚ,ÄÚ²¿Ê¹ÓÃµÄº¯Êı.
	 */
	inline static bool check_index(const grid * g,int x,int z, const world::off_node_t &node)
	{
		int nx = x + node.x_off;
		if(nx < 0 || nx >= g->reg_column) return false;
		int nz = z + node.z_off;
		if(nz < 0 || nz >= g->reg_row) return false;
		return true;
	}

	void BattleFactionSay(int faction, const void * msg, size_t size, char emote_id=0, const void * aux_data=NULL, size_t dsize=0, int self_id=0, int self_level=0);
	void BattleSay(const void * msg, size_t size);
	void InstanceSay(const void * msg, size_t size, bool middle, const void* data=NULL, size_t dsize = 0);

public:
//Ä£°åº¯Êı½Ó¿Ú
	/*
	 *	µ±Ò»¸ö¶ÔÏóÔÚÁ½¸ö¸ñ×Ó¼äÒÆ¶¯,ÅĞ¶Ï¸Ã¶ÔÏóÀë¿ªÁËÄÄĞ©¸ñ×ÓµÄÊÓÒ°,»áµ÷ÓÃÏàÓ¦µÄenterºÍleaveº¯Êı¶ÔÏó
	 */
	template <typename ENTER,typename LEAVE>
	inline void MoveBetweenSlice(slice * pPiece, slice * pNewPiece,ENTER enter,LEAVE leave)
	{
		int i;
		grid * pGrid = &GetGrid();
		int ox,oy,nx,ny;
		pGrid->GetSlicePos(pPiece,ox,oy);
		pGrid->GetSlicePos(pNewPiece,nx,ny);
		float vision = w_vision + pGrid->slice_step - 1e-3;
		float dis = pNewPiece->Distance(pPiece);
		if(dis > vision)
		{
			//±¾¸ñµÄÎŞ·¨¿´¼û ËùÒÔÒª½øĞĞÀë¿ª±¾¸ñµÄ²Ù×÷,ºóÃæµÄÑ­»·²¢Ã»ÓĞÅĞ¶Ï±¾¸ñ
			leave(pPiece);
			enter(pNewPiece);
			if(dis > vision*2)
			{
				for(i = 0; i < w_far_vision; i ++)
				{
					const world::off_node_t &node = w_off_list[i];
					slice * pTmpPiece = pPiece + node.idx_off;
					leave(pTmpPiece);
				}

				for(i = 0; i < w_far_vision; i ++)
				{
					const world::off_node_t &node = w_off_list[i];
					slice * pTmpPiece = pNewPiece + node.idx_off;
					enter(pTmpPiece);
				}
				return ;
			}
		}

		for(i = 0; i < w_far_vision; i ++)
		{
			const world::off_node_t &node = w_off_list[i];
			if(check_index(pGrid,ox,oy,node)) 
			{
				slice * pTmpPiece = pPiece + node.idx_off;
				if(pTmpPiece->Distance(pNewPiece) > vision && pTmpPiece->IsInWorld())
				{
					leave(pTmpPiece);
					//Àë¿ªÁËÕâ¸öslice
				}
			}

			if(check_index(pGrid,nx,ny,node))
			{
				slice * pTmpPiece = pNewPiece + node.idx_off;
				if(pTmpPiece->Distance(pPiece) > vision && pTmpPiece->IsInWorld())
				{
					enter(pTmpPiece);
				}
			}
		}
	}

	/*
	 *	É¨Ãè¸½½üËùÓĞµÄĞ¡¸ñ×Ó,°´ÕÕÔ¤¶¨µÄ·¶Î§À´É¨Ãè,ÕâÀï²»½øĞĞ¸ñ×ÓÊÇ·ñÔÚ±¾·şÎñÆ÷µÄÅĞ¶Ï 
	 */
	template <typename FUNC >
	inline void ForEachSlice(slice * pStart, FUNC func,int vlevel = 0)
	{
		int total = vlevel?w_near_vision:w_far_vision;
		int slice_x,slice_z;
		GetGrid().GetSlicePos(pStart,slice_x,slice_z);
		for(int i = 0; i <total; i ++)
		{
			off_node_t &node = w_off_list[i]; 
			int nx = slice_x + node.x_off; 
			int nz = slice_z + node.z_off; 
			if(nx < 0 || nz < 0 || nx >= GetGrid().reg_column || nz >= GetGrid().reg_row) continue;
			slice * pNewPiece = pStart+ node.idx_off;
			func(i,pNewPiece);
		}
	}

	/*
	 * °´ÕÕÎ»ÖÃºÍ·¶Î§É¨Ãè¸½½üËùÓĞµÄĞ¡¸ñ,²¢ÇÒÒÀ´Îµ÷ÓÃÏàÓ¦µÄ´¦Àíº¯Êı¶ÔÏó
	 * ÕâÀï»áÊ×ÏÈÅĞ¶Ï¸ñ×ÓÊÇ·ñÔÚµ±Ç°·şÎñÆ÷ÖĞ,·ñÔò²»»á·¢ËÍµ½funcº¯ÊıÖĞ
	 */
	template <typename FUNC>
	inline void ForEachSlice(const A3DVECTOR &pos, float fRadius, FUNC func)
	{
		grid * pGrid = &GetGrid();
		float fx = pos.x - pGrid->grid_region.left;
		float fz = pos.z - pGrid->grid_region.top;
		float inv_step = pGrid->inv_step;
		int ofx1 = (int)((fx - fRadius) * inv_step);
		int ofx2 = (int)((fx + fRadius) * inv_step);
		int ofz1 = (int)((fz - fRadius) * inv_step);
		int ofz2 = (int)((fz + fRadius) * inv_step);
		if(ofx1 < 0) ofx1 = 0;
		if(ofx2 >= pGrid->reg_column) ofx2 = pGrid->reg_column -1;
		if(ofz1 < 0) ofz1 = 0;
		if(ofz2 >= pGrid->reg_row) ofz2 = pGrid->reg_row - 1;

		slice * pPiece = pGrid->GetSlice(ofx1,ofz1);
		for(int i = ofz1;i <= ofz2; i ++,pPiece += pGrid->reg_column)
		{
			slice * pStart = pPiece;
			for(int j = ofx1; j <= ofx2; j ++, pStart++)
			{
				if(pStart->IsInWorld()) func(pStart,pos);
			}
		}
	}

	template <typename FUNC>
	inline void ForEachSlice(const A3DVECTOR &pos, const rect & rt, FUNC func)
	{
		grid * pGrid = &GetGrid();
		
		float inv_step = pGrid->inv_step;
		int ofx1 = (int)((rt.left   - pGrid->grid_region.left) * inv_step);
		int ofx2 = (int)((rt.right  - pGrid->grid_region.left) * inv_step);
		int ofz1 = (int)((rt.top    - pGrid->grid_region.top ) * inv_step);
		int ofz2 = (int)((rt.bottom - pGrid->grid_region.top ) * inv_step);
		if(ofx1 < 0) ofx1 = 0;
		if(ofx2 >= pGrid->reg_column) ofx2 = pGrid->reg_column -1;
		if(ofz1 < 0) ofz1 = 0;
		if(ofz2 >= pGrid->reg_row) ofz2 = pGrid->reg_row - 1;

		slice * pPiece = pGrid->GetSlice(ofx1,ofz1);
		for(int i = ofz1;i <= ofz2; i ++,pPiece += pGrid->reg_column)
		{
			slice * pStart = pPiece;
			for(int j = ofx1; j <= ofx2; j ++, pStart++)
			{
				if(pStart->IsInWorld()) func(pStart,pos);
			}
		}
	}

	template <typename FUNC>
	inline void ForEachSlice(const A3DVECTOR &pos1, const A3DVECTOR & pos2, FUNC func)
	{
		rect rt(pos1,pos2);
		ForEachSlice(pos1,rt,func);
	}

	template <int foo>
	inline static void InspirePieceNPC(slice * pPiece,int tick)
	{
		int timestamp = pPiece->idle_timestamp;
		if(tick - timestamp < 40)
		{
			return;
		}
		pPiece->Lock();
		if(tick - pPiece->idle_timestamp < 40)	//ÓÉpiece¾ö¶¨Ã¿Á½ÃëÉèÖÃÒ»´Î
		{
			pPiece->Unlock();
			return;
		}
		pPiece->idle_timestamp = tick;
		gnpc * pNPC = (gnpc*)(pPiece->npc_list);
		while(pNPC)
		{
			pNPC->idle_timer = NPC_IDLE_TIMER;
			pNPC = (gnpc*)(pNPC->pNext);
		}
		pPiece->Unlock();
	}
	/*
	 *	É¨Ãè¸½½üËùÓĞµÄĞ¡¸ñ×Ó,°´ÕÕÔ¤¶¨µÄ·¶Î§À´É¨Ãè,ÕâÀï²»½øĞĞ¸ñ×ÓÊÇ·ñÔÚ±¾·şÎñÆ÷µÄÅĞ¶Ï 
	 */
	template <int foo>
	inline void InspireNPC(slice * pStart, int vlevel = 0)
	{
		int total = vlevel?w_near_vision:w_far_vision;
		int slice_x,slice_z;
		GetGrid().GetSlicePos(pStart,slice_x,slice_z);
		int tick = g_timer.get_tick();
		InspirePieceNPC<0>(pStart,tick);

		for(int i = 0; i <total; i ++)
		{
			off_node_t &node = w_off_list[i]; 
			int nx = slice_x + node.x_off; 
			int nz = slice_z + node.z_off; 
			if(nx < 0 || nz < 0 || nx >= GetGrid().reg_column || nz >= GetGrid().reg_row) continue;
			slice * pNewPiece = pStart+ node.idx_off;
			InspirePieceNPC<0>(pNewPiece,tick);
		}
	}

private:
	void CheckGSvrPoolUpdate();						//¼ì²éµ±Ç°ÔÚÏßµÄÓÎÏ··şÎñÆ÷ÁĞ±í
	void ConnectGSvr(int index, const char * ipaddr, const char * unixaddr);	//Á¬½ÓÁíÍâÒ»Ì¨·şÎñÆ÷ 

	/*
	 * Õâ¸öÀàÊÇ¶¨ÆÚË¢ĞÂÓÎÏ··şÎñÆ÷µÄÀà,Ëü½«×÷ÎªÏß³Ì³ØµÄÒ»¸öÈÎÎñÀ´½øĞĞ
	 * Éú³ÉËüµÄµØ·½ÊÇÏà¹ØµÄ¶¨Ê±Æ÷º¯Êı
	 */

	int _message_handle_count;				//¼ÇÂ¼µ±Ç°message´¦ÀíµÄÇ¶Ì× 

private:
	//ÏûÏ¢·¢ËÍÆ÷µÄº¯Êı£¬ÕâÁ½¸öº¯ÊıÖ»ÓĞÏûÏ¢·¢ËÍÆ÷Á¬½ÓÉÏºÍ¶Ï¿ªÊ±µ÷ÓÃ
};

template <typename WRAPPER>
inline int WrapObject(WRAPPER & wrapper,controller * ctrl, gobject_imp * imp, dispatcher * runner)
{
	ctrl->SaveInstance(wrapper);
	imp->SaveInstance(wrapper);
	runner->SaveInstance(wrapper);
	return 0;
}

template <typename WRAPPER,typename OBJECT>
inline int RestoreObject(WRAPPER & wrapper,OBJECT *obj,world * pPlane)
{
	controller * ctrl =NULL;
	gobject_imp * imp = NULL;
	dispatcher * runner = NULL;

	ctrl = substance::DynamicCast<controller>(substance::LoadInstance(wrapper));
	if(ctrl) imp = substance::DynamicCast<gobject_imp>(substance::LoadInstance(wrapper));
	if(imp) runner = substance::DynamicCast<dispatcher>(substance::LoadInstance(wrapper));
	if(!ctrl || !runner ||!imp) 
	{
		delete imp;
		delete ctrl;
		return -1;
	}
	obj->imp = imp;
	imp->_runner = runner;
	imp->_commander = ctrl;
	imp->Init(pPlane,obj);
	ctrl->Init(imp);
	runner->init(imp);
	//ÒÔºó»¹ĞèÒªµ÷ÓÃReInit
	return 0;
}

extern abase::timer	g_timer;

struct NPCService
{
	enum
	{
		vendor = 1,
		purchase = 2,
		repair = 3,
		heal = 4,
		transmit = 5,
		task_in = 6,
		task_out = 7,
		task_matter = 8,
		skill = 9,
		install = 10,
		uninstall = 11,
		produce = 12,
		decompose = 13,
		trashbox_passwd = 14,
		trashbox_open = 15,
		plane_switch = 16,
		identify = 17,
		faction_service = 18,
		player_market = 19,
		vehicle_service = 20,
		player_market2 = 21,
		waypoint_service = 22,
		unlearn_skill = 23,
		cosmetic = 24,
		mail_service = 25,
		auction_service = 26,
		double_exp = 27,
		hatch_pet_service = 28,
		restore_pet_service = 29,
		battle_service = 30,
		towerbuild = 31,
		battle_leave = 32,
		resetprop = 33,
		spec_trade = 34,
		refine_service = 35,
		change_pet_name = 36,
		forget_pet_skill = 37,
		pet_skill = 38,
		bind_item = 39,
		destory_bind_item = 40,
		destory_item_restore = 41,
		stock_service1 = 42,
		stock_service2 = 43,
		dye_service = 44,
		refine_transmit_service = 45,
		produce2 = 46,
		feedback = 47,
		elf_dec_attribute = 48,//lgc
		elf_flush_genius = 49,
		elf_learn_skill = 50,
		elf_forget_skill = 51,
		elf_refine = 52,
		elf_refine_transmit = 53,
		elf_decompose = 54,
		elf_destroy_item = 55,
		dye_suit_service = 56,
		repair_damaged_item = 57,
		produce3 = 58,
		user_trashbox_open = 59,
		webtrade_service = 60,
		god_evil_convert_service = 61,
		wedding_book = 62,
		wedding_invite = 63,
		factionfortress_service = 64,
		factionfortress_service2 = 65,
		factionfortress_material_exchange = 66,
		dye_pet_service = 67,
		trashbox_open_view_only = 68,
		engrave = 69,
		dpsrank_service = 70,
		addonregen = 71,
		player_force_service = 72,
		unlimited_transmit = 73,
		produce4 = 74,
		country_service = 75,
		countrybattle_leave = 76,
		equip_signature = 77,
		change_ds_forward = 78,
		change_ds_backward = 79,
		player_rename = 80,
		addon_change_service = 81,
		addon_replace_service = 82,
		kingelection_service = 83,
		decompose_fashion_item = 84,
		player_shop_service = 85,
		reincarnation = 86,
		giftcardredeem = 87,
		trickbattle_apply_service = 88,
		generalcard_rebirth_service = 89,
		improve_flysword_service = 90,
		mafia_pvp_signup = 91,
		produce5 = 92,
		npc_goldshop = 93,
		npc_dividendshop = 94,
		player_change_gender = 95,
		pepegapepegapepega = 999, // fake
	};
	
	static bool IsRemoteService(int type)
	{
		int services[100] =
		{
			NPCService::repair,							/* ğåìîíò èçíîøåííîãî ñíàğÿæåíèÿ */
			NPCService::install,						/* âïëàâêà êàìíåé */
			NPCService::uninstall,						/* î÷èñòêà êàìíåé */
			//NPCService::cosmetic,						/* âíåøíîñòü ïåğñîíàæà */
			NPCService::mail_service,					/* ïî÷òîâûé ÿùèê */
			//NPCService::resetprop,					/* ñáğîñ õàğàêòåğèñòèê ïåğñîíàæà */
			NPCService::refine_service,					/* çàòî÷êà ñíàğÿæåíèÿ */
			NPCService::elf_flush_genius,				/* cáğîñ õàğàêòåğèñòèê äæèííà */
			//NPCService::elf_learn_skill,				/* èçó÷åíèå ñêèëëà äæèííà */
			NPCService::elf_forget_skill,				/* óäàëåíèå ñêèëëà äæèííà */
			NPCService::repair_damaged_item,			/* ğåìîíò ñíàğÿæåíèÿ (êàìåíü áåññìåğòíûõ) */
			//NPCService::god_evil_convert_service,		/* ñìåíà êóëüòèâàöèè */
			NPCService::equip_signature,				/* ğîñïèñü ñíàğÿæåíèÿ */
			//NPCService::player_rename,				/* ñìåíà íèêà ïåğñîíàæà */
			//NPCService::player_change_gender,			/* ñìåíà ïîëà ïåğñîíàæà */
			NPCService::dye_service,					/* ïîêğàñêà ñòèëÿ */
			NPCService::hatch_pet_service,				/* èíêóáàöèÿ ïåòà */
			NPCService::pepegapepegapepega,				/* */
			NPCService::pepegapepegapepega,				/* */
			NPCService::pepegapepegapepega,				/* */
			NPCService::pepegapepegapepega,				/* */
			NPCService::pepegapepegapepega,				/* */
			NPCService::pepegapepegapepega,				/* */
			NPCService::pepegapepegapepega,				/* */
			NPCService::pepegapepegapepega,				/* */
		};
		
		for (int i = 0; i < 50; i++)
		{
			if (services[i] == type)
			{
				return true;
			}
		}
		
		return false;
	}
};

#endif

