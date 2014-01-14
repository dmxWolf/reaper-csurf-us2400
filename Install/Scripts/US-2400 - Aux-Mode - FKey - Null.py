
###############################################################################
##
## US2400 - Aux-Mode - FKey - Null
##
## Custom action for FKey + Null Key on the US-2400 in Aux Mode
##
## You can change the code below to whatever action you wish,
## just DON'T CHANGE THE FILENAME, the binding to the button depends on it
##
###############################################################################


## SCROLL TO PLAY POS


from reaper_python import *
from sws_python import *

# RPR_ShowConsoleMsg("Aux FKey + Null: Scroll to play pos")

RPR_Main_OnCommand(RPR_NamedCommandLookup("_SWS_HSCROLLPLAY50"), 0)

