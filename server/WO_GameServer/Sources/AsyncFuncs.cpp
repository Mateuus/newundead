#include "r3dPCH.h"
#include "r3d.h"
#include "r3dNetwork.h"

#include "AsyncFuncs.h"
#include "ServerGameLogic.h"
#include "ObjectsCode/obj_ServerPlayer.h"
#include "ObjectsCode/WEAPONS/WeaponArmory.h"
#include "ObjectsCode/WEAPONS/WeaponConfig.h"

#include "../../EclipseStudio/Sources/backend/WOBackendAPI.h"

char* g_ServerApiKey = "bvx425698dg6GsnxwedszF";

CAsyncApiMgr*	g_AsyncApiMgr = NULL;

CAsyncApiJob::CAsyncApiJob()
{
	peerId       = -1;
	CustomerID   = 0;
	SessionID    = 0;
	CharID       = 0;

	ResultCode   = 0;
}

CAsyncApiJob::CAsyncApiJob(const obj_ServerPlayer* plr)
{
	peerId       = plr->peerId_;
	CustomerID   = plr->profile_.CustomerID;
	SessionID    = plr->profile_.SessionID;
	CharID       = plr->loadout_->LoadoutID;

	r3d_assert(CustomerID);
	r3d_assert(CharID);

	ResultCode   = 0;
}

CAsyncApiWorker::CAsyncApiWorker()
{
	InitializeCriticalSection(&csJobs_);

	idx_        = -1;
	hThread     = NULL;

	curJob_     = NULL;
	hStartEvt_  = CreateEvent(NULL, FALSE, FALSE, NULL);
	needToExit_ = false;
}

CAsyncApiWorker::~CAsyncApiWorker()
{
	r3d_assert(jobs_.size() == 0);
	r3d_assert(curJob_ == NULL);
	DeleteCriticalSection(&csJobs_);
}

static unsigned int WINAPI CAsyncApiWorker_WorkerThread(void* in_ptr)
{
	return ((CAsyncApiWorker*)in_ptr)->WorkerThread();
}

CAsyncApiJob* CAsyncApiWorker::GetNextJob()
{
	r3dCSHolder cs1(csJobs_);
	if(jobs_.size() == 0)
		return NULL;

	CAsyncApiJob* job = jobs_.front();
	jobs_.pop_front();

	return job;
}

void CAsyncApiWorker::ProcessJob(CAsyncApiJob* job)
{
	r3d_assert(curJob_ == NULL);
	curJob_ = job;

	// exec it
#ifdef _DEBUG
	r3dOutToLog("CAsyncApiWorker %d executing %s\n", idx_, job->desc);
#endif

	try
	{
		job->ResultCode = job->Exec();
	}
	catch(const char* msg)
	{
		r3dOutToLog("!!!! CAsyncApiWorker %d job crashed: %s\n", idx_, msg);
		job->ResultCode = 99;
	}

#ifdef _DEBUG
	r3dOutToLog("CAsyncApiWorker %d finished %s\n", idx_, job->desc);
#endif

	// if job is failed, remove all future jobs for that customerid
	if(job->ResultCode != 0 && job->CustomerID)
	{
		r3dOutToLog("!!!! CAsyncApiWorker %d job %s failed\n", idx_, job->desc);

		r3dCSHolder cs3(csJobs_);
		for(std::list<CAsyncApiJob*>::iterator it = jobs_.begin(); it != jobs_.end(); )
		{
			if((*it)->CustomerID == job->CustomerID)
			{
				r3dOutToLog("!!! CAsyncApiWorker %d removed future job %s\n", idx_, (*it)->desc);
				it = jobs_.erase(it);
				continue;
			}

			++it;
		}
	}

	// place it to finish queue
	{
		r3dCSHolder cs2(csJobs_);

		curJob_ = NULL;
		finished_.push_back(job);
	}
}

int CAsyncApiWorker::WorkerThread()
{
	while(true)
	{
		::WaitForSingleObject(hStartEvt_, INFINITE);

		while(CAsyncApiJob* job = GetNextJob())
		{
			ProcessJob(job);
		}

		r3dCSHolder cs1(csJobs_);
		if(jobs_.size() == 0 && needToExit_)
		{
			r3dOutToLog("CAsyncApiWorker %d finished\n", idx_);
			return 0;
		}
	}
}

CAsyncApiMgr::CAsyncApiMgr()
{
	StartWorkers();
}

CAsyncApiMgr::~CAsyncApiMgr()
{
	r3dOutToLog("CAsyncApiMgr stopping\n"); CLOG_INDENT;

	HANDLE hs[NUM_WORKER_THREADS] = {0};

	for(int i=0; i<NUM_WORKER_THREADS; i++)
	{
		hs[i] = workers_[i].hThread;

		workers_[i].needToExit_ = true;
		::SetEvent(workers_[i].hStartEvt_);
	}

	// wait for thread finishing
	DWORD w0 = ::WaitForMultipleObjects(NUM_WORKER_THREADS, hs, TRUE, 10000);
	if(w0 == WAIT_TIMEOUT) 
	{
		r3dOutToLog("!!!! some API thread wasn't finished\n");
	}
}

void CAsyncApiMgr::StartWorkers()
{
	r3dOutToLog("CAsyncApiMgr starting\n");

	for(int i=0; i<NUM_WORKER_THREADS; i++)
	{
		CAsyncApiWorker& worker = workers_[i];

		worker.idx_    = i;
		worker.hThread = (HANDLE)_beginthreadex(NULL, 0, CAsyncApiWorker_WorkerThread, &worker, 0, NULL);
	}
}

void CAsyncApiMgr::TerminateAll()
{
	r3dOutToLog("CAsyncApiMgr terminating all threads\n");

	for(int i=0; i<NUM_WORKER_THREADS; i++)
	{
		::TerminateThread(workers_[i].hThread, 2);
	}
}

void CAsyncApiMgr::GetStatus(char* text)
{
	*text = 0;
	for(int i=0; i<NUM_WORKER_THREADS; i++)
	{
		r3dCSHolder cs1(workers_[i].csJobs_);
		sprintf(text + strlen(text), "%d ", workers_[i].jobs_.size());
	}
}

void CAsyncApiMgr::AddJob(CAsyncApiJob* job)
{
	//r3dOutToLog("AddJob %s\n", job->desc); CLOG_INDENT;

	// search for less used worker
	int    workerIdx = 0;
	size_t minJobs   = 99;
	for(int i=0; i<NUM_WORKER_THREADS; i++)
	{
		CAsyncApiWorker& worker = workers_[i];
		r3dCSHolder cs1(worker.csJobs_);

		// check if we already have job with that customerid
		if(job->CustomerID)
		{
			if(worker.curJob_ && worker.curJob_->CustomerID == job->CustomerID)
			{
				workerIdx = i;
				break;
			}
			bool found = false;
			for(std::list<CAsyncApiJob*>::iterator it = worker.jobs_.begin(); it != worker.jobs_.end(); ++it)
			{
				if((*it)->CustomerID == job->CustomerID)
				{
					workerIdx = i;
					found     = true;
					break;
				}
			}
			if(found)
				break;
		}

		if(worker.jobs_.size() < minJobs)
		{
			minJobs   = worker.jobs_.size();
			workerIdx = i;
		}
	}

	// add job to worker
	CAsyncApiWorker& worker = workers_[workerIdx];
	r3dCSHolder cs1(worker.csJobs_);
	worker.jobs_.push_back(job);

	::SetEvent(worker.hStartEvt_);

	return;
}

void CAsyncApiMgr::Tick()
{
	for(int i=0; i<NUM_WORKER_THREADS; i++)
	{
		CAsyncApiWorker& worker = workers_[i];

		r3dCSHolder cs1(worker.csJobs_);

		for(std::list<CAsyncApiJob*>::iterator it = worker.finished_.begin(); it != worker.finished_.end(); ++it)
		{
			CAsyncApiJob* job = *it;

			// log that job is failed
			if(job->ResultCode != 0)
			{
				r3dOutToLog("CAsyncApiWorker %d job %s failed\n", worker.idx_, job->desc);

				if(strcmp(job->desc, "CJobAddLogInfo") != 0) // do NOT log it, or we can get into infinite loop
				{
					CJobAddLogInfo* job2 = new CJobAddLogInfo();
					job2->CheatID    = PKT_S2C_CheatWarning_s::CHEAT_Api;
					job2->CustomerID = job->CustomerID;
					job2->CharID     = job->CharID;
					job2->IP         = gServerLogic.GetExternalIP();
					sprintf(job2->Msg,  "Api");
					sprintf(job2->Data, "%d %d job: %d %s", job->CustomerID, job->CharID, job->ResultCode, job->desc);
					AddJob(job2);
				}
			}

			// peerless job
			if(job->peerId == -1)
			{
				job->OnSuccess();
				delete job;
				continue;
			}

			// see if this is still a valid peer
			ServerGameLogic::peerInfo_s& peer = gServerLogic.GetPeer(job->peerId);
			if(peer.CustomerID != job->CustomerID || peer.SessionID != job->SessionID)
			{
				//r3dOutToLog("CAsyncApiWorker %d job %s peer already disconnected\n", worker.idx_, job->desc);
			}
			else if(job->ResultCode != 0)
			{
				// prevent player from updating it's loadout after error
				//if(peer.player) 
				//peer.player->wasDisconnected_ = true;

				// player job failed, disconnect him
				//gServerLogic.DisconnectPeer(job->peerId, false, "API FAILED");
			}
			else
			{
				job->OnSuccess();
			}

			delete job;
		}

		worker.finished_.clear();
	}
}

//
//
// api classes
//
//
int CJobGroupAdd::Exec()
{
	CWOBackendReq req("api_SrvGroupAdd.aspx");
	req.AddSessionInfo(CustomerID, SessionID);
	req.AddParam("CustomerID", CustomerID);
	//req.AddParam("CharID", CharID);

	if(!req.Issue())
	{
		r3dOutToLog("!!!! api_SrvBan failed\n");
	}
	return 0;
}
CJobProcessUserJoin::CJobProcessUserJoin(int in_peerId) : CAsyncApiJob()
{
	sprintf(desc, "CJobProcessUserJoin[%d] %p", CustomerID, this);

	ServerGameLogic::peerInfo_s& peer = gServerLogic.GetPeer(in_peerId);
	r3d_assert(peer.startGameAns == 0);

	peerId       = in_peerId;
	CustomerID   = peer.CustomerID;
	SessionID    = peer.SessionID;
	CharID       = peer.CharID;

	r3d_assert(CustomerID);
	r3d_assert(CharID);
}

CJobSrvBuyItem::CJobSrvBuyItem(obj_ServerPlayer* p,wiInventoryItem w,wiStoreItem s)
{
	plr = p;
	item = w;
	store = s;
}

int CJobSrvBuyItem::Exec()
{
	CWOBackendReq req("api_SrvBuyItem.aspx");
	req.AddSessionInfo(plr->profile_.CustomerID,plr->profile_.SessionID);
	req.AddParam("ItemID",  item.itemID);
	if (store.pricePerm > 0)
		req.AddParam("Amount",store.pricePerm);
	else if (store.gd_pricePerm > 0)
		req.AddParam("Amount",store.gd_pricePerm);

	req.AddParam("BuyIdx",store.gd_pricePerm > 0 ? 8 : 4);
	if (!req.Issue())
	{
		r3dOutToLog("CJobSrvBuyItem Failed. Code %d\n",req.resultCode_);
		return req.resultCode_;
	}
	else
		r3dOutToLog("CJobSrvBuyItem %d Success\n",CustomerID);

	return 0;
}

void CJobSrvBuyItem::OnSuccess()
{
	if (ResultCode != 0)
	{
		PKT_C2S_BuyItemAns_s n1;
		n1.ansCode = 1;
		gServerLogic.p2pSendRawToPeer(plr->peerId_,&n1,sizeof(n1));
		return;
	}

	if (store.pricePerm > 0)
		plr->profile_.ProfileData.GamePoints -= store.pricePerm;
	else if (store.gd_pricePerm > 0)
		plr->profile_.ProfileData.GameDollars -= store.gd_pricePerm;

	plr->BackpackAddItem(item);
	gServerLogic.ApiPlayerUpdateChar(plr);

	PKT_C2S_BuyItemAns_s n1;
	n1.ansCode = 0;
	gServerLogic.p2pSendRawToPeer(plr->peerId_,&n1,sizeof(n1));
}

CJobTradeLog::CJobTradeLog(int customerid , int customerid2 , int loadoutid,int loadoutid2 ,const char* gmt ,const char* gmt2, int GameServerId , wiInventoryItem wi) : CAsyncApiJob()
{
	gamesid = GameServerId;
	item = wi;
	customerid1 = customerid;
	customerid22 = customerid2;
	loadoutid1 = loadoutid;
	loadoutid22 = loadoutid2;
	sprintf(gmt1,gmt);
	sprintf(gmt22,gmt2);
}
int CJobTradeLog::Exec()
{
	CWOBackendReq req("api_SrvTradeLog.aspx");
	char msg[512];
	sprintf(msg,"%d -> %d",customerid1,customerid22);
	req.AddParam("CustomerID",  msg);
	sprintf(msg,"%d -> %d",loadoutid1,loadoutid22);
	req.AddParam("CharID",  msg);
	sprintf(msg,"%s -> %s",gmt1,gmt22);
	req.AddParam("Gamertag",  msg);
	sprintf(msg,"%d , %d",(int)item.InventoryID,(int)item.itemID);
	req.AddParam("ItemID",  msg);
	sprintf(msg,"%d , Var1 %d, Var2 %d",item.quantity,item.Var1,item.Var2);
	req.AddParam("quantity",  msg);
	req.AddParam("GameServerId",  gamesid);

	if (!req.Issue())
		r3dOutToLog("CJobTradeLog Failed. Code %d\n",req.resultCode_);
	else
		r3dOutToLog("CJobTradeLog %d Success\n",CustomerID);

	return 0;
}

void CJobTradeLog::OnSuccess()
{

}

CJobBanID::CJobBanID(int in_CustomerID , char reason1[512]) : CAsyncApiJob()
{
	sprintf(desc, "CJobBanID[%d] %p", in_CustomerID, this);

	CustomerID   = in_CustomerID;

	sprintf(reason,reason1);
	r3d_assert(CustomerID);
}
void CJobBanID::OnSuccess()
{
}
int CJobBanID::Exec()
{
	CWOBackendReq req("api_BanID.aspx");
	req.AddSessionInfo(CustomerID, SessionID);
	req.AddParam("CustomerID",  CustomerID);
	req.AddParam("reason",  reason);
	if (!req.Issue())
		r3dOutToLog("apiBanID Failed. Code %d\n",req.resultCode_);
	else
		r3dOutToLog("apiBanID %d Success\n",CustomerID);

	return 0;
}
int CJobProcessUserJoin::Exec()
{
	// step 1 - get profile (login credentials is checked here as well)
	prof.CustomerID = CustomerID;
	prof.SessionID  = SessionID;
	GameJoinResult = prof.GetProfile(CharID);
	if(GameJoinResult != 0)
	{
		r3dOutToLog("!!!! prof.GetProfile failed, code: %d\n", GameJoinResult);
		return 0; // return success so we can pass GameJoinResult to main server loop
	}
	obj_ServerPlayer* tplr = gServerLogic.FindPlayerCustom(prof.CustomerID);
	if (tplr)
	{
		//tplr->loadout_->isTele = false;
		if (tplr->loadout_->GameServerId != gServerLogic.ginfo_.gameServerId)
		{
			//tplr->loadout_->isTele = true;
		}
	}
	// step 2, join the game
	CWOBackendReq req("api_SrvUserJoinedGame.aspx");
	req.AddSessionInfo(CustomerID, SessionID);
	req.AddParam("skey1",  g_ServerApiKey);
	req.AddParam("CharID", CharID);
	req.AddParam("g1", gServerLogic.ginfo_.mapId);
	req.AddParam("g2", gServerLogic.ginfo_.gameServerId);
	req.Issue();
	GameJoinResult = req.resultCode_;
	if(GameJoinResult != 0)
	{
		r3dOutToLog("!!!! api_SrvUserJoinedGame failed, code: %d\n", req.resultCode_);
		return 0; // return success so we can pass GameJoinResult to main server loop
	}

	return 0;
}

void CJobProcessUserJoin::OnSuccess()
{
	// check if we still have correct peer
	ServerGameLogic::peerInfo_s& peer = gServerLogic.GetPeer(peerId);
	r3d_assert(peer.CustomerID == CustomerID && peer.SessionID == SessionID);

	// set profile and map API error code to haveprofile error
	peer.temp_profile = prof;
	switch(GameJoinResult)
	{
	case 0: peer.startGameAns = PKT_S2C_StartGameAns_s::RES_Ok; break;
	case 1: peer.startGameAns = PKT_S2C_StartGameAns_s::RES_InvalidLogin; break; // invalid login session
	case 7: peer.startGameAns = PKT_S2C_StartGameAns_s::RES_StillInGame;  break; // user already in game (ResultCode from [WZ_SRV_UserJoinedGame2])
	default:peer.startGameAns = PKT_S2C_StartGameAns_s::RES_Failed; break; // some other generic error
	}
	return;
}

int CJobUserLeftGame::Exec()
{
	r3d_assert(GameMapId);
	r3d_assert(GameServerId);

	CWOBackendReq req("api_SrvUserLeftGame.aspx");
	req.AddSessionInfo(CustomerID, SessionID);
	req.AddParam("skey1",  g_ServerApiKey);
	req.AddParam("CharID", CharID);

	req.AddParam("g1", GameMapId);
	req.AddParam("g2", GameServerId);
	req.AddParam("s1", TimePlayed);

	if(!req.Issue())
	{
		r3dOutToLog("!!!! api_SrvUserLeftGame failed, code: %d , Rerun\n", req.resultCode_);
		//Exec();
		return req.resultCode_;
	}

	return 0;
}

static void UpdateChar_SetAttachments(CWOBackendReq& req, const wiCharDataFull& w)
{
	char attm[2][512];
	for(int i=0; i<2; i++) 
	{
		// should match arguments of parseCharAttachments
		sprintf(attm[i], "%d %d %d %d %d %d %d %d", 
			w.Attachment[i].attachments[0],
			w.Attachment[i].attachments[1],
			w.Attachment[i].attachments[2],
			w.Attachment[i].attachments[3],
			w.Attachment[i].attachments[4],
			w.Attachment[i].attachments[5],
			w.Attachment[i].attachments[6],
			w.Attachment[i].attachments[7]);
	}

	req.AddParam("attm1",  attm[0]);
	req.AddParam("attm2",  attm[1]);
}

static void UpdateChar_SetBackpack(CWOBackendReq& req, const wiCharDataFull& cur, const wiCharDataFull& old)
{
	int updIdx = 0;
	for(int i=0; i<wiCharDataFull::CHAR_MAX_BACKPACK_SIZE; i++) 
	{
		const wiInventoryItem& w1 = cur.Items[i];
		const wiInventoryItem& w2 = old.Items[i];
		if(w1 == w2)
			continue;

		//r3dOutToLog("backpack slot %d changed %d -> %d\n", i, w2.itemID, w1.itemID);

		char value[128];
		int op;
		if(w1.itemID && w2.itemID == 0)
			op = 0; // add
		else if(w1.itemID && w2.itemID)
			op = 1; // alter
		else if(w1.itemID == 0 && w2.itemID)
			op = 2; // remove
		else if(w1.itemID == 0 && w2.itemID == 0)
			continue; // slot was changed, but no items was modified
		sprintf(value, "%d %d %d %d %d %d", i, op, w1.itemID, w1.quantity, w1.Var1, w1.Var2);
		//r3dOutToLog("\n[Krit] UpdateChar_SetBackpack %d %d %d %d %d %d\n", i, op, w1.itemID, w1.quantity, w1.Var1, w1.Var2);

		char name[128];
		sprintf(name, "bp%d", updIdx++);
		req.AddParam(name, value);
	}
}

int CJobUpdateChar::Exec()
{
	const wiCharDataFull& slot = CharData;
	
	float startTime = r3dGetTime();
  
	CWOBackendReq req("api_SrvCharUpdate.aspx");
	req.AddSessionInfo(CustomerID, SessionID);
	req.AddParam("skey1",  g_ServerApiKey);
	req.AddParam("CharID", slot.LoadoutID);

	// character status
	char GamePos[256];
	sprintf(GamePos, "%.3f %.3f %.3f %.0f", slot.GamePos.x, slot.GamePos.y, slot.GamePos.z, slot.GameDir);
	req.AddParam("s1",          slot.Alive);
	req.AddParam("s2",          GamePos);
	req.AddParam("s3",          (int)slot.Health);
	req.AddParam("s4",          (int)slot.Hunger);
	req.AddParam("s5",          (int)slot.Thirst);
	req.AddParam("s6",          (int)slot.Toxic);
	req.AddParam("s7",          slot.Stats.TimePlayed);
	req.AddParam("s8",          slot.Stats.XP);
	req.AddParam("s9",          slot.Stats.Reputation);
	req.AddParam("sA",          slot.GameFlags);
	req.AddParam("sB",          GameDollars);
	
	UpdateChar_SetAttachments(req, slot);
	UpdateChar_SetBackpack(req, CharData, OldData);
	
	//trackable stats
	req.AddParam("ts00",        slot.Stats.KilledZombies);
	req.AddParam("ts01",        slot.Stats.KilledSurvivors);
	req.AddParam("ts02",        slot.Stats.KilledBandits);
	req.AddParam("ts03",        0);
	req.AddParam("ts04",        0);
	req.AddParam("ts05",        0);

		req.AddParam("GroupID",        slot.GroupID);
		req.AddParam("Mission1",        slot.Mission1);
		req.AddParam("legfall",        slot.legfall);
		req.AddParam("bleeding",        slot.bleeding);

	req.AddParam("SkillID0",	slot.Stats.skillid0);
	req.AddParam("SkillID1",	slot.Stats.skillid1);
	req.AddParam("SkillID2",	slot.Stats.skillid2);
	req.AddParam("SkillID3",	slot.Stats.skillid3);
	req.AddParam("SkillID4",	slot.Stats.skillid4);
	req.AddParam("SkillID5",	slot.Stats.skillid5);
	req.AddParam("SkillID6",	slot.Stats.skillid6);
	req.AddParam("SkillID7",	slot.Stats.skillid7);
	req.AddParam("SkillID8",	slot.Stats.skillid8);
	req.AddParam("SkillID9",	slot.Stats.skillid9);
	req.AddParam("SkillID10",	slot.Stats.skillid10);
	req.AddParam("SkillID11",	slot.Stats.skillid11);
	req.AddParam("SkillID12",	slot.Stats.skillid12);
	req.AddParam("SkillID13",	slot.Stats.skillid13);
	req.AddParam("SkillID14",	slot.Stats.skillid14);
	req.AddParam("SkillID15",	slot.Stats.skillid15);
	req.AddParam("SkillID16",	slot.Stats.skillid16);
	req.AddParam("SkillID17",	slot.Stats.skillid17);
	req.AddParam("SkillID18",	slot.Stats.skillid18);
	req.AddParam("SkillID19",	slot.Stats.skillid19);
	req.AddParam("SkillID20",	slot.Stats.skillid20);
	req.AddParam("SkillID21",	slot.Stats.skillid21);
	req.AddParam("SkillID22",	slot.Stats.skillid22);
	req.AddParam("SkillID23",	slot.Stats.skillid23);
	req.AddParam("SkillID24",	slot.Stats.skillid24);
	req.AddParam("SkillID25",	slot.Stats.skillid25);
	req.AddParam("SkillID26",	slot.Stats.skillid26);
	req.AddParam("SkillID27",	slot.Stats.skillid27);
	req.AddParam("SkillID28",	slot.Stats.skillid28);
	req.AddParam("SkillID29",	slot.Stats.skillid29);
	req.AddParam("SkillID30",	slot.Stats.skillid30);
	req.AddParam("SkillID31",	slot.Stats.skillid31);
	req.AddParam("SkillID32",	slot.Stats.skillid32);
	req.AddParam("SkillID33",	slot.Stats.skillid33);

	r3dOutToLog("UpdateData '%s'\n",slot.Gamertag);
	if(!req.Issue())
	{
		r3dOutToLog("!!!! UpdateCharThread failed, code: %d, ans: %s , Rerun\n", req.resultCode_, req.bodyStr_);
		//Exec();
		return req.resultCode_;
	}
	else
	{
	r3dOutToLog("UpdateChar %s Success\n",slot.Gamertag);
	}
	
	if(Disconnect)
	{
		float sleepTime = r3dGetTime() - startTime + DISCONNECT_WAIT_TIME;
		if(sleepTime > 0) 
			::Sleep((int)(sleepTime * 1000));
	}

	return 0;
}

void CJobUpdateChar::OnSuccess()
{
	r3dOutToLog("@@ UpdateCharThread: finished %d\n", CustomerID);

	// check if we still have correct peer - player can be already disconnected at the moment
	ServerGameLogic::peerInfo_s& peer = gServerLogic.GetPeer(peerId);
	if(peer.CustomerID != CustomerID || peer.SessionID != peer.SessionID || peer.status_ < ServerGameLogic::PEER_PLAYING)
	{
		r3dOutToLog("!! CJobUpdateChar %d/%d %d/%d\n", peer.CustomerID, CustomerID, peer.SessionID ,peer.SessionID);
		return;
	}
	r3d_assert(peer.player);

	//if (drop.itemID > 0)
	//	peer.player->OnPlayerUpdateCharSuccess(drop,slot);
	// confirm player if needed disconnect
	if(Disconnect)
	{
		peer.player->wasDisconnected_ = true;

		PKT_C2S_DisconnectReq_s n;
		gServerLogic.p2pSendToPeer(peerId, peer.player, &n, sizeof(n));

		gServerLogic.DisconnectPeer(peerId, false, "finished disconnect");
	}
}

int CJobChangeBackpack::Exec()
{
	CWOBackendReq req("api_CharBackpack.aspx");
	req.AddSessionInfo(CustomerID, SessionID);
	req.AddParam("CharID", CharID);
	req.AddParam("skey1",  g_ServerApiKey);

	req.AddParam("op",  56);
	req.AddParam("v1",  BackpackID);
	req.AddParam("v2",  BackpackSize);
	req.AddParam("v3",  0);

	// issue
	if(!req.Issue())
	{
		r3dOutToLog("!!!! BackpackChangeThread failed, code: %d\n", req.resultCode_);
		return req.resultCode_;
	}

	return 0;
}

void CJobChangeBackpack::OnSuccess()
{
	ServerGameLogic::peerInfo_s& peer = gServerLogic.GetPeer(peerId);
	if(peer.CustomerID != CustomerID || peer.SessionID != SessionID)
		return;

	peer.player->OnChangeBackpackSuccess(DroppedItems);
}

int CJobAddLogInfo::Exec()
{
	CWOBackendReq req("api_SrvAddLogInfo.aspx");
	req.AddParam("skey1", g_ServerApiKey);

	req.AddParam("s_id",    CustomerID);	// note - it CAN be ZERO for not-connected users
	req.AddParam("CharID",  CharID);
	req.AddParam("Gamertag",Gamertag);
	req.AddParam("GameSessionID", GameServerId);
	req.AddParam("CheatID", CheatID);
	req.AddParam("Msg",     Msg);
	req.AddParam("Data",    Data);

	char ipstr[128];
	sprintf(ipstr, "%u", 	IP);
	req.AddParam("IP",      ipstr);

	// issue
	if(!req.Issue())
	{
		r3dOutToLog("!!!! AddLogInfoThread failed, code: %d\n", req.resultCode_);
		return 0;
	}

	return 0;
}


int CJobMarketBuyItem::Exec()
{
	CWOBackendReq req("api_SrvBuyItem.aspx");
	req.AddSessionInfo(CustomerID, SessionID);
	req.AddParam("CharID", CharID);
	req.AddParam("skey1",  g_ServerApiKey);

	req.AddParam("ItemID",  ItemId);
	req.AddParam("BuyIdx",  BuyIdx);

	// issue
	if(!req.Issue())
	{
		r3dOutToLog("!!!! CJobMarketBuyItem failed, code: %d\n", req.resultCode_);
		return req.resultCode_;
	}

	return 0;
}

void CJobMarketBuyItem::OnSuccess()
{
	ServerGameLogic::peerInfo_s& peer = gServerLogic.GetPeer(peerId);
	if(peer.CustomerID != CustomerID || peer.SessionID != SessionID)
		return;

	//peer.player->OnMarketBuyItemSuccess();
}
int CJobBan::Exec()
{
	CWOBackendReq req("api_BanPlayer.aspx");
	req.AddParam("skey1", g_ServerApiKey);
	req.AddSessionInfo(CustomerID, SessionID);
	if(!req.Issue())
	{
		r3dOutToLog("!!!! CJobBan failed, code: %d\n", req.resultCode_);
		return req.resultCode_;
	}

	return 0;
}

void CJobBan::OnSuccess()
{
	ServerGameLogic::peerInfo_s& peer = gServerLogic.GetPeer(peerId);
	if(peer.CustomerID != CustomerID || peer.SessionID != SessionID)
		return;
}