#ifndef CONFIG_XML_H_
#define CONFIG_XML_H_

#include "xml_parser.hpp"
#include "configdef.hpp"

namespace ConfigInfo
{

class config : public XML_Helper
{
public:
    config(xmlType_ type);
    ~config();

    // GET =========================================================================================
    std::string get_language_cfg();                             // get language
    bool getConsoleStatus();                                    // get console status
    std::string getRootPath();                                  // get root path

    bool get_executive_bodies(std::vector<sExecutiveBody>& exe_bodies);

    // SET =========================================================================================
    bool set_language_cfg(char* value);                   // set/add language
    bool setConsoleStatus(bool status);                         // set/add console status
    bool setRootPath(std::string path);                         // set/add root path
};

} // !namespace ConfigInfo
#endif  // CONFIG_XML_H_
