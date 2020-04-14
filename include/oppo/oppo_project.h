/*
 *
 * yixue.ge add for oppo project
 *
 *
 */
#ifndef _OPPO_PROJECT_H_
#define _OPPO_PROJECT_H_

enum PCB_VERSION{
    HW_VERSION__UNKNOWN,
    HW_VERSION__10,		//EVB
    HW_VERSION__11,		//T0
    HW_VERSION__12,		//EVT-1
    HW_VERSION__13,		//EVT-2
    HW_VERSION__14,		//DVT-1
    HW_VERSION__15,		//DVT-2
    HW_VERSION__16,		//PVT
    HW_VERSION__17,		//MP
    HW_VERSION_MAX,
};


enum{
    RF_VERSION__UNKNOWN,
    RF_VERSION__11,
    RF_VERSION__12,
    RF_VERSION__13,
    RF_VERSION__21,
    RF_VERSION__22,
    RF_VERSION__23,
    RF_VERSION__31,
    RF_VERSION__32,
    RF_VERSION__33,
};

#define GET_PCB_VERSION() (get_PCB_Version())
#define GET_PCB_VERSION_STRING() (get_PCB_Version_String())

#define GET_MODEM_VERSION() (get_Modem_Version())
#define GET_OPERATOR_VERSION() (get_Operator_Version())

enum OPPO_PROJECT {
    OPPO_UNKOWN = 0,
	OPPO_17061 = 17061,
    OPPO_17331 = 17331,
    OPPO_17332 = 17332,
    OPPO_17335 = 17335,
    OPPO_17197 = 17197,
    OPPO_17199 = 17199,
};

enum OPPO_OPERATOR {
    OPERATOR_UNKOWN                   = 0,
    OPERATOR_OPEN_MARKET              = 1,
    OPERATOR_CHINA_MOBILE             = 2,
    OPERATOR_CHINA_UNICOM             = 3,
    OPERATOR_CHINA_TELECOM            = 4,
    OPERATOR_FOREIGN                  = 5,
    OPERATOR_FOREIGN_WCDMA            = 6,
    OPERATOR_FOREIGN_RESERVED         = 7,
    OPERATOR_ALL_TELECOM_CARRIER      = 8,
    OPERATOR_ALL_MOBILE_CARRIER       = 9,
    OPERATOR_FOREIGN_TAIWAN           = 10,
    OPERATOR_FOREIGN_ASIA             = 11,
    OPERATOR_FOREIGN_INDIA            = 16,
    OPERATOR_FOREIGN_MALAYSIA         = 17,
    OPERATOR_FOREIGN_EU               = 18,
    OPERATOR_ALL_BAND_HIGH_CONFIG	  = 19,
    OPERATOR_17331_EVB				  = 20,
};


enum{
    SMALLBOARD_VERSION__0,
    SMALLBOARD_VERSION__1,
    SMALLBOARD_VERSION__2,
    SMALLBOARD_VERSION__3,
    SMALLBOARD_VERSION__4,
    SMALLBOARD_VERSION__5,
    SMALLBOARD_VERSION__6,
    SMALLBOARD_VERSION__UNKNOWN = 100,
};

typedef enum OPPO_PROJECT OPPO_PROJECT;

typedef struct
{
    unsigned int    nProject;
    unsigned int    nModem;
    unsigned int    nOperator;
    unsigned int    nPCBVersion;
} ProjectInfoCDTType;

//unsigned int init_project_version(void);
unsigned int get_project(void);
unsigned int is_project(OPPO_PROJECT project );
unsigned int get_PCB_Version(void);
unsigned int get_Modem_Version(void);
unsigned int get_Operator_Version(void);

#endif
