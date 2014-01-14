
###############################################################################
##
## US2400 - Aux-Mode - Fkey - FFwd
##
## Custom action for FKey + FFwd Key on the US-2400 in Aux Mode
##
## You can change the code below to whatever action you wish,
## just DON'T CHANGE THE FILENAME, the binding to the button depends on it
##
###############################################################################


## REDO


from reaper_python import *
from sws_python import *

# RPR_ShowConsoleMsg("Aux FKey + FFwd: Redo")

cmd_id = 40030 # Redo
RPR_Main_OnCommand(cmd_id, 0)

