#include "../global.hpp"
#include "../helpers/colors.hpp"
#include "../helpers/helpers.hpp"
#include "gpio_parser.hpp"
#include "gpio_facade.hpp"
#include "gpio_main.hpp"

using namespace de::gpio;

#ifdef TEST_MODE_NO_WIRINGPI_LINK

#else
#include <wiringPi.h>
#endif



/// @brief Parses & executes messages received from uavos_comm"
/// @param parsed JSON message received from uavos_comm 
/// @param full_message 
/// @param full_message_length 
void CGPIOParser::parseMessage (Json_de &andruav_message, const char * full_message, const int & full_message_length)
{
    const int messageType = andruav_message[ANDRUAV_PROTOCOL_MESSAGE_TYPE].get<int>();
    bool is_binary = !(full_message[full_message_length-1]==125 || (full_message[full_message_length-2]==125));   // "}".charCodeAt(0)  IS TEXT / BINARY Msg  
    

    uint32_t permission = 0;
    if (validateField(andruav_message, ANDRUAV_PROTOCOL_MESSAGE_PERMISSION, Json_de::value_t::number_unsigned))
    {
        permission =  andruav_message[ANDRUAV_PROTOCOL_MESSAGE_PERMISSION].get<int>();
    }

    bool is_system = false;
    if ((validateField(andruav_message, ANDRUAV_PROTOCOL_SENDER, Json_de::value_t::string)) && (andruav_message[ANDRUAV_PROTOCOL_SENDER].get<std::string>().compare(SPECIAL_NAME_SYS_NAME)==0))
    {   // permission is not needed if this command sender is the communication server not a remote GCS or Unit.
        is_system = true;
    }

    UNUSED(is_system);
    UNUSED(permission);

    #ifdef DEBUG
    std::cout << _INFO_CONSOLE_TEXT << "RXmessage:" << _LOG_CONSOLE_BOLD_TEXT << andruav_message.dump() << _NORMAL_CONSOLE_TEXT_ << std::endl;
    #endif    
    

    if (messageType == TYPE_AndruavMessage_RemoteExecute)
    {
        parseRemoteExecute(andruav_message);

        return ;
    }

    else
    {
        Json_de cmd = andruav_message[ANDRUAV_PROTOCOL_MESSAGE_CMD];
        
        switch (messageType)
        {
            case TYPE_AndruavMessage_GPIO_ACTION:
            {
                /**
                * @brief This is a general purpose message 
                * 
                * a: P2P_ACTION_ ... commands
                * 
                */

                const Json_de cmd = andruav_message[ANDRUAV_PROTOCOL_MESSAGE_CMD];
        
                if (cmd.contains("i")) 
                {   // if module_key is specified then check if it is the same as the current module key.

                    const std::string module_key = cmd["i"].get<std::string>(); 
                    
                    de::gpio::CGPIOMain& cGPIOMain = de::gpio::CGPIOMain::getInstance();
                    if (module_key != cGPIOMain.getModuleKey())
                    {
                        // Module key mismatch
                        return;
                    }
                }

                if (!cmd.contains("a") || !cmd["a"].is_number_integer()) return ;
                    
                

                switch (cmd["a"].get<int>())
                {
                    case GPIO_ACTION_PORT_CONFIG:
                    {
                        /**
                         * 'm': set mode 
                         * 
                         * *    INPUT			        0
                         * *    OUTPUT			        1
                         * *    PWM_OUTPUT		        2
                         * *    PWM_MS_OUTPUT	        8
                         * *    PWM_BAL_OUTPUT          9
                         * *    GPIO_CLOCK		        3
                         * *    SOFT_PWM_OUTPUT		    4
                         * *    SOFT_TONE_OUTPUT	    5
                         * *    PWM_TONE_OUTPUT		    6
                         * *    PM_OFF		            7   // to input / release line
                         *  
                         * 'n': gpio name
                         * 'p': gpio number
                         * 'v': value [used in write mode.]
                         * 'r': read
                         */

                        if (!cmd.contains("p")) return ; // missing parameters

                        GPIO gpio;
                        gpio.pin_number = cmd["p"].get<int>();
                        gpio.pin_mode = cmd["m"].get<int>();
                        if (cmd.contains("v"))
                        {
                            gpio.pin_value = cmd["v"].get<int>();
                        }
                        if (cmd.contains("n"))
                        {
                            gpio.pin_name = cmd["n"].get<int>();
                        }
                        m_gpio_driver.configurePort (gpio);

                        // Send updated GPIO Status
                        CGPIO_Facade::getInstance().API_sendGPIOStatus("", true);

                    }
                    break;

                    case GPIO_ACTION_PORT_WRITE:
                    {
                        /**
                         * 'n': gpio name       // 1st priority
                         * 'p': gpio number     // 2nd priority
                         * 'v': value           // mandatory
                         */

                        // Mandatory value check
                        if (!cmd.contains("v")) return;

                        // Extract the GPIO value
                        const uint value = cmd["v"].get<int>();
                        
                        // Find the GPIO object
                        const GPIO* gpio = nullptr;
                        if (cmd.contains("n")) {
                            // Priority for named GPIO
                            gpio = m_gpio_driver.getGPIOByName(cmd["n"].get<std::string>());
                        } else if (cmd.contains("p")) {
                            // Fallback to GPIO number
                            gpio = m_gpio_driver.getGPIOByNumber(cmd["p"].get<uint>());
                        }

                        // Validate GPIO
                        if (gpio == nullptr) return; // GPIO not found


                        // Determine if an event should be triggered
                        bool trigger_event = false;
                        
                        
                        // Handle GPIO write based on mode
                        if (gpio->pin_mode == OUTPUT) {
                            if (gpio->pin_value != value) 
                            {
                                if (gpio->pin_name != "power_led")
                                {
                                    trigger_event = true;
                                }
                            }
                            m_gpio_driver.writePin(gpio->pin_number, value);
                        } else if (gpio->pin_mode == PWM_OUTPUT) {
                            // PWM mode requires PWM width
                            if (!cmd.contains("d")) return;

                            const uint pwm_width = cmd["d"].get<uint>();
                            if (gpio->pin_pwm_width != pwm_width) trigger_event = true;

                            m_gpio_driver.writePWM(gpio->pin_number, value, pwm_width);
                        }
                        

                        // Send updated GPIO Status
                        if (trigger_event)
                        {
                            CGPIO_Facade::getInstance().API_sendSingleGPIOStatus("", *gpio, false);
                        }
                    }
                    break;

                    case GPIO_ACTION_PORT_READ:
                    {
                        // TODO should reply to sender with a message contains value.

                        
                    }
                    break;

                    default:
                    {

                    }
                    break;                 


                };
            }
            break;


            case TYPE_AndruavMessage_GPIO_REMOTE_EXECUTE:
            {
                const int subCommand = cmd["a"].get<int>();
                switch (subCommand)
                {

                    case TYPE_AndruavMessage_GPIO_STATUS:
                        if (cmd.contains("p")) {
                            const GPIO* gpio = nullptr;
                            gpio = m_gpio_driver.getGPIOByNumber(cmd["p"].get<uint>());
                            if (gpio!= nullptr) 
                            {
                                CGPIO_Facade::getInstance().API_sendSingleGPIOStatus("", *gpio, false);
                                break;
                            }
                            // if pin number is not found then send all GPIO status
                        }

                        CGPIO_Facade::getInstance().API_sendGPIOStatus("", false);

                    break;

                    default:
                    break;
                }
            }
            break;       

            default:
            {

            }
            break;
        }

    }

    UNUSED(is_binary);
}

/**
 * @brief part of parseMessage that is responsible only for
 * parsing remote execute command.
 * 
 * @param andruav_message 
 */
void CGPIOParser::parseRemoteExecute (Json_de &andruav_message)
{
    const Json_de cmd = andruav_message[ANDRUAV_PROTOCOL_MESSAGE_CMD];
    
    if (!validateField(cmd, "C", Json_de::value_t::number_unsigned)) return ;
                
    uint32_t permission = 0;
    if (validateField(andruav_message, ANDRUAV_PROTOCOL_MESSAGE_PERMISSION, Json_de::value_t::number_unsigned))
    {
        permission =  andruav_message[ANDRUAV_PROTOCOL_MESSAGE_PERMISSION].get<int>();
    }

    bool is_system = false;
     
    if ((validateField(andruav_message, ANDRUAV_PROTOCOL_SENDER, Json_de::value_t::string)) && (andruav_message[ANDRUAV_PROTOCOL_SENDER].get<std::string>().compare(SPECIAL_NAME_SYS_NAME)==0))
    {   // permission is not needed if this command sender is the communication server not a remote GCS or Unit.
        is_system = true;
    }

    UNUSED (is_system);
    UNUSED (permission);
    
    const int remoteCommand = cmd["C"].get<int>();
    
    #ifdef DEBUG
    std::cout << "cmd: " << remoteCommand << std::endl;
    #endif 

    UNUSED (remoteCommand);
}