#include "sf33rd/Source/Game/message/en/msgtable_en.h"
#include "common.h"
#include "sf33rd/Source/Game/message/en/msgextra_en.h"
#include "sf33rd/Source/Game/message/en/msgmenu_en.h"
#include "sf33rd/Source/Game/message/en/msgsysdir_en.h"
#include "sf33rd/Source/Game/message/en/pl00end_en.h"
#include "sf33rd/Source/Game/message/en/pl00tlk_en.h"
#include "sf33rd/Source/Game/message/en/pl00win_en.h"
#include "sf33rd/Source/Game/message/en/pl01end_en.h"
#include "sf33rd/Source/Game/message/en/pl01tlk_en.h"
#include "sf33rd/Source/Game/message/en/pl01win_en.h"
#include "sf33rd/Source/Game/message/en/pl02end_en.h"
#include "sf33rd/Source/Game/message/en/pl02tlk_en.h"
#include "sf33rd/Source/Game/message/en/pl02win_en.h"
#include "sf33rd/Source/Game/message/en/pl03end_en.h"
#include "sf33rd/Source/Game/message/en/pl03tlk_en.h"
#include "sf33rd/Source/Game/message/en/pl03win_en.h"
#include "sf33rd/Source/Game/message/en/pl04end_en.h"
#include "sf33rd/Source/Game/message/en/pl04tlk_en.h"
#include "sf33rd/Source/Game/message/en/pl04win_en.h"
#include "sf33rd/Source/Game/message/en/pl05end_en.h"
#include "sf33rd/Source/Game/message/en/pl05tlk_en.h"
#include "sf33rd/Source/Game/message/en/pl05win_en.h"
#include "sf33rd/Source/Game/message/en/pl06end_en.h"
#include "sf33rd/Source/Game/message/en/pl06tlk_en.h"
#include "sf33rd/Source/Game/message/en/pl06win_en.h"
#include "sf33rd/Source/Game/message/en/pl07end_en.h"
#include "sf33rd/Source/Game/message/en/pl07tlk_en.h"
#include "sf33rd/Source/Game/message/en/pl07win_en.h"
#include "sf33rd/Source/Game/message/en/pl08end_en.h"
#include "sf33rd/Source/Game/message/en/pl08tlk_en.h"
#include "sf33rd/Source/Game/message/en/pl08win_en.h"
#include "sf33rd/Source/Game/message/en/pl09end_en.h"
#include "sf33rd/Source/Game/message/en/pl09tlk_en.h"
#include "sf33rd/Source/Game/message/en/pl09win_en.h"
#include "sf33rd/Source/Game/message/en/pl10end_en.h"
#include "sf33rd/Source/Game/message/en/pl10tlk_en.h"
#include "sf33rd/Source/Game/message/en/pl10win_en.h"
#include "sf33rd/Source/Game/message/en/pl11end_en.h"
#include "sf33rd/Source/Game/message/en/pl11tlk_en.h"
#include "sf33rd/Source/Game/message/en/pl11win_en.h"
#include "sf33rd/Source/Game/message/en/pl12end_en.h"
#include "sf33rd/Source/Game/message/en/pl12tlk_en.h"
#include "sf33rd/Source/Game/message/en/pl12win_en.h"
#include "sf33rd/Source/Game/message/en/pl13end_en.h"
#include "sf33rd/Source/Game/message/en/pl13tlk_en.h"
#include "sf33rd/Source/Game/message/en/pl13win_en.h"
#include "sf33rd/Source/Game/message/en/pl14end_en.h"
#include "sf33rd/Source/Game/message/en/pl14win_en.h"
#include "sf33rd/Source/Game/message/en/pl15end_en.h"
#include "sf33rd/Source/Game/message/en/pl15tlk_en.h"
#include "sf33rd/Source/Game/message/en/pl15win_en.h"
#include "sf33rd/Source/Game/message/en/pl16end_en.h"
#include "sf33rd/Source/Game/message/en/pl16tlk_en.h"
#include "sf33rd/Source/Game/message/en/pl16win_en.h"
#include "sf33rd/Source/Game/message/en/pl17end_en.h"
#include "sf33rd/Source/Game/message/en/pl17win_en.h"
#include "sf33rd/Source/Game/message/en/pl18end_en.h"
#include "sf33rd/Source/Game/message/en/pl18tlk_en.h"
#include "sf33rd/Source/Game/message/en/pl18win_en.h"
#include "sf33rd/Source/Game/message/en/pl19end_en.h"
#include "sf33rd/Source/Game/message/en/pl19tlk_en.h"
#include "sf33rd/Source/Game/message/en/pl19win_en.h"

MessageTable* pl_mes_tbl[20] = { &pl00win_en_tbl, &pl01win_en_tbl, &pl02win_en_tbl, &pl03win_en_tbl, &pl04win_en_tbl,
                                 &pl05win_en_tbl, &pl06win_en_tbl, &pl07win_en_tbl, &pl08win_en_tbl, &pl09win_en_tbl,
                                 &pl10win_en_tbl, &pl11win_en_tbl, &pl12win_en_tbl, &pl13win_en_tbl, &pl14win_en_tbl,
                                 &pl15win_en_tbl, &pl16win_en_tbl, &pl17win_en_tbl, &pl18win_en_tbl, &pl19win_en_tbl };

MessageTable* pl_tlk_tbl[20] = { &pl00tlk_en_tbl, &pl01tlk_en_tbl, &pl02tlk_en_tbl, &pl03tlk_en_tbl, &pl04tlk_en_tbl,
                                 &pl05tlk_en_tbl, &pl06tlk_en_tbl, &pl07tlk_en_tbl, &pl08tlk_en_tbl, &pl09tlk_en_tbl,
                                 &pl10tlk_en_tbl, &pl11tlk_en_tbl, &pl12tlk_en_tbl, &pl13tlk_en_tbl, &pl13tlk_en_tbl,
                                 &pl15tlk_en_tbl, &pl16tlk_en_tbl, &pl16tlk_en_tbl, &pl18tlk_en_tbl, &pl19tlk_en_tbl };

MessageTable* pl_end_tbl[20] = { &pl00end_en_tbl, &pl01end_en_tbl, &pl02end_en_tbl, &pl03end_en_tbl, &pl04end_en_tbl,
                                 &pl05end_en_tbl, &pl06end_en_tbl, &pl07end_en_tbl, &pl08end_en_tbl, &pl09end_en_tbl,
                                 &pl10end_en_tbl, &pl11end_en_tbl, &pl12end_en_tbl, &pl13end_en_tbl, &pl14end_en_tbl,
                                 &pl15end_en_tbl, &pl16end_en_tbl, &pl17end_en_tbl, &pl18end_en_tbl, &pl19end_en_tbl };

MessageTable* msgSysDirTbl[] = { &msgSysDirTbl_en };

MessageTable* msgExtraTbl[] = { &msgExtraTbl_en };

MessageTable* msgMenuTbl[] = { &msgMenuTbl_en };
