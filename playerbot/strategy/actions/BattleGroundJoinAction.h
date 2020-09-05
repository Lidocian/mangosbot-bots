#pragma once
#include "MovementActions.h"
#include "../../AiFactory.h"
#include "../../PlayerbotAIConfig.h"
#include "../ItemVisitors.h"
#include "../../RandomPlayerbotMgr.h"
#include "BattleGround.h"
#include "BattleGroundMgr.h"
#include "BattleGroundWS.h"
#include "ChooseTargetActions.h"
#include "CheckMountStateAction.h"
#include "G3D/Vector3.h"
#include "GameObject.h"

using namespace ai;

  class WsGJoinAction : public Action
  {
  public:
     WsGJoinAction(PlayerbotAI* ai, string name = "wsg join") : Action(ai, name) {}
     virtual bool Execute(Event event);

  protected:
     bool WsGjoin();
  };
