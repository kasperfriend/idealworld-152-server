#ifndef __ONLINEGAME_COMMON_MESSAGE_H__
#define __ONLINEGAME_COMMON_MESSAGE_H__

#include <stdlib.h>
#include "ASSERT.h"
#include <amemory.h>
struct MSG 
{
	int 	message;	//ÏûÏ¢µÄÀàĞÍ
	struct XID target;	//ÊÕÏûÏ¢µÄÄ¿±ê£¬¿ÉÄÜÊÇ·şÎñÆ÷£¬Íæ¼Ò£¬ÎïÆ·£¬NPCµÈ
	struct XID source;	//´ÓÄÄÀï·¢¹ıÀ´µÄ£¬¿ÉÄÜµÄidºÍÉÏÃæÒ»Ñù
	A3DVECTOR pos;		//ÏûÏ¢·¢³öÊ±µÄÎ»ÖÃ£¬ÓĞµÄÏûÏ¢¿ÉÄÜÎ»ÖÃ²¢ÎŞ×÷ÓÃ
	int	ttl;		//time to live,Õâ¸öÖµÈç¹ûĞ¡ÓÚ0£¬ÄÇÃ´¾Í²»»á½øĞĞÔÙ´Î×ª·¢ÁË
	int	param;		//Ò»¸ö²ÎÊı£¬Èç¹ûÕâ¸ö²ÎÊı¹»ÓÃ£¬ÄÇÃ´¾ÍÊ¹ÓÃÕâ¸ö²ÎÊı
	size_t 	content_length;	//ÏûÏ¢µÄ¾ßÌåÊı¾İ³¤¶È
	const void * content;	//ÏûÏ¢µÄ¾ßÌåÊı¾İ ÍøÂçÉÏ´«²¥Ê±Õâ¸ö×Ö¶ÎÎŞĞ§
private:
	enum {FAST_ALLOC_LEN = 128};
	friend void * SerializeMessage(const MSG &);
	friend void FreeMessage(MSG *);
};

inline void * SerializeMessage(const MSG & msg)
{
	void * buf;
	size_t length = msg.content_length;
	if(length <= MSG::FAST_ALLOC_LEN)
	{
	//	printf("%d %dalloced\n",sizeof(MSG) + length,msg.message);
		buf = abase::fast_allocator::align_alloc(sizeof(MSG) + length);		//±ØĞë¶ÔÆë£¬¿¼ÂÇ¶à¸ömsg
		memcpy(buf,&msg,sizeof(MSG));
		if(length)
		{
			memcpy((char*)buf + sizeof(MSG),msg.content,length);
		}
	}
	else
	{
		buf = abase::fast_allocator::raw_alloc(sizeof(MSG) + length);
		memcpy(buf,&msg,sizeof(MSG));
		memcpy((char*)buf + sizeof(MSG),msg.content,msg.content_length);
	}
	return buf;
}

inline MSG * DupeMessage(const MSG & msg)
{
	MSG * pMsg = (MSG*)SerializeMessage(msg);
	pMsg->content = ((char*)pMsg) + sizeof(MSG);
	return pMsg;
}

inline void FreeMessage(MSG * pMsg)
{
	ASSERT(pMsg->content == ((char*)pMsg) + sizeof(MSG));
	size_t length = pMsg->content_length;
	if(length <= MSG::FAST_ALLOC_LEN)
	{
		abase::fast_allocator::align_free(pMsg, sizeof(MSG) + length);
	}
	else
	{
		abase::fast_allocator::raw_free(pMsg);
	}
}
inline void BuildMessage(MSG & msg, int message, const XID &target, const XID & source,
			const A3DVECTOR & pos,int param = 0,
			const void * content = NULL,size_t content_length = 0)
{
	msg.message = message;
	msg.target = target;
	msg.source = source;
	msg.pos = pos;
	msg.ttl = 2;
	msg.param = param;
	msg.content_length = content_length;
	msg.content = content;
}

enum
{
//	normal message
	GM_MSG_NULL,				//¿ÕÏûÏ¢
	GM_MSG_FORWARD_USERBC,			//×ª·¢µÄÓÃ»§¹ã²¥
	GM_MSG_FORWARD,				//×ª·¢µÄÏûÏ¢£¬ÄÚÈİÓ¦¸Ã×÷ÎªÒ»ÌõĞÂµÄÏûÏ¢ÄÚÈİÀ´½âÊÍ
	GM_MSG_FORWARD_BROADCAST,		//×ª·¢µÄÏûÏ¢¹ã²¥ÏûÏ¢,contentÊÇÁíÍâÒ»ÌõÍêÕûµÄÏûÏ¢
	GM_MSG_USER_GET_INFO,			//ÓÃ»§È¡µÃ±ØÒªµÄÊı¾İ

//5
	GM_MSG_IDENTIFICATION,			//·şÎñÆ÷¸æÖª×Ô¼ºµÄÉí·İ,Ô­µÄÀàĞÍ±ØĞëÊÇserver²¢ÇÒidÊÇËûµÄ·ûºÅ
	GM_MSG_SWITCH_GET,			//È¡µÃÓÃ»§Êı¾İ,·şÎñÆ÷ÇĞ»»£¬È¡µÃÓÃ»§Êı¾İ paramÊÇ tag,contentÊÇkey
	GM_MSG_SWITCH_USER_DATA,		//ÓÃ»§Êı¾İ,SWITCH_GETµÄ»ØÓ¦
	GM_MSG_SWITCH_NPC,			//NPCÇĞ»»·şÎñÆ÷
	GM_MSG_USER_MOVE_OUTSIDE,		//ÓÃ»§ÔÚ±ß½çÒÆ¶¯

//10	
	GM_MSG_USER_NPC_OUTSIDE,		//NPCÔÚ±ß½ç´¦ÒÆ¶¯£¬²»Í¬Ö®´¦ÔÚÓÚNPC²»ĞèÒªÈ¡µÃĞÂ¿´µ½ÇøÓòµÄ¶ÔÏó
	GM_MSG_ENTER_WORLD,			//¸øcontrollerµÄ£¬±íÊ¾ÓÃ»§ÒÑ¾­½øÈëÁËÊÀ½ç
	GM_MSG_ATTACK,				//Ä¿±êºÍÔ´¶¼±ØĞëÊÇ¸öÌå
	GM_MSG_SKILL,				//Ä¿±êºÍÔ´¶¼±ØĞëÊÇ¸öÌå
	GM_MSG_PICKUP,				//¼ğÆğÎïÆ·,Ä¿±êÒ»°ãÊÇÎïÆ·

//15
	GM_MSG_FORCE_PICKUP,			//Ç¿ÖÆ¼ñÆğÎïÆ·£¬²»Ğ£ÑéËùÊéÕßIDºÍ×é¶ÓID
	GM_MSG_PICKUP_MONEY,			//ÎïÆ·Í¨ÖªÓÃ»§¼ğµ½Ç® paramÊÇÇ®Êı  contentÊÇË­¶ªÆúµÄ
	GM_MSG_PICKUP_TEAM_MONEY,		//ÎïÆ·Í¨Öª¶Ó³¤¼ğµ½Ç® paramÊÇÇ®Êı  contentÊÇË­¶ªÆúµÄ
	GM_MSG_RECEIVE_MONEY,			//Í¨ÖªÍæ¼ÒµÃµ½Ç®£¨¿ÉÄÜÊÇ×é¶Ó)
	GM_MSG_PICKUP_ITEM,			//ÎïÆ·Í¨ÖªÓÃ»§¼ğµ½ÎïÆ· paramÊÇ palyer_id | 0x80000000(Èç¹û×é¶Ó£©

//20
	GM_MSG_ERROR_MESSAGE,			//ÈÃplayer·¢ËÍÒ»¸öerror message
	GM_MSG_NPC_SVR_UPDATE,			//NPC·¢ÉúÁË·şÎñÆ÷ÇĞ»»£¬Õâ¸öÏûÏ¢Ö»·¢¸ø´¦ÓÚÒÆ×ß×´Ì¬µÄÔ­ÉúNPC
	GM_MSG_EXT_NPC_DEAD,			//Íâ²¿µÄNPCµÄËÀÍöÏûÏ¢(ÕæÕıÉ¾³ı)£¬Õâ¸öÏûÏ¢Ö»·¢¸ø´¦ÓÚÒÆ×ß×´Ì¬µÄÔ­ÉúNPC
	GM_MSG_EXT_NPC_HEARTBEAT,		//Íâ²¿NPCµÄĞÄÌø£¬ÓÃÓÚÅĞ¶ÏÊÇ·ñ³¬Ê± 
	GM_MSG_WATCHING_YOU,			//Ö÷¶¯¹ÖÎï¼¤»îµÄÏûÏ¢,ÓÉÍæ¼Ò»ònpc·¢³ö£¬ºóÃæÊÇÒ»¸öwatching_tµÄ½á¹¹

//25
//	AGGRO  message 
	GM_MSG_GEN_AGGRO,			//Éú³Éaggro£¬ºóÃæ¸½¼ÓÁËÒ»¸öaggro_info_tµÄ½á¹¹
	GM_MSG_TRANSFER_AGGRO,			//aggroµÄ´«ËÍ Ä¿Ç°Ö»´«ËÍµÚÒ»Î» contentÊÇÒ»¸öXID,Èç¹û¸ÃXIDµÄidÎª-1    ÔòÇå¿Õ³ğºŞÁĞ±í paramÊÇ¸ÃÈË³ğºŞÖµ
	GM_MSG_AGGRO_ALARM,			//aggro¾¯±¨£¬µ±ÊÜµ½¹¥»÷Ê±»á·¢ËÍ£¬ºóÃæ¸½¼ÓÁËÒ»¸öaggro_alarm_tÎ´Ê¹ÓÃ
	GM_MSG_AGGRO_WAKEUP,			//aggro¾¯±¨£¬½«ĞİÃßµÄ¹ÖÎï¾ªĞÑ,ºóÃæ¸½¼ÓÁËÒ»¸öaggro_alarm_tÎ´Ê¹ÓÃ
	GM_MSG_AGGRO_TEST,			//aggro²âÊÔ,Ö»ÓĞµ±·¢ËÍÕßÔÚaggroÁĞ±íÖĞ£¬²Å»áÒı·¢ĞÂµÄaggro£¬ºóÃæ¸½¼ÓÁËÒ»¸öaggro_info_tÎ´Ê¹ÓÃ
	
//30
	GM_MSG_OBJ_SESSION_END,			//¶ÔÏóµÄsessionÍê³É
	GM_MSG_OBJ_SESSION_REPEAT,		//±íÊ¾sessionÒª¼ÌĞøÖ´ĞĞ 
	GM_MSG_OBJ_ZOMBIE_END,			//±íÊ¾Òª½áÊø½©Ê¬×´Ì¬
	GM_MSG_EXPERIENCE,			//µÃµ½¾­ÑéÖµ	content ÊÇÒ»¸ömsg_exp_t
	GM_MSG_GROUP_EXPERIENCE,		//µÃµ½×é¶Ó¾­ÑéÖµ conennt ÊÇ¶à¸ömsg_grp_exp_t , param Ôì³ÉµÄ×ÜÉËº¦
	
//35
	GM_MSG_TEAM_EXPERIENCE,			//µÃµ½×é¶Ó¾­ÑéÖµ conennt ÊÇmsg_exp_t ³¬¹ı¾àÀë¾­ÑéÖµ»á±»ºöÂÔ param ÊÇÉ±ËÀµÄnpcid Èç¹Îª0Ôò²»ÊÇ±¾¶ÓÎéÉ±ËÀµÄ
	GM_MSG_QUERY_OBJ_INFO00,		//È¡µÃ¶ÔÏóµÄinfo00 paramÊÇ·¢ËÍÕßµÄsid ,contentÊÇÒ»¸öint´ú±ícs_index
	GM_MSG_HEARTBEAT,			//·¢¸ø×Ô¼ºµÄĞÄÌøÏûÏ¢  ²ÎÊıÊÇÕâ´ÎHeartbeatµÄÃëÊı
	GM_MSG_HATE_YOU,
	GM_MSG_TEAM_INVITE,			//ÇëÇóÄ³ÈË¼ÓÈë¶ÓÎéparamÊÇteamseq, contentÊÇÒ»¸öint ±íÊ¾pickup_flag

//40	
	GM_MSG_TEAM_AGREE_INVITE,		//±»ÑûÇëÈËÍ¬Òâ¼ÓÈë¶ÓÎé contentÊÇÒ»¸öint(±íÊ¾Ö°Òµ)+ team_mutable_prop
	GM_MSG_TEAM_REJECT_INVITE,		//¾Ü¾ø¼ÓÈëÑûÇë
	GM_MSG_JOIN_TEAM,			//¶Ó³¤Í¬ÒâÄ³ÈË¼ÓÈë¶ÓÎé param¸ßÎ»ÊÇ¼ñÈ¡·½Ê½ paramµÍÎ»ÊÇ¶ÓÔ±¸öÊı£¬contentÊÇmember_entryµÄ±í 
	GM_MSG_JOIN_TEAM_FAILED,		//¶ÔÏóÎŞ·¨¼ÓÈë¶ÓÎé£¬Ó¦¸Ã´Ó¶ÓÎéÖĞÈ¥³ı
	GM_MSG_MEMBER_NOTIFY_DATA,		//×é¶Ó³ÉÔ±Í¨ÖªÆäËûÈË×Ô¼ºµÄ»ù´¡ĞÅÏ¢ content ÊÇÒ»¸öteam_mutable_prop

//45	
	GM_MSG_NEW_MEMBER,			//leaderÍ¨ÖªĞÂ³ÉÔ±¼ÓÈë£¬contentÊÇÒ»¸ömember_entry list paramÊÇÊıÁ¿
	GM_MSG_LEAVE_PARTY_REQUEST,
	GM_MSG_LEADER_CANCEL_PARTY,
	GM_MSG_MEMBER_NOT_IN_TEAM,
	GM_MSG_LEADER_KICK_MEMBER,

//50	
	GM_MSG_MEMBER_LEAVE,
	GM_MSG_LEADER_UPDATE_MEMBER,
	GM_MSG_GET_MEMBER_POS,			//ÒªÇó¶ÓÓÑ·¢ËÍÎ»ÖÃ paramÊÇ·¢ËÍÕßµÄsid ,contentÊÇÒ»¸öint´ú±ícs_index
	GM_MSG_QUERY_PLAYER_EQUIPMENT,		//È¡µÃÌØ¶¨Íæ¼ÒµÄÊı¾İ£¬ÒªÇóÆ½Ãæ¾àÀëÔÚÒ»¶¨·¶Î§Ö®ÄÚparamÊÇ·¢ËÍÕßµÄsid ,contentÊÇÒ»¸öint´ú±ícs_index
	GM_MSG_TEAM_PICKUP,			//¶ÓÓÑ·ÖÅäµ½ÎïÆ·£¬ param ÊÇ type, content ÊÇcount

//55	
	GM_MSG_TEAM_CHAT,			//×é¶ÓÁÄÌì param ÊÇchannel, content ÊÇÄÚÈİ
	GM_MSG_SERVICE_REQUEST,			//playerÒªÇó·şÎñµÄÏûÏ¢ param ÊÇ·şÎñÀàĞÍ content ÊÇ¾ßÌåÊı¾İ ¾
	GM_MSG_SERVICE_DATA,			//·şÎñµÄÊı¾İµ½´ï param ÊÇ·şÎñÀàĞÍ  content ÊÇ ¾ßÌåÊı¾İ
	GM_MSG_SERVICE_HELLO,			//player Ïò·şÎñÉÌÎÊºÃ  param ÊÇ player×Ô¼ºµÄfaction
	GM_MSG_SERVICE_GREETING,		//·şÎñÉÌ½øĞĞ»Ø»° ¿ÉÄÜĞèÒªÔÚÀïÃæ·µ»Ø·şÎñÁĞ±í$$$$(ÏÖÔÚÎ´×ö)

//60	
	GM_MSG_SERVICE_QUIERY_CONTENT,		//È¡µÃ·şÎñÄÚÈİ 	 param ÊÇ·şÎñÀàĞÍ, content¿É¿´×÷pair<cs_index,sid>
	GM_MSG_EXTERN_OBJECT_APPEAR,		//content ÊÇextern_object_manager::object_appear
	GM_MSG_EXTERN_OBJECT_DISAPPEAR,		//ÏûÊ§»òÕß
	GM_MSG_EXTERN_OBJECT_REFRESH,		//¸üĞÂÎ»ÖÃºÍÑªÖµ£¬paramÖĞ±£´æµÄÊÇÑªÖµ 
	GM_MSG_USER_APPEAR_OUTSIDE,		//ÓÃ»§ÔÚÍâÃæ³öÏÖ£¬Òª·¢ËÍ±ØÒªµÄÊı¾İ¸ø¸ÃÍæ¼Ò£¬content ÀïÊÇsid,paramÊÇlinkd id

//65
	GM_MSG_FORWARD_BROADCAST_SPHERE,	//×ª·¢µÄÏûÏ¢¹ã²¥ÏûÏ¢,contentÊÇÁíÍâÒ»ÌõÍêÕûµÄÏûÏ¢
	GM_MSG_FORWARD_BROADCAST_CYLINDER,	//×ª·¢µÄÏûÏ¢¹ã²¥ÏûÏ¢,contentÊÇÁíÍâÒ»ÌõÍêÕûµÄÏûÏ¢
	GM_MSG_FORWARD_BROADCAST_TAPER,		//×ª·¢µÄÏûÏ¢¹ã²¥ÏûÏ¢,contentÊÇÁíÍâÒ»ÌõÍêÕûµÄÏûÏ¢
	GM_MSG_ENCHANT,				//Ê¹ÓÃ¸¨ÖúÄ§·¨
	GM_MSG_ENCHANT_ZOMBIE,			//Ê¹ÓÃ¸¨ÖúÄ§·¨,×¨ÃÅ¸øËÀÈËÓÃµÄ

//70
	GM_MSG_OBJ_SESSION_REPEAT_FORCE,	//±íÊ¾sessionÒªrepeat £¬ºóÃæ¼´Ê¹ÓĞÈÎÎñÒ²Òª¼ÌĞøÖ´ĞĞ
	GM_MSG_NPC_BE_KILLED,			//ÏûÏ¢·¢¸øÉ±ËÀnpcµÄÍæ¼Ò£¬param ±íÊ¾±»É±ËÀnpcµÄÀàĞÍ contentÊÇNPCµÄ¼¶±ğ
	GM_MSG_NPC_CRY_FOR_HELP,		//npc ½øĞĞÇó¾È²Ù×÷
	GM_MSG_PLAYER_TASK_TRANSFER,		//ÈÎÎñÔÚplayerÖ®¼ä½øĞĞ´«ËÍºÍÍ¨Ñ¶µÄº¯Êı
	GM_MSG_PLAYER_BECOME_INVADER,		//³ÉÎª·ÛÃû msg.param ÊÇÔö¼ÓµÄÊ±¼ä

//75
	GM_MSG_PLAYER_BECOME_PARIAH,		//³ÉÎªºìÃû 
	GM_MSG_FORWARD_CHAT_MSG,		//×ª·¢µÄÓÃ»§ÁÄÌìĞÅÏ¢,paramÊÇrlevel,sourceÊÇXID(-channel,self_id)
	GM_MSG_QUERY_SELECT_TARGET,		//È¡µÃ¶ÓÓÑÑ¡ÔñµÄ¶ÔÏó
	GM_MSG_NOTIFY_SELECT_TARGET,		//È¡µÃ¶ÓÓÑÑ¡ÔñµÄ¶ÔÏó
	GM_MSG_SUBSCIBE_TARGET,			//ÒªÇó¶©ÔÄÒ»¸ö¶ÔÏó

//80
	GM_MSG_UNSUBSCIBE_TARGET,		//ÒªÇó¶©ÔÄÒ»¸ö¶ÔÏó
	GM_MSG_SUBSCIBE_CONFIRM,		//È·ÈÏ¶©ÔÄÊÇ·ñ´æÔÚ
	GM_MSG_PRODUCE_MONEY,			//Í¨ÖªÏµÍ³²úÉú½ğÇ® ·¢ËÍÔ´ÊÇËùÊôÕß£¬paramÊÇ×éid£¬ contentÊÇÇ®Êı
	GM_MSG_PRODUCE_MONSTER_DROP,		//Í¨ÖªÏµÍ³²úÉú¹ÖÎïµôÂäÎïÆ·ºÍ½ğÇ®£¬ ·¢ËÍÔ´ÊÇËùÊôÕß£¬paramÊÇmoney£¬ content ÊÇ struct { int team_id; int team_seq;int npc_id;int item_count; int item[];}
	GM_MSG_GATHER_REQUEST,			//ÇëÇóÊÕ¼¯Ô­ÁÏ£¬  param ÊÇÍæ¼ÒµÄfaction, content ·Ö±ğÊÇÍæ¼Ò¼¶±ğ¡¢²É¼¯¹¤¾ßºÍÈÎÎñID

//85
	GM_MSG_GATHER_REPLY,			//Í¨Öª¿ÉÒÔ½øĞĞ²É¼¯  param ÊÇ²É¼¯ĞèÒªµÄÊ±¼ä
	GM_MSG_GATHER_CANCEL,			//È¡Ïû²É¼¯
	GM_MSG_GATHER,				//½øĞĞ²É¼¯£¬ÒªÇóÈ¡µÃÎïÆ·
	GM_MSG_GATHER_RESULT,			//²É¼¯Íê³É£¬param ÄÚÊÇ²É¼¯µ½µÄÎïÆ·id, contentÊÇÊıÁ¿ ºÍ¿ÉÄÜ¸½¼ÓµÄÈÎÎñID
	GM_MSG_HP_STEAL,				//ÊÕµ½ÎüÑªµÄ½á¹û

//90
	GM_MSG_INSTANCE_SWITCH_GET,		//È¡µÃÓÃ»§Êı¾İ,·şÎñÆ÷ÇĞ»»£¬È¡µÃÓÃ»§Êı¾İ ÓÃÓÚ¸±±¾¼äµÄÇĞ»» paramÊÇkey
	GM_MSG_INSTANCE_SWITCH_USER_DATA,	//ÓÃ»§Êı¾İ,SWITCH_SWITCH_GETµÄ»ØÓ¦
	GM_MSG_EXT_AGGRO_FORWARD,		//Í¨ÖªÔ­Éúnpc½øĞĞ³ğºŞ×ª·¢ param ÊÇrage´óĞ¡£¬ contentÊÇ²úÉú³ğºŞµÄid
	GM_MSG_TEAM_APPLY_PARTY,		//ÉêÇë½øÈë¶ÓÎéÑ¡Ïî
	GM_MSG_TEAM_APPLY_REPLY,		//ÉêÇë³É¹¦»Ø¸´ ÆäÖĞµÄparamÊÇseq	

//95
	GM_MSG_QUERY_INFO_1,			//²éÑ¯INFO1£¬¿ÉÒÔ·¢¸øÍæ¼Ò»òÕßNPC,paramµÄÄÚÈİÊÇcs_index,contentÊÇsid
	GM_MSG_CON_EMOTE_REQUEST,		//½øĞĞĞ­Í¬¶¯×÷µÄÇëÇó param ÊÇ action
	GM_MSG_CON_EMOTE_REPLY,			//½øĞĞĞ­Í¬¶¯×÷µÄ»ØÓ¦ param ÊÇactionºÍÍ¬ÒâÓë·ñµÄÁ½¸ö×Ö½ÚµÄ×éºÏ
	GM_MSG_TEAM_CHANGE_TO_LEADER,		//Í¨Öª±ğÈËÒª³ÉÎªleader
	GM_MSG_TEAM_LEADER_CHANGED,		//Í¨Öª¶ÓÓÑ¶Ó³¤µÄ¸Ä±ä

//100
	GM_MSG_OBJ_ZOMBIE_SESSION_END,		//ËÀÍöºó½øĞĞsessionµÄ²Ù×÷£¬ÆäËû¶¨ÒåºÍÕı³£µÄsession²Ù×÷Ò»Ñù
	GM_MSG_QUERY_PERSONAL_MARKET_NAME,	//È¡µÃ°ÚÌ¯µÄÃû×Ö£¬paramÊÇ·¢ËÍÕßµÄsid ,contentÊÇÒ»¸öint´ú±ícs_index
	GM_MSG_HURT,				//¶ÔÏó²úÉúÉËº¦ content ÊÇmsg_hurt_extra_info_t
	GM_MSG_DEATH,				//Ç¿ĞĞÈÃ¶ÔÏóËÀÍö,param=0·ÇÈÎÎñ=1ÈÎÎñÓĞËğ=2ÈÎÎñÎŞËğ(´Ëparam½ö¶ÔplayerÓĞĞ§) 
	GM_MSG_PLANE_SWITCH_REQUEST,		//ÇëÇó¿ªÊ¼´«ËÍ£¬contentÊÇkey£¬Èç¹û½øĞĞ´«ËÍ£¬Ôò·µ»Ø SWITCH_REPLAY

//105
	GM_MSG_PLANE_SWITCH_REPLY,		//´«ËÍÇëÇó±»È·ÈÏ£¬contentÊÇkey
	GM_MSG_SCROLL_RESURRECT,		//¾íÖá¸´»î  param±íÊ¾¸´»îÕßÊÇ·ñ¿ªÆôÁËpvpÄ£Ê½1±íÊ¾¿ªÆôÁË
	GM_MSG_LEAVE_COSMETIC_MODE,		//ÍÑÀëÕûÈİ×´Ì¬
	GM_MSG_DBSAVE_ERROR,			//Êı¾İ¿â±£´æ´íÎó
	GM_MSG_SPAWN_DISAPPEAR,			//Í¨ÖªNPCºÍÎïÆ·ÏûÊ§ paramÊÇcondition

//110
	GM_MSG_PET_CTRL_CMD,			//Íæ¼Ò·¢À´µÄ¿ØÖÆÏûÏ¢»áÓÃÕâ¸öÏûÏ¢·¢¸ø³èÎï
	GM_MSG_ENABLE_PVP_DURATION,		//¼¤»îPVP×´Ì¬
	GM_MSG_PLAYER_KILLED_BY_NPC,		//Íæ¼Ò±»NPCÉ±ËÀºóNPCµÄÂß¼­
	GM_MSG_PLAYER_DUEL_REQUEST,             //Íæ¼Ò·¢³öÒªÇóduelµÄÇëÇó
	GM_MSG_PLAYER_DUEL_REPLY,               //Íæ¼Ò»ØÓ¦duelµÄÇëÇó£¬paramÊÇÊÇ·ñ´ğÓ¦duel

//115
	GM_MSG_PLAYER_DUEL_PREPARE,      	//¾ö¶·×¼±¸¿ªÊ¼ 3Ãëµ¹¼ÆÊ±ºó¿ªÊ¼
	GM_MSG_PLAYER_DUEL_START,               //¾ö¶·¿ªÊ¼ 
	GM_MSG_PLAYER_DUEL_CANCEL,		//Í£Ö¹¾ö¶·
	GM_MSG_PLAYER_DUEL_STOP,		//¾ö¶·½áÊø
	GM_MSG_DUEL_HURT,			//PVP¶ÔÏó²úÉúÉËº¦content ±»ºöÂÔ

//120
	GM_MSG_PLAYER_BIND_REQUEST,		//ÇëÇóÆïÔÚ±ğÈËÉíÉÏ
	GM_MSG_PLAYER_BIND_INVITE,		//ÑûÇë±ğÈËÆïÔÚ×Ô¼ºÉíÉÏ
	GM_MSG_PLAYER_BIND_REQUEST_REPLY,	//ÇëÇóÆïµÄ»ØÓ¦
	GM_MSG_PLAYER_BIND_INVITE_REPLY,	//ÑûÇëÆïµÄ»ØÓ¦
	GM_MSG_PLAYER_BIND_PREPARE,		//×¼±¸¿ªÊ¼Á¬½Ó

//125
	GM_MSG_PLAYER_BIND_LINK,		//Á¬½Ó¿ªÊ¼
	GM_MSG_PLAYER_BIND_STOP,		//Í£Ö¹Á¬½Ó
	GM_MSG_PLAYER_BIND_FOLLOW,		//ÒªÇóÍæ¼Ò¸úËæ
	GM_MSG_QUERY_EQUIP_DETAIL,		//param Îªfaction, content Îªcs_index ºÍcs_sid
	GM_MSG_PLAYER_RECALL_PET,		//ÈÃÍæ¼ÒÇ¿ÖÆÏû³ıÕÙ»½×´Ì¬

//130
	GM_MSG_CREATE_BATTLEGROUND,		//ÒªÇóÕ½³¡·şÎñÆ÷´´½¨Ò»¸öÕ½³¡µÄÏûÏ¢£¬Ö÷ÒªÓÃÓÚ²âÊÔ
	GM_MSG_BECOME_TURRET_MASTER,		//³ÉÎª¹¥³Ç³µµÄmaster,paramÊÇtid, content ÊÇfaction
	GM_MSG_REMOVE_ITEM,			//É¾³ıÒ»¸öÎïÆ·µÄÏûÏ¢£¬ÓÃÓÚ¹¥³Ç³µ¿ØÖÆºóµÄÎïÆ·¼õÉÙ paramÊÇtid
	GM_MSG_NPC_TRANSFORM,			//NPC±äĞÎĞ§¹û£¬contentÀï±£´æ ÖĞ¼ä×´Ì¬£¬ÖĞ¼äÊ±¼ä ÖĞ¼ä±êÖ¾ ×îºó×´Ì¬
	GM_MSG_NPC_TRANSFORM2,			//NPC±äĞÎĞ§¹û2£¬param ÊÇÄ¿±êID Èç¹û±¾À´¾ÍºÍÄ¿±êIDÒ»ÖÂÁË£¬ÄÇÃ´¾Í²»±äĞÎÁË

//135
	GM_MSG_TURRET_NOTIFY_LEADER,		//¹¥³Ç³µÍ¨Öªleader×Ô¼º´æÔÚ£¬ÈÃÆäÎŞ·¨ÔÙ´Î½øĞĞÕÙ»½
	GM_MSG_PET_RELOCATE_POS,		//³èÎïÒªÇóÖØĞÂ¶¨Î»×ø±ê
	GM_MSG_PET_CHANGE_POS,			//Ö÷ÈËĞŞ¸ÄÁË³èÎïµÄ×ø±ê
	GM_MSG_PET_DISAPPEAR,			//Êı¾İ²»ÕıÈ·,»òÕßÆäËüÇé¿ö,Ö÷ÈËÒªÇó³èÎïÏûÊ§
	GM_MSG_PET_NOTIFY_HP,			//³èÎïÍ¨ÖªÖ÷ÈË£¬¸æÖª×Ô¼ºµÄÑªÁ¿ param ÊÇ stamp,content ÊÇfloat hp ratio

//140
	GM_MSG_PET_NOTIFY_DEATH,		//³èÎïÍ¨ÖªÖ÷ÈË×Ô¼ºµÄËÀÍö
	GM_MSG_PET_MASTER_INFO,			//Ö÷ÈËÍ¨Öª³èÎï×Ô¼ºµÄÊı¾İ
	GM_MSG_PET_LEVEL_UP,			//Ö÷ÈËÍ¨Öª³èÎïÉı¼¶ÁË ,contentÊÇ level
	GM_MSG_PET_HONOR_MODIFY,		//Ö÷ÈËÍ¨Öª³èÎïµÄÖÒ³Ï¶È·¢Éú±ä»¯
	GM_MSG_MASTER_ASK_HELP,			//Ö÷ÈËÒªÇó³èÎï°ïÖú

//145	
	GM_MSG_PET_SET_COOLDOWN,		//³èÎïÍ¨ÖªÖ÷ÈËÉèÖÃÀäÈ´Ê±¼ä msg.paramÊÇ cooldown id, content ÊÇ msec
	GM_MSG_MOB_BE_TRAINED,			//¹ÖÎï±»Ñ±·ş£¬´«ËÍ³èÎïµ°¸øÊ©·¨Õß
	GM_MSG_PET_AUTO_ATTACK,			//Ö÷ÈËÍ¨Öª³èÎï×Ô¶¯¹¥»÷ msg.param ÊÇforce attack, contentÊÇÄ¿±ê
	GM_MSG_PET_SKILL_LIST,			//Ö÷ÈËÍ¨Öª³èÎïĞÂµÄ¼¼ÄÜÁĞ±í
	GM_MSG_SWITCH_FAILED,			//¸±±¾Í¨ÖªËµ´«ËÍÊ§°Ü

//150	
	GM_MSG_PET_ANTI_CHEAT,
	GM_MSG_QUERY_PROPERTY,			//²éÑ¯ÆäËûÈËµÄÊôĞÔ£¬paramÊÇ²éÑ¯µÀ¾ßÔÚ°ü¹üÀ¸Ë÷Òı
	GM_MSG_QUERY_PROPERTY_REPLY,	//²éÑ¯ÆäËûÈËµÄÊôĞÔ·µ»Ø£¬paramÊÇ²éÑ¯µÀ¾ßÔÚ°ü¹üÀ¸Ë÷Òı£¬contentÊÇÊôĞÔÊı¾İ
	GM_MSG_TRY_CLEAR_AGGRO,			//×ÔÉíÒşÉíµÈ¼¶´óÓÚnpc·´ÒşµÈ¼¶ÔòÇå³ınpc¶Ô×Ô¼ºµÄ³ğºŞ£¬paramÊÇ×Ô¼ºµÄÒşÉíµÈ¼¶
	GM_MSG_NOTIFY_INVISIBLE_DATA,	//½«×ÔÉíµÄÒşÉíÊı¾İÍ¨Öª¸ø³èÎï,ÈÃ³èÎïÒşÉí»òÏÖÉí

//155
	GM_MSG_NOTIFY_CLEAR_INVISIBLE,	//³èÎï½â³ıÒşÉíÍ¨ÖªÖ÷ÈË£¬ÈÃÖ÷ÈËÒ²½â³ıÒşÉí
	GM_MSG_CONTRIBUTION_TO_KILL_NPC,//Íæ¼ÒÉ±ËÀnpcºó£¬npc·¢ËÍ¸øÍæ¼ÒµÄÏûÏ¢£¬paramÊÇnpc world_tag contentÊÇ½á¹¹msg_contribution_t
	GM_MSG_GROUP_CONTRIBUTION_TO_KILL_NPC,//¶ÓÎéÉ±ËÀnpcºó£¬npc·¢ËÍ¸øÍæ¼ÒµÄÏûÏ¢£¬paramÊÇnpc world_tag contentÊÇ½á¹¹msg_group_contribution_t
	GM_MSG_REBUILD_TEAM_INSTANCE_KEY_REQ,	//¶ÓÔ±·¢¸ø¶Ó³¤µÄÖØ½¨×é¶Ó¸±±¾keyµÄÇëÇó,paramÊÇworldtag,contentÊÇÖØ½¨Ç°µÄteam_instance_key
	GM_MSG_REBUILD_TEAM_INSTANCE_KEY,		//¶Ó³¤·¢¸ø¶ÓÔ±µÄÖØ½¨×é¶Ó¸±±¾key,paramÊÇworldtag,contentÊÇ¾ÉµÄºÍĞÂµÄteam_instance_key

//160
	GM_MSG_TRANSFER_FILTER_DATA,		//filter×ªÒÆ£¬paramÊÇfilter¸öÊı£¬contentÊÇfilterÊı¾İ
	GM_MSG_PLANT_PET_NOTIFY_DEATH,		//Ö²Îï³èÍ¨ÖªÖ÷ÈËËÀÍö£¬²ÎÊıÊÇpet_stamp
	GM_MSG_PLANT_PET_NOTIFY_HP,			//Ö²Îï³èÍ¨ÖªÖ÷ÈËĞÅÏ¢£¬²ÎÊıÊÇpet_stamp£¬dataÊÇmsg_plant_pet_hp_notify
	GM_MSG_PLANT_PET_NOTIFY_DISAPPEAR,	//Ö²Îï³èÍ¨ÖªÖ÷ÈËÏûÊ§£¬²ÎÊıÊÇpet_stamp
	GM_MSG_PLANT_PET_SUICIDE,			//Ö÷ÈËÍ¨ÖªÖ²ÎïÊ¹Æä×Ô±¬£¬²ÎÊıÊÇpet_stamp

//165
	GM_MSG_MASTER_NOTIFY_LAYER,		//Ö÷ÈËÍ¨Öª³èÎï×ÔÉílayer,²ÎÊıÊÇpetstamp dataÊÇchar layer
	GM_MSG_INJECT_HP_MP,			//¸øÄ¿±êÔö¼ÓhpºÍmp,dataÊÇmsg_hp_mp_t
	GM_MSG_DRAIN_HP_MP,				//Ê¹Ä¿±êÏûºÄhpºÍmp,dataÊÇmsg_hp_mp_t
	GM_MSG_CONGREGATE_REQUEST,		//¼¯½áÇëÇó, param¼¯½áÀàĞÍ data:msg_congregate_req_t
	GM_MSG_REJECT_CONGREGATE,		//¾Ü¾ø¼¯½áÇëÇó, param¼¯½áÀàĞÍ

//170
	GM_MSG_NPC_BE_KILLED_BY_OWNER,	//NPC±»Íæ¼ÒÉ±ËÀ,paramÊÇnpc tid,contentÊÇmsg_dps_dph_t
	GM_MSG_EXCHANGE_POS,			//Íæ¼ÒÖ®¼ä½»»»Î»ÖÃ£¬»áÍ¬Ê±·¢¸øÁ½¸öÈË
	GM_MSG_EXTERN_HEAL,				//¸øÄ³Ä³¶ÔÏó¼ÓÑªµÄÏûÏ¢
	GM_MSG_QUERY_INVENTORY_DETAIL,	//²éÑ¯Íæ¼Ò°ü¹üÏêÏ¸Êı¾İ
	GM_MSG_TURRET_OUT_OF_CONTROL,	//½â³ı¹¥³Ç³µµÄ¿ØÖÆ

//175
	GM_MSG_TRANSFER_FILTER_GET,		//filter×ªÒÆ, paramÊÇfilter_mask contentÊÇ×ªÒÆµÄÊıÁ¿
	GM_MSG_PET_TEST_SANCTUARY,		//Í¨Öª³èÎï½øÈëÁË°²È«Çø
	GM_MSG_PLAYER_KILLED_BY_PLAYER,	//Íæ¼Ò±»Íæ¼ÒÉ±ËÀ£¬paramÊÇmsg_player_killed_info_t
	GM_MSG_CREATE_COUNTRYBATTLE,	//ÒªÇó¹úÕ½Õ½³¡·şÎñÆ÷´´½¨Ò»¸öÕ½³¡µÄÏûÏ¢£¬Ö÷ÒªÓÃÓÚ²âÊÔ
	GM_MSG_COUNTRYBATTLE_HURT_RESULT,	//¹úÕ½ÖĞÍ¨Öª¹¥»÷ÕßÊµ¼ÊÔì³ÉµÄÉËº¦£¬paramÎªÉËº¦Öµ£¬contentÊÜ¹¥»÷Õß»êÁ¦(player)»ò0(npc)
	
//180
	GM_MSG_LONGJUMP,				//Íæ¼ÒË²ÒÆ£¬paramÊÇworldtag, contentÊÇpos
	GM_MSG_TRICKBATTLE_PLAYER_KILLED,
	GM_MSG_COUNTRYBATTLE_PLAYER_KILLED,	//¹úÕ½ÖĞÍæ¼ÒËÀÍö
	GM_MSG_MAFIA_PVP_AWARD, //°ïÅÉpvpÌØÊâ»î¶¯½±Àø
	GM_MSG_MAFIA_PVP_STATUS, //°ïÅÉpvp ×´Ì¬Í¨Öª
//185	
	GM_MSG_MAFIA_PVP_ELEMENT,// °ïÅÉpvp ¼ÓÔØÇëÇó
	GM_MSG_PUNISH_ME,	// ÇëÇó¶Ô·½¶Ô¼ºenchant¼¼ÄÜ
	GM_MSG_REDUCE_CD,	// ¸øÄ¿±ê½µcd
	GM_MSG_DELIVER_TASK, // ·¢·ÅÈÎÎñ¸øÄ¿±ê
	GM_MSG_OBJ_ACTION_END,			//¶ÔÏóµÄactionÍê³É
//190	
	GM_MSG_OBJ_ACTION_REPEAT,		//±íÊ¾actionÒª¼ÌĞøÖ´ĞĞ 
	GM_MSG_SUBSCIBE_SUBTARGET,			//ÒªÇó¶©ÔÄÒ»¸ö´Î¼¶Ä¿±ê
	GM_MSG_UNSUBSCIBE_SUBTARGET,		//ÒªÇóÈ¡Ïû¶©ÔÄÒ»¸ö´Î¼¶Ä¿±ê
	GM_MSG_SUBSCIBE_SUBTARGET_CONFIRM, // È·ÈÏ´Î¼¶Ä¿±ê¶©ÔÄÊÇ·ñ´æÔÚ
	GM_MSG_NOTIFY_SELECT_SUBTARGET,	   // Í¨Öª¶©ÔÄÕß´Î¼¶¶©ÔÄÕß¸Ä±ä
//195
	GM_MSG_ATTACK_CRIT_FEEDBACK,
	GM_MSG_DELIVER_STORAGE_TASK,       // ·¢·ÅËæ»ú¿âÈÎÎñ
    GM_MSG_CHANGE_GENDER_LOGOUT,       // ½ÇÉ«±äĞÔ³É¹¦ºóÏÂÏß
//GMËù²ÉÓÃµÄÏûÏ¢	
	GM_MSG_GM_GETPOS=600,			//È¡µÃÖ¸¶¨Íæ¼ÒµÄ×ø±ê param ÊÇ cs_index, content ÊÇsid
	GM_MSG_GM_MQUERY_MOVE_POS,		//GMÒªÇó²éÑ¯×ø±ê ÓÃÓÚÏÂÒ»²½Ìø×ªµ½Íæ¼Ò´¦ 
	GM_MSG_GM_MQUERY_MOVE_POS_REPLY,	//GMÒªÇó²éÑ¯×ø±êµÄ»ØÓ¦,ÓÃÓÚGMµÄÌø×ªÃüÁî contentÊÇµ±Ç°µÄinstance key
	GM_MSG_GM_RECALL,			//GMÒªÇó½øĞĞÌø×ª
	GM_MSG_GM_CHANGE_EXP,			//GMÔö¼Óexp ºÍsp , param ÊÇ exp , content ÊÇsp
	GM_MSG_GM_ENDUE_ITEM,			//GM¸øÓëÁËÈô¸ÉÎïÆ· £¬param ÊÇitem id, content ÊÇÊıÄ¿ 
	GM_MSG_GM_ENDUE_SELL_ITEM,		//GM¸øÓëÁËÉÌµêÀïÂôµÄÎïÆ·£¬ÆäËûÍ¬ÉÏ
	GM_MSG_GM_REMOVE_ITEM,			//GMÒªÇóÉ¾³ıÄ³Ğ©ÎïÆ·£¬param ÊÇitem id, content ÊÇÊıÄ¿
	GM_MSG_GM_ENDUE_MONEY,			//GMÔö¼Ó»òÕß¼õÉÙ½ğÇ®
	GM_MSG_GM_RESURRECT,			//GMÒªÇó¸´»î
	GM_MSG_GM_OFFLINE,			//GMÒªÇóÏÂÏß 
	GM_MSG_GM_DEBUG_COMMAND,		//GMÒªÇóÏÂÏß 
	GM_MSG_GM_RESET_PP,			//GM½øĞĞÏ´µã²Ù×÷
	GM_MSG_GM_QUERY_SPEC_ITEM,	//GM²éÑ¯Íæ¼ÒÊÇ·ñ´æÔÚÖ¸¶¨ÎïÆ·
	GM_MSG_GM_REMOVE_SPEC_ITEM,	//GMÉ¾³ıÍæ¼ÒÖ¸¶¨ÎïÆ·

	GM_MSG_MAX,

};

struct msg_usermove_t	//ÓÃ»§ÒÆ¶¯²¢ÇÒ¿çÔ½±ß½çµÄÏûÏ¢
{
	int cs_index;
	int cs_sid;
	int user_id;
	A3DVECTOR newpos;	//ÏûÏ¢ÀïÃæÓĞoldpos
	size_t leave_data_size;	//Àë¿ª·¢ËÍµÄÏûÏ¢´óĞ¡£¨¸ÃÏûÏ¢¸½¼ÓÔÚºóÃæ)
	size_t enter_data_size;	//Àë¿ª·¢ËÍµÄÏûÏ¢´óĞ¡£¨¸ÃÏûÏ¢¸½¼ÓÔÚºóÃæ)
};

struct msg_aggro_info_t
{
	XID source;		//Ë­Éú³ÉÁËÕâĞ©³ğºŞ
	int aggro;		//³ğºŞµÄ´óĞ¡
	int aggro_type;		//³ğºŞµÄÀàĞÍ
	int faction;		//¶Ô·½ËùÊôµÄÅÉÏµ
	int level;		//¶Ô·½µÄ¼¶±ğ
};

struct msg_watching_t
{
	int level;		//Ô´µÄ¼¶±ğ
	int faction;		//Ô´µÄÅÉÏµ
	int invisible_degree;//Ô´µÄÒşÉí¼¶±ğ
};

struct msg_aggro_list_t
{
	int count;
	struct 
	{
		XID id;
		int aggro;
	}list[1];
};

struct msg_cry_for_help_t
{
	XID attacker;
	int lamb_faction;
	int helper_faction;
};

struct msg_aggro_alarm_t
{
	XID attacker;	//¹¥»÷Õß
	int rage;	
	int faction;	//·¢ËÍÕßµÄÅÉÏµ
	int target_faction;	//Ä¿±êµÄ½ÓÊÜÇó¾ÈÀàĞÍ
};

struct team_exp_entry
{
	int exp;
	int sp;
	XID who;
};

struct msg_exp_t
{
	int level;
	int exp;
	int sp;
};

struct msg_grp_exp_t
{
	int level;
	int exp;
	int sp;
	float rand;
};

struct msg_grpexp_t
{
	XID who;
	int damage;
	int reserve;
	/*
		×é¶ÓµÄÊı¾İ½Ï¶à
		×é¶ÓµÄ¾­ÑéÓÉ¶à¸öÈËµÄÉËº¦×é³É
		ËùÒÔ¸½´øÁËËùÓĞÈËµÄÉËº¦ÁĞ±í,
		ÁĞ±íµÄµÚÒ»¸öÔªËØ·Ö±ğÔÚwho.type,who.id damageÀï±£´æÁË ¾­ÑéÖµ level/sp ºÍ¶ÓÎéµÄteam_seq
		ÆäÖĞwho.idµÄ¸ß16Î»Îª¼¶±ğ£¬µÍ16Î»ÎªspµÄÊıÄ¿
		Õâ¸ö½á¹¹ÔÚnpc.cppÄÚ²¿µÄ¶ÔÏóÀàĞÍÎªTempDmgNode(±¸²é)

		Èç¹ûÊÇ¸Ã¶ÓÎéÔì³ÉÁË×î´óÉËº¦£¬ÔòÁĞ±íµÄµÚ¶ş¸öÔªËØ±£´æÁËÉ±ËÀµÄ¹ÖÎïÃû³ÆºÍ¼¶±ğ
		ÆäÖĞwho.type ±£´ænpc tid,who.id ±£´æÁËÒ»¸öËæ»úÊı,Ê¹µÃ´ó¼ÒÈÎÎñ½±ÀøÒ»ÖÂ

		µÚ¶ş¸öÔªËØµÄdamage Ê¼ÖÕ±£´æ¹ÖÎïµÄÊÀ½çtag£¬ÎŞÂÛÊÇ·ñ¸Ã¶ÓÎéÔì³É×î´óÉËº¦
	*/
};

struct gather_reply
{
	int can_be_interrupted;
	int eliminate_tool;	//ÏûºÄ¹¤¾ßµÄID
	unsigned short gather_time_min;
	unsigned short gather_time_max;
};

struct gather_result
{
	int amount;
	int task_id;
	int eliminate_tool;		//Èç¹ûÉ¾³ıÎïÆ·Ôò¸½¼Ó´ËID
	int mine_tid;
	int life;		//²É¼¯µ½ÎïÆ·µÄÊÙÃü
	char mine_type;	//¿óÎïµÄÀàĞÍ
};

struct msg_pickup_t
{
	XID who;
	int team_seq;
};

struct msg_gen_money
{
	int team_id;
	int team_seq;
};

struct msg_npc_transform
{
	int id_in_build;
	int time_use;
	int flag;
	int id_buildup;
	enum 
	{
		FLAG_DOUBLE_DMG_IN_BUILD = 1,
	};
};

struct msg_pet_pos_t
{
	A3DVECTOR pos;
	char inhabit_mode;
};

struct msg_pet_hp_notify
{
	float hp_ratio;
	int   cur_hp;
	char  aggro_state;		//ÈıÖÖ³ğºŞ×´Ì¬  0 ±»¶¯ 1 Ö÷¶¯ 2 ·¢´ô
	char  stay_mode;		//Á½ÖÖ¸úËæ·½Ê½: 0 ¸úËæ£¬1¡¡Í£Áô
	char  combat_state;		//ÊÇ·ñÔÚÕ½¶·
	char  attack_monster;	//ÊÇ·ñ¹¥»÷¹ÖÎï
	float mp_ratio;
	int   cur_mp;
};

struct msg_invisible_data
{
	int invisible_degree;
	int anti_invisible_degree;
};

struct msg_contribution_t
{
	int npc_id;				//npcµÄÄ£°åid
	bool is_owner;			//ÊÇ·ñÊÇ¹ÖÎïËùÊô£¬ËùÊôÅĞ¶¨Í¬ÈÎÎñÉ±¹Ö
	float team_contribution;//¶ÓÎé¹±Ï×£¬ÈçÍæ¼Ò²»ÔÚ¶ÓÎéÖĞÔòÎª¸öÈË¹±Ï×
	int team_member_count;	//¶ÓÎéÈËÊı£¬ÈçÍæ¼Ò²»ÔÚ¶ÓÎéÖĞÔòÎª1
	float personal_contribution;	//¸öÈË¹±Ï×¶È
};

struct msg_group_contribution_t
{
	int npc_id;				//npcµÄÄ£°åid
	bool is_owner;			//ÊÇ·ñÊÇ¹ÖÎïËùÊô£¬ËùÊôÅĞ¶¨Í¬ÈÎÎñÉ±¹Ö
	int count;
	struct _list{
		XID xid;	
		float contribution;
	}list[];
};

struct msg_plant_pet_hp_notify
{
	float hp_ratio;
	int   cur_hp;
	float mp_ratio;
	int   cur_mp;
};

struct msg_hp_mp_t
{
	int hp;
	int mp;
};

struct msg_query_spec_item_t
{
	int type;
	int cs_index;
	int cs_sid;
};

struct msg_remove_spec_item_t
{
	int type;
	unsigned char where;
	unsigned char index;
	size_t count;
	int cs_index;
	int cs_sid;
};

struct msg_congregate_req_t
{
	int world_tag;
	int level_req;
	int sec_level_req;
    int reincarnation_times_req;
};

struct msg_dps_dph_t
{
	int level;
	int dps;
	int dph;
	bool update_rank;
};

struct msg_player_t
{
	int id;
	int cs_index;
	int cs_sid;
};

struct msg_player_killed_info_t
{
	int cls;
	bool gender;
	int level;
	int force_id;
};

struct msg_hurt_extra_info_t
{
	bool orange_name;
	char attacker_mode;
};

struct msg_mafia_pvp_award_t
{
	int mafia_id;
	int domain_id;
};

struct msg_punish_me_t
{
	int skill_id;
	int skill_lvl;
};

struct msg_reduce_cd_t
{
	int skill_id;
	int msec;
};

#endif

