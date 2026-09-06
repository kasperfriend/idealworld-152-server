
TOP_SRCDIR = ..

# SINGLE_THREAD used to be "true" here.  That mode cannot be built from
# source any more: without -D_REENTRANT_ cnet/common/thread.cpp does not
# define the ONET/GNET thread pool statics and cnet/io/pollio.cpp does not
# define PollControl::writer, both of which the shared logclient code
# (cnet/logclient/log.cpp) references unconditionally - the link only worked
# back then because the checked in objects had been compiled with
# -D_REENTRANT_ anyway.  The other daemons (gauthd, gamedbd, gamed) already
# build this way.
SINGLE_THREAD = false
DEBUG_VERSION = true

include ../mk/gcc.defs.mk

INCLUDES += -I$(TOP_SRCDIR)/logclient -I$(TOP_SRCDIR)/log_inl -I$(TOP_SRCDIR)/include
#DEFINES += -DUSE_EPOLL -O3 -DUSE_LOGCLIENT -ipo
DEFINES += -DUSE_EPOLL -DUSE_LOGCLIENT -g -ggdb 
DEFINES += -D__USER__=\"$(USER)\"
#LDFLAGS += -O3 -static -ipo
LDFLAGS += -O0

OUTEROBJS = $(CO_DIR)/matcher.o trade.o ./mailbox.o ./postoffice.o ./auctionmarket.o ./serverattr.o ./pshopmarket.o
CLEAN += $(OUTEROBJS)

OBJS = map.o gdeliveryserver.o gauthclient.o gproviderserver.o ganticheatclient.o uniquenameclient.o state.o stubs.o \
	   gdeliveryd.o gamedbclient.o gfactionclient.o gmrestartserver.o groledbclient.o chatroom.o battlemanager.o \
	   domaindataman.o countrybattleman.o playerlogin.o maptaskdata.o stockexchange.o billingagent.o domaindaemon.o gamemaster.o \
	   referencemanager.o refspreadcode.o rewardmanager.o gwebtradeclient.o webtrademarket.o cert.o sysauctionmanager.o \
	   factionfortressmanager.o gametalkmanager.o gametalkclient.o snsclient.o snsmanager.o forcemanager.o	\
	   friendextinfomanager.o discountman.o playerlogin_re.o globalcontrol.o disabled_system.o centraldeliveryclient.o centraldeliveryserver.o crosssystem.o \
	   namemanager.o kingelection.o gdeliverytool.o playerprofileman.o autoteamman.o uniquedataserver.o tankbattlemanager.o factionresourcebattleman.o mappasswd.o waitqueue.o
	   
all : gdeliveryd

gdeliveryd : $(SHAREOBJ) $(OBJS) $(OUTEROBJS) $(SHARE_SOBJ)  $(LOGSTUB) $(LOGOBJ)
	$(LD) $(LDFLAGS) -o $@ $(OUTEROBJS) $(OBJS) $(SHAREOBJ) $(SHARE_SOBJ) $(LOGSTUB) $(LOGOBJ) -lpcre -lcrypto -ldl

include ../mk/gcc.rules.mk

