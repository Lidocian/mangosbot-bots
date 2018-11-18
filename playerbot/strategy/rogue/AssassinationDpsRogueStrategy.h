#pragma once

#include "../Strategy.h"
#include "../generic/MeleeCombatStrategy.h"

namespace ai
{
    class AssassinationDpsRogueStrategy : public MeleeCombatStrategy
    {
    public:
       AssassinationDpsRogueStrategy(PlayerbotAI* ai);

    public:
        virtual void InitTriggers(std::list<TriggerNode*> &triggers);
        virtual string getName() { return "assa dps"; }
        virtual NextAction** getDefaultActions();
    };
}
