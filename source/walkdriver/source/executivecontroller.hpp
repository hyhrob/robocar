
#include <walkdriver/walkdriver.h>
#include <walkdriver/ExecutiveDevice.h>


namespace WalkDriver
{
    // electrical level
    enum ElectricalLevel
    {
        NegativeLevel = -1,     // ����ƽ
        ZeroLevel,              // ���ƽ
        PositiveLevel,          // ����ƽ
    };

    class ExecutiveController
    {
    public:
        ExecutiveController()
        {

        }
        ~ExecutiveController()
        {

        }
        
        virtual int setPin(int index, ElectricalLevel lvl) = 0;
    }
    

} // namespace WalkDriver
