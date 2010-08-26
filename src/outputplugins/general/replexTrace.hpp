/*  DYNAMO:- Event driven molecular dynamics simulator 
    http://www.marcusbannerman.co.uk/dynamo
    Copyright (C) 2010  Marcus N Campbell Bannerman <m.bannerman@gmail.com>

    This program is free software: you can redistribute it and/or
    modify it under the terms of the GNU General Public License
    version 3 as published by the Free Software Foundation.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#ifndef OPReplexTrace_HPP
#define OPReplexTrace_HPP

#include "../outputplugin.hpp"
#include <fstream>
#include <string>

class OPReplexTrace: public OutputPlugin
{
public:
  OPReplexTrace(const DYNAMO::SimData*, const XMLNode&);

  OPReplexTrace(const OPReplexTrace&);

  ~OPReplexTrace();

  void eventUpdate(const IntEvent&, const PairEventData&) {}

  void eventUpdate(const GlobalEvent&, const NEventData&) {}

  void eventUpdate(const LocalEvent&, const NEventData&) {}

  void eventUpdate(const System&, const NEventData&, const Iflt&) {}

  OutputPlugin *Clone() const { return new OPReplexTrace(*this); }

  virtual void initialise();

  virtual void output(xml::XmlStream&);

  virtual void changeSystem(OutputPlugin*); 

private:
  void addPoint();

  mutable std::fstream tmpfile;
  std::string filename;
};

#endif
