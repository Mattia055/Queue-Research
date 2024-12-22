#pragma once

#include "TemplateSet.hpp"

//TODO: Add fastflow queue Implementation

#include "LCRQ.hpp"
#include "LPRQ.hpp"
#include "FAArray.hpp"
//Fastflow Queue
#include "MuxQueue.hpp"


using UnboundedQueues   = TemplateSet<FAAQueue,LCRQueue,LPRQueue,LinkedMuxQueue>;
using BoundedQueues     = TemplateSet<BoundedCRQueue,BoundedPRQueue,BoundedMuxQueue>;