/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/*
 * Copyright (c) 2007 INRIA
 *               2009,2010 Contributors
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation;
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 * Author: Mathieu Lacage <mathieu.lacage@sophia.inria.fr>
 * Contributors: Thomas Waldecker <twaldecker@rocketmail.com>
 *               Martín Giachino <martin.giachino@gmail.com>
 *
 * Brief description: Implementation of a ns2 movement trace file reader.
 *
 * This implementation is based on the ns2 movement documentation of ns2
 * as described in http://www.isi.edu/nsnam/ns/doc/node174.html
 *
 * Valid trace files use the following ns2 statements:
 *
 * $node set X_ x1
 * $node set Y_ y1
 * $node set Z_ z1
 * $ns at $time $node setdest x2 y2 speed
 * $ns at $time $node set X_ x1
 * $ns at $time $node set Y_ Y1
 * $ns at $time $node set Z_ Z1
 *
 */


#include <fstream>
#include <sstream>
#include <map>
#include "ns3/log.h"
#include "ns3/simulator.h"
#include "ns3/node-list.h"
#include "ns3/node.h"
#include "ns3/constant-velocity-mobility-model.h"
#include "ns2-mobility-helper.h"

#ifdef WIN32
#include "winport.h"
#endif

NS_LOG_COMPONENT_DEFINE ("Ns2MobilityHelper");

using namespace std;

namespace ns3 {

// Constants definitions
#define  NS2_AT       "at"
#define  NS2_X_COORD  "X_"
#define  NS2_Y_COORD  "Y_"
#define  NS2_Z_COORD  "Z_"
#define  NS2_SETDEST  "setdest"
#define  NS2_SET      "set"
#define  NS2_NODEID   "$node_("
#define  NS2_NS_SCH   "$ns_"


// Type to maintain line parsed and its values
struct ParseResult
{
  vector<string> tokens; // tokens from a line
  vector<int> ivals;     // int values for each tokens
  vector<bool> has_ival; // points if a tokens has an int value
  vector<double> dvals;  // double values for each tokens
  vector<bool> has_dval; // points if a tokens has a double value
  vector<string> svals;  // string value for each token
};


// Parses a line of ns2 mobility
static ParseResult ParseNs2Line (const string& str);

// Put out blank spaces at the start and end of a line
static string TrimNs2Line (const string& str);

// Checks if a string represents a number or it has others characters than digits an point.
static bool IsNumber (const string& s);

// Check if s string represents a numeric value
template<class T>
static bool IsVal (const string& str, T& ret);

// Checks if the value between brackets is a correct nodeId number
static bool HasNodeIdNumber (string str);

// Gets nodeId number in string format from the string like $node_(4)
static string GetNodeIdFromToken (string str);

// Get node id number in int format
static int GetNodeIdInt (ParseResult pr);

// Get node id number in string format
static string GetNodeIdString (ParseResult pr);

// Add one coord to a vector position
static Vector SetOneInitialCoord (Vector actPos, string& coord, double value);

// Check if this corresponds to a line like this: $node_(0) set X_ 123
static bool IsSetInitialPos (ParseResult pr);

// Check if this corresponds to a line like this: $ns_ at 1 "$node_(0) setdest 2 3 4"
static bool IsSchedSetPos (ParseResult pr);

// Check if this corresponds to a line like this: $ns_ at 1 "$node_(0) set X_ 2"
static bool IsSchedMobilityPos (ParseResult pr);

// Set waypoints and speed for movement.
static Vector SetMovement (Ptr<ConstantVelocityMobilityModel> model, Vector lastPos, double at,
                           double xFinalPosition, double yFinalPosition, double speed);

// Set initial position for a node
static Vector SetInitialPosition (Ptr<ConstantVelocityMobilityModel> model, string coord, double coordVal);

// Schedule a set of position for a node
static Vector SetSchedPosition (Ptr<ConstantVelocityMobilityModel> model, double at, string coord, double coordVal);


Ns2MobilityHelper::Ns2MobilityHelper (std::string filename)
  : m_filename (filename)
{
}

Ptr<ConstantVelocityMobilityModel>
Ns2MobilityHelper::GetMobilityModel (std::string idString, const ObjectStore &store) const
{
  std::istringstream iss;
  iss.str (idString);
  uint32_t id (0);
  iss >> id;
  Ptr<Object> object = store.Get (id);
  if (object == 0)
    {
      return 0;
    }
  Ptr<ConstantVelocityMobilityModel> model = object->GetObject<ConstantVelocityMobilityModel> ();
  if (model == 0)
    {
      model = CreateObject<ConstantVelocityMobilityModel> ();
      object->AggregateObject (model);
    }
  return model;
}


void
Ns2MobilityHelper::ConfigNodesMovements (const ObjectStore &store) const
{
  map<int, Vector> last_pos;    // Vector containing lasts positions for each node

  std::ifstream file (m_filename.c_str (), std::ios::in);
  if (file.is_open ())
    {
      while (!file.eof () )
        {
          int         iNodeId = 0;
          std::string nodeId;
          std::string line;

          getline (file, line);

          // ignore empty lines
          if (line.empty ())
            {
              continue;
            }

          ParseResult pr = ParseNs2Line (line); // Parse line and obtain tokens

          // Check if the line corresponds with one of the three types of line
          if (pr.tokens.size () != 4 && pr.tokens.size () != 7 && pr.tokens.size () != 8)
            {
              NS_LOG_ERROR ("Line has not correct number of parameters (corrupted file?): " << line << "\n");
              continue;
            }

          // Get the node Id
          nodeId  = GetNodeIdString (pr);
          iNodeId = GetNodeIdInt (pr);
          if (iNodeId == -1)
            {
              NS_LOG_ERROR ("Node number couldn't be obtained (corrupted file?): " << line << "\n");
              continue;
            }

          // get mobility model of node
          Ptr<ConstantVelocityMobilityModel> model = GetMobilityModel (nodeId,store);

          // if model not exists, continue
          if (model == 0)
            {
              NS_LOG_ERROR ("Unknown node ID (corrupted file?): " << nodeId << "\n");
              continue;
            }


          /*
           * In this case a initial position is being seted
           * line like $node_(0) set X_ 151.05190721688197
           */
          if (IsSetInitialPos (pr))
            {
              //                                            coord         coord value
              last_pos[iNodeId] = SetInitialPosition (model, pr.tokens[2], pr.dvals[3]);

              // Log new position
              NS_LOG_DEBUG ("Positions after parse for node " << iNodeId << " " << nodeId <<
                            " x=" << last_pos[iNodeId].x << " y=" << last_pos[iNodeId].y << " z=" << last_pos[iNodeId].z);
            }

          else // NOW EVENTS TO BE SCHEDULED
            {

              // This is a scheduled event, so time at should be present
              double at;

              if (!IsNumber (pr.tokens[2]))
                {
                  NS_LOG_WARN ("Time is not a number: " << pr.tokens[2]);
                  continue;
                }

              at = pr.dvals[2]; // set time at

              if ( at < 0 )
                {
                  NS_LOG_WARN ("Time is less than cero: " << at);
                  continue;
                }



              /*
               * In this case a new waypoint is added
               * line like $ns_ at 1 "$node_(0) setdest 2 3 4"
               */
              if (IsSchedMobilityPos (pr))
                {
                  //                                     last position     time  X coord     Y coord      velocity
                  last_pos[iNodeId] = SetMovement (model, last_pos[iNodeId], at, pr.dvals[5], pr.dvals[6], pr.dvals[7]);

                  // Log new position
                  NS_LOG_DEBUG ("Positions after parse for node " << iNodeId << " " << nodeId <<
                                " x=" << last_pos[iNodeId].x << " y=" << last_pos[iNodeId].y << " z=" << last_pos[iNodeId].z);
                }


              /*
               * Scheduled set position
               * line like $ns_ at 4.634906291962 "$node_(0) set X_ 28.675920486450"
               */
              else if (IsSchedSetPos (pr))
                {
                  //                                         time  coordinate   coord value
                  last_pos[iNodeId] = SetSchedPosition (model, at, pr.tokens[5], pr.dvals[6]);

                  // Log new position
                  NS_LOG_DEBUG ("Positions after parse for node " << iNodeId << " " << nodeId <<
                                " x=" << last_pos[iNodeId].x << " y=" << last_pos[iNodeId].y << " z=" << last_pos[iNodeId].z);
                }
              else
                {
                  NS_LOG_WARN ("Format Line is not correct: " << line << "\n");
                }
            }
        }
      file.close ();
    }
}


ParseResult
ParseNs2Line (const string& str)
{
  ParseResult ret;
  istringstream s;
  string line;

  // ignore comments (#)
  size_t pos_sharp = str.find_first_of ('#');
  if (pos_sharp != string::npos)
    {
      line = str.substr (0, pos_sharp);
    }
  else
    {
      line = str;
    }

  line = TrimNs2Line (line);

  // If line hasn't a correct node Id
  if (!HasNodeIdNumber (line))
    {
      NS_LOG_WARN ("Line has no node Id: " << line);
      return ret;
    }

  s.str (line);

  while (!s.eof ())
    {
      string x;
      s >> x;
      ret.tokens.push_back (x);
      int ii (0);
      double d (0);
      if (HasNodeIdNumber (x))
        {
          x = GetNodeIdFromToken (x);
        }
      ret.has_ival.push_back (IsVal<int> (x, ii));
      ret.ivals.push_back (ii);
      ret.has_dval.push_back (IsVal<double> (x, d));
      ret.dvals.push_back (d);
      ret.svals.push_back (x);
    }

  size_t tokensLength   = ret.tokens.size ();                 // number of tokens in line
  size_t lasTokenLength = ret.tokens[tokensLength - 1].size (); // length of the last token

  // if it is a scheduled set _[XYZ] or a setdest I need to remove the last "
  // and re-calculate values
  if ( (tokensLength == 7 || tokensLength == 8)
       && (ret.tokens[tokensLength - 1][lasTokenLength - 1] == '"') )
    {

      // removes " from the last position
      ret.tokens[tokensLength - 1] = ret.tokens[tokensLength - 1].substr (0,lasTokenLength - 1);

      string x;
      x = ret.tokens[tokensLength - 1];

      if (HasNodeIdNumber (x))
        {
          x = GetNodeIdFromToken (x);
        }

      // Re calculate values
      int ii (0);
      double d (0);
      ret.has_ival[tokensLength - 1] = IsVal<int> (x, ii);
      ret.ivals[tokensLength - 1] = ii;
      ret.has_dval[tokensLength - 1] = IsVal<double> (x, d);
      ret.dvals[tokensLength - 1] = d;
      ret.svals[tokensLength - 1] = x;

    }
  else if ( (tokensLength == 9 && ret.tokens[tokensLength - 1] == "\"")
            || (tokensLength == 8 && ret.tokens[tokensLength - 1] == "\""))
    {
      // if the line has the " character in this way: $ns_ at 1 "$node_(0) setdest 2 2 1  "
      // or in this: $ns_ at 4 "$node_(0) set X_ 2  " we need to ignore this last token

      ret.tokens.erase (ret.tokens.begin () + tokensLength - 1);
      ret.has_ival.erase (ret.has_ival.begin () + tokensLength - 1);
      ret.ivals.erase (ret.ivals.begin () + tokensLength - 1);
      ret.has_dval.erase (ret.has_dval.begin () + tokensLength - 1);
      ret.dvals.erase (ret.dvals.begin () + tokensLength - 1);
      ret.svals.erase (ret.svals.begin () + tokensLength - 1);

    }



  return ret;
}


string
TrimNs2Line (const string& s)
{
  string ret = s;

  while (ret.size () > 0 && isblank (ret[0]))
    {
      ret.erase (0, 1);    // Removes blank spaces at the begining of the line
    }

  while (ret.size () > 0 && isblank (ret[ret.size () - 1]))
    {
      ret.erase (ret.size () - 1, 1); // Removes blank spaces from at end of line
    }

  return ret;
}


bool
IsNumber (const string& s)
{
  char *endp;
  double v = strtod (s.c_str (), &endp); // declared with warn_unused_result
  //cast v to void, to suppress v set but not used compiler warning
  (void) v;
  return endp == s.c_str () + s.size ();
}


template<class T>
bool IsVal (const string& str, T& ret)
{
  if (str.size () == 0)
    {
      return false;
    }
  else if (IsNumber (str))
    {
      string s2 = str;
      istringstream s (s2);
      s >> ret;
      return true;
    }
  else
    {
      return false;
    }
}


bool
HasNodeIdNumber (string str)
{

  // find brackets
  std::string::size_type startNodeId = str.find_first_of ("("); // index of left bracket
  std::string::size_type endNodeId   = str.find_first_of (")"); // index of right bracket

  // Get de nodeId in a string and in a int value
  std::string nodeId;     // node id

  // if no brackets, continue!
  if (startNodeId == std::string::npos || endNodeId == std::string::npos)
    {
      return false;
    }

  nodeId = str.substr (startNodeId + 1, endNodeId - (startNodeId + 1)); // set node id

  //   is number              is integer                                       is not negative
  if (IsNumber (nodeId) && (nodeId.find_first_of (".") == std::string::npos) && (nodeId[0] != '-'))
    {
      return true;
    }
  else
    {
      return false;
    }
}


string
GetNodeIdFromToken (string str)
{
  if (HasNodeIdNumber (str))
    {
      // find brackets
      std::string::size_type startNodeId = str.find_first_of ("(");     // index of left bracket
      std::string::size_type endNodeId   = str.find_first_of (")");     // index of right bracket

      return str.substr (startNodeId + 1, endNodeId - (startNodeId + 1)); // set node id
    }
  else
    {
      return "";
    }
}


int
GetNodeIdInt (ParseResult pr)
{
  int result = -1;
  switch (pr.tokens.size ())
    {
    case 4:   // line like $node_(0) set X_ 11
      result = pr.ivals[0];
      break;
    case 7:   // line like $ns_ at 4 "$node_(0) set X_ 28"
      result = pr.ivals[3];
      break;
    case 8:   // line like $ns_ at 1 "$node_(0) setdest 2 3 4"
      result = pr.ivals[3];
      break;
    default:
      result = -1;
    }
  return result;
}

// Get node id number in string format
string
GetNodeIdString (ParseResult pr)
{
  switch (pr.tokens.size ())
    {
    case 4:   // line like $node_(0) set X_ 11
      return pr.svals[0];
      break;
    case 7:   // line like $ns_ at 4 "$node_(0) set X_ 28"
      return pr.svals[3];
      break;
    case 8:   // line like $ns_ at 1 "$node_(0) setdest 2 3 4"
      return pr.svals[3];
      break;
    default:
      return "";
    }
}


Vector
SetOneInitialCoord (Vector position, string& coord, double value)
{

  // set the position for the coord.
  if (coord == NS2_X_COORD)
    {
      position.x = value;
      NS_LOG_DEBUG ("X=" << value);
    }
  else if (coord == NS2_Y_COORD)
    {
      position.y = value;
      NS_LOG_DEBUG ("Y=" << value);
    }
  else if (coord == NS2_Z_COORD)
    {
      position.z = value;
      NS_LOG_DEBUG ("Z=" << value);
    }
  return position;
}


bool
IsSetInitialPos (ParseResult pr)
{
  //        number of tokens         has $node_( ?                        has "set"           has doble for position?
  return pr.tokens.size () == 4 && HasNodeIdNumber (pr.tokens[0]) && pr.tokens[1] == NS2_SET && pr.has_dval[3]
         // coord name is X_, Y_ or Z_ ?
         && (pr.tokens[2] == NS2_X_COORD || pr.tokens[2] == NS2_Y_COORD || pr.tokens[2] == NS2_Z_COORD);

}


bool
IsSchedSetPos (ParseResult pr)
{
  //      correct number of tokens,    has $ns_                   and at
  return pr.tokens.size () == 7 && pr.tokens[0] == NS2_NS_SCH && pr.tokens[1] == NS2_AT
         && pr.tokens[4] == NS2_SET && pr.has_dval[2] && pr.has_dval[3]   // has set and double value for time and nodeid
         && ( pr.tokens[5] == NS2_X_COORD || pr.tokens[5] == NS2_Y_COORD || pr.tokens[5] == NS2_Z_COORD) // has X_, Y_ or Z_?
         && pr.has_dval[2]; // time is a number
}

bool
IsSchedMobilityPos (ParseResult pr)
{
  //     number of tokens      and    has $ns_                and    has at
  return pr.tokens.size () == 8 && pr.tokens[0] == NS2_NS_SCH && pr.tokens[1] == NS2_AT
         //    time             x coord          y coord          velocity are numbers?
         && pr.has_dval[2] && pr.has_dval[5] && pr.has_dval[6] && pr.has_dval[7]
         && pr.tokens[4] == NS2_SETDEST; // and has setdest

}

Vector
SetMovement (Ptr<ConstantVelocityMobilityModel> model, Vector last_pos, double at,
             double xFinalPosition, double yFinalPosition, double speed)
{
  Vector position;
  position.x = last_pos.x;
  position.y = last_pos.y;
  position.z = last_pos.z;

  if (speed == 0)
    {
      // We have to maintain last position, and stop the movement
      Simulator::Schedule (Seconds (at), &ConstantVelocityMobilityModel::SetVelocity, model,
                           Vector (0, 0, 0));
    }
  else if (speed > 0)
    {
      // first calculate the time; time = distance / speed
      double time = sqrt (pow (xFinalPosition - position.x, 2) + pow (yFinalPosition - position.y, 2)) / speed;
      NS_LOG_DEBUG ("at=" << at << " time=" << time);
      // now calculate the xSpeed = distance / time
      double xSpeed = (xFinalPosition - position.x) / time;
      double ySpeed = (yFinalPosition - position.y) / time; // & same with ySpeed

      // quick and dirty set zSpeed = 0
      double zSpeed = 0;

      NS_LOG_DEBUG ("Calculated Speed: X=" << xSpeed << " Y=" << ySpeed << " Z=" << zSpeed);

      // Set the Values
      Simulator::Schedule (Seconds (at), &ConstantVelocityMobilityModel::SetVelocity, model,
                           Vector (xSpeed, ySpeed, zSpeed));

      if (time >= 0)
        {
          Simulator::Schedule (Seconds (at + time), &ConstantVelocityMobilityModel::SetVelocity,
                               model, Vector (0, 0, 0));
        }

      position.x = xFinalPosition;
      position.y = yFinalPosition;
      position.z = 0;
    }

  return position;
}


Vector
SetInitialPosition (Ptr<ConstantVelocityMobilityModel> model, string coord, double coordVal)
{
  model->SetPosition (SetOneInitialCoord (model->GetPosition (), coord, coordVal));

  Vector position;
  position.x = model->GetPosition ().x;
  position.y = model->GetPosition ().y;
  position.z = model->GetPosition ().z;

  return position;
}

// Schedule a set of position for a node
Vector
SetSchedPosition (Ptr<ConstantVelocityMobilityModel> model, double at, string coord, double coordVal)
{
  // update position
  model->SetPosition (SetOneInitialCoord (model->GetPosition (), coord, coordVal));

  Vector position;
  position.x = model->GetPosition ().x;
  position.y = model->GetPosition ().y;
  position.z = model->GetPosition ().z;

  // Chedule next positions
  Simulator::Schedule (Seconds (at), &ConstantVelocityMobilityModel::SetPosition, model,position);

  return position;
}

void
Ns2MobilityHelper::Install (void) const
{
  Install (NodeList::Begin (), NodeList::End ());
}

} // namespace ns3
