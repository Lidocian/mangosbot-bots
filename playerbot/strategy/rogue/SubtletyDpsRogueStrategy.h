#pragma once

#include "../Strategy.h"
#include "../generic/MeleeCombatStrategy.h"

namespace ai
{
   class SubtletyDpsRogueStrategy : public MeleeCombatStrategy
   {
   public:
      SubtletyDpsRogueStrategy(PlayerbotAI* ai);

   public:
      virtual void InitTriggers(std::list<TriggerNode*> &triggers);
      virtual string getName() { return "sub dps"; }
      virtual NextAction** getDefaultActions();
   };
}

