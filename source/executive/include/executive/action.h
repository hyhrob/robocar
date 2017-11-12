
#pragma once

#include <executive/executive_api.h>
#include <executive/executive_def.h>

namespace Driver
{
    /**
    *  @brief
    *    Action interface
    */
    class EXECUTIVE_API Action
    {
    public:
        /**
        *  @brief
        *    Constructor
        */
        Action();

        /**
        *  @brief
        *    Destructor
        */
        virtual ~Action();
        
        /**
        *  @brief
        *    Get type of action
        *
        *  @return
        *    Type of the device
        */
        virtual ActionType getType() = 0;
        
        /**
        *  @brief
        *    Do work
        *
        *  @return
        *    Code of running result
        */
        virtual unsigned int execute(SpeedPercent speed) = 0;
    };

} // namespace Driver
