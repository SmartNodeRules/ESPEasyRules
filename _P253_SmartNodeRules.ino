//#######################################################################################################
//#################################### Plugin 253: MSGBus ###############################################
//#######################################################################################################

#define PLUGIN_253
#define PLUGIN_ID_253         253
#define PLUGIN_NAME_253       "SmartNodeRules"
#define PLUGIN_VALUENAME1_253 "Value"

#define PLUGIN_CUSTOM_CALL_253  253
#define P253_UNIT_MAX            32    // max buffer size for peer message bus nodes
                                       // They can announce themselves at topic MSGBUS/Hostname=xxxx

WiFiUDP P253_portUDP;

// Custom settings for this plugin
struct P253_SettingsStruct
{
  char Group[26]; // Group name that this node belongs to
} P253_Settings;

// We maintain a dedicated node list of all SmartNodeRules devices
struct P253_NodeStruct
{
  byte IP[4];
  byte age;
  String nodeName;
  String group;
} P253_Nodes[P253_UNIT_MAX];

String P253_printWebString = "";

boolean Plugin_253(byte function, struct EventStruct *event, String& string)
{
  boolean success = false;
  static boolean init = false;
  static int counter = 0;

  switch (function)
  {
    case PLUGIN_DEVICE_ADD:
      {
        Device[++deviceCount].Number = PLUGIN_ID_253;
        Device[deviceCount].Type = DEVICE_TYPE_DUMMY;
        Device[deviceCount].VType = SENSOR_TYPE_SINGLE;
        Device[deviceCount].Ports = 0;
        Device[deviceCount].PullUpOption = false;
        Device[deviceCount].InverseLogicOption = false;
        Device[deviceCount].FormulaOption = false;
        Device[deviceCount].DecimalsOnly = true;
        Device[deviceCount].ValueCount = 4;
        Device[deviceCount].SendDataOption = true;
        Device[deviceCount].TimerOption = true;
        Device[deviceCount].GlobalSyncOption = true;
        break;
      }

    case PLUGIN_GET_DEVICENAME:
      {
        string = F(PLUGIN_NAME_253);
        break;
      }

    case PLUGIN_WEBFORM_LOAD:
      {
        byte choice = Settings.TaskDevicePluginConfig[event->TaskIndex][0];
        String options[11];
        options[0] = F("SENSOR_TYPE_SINGLE");
        options[1] = F("SENSOR_TYPE_TEMP_HUM");
        options[2] = F("SENSOR_TYPE_TEMP_BARO");
        options[3] = F("SENSOR_TYPE_TEMP_HUM_BARO");
        options[4] = F("SENSOR_TYPE_DUAL");
        options[5] = F("SENSOR_TYPE_TRIPLE");
        options[6] = F("SENSOR_TYPE_QUAD");
        options[7] = F("SENSOR_TYPE_SWITCH");
        options[8] = F("SENSOR_TYPE_DIMMER");
        options[9] = F("SENSOR_TYPE_LONG");
        options[10] = F("SENSOR_TYPE_WIND");
        int optionValues[11];
        optionValues[0] = SENSOR_TYPE_SINGLE;
        optionValues[1] = SENSOR_TYPE_TEMP_HUM;
        optionValues[2] = SENSOR_TYPE_TEMP_BARO;
        optionValues[3] = SENSOR_TYPE_TEMP_HUM_BARO;
        optionValues[4] = SENSOR_TYPE_DUAL;
        optionValues[5] = SENSOR_TYPE_TRIPLE;
        optionValues[6] = SENSOR_TYPE_QUAD;
        optionValues[7] = SENSOR_TYPE_SWITCH;
        optionValues[8] = SENSOR_TYPE_DIMMER;
        optionValues[9] = SENSOR_TYPE_LONG;
        optionValues[10] = SENSOR_TYPE_WIND;
        addFormSelector(F("Simulate Data Type"), F("plugin_253_sensortype"), 11, options, optionValues, choice );

        char deviceTemplate[4][80];
        LoadCustomTaskSettings(event->TaskIndex, (byte*)&deviceTemplate, sizeof(deviceTemplate));
        for (byte varNr = 0; varNr < 4; varNr++)
        {
          addHtml(F("<TR><TD>Line "));
          addHtml(String(varNr + 1));
          addHtml(F(":<TD><input type='text' size='80' maxlength='80' name='Plugin_253_template"));
          addHtml(String(varNr + 1));
          addHtml(F("' value='"));
          addHtml(deviceTemplate[varNr]);
          addHtml(F("'>"));
        }
        success = true;
        break;
      }

    case PLUGIN_WEBFORM_SAVE:
      {
        Settings.TaskDevicePluginConfig[event->TaskIndex][0] = getFormItemInt(F("plugin_253_sensortype"));

        char deviceTemplate[4][80];
        for (byte varNr = 0; varNr < 4; varNr++)
        {
          char argc[25];
          String arg = F("Plugin_253_template");
          arg += varNr + 1;
          arg.toCharArray(argc, 25);
          String tmpString = WebServer.arg(argc);
          strncpy(deviceTemplate[varNr], tmpString.c_str(), sizeof(deviceTemplate[varNr]));
        }
        SaveCustomTaskSettings(event->TaskIndex, (byte*)&deviceTemplate, sizeof(deviceTemplate));
        success = true;
        break;
      }

    case PLUGIN_INIT:
      {
        success = true;
        break;
      }

    case PLUGIN_WRITE:
      {
        String command = parseString(string, 1);

        if (command == F("config"))
        {
          String setting = P253_parseString(string,2,',');
          String strP1 = P253_parseString(string,3,',');
          if (setting.equalsIgnoreCase(F("Group"))){
            strcpy(P253_Settings.Group, strP1.c_str());
          }          
          success = true;
        }

        if (command == F("webrootredirect"))
        {
          success = true;
          WebServer.on("/", P253_handle_root);
        }

        if (command == F("webprint"))
        {
          success = true;
          if (string.length() == 8)
            P253_printWebString = "";
          else
            P253_printWebString += string.substring(9);
        }

        if (command == F("webbutton"))
        {
           success = true;
           P253_printWebString += F("<a class=\"");
           P253_printWebString += P253_parseString(string,2,';');
           P253_printWebString += F("\" href=\"");
           P253_printWebString += P253_parseString(string,3,';');
           P253_printWebString += F("\">");
           P253_printWebString += P253_parseString(string,4,';');
           P253_printWebString += F("</a>");
        }
        
        if (command == F("msgbus"))
        {
          success = true;
          String msg = string.substring(7);
          P253_UDPSend(msg);
        }
       
        break;
      }

    case PLUGIN_UNCONDITIONAL_POLL: // runs 10 per second
      {
        if (wifiStatus != ESPEASY_WIFI_DISCONNECTED) {

          if (!init){
            String log = F("Message Bus init");
            addLog(LOG_LEVEL_INFO, log);
            // Start listening to msgbus messages
            P253_portUDP.begin(65501);

            // Add myself to the msgbus node list
            IPAddress IP = WiFi.localIP();
            for (byte x = 0; x < 4; x++)
              P253_Nodes[0].IP[x] = IP[x];
            P253_Nodes[0].age = 0;
            P253_Nodes[0].nodeName = Settings.Name;
            P253_Nodes[0].group = P253_Settings.Group;


            // Get others to announce themselves so i can update my node list
            String msg = "MSGBUS/Refresh";
            P253_UDPSend(msg);
            init = true;
          }

          P253_MSGBusReceive();
          counter++;
          if (counter == 600) { // one minute
            String msg = F("MSGBUS/Hostname=");
            msg += Settings.Name;
            msg += ",0.0.0.0,";
            msg += P253_Settings.Group;
            P253_UDPSend(msg);
            P253_refreshNodeList();
            counter = 0;
          }
        }
        break;
      }
      
    case PLUGIN_TEN_PER_SECOND:
      {
        break;
      }
      
    case PLUGIN_ONCE_A_SECOND:
      {
        break;
      }

    case PLUGIN_READ:
      {
        event->sensorType = Settings.TaskDevicePluginConfig[event->TaskIndex][0];
        for (byte x = 0; x < 4; x++)
        {
          String log = F("MSGBS: value ");
          log += x + 1;
          log += F(": ");
          log += UserVar[event->BaseVarIndex + x];
          addLog(LOG_LEVEL_INFO, log);
        }
        success = true;
        break;
      }

    case PLUGIN_CUSTOM_CALL_253:
      {
        char deviceTemplate[4][80];
        LoadCustomTaskSettings(event->TaskIndex, (byte*)&deviceTemplate, sizeof(deviceTemplate));
        for (byte varNr = 0; varNr < 4; varNr++)
        {
          String match = deviceTemplate[varNr];
          if (match.length() > 0) {
            String tmpString = string.substring(0, match.length());
            if (tmpString.equals(match)) {
              String log = "MSGBS: Captured: " + string;
              addLog(LOG_LEVEL_INFO, log);
              String value = string.substring(match.length());
              UserVar[event->TaskIndex * VARS_PER_TASK + varNr] = value.toFloat();
            }
          }
        }
        break;
      }
  }
  return success;
}


//********************************************************************************************
// UDP receive message bus packets
//********************************************************************************************
void P253_MSGBusReceive() {
  int packetSize = P253_portUDP.parsePacket();
  if (packetSize)
  {
    IPAddress remoteIP = P253_portUDP.remoteIP();
    char packetBuffer[packetSize + 1];
    int len = P253_portUDP.read(packetBuffer, packetSize);

    // check if this is a plain text message, do not process other messages
    if (packetBuffer[0] > 127)
      return;

    packetBuffer[packetSize] = 0;
    addLog(LOG_LEVEL_DEBUG, packetBuffer);
    String msg = packetBuffer;

    // First process messages that request confirmation
    // These messages start with '>' and must be addressed to my node name
    String mustConfirm = String(">") + Settings.Name + String("/");
    if (msg.startsWith(mustConfirm)) {
      String reply = "<" + msg.substring(1);
      P253_UDPSend(reply);
    }
    if (msg[0] == '>'){
     msg = msg.substring(1); // Strip the '>' request token from the message
    }

    // Special MSGBus system events
    if (msg.substring(0, 7) == F("MSGBUS/")) {
      String sysMSG = msg.substring(7);
      if (sysMSG.substring(0, 9) == F("Hostname=")) {
        String params = sysMSG.substring(9);
        String hostName = P253_parseString(params, 1,',');
        //String ip = P253_parseString(params, 2,','); we just take the remote ip here
        String group = P253_parseString(params, 3,',');
        P253_nodelist(remoteIP, hostName, group);
      }
      if (sysMSG.substring(0, 7) == F("Refresh")) {
//        MSGBusAnnounceMe();
      }
    }

    // Create event for this message
    rulesProcessing(msg);

    // Run through tasks for MSGBus sensor updates
    for (byte y = 0; y < TASKS_MAX; y++)
    {
      if (Settings.TaskDeviceEnabled[y] && Settings.TaskDeviceNumber[y] != 0)
      {
        struct EventStruct TempEvent;
        byte DeviceIndex = getDeviceIndex(Settings.TaskDeviceNumber[y]);
        TempEvent.TaskIndex = y;
        TempEvent.BaseVarIndex = y * VARS_PER_TASK;
        TempEvent.sensorType = Device[DeviceIndex].VType;

        for (byte x = 0; x < PLUGIN_MAX; x++)
        {
          if (validPluginID(DeviceIndex_to_Plugin_id[x]))          
          //if (Plugin_id[x] == Settings.TaskDeviceNumber[y])
          {
            Plugin_ptr[x](PLUGIN_CUSTOM_CALL_253, &TempEvent, msg);
          }
        }
      }
    }
  }
}


//********************************************************************************************
//  UDP Send message bus packet
//********************************************************************************************
void P253_UDPSend(String message)
{
  IPAddress broadcastIP(255, 255, 255, 255);
  P253_portUDP.beginPacket(broadcastIP, 65501);
  P253_portUDP.print(message);
  P253_portUDP.endPacket();
}


//********************************************************************************************
//  Maintain node list
//********************************************************************************************
void P253_nodelist(IPAddress remoteIP, String hostName, String group) {

  boolean found = false;
  if(group.length()==0)
    group = "-";
  for (byte x = 0; x < P253_UNIT_MAX; x++) {
    if (P253_Nodes[x].nodeName == hostName) {
      P253_Nodes[x].group = group;
      for (byte y = 0; y < 4; y++)
        P253_Nodes[x].IP[y] = remoteIP[y];
      P253_Nodes[x].age = 0;
      found = true;
      break;
    }
  }
  if (!found) {
    for (byte x = 0; x < P253_UNIT_MAX; x++) {
      if (P253_Nodes[x].IP[0] == 0) {
        P253_Nodes[x].nodeName = hostName;
        P253_Nodes[x].group = group;
        for (byte y = 0; y < 4; y++)
          P253_Nodes[x].IP[y] = remoteIP[y];
        P253_Nodes[x].age = 0;
        break;
      }
    }
  }
}


//*********************************************************************************************
//   Refresh aging for remote units, drop if too old...
//*********************************************************************************************
void P253_refreshNodeList()
{
  // start at 1, 0 = myself and does not age...
  for (byte counter = 1; counter < P253_UNIT_MAX; counter++)
  {
    if (P253_Nodes[counter].IP[0] != 0)
    {
      P253_Nodes[counter].age++;  // increment age counter
      if (P253_Nodes[counter].age > 10) // if entry to old, clear this node ip from the list.
        for (byte x = 0; x < 4; x++)
          P253_Nodes[counter].IP[x] = 0;
    }
  }
}


//********************************************************************************
// Web page header
//********************************************************************************
void P253_addHeader(boolean showMenu, String & reply) {
  reply += F("<meta name=\"viewport\" content=\"width=width=device-width, initial-scale=1\">");
  reply += F("<STYLE>* {font-family:sans-serif; font-size:12pt;}");
  reply += F("h1 {font-size: 16pt; color: #07D; margin: 8px 0; font-weight: bold;}");
  reply += F(".button {margin:4px; padding:5px 15px; background-color:#07D; color:#FFF; text-decoration:none; border-radius:4px}");
  reply += F(".button-link {padding:5px 15px; background-color:#07D; color:#FFF; border:solid 1px #FFF; text-decoration:none}");
  reply += F(".button-nodelink {display: inline-block; width: 100%; text-align: center; padding:5px 15px; background-color:#888; color:#FFF; border:solid 1px #FFF; text-decoration:none}");
  reply += F(".button-nodelinkA {display: inline-block; width: 100%; text-align: center; padding:5px 15px; background-color:#28C; color:#FFF; border:solid 1px #FFF; text-decoration:none}");
  reply += F("td {padding:7px;}");
  reply += F("</STYLE>");
  reply += F("<h1>");
  reply += Settings.Name;
  reply += F("</h1>");  
  reply += F("<a class=\"button-link\" href=\"/\">Main</a>");
  reply += F("<a class=\"button-link\" href=\"/config\">Config</a>");
  reply += F("<a class=\"button-link\" href=\"/rules\">Rules</a>");
  reply += F("<a class=\"button-link\" href=\"/tools\">Tools</a>");
  reply += F("<BR><BR>");
}


//********************************************************************************
// Web Interface custom root page
//********************************************************************************
byte P253_sortedIndex[P253_UNIT_MAX + 1];
void P253_handle_root() {

  String sCommand = WebServer.arg(F("cmd"));
  String group = WebServer.arg("group");
  boolean groupList = true;

  if (group != "")
    groupList = false;
  
  if (strcasecmp_P(sCommand.c_str(), PSTR("reboot")) != 0)
  {
    String reply = "";
    P253_addHeader(true, reply);

    if (sCommand.length() > 0)
    {
      String event = sCommand.substring(6);
      rulesProcessing(event);
      event = F("Web#Print");
      rulesProcessing(event);
    }
   
    reply += P253_printWebString;

    IPAddress ip = WiFi.localIP();
    reply += "";
    reply += F("<form>");
    reply += F("<table>");

    // first get the list in alphabetic order
    for (byte x = 0; x < P253_UNIT_MAX; x++)
      P253_sortedIndex[x] = x;

    if (groupList == true) {
      // Show Group list
      P253_sortDeviceArrayGroup(); // sort on groupname
      String prevGroup = "?";
      for (byte x = 0; x < P253_UNIT_MAX; x++)
      {
        byte index = P253_sortedIndex[x];
        if (P253_Nodes[index].IP[0] != 0) {
          String group = P253_Nodes[index].group;
          if (group != prevGroup)
          {
            prevGroup = group;
            reply += F("<TR><TD><a class=\"");
            reply += F("button-nodelink");
            reply += F("\" ");
            reply += F("href='/?group=");
            reply += group;
            reply += "'>";
            reply += group;
            reply += F("</a>");
            reply += F("<TD>");
          }
        }
      }
      // All nodes group button
      reply += F("<TR><TD><a class=\"button-nodelink\" href='/?group=*'>_ALL_</a><TD>");
    }
    else {
      // Show Node list
      P253_sortDeviceArray();  // sort on nodename
      for (byte x = 0; x < P253_UNIT_MAX; x++)
      {
        byte index = P253_sortedIndex[x];
        if (P253_Nodes[index].IP[0] != 0 && (group == "*" || P253_Nodes[index].group == group))
        {
          String buttonclass ="";
          if ((String)Settings.Name == P253_Nodes[index].nodeName)
            buttonclass = F("button-nodelinkA");
          else
            buttonclass = F("button-nodelink");
          reply += F("<TR><TD><a class=\"");
          reply += buttonclass;
          reply += F("\" ");
          char url[40];
          sprintf_P(url, PSTR("href='http://%u.%u.%u.%u"), P253_Nodes[index].IP[0], P253_Nodes[index].IP[1], P253_Nodes[index].IP[2], P253_Nodes[index].IP[3]);
          reply += url;
          if (group != "") {
            reply += F("?group=");
            reply += P253_Nodes[index].group;
          }
          reply += "'>";
          reply += P253_Nodes[index].nodeName;
          reply += F("</a>");
          reply += F("<TD>");
        }
      }
    }
    reply += "</table></form>";
    WebServer.send(200, "text/html", reply);
  }
  else
  {
    // have to disconnect or reboot from within the main loop
    // because the webconnection is still active at this point
    // disconnect here could result into a crash/reboot...
    if (strcasecmp_P(sCommand.c_str(), PSTR("reboot")) == 0)
    {
      cmd_within_mainloop = CMD_REBOOT;
    }

    WebServer.send(200, "text/html", "OK");
  }
}


//********************************************************************************
// Device Sort routine, switch array entries
//********************************************************************************
void P253_switchArray(byte value)
{
  byte temp;
  temp = P253_sortedIndex[value - 1];
  P253_sortedIndex[value - 1] = P253_sortedIndex[value];
  P253_sortedIndex[value] = temp;
}


//********************************************************************************
// Device Sort routine, compare two array entries
//********************************************************************************
boolean P253_arrayLessThan(const String& ptr_1, const String& ptr_2)
{
  unsigned int i = 0;
  while (i < ptr_1.length())    // For each character in string 1, starting with the first:
  {
    if (ptr_2.length() < i)    // If string 2 is shorter, then switch them
    {
      return true;
    }
    else
    {
      const char check1 = (char)ptr_1[i];  // get the same char from string 1 and string 2
      const char check2 = (char)ptr_2[i];
      if (check1 == check2) {
        // they're equal so far; check the next char !!
        i++;
      } else {
        return (check2 > check1);
      }
    }
  }
  return false;
}


//********************************************************************************
// Device Sort routine, actual sorting
//********************************************************************************
void P253_sortDeviceArray()
{
  int innerLoop ;
  int mainLoop ;
  String one,two;
  for ( mainLoop = 1; mainLoop < P253_UNIT_MAX; mainLoop++)
  {
    innerLoop = mainLoop;
    while (innerLoop  >= 1)
    {
      one = P253_Nodes[P253_sortedIndex[innerLoop]].nodeName;
      two = P253_Nodes[P253_sortedIndex[innerLoop-1]].nodeName;
      if (P253_arrayLessThan(one, two))
      {
        P253_switchArray(innerLoop);
      }
      innerLoop--;
    }
  }
}


//********************************************************************************
// Device Sort routine, actual sorting
//********************************************************************************
void P253_sortDeviceArrayGroup()
{
  int innerLoop ;
  int mainLoop ;
  for ( mainLoop = 1; mainLoop < P253_UNIT_MAX; mainLoop++)
  {
    innerLoop = mainLoop;
    while (innerLoop  >= 1)
    {
      String one = P253_Nodes[P253_sortedIndex[innerLoop]].group;
      if(one.length()==0) one = "_";
      String two = P253_Nodes[P253_sortedIndex[innerLoop - 1]].group;
      if(two.length()==0) two = "_";
      if (P253_arrayLessThan(one, two))
      {
        P253_switchArray(innerLoop);
      }
      innerLoop--;
    }
  }
}


//*********************************************************************************************
//  Parse a string and get the xth command or parameter
//*********************************************************************************************
String P253_parseString(String& string, byte indexFind, char separator)
{
  String tmpString = string;
  tmpString += separator;
  tmpString.replace(" ", (String)separator);
  String locateString = "";
  byte count = 0;
  int index = tmpString.indexOf(separator);
  while (index > 0)
  {
    count++;
    locateString = tmpString.substring(0, index);
    tmpString = tmpString.substring(index + 1);
    index = tmpString.indexOf(separator);
    if (count == indexFind)
    {
      return locateString;
    }
  }
  return "";
}


