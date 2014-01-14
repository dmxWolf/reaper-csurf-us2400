
###############################################################################
##
## US2400 - Aux-Mode - Shift - Null
##
## Custom action for Shift + Null Key on the US-2400 in Aux Mode
##
## You can change the code below to whatever action you wish,
## just DON'T CHANGE THE FILENAME, the binding to the button depends on it
##
###############################################################################


## ZOOM TO TRACK SELECTION


from reaper_python import *
from sws_python import *

# RPR_ShowConsoleMsg("Aux Shift + Null: Zoom to Track Sel")

RPR_Main_OnCommand(RPR_NamedCommandLookup("_SWS_VZOOMFIT"), 0)