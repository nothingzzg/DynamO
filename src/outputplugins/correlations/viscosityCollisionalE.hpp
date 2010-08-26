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

#ifndef OPViscosityCollisionalE_H
#define OPViscosityCollisionalE_H

#include "../outputplugin.hpp"
#include "../../datatypes/vector.hpp"
#include <boost/circular_buffer.hpp>

/*! \brief The Correlator class for the viscosity.*/
class OPViscosityCollisionalE: public OutputPlugin
{
  typedef boost::array<Iflt, NDIM> col;
  typedef boost::array<col, NDIM> matrix;
  
public:
  OPViscosityCollisionalE(const DYNAMO::SimData*, const XMLNode& XML);

  virtual void initialise();

  virtual void output(xml::XmlStream &);
  
  virtual OutputPlugin* Clone() const { return new OPViscosityCollisionalE(*this); }
  
  virtual void eventUpdate(const GlobalEvent&, const NEventData&);

  virtual void eventUpdate(const LocalEvent&, const NEventData&);
  
  virtual void eventUpdate(const System&, const NEventData&, const Iflt&);
  
  virtual void eventUpdate(const IntEvent&, const PairEventData&);

  void stream(const Iflt&);

  virtual void operator<<(const XMLNode&);

protected:
  void impulseDelG(const PairEventData&);
  void impulseDelG(const NEventData&);

  void newG(const matrix&);
  void accPass();

  matrix avgTrace;

  size_t count;
  Iflt dt, currentdt;
  matrix delG;

  size_t currlen;
  bool notReady;

  size_t CorrelatorLength;

  boost::circular_buffer<matrix> G;
  std::vector<matrix> accG2;
  Iflt dtfactor;
};

#endif
