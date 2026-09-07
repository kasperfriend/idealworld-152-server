#include "playermall.h"
#include <spinlock.h>
#include "template/itemdataman.h"
#include "template/globaldataman.h"
#include "worldmanager.h"

namespace netgame{

bool
mall::AddGoods(const node_t & node)
{
	spin_autolock keeper(_lock);
	
	if(node.goods_id <= 0 || node.goods_count <= 0)
	{
		return false;
	}
	if(world_manager::GetWorldParam().southamerican_shop)
	{
		ASSERT(!(node.entry[0].cash_need % node.goods_count));
	}
	if(node.entry[0].cash_need <= 0) return false;
	_map.push_back(node);
	return true;
}

bool 
mall::QueryGoods(size_t index, node_t & n)
{
	spin_autolock keeper(_lock);

	if(index >= _map.size()) return false;
	n = _map[index];
/*	
	MAP::iterator it = _map.find(id);
	if(it == _map.end()) return false;
	n = it->second;
	*/
	return true;
}
//lgc
size_t mall::GetGoodsCount()
{
	spin_autolock keeper(_lock);
	return _map.size();
}


int mall::GetGroupId()
{
	spin_autolock keeper(_group_id_lock);
	return _group_id;
}

void mall::SetGroupId(int id)
{
	spin_autolock keeper(_group_id_lock);
	_group_id = id;
}

bool mall::AddLimitGoods(const node_t & node, int index)
{
	_limit_goods.push_back(index_node_t(node, index));
	return true;
}

bool mall::sale_time::CheckAvailable(time_t t) const
{
	switch(type)
	{
	case TYPE_NOLIMIT: return true;
	case TYPE_INTERZONE:
		if(param2) 
			return t > param1 && t < param2;
		else
			return t > param1;
	case TYPE_WEEK:
	{
		struct tm tm1;
		localtime_r(&t, &tm1);	
		return (1 << tm1.tm_wday) & param1;
	}
	case TYPE_MONTH:
	{
		struct tm tm1;
		localtime_r(&t, &tm1);	
		return (1 << tm1.tm_mday) & param1;
	}
	default: return false;
	}
	return false;	
}

bool mall::sale_time::CheckParam(int type, int param1, int param2) 
{
	switch(type)
	{
		case TYPE_NOLIMIT: 
			return param1 == 0 && param2 == 0;
		case TYPE_INTERZONE:
			if(param1 < 0 || param2 < 0) return false;
			if(!param1 && !param2)	return false;
			if(param1 && param2) return param1 < param2;
			else return true;
		case TYPE_WEEK:
			return param1 & 0x7f && !(param1 & 0xffffff80) && param2 == 0;
		case TYPE_MONTH:
			return param1 & 0xfffffffe && !(param1 & 0x01) && param2 == 0;
		default: return false;
	}
	return false;
}

//¼ì²éÏúÊÛÊ±¼ä¶Î(²»º¬¹ýÆÚÊ±¼äµÄ)ÊÇ·ñÓÐ³åÍ» true ÓÐ false Ã»ÓÐ£¬µ÷ÓÃÇ°Ó¦±£Ö¤Ã¿¸ösale_timeµÄ²ÎÊýÊÇÓÐÐ§µÄ
static bool CheckSaleTimeConflictNotExpired(const netgame::mall::sale_time& st1, const netgame::mall::sale_time& st2)
{
	if(st1.GetType() != st2.GetType())
	{
		if(st1.GetType() == netgame::mall::sale_time::TYPE_NOLIMIT || st2.GetType() == netgame::mall::sale_time::TYPE_NOLIMIT)
			return false;
		else
			return true;
	}
	else
	{
		switch(st1.GetType())
		{
			case netgame::mall::sale_time::TYPE_NOLIMIT:
				return true;
			case netgame::mall::sale_time::TYPE_INTERZONE:
			{
				int a = st1.GetParam1();
				int b = (st1.GetParam2() ? st1.GetParam2() : 0x7fffffff);
				int c = st2.GetParam1();
				int d = (st2.GetParam2() ? st2.GetParam2() : 0x7fffffff);
				return c < b && d > a;			
			}
			case netgame::mall::sale_time::TYPE_WEEK:
				return (st1.GetParam1() & 0x7f) & (st2.GetParam1() & 0x7f);
			case netgame::mall::sale_time::TYPE_MONTH:
				return (st1.GetParam1() & 0xfffffffe) & (st2.GetParam1() & 0xfffffffe);
			default: return true;
		}
	}
	return true;
}

static bool CheckSaleTimeConflictNotExpired(const netgame::mall::sale_time& st1, const netgame::mall::sale_time& st2, const netgame::mall::sale_time& st3)
{
	return CheckSaleTimeConflictNotExpired(st1, st2) || CheckSaleTimeConflictNotExpired(st1, st3) || CheckSaleTimeConflictNotExpired(st2, st3);	
}

static bool CheckSaleTimeConflictNotExpired(const netgame::mall::sale_time& st1, const netgame::mall::sale_time& st2, const netgame::mall::sale_time& st3, const netgame::mall::sale_time& st4)
{
	return CheckSaleTimeConflictNotExpired(st1, st2) || CheckSaleTimeConflictNotExpired(st1, st3) || CheckSaleTimeConflictNotExpired(st1, st4) || CheckSaleTimeConflictNotExpired(st2, st3) || CheckSaleTimeConflictNotExpired(st2, st4) || CheckSaleTimeConflictNotExpired(st3, st4);	
}


bool
InitMall(netgame::mall & __mall, itemdataman & dataman, const abase::vector<MALL_ITEM_SERV> & __list)
{
	//const abase::vector<MALL_ITEM_SERV> & __list = globaldata_getmallitemservice();
	for(size_t i = 0; i < __list.size(); i ++)
	{
		int id = __list[i].goods_id;
		int count = __list[i].goods_count;
		if(id <= 0 || count <= 0) return false;

		int pile_limit = dataman.get_item_pile_limit(id);
		if(pile_limit <= 0 || count > pile_limit) 
		{
			__PRINTINFO("°Ù±¦¸óÖÐÎïÆ·%d(%d¸ö)³¬¹ýÁË¶ÑµþÉÏÏÞ%d\n",id,count,pile_limit);
			return false;
		}

		netgame::mall::node_t node;
		memset(&node, 0, sizeof(node));

		node.goods_id = id;
		node.goods_count = count;

		ASSERT(netgame::mall::MAX_ENTRY == 4);
		bool bExpire = false;
		size_t slot_count = 0;
		int gid_array[4] = {-1,-1,-1,-1};	//lgc ±£´æµ±Ç°ÎïÆ·¹ºÂò·½Ê½ÖÐµÄ²»Í¬×éid
		size_t gid_count = 0;
		bool group_active = false;
		bool sale_time_active = false;		
		time_t t1 = time(NULL);
		struct tm tm1;
		localtime_r(&t1, &tm1);
		long wp_tz = 0; _get_timezone(&wp_tz);
		int tz_adjust = wp_tz;	//gshop±à¼­Æ÷±£´æµÄÊ±¼äÊÇgmtime,sale_time¸ù¾ÝgsÔËÐÐµÄÊ±Çø½øÐÐµ÷Õû
		for(size_t j = 0; j < 4;  j ++)
		{
			if(__list[i].list[j].cash_need <= 0) break;
			int expire_time = __list[i].list[j].expire_time;
			if(expire_time < 0)
			{
				__PRINTINFO("°Ù±¦¸óÖÐÎïÆ·%d¹ýÆÚÊ±¼äÐ¡ÓÚ0\n",id);	
				return false;
			}
			node.entry[slot_count].expire_type = __list[i].list[j].expire_date_valid? (netgame::mall::EXPIRE_TYPE_DATE) : (netgame::mall::EXPIRE_TYPE_TIME);
			node.entry[slot_count].expire_time = expire_time;
			node.entry[slot_count].cash_need = __list[i].list[j].cash_need;
			if(expire_time) bExpire = true;
			//lgc
			int st_type = __list[i].list[j].st_type;
			int st_param1 = __list[i].list[j].st_param1;
			int st_param2 = __list[i].list[j].st_param2;
			//Ê±Çøµ÷Õû
			if(st_type == netgame::mall::sale_time::TYPE_INTERZONE)
			{
				if(st_param1)	st_param1 += tz_adjust;	
				if(st_param2)	st_param2 += tz_adjust;	
			}
			if(!netgame::mall::sale_time::CheckParam(st_type, st_param1, st_param2))
			{
				__PRINTINFO("°Ù±¦¸óÖÐÎïÆ·%dÏúÊÛÊ±¼ä²ÎÊý²»ÕýÈ·\n",id);	
				return false;
			}
			node.entry[slot_count]._sale_time.SetParam(st_type, st_param1, st_param2);
			int group_id = __list[i].list[j].group_id;
			if(group_id < 0)
			{
				__PRINTINFO("°Ù±¦¸óÖÐÎïÆ·%d¹ºÂò·½Ê½×éidÐ¡ÓÚ0\n",id);	
				return false;
			}
			node.entry[slot_count].group_id = group_id;
			if(group_id != gid_array[0] && group_id != gid_array[1] && group_id != gid_array[2] && group_id != gid_array[3])
				gid_array[gid_count++] = group_id;
			node.entry[slot_count].status = __list[i].list[j].status;
			if(node.entry[slot_count].status < 0)
			{
				__PRINTINFO("°Ù±¦¸óÖÐÎïÆ·%dÉÌÆ·×´Ì¬Ð¡ÓÚ0\n",id);	
				return false;
			}
			if(st_type != netgame::mall::sale_time::TYPE_NOLIMIT)
				sale_time_active = true;
			if(group_id != 0)
				group_active = true;
			//
			slot_count ++;
		}
		
		if(slot_count == 0)
		{
			__PRINTINFO("°Ù±¦¸óÖÐÎïÆ·%d²»´æÔÚ¿ÉÓÃµÄÌõÄ¿Ë÷ÒýÒ%d\n", id, i);
			return false;
		}
		
		if(bExpire && pile_limit != 1)
		{
			__PRINTINFO("°Ù±¦¸óÖÐÎïÆ·´æÔÚ¿É¶Ñµþµ«´æÔÚÓÐÐ§ÆÚµÄÎïÆ·%d£¬ÎÞ·¨½øÐÐ³õÊ¼»¯²Ù×÷ Ë÷Òý%d\n", id, i);
			return false;
		}
		
		if(bExpire && sale_time_active)
		{
			__PRINTINFO("°Ù±¦¸óÖÐÎïÆ·%dÉè¶¨ÁË×â½èÊ±¼äµ«´æÔÚ²»ÊÇÓÀ¾ÃµÄÏúÊÛÊ±¼ä\n",id);	
			return false;
		}
		
		//¼ì²é¶àÖÖ¹ºÂò·½Ê½Ö®¼äÊÇ·ñ³åÍ».lgc
		if(bExpire)
		{
			bool isconflict = false;
			int et[4] = {0};
			
			for(size_t m=0; m<gid_count; m++)
			{
				int same_gid_slot_count = 0;	//Í¬Ò»¸ö×éÄÚ¹ºÂò·½Ê½Êý
				for(size_t n=0; n<slot_count ;n++)
					if(node.entry[n].group_id == gid_array[m])
						et[same_gid_slot_count++] = node.entry[n].expire_type==netgame::mall::EXPIRE_TYPE_DATE 
													? -node.entry[n].expire_time 
													: node.entry[n].expire_time;
				if(same_gid_slot_count == 1)
					continue;
				else if(same_gid_slot_count == 2)
					isconflict = et[0]==et[1];	
				else if(same_gid_slot_count == 3)
					isconflict = et[0]==et[1] || et[0]==et[2] || et[1]==et[2];
				else if(same_gid_slot_count == 4)
					isconflict = et[0]==et[1] || et[0]==et[2] || et[0]==et[3] || et[1]==et[2] || et[1]==et[3] || et[2]==et[3]; 
				else
				{
					ASSERT(false);
					return false;
				}
				if(isconflict)
				{
					__PRINTINFO("°Ù±¦¸óÖÐÎïÆ·%d×éid%d¹ýÆÚÊ±¼äÏàÍ¬\n", id, gid_array[m]);
					return false;
				}

			}
		}
		else
		{
			bool isconflict = false;
			netgame::mall::sale_time * pst[4] = {NULL, NULL, NULL, NULL};

			for(size_t m=0; m<gid_count; m++)
			{
				int same_gid_slot_count = 0;				
				for(size_t n=0; n<slot_count ;n++)
					if(node.entry[n].group_id == gid_array[m])
						pst[same_gid_slot_count++] = &(node.entry[n]._sale_time);
			
				if(same_gid_slot_count == 1)
					continue;
				else if(same_gid_slot_count == 2)
					isconflict = CheckSaleTimeConflictNotExpired(*pst[0], *pst[1]);
				else if(same_gid_slot_count == 3)
					isconflict = CheckSaleTimeConflictNotExpired(*pst[0], *pst[1], *pst[2]);
				else if(same_gid_slot_count == 4)
					isconflict = CheckSaleTimeConflictNotExpired(*pst[0], *pst[1], *pst[2], *pst[3]);
				else
				{
					ASSERT(false);
					return false;
				}
				if(isconflict)
				{
					__PRINTINFO("°Ù±¦¸óÖÐÎïÆ·%d×éid%d¶àÖÖÏúÊÛÊ±¼ä³åÍ»\n", id, gid_array[m]);
					return false;
				}
			}
		}
		
		if(__list[i].gift_id > 0)	//´æÔÚÔùÆ·
		{
			int gift_id = __list[i].gift_id;
			int gift_count = __list[i].gift_count;
			if(gift_count <= 0)
			{
				__PRINTINFO("°Ù±¦¸óÖÐÔùÆ·%dÊýÁ¿Ð¡ÓÚ0\n",gift_id);
				return false;
			}
			int gift_pile_limit = dataman.get_item_pile_limit(gift_id);
			if(gift_pile_limit <= 0 || gift_count > gift_pile_limit)
			{
				__PRINTINFO("°Ù±¦¸óÖÐÔùÆ·%d(%d¸ö)³¬¹ýÁË¶ÑµþÉÏÏÞ%d\n",gift_id,gift_count,gift_pile_limit);
				return false;
			}
			int gift_expire_time = __list[i].gift_expire_time;
			if(gift_expire_time < 0)
			{
				__PRINTINFO("°Ù±¦¸óÖÐÔùÆ·%d¹ýÆÚÊ±¼äÐ¡ÓÚ0\n",gift_id);
				return false;
			}
			if(gift_expire_time > 0 && gift_pile_limit != 1)
			{
				__PRINTINFO("°Ù±¦¸óÖÐÔùÆ·´æÔÚ¿É¶Ñµþµ«´æÔÚÓÐÐ§ÆÚµÄÎïÆ·%d£¬ÎÞ·¨½øÐÐ³õÊ¼»¯²Ù×÷\n", gift_id);
				return false;
			}
			int gift_log_price = __list[i].gift_log_price;
			if(gift_log_price < 0)
			{
				__PRINTINFO("°Ù±¦¸óÖÐÔùÆ·%dlog¼Û¸ñÐ¡ÓÚ0\n",gift_id);
				return false;
			}
			node.gift_id = gift_id;
			node.gift_count = gift_count;
			node.gift_expire_time = gift_expire_time;
			node.gift_log_price = gift_log_price;
		}
	
		memcpy(node.spec_owner,__list[i].spec_owner,sizeof(node.spec_owner));
		node.group_active = group_active;
		node.sale_time_active = sale_time_active;
		//º«·þ²»¿ª·Ågroup,sale_time
		if(world_manager::GetWorldParam().korea_shop || world_manager::GetWorldParam().southamerican_shop)
		{
			if(group_active || sale_time_active)
			{
				__PRINTINFO("°Ù±¦¸óÖÐµÄÎïÆ·%d,º«·þ²»ÔÊÐí³öÏÖ¹ºÂò·½Ê½µÄ×é¿ØÖÆºÍÏúÊÛÊ±¼ä¿ØÖÆ", id);
				return false;				
			}
			if(node.gift_id > 0)
			{
				__PRINTINFO("°Ù±¦¸óÖÐµÄÎïÆ·%d,º«·þÔÝ²»Ö§³ÖÂòÒ»ÔùÒ»\n", id);	
				return false;
			}
		}
		//ÔÝ²»¿ª·Å×é¿ØÖÆ¹¦ÄÜ
		if(group_active)
		{
				__PRINTINFO("°Ù±¦¸óÖÐµÄÎïÆ·%d,ÔÝ²»¿ª·Å¹ºÂò·½Ê½µÄ×é¿ØÖÆ", id);
				return false;				
		}
		__mall.AddGoods(node);
		if(group_active || sale_time_active)
			__mall.AddLimitGoods(node, i);

		//__PRINTINFO("¼ÓÈë%d¸ö%dµ½°×±¦¸óÖÐ\n",count,id);
	}
	__PRINTINFO("°Ù±¦¸óÄÚ¹²ÓÐ%dÑù»õÎï\n",__list.size());
	
	return true;
}

bool 
InitTouchShop(netgame::touchshop & __shop, itemdataman & dataman)
{
	DATA_TYPE dt;
	TOUCH_SHOP_CONFIG * config = (TOUCH_SHOP_CONFIG*) dataman.get_data_ptr(TOUCH_SHOP_CONFIG_ID, ID_SPACE_CONFIG, dt);
	if(config == NULL || dt != DT_TOUCH_SHOP_CONFIG)
	{
		return false;
	}

	const int  TSC_PAGE_MAX = sizeof(config->page)/sizeof(config->page[0]);
	const int  TSC_PAGE_ITEM_MAX = sizeof(config->page[0].item)/sizeof(config->page[0].item[0]);	

	for(int pi = 0; pi < TSC_PAGE_MAX; ++pi)
	{
		for(int i = 0; i < TSC_PAGE_ITEM_MAX; ++i)
		{
			int type = config->page[pi].item[i].id;
			int count = config->page[pi].item[i].num;
			int expire = config->page[pi].item[i].expire_timelength;

			if(type && NULL == dataman.get_item_for_sell(type))
			{
				printf("touch shop ´æÔÚ·Ç·¨ÎïÆ·%dpage%dindex%d\n",type,pi,i);

				return false;
			}

			int pile_limit = dataman.get_item_pile_limit(type);
			if(count > pile_limit || count < 0) 
			{
				printf("touch shop ´æÔÚ·Ç·¨ÊýÁ¿%dpage%dindex%d\n",count,pi,i);

				return false;
			}

			if(expire == 0 && world_manager::IsExpireItem(type))
			{
				printf("touch shop ´æÔÚ·Ç·¨ÊÙÃüÊ±¼ä%dpage%dindex%d\n",count,pi,i);

				return false;
			}

			__shop.AddGoods(config->page[pi].item[i].id, config->page[pi].item[i].num, 
					config->page[pi].item[i].price, config->page[pi].item[i].expire_timelength);
		}
	}

	return true;
}

}

