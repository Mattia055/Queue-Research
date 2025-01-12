#pragma once

#include "TemplateSet.hpp"

//TODO: Add fastflow queue Implementation

#include "LCRQ.hpp"
#include "LPRQ.hpp"
#include "FAArray.hpp"
//Fastflow Queue
#include "LMTQ.hpp"
#include "MuxQueue.hpp"


using UnboundedQueues   = TemplateSet<FAAQueue,LCRQueue,LPRQueue,LinkedMuxQueue>;
using BoundedQueues     = TemplateSet<BoundedCRQueue,BoundedPRQueue,BoundedMuxQueue,BoundedMTQueue>;
using Queues            = UnboundedQueues::Cat<BoundedQueues>;