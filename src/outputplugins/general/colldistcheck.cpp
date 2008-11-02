/*  DYNAMO:- Event driven molecular dynamics simulator 
    http://www.marcusbannerman.co.uk/dynamo
    Copyright (C) 2008  Marcus N Campbell Bannerman <m.bannerman@gmail.com>

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

#include "colldistcheck.hpp"
#include <boost/foreach.hpp>
#include "../../dynamics/include.hpp"

COPCollDistCheck::COPCollDistCheck(const DYNAMO::SimData* t1, const XMLNode&):
  COutputPlugin(t1,"CollDistCheck")
{
}

COPCollDistCheck::~COPCollDistCheck()
{
}

void 
COPCollDistCheck::initialise() 
{}

void 
COPCollDistCheck::eventUpdate(const CIntEvent& eevent, 
			      const C2ParticleData& PDat)
{
  const eventKey locPair(getClassKey(eevent.getInteraction()), 
			 eevent.getType());
  
  if (distList.find(locPair) == distList.end())
    distList[locPair] = C1DHistogram(0.01 * Sim->Dynamics.units().unitLength());
 
  distList[locPair].addVal(PDat.rij.length());
}

void 
COPCollDistCheck::eventUpdate(const CGlobEvent& gEvent, 
			      const CNParticleData& PDat)
{
  const eventKey locPair(getClassKey(gEvent), 
			 gEvent.getType());
  
  if ((!PDat.L2partChanges.empty()) 
      && (distList.find(locPair) == distList.end()))
    distList[locPair] = C1DHistogram(0.01 * Sim->Dynamics.units().unitLength());
  
  BOOST_FOREACH(const C2ParticleData& dat, PDat.L2partChanges)
    distList[locPair].addVal(dat.rij.length());
}

void 
COPCollDistCheck::eventUpdate(const CLocalEvent& lEvent, 
			      const CNParticleData& PDat)
{
  const eventKey locPair(getClassKey(lEvent), 
			 lEvent.getType());
  
  if ((!PDat.L2partChanges.empty()) 
      && (distList.find(locPair) == distList.end()))
    distList[locPair] = C1DHistogram(0.01 * Sim->Dynamics.units().unitLength());
  
  BOOST_FOREACH(const C2ParticleData& dat, PDat.L2partChanges)
    distList[locPair].addVal(dat.rij.length());
}
  
void 
COPCollDistCheck::eventUpdate(const CSystem& sysEvent, 
			      const CNParticleData& PDat, const Iflt& dt)
{
  const eventKey locPair(getClassKey(sysEvent), sysEvent.getType());
  
  if ((!PDat.L2partChanges.empty())
      && (distList.find(locPair) == distList.end()))
    distList[locPair] = C1DHistogram(0.01 * Sim->Dynamics.units().unitLength());
  
  BOOST_FOREACH(const C2ParticleData& dat, PDat.L2partChanges)
    distList[locPair].addVal(dat.rij.length());
}

void 
COPCollDistCheck::output(xmlw::XmlStream& XML)
{
  XML << xmlw::tag("CollDistCheck");
  
  typedef std::pair<eventKey, C1DHistogram>
    localPair;
  
  BOOST_FOREACH(const localPair& p, distList)
    {
      XML << xmlw::tag("Distance") << xmlw::attr("Name") 
	  << getName(p.first.first, Sim)
	  << xmlw::attr("Type") 
	  << CIntEvent::getCollEnumName(p.first.first.second)
	  << xmlw::attr("EventType") 
	  << CIntEvent::getCollEnumName(p.first.second);

      p.second.outputHistogram(XML, 1.0 / Sim->Dynamics.units().unitLength());
      
      XML << xmlw::endtag("Distance");
    }
  
  XML << xmlw::endtag("CollDistCheck");

}
