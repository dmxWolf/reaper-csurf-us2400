/*
** reaper_csurf
** Tascam US-2400 support
** Cobbled together by David Lichtenberger
** No license, no guarantees.
*/



////// PREFERENCES //////


// Encoder resolution for volume, range: 0 > 16256
#define ENCRESVOL 100
// Encoder resolution for pan, range:  -1 > +1
#define ENCRESPAN 0.01 
// Encoder resolution for fx param, in divisions of range (e.g. -1 > +1, reso 100 > stepsize 0.02)
#define ENCRESFX 200
// Wheel resolution for fast scrub
#define SCRUBRESFAST 1
// Wheel resolution for slow scrub
#define SCRUBRESSLOW 5
// Wheel resolution for edit cursor move, fast
#define MOVERESFAST 10
// Wheel resolution for edit cursor move, slow
#define MOVERESSLOW 50
// Stick interval (stick data gets only updated every x cycles)
#define STICKINTV 5
// Stick - size of dead zone in the middle (128 = totally dead)
#define STICKDEAD 90
// Blink interval / ratio (1 = appr. 30 Hz / 0.03 s)
#define MYBLINKINTV 20
#define MYBLINKRATIO 1
// For finding sends (also in custom reascript actions, see MyCSurf_Aux_Send)
#define AUXSTRING "aux---%d"
// Amount of Faders and Encoders that get updated per cycle (execution of function run)
#define UPDCYCLE 3



////// DEBUG //////

// debug macros
#define DBGS(x) ShowConsoleMsg(x);
#define DBGF(x) sprintf(debug, "%f   ", x); ShowConsoleMsg(debug);
#define DBGD(x) sprintf(debug, "%d   ", x); ShowConsoleMsg(debug);
#define DBGX(x) sprintf(debug, "%x   ", x); ShowConsoleMsg(debug);
#define DBGB(x) if(x) ShowConsoleMsg("true   "); else ShowConsoleMsg("false   ");
#define DBGN ShowConsoleMsg("\n");



// Command Lookup
#define CMD(x) NamedCommandLookup(x)

// Unnamed Commands
#define CMD_SELALLTKS 40296
#define CMD_UNSELALLTKS 40297
#define CMD_SELALLITEMS 40182
#define CMD_UNSELALLITEMS 40289
#define CMD_PREVTK 40286
#define CMD_NEXTTK 40285
#define CMD_INSERTTK 40001
#define CMD_DUPLITK 40062
#define CMD_RENAMETK 40696
#define CMD_MCP_HIDECHILDR 40199
#define CMD_GROUPSETTINGS 40772
#define CMD_RJUMP 40172     
#define CMD_FJUMP 40173
#define CMD_TIMESEL2ITEMS 40290
#define CMD_CLEARTIMESEL 40635
#define CMD_TGLPLAYSCROLL 40036
#define CMD_TGLRECSCROLL 40262
#define CMD_TGGLRECBEAT 40045
#define CMD_AUTOTOSEL 41160
#define CMD_FXBROWSER 40271
#define CMD_UNDO 40029
#define CMD_REDO 40030
#define CMD_SAVE 40026
#define CMD_SAVEAS 40022
#define CMD_SEL2LASTTOUCH 40914



#include "csurf.h"



// for debug  
char debug[64];

class CSurf_US2400;
static bool g_csurf_mcpmode = true; 

class CSurf_US2400 : public IReaperControlSurface
{
  ////// GLOBAL VARS //////

  int m_midi_in_dev,m_midi_out_dev;
  int m_offset, m_size;
  midi_Output *m_midiout;
  midi_Input *m_midiin;
  int m_cfg_flags;  // config_flag_fader_touch_mode etc

  WDL_String descspace;
  char configtmp[1024];

  // buffer for fader data
  bool waitformsb;

  // for joystick
  unsigned char last_joy_x, last_joy_y, lsb;
  char stick_ctr;

  // for updates
  char curr_update;

  // for myblink
  bool s_myblink;  
  int myblink_ctr;

  // for init
  bool s_initdone;
  bool s_exitdone;

  // touched faders 
  unsigned long s_touchstates;
    
  // general states
  int s_ch_offset; // bank up/down
  bool s_play, s_rec, s_loop; // play states
  char s_automode_alltks; // automation modes

  // modes
  bool m_flip, m_chan, m_pan, m_scrub;
  char m_aux;

  // qualifier keys
  bool q_fkey, q_shift;

  // for channel strip
  MediaTrack* chan_rpr_tk;
  char chan_ch;
  int chan_fx;
  int chan_par_offs;

  // save track sel
  MediaTrack** saved_sel;
  int saved_sel_len;

  // loop all
  bool s_loop_all;
  double s_ts_start;
  double s_ts_end;



  //////// MIDI ////////

  void MIDIin(MIDI_event_t *evt)
  {
    unsigned char ch_id;
    
    bool btn_state = false;
    if (evt->midi_message[2] == 0x7f) btn_state = true;

    // msb of fader move?

    if ( (waitformsb) && (evt->midi_message[0] == 0xb0) && (evt->midi_message[1] < 0x19) ) {

      ch_id = evt->midi_message[1];
      int value = (evt->midi_message[2] << 7) | lsb;

      OnFaderChange(ch_id, value);

      waitformsb = false;

    } else {

      // buttons / track elements
      if (evt->midi_message[0] == 0xb1)
      {
        // master buttons
        switch (evt->midi_message[1])
        {
          case 0x61 : if (btn_state) OnMasterSel(); break;
          case 0x62 : OnClrSolo(btn_state); break;
          case 0x63 : if (btn_state) OnFlip(); break;
          case 0x64 : if (btn_state) OnChan(); break;
          case 0x6c : if (btn_state) OnPan(); break;
          //case 0x6b : if (btn_state) OnMeter(); break; // maybe later (see below)
          case 0x6d : OnFKey(btn_state); break;
          case 0x65 : if (btn_state) OnAux(1); break;
          case 0x66 : if (btn_state) OnAux(2); break;
          case 0x67 : if (btn_state) OnAux(3); break;
          case 0x68 : if (btn_state) OnAux(4); break;
          case 0x69 : if (btn_state) OnAux(5); break;
          case 0x6a : if (btn_state) OnAux(6); break;
          case 0x6e : OnNull(btn_state); break;
          case 0x6f : if (btn_state) OnScrub(); break;
          case 0x70 : OnBank(-1, btn_state); break;
          case 0x71 : OnBank(1, btn_state); break;
          case 0x72 : OnIn(btn_state); break;
          case 0x73 : OnOut(btn_state); break;
          case 0x74 : OnShift(btn_state); break;
          case 0x75 : if (btn_state) OnRew(); break;
          case 0x76 : if (btn_state) OnFwd(); break;
          case 0x77 : if (btn_state) OnStop(); break;
          case 0x78 : if (btn_state) OnPlay(); break;
          case 0x79 : if (btn_state) OnRec(); break;
          default :
          {
            // track elements
            if (evt->midi_message[1] < 0x60)
            {

              ch_id = evt->midi_message[1] / 4; // floor

              char ch_element;
              ch_element = evt->midi_message[1] % 4;    // modulo

              switch (ch_element)
              {
                case 0 : OnFaderTouch(ch_id, btn_state); break;
                case 1 : if (btn_state) OnTrackSel(ch_id); break;
                case 2 : if (btn_state) OnTrackSolo(ch_id); break;
                case 3 : if (btn_state) OnTrackMute(ch_id); break;
              } // switch (ch_element)
            } // if (evt->midi_message[1] < 0x60)
          } // default
        } // switch (evt->midi_message[1])

        // encoders, fader values, jog wheel
      } else if (evt->midi_message[0] == 0xb0) 
      {
        // calculate relative value for encoders and jog wheel
        signed char rel_value;
        if (evt->midi_message[2] < 0x40) rel_value = evt->midi_message[2];
        else rel_value = 0x40 - evt->midi_message[2];

        // fader (track and master): catch lsb - msb see above
        if ( (evt->midi_message[1] >= 0x20) && (evt->midi_message[1] <= 0x38) )
        {
          lsb = evt->midi_message[3];
          waitformsb = true;

          // jog wheel
        } else if (evt->midi_message[1] == 0x3C) 
        {

          OnJogWheel(rel_value);

          // encoders
        } else if ( (evt->midi_message[1] >= 0x40) && (evt->midi_message[1] <= 0x58) ) 
        {
          ch_id = evt->midi_message[1] - 0x40;

          OnEncoderChange(ch_id, rel_value);
        }

        // touch master fader
      } else if (evt->midi_message[0] == 0xb2) 
      {
        OnFaderTouch(24, btn_state);

        // joystick
      } else if (evt->midi_message[0] == 0xbe) 
      {
        // x or y
        switch (evt->midi_message[1])
        {
          case 0x5a : last_joy_x = evt->midi_message[2]; break;
          case 0x5b : last_joy_y = evt->midi_message[2]; break; 
        } // switch (evt->midi_message[1])
  
        /* don't shoot events, query in regular intervals
        // OnJoystick(last_joy_x, last_joy_y); */

      } // (evt->midi_message[0] == 0xb1), else, else ...
    } // if ( (waitformsb) && (evt->midi_message[0] == 0xb0) && (evt->midi_message[1] < 0x19), else
  } // MIDIin()


  void MIDIOut(unsigned char s, unsigned char d1, unsigned char d2) 
  {
    if (m_midiout) m_midiout->Send(s, d1, d2, 0);
  } // void MIDIOut(char s, char d1, char d2)



  //////// EVENTS (called by MIDIin) //////


  // TRACK ELEMENTS

  void OnTrackSel(char ch_id)
  {
    if (m_chan) MySetSurface_Chan_SelectTrack(ch_id);
    else {
      MediaTrack* rpr_tk = Cnv_ChannelIDToMediaTrack(ch_id);

      if (q_fkey) CSurf_OnRecArmChange(rpr_tk, -1);
      else if (q_shift) MyCSurf_SwitchPhase(rpr_tk);
      else CSurf_OnSelectedChange(rpr_tk, -1);
    }
  } // OnTrackSel


  void OnTrackSolo(char ch_id)
  {
    MediaTrack* rpr_tk = Cnv_ChannelIDToMediaTrack(ch_id);    

    if (q_shift) MyCSurf_ToggleSolo(rpr_tk, true);
    else MyCSurf_ToggleSolo(rpr_tk, false);
  } // OnTrackSolo


  void OnTrackMute(char ch_id)
  {
    MediaTrack* rpr_tk = Cnv_ChannelIDToMediaTrack(ch_id);    

    if (q_shift) MyCSurf_ToggleMute(rpr_tk, true);
    else MyCSurf_ToggleMute(rpr_tk, false);
  } // OnTrackMute


  void OnFaderTouch(char ch_id, bool btn_state)
  {
    if (btn_state) 
    {
      s_touchstates = s_touchstates | (1 << ch_id);
    } else
    {
      s_touchstates = s_touchstates ^ (1 << ch_id);

      /* Stuff below is not needed as when we're updating faders/encoders in the loop */

      /*

      // update once again on untouch (important for resetting faders) - but not empty tracks!
      MediaTrack* istrack = Cnv_ChannelIDToMediaTrack(ch_id);
      if (istrack != NULL) MySetSurface_UpdateFader(ch_id);

      */
    }
  } // OnFaderTouch


  void OnFaderChange(char ch_id, int value)
  {
    MediaTrack* rpr_tk = Cnv_ChannelIDToMediaTrack(ch_id);
    int para_amount;

    double d_value;

    bool istrack = false;
    bool ismaster = false;
    bool isactive = true;

    // is track or master?
    if ( (ch_id >= 0) && (ch_id <= 23) ) istrack = true; // no track fader
    else if (rpr_tk == CSurf_TrackFromID(0, false)) ismaster = true;

    // active fader? 
    if (!rpr_tk) isactive = false; // no corresponding rpr_tk
    
    if ( (m_chan) && (m_flip) ) 
    { // only chan and flip: inside para_amount?
   
      para_amount = TrackFX_GetNumParams(chan_rpr_tk, chan_fx);
      if (chan_par_offs + ch_id >= para_amount) isactive = false;
      else isactive = true; // is track doesn't matter when chan and flipped
    
    } else if ( (m_aux > 0) && (m_flip) ) 
    { // only aux #x and flip: has send #x?
      int send_id = Cnv_AuxIDToSendID(ch_id, m_aux);
      if (send_id == -1) isactive = false;
    }
  

    // get values
    if (ismaster || istrack)
    {
    
      if (ismaster)
      { // if master -> volume

        if (q_fkey) d_value = 0.0; // MINIMUM
        else if (q_shift) d_value = 1.0; // DEFAULT
        else d_value = Cnv_FaderToVol(value); 
        CSurf_OnVolumeChange(rpr_tk, d_value, false);
      
      } else if (isactive)
      { // if is active track fader

        if (m_flip)
        {
          if (m_chan) 
          { // flip & chan -> fx param

            double min, max;
            d_value = TrackFX_GetParam(chan_rpr_tk, chan_fx, chan_par_offs + ch_id, &min, &max);

            if (q_fkey) d_value = min; // MINIMUM
            else if (q_shift) d_value = max; // MAXIMUM
            else d_value = Cnv_FaderToFXParam(min, max, value);
            MyCSurf_Chan_SetFXParam(chan_rpr_tk, chan_fx, chan_par_offs + ch_id, d_value);

          } else if (m_aux > 0) 
          { // flip + aux -> send Vol

            int send_id = Cnv_AuxIDToSendID(ch_id, m_aux);
            if (send_id != -1)
            {

              if (q_fkey) d_value = 0.0; // MINIMUM
              else if (q_shift) d_value = 1.0; // DEFAULT
              else d_value = Cnv_FaderToVol(value); 

              CSurf_OnSendVolumeChange(rpr_tk, send_id, d_value, false);
            }
          } else
          { // pan

            if (q_fkey) 
            { // flip & fkey & pan -> width
            
              if (q_shift) d_value = 1.0; // default
              else d_value = Cnv_FaderToPanWidth(value);
              CSurf_OnWidthChange(rpr_tk, d_value, false);

            } else 
            { // flip & pan mode -> pan
            
              if (q_shift) d_value = 0.0; // default
              else d_value = Cnv_FaderToPanWidth(value);
              CSurf_OnPanChange(rpr_tk, d_value, false);
            }         
          } // if (m_chan), else

        } else
        { // no flip -> volume

          // no flip / master -> track volume
          if (q_fkey) d_value = 0.0; // MINIMUM
          else if (q_shift) d_value = 1.0; // DEFAULT
          else d_value = Cnv_FaderToVol(value); 
          CSurf_OnVolumeChange(rpr_tk, d_value, false);

        } // if (m_flip), else
      } // if (ismaster), else if (isactive)
    } // if (exists)
  } // OnFaderChange()


  void OnEncoderChange(char ch_id, signed char rel_value)
  {
    MediaTrack* rpr_tk = Cnv_ChannelIDToMediaTrack(ch_id);
    int para_amount;

    double d_value;
    bool dot = false; // for phase switch, rec arm

    bool exists = false;
    bool isactive = true;

    // encoder exists?
    if ( (ch_id >= 0) && (ch_id <= 23) ) exists = true;

    // active encoder? 
    if (!rpr_tk) isactive = false; // no track

    if ( (m_chan) && (!m_flip) )
    { 
      para_amount = TrackFX_GetNumParams(chan_rpr_tk, chan_fx);
      if (chan_par_offs + ch_id >= para_amount) isactive = false;
      else isactive = true; // chan + fip: is track doesn't matter

    } else if (m_aux > 0) 
    { // only aux #x: has send #x?
      int send_id = Cnv_AuxIDToSendID(ch_id, m_aux);
      if (send_id == -1) isactive = false;
    }

    if ( (exists) && (isactive) )
    { // is encoder and there is a track, fx parameter (chan, no flip), or send (aux, noflip)
      if (m_flip)
      {
        if (m_aux > 0)
        { // aux & flip -> send pan
          int send_id = Cnv_AuxIDToSendID(ch_id, m_aux);
          if (send_id != -1)
          {
            if (q_shift) d_value = 0.0; // DEFAULT
            else
            {
              GetTrackSendUIVolPan(rpr_tk, send_id, NULL, &d_value);
              d_value = Cnv_EncoderToPanWidth(d_value, rel_value);
            }
            CSurf_OnSendPanChange(rpr_tk, send_id, d_value, false);
          }
        
        } else
        { // just flip -> track volume
          
          if (q_fkey) d_value = 0.0; // MINIMUM
          else if (q_shift) d_value = 1.0; // DEFAULT
          else 
          { 
            GetTrackUIVolPan(rpr_tk, &d_value, NULL);
            d_value = Cnv_EncoderToVol(d_value, rel_value); 
          }
          CSurf_OnVolumeChange(rpr_tk, d_value, false);
        }
      } else
      {
        if (m_chan)
        { // chan -> fx_param (para_offset checked above)

          double min, max;
          d_value = TrackFX_GetParam(chan_rpr_tk, chan_fx, chan_par_offs + ch_id, &min, &max);

          if (q_fkey) d_value = min; // MINIMUM
          else if (q_shift) d_value = max; // MAXIMUM
          else d_value = Cnv_EncoderToFXParam(d_value, min, max, rel_value);

          MyCSurf_Chan_SetFXParam(chan_rpr_tk, chan_fx, chan_par_offs + ch_id, d_value);
       
        } else if (m_aux > 0)
        {
          int send_id = Cnv_AuxIDToSendID(ch_id, m_aux);
          if (send_id != -1)
          {
            // aux & no flip -> send-level
            if (q_fkey) d_value = 0.0; // MINIMUM 
            else if (q_shift) d_value = 1.0; // DEFAULT 
            else
            {
              
              GetTrackSendUIVolPan(rpr_tk, send_id, &d_value, NULL);
              d_value = Cnv_EncoderToVol(d_value, rel_value);
            }
            CSurf_OnSendVolumeChange(rpr_tk, send_id, d_value, false);
          }
        
        } else if (m_pan)
        {
          if (q_fkey) 
          { // pan + fkey -> width
            
            
            if (q_shift) d_value = 1.0; // default
            else 
            {
              d_value = GetMediaTrackInfo_Value(rpr_tk, "D_WIDTH");
              d_value = Cnv_EncoderToPanWidth(d_value, rel_value);
            }
            CSurf_OnWidthChange(rpr_tk, d_value, false);
          
          } else
          { // pan mode -> pan

            if (q_shift) d_value = 0.0; // default
            else 
            {
              GetTrackUIVolPan(rpr_tk, NULL, &d_value);
              d_value = Cnv_EncoderToPanWidth(d_value, rel_value);
            }
            CSurf_OnPanChange(rpr_tk, d_value, false);
          }
        }
      } // if (m_flip), else

      MySetSurface_UpdateEncoder(ch_id); // because touched track doesn't get updated

    } // if ( (exists) && (isactive) )
  } // OnEncoderChange


  // MASTER TRACK

  void OnMasterSel()
  {
    if (m_chan) 
    {
      MySetSurface_Chan_SelectTrack(24);
    } else
    {
      if (q_fkey) MyCSurf_ToggleSelectAllTracks();
      else MyCSurf_SelectMaster();
    }
  } // OnMasterSel()


  void OnClrSolo(bool btn_state)
  {
    if (btn_state)
      if (q_fkey) MyCSurf_UnmuteAllTracks();
      else MyCSurf_UnsoloAllTracks();

    MySetSurface_UpdateButton(0x62, btn_state, false);
  } // OnClrSolo()


  void OnFlip()
  {
    MySetSurface_ToggleFlip();
  } // OnFlip()


  // MODE BUTTONS

  void OnChan()
  {
    if (m_chan) MySetSurface_ExitChanMode();
    else MySetSurface_EnterChanMode();
  } // OnChan()


  void OnPan()
  {
    MySetSurface_EnterPanMode();
  } // OnPan()


  void OnAux(char sel)
  { 
    if (q_fkey)
    { 
      if (m_aux > 0) MyCSurf_Aux_Send(sel, true);
      else
      {
        switch(sel)
        {
          case 1 : MyCSurf_Tks_MoveSelected(-1); break;
          case 2 : MyCSurf_Tks_MoveSelected(1); break;
          case 3 : MyCSurf_Tks_DuplicateSelected(); break; 
          case 4 : MyCSurf_Tks_RenameSelected(); break;
          case 5 : MyCSurf_EmulateKeyStroke(0x1b); break; // Escape
          case 6 : MyCSurf_EmulateKeyStroke(0x0d); break; // Enter
        }
      }
    } else if (q_shift)
    {
      if (m_aux > 0) MyCSurf_Aux_Send(sel, false);
      else
      {
        switch(sel)
        {
          case 1 : MyCSurf_Tks_WrapUnwrapSelected(); break; 
          case 2 : MyCSurf_Tks_ToggleShowFolder(); break;
          case 3 : MyCSurf_Tks_GroupSelected(true); break;
          case 4 : MyCSurf_Tks_Insert(); break;
          case 5 : MyCSurf_Tks_DeleteSelected(); break; 
          case 6 : MyCSurf_Tks_GroupSelected(false); break;
        }
      }
    } else
    {
      if (m_chan)
      {
        switch(sel)
        {
          case 1 : MySetSurface_Chan_SetFxParamOffset(1); break;
          case 2 : MySetSurface_Chan_SetFxParamOffset(-1); break;
          case 3 : MyCSurf_Chan_ToggleFXBypass(); break;
          case 4 : MyCSurf_Chan_InsertFX(); break;
          case 5 : MyCSurf_Chan_DeleteFX(); break;
          case 6 : MyCSurf_Chan_ToggleAllFXBypass(); break;

        } 
      } else MySetSurface_EnterAuxMode(sel);
    }
    MySetSurface_UpdateAuxButtons();
  } // OnAux()


  // METER MODE
  
  /* Maybe later. I don't think it's that important (and I don't know how to do it) */


  // QUALIFIERS

  void OnFKey(bool btn_state)
  {
    MySetSurface_ToggleFKey(btn_state);
  } // OnFKey()

  void OnShift(bool btn_state)
  {
    MySetSurface_ToggleShift(btn_state);
  } // OnShift()


  // TRANSPORT

  void OnRew()
  {
    if (q_fkey) MyCSurf_Undo();
    else if (q_shift) MyCSurf_Auto_SetMode(0);
    else MyCSurf_OnRew();
  } // OnRew()


  void OnFwd()
  {
    if (q_fkey) MyCSurf_Redo();
    else if (q_shift) MyCSurf_Auto_SetMode(1);
    else MyCSurf_OnFwd();
  } // OnFwd()


  void OnStop()
  {
    if (q_fkey) MyCSurf_ToggleScrollOnPlay(); 
    else if (q_shift) MyCSurf_Auto_SetMode(2); 
    else CSurf_OnStop();   
  } // OnStop()


  void OnPlay()
  {
    if (q_fkey) MyCSurf_Save(false);
    else if (q_shift) MyCSurf_Auto_SetMode(3);
    else CSurf_OnPlay();
  } // OnPlay()


  void OnRec()
  {
    if (q_fkey) MyCSurf_Save(true);
    else if (q_shift) MyCSurf_Auto_WriteCurrValues();
    else MyCSurf_OnRec();
  } // OnRec()


  // OTHER KEYS

  void OnNull(bool btn_state)
  {
    if (btn_state) 
      if (q_fkey) MyCSurf_CenterHScrollToPlay();
      else if (q_shift) MyCSurf_VZoomToSelTracks();
      else MyCSurf_ToggleHZoomToTimeSel();

    MySetSurface_UpdateButton(0x6e, btn_state, false);
  } // OnNull()


  void OnScrub()
  {
    MySetSurface_ToggleScrub();
  } // OnScrub()


  void OnBank(signed char dir, bool btn_state)
  {
    char btn_id;
    if (dir > 0) btn_id = 0x71;
    else btn_id = 0x70;
    
    if (btn_state)
    {
      if (m_chan)
      {
        if (q_fkey) MyCSurf_Chan_MoveFX(dir);
        else if (q_shift)
        {
          if (dir < 0) MyCSurf_Chan_CloseFX(chan_fx);
          else MyCSurf_Chan_OpenFX(chan_fx);

        } else
        {
          MyCSurf_Chan_SelectFX(chan_fx + dir);
        }
      } else
      {
        if (q_fkey) MyCSurf_MoveTimeSel(dir, 0, false);
        else
        {
          char factor = 8;
          if (q_shift) factor = 24;
          MySetSurface_ShiftBanks(dir, factor);
        }
      }
      MySetSurface_UpdateButton(btn_id, true, false);
    } else
    {
      if (m_chan) MySetSurface_UpdateButton(btn_id, true, true);
      else MySetSurface_UpdateButton(btn_id, false, false);
    }
  }


  void OnIn(bool btn_state)
  {
    if (btn_state)
    {
      if (q_fkey) MyCSurf_MoveTimeSel(0, -1, false);
      else if (q_shift) MyCSurf_Loop_ToggleAll();
      else MyCSurf_MoveTimeSel(-1, -1, true);
    }

    // keep lit if loop activated
    if (s_loop) btn_state = true;
    MySetSurface_UpdateButton(0x72, btn_state, false);
  } // OnIn()


  void OnOut(bool btn_state)
  {
    if (btn_state)
    {
      if (q_fkey) MyCSurf_MoveTimeSel(0, 1, false);
      else if (q_shift) MyCSurf_ToggleRepeat();
      else MyCSurf_MoveTimeSel(1, 1, true);
    }

    // keep lit if loop activated
    if (s_loop) btn_state = true;
    MySetSurface_UpdateButton(0x73, btn_state, false);
  } // OnOut()


  // SPECIAL INPUT  

  void OnJogWheel(signed char rel_value)
  {
    if (m_scrub)
    {
      if (q_fkey) MyCSurf_Scrub(rel_value, true);
      else MyCSurf_Scrub(rel_value, false);
    
    } else
    {
      if (q_fkey) MyCSurf_MoveEditCursor(rel_value, true);
      else MyCSurf_MoveEditCursor(rel_value, false);
    }
  } // OnJogWheel()


  void OnJoystick()
  { 
    if ( (q_fkey) || (q_shift) )
    {
      // zoom or scroll
      bool zoom = false;
      if (q_shift) zoom = true;

      // process y data
      char dir = -1;
      if (last_joy_y < 64 - STICKDEAD / 2) dir = 1;
      else if (last_joy_y > 64 + STICKDEAD / 2) dir = 0;
      
      if (dir != -1) CSurf_OnArrow(dir, zoom);

      // process x data
      dir = -1;
      if (last_joy_x < 64 - STICKDEAD / 2) dir = 2;
      else if (last_joy_x > 64 + STICKDEAD / 2) dir = 3;

      if (dir != -1) CSurf_OnArrow(dir, zoom);
    }
  } // OnJoystick



  ////// CONVERSION & HELPERS //////


  // TRACKS / CHANNELS / SENDS

  int Cnv_MediaTrackToChannelID(MediaTrack* rpr_tk)
  {
    int ch_id = CSurf_TrackToID(rpr_tk, g_csurf_mcpmode);

    ch_id -= s_ch_offset;
    if (ch_id == 0) ch_id = 24;
    else ch_id = ch_id - 1;

    return ch_id;
  } // Cnv_MediaTrackToChannelID


  MediaTrack* Cnv_ChannelIDToMediaTrack(unsigned char ch_id) 
  {
    if (ch_id == 24) 
    {
      ch_id = 0; // master = 0
    } else 
    { 
      ch_id += s_ch_offset + 1;
    }
    
    MediaTrack* rpr_tk = CSurf_TrackFromID(ch_id, g_csurf_mcpmode);

    return rpr_tk;
  } // Cnv_ChannelIDToMediaTrack

  int Cnv_AuxIDToSendID(int ch_id, char aux)
  {
    char search[256];
    char sendname[256];
    sprintf(search, AUXSTRING, aux);
    
    MediaTrack* rpr_tk = Cnv_ChannelIDToMediaTrack(ch_id);
    int all_sends = GetTrackNumSends(rpr_tk, 0);

    for (int s = 0; s < all_sends; s++)
    {
      GetTrackSendName(rpr_tk, s, sendname, 256);
      if (strstr(sendname, search)) return s;
    }
    
    return -1;
  } // Cnv_AuxIDToSendID

  // VOLUME / SEND LEVELS

  double Cnv_FaderToVol(unsigned int value) 
  {
    //double new_val = ((double) (value + 41) * 1000.0) / 16297.0;
    double new_val = ((double) value * 1000.0) / 16383.0;

    if (new_val < 0.0) new_val = 0.0;
    else if (new_val > 1000.0) new_val = 1000.0;

    new_val = DB2VAL(SLIDER2DB(new_val));

    return(new_val);
  } // Cnv_FaderToVol


  double Cnv_EncoderToVol(double old_val, signed char rel_val) 
  {
    double lin = (DB2SLIDER( VAL2DB(old_val) ) * 16383.0 / 1000.0);
    double d_rel = (double)rel_val * (double)ENCRESVOL;
    lin = lin + d_rel;

    double new_val = ((double) lin * 1000.0) / 16383.0;

    if (new_val < 0.0) new_val = 0.0;
    else if (new_val > 1000.0) new_val = 1000.0;

    new_val = DB2VAL(SLIDER2DB(new_val));

    return new_val;
  } // Cnv_EncoderToVol


  int Cnv_VolToFader(double value) 
  {
    //double new_val = DB2SLIDER( VAL2DB(value) ) * 16297.0 / 1000.0 - 41.0;
    double new_val = DB2SLIDER( VAL2DB(value) ) * 16383.0 / 1000.0;
    int out;

    if (new_val < 0.0) new_val = 0.0;
    else if (new_val > 16383.0) new_val = 16383.0;

    out = (int)(new_val + 0.5);

    return out;
  } // Cnv_VolToFader


  unsigned char Cnv_VolToEncoder(double value) 
  {
    double new_val = (DB2SLIDER( VAL2DB(value) ) * 14.0 / 1000.0) + 1;

    if (new_val < 1.0) new_val = 1.0;
    else if (new_val > 15.0) new_val = 15.0;

    return (char)(new_val + 0.5);
  } // Cnv_VolToEncoder


  // PAN / WIDTH

  double Cnv_FaderToPanWidth(unsigned int value) 
  {
    double new_val = -1.0 + ((double)value / 16383.0 * 2.0);

    if (new_val < -1.0) new_val = -1.0;
    else if (new_val > 1.0) new_val = 1.0;

    return new_val;
  } // Cnv_FaderToPanWidth


  double Cnv_EncoderToPanWidth(double old_val, signed char rel_val) 
  { 
    double new_val = old_val + (double)rel_val * (double)ENCRESPAN;

    if (new_val < -1.0) new_val = -1.0;
    else if (new_val > 1.0) new_val = 1.0;
    return new_val;
  } // Cnv_EncoderToPanWidth


  int Cnv_PanWidthToFader(double value) 
  {
    double new_val = (1.0 + value) * 16383.0 * 0.5;

    if (new_val < 0.0) new_val = 0.0;
    else if ( new_val > 16383.0) new_val = 16383.0;

    return (int)(new_val + 0.5);
  } // Cnv_PanWidthToFader


  char Cnv_PanToEncoder(double value) 
  {
    double new_val = ((1.0 + value) * 14.0 * 0.5) + 1;

    if (new_val < 1.0) new_val = 1.0;
    else if ( new_val > 15.0) new_val = 15.0;

    return (char)(new_val + 0.5);
  } // Cnv_PanToEncoder


  char Cnv_WidthToEncoder(double value) 
  {
    double new_val = abs(value) * 7.0 + 1;
    if (new_val < 1.0) new_val = 1.0;
    else if ( new_val > 15.0) new_val = 15.0;

    return (char)(new_val + 0.5);
  } // Cnv_WidthToEncoder


  // FX PARAM

  double Cnv_FaderToFXParam(double min, double max, unsigned int value)
  {
    double new_val = min + ((double)value / 16256.0 * (max - min));

    if (new_val < min) new_val = min;
    else if (new_val > max) new_val = max;

    return new_val;
  } // Cnv_FaderToFXParam


  double Cnv_EncoderToFXParam(double old_val, double min, double max, signed char rel_val)  
  {
    double d_rel = (double)rel_val * (max - min) / (double)ENCRESFX;
    double new_val = old_val + d_rel;

    if (new_val < min) new_val = min;
    else if (new_val > max) new_val = max;

    return new_val;
  } // Cnv_EncoderToFXParam


  int Cnv_FXParamToFader(double min, double max, double value)
  {
    double new_val = (value - min) / (max - min) * 16383.0;

    if (new_val < 0.0) new_val = 0.0;
    else if (new_val > 16383.0) new_val = 16383.0;

    return (int)(new_val + 0.5);
  } // Cnv_FXParamToFader


  char Cnv_FXParamToEncoder(double min, double max, double value)
  { 
    double new_val = (value - min) / (max - min) * 14.0 + 1;

    if (new_val < 1.0) new_val = 1.0;
    else if (new_val > 15.0) new_val = 15.0;

    return (char)(new_val + 0.5);
  } // Cnv_FXParamToEncoder


  void Hlp_SaveSelection()
  {
    if (saved_sel == 0)
    {
    
      saved_sel_len = CountSelectedTracks(0);
      saved_sel = new MediaTrack*[saved_sel_len];

      ReaProject* rpr_pro = EnumProjects(-1, NULL, 0);

      for(int sel_tk = 0; sel_tk < saved_sel_len; sel_tk++)
        saved_sel[sel_tk] = GetSelectedTrack(rpr_pro, sel_tk);

      Main_OnCommand(CMD_UNSELALLTKS, 0);
    }
  } // Hlp_SaveSelection


  void Hlp_RestoreSelection()
  {
    if (saved_sel != 0)
    {
      Main_OnCommand(CMD_UNSELALLTKS, 0);

      for(int sel_tk = 0; sel_tk < saved_sel_len; sel_tk++)
        SetTrackSelected(saved_sel[sel_tk], true);

      delete saved_sel;
      saved_sel = 0;
    }
  } // Hlp_RestoreSelection



public:



  ////// CONSTRUCTOR / DESTRUCTOR //////

  CSurf_US2400(int indev, int outdev, int *errStats)
  {
    ////// GLOBAL VARS //////

    m_midi_in_dev = indev;
    m_midi_out_dev = outdev;

    m_offset = 0;
    m_size = 0;

    // for fader data
    waitformsb = false;

    // for joystick;
    last_joy_x = 0x3f;
    last_joy_y = 0x3f;
    stick_ctr = 0;
    
    // for updates
    curr_update = 0;

    // for myblink
    s_myblink = false;
    myblink_ctr = 0;

    // for init
    s_initdone = false;
    s_exitdone = false;
  
    // touched faders 
    s_touchstates = 0;
      
    // general states
    s_ch_offset = 0; // bank up/down
    s_play = false; // playstates
    s_rec = false;
    s_loop = false;
    s_automode_alltks = 0; // automationmodes

    // modes
    m_flip = false;
    m_chan = false;
    m_pan = true;
    m_aux = 0;
    m_scrub = false;

    // qualifier keys
    q_fkey = false;
    q_shift = false;

    // for channel strip
    chan_ch = 0;
    chan_fx = 0;
    chan_par_offs = 0;

    // save selection
    saved_sel = 0;
    saved_sel_len = 0;

    // loop all
    s_loop_all = false;

    // create midi hardware access
    m_midiin = m_midi_in_dev >= 0 ? CreateMIDIInput(m_midi_in_dev) : NULL;
    m_midiout = m_midi_out_dev >= 0 ? CreateThreadedMIDIOutput( CreateMIDIOutput(m_midi_out_dev, false, NULL) ) : NULL;

    if (errStats)
    {
      if (m_midi_in_dev >=0  && !m_midiin) *errStats|=1;
      if (m_midi_out_dev >=0  && !m_midiout) *errStats|=2;
    }

    if (m_midiin) m_midiin->start();
  } // CSurf_US2400()


  ~CSurf_US2400()
  {
    s_exitdone = MySetSurface_Exit();
    do
    { Sleep(500);  
    } while (!s_exitdone);
    
    delete saved_sel;

    delete m_midiout;
    delete m_midiin;
  } // ~CSurf_US2400()



  ////// SURFACE UPDATES //////

  bool MySetSurface_Init() 
  {
    CSurf_ResetAllCachedVolPanStates(); 
    TrackList_UpdateAllExternalSurfaces(); 

    // Initially Pan Mode
    MySetSurface_EnterPanMode();

    // Initially Scrub off
    m_scrub = true;
    MySetSurface_ToggleScrub();
    
    // update BankLEDs
    MySetSurface_UpdateBankLEDs();

    // update loop
    s_loop = !(bool)GetSetRepeat(-1);
    MyCSurf_ToggleRepeat();
    SetRepeatState(s_loop);

    // Update Auto modes, start with off / trim
    MyCSurf_Auto_SetMode(0, false);
    MySetSurface_UpdateAutoLEDs();

    return true;
  } // MySetSurface_Init


  bool MySetSurface_Exit()
  {
    CSurf_ResetAllCachedVolPanStates();

    MySetSurface_ExitChanMode();

    for (char ch_id = 0; ch_id <= 24; ch_id++)
    {
      // reset faders
      MIDIOut(0xb0, ch_id + 0x1f, 0);
      MIDIOut(0xb0, ch_id, 0);

      // reset encoders
      if (ch_id < 24) MIDIOut(0xb0, ch_id + 0x40, 0);
    }

    // reset mute/solo/select
    for (char btn_id = 0; btn_id <= 0x79; btn_id ++)
      MIDIOut(0xb1, btn_id, 0);

    // reset bank leds
    MIDIOut(0xb0, 0x5d, 0);

    return true;
  } // MySetSurface_Exit


  void MySetSurface_UpdateButton(unsigned char btn_id, bool btn_state, bool blink)
  {
    unsigned char btn_cmd = 0x7f; // on
    if (blink) btn_cmd = 0x01; // blink
    if (!btn_state) btn_cmd = 0x00; // off
    MIDIOut(0xb1, btn_id, btn_cmd);
  } // MySetSurface_UpdateButton


  void MySetSurface_UpdateBankLEDs()
  {
    char led_id = s_ch_offset / 24;
    MIDIOut(0xb0, 0x5d, led_id);
  } // MySetSurface_UpdateBankLEDs


  void MySetSurface_UpdateAutoLEDs()
  {
    // update transport buttons
    for (char btn_id = 0; btn_id <= 3; btn_id ++)
    {
      bool on = false;

      if ( (btn_id == 3) && (s_play) ) on = true;
      if ( (btn_id == s_automode_alltks) && (s_myblink) ) on = !on;

      MySetSurface_UpdateButton(0x75 + btn_id, on, false);
    }
  } // MySetSurface_UpdateAutoLEDs


  void MySetSurface_UpdateAuxButtons()
  {
    for (char aux_id = 1; aux_id <= 6; aux_id++)
    {
      bool on = false;
      
      if ( (q_fkey) || (q_shift) )
      { // qualifier keys

        on = false;

      } else if (m_aux == aux_id) 
      { // aux modes

        on = true;

      } else if (m_chan) 
      { // chan mode
      
        // chan: bypass states
        bool bypass_fx = !(bool)TrackFX_GetEnabled(chan_rpr_tk, chan_fx);
        bool bypass_allfx = !(bool)GetMediaTrackInfo_Value(chan_rpr_tk, "I_FXEN");

        if ( (aux_id == 3) && (bypass_fx) && (!q_fkey) && (!q_shift) ) on = true;
        if ( (aux_id == 6) && (bypass_allfx) && (!q_fkey) && (!q_shift) ) on = true;
      }

      if (m_chan) MySetSurface_UpdateButton(0x64 + aux_id, on, true);
      else MySetSurface_UpdateButton(0x64 + aux_id, on, false);
    }
  } // MySetSurface_UpdateAuxButtons


  void MySetSurface_UpdateFader(unsigned char ch_id)
  {

    MediaTrack* rpr_tk = Cnv_ChannelIDToMediaTrack(ch_id);
    int para_amount;

    double d_value;
    int value;
    
    bool istrack = false;
    bool ismaster = false;
    bool isactive = true;

    // is track or master?
    if ( (ch_id >= 0) && (ch_id <= 23) ) istrack = true; // no track fader
    else if (rpr_tk == CSurf_TrackFromID(0, false)) ismaster = true;

    // active fader? 
    if (!rpr_tk) isactive = false; // no corresponding rpr_tk
    
    if ( (m_chan) && (m_flip) ) 
    { // only chan and flip: inside para_amount?
   
      para_amount = TrackFX_GetNumParams(chan_rpr_tk, chan_fx);
      if (chan_par_offs + ch_id >= para_amount) isactive = false;
      else isactive = true; // is track doesn't matter when chan and flipped
    
    } else if ( (m_aux > 0) && (m_flip) ) 
    { // only aux #x and flip: has send #x?
      
      int send_id = Cnv_AuxIDToSendID(ch_id, m_aux);
      if (send_id == -1) isactive = false;
    }

    if (istrack || ismaster)
    {
      // get values
      if (ismaster)
      { // if master -> volume

        GetTrackUIVolPan(rpr_tk, &d_value, NULL);
        value = Cnv_VolToFader(d_value);
      
      } else if (!isactive)
      { // if there is no track, parameter (chan & flip), send (aux & flip), reset

        value = 0; 
   
      } else
      { // if is active track fader

        if (m_flip)
        {
          if (m_chan) 
          { // flip & chan -> fx param

            double min, max;
            d_value = TrackFX_GetParam(chan_rpr_tk, chan_fx, chan_par_offs + ch_id, &min, &max);
            value = Cnv_FXParamToFader(min, max, d_value);

          } else if (m_aux > 0)
          { // flip + aux -> send Vol
            int send_id = Cnv_AuxIDToSendID(ch_id, m_aux);
            if (send_id != -1)
            {
              d_value = GetTrackSendUIVolPan(rpr_tk, send_id, &d_value, NULL);
              value = Cnv_VolToFader(d_value);
            }
          
          } else
          { // pan
            
            if (q_fkey)
            { // flip + fkey + pan -> width

              d_value = GetMediaTrackInfo_Value(rpr_tk, "D_WIDTH");
              value = Cnv_PanWidthToFader(d_value);
            
            } else {
              // flip + pan -> pan

              GetTrackUIVolPan(rpr_tk, NULL, &d_value);
              value = Cnv_PanWidthToFader(d_value);
            }
          } // if (m_chan)
        } else
        { // no flip -> volume

          GetTrackUIVolPan(rpr_tk, &d_value, NULL);
          value = Cnv_VolToFader(d_value);

        } // if (m_flip), else
      } // if (!rpr_tk || rpr_tk == CSurf_TrackFromID(0, false)), else
      
      // update
      MIDIOut(0xb0, ch_id + 0x1f, (value & 0x7f));
      MIDIOut(0xb0, ch_id, ((value >> 7) & 0x7f));

    } // if (active or master)
  } // MySetSurface_UpdateFader


  void MySetSurface_UpdateEncoder(int ch_id)
  {
    MediaTrack* rpr_tk = Cnv_ChannelIDToMediaTrack(ch_id);
    int para_amount;

    double d_value;
    unsigned char value;
    bool dot = false; // for phase switch, rec arm

    bool istrack = true;
    bool exists = false;
    bool isactive = true;

    // encoder exists?
    if ( (ch_id >= 0) && (ch_id <= 23) ) exists = true;

    // active encoder? 
    if (!rpr_tk) 
    { // no track
      
      isactive = false; 
      istrack = false;
    }

    if ( (m_chan) && (!m_flip) )
    { 
      para_amount = TrackFX_GetNumParams(chan_rpr_tk, chan_fx);
      if (chan_par_offs + ch_id >= para_amount) isactive = false;
      else isactive = true; // chan + fip: is track doesn't matter

    } else if (m_aux > 0) 
    { // only aux #x: has send #x?
      int send_id = Cnv_AuxIDToSendID(ch_id, m_aux);
      if (send_id == -1) isactive = false;
    }

    // get values and update
    if (exists)
    {
      if (!isactive)
      { // is not active encoder

        if (m_pan) value = 15; // reset, show something to indicate inactive
        else value = 0; // reset
      
      } else 
      { 
        if (m_flip)
        {
          if (m_aux > 0)
          { // flip + aux -> send Pan
            int send_id = Cnv_AuxIDToSendID(ch_id, m_aux);
            if (send_id != -1)
            {
              GetTrackSendUIVolPan(rpr_tk, send_id, NULL, &d_value);
              value = Cnv_PanToEncoder(d_value);
              value += 0x10; // pan mode
            }
          } else
          { // flip -> volume

            GetTrackUIVolPan(rpr_tk, &d_value, NULL);
            value = Cnv_VolToEncoder(d_value);
            value += 0x20; // bar mode
          }
        } else
        {
          if (m_chan)
          { // chan -> fx_param (para_offset checked above)

            double min, max;
            d_value = TrackFX_GetParam(chan_rpr_tk, chan_fx, chan_par_offs + ch_id, &min, &max);
            value = Cnv_FXParamToEncoder(min, max, d_value);
            value += 0x20; // bar mode
         
          } else if (m_aux > 0)
          { // aux -> send level

            int send_id = Cnv_AuxIDToSendID(ch_id, m_aux);
            if (send_id != -1)
            {
              GetTrackSendUIVolPan(rpr_tk, send_id, &d_value, NULL);
              value = Cnv_VolToEncoder(d_value);
              value += 0x20; // bar mode;
            }
          
          } else if (m_pan)
          {
            if (q_fkey) 
            { // pan mode + fkey -> width
              
              d_value = GetMediaTrackInfo_Value(rpr_tk, "D_WIDTH");
              value = Cnv_WidthToEncoder(d_value);
              value += 0x30; // width mode

            } else
            { // pan mode -> pan

              GetTrackUIVolPan(rpr_tk, NULL, &d_value);
              value = Cnv_PanToEncoder(d_value);
              value += 0x10; // pan mode
            }
          }
        } // if (m_flip), else
         
      } // if !active, else

      if (istrack)
      {
        // phase states
        if ( (bool)GetMediaTrackInfo_Value(rpr_tk, "B_PHASE") ) dot = true;
          
        // rec arms: blink
        if ( ((bool)GetMediaTrackInfo_Value(rpr_tk, "I_RECARM")) && (s_myblink) ) dot = !dot;

        if (dot) value += 0x40; // set dot
      }

      MIDIOut(0xb0, ch_id + 0x40, value);
    } // if exists
  } // MySetSurface_UpdateEncoder


  void MySetSurface_UpdateTrackElement(char ch_id)
  {
    MediaTrack* rpr_tk = Cnv_ChannelIDToMediaTrack(ch_id);
    bool selected = IsTrackSelected(rpr_tk);
    bool solo = (bool)GetMediaTrackInfo_Value(rpr_tk, "I_SOLO");
    bool mute = (bool)GetMediaTrackInfo_Value(rpr_tk, "B_MUTE");

    MySetSurface_UpdateButton(ch_id * 4 + 1, selected, false);
    MySetSurface_UpdateButton(ch_id * 4 + 2, solo, false);
    MySetSurface_UpdateButton(ch_id * 4 + 2, mute, false);
  } // MySetSurface_UpdateTrackElement


  void MySetSurface_ToggleFlip()
  {
    m_flip = !m_flip;

    CSurf_ResetAllCachedVolPanStates();
    TrackList_UpdateAllExternalSurfaces();

    MySetSurface_UpdateButton(0x63, m_flip, true);
  } // MySetSurface_ToggleFlip


  void MySetSurface_ToggleFKey(bool btn_state)
  {
    q_fkey = btn_state;
    MySetSurface_UpdateButton(0x6d, btn_state, false);
    MySetSurface_UpdateAuxButtons();

    // update encoders for width in pan mode
    if (m_pan)
      for (char ch_id = 0; ch_id < 24; ch_id++) 
        if (m_flip) MySetSurface_UpdateFader(ch_id);
        else MySetSurface_UpdateEncoder(ch_id);
  } // MySetSurface_ToggleFKey


  void MySetSurface_ToggleShift(bool btn_state)
  {
    q_shift = btn_state;
    MySetSurface_UpdateButton(0x74, btn_state, false);
    MySetSurface_UpdateAuxButtons();
  } // MySetSurface_ToggleShift


  void MySetSurface_ToggleScrub()
  {
    m_scrub = !m_scrub;
    MySetSurface_UpdateButton(0x6f, m_scrub, false);
  } // MySetSurface_ToggleScrub


  void MySetSurface_Chan_SetFxParamOffset(char dir)
  {
    chan_par_offs += 24 * dir;
    if (chan_par_offs < 0) chan_par_offs = 0;

    // check parameter count
    int amount_paras = TrackFX_GetNumParams(chan_rpr_tk, chan_fx);
    if (amount_paras <= chan_par_offs) chan_par_offs -= 24;
    
    // update encoders or faders
    for (char ch_id = 0; ch_id < 23; ch_id++) 
      if (m_flip) MySetSurface_UpdateFader(ch_id);
      else MySetSurface_UpdateEncoder(ch_id);
  } // MySetSurface_Chan_Set_FXParamOffset


  void MySetSurface_Chan_SelectTrack(unsigned char ch_id)
  {
    if (ch_id != chan_ch)
    {
      // reset button of old channel
      if (IsTrackSelected(chan_rpr_tk)) MySetSurface_UpdateButton(chan_ch * 4 + 1, true, false);
      else MySetSurface_UpdateButton(chan_ch * 4 + 1, false, false);

      // close fx of old channel
      MyCSurf_Chan_CloseFX(chan_fx);

      // activate new channel
      MediaTrack* rpr_tk = Cnv_ChannelIDToMediaTrack(ch_id);
      chan_ch = ch_id;
      chan_rpr_tk = rpr_tk;
      
      MySetSurface_UpdateButton(chan_ch * 4 + 1, true, true);

      // open fx              
      MyCSurf_Chan_OpenFX(chan_fx);

    } else MySetSurface_ExitChanMode();
  } // MySetSurface_Chan_SelectTrack


  void MySetSurface_ShiftBanks(char dir, char factor)
  { 

    double track_amount = (double)CountTracks(0);
    int max_offset = (int)( ceil( (track_amount) / (double)factor ) - (24.0 / double(factor)));
    max_offset *= factor;

    int old_ch_offset = s_ch_offset;

    // move in dir by 8 or 24 (factor)
    s_ch_offset += factor * dir;
    s_ch_offset -= (s_ch_offset % factor);

    // min / max
    if (s_ch_offset > max_offset) s_ch_offset = max_offset;
    if (s_ch_offset > 168) s_ch_offset = 168;
    if (s_ch_offset < 0) s_ch_offset = 0;

    // if correction push in wrong direction keep old
    if ( (dir > 0) && (old_ch_offset > s_ch_offset) ) s_ch_offset = old_ch_offset;
    if ( (dir < 0) && (old_ch_offset < s_ch_offset) ) s_ch_offset = old_ch_offset;

    // update mixer display
    MediaTrack* leftmost = GetTrack(0, s_ch_offset);
    SetMixerScroll(leftmost);

    for(char ch_id = 0; ch_id < 24; ch_id++)
    {
      MySetSurface_UpdateEncoder(ch_id);
      MySetSurface_UpdateFader(ch_id);
      MySetSurface_UpdateTrackElement(ch_id);
    }
    //TrackList_UpdateAllExternalSurfaces();
    MySetSurface_UpdateBankLEDs();
  } // MySetSurface_ShiftBanks


  void MySetSurface_EnterChanMode()
  {
    if (m_pan) MySetSurface_ExitPanMode();
    if (m_aux > 0) MySetSurface_ExitAuxMode();
    
    m_chan = true;
    // blink Chan Button
    MySetSurface_UpdateButton(0x64, true, true);

    chan_rpr_tk = Cnv_ChannelIDToMediaTrack(chan_ch);

    MyCSurf_Chan_OpenFX(chan_fx);
    
    // blink Track Select
    MySetSurface_UpdateButton(chan_ch * 4 + 1, true, true);

    // blink para offset, bypass
    MySetSurface_UpdateAuxButtons();
      
    // blink banks
    MySetSurface_UpdateButton(0x70, true, true);
    MySetSurface_UpdateButton(0x71, true, true);
  } // MySetSurface_EnterChanMode


  void MySetSurface_ExitChanMode()
  {
    m_chan = false;
    MySetSurface_UpdateButton(0x64, false, false);

    // reset select button
    if (IsTrackSelected(chan_rpr_tk)) MySetSurface_UpdateButton(chan_ch * 4 + 1, true, false);
    else MySetSurface_UpdateButton(chan_ch * 4 + 1, false, false);

    MyCSurf_Chan_CloseFX(chan_fx);

    // unblink bank buttons
    MySetSurface_UpdateButton(0x70, false, false);
    MySetSurface_UpdateButton(0x71, false, false);

    MySetSurface_EnterPanMode();
  } // MySetSurface_ExitChanMode


  void MySetSurface_EnterPanMode()
  {
    if (m_chan) MySetSurface_ExitChanMode();
    if (m_aux > 0) MySetSurface_ExitAuxMode();

    m_pan = true;
    MySetSurface_UpdateButton(0x6c, true, false);

    // reset Aux
    MySetSurface_UpdateAuxButtons();

    // update encoders or faders
    for (char ch_id = 0; ch_id < 23; ch_id++) 
      if (m_flip) MySetSurface_UpdateFader(ch_id);
      else MySetSurface_UpdateEncoder(ch_id);
  } // MySetSurface_EnterPanMode


  void MySetSurface_ExitPanMode()
  {
    m_pan = false;
    MySetSurface_UpdateButton(0x6c, false, false);
  } // MySetSurface_ExitPanMode


  void MySetSurface_EnterAuxMode(unsigned char sel)
  {
    if (m_pan) MySetSurface_ExitPanMode();

    m_aux = sel;

    // reset Aux
    MySetSurface_UpdateAuxButtons();

    // update encoders or faders
    for (char ch_id = 0; ch_id < 23; ch_id++)
    { 
      if (m_flip) MySetSurface_UpdateFader(ch_id);
      MySetSurface_UpdateEncoder(ch_id); // update encoders on flip also (> send pan)!
    }
  } // MySetSurface_EnterAuxMode


  void MySetSurface_ExitAuxMode()
  {
    m_aux = 0;
  } // MySetSurface_ExitAuxMode

  

  // REAPER INITIATED SURFACE UPDATES

  /* To avoid swamping the US-2400 faders/encoders with MIDI updates (especially
  in auto read mode), we don't use the volume/pan 'events' below. instead we query 
  values and update faders/encoders in the central loop (see 'run' function). This 
  fixes the problem but also slows down response. Maybe just a temprary fix? */


  /*

  void SetSurfaceVolume(MediaTrack* rpr_tk, double vol)
  { 
    int ch_id = Cnv_MediaTrackToChannelID(rpr_tk);

    if ( (ch_id >= 0) && (ch_id <= 24) )
      if (!m_flip) MySetSurface_UpdateFader(ch_id);
      else if (ch_id <= 23) MySetSurface_UpdateEncoder(ch_id);
  } // SetSurfaceVolume
  

  void SetSurfacePan(MediaTrack* rpr_tk, double pan)
  {
    int ch_id = Cnv_MediaTrackToChannelID(rpr_tk);

    if ( (ch_id >= 0) && (ch_id <= 24) )
      if (m_flip) MySetSurface_UpdateFader(ch_id);
      else if (ch_id <= 23) MySetSurface_UpdateEncoder(ch_id);
  } // SetSurfacePan
  
  */


  void SetTrackListChange()
  {
    CSurf_ResetAllCachedVolPanStates(); // is this needed?
  } // SetTrackListChange


  void SetSurfaceMute(MediaTrack* rpr_tk, bool mute)
  {
    int ch_id = Cnv_MediaTrackToChannelID(rpr_tk);

    if ( (ch_id >= 0) && (ch_id <= 23) ) 
    {
      if (mute) MySetSurface_UpdateButton(4 * ch_id + 3, true, false);
      else MySetSurface_UpdateButton(4 * ch_id + 3, false, false);
    }
  } // SetSurfaceMute


  void SetSurfaceSelected(MediaTrack* rpr_tk, bool selected)
  {
    int ch_id = Cnv_MediaTrackToChannelID(rpr_tk);

    if ( (ch_id >= 0) && (ch_id <= 24) ) 
    {
      bool blink = false;
      bool on = false;
      if (selected) on = true;
      if ( (m_chan) && (ch_id == chan_ch) ) 
      { 
        on = true;
        blink = true;
      }
      MySetSurface_UpdateButton(4 * ch_id + 1, on, blink);
      Main_OnCommand(CMD_SEL2LASTTOUCH, 0);
    }
  } // SetSurfaceSelected


  void SetSurfaceSolo(MediaTrack* rpr_tk, bool solo)
  {
    int ch_id = Cnv_MediaTrackToChannelID(rpr_tk);

    if ( (ch_id >= 0) && (ch_id <= 23) ) 
    {
      if (solo) MySetSurface_UpdateButton(4 * ch_id + 2, true, false);
      else MySetSurface_UpdateButton(4 * ch_id + 2, false, false);
    }

    // update CLR SOLO
    if (AnyTrackSolo(0)) MySetSurface_UpdateButton(0x62, true, true);
    else MySetSurface_UpdateButton(0x62, false, false);
  } // SetSurfaceSolo


  void SetSurfaceRecArm(MediaTrack* rpr_tk, bool recarm)
  {
    int ch_id = Cnv_MediaTrackToChannelID(rpr_tk);
    MySetSurface_UpdateEncoder(ch_id);
  } // SetSurfaceRecArm


  bool GetTouchState(MediaTrack* rpr_tk, int isPan)
  {
    int ch_id = Cnv_MediaTrackToChannelID(rpr_tk);

    if ( (ch_id >= 0) && (ch_id <= 23) && ((s_touchstates & (1 << ch_id)) > 0) ) return true;
    else return false;
  } // GetTouchState


  void SetAutoMode(int mode)
  {
    s_automode_alltks = mode; 
    MySetSurface_UpdateAutoLEDs();
  } // SetAutoMode


  void SetPlayState(bool play, bool pause, bool rec)
  {
    s_play = play && !pause;
    s_rec = rec;
    
    MySetSurface_UpdateAutoLEDs();
    MySetSurface_UpdateButton(0x79, s_rec, false);
  } // SetPlayState


  void SetRepeatState(bool rep)
  {
    s_loop = rep;

    // update in/out buttons
    MySetSurface_UpdateButton(0x72, s_loop, false); 
    MySetSurface_UpdateButton(0x73, s_loop, false); 
  } // SetRepeatState



  ////// SUBMIT CHANGES TO REAPER /////

  // API Event Triggers:
  // double CSurf_OnVolumeChange(MediaTrack *trackid, double volume, bool relative)
  // double CSurf_OnPanChange(MediaTrack *trackid, double pan, bool relative)
  // bool CSurf_OnMuteChange(MediaTrack *trackid, int mute)
  // bool CSurf_OnSelectedChange(MediaTrack *trackid, int selected)
  // bool CSurf_OnSoloChange(MediaTrack *trackid, int solo)
  // bool CSurf_OnFXChange(MediaTrack *trackid, int en)
  // bool CSurf_OnRecArmChange(MediaTrack *trackid, int recarm)
  // void CSurf_OnPlay()
  // void CSurf_OnStop()
  // void CSurf_OnFwd(int seekplay)
  // void CSurf_OnRew(int seekplay)
  // void CSurf_OnRecord()
  // void CSurf_GoStart()
  // void CSurf_GoEnd()
  // void CSurf_OnArrow(int whichdir, bool wantzoom)
  // void CSurf_OnTrackSelection(MediaTrack *trackid)
  // void CSurf_ResetAllCachedVolPanStates()
  // void CSurf_void ScrubAmt(double amt)


  // TRACK CONTROLS AND BEYOND

  void MyCSurf_ToggleSolo(MediaTrack* rpr_tk, bool this_only)
  {
    int solo = (int)GetMediaTrackInfo_Value(rpr_tk, "I_SOLO");
    if (this_only)
    {
      // unsolo all, (re-)solo only selected
      SoloAllTracks(0);
      solo = 2;
    } else {
      // toggle solo
      if (solo == 2) solo = 0;
      else solo = 2;
    }

    CSurf_OnSoloChange(rpr_tk, solo);

    bool solo_surf = true;  
    if (solo == 0) solo_surf = false;

    SetSurfaceSolo(rpr_tk, solo_surf);
  } // MyCSurf_ToggleSolo


  void MyCSurf_UnsoloAllTracks()
  {
    SoloAllTracks(0);
  } // MyCSurf_UnsoloAllTracks


  void MyCSurf_ToggleMute(MediaTrack* rpr_tk, bool this_only)
  {
    char mute;
    if (this_only)
    {
      // unmute all, (re-)mute only selected
      MuteAllTracks(0);
      mute = 1;
    } else
    {
      // toggle mute on selected
      mute = -1;
    }
    CSurf_OnMuteChange(rpr_tk, mute);
  } // MyCSurf_ToggleMute


  void MyCSurf_UnmuteAllTracks()
  {
    MuteAllTracks(0);
  } // MyCSurf_UnmuteAllTracks


  void MyCSurf_SwitchPhase(MediaTrack* rpr_tk)
  {
    char ch_id = Cnv_MediaTrackToChannelID(rpr_tk);

    bool phase = (bool)GetMediaTrackInfo_Value(rpr_tk, "B_PHASE");

    phase = !phase;
    SetMediaTrackInfo_Value(rpr_tk, "B_PHASE", phase);

    MySetSurface_UpdateEncoder(ch_id);
  } // MyCSurf_SwitchPhase


  void MyCSurf_SelectMaster() 
  {
    MediaTrack* rpr_master = Cnv_ChannelIDToMediaTrack(24);

    //bool master_sel = (bool)GetMediaTrackInfo_Value(rpr_master, "I_SELECTED");
    bool master_sel = IsTrackSelected(rpr_master);
    master_sel = !master_sel;
    CSurf_OnSelectedChange(rpr_master, (int)master_sel); // ?
    SetTrackSelected(rpr_master, master_sel); // ?
    Main_OnCommand(CMD_SEL2LASTTOUCH, 0);
  } // MyCSurf_SelectMaster


  void MyCSurf_ToggleSelectAllTracks() 
  {
    int sel_tks = CountSelectedTracks(0);
    int all_tks = CountTracks(0);

    if (sel_tks != all_tks) Main_OnCommand(CMD_SELALLTKS, 0);
    else Main_OnCommand(CMD_UNSELALLTKS, 0);
  } // MyCSurf_ToggleSelectAllTracks() 


  // CUSTOM TRANSPORT 

  void MyCSurf_OnRew()
  {
    Main_OnCommand(CMD_RJUMP, 0);
  } // MyCSurf_OnRew


  void MyCSurf_OnFwd()
  {
    Main_OnCommand(CMD_FJUMP, 0);
  } // MyCSurf_OnFwd


  void MyCSurf_OnRec()
  {
    if (s_play) 
    {
      s_rec = !s_rec;
      Main_OnCommand(CMD_TGGLRECBEAT, 0);
      if (s_rec) MySetSurface_UpdateButton(0x79, true, true);
      else MySetSurface_UpdateButton(0x79, false, false);
    } else 
    {
      CSurf_OnRecord();
    }
  } // MyCSurf_OnRec


  // AUX

  void MyCSurf_Aux_Send(char sel, bool add)
  {
    /* AddReceive / RemoveReceivesFrom don't work, triggering custom actions instead 

    char* name;
    sprintf(name, "AUX %d", sel);

    MediaTrack* aux = Hlp_FindTrackByName(name);

    int sel_tks = CountSelectedTracks(0);
    for (int tk = 0; tk < sel_tks; tk++)
      if (add) SNM_AddReceive(GetTrack(0, tk), aux, 1);
      else SNM_RemoveReceivesFrom(aux, GetTrack(0, tk));
    */
    if (add)
    { // add aux
      switch (sel)
      { 
        case 1 : Main_OnCommand(CMD("_5f42cd59520ef749a178581506725018"), 0); break;
        case 2 : Main_OnCommand(CMD("_dbc1c827ec5e2140819ca67de25be76c"), 0); break;
        case 3 : Main_OnCommand(CMD("_277b0ae3c0dfcf4ba9404cd93a6f79fd"), 0); break;
        case 4 : Main_OnCommand(CMD("_8783a9f3d96ace499c8eee563e763f56"), 0); break;
        case 5 : Main_OnCommand(CMD("_72770d531b4cdb42833c6bd9978be87d"), 0); break;
        case 6 : Main_OnCommand(CMD("_d3d039b1a04d654dabc94cc5972d63b7"), 0); break;
      }

    } else
    { // remove aux
      switch (sel)
      { 
        case 1 : Main_OnCommand(CMD("_5d5c6ca3ea072c4abec0faa76591d602"), 0); break;
        case 2 : Main_OnCommand(CMD("_bd4148cf5366364f84574ba3d982fd60"), 0); break;
        case 3 : Main_OnCommand(CMD("_9fd2e3ff6dc66749a07f07ce5e490ffb"), 0); break;
        case 4 : Main_OnCommand(CMD("_4fa134fcf3a2224281d0a1b818791fde"), 0); break;
        case 5 : Main_OnCommand(CMD("_72ebde5464da3242bb7dc58353ab8216"), 0); break;
        case 6 : Main_OnCommand(CMD("_7c2d815481b8b54bba9f7ce5a6bb4618"), 0); break;
      }
    }

    for (char ch = 0; ch < 24; ch++)
    {
      if (m_flip) MySetSurface_UpdateFader(ch);
      MySetSurface_UpdateEncoder(ch);
    }
  }


  // CHANNEL STRIP

  void MyCSurf_Chan_SelectFX(int open_fx_id)
  {
    MyCSurf_Chan_CloseFX(chan_fx);
    MyCSurf_Chan_OpenFX(open_fx_id);
  } // MyCSurf_Chan_SelectFX


  void MyCSurf_Chan_OpenFX(int fx_id)
  {
    int amount_fx = TrackFX_GetCount(chan_rpr_tk);

    // any fx?
    if (amount_fx > 0)
    {
      if (fx_id >= amount_fx) fx_id = 0;
      else if (fx_id < 0) fx_id = amount_fx - 1;
    
      chan_fx = fx_id;
      TrackFX_Show(chan_rpr_tk, chan_fx, 2); // hide floating window
      TrackFX_Show(chan_rpr_tk, chan_fx, 1); // show chain window
      TrackFX_SetOpen(chan_rpr_tk, chan_fx, true);

      /* Stuff below is not needed when we update faders/encoders in a loop 

      // update encoders or faders
      for (char ch_id = 0; ch_id < 23; ch_id++) 
        if (m_flip) MySetSurface_UpdateFader(ch_id);
        else MySetSurface_UpdateEncoder(ch_id);

      */

    } else MyCSurf_Chan_InsertFX();
  } // MyCSurf_Chan_OpenFX
  

  void MyCSurf_Chan_CloseFX(int fx_id)
  {
    TrackFX_Show(chan_rpr_tk, fx_id, 2); // hide floating window
    TrackFX_Show(chan_rpr_tk, fx_id, 0); // hide chain window
  } // MyCSurf_Chan_CloseFX


  void MyCSurf_Chan_DeleteFX()
  {
    int before_del = TrackFX_GetCount(chan_rpr_tk);

    //isolate track for action
    Hlp_SaveSelection();
    SetOnlyTrackSelected(chan_rpr_tk);
    Main_OnCommand(CMD_SEL2LASTTOUCH, 0);

    TrackFX_SetOpen(chan_rpr_tk, chan_fx, true);
    Main_OnCommand(CMD("_S&M_REMOVE_FX"), 0);
    
    Hlp_RestoreSelection();
    
    if (before_del > 1)
    { 
      // if there are fx left open the previous one in chain
      chan_fx--;
      MyCSurf_Chan_OpenFX(chan_fx);
      
      // otherwise exit chan mode
    } else MySetSurface_ExitChanMode();
  } // MyCSurf_Chan_DeleteFX


  void MyCSurf_Chan_InsertFX()
  {
    // isolate track for action
    Hlp_SaveSelection();
    SetOnlyTrackSelected(chan_rpr_tk);
    Main_OnCommand(CMD_SEL2LASTTOUCH, 0);
    
    TrackFX_Show(chan_rpr_tk, chan_fx, 1); // show chain window
    TrackFX_SetOpen(chan_rpr_tk, chan_fx, true);
    Main_OnCommand(CMD_FXBROWSER, 0);

    Hlp_RestoreSelection();
  } // MyCSurf_Chan_InsertFX


  void MyCSurf_Chan_MoveFX(char dir)
  {
    int amount_fx = TrackFX_GetCount(chan_rpr_tk);

    // isolate track for selection
    Hlp_SaveSelection();
    SetOnlyTrackSelected(chan_rpr_tk);
    Main_OnCommand(CMD_SEL2LASTTOUCH, 0);

    TrackFX_SetOpen(chan_rpr_tk, chan_fx, true);
    if ( (dir < 0) && (chan_fx > 0) )
    {
      Main_OnCommand(CMD("_S&M_MOVE_FX_UP"), 0);
      chan_fx--;
    
    } else if ( (dir > 0) && (chan_fx < amount_fx - 1) )
    {
      Main_OnCommand(CMD("_S&M_MOVE_FX_DOWN"), 0);
      chan_fx++;  
    }

    Hlp_RestoreSelection();
  } // MyCSurf_Chan_MoveFX


  void MyCSurf_Chan_ToggleAllFXBypass()
  {
    bool bypass = (bool)GetMediaTrackInfo_Value(chan_rpr_tk, "I_FXEN");
    bypass = !bypass;
    CSurf_OnFXChange(chan_rpr_tk, (int)bypass);
    MySetSurface_UpdateAuxButtons();
  } // MyCSurf_Chan_ToggleAllFXBypass


  void MyCSurf_Chan_ToggleFXBypass()
  {
    bool bypass = (bool)TrackFX_GetEnabled(chan_rpr_tk, chan_fx);
    bypass = !bypass;
    TrackFX_SetEnabled(chan_rpr_tk, chan_fx, bypass);
    MySetSurface_UpdateAuxButtons();
  } // MyCSurf_Chan_ToggleFXBypass


  void MyCSurf_Chan_SetFXParam(MediaTrack* rpr_tk, int fx_id, int para_id, double value)
  {
    char ch_id = Cnv_MediaTrackToChannelID(rpr_tk);

    TrackFX_SetParam(rpr_tk, fx_id, para_id, value);

    if (m_flip) MySetSurface_UpdateFader(ch_id);
    else MySetSurface_UpdateEncoder(ch_id);
  } // MyCSurf_Chan_SetFXParam


  // TRACKS

  void MyCSurf_Tks_MoveSelected(char dir)
  {
    int all_sel = CountSelectedTracks(0);

    if (all_sel > 0)
    {
      ReaProject* rpr_pro = EnumProjects(-1, NULL, 0);
      MediaTrack* insert;

      Main_OnCommand(CMD("_S&M_CUTSNDRCV1"), 0);

      if (dir < 0)
      {
        //start at first Track of sel
        insert = GetSelectedTrack(0, 0);
        SetOnlyTrackSelected(insert);
        Main_OnCommand(CMD_SEL2LASTTOUCH, 0);

        // go left
        Main_OnCommand(CMD_PREVTK, 0);

        // if first track reached select master
        if (GetSelectedTrack(0, 0) == GetTrack(0, 0)) insert = GetMasterTrack(rpr_pro);
        // otherwise go left again
        else {
          Main_OnCommand(CMD_PREVTK, 0);
          insert = GetSelectedTrack(0, 0);
        }
      
      } else
      {
        insert = GetSelectedTrack(0, all_sel);
        SetOnlyTrackSelected(insert);
        Main_OnCommand(CMD_SEL2LASTTOUCH, 0);
      }

      // get selection back and cut          
      Hlp_RestoreSelection();

      SetOnlyTrackSelected(insert);
      Main_OnCommand(CMD_SEL2LASTTOUCH, 0);
      Main_OnCommand(CMD("_S&M_PASTSNDRCV1"), 0);
    }
  } // MyCSurf_Tks_MoveSelected


  void MyCSurf_Tks_Insert()
  {
    int all_sel = CountSelectedTracks(0);

    if (all_sel > 0)
    {
      Main_OnCommand(CMD_SEL2LASTTOUCH, 0);
      Main_OnCommand(CMD_INSERTTK, 0);
    }
  } // MyCSurf_Tks_Insert


  void MyCSurf_Tks_DeleteSelected()
  {
    int all_sel = CountSelectedTracks(0);
      
    if (all_sel > 0)
    {
      ReaProject* rpr_pro = EnumProjects(-1, NULL, 0);
      MediaTrack* rpr_tk;

      for(int sel_tk = 0; sel_tk < all_sel; sel_tk++)
      {
        rpr_tk = GetSelectedTrack(rpr_pro, sel_tk);
        DeleteTrack(rpr_tk);
      }
    }
  } // MyCSurf_Tks_DeleteSelected


  void MyCSurf_Tks_DuplicateSelected()
  {
    int all_sel = CountSelectedTracks(0);
      
    if (all_sel > 0)
      Main_OnCommand(CMD_DUPLITK, 0);
  } // MyCSurf_Tks_DuplicateSelected


  void MyCSurf_Tks_RenameSelected()
  {
    if (CountSelectedTracks(0) > 0)
    {
      Hlp_SaveSelection();
      SetOnlyTrackSelected(GetSelectedTrack(0,0));
      Main_OnCommand(CMD_SEL2LASTTOUCH, 0);
      Main_OnCommand(CMD_RENAMETK,0 );
      Hlp_RestoreSelection();
    }
  } // MyCSurf_Tks_RenameSelected


  void MyCSurf_Tks_GroupSelected(bool group)
  {
    if (group) Main_OnCommand(CMD("_S&M_SET_TRACK_UNUSEDGROUP"), 0);
    else Main_OnCommand(CMD("_S&M_REMOVE_TR_GRP"), 0);
  } // MyCSurf_Tks_GroupSelected


  void MyCSurf_Tks_WrapUnwrapSelected()
  {
    MediaTrack* folder;
    MediaTrack* track;
    bool hasfolder = false;

    // save first selected track (for wrap)
    track = GetSelectedTrack(0, 0);
    
    // go through selected until folder
    int all_sel = CountSelectedTracks(0);
    for (int tk = 0; tk < all_sel; tk++)
    {
      folder = GetSelectedTrack(0, tk);
      
      int flags;
      GetTrackState(folder, &flags);
      // folder found, end loop
      if (flags & 1) {
        hasfolder = true;
        tk = all_sel;
      }
    }

    // buffer and remove selection
    Hlp_SaveSelection();
    
    if (hasfolder)
    { // UNWRAP

      SetOnlyTrackSelected(folder);
      Main_OnCommand(CMD_SEL2LASTTOUCH, 0);

      // copy routing from folder to children
      SetOnlyTrackSelected(folder);
      Main_OnCommand(CMD_SEL2LASTTOUCH, 0);

      if (CountSelectedTracks(0) > 0)
      {
        Main_OnCommand(CMD("_S&M_CUTSNDRCV2"), 0);
        Main_OnCommand(CMD("_SWS_SELCHILDREN2"), 0); //_SWS_SELCHILDREN  ??
        Main_OnCommand(CMD("_S&M_PASTSNDRCV2"), 0);

        SetOnlyTrackSelected(folder);
        Main_OnCommand(CMD_SEL2LASTTOUCH, 0);
        Main_OnCommand(CMD("_XENAKIOS_SELTRACKTONOTFOLDER"), 0);
        DeleteTrack(folder);
      }

      // get selection back
      Hlp_RestoreSelection();
      
    } else 
    { // WRAP

      // start at first selected track
      SetOnlyTrackSelected(track);
      Main_OnCommand(CMD_SEL2LASTTOUCH, 0);

      // go one to the left
      Main_OnCommand(CMD_PREVTK, 0);
      
      // insert a new track
      MyCSurf_Tks_Insert();
      
      // folder is new track
      folder = GetSelectedTrack(0, 0);

      // copy routing of first track to master
      SetOnlyTrackSelected(track);
      Main_OnCommand(CMD_SEL2LASTTOUCH, 0);
      Main_OnCommand(CMD("_S&M_CUTSNDRCV2"), 0);

      SetOnlyTrackSelected(folder);
      Main_OnCommand(CMD_SEL2LASTTOUCH, 0);
      Main_OnCommand(CMD("_S&M_PASTSNDRCV2"), 0);

      // get selection back
      Hlp_RestoreSelection();

      // all selected tracks
      int all_sel = CountSelectedTracks(0);
      for (int tk = 0; tk < all_sel; tk++)
      {
        track = GetSelectedTrack(0, tk);
        Main_OnCommand(CMD_SEL2LASTTOUCH, 0);

        // remove routing
        Main_OnCommand(CMD("_S&M_CUTSNDRCV2"), 0);
      }

      // add folder to selection
      SetTrackSelected(folder, true);
      Main_OnCommand(CMD_SEL2LASTTOUCH, 0);
      Main_OnCommand(CMD("_SWS_MAKEFOLDER"), 0);
    }
  } // MyCSurf_Tks_WrapUnwrapSelected


  void MyCSurf_Tks_ToggleShowFolder()
  {
    Main_OnCommand(CMD_MCP_HIDECHILDR, 0);   
  } // MyCSurf_Tks_ToggleShowFolder


  // AUTOMATION

  void MyCSurf_Auto_SetMode(int mode)
  {
    // mode: 0 = off / trim, 1 = read, 2 = touch, 3 = write
    // only_sel: false = all tracks
    int sel_tks = CountSelectedTracks(0);
    int all_tks = CountTracks(0);
    
    bool only_sel = true;
    if ( (sel_tks == all_tks) || (sel_tks == 0) ) only_sel = false;
    
    SetAutomationMode(mode, only_sel);
  } // MyCSurf_Auto_SetMode


  // overloaded with set only_sel for init
  void MyCSurf_Auto_SetMode(int mode, bool only_sel)
  {
    // mode: 0 = off / trim, 1 = read, 2 = touch, 3 = write
    // only_sel: false = all tracks
    int sel_tks = CountSelectedTracks(0);
    int all_tks = CountTracks(0);
    
    SetAutomationMode(mode, only_sel);
  } // MyCSurf_Auto_SetMode


  void MyCSurf_Auto_WriteCurrValues()
  {
    Main_OnCommand(CMD_AUTOTOSEL, 0);
  } // MyCSurf_Auto_WriteCurrValues


  // CUSTOM COMMANDS FOR TIME SELECTION / CURSOR

  void MyCSurf_ToggleRepeat()
  {
    s_loop = (bool)GetSetRepeat(2);
  } // MyCSurf_ToggleRepeat


  void MyCSurf_MoveTimeSel(signed char start_dir, signed char end_dir, bool markers)
  {
    // markers: true = move to next marker, false = move 1 bar
    // start_dir: move start of time sel by dir
    // end_dir move end of time sel by dir

    // get time range
    ReaProject* rpr_pro = EnumProjects(-1, NULL, 0);
    double start_time, start_beats, end_time, end_beats;
    int start_num, start_denom, end_num, end_denom;

    // get time selection
    GetSet_LoopTimeRange(false, true, &start_time, &end_time, false); // ??

    if (markers)
    {
      // go through all markers
      int x = 0;
      bool stop = false;
      double prev_marker = 0.0;
      double marker;
      while ( (x = EnumProjectMarkers(x, NULL, &marker, NULL, NULL, NULL)) && !stop )
      {
        if ( ((start_dir < 0) && (marker >= start_time))
          || ((end_dir > 0) && (marker > end_time)) )
        {
          start_time = prev_marker;
          end_time = marker;  
          stop = true;
        }
        prev_marker = marker;
      }
      
      // set time selection
      if (stop) GetSet_LoopTimeRange(true, true, &start_time, &end_time, false);

    } else
    {
      // start / end: get time sig and position in beats
      TimeMap_GetTimeSigAtTime(rpr_pro, start_time, &start_num, &start_denom, NULL);
      start_beats = TimeMap2_timeToQN(rpr_pro, start_time);

      TimeMap_GetTimeSigAtTime(rpr_pro, end_time, &end_num, &end_denom, NULL);
      end_beats = TimeMap2_timeToQN(rpr_pro, end_time);

      // shift by bars
      start_beats += start_dir * (4 * start_num / start_denom);
      end_beats += end_dir * (4 * end_num / end_denom);

      // reconvert to seconds
      start_time = TimeMap2_QNToTime(rpr_pro, start_beats);
      end_time = TimeMap2_QNToTime(rpr_pro, end_beats);

      // snap to grid
      SnapToGrid(rpr_pro, start_time);
      SnapToGrid(rpr_pro, end_time);

      // set time selection
      GetSet_LoopTimeRange(true, true, &start_time, &end_time, false);
    }

  } // MyCSurf_MoveTimeSel


  void MyCSurf_Loop_ToggleAll()
  {
    if (!s_loop_all) {
      
      // save current sel
      GetSet_LoopTimeRange(false, true, &s_ts_start, &s_ts_end, false);
      
      // time sel all
      Main_OnCommand(CMD_SELALLITEMS, 0);
      Main_OnCommand(CMD_TIMESEL2ITEMS, 0);
      Main_OnCommand(CMD_UNSELALLITEMS, 0);  

      s_loop_all = true;
    
    } else {

      // restore time sel
      GetSet_LoopTimeRange(true, true, &s_ts_start, &s_ts_end, false);

      s_loop_all = false;
    }   
  } // MyCSurf_Loop_ToggleAll


  void MyCSurf_RemoveTimeSel()
  {
    Main_OnCommand(CMD_CLEARTIMESEL, 0);
  } // MyCSurf_RemoveTimeSel


  void MyCSurf_Scrub(char rel_value, bool fast)
  {
    double d_value;
    if (fast) d_value = (double)rel_value / (double)SCRUBRESFAST;
    else d_value = (double)rel_value / (double)SCRUBRESSLOW;
    
    CSurf_ScrubAmt(d_value);
  } // MyCSurf_Scrub
    

  void MyCSurf_MoveEditCursor(char rel_value, bool fast)
  {
    ReaProject* rpr_pro = EnumProjects(-1, NULL, 0);
    double old_val = GetCursorPosition();
    double d_value;
    if (fast) d_value = (double)rel_value / (double)MOVERESFAST;
    else d_value = (double)rel_value / (double)MOVERESSLOW;

    SetEditCurPos2(rpr_pro, old_val + d_value, true, false);
  } // MyCSurf_MoveEditCursor


  // CUSTOM COMMANDS FOR SCROLL AND ZOOM

  void MyCSurf_ToggleScrollOnPlay()
  {
    Main_OnCommand(CMD_TGLPLAYSCROLL, 0);
    Main_OnCommand(CMD_TGLRECSCROLL, 0);
  } // MyCSurf_ToggleScrollOnPlay


  void MyCSurf_CenterHScrollToPlay()
  {
    Main_OnCommand(CMD("_SWS_HSCROLLPLAY50"), 0);
  } // MyCSurf_CenterHScrollToPlay


  void MyCSurf_ToggleHZoomToTimeSel()
  {
    Main_OnCommand(CMD("_SWS_TOGZOOMTT"), 0);
  } // MyCSurf_ToggleHZoomToTimeSel


  void MyCSurf_VZoomToSelTracks()
  {
    Main_OnCommand(CMD("_SWS_VZOOMFIT"), 0);
  } // MyCSurf_VZoomToSelTracks


  // CUSTOM COMMANDS FOR FILE HANDLING

  void MyCSurf_Undo()
  {
    Main_OnCommand(CMD_UNDO, 0);
  } // MyCSurf_Undo


  void MyCSurf_Redo()
  {
    Main_OnCommand(CMD_REDO, 0);
  } // MyCSurf_Redo


  void MyCSurf_Save(bool overwrite)
  {
    if (overwrite) Main_OnCommand(CMD_SAVE, 0);
    else Main_OnCommand(CMD_SAVEAS, 0);
  } // MyCSurf_Save


  // WINDOW MESSAGES (EMULATE MOUSE AND KEYSTROKES FOR SOME ACTIONS)

  void MyCSurf_EmulateKeyStroke(int key_id)
  {
    HWND top = GetForegroundWindow();
    PostMessage(top, WM_KEYDOWN, key_id, 0);
    PostMessage(top, WM_KEYUP, key_id, 0);
  } // MyCSurf_EmulateKeyStroke



  ////// CONFIG STUFF FOR REGISTERING AND PREFERENCES //////

  const char *GetTypeString() 
  {
    return "US-2400";
  }
  

  const char *GetDescString()
  {
    descspace.Set("Tascam US-2400");
    char tmp[512];
    sprintf(tmp," (dev %d,%d)",m_midi_in_dev,m_midi_out_dev);
    descspace.Append(tmp);
    return descspace.Get();     
  }
  

  const char *GetConfigString() // string of configuration data
  {
    sprintf(configtmp,"0 0 %d %d",m_midi_in_dev,m_midi_out_dev);      
    return configtmp;
  }



  ////// CENTRAL CSURF MIDI EVENT LOOP //////

  void Run()
  {
    if ( (m_midiin) ) //&& (s_initdone) )
    {
      m_midiin->SwapBufs(timeGetTime());
      int l=0;
      MIDI_eventlist *list=m_midiin->GetReadBuf();
      MIDI_event_t *evts;
      while ((evts=list->EnumItems(&l))) MIDIin(evts);
    }

    // update faders + encoders (X at a time)
    for (char i = 0; i < UPDCYCLE; i++)
    {
      MySetSurface_UpdateFader(curr_update);
      if (curr_update < 24) MySetSurface_UpdateEncoder(curr_update);
      curr_update++;
      if (curr_update > 24) curr_update = 0;
    }

    // blink
    if (myblink_ctr > MYBLINKINTV)
    {
      s_myblink = !s_myblink;

      // reset counter, on is shorter
      if (s_myblink) myblink_ctr = MYBLINKINTV - MYBLINKRATIO;
      else myblink_ctr = 0;

      /* We don't have to do this since as of now we're updating faders/encoders 
      in run-loop anyway

      // update encoders (rec arm)
      for (char enc_id = 0; enc_id < 24; enc_id++) MySetSurface_UpdateEncoder(enc_id);

      */

      // Update automation modes
      MySetSurface_UpdateAutoLEDs();

    } else
    {
      myblink_ctr++;
    }


    // stick
    if (stick_ctr > STICKINTV)
    {
      OnJoystick();
      stick_ctr = 0;
    } else
    {
      stick_ctr++;
    }

    // init
    if (!s_initdone) s_initdone = MySetSurface_Init();
  } // Run



}; // class CSurf_US2400



////// REGISTRATION, SETUP AND CONFIGURATION DIALOGS //////

static void parseParms(const char *str, int parms[4])
{
  parms[0]=0;
  parms[1]=9;
  parms[2]=parms[3]=-1;

  const char *p=str;
  if (p)
  {
    int x=0;
    while (x<4)
    {
      while (*p == ' ') p++;
      if ((*p < '0' || *p > '9') && *p != '-') break;
      parms[x++]=atoi(p);
      while (*p && *p != ' ') p++;
    }
  }  
}


static IReaperControlSurface *createFunc(const char *type_string, const char *configString, int *errStats)
{
  int parms[4];
  parseParms(configString,parms);

  return new CSurf_US2400(parms[2],parms[3],errStats);
}


static WDL_DLGRET dlgProc(HWND hwndDlg, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
  switch (uMsg)
  {
    case WM_INITDIALOG:
      {
        int parms[4];
        parseParms((const char *)lParam,parms);

        ShowWindow(GetDlgItem(hwndDlg,IDC_EDIT1),SW_HIDE);
        ShowWindow(GetDlgItem(hwndDlg,IDC_EDIT1_LBL),SW_HIDE);
        ShowWindow(GetDlgItem(hwndDlg,IDC_EDIT2),SW_HIDE);
        ShowWindow(GetDlgItem(hwndDlg,IDC_EDIT2_LBL),SW_HIDE);
        ShowWindow(GetDlgItem(hwndDlg,IDC_EDIT2_LBL2),SW_HIDE);

        int n=GetNumMIDIInputs();
        int x=SendDlgItemMessage(hwndDlg,IDC_COMBO2,CB_ADDSTRING,0,(LPARAM)"None");
        SendDlgItemMessage(hwndDlg,IDC_COMBO2,CB_SETITEMDATA,x,-1);
        x=SendDlgItemMessage(hwndDlg,IDC_COMBO3,CB_ADDSTRING,0,(LPARAM)"None");
        SendDlgItemMessage(hwndDlg,IDC_COMBO3,CB_SETITEMDATA,x,-1);
        for (x = 0; x < n; x ++)
        {
          char buf[512];
          if (GetMIDIInputName(x,buf,sizeof(buf)))
          {
            int a=SendDlgItemMessage(hwndDlg,IDC_COMBO2,CB_ADDSTRING,0,(LPARAM)buf);
            SendDlgItemMessage(hwndDlg,IDC_COMBO2,CB_SETITEMDATA,a,x);
            if (x == parms[2]) SendDlgItemMessage(hwndDlg,IDC_COMBO2,CB_SETCURSEL,a,0);
          }
        }
        n=GetNumMIDIOutputs();
        for (x = 0; x < n; x ++)
        {
          char buf[512];
          if (GetMIDIOutputName(x,buf,sizeof(buf)))
          {
            int a=SendDlgItemMessage(hwndDlg,IDC_COMBO3,CB_ADDSTRING,0,(LPARAM)buf);
            SendDlgItemMessage(hwndDlg,IDC_COMBO3,CB_SETITEMDATA,a,x);
            if (x == parms[3]) SendDlgItemMessage(hwndDlg,IDC_COMBO3,CB_SETCURSEL,a,0);
          }
        }
      }
    break;
    case WM_USER+1024:
      if (wParam > 1 && lParam)
      {
        char tmp[512];

        int indev=-1, outdev=-1, offs=0, size=9;
        int r=SendDlgItemMessage(hwndDlg,IDC_COMBO2,CB_GETCURSEL,0,0);
        if (r != CB_ERR) indev = SendDlgItemMessage(hwndDlg,IDC_COMBO2,CB_GETITEMDATA,r,0);
        r=SendDlgItemMessage(hwndDlg,IDC_COMBO3,CB_GETCURSEL,0,0);
        if (r != CB_ERR)  outdev = SendDlgItemMessage(hwndDlg,IDC_COMBO3,CB_GETITEMDATA,r,0);

        sprintf(tmp,"0 0 %d %d",indev,outdev);
        lstrcpyn((char *)lParam, tmp,wParam);       
      }
    break;
  }
  return 0;
}


static HWND configFunc(const char *type_string, HWND parent, const char *initConfigString)
{
  return CreateDialogParam(g_hInst, MAKEINTRESOURCE(IDD_SURFACEEDIT_MCU), parent, dlgProc, (LPARAM) initConfigString);
}


reaper_csurf_reg_t csurf_us2400_reg = 
{
  "US-2400",
  "Tascam US-2400",
  createFunc,
  configFunc,
};