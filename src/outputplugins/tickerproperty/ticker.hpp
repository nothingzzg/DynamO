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

#ifndef OPTicker_HPP
#define OPTicker_HPP

#include "../outputplugin.hpp"

/*! \brief An output plugin marker class for periodically 'ticked'
 * plugins, ticked by the CSTicker class.
 *
 * This class doesn't require any Liouvillean::updateParticle or
 * Liouvillean::updateAllParticles as this is done in the CSTicker
 * class. This is optimal as most ticker plugins need it anyway
 */
class OPTicker: public OutputPlugin
{
public:
  OPTicker(const DYNAMO::SimData*, const char*);

  //Non virtual to warn if you use them,
  void eventUpdate(const IntEvent&, const PairEventData&) {}
  void eventUpdate(const GlobalEvent&, const NEventData&) {}
  void eventUpdate(const LocalEvent&, const NEventData&) {}
  void eventUpdate(const System&, const NEventData&, const Iflt&) {}

  virtual void output(xml::XmlStream&) {}

  virtual void ticker() = 0;
  
  virtual void periodicOutput() {}

protected:

  Iflt getTickerTime() const;
};

#endif
