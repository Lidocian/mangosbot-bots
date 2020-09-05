#include "ObjectGuid.h"
#include "botpch.h"
#include "../../playerbot.h"
#include "../../playerbotAI.h"
#include "LfgActions.h"
#include "../../AiFactory.h"
//#include "../../PlayerbotAIConfig.h"
//#include "../ItemVisitors.h"
#include "../../RandomPlayerbotMgr.h"
//#include "../../../../game/LFGMgr.h"
#include "strategy/values/PositionValue.h"
//#include "ServerFacade.h"
#include "GridNotifiers.h"
#include "GridNotifiersImpl.h"
#include "CellImpl.h"
#include "Object.h"
#include "ObjectMgr.h"
#include "strategy/values/LastMovementValue.h"
#include "strategy/actions/LogLevelAction.h"
#include "strategy/values/LastSpellCastValue.h"
#include "MovementActions.h"
#include "MotionMaster.h"
#include "MovementGenerator.h"
#include "../values/PositionValue.h"
#include "MotionGenerators/TargetedMovementGenerator.h"
#include "BattleGround.h"
#include "BattleGroundMgr.h"
#include "BattlegroundJoinAction.h"

using namespace ai;


bool WsGJoinAction::Execute(Event event)
{
   //if (!sPlayerbotAIConfig.randomBotJoinBG)
     // return false;

   if (bot->IsDead())
      return false;

   if (bot->getLevel() < 70)
      return false;

   if (!sRandomPlayerbotMgr.IsRandomBot(bot))
      return false;

   if (bot->InBattleGroundQueue())
      return false;

   if (bot->IsBeingTeleported())
      return false;

   Map* map = bot->GetMap();
   if (map && map->Instanceable())
      return false;

   return WsGjoin();
}

bool WsGJoinAction::WsGjoin()
{
   WorldSession *m_session = bot->GetSession();

   Player* bot = m_session->GetPlayer();

   if (bot->getLevel() < 70)
      return false;

   ObjectGuid guid = bot->GetObjectGuid();
   uint32 bgTypeId_ = BATTLEGROUND_WS;
   uint32 instanceId = 0;
   bool joinAsGroup = bot->GetGroup() && bot->GetGroup()->GetLeaderGuid() == guid ? true : false;
   bool isPremade = false;
   Group* grp = nullptr;

   BattleGroundTypeId bgTypeId = BattleGroundTypeId(bgTypeId_);

   DEBUG_LOG("WORLD: Received opcode CMSG_BATTLEMASTER_JOIN from %s", guid.GetString().c_str());

   // can do this, since it's battleground, not arena
   BattleGroundQueueTypeId bgQueueTypeId = BattleGroundMgr::BGQueueTypeId(bgTypeId, ARENA_TYPE_NONE);

   // ignore if player is already in BG
   if (bot->InBattleGround())
      return false;

   // get bg instance or bg template if instance not found
   BattleGround* bg = nullptr;
   if (instanceId)
      bg = sBattleGroundMgr.GetBattleGroundThroughClientInstance(instanceId, bgTypeId);
   if (!bg)
      bg = sBattleGroundMgr.GetBattleGroundTemplate(bgTypeId);
   if (!bg)
   {
      sLog.outError("Battleground: no available bg / template found");
      return false;
   }

   BattleGroundBracketId bgBracketId = bot->GetBattleGroundBracketIdFromLevel(bgTypeId);

   // check queue conditions
   if (!joinAsGroup)
   {
      // check Deserter debuff
      if (!bot->CanJoinToBattleground())
      {
         WorldPacket data(SMSG_GROUP_JOINED_BATTLEGROUND, 4);
         data << uint32(0xFFFFFFFE);
         bot->GetSession()->SendPacket(&data);
         return false;
      }
      // check if already in queue
      if (bot->GetBattleGroundQueueIndex(bgQueueTypeId) < PLAYER_MAX_BATTLEGROUND_QUEUES)
         // player is already in this queue
         return false;
      // check if has free queue slots
      if (!bot->HasFreeBattleGroundQueueId())
         return false;
   }
   else
   {
      grp = bot->GetGroup();
      // no group found, error
      if (!grp)
         return false;
      uint32 err = grp->CanJoinBattleGroundQueue(bgTypeId, bgQueueTypeId, 0, bg->GetMaxPlayersPerTeam(), false, 0);
      isPremade = sWorld.getConfig(CONFIG_UINT32_BATTLEGROUND_PREMADE_GROUP_WAIT_FOR_MATCH) &&
         (grp->GetMembersCount() >= bg->GetMinPlayersPerTeam());
      if (err != BG_JOIN_ERR_OK)
      {
         m_session->SendBattleGroundOrArenaJoinError(err);
         return false;
      }
   }
   // if we're here, then the conditions to join a bg are met. We can proceed in joining.

   // _player->GetGroup() was already checked, grp is already initialized
   BattleGroundQueue& bgQueue = sBattleGroundMgr.m_BattleGroundQueues[bgQueueTypeId];
   if (joinAsGroup)
   {
      DEBUG_LOG("Battleground: the following players are joining as group:");
      GroupQueueInfo* ginfo = bgQueue.AddGroup(bot, grp, bgTypeId, bgBracketId, ARENA_TYPE_NONE, false, isPremade, 0);
      uint32 avgTime = bgQueue.GetAverageQueueWaitTime(ginfo, bot->GetBattleGroundBracketIdFromLevel(bgTypeId));
      for (GroupReference* itr = grp->GetFirstMember(); itr != nullptr; itr = itr->next())
      {
         Player* member = itr->getSource();
         if (!member)
            continue;                                   // this should never happen

         uint32 queueSlot = member->AddBattleGroundQueueId(bgQueueTypeId);           // add to queue

         // store entry point coords (same as leader entry point)
         member->SetBattleGroundEntryPoint(bot);

         // send status packet (in queue)
         WorldPacket data;
         sBattleGroundMgr.BuildBattleGroundStatusPacket(&data, bg, queueSlot, STATUS_WAIT_QUEUE, avgTime, 0, ginfo->arenaType, TEAM_NONE);
         member->GetSession()->SendPacket(&data);
         sBattleGroundMgr.BuildGroupJoinedBattlegroundPacket(&data, bgTypeId);
         member->GetSession()->SendPacket(&data);
         DEBUG_LOG("Battleground: player joined queue for bg queue type %u bg type %u: GUID %u, NAME %s", bgQueueTypeId, bgTypeId, member->GetGUIDLow(), member->GetName());
      }
      DEBUG_LOG("Battleground: group end");
   }
   else
   {
      GroupQueueInfo* ginfo = bgQueue.AddGroup(bot, nullptr, bgTypeId, bgBracketId, ARENA_TYPE_NONE, false, isPremade, 0);
      uint32 avgTime = bgQueue.GetAverageQueueWaitTime(ginfo, bot->GetBattleGroundBracketIdFromLevel(bgTypeId));
      // already checked if queueSlot is valid, now just get it
      uint32 queueSlot = bot->AddBattleGroundQueueId(bgQueueTypeId);
      // store entry point coords
      bot->SetBattleGroundEntryPoint();

      WorldPacket data;
      // send status packet (in queue)
      sBattleGroundMgr.BuildBattleGroundStatusPacket(&data, bg, queueSlot, STATUS_WAIT_QUEUE, avgTime, 0, ginfo->arenaType, TEAM_NONE);
      m_session->SendPacket(&data);
      DEBUG_LOG("Battleground: player joined queue for bg queue type %u bg type %u: GUID %u, NAME %s", bgQueueTypeId, bgTypeId, bot->GetGUIDLow(), bot->GetName());
   }
   sBattleGroundMgr.ScheduleQueueUpdate(0, ARENA_TYPE_NONE, bgQueueTypeId, bgTypeId, bot->GetBattleGroundBracketIdFromLevel(bgTypeId));
   return true;
}