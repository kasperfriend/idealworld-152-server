/*
	conf.cpp - ONET::Conf, the ini file reader of the cmlib (ONET) support
	library.

	The parser is the same state machine the daemon side uses in
	cnet/common/conf.h (GNET::Conf) - the two classes are siblings that grew
	apart when cmlib moved its reload() out of line - so a section header
	"[ name ]", "key = value" lines and '#' / ';' comments behave
	identically for gs and for the tools.  reload() is cheap when nothing
	changed: it only re-reads the file when its mtime moved, which is what
	makes "reload the config by touching the file" work for the server.

	Instance handling: Conf::GetInstance(name) is the singleton accessor the
	whole server uses; AppendConfFile() layers a second file on top of it
	(the values from the appended file win).  All access goes through the
	static RWLock, which in this build is just ONET::Thread::Mutex
	(a spinlock) - see cgame/include/threadpool.h.
*/

#include "conf.h"

#include <sys/stat.h>
#include <unistd.h>

#include <string>
#include <fstream>

namespace ONET
{

Conf Conf::instance;
Thread::RWLock Conf::locker;

void Conf::reload()
{
	struct stat st;
	Thread::RWLock::WRScoped l(locker);

	for (stat(filename.c_str(), &st); mtime != st.st_mtime; stat(filename.c_str(), &st))
	{
		mtime = st.st_mtime;
		std::ifstream ifs(filename.c_str());
		string line;
		section_type section;
		section_hash sechash;
		if (!confhash.empty()) confhash.clear();
		while (std::getline(ifs, line))
		{
			const char c = line[0];
			if (c == '#' || c == ';') continue;
			if (c == '[')
			{
				string::size_type start = line.find_first_not_of(" \t", 1);
				if (start == string::npos) continue;
				string::size_type end   = line.find_first_of(" \t]", start);
				if (end   == string::npos) continue;
				if (!section.empty()) confhash[section] = sechash;
				section = section_type(line, start, end - start);
				sechash.clear();
			}
			else
			{
				string::size_type key_start = line.find_first_not_of(" \t");
				if (key_start == string::npos) continue;
				string::size_type key_end   = line.find_first_of(" \t=", key_start);
				if (key_end == string::npos) continue;
				string::size_type val_start = line.find_first_of("=", key_end);
				if (val_start == string::npos) continue;
				val_start = line.find_first_not_of(" \t", val_start + 1);
				if (val_start == string::npos) continue;
				string::size_type val_end = line.find_last_not_of(" \t\r\n");
				if (val_end == string::npos) continue;
				if (val_end < val_start) continue;
				sechash[key_type(line, key_start, key_end - key_start)] =
					value_type(line, val_start, val_end - val_start + 1);
			}
		}
		if (!section.empty()) confhash[section] = sechash;
	}
}

/*
	Layer rhs on top of what we already hold.  rhs is a freshly parsed file
	(AppendConfFile), so its keys win; sections we do not know about are
	copied as a whole.
*/
void Conf::Merge(Conf & rhs)
{
	Thread::RWLock::WRScoped l(locker);
	for (conf_hash::const_iterator is = rhs.confhash.begin(), ie = rhs.confhash.end();
		is != ie; ++is)
	{
		section_hash & dest = confhash[(*is).first];
		const section_hash & src = (*is).second;
		for (section_hash::const_iterator ks = src.begin(), ke = src.end(); ks != ke; ++ks)
			dest[(*ks).first] = (*ks).second;
	}
}

void Conf::dump(FILE * out)
{
	if (!out) out = stdout;
	Thread::RWLock::RDScoped l(locker);
	fprintf(out, "# %s\n", filename.c_str());
	for (conf_hash::const_iterator is = confhash.begin(), ie = confhash.end(); is != ie; ++is)
	{
		fprintf(out, "[%s]\n", (*is).first.c_str());
		const section_hash & src = (*is).second;
		for (section_hash::const_iterator ks = src.begin(), ke = src.end(); ks != ke; ++ks)
			fprintf(out, "%s = %s\n", (*ks).first.c_str(), (*ks).second.c_str());
	}
	fflush(out);
}

}
