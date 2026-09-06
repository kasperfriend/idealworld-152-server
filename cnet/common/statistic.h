/*
 * implementing statistic.
 */
#ifndef __GNET_STATISTIC_H__
#define __GNET_STATISTIC_H__

#ifdef WIN32

#define STAT_MIN5(ds,value)
#define STAT_HOUR(ds,value)
#define STAT_DAY(ds,value)

/* The real collector aggregates counters in memory and periodically dumps
 * them over syslog.  On Windows we keep the same class/interface so that
 * log.h and friends compile, but do not spin up the dump machinery. */
#include <string>
#include <stdint.h>

namespace GNET
{
	class Statistic
	{
	public:
		enum StatInterval { min5, hour, day };
		int     m_interval;
		int64_t m_max;
		int64_t m_min;
		int64_t m_cur;
		int64_t m_cnt;
		int64_t m_sum;

		Statistic() : m_interval(min5), m_max(0), m_min(0),
			m_cur(0), m_cnt(0), m_sum(0) { }

		void update( int64_t __delta )
		{
			m_cur = __delta;
			m_cnt ++;
			m_sum += __delta;
			if (__delta > m_max) m_max = __delta;
			if (0 == m_min || __delta < m_min) m_min = __delta;
		}

		typedef bool (*EnumerateFunc)( const std::string & __name,
		                               const Statistic * __pstat );
		static void enumerate( EnumerateFunc __fn, StatInterval __iv )
		{
			(void)__fn; (void)__iv;
		}

		static Statistic * GetInstance( const std::string & __name )
		{
			(void)__name;
			static Statistic s_stat;
			return &s_stat;
		}
		static Statistic * GetStatHour( const std::string & __name )
		{
			(void)__name;
			static Statistic s_stat;
			return &s_stat;
		}
		static Statistic * GetStatDay( const std::string & __name )
		{
			(void)__name;
			static Statistic s_stat;
			return &s_stat;
		}
	};
}

#else

#include <iostream>
#include <string>
#include <memory.h>
#include <stdint.h>
#include <map>

#if defined _REENTRANT_
#include <pthread.h>
#endif

#ifndef _NOSTAT
#define STAT_MIN5(ds,value)	Statistic::GetInstance(std::string(ds))->update(value)
#define STAT_HOUR(ds,value)	Statistic::GetStatHour(std::string(ds))->update(value)
#define STAT_DAY(ds,value)	Statistic::GetStatDay(std::string(ds))->update(value)
#else
#define STAT_MIN5(ds,value)
#define STAT_HOUR(ds,value)
#define STAT_DAY(ds,value)
#endif

namespace GNET
{
	/*
			ͳ�����ʹ�á�ͳ����Ϣ��ͨ��syslogdÿ��5�����Զ����浽/var/log/statinfo�ļ��У�
			��ͨ��cricket�����������ʾ���鿴ͳ�ƽ����

			������ʾ��ͳ��QQ����Ϣ������ע��"QQ.Msg"�Ǹ�ͳ�Ƶı�ʶ��Ӧ��Ψһ��

			Statistic * pstat = GNET::Statistic::GetInstance(std::string("QQ.Msg"));

			���Խ�pstat����������ÿ�η�����Ϣʱ������
				pstat->update( 1 ); // 1Ϊ���η��͵���Ϣ����
				���� GNET::Statistic::GetInstance(std::string("QQ.Msg"))->update(1);
	*/
	class Statistic
	{
	private:

		struct stringcasecmp
		{
			bool operator() (const std::string &x, const std::string &y) const
			{ return strcasecmp(x.c_str(), y.c_str()) < 0; }
		};

		typedef std::map<const std::string,Statistic,stringcasecmp> Map;
		static Map & GetMap( ) { static Map s_map; return s_map; }

#if defined _REENTRANT_
		static pthread_mutex_t & GetMapLocker()
		{	static pthread_mutex_t locker_map = PTHREAD_MUTEX_INITIALIZER; return locker_map;	}
#endif

	public:
		enum StatInterval
		{
			any = 0,
			min5  = 1,
			hour = 2,
			day  = 3
		};
		int		m_interval;

		// stat data
		int64_t m_max;
		int64_t m_min;
		int64_t m_cur;
		int64_t m_cnt;
		int64_t m_sum;

		Statistic() : m_interval(min5), m_max(0), m_min(0), m_cur(0), m_cnt(0), m_sum(0) { }

		virtual ~Statistic() { }

		void reset( )
		{
			m_cur = 0;
			m_cnt = 0;
			m_sum = 0;
			m_max = 0;
			m_min = 0;
		}

		void update( int64_t __delta )
		{
			m_cur = __delta;
			m_cnt ++;
			m_sum += __delta;
			m_max = std::max((int64_t)__delta,m_max);
			m_min = (0 == m_min ? __delta : std::min((int64_t)__delta,m_min));
		}

		static Statistic * GetInstance( const std::string& __name )
		{
#if defined _REENTRANT_
			pthread_mutex_lock( &GetMapLocker() );
			Statistic * p = &(GetMap()[__name]);
			pthread_mutex_unlock( &GetMapLocker() );
			return p;
#else
			return &(GetMap()[__name]);
#endif
		}

		static Statistic * GetStatHour( const std::string& __name )
		{
#if defined _REENTRANT_
			pthread_mutex_lock( &GetMapLocker() );
			Statistic * p = &(GetMap()[__name]);
			pthread_mutex_unlock( &GetMapLocker() );
			p->m_interval = hour;
			return p;
#else
			Statistic * p = &(GetMap()[__name]);
			p->m_interval = hour;
			return p;
#endif
		}

		static Statistic * GetStatDay( const std::string& __name )
		{
#if defined _REENTRANT_
			pthread_mutex_lock( &GetMapLocker() );
			Statistic * p = &(GetMap()[__name]);
			pthread_mutex_unlock( &GetMapLocker() );
			p->m_interval = day;
			return p;
#else
			Statistic * p = &(GetMap()[__name]);
			p->m_interval = day;
			return p;
#endif
		}

		static bool enumdefault( const std::string & __name, const Statistic * __pstat )
		{
			std::cout << std::endl;
			std::cout << __name << std::endl;
			std::cout << " MAX: " << __pstat->m_max << " MIN: " << __pstat->m_min
				<< " CUR: " << __pstat->m_cur << " CNT: " << __pstat->m_cnt
				<< " SUM: " << __pstat->m_sum;
			std::cout << std::endl;
			std::cout << std::endl;
			std::cout.flush();
			return true;
		}

		static void enumerate( bool (*f)(const std::string &,const Statistic *),
							StatInterval __interval = any )
		{
#if defined _REENTRANT_
			pthread_mutex_lock( &GetMapLocker() );
			Map map = GetMap();
			pthread_mutex_unlock( &GetMapLocker() );
			for( Map::const_iterator i = map.begin(), e = map.end(); i!=e; ++i )
			{
				if( (*i).second.m_interval == __interval || any == __interval )
				{
					if( !f((*i).first,&((*i).second)) )
						break;
				}
			}
#else
			for( Map::const_iterator i = GetMap().begin(), e = GetMap().end(); i!=e; ++i )
			{
				if( (*i).second.m_interval == __interval || any == __interval )
				{
					if( !f((*i).first,&((*i).second)) )
						break;
				}
			}
#endif
		}

		static void resetall( StatInterval __interval = any )
		{
#if defined _REENTRANT_
			pthread_mutex_lock( &GetMapLocker() );
			Map map = GetMap();
			pthread_mutex_unlock( &GetMapLocker() );

			for( Map::iterator i = map.begin(), e = map.end(); i!=e; ++i )
			{
				if( (*i).second.m_interval == __interval || any == __interval )
					(*i).second.reset();
			}
#else
			for( Map::iterator i = GetMap().begin(), e = GetMap().end(); i!=e; ++i )
			{
				if( (*i).second.m_interval == __interval || any == __interval )
					(*i).second.reset();
			}
#endif
		}

	};
}

#endif // WIN32

#endif // __GNET_STATISTIC_H__
