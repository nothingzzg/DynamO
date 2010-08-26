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

#include "NewtonL.hpp"
#include "../../extcode/xmlwriter.hpp"
#include "../interactions/intEvent.hpp"
#include "../2particleEventData.hpp"
#include "../NparticleEventData.hpp"
#include "../dynamics.hpp"
#include "../BC/BC.hpp"
#include "../../base/is_exception.hpp"
#include "../../base/is_simdata.hpp"
#include "../species/species.hpp"
#include "../../schedulers/sorters/datastruct.hpp"
#include "shapes/frenkelroot.hpp"
#include "shapes/oscillatingplate.hpp"

bool 
LNewtonian::CubeCubeInRoot(CPDData& dat, const Iflt& d) const
{
  //To be approaching, the largest dimension of rij must be being
  //reduced
  
  size_t largedim(0);
  for (size_t iDim(1); iDim < NDIM; ++iDim)
    if (fabs(dat.rij[iDim]) > fabs(dat.rij[largedim])) largedim = iDim;
    
  if (dat.rij[largedim] * dat.vij[largedim] >= 0) return false;

  Iflt tInMax(-HUGE_VAL), tOutMin(HUGE_VAL);
  
  for (size_t iDim(0); iDim < NDIM; ++iDim)
    {
      Iflt tmptime1 = -(dat.rij[iDim] + d) / dat.vij[iDim];
      Iflt tmptime2 = -(dat.rij[iDim] - d) / dat.vij[iDim];
      
      if (tmptime1 < tmptime2)
	{
	  if (tmptime1 > tInMax) tInMax = tmptime1;
	  if (tmptime2 < tOutMin) tOutMin = tmptime2;
	}
      else
	{
	  if (tmptime2 > tInMax) tInMax = tmptime2;
	  if (tmptime1 < tOutMin) tOutMin = tmptime1;
	}
    }
  
  if (tInMax >= tOutMin) return false;

  dat.dt = tInMax;
  return true;
}

bool 
LNewtonian::CubeCubeInRoot(CPDData& dat, const Iflt& d, const Matrix& Rot) const
{
  //To be approaching, the largest dimension of rij must be being
  //reduced

  Vector rij = Rot * dat.rij, vij = Rot * dat.vij;
  
  size_t largedim(0);
  for (size_t iDim(1); iDim < NDIM; ++iDim)
    if (fabs(rij[iDim]) > fabs(rij[largedim])) largedim = iDim;
    
  if (rij[largedim] * vij[largedim] < 0)
    {      
      Iflt tInMax(-HUGE_VAL), tOutMin(HUGE_VAL);
      
      for (size_t iDim(0); iDim < NDIM; ++iDim)
	{
	  Iflt tmptime1 = -(rij[iDim] + d) / vij[iDim];
	  Iflt tmptime2 = -(rij[iDim] - d) / vij[iDim];
	  
	  if (tmptime1 < tmptime2)
	    {
	      if (tmptime1 > tInMax) tInMax = tmptime1;
	      if (tmptime2 < tOutMin) tOutMin = tmptime2;
	    }
	  else
	    {
	      if (tmptime2 > tInMax) tInMax = tmptime2;
	      if (tmptime1 < tOutMin) tOutMin = tmptime1;
	    }
	}
      
      if (tInMax < tOutMin)
	{
	  dat.dt = tInMax;
	  return true;
	}
    }
  return false;
}

bool 
LNewtonian::cubeOverlap(const CPDData& dat, const Iflt& d) const
{
  for (size_t iDim(0); iDim < NDIM; ++iDim)
    if (fabs(dat.rij[iDim]) > d) return false;
  
  return true;
}

bool 
LNewtonian::cubeOverlap(const CPDData& dat, const Iflt& d, 
		      const Matrix& rot) const
{
  Vector rij = rot * dat.rij;

  for (size_t iDim(0); iDim < NDIM; ++iDim)
    if (fabs(rij[iDim]) > d) 
	return false;
  
  return true;
}

bool 
LNewtonian::SphereSphereInRoot(CPDData& dat, const Iflt& d2) const
{
  if (dat.rvdot < 0)
    {
      Iflt arg = dat.rvdot * dat.rvdot - dat.v2 * (dat.r2 - d2);

      if (arg > 0)
	{
	  //This is the more numerically stable form of the quadratic
	  //formula
	  dat.dt = (d2 - dat.r2) / (dat.rvdot-sqrt(arg));

#ifdef DYNAMO_DEBUG
	  if (std::isnan(dat.dt))
	    D_throw() << "dat.dt is nan";
#endif
	  return true;
	}
      else 
	return false;
    }
  else
    return false;
}
  
bool 
LNewtonian::SphereSphereOutRoot(CPDData& dat, const Iflt& d2) const
{
  dat.dt = (sqrt(dat.rvdot * dat.rvdot 
		 - dat.v2 * (dat.r2 - d2)) - dat.rvdot) / dat.v2;

  if (isnan(dat.dt))
    {//The nan occurs if the spheres aren't moving apart
      dat.dt = HUGE_VAL;
      return false;
    }
  else
    return true;
}

bool 
LNewtonian::sphereOverlap(const CPDData& dat, const Iflt& d2) const
{
  return (dat.r2 - d2) < 0.0;
}

ParticleEventData 
LNewtonian::randomGaussianEvent(const Particle& part, const Iflt& sqrtT) const
{
  //See http://mathworld.wolfram.com/SpherePointPicking.html

  //Ensure the particle is free streamed first
  updateParticle(part);

  //Collect the precoll data
  ParticleEventData tmpDat(part, Sim->dynamics.getSpecies(part), GAUSSIAN);

  Iflt factor = 
    sqrtT / std::sqrt(tmpDat.getSpecies().getMass());

  //Assign the new velocities
  for (size_t iDim = 0; iDim < NDIM; iDim++)
    const_cast<Particle&>(part).getVelocity()[iDim] 
      = Sim->normal_sampler() * factor;

  tmpDat.setDeltaKE(0.5 * tmpDat.getSpecies().getMass()
		    * (part.getVelocity().nrm2() 
		       - tmpDat.getOldVel().nrm2()));
  
  return tmpDat;
}

LNewtonian::LNewtonian(DYNAMO::SimData* tmp):
  Liouvillean(tmp)
{}

void
LNewtonian::streamParticle(Particle &particle, const Iflt &dt) const
{
  particle.getPosition() += particle.getVelocity() * dt;
}

Iflt 
LNewtonian::getWallCollision(const Particle &part, 
			   const Vector  &wallLoc, 
			   const Vector  &wallNorm) const
{
  Vector  rij = part.getPosition(),
    vel = part.getVelocity();

  Sim->dynamics.BCs().applyBC(rij, vel);

  Iflt rvdot = (vel | wallNorm);

  rij -= wallLoc;

  if (rvdot < 0)
    return  - ((rij | wallNorm) / rvdot);
  
  return HUGE_VAL;
}


ParticleEventData 
LNewtonian::runWallCollision(const Particle &part, 
			   const Vector  &vNorm,
			   const Iflt& e
			   ) const
{
  updateParticle(part);

  ParticleEventData retVal(part, Sim->dynamics.getSpecies(part), WALL);
  
  const_cast<Particle&>(part).getVelocity()
    -= (1+e) * (vNorm | part.getVelocity()) * vNorm;
  
  retVal.setDeltaKE(0.5 * retVal.getSpecies().getMass()
		    * (part.getVelocity().nrm2() 
		       - retVal.getOldVel().nrm2()));
  
  return retVal; 
}

ParticleEventData 
LNewtonian::runAndersenWallCollision(const Particle& part, 
				   const Vector & vNorm,
				   const Iflt& sqrtT
				   ) const
{  
  updateParticle(part);

  //This gives a completely new random unit vector with a properly
  //distributed Normal component. See Granular Simulation Book
  ParticleEventData tmpDat(part, Sim->dynamics.getSpecies(part), WALL);
 
  for (size_t iDim = 0; iDim < NDIM; iDim++)
    const_cast<Particle&>(part).getVelocity()[iDim] 
      = Sim->normal_sampler() * sqrtT 
      / sqrt(Sim->dynamics.getSpecies(part).getMass());
  
  const_cast<Particle&>(part).getVelocity() 
    //This first line adds a component in the direction of the normal
    += vNorm * (sqrtT * sqrt(-2.0*log(1.0-Sim->uniform_sampler())
			     / Sim->dynamics.getSpecies(part).getMass())
		//This removes the original normal component
		-(part.getVelocity() | vNorm));

  tmpDat.setDeltaKE(0.5 * tmpDat.getSpecies().getMass()
		    * (part.getVelocity().nrm2()
		       - tmpDat.getOldVel().nrm2()));
  
  return tmpDat; 
}

Iflt
LNewtonian::getSquareCellCollision2(const Particle& part, 
				 const Vector & origin, 
				 const Vector & width) const
{
  Vector  rpos(part.getPosition() - origin);
  Vector  vel(part.getVelocity());
  Sim->dynamics.BCs().applyBC(rpos, vel);
  
#ifdef DYNAMO_DEBUG
  for (size_t iDim = 0; iDim < NDIM; ++iDim)
    if ((vel[iDim] == 0) && (std::signbit(vel[iDim])))
      D_throw() << "You have negative zero velocities, dont use them."
		<< "\nPlease think of the neighbour lists.";
#endif 

  Iflt retVal;
  if (vel[0] < 0)
    retVal = -rpos[0]/vel[0];
  else
    retVal = (width[0]-rpos[0]) / vel[0];

  for (size_t iDim = 1; iDim < NDIM; ++iDim)
    {
      Iflt tmpdt((vel[iDim] < 0)
		 ? -rpos[iDim]/vel[iDim] 
		 : (width[iDim]-rpos[iDim]) / vel[iDim]);
      
      if (tmpdt < retVal)
	retVal = tmpdt;
    }
  
  return retVal;
}

int
LNewtonian::getSquareCellCollision3(const Particle& part, 
				    const Vector & origin, 
				    const Vector & width) const
{
  Vector  rpos(part.getPosition() - origin);
  Vector  vel(part.getVelocity());

  Sim->dynamics.BCs().applyBC(rpos, vel);

  int retVal(0);
  Iflt time(HUGE_VAL);
  
#ifdef DYNAMO_DEBUG
  for (size_t iDim = 0; iDim < NDIM; ++iDim)
    if ((vel[iDim] == 0) && (std::signbit(vel[iDim])))
      D_throw() << "You have negative zero velocities, dont use them."
		<< "\nPlease think of the neighbour lists.";
#endif

  for (size_t iDim = 0; iDim < NDIM; ++iDim)
    {
      Iflt tmpdt = ((vel[iDim] < 0) 
		  ? -rpos[iDim]/vel[iDim] 
		  : (width[iDim]-rpos[iDim]) / vel[iDim]);

      if (tmpdt < time)
	{
	  time = tmpdt;
	  retVal = (vel[iDim] < 0) ? -(iDim+1) : (iDim+1);
	}
    }

  if (((retVal < 0) && (vel[abs(retVal)-1] > 0))
      || ((retVal > 0) && (vel[abs(retVal)-1] < 0)))
    D_throw() << "Found an error! retVal " << retVal
	      << " vel is " << vel[abs(retVal)-1];

  return retVal;
}

bool 
LNewtonian::DSMCSpheresTest(const Particle& p1, 
			  const Particle& p2, 
			  Iflt& maxprob,
			  const Iflt& factor,
			  CPDData& pdat) const
{
  pdat.vij = p1.getVelocity() - p2.getVelocity();

  //Sim->dynamics.BCs().applyBC(pdat.rij, pdat.vij);
  pdat.rvdot = (pdat.rij | pdat.vij);
  
  if (pdat.rvdot > 0)
    return false; //Positive rvdot

  Iflt prob = factor * (-pdat.rvdot);

  if (prob > maxprob)
    maxprob = prob;

  return prob > Sim->uniform_sampler() * maxprob;
}

PairEventData
LNewtonian::DSMCSpheresRun(const Particle& p1, 
			 const Particle& p2, 
			 const Iflt& e,
			 CPDData& pdat) const
{
  updateParticlePair(p1, p2);  

  PairEventData retVal(p1, p2,
			Sim->dynamics.getSpecies(p1),
			Sim->dynamics.getSpecies(p2),
			CORE);
  
  retVal.rij = pdat.rij;
  retVal.rvdot = pdat.rvdot;

  Iflt p1Mass = retVal.particle1_.getSpecies().getMass(); 
  Iflt p2Mass = retVal.particle2_.getSpecies().getMass();
  Iflt mu = p1Mass * p2Mass/(p1Mass+p2Mass);

  retVal.dP = retVal.rij * ((1.0 + e) * mu * retVal.rvdot 
			    / retVal.rij.nrm2());  

  //This function must edit particles so it overrides the const!
  const_cast<Particle&>(p1).getVelocity() -= retVal.dP / p1Mass;
  const_cast<Particle&>(p2).getVelocity() += retVal.dP / p2Mass;

  retVal.particle1_.setDeltaKE(0.5 * retVal.particle1_.getSpecies().getMass()
			       * (p1.getVelocity().nrm2() 
				  - retVal.particle1_.getOldVel().nrm2()));
  
  retVal.particle2_.setDeltaKE(0.5 * retVal.particle2_.getSpecies().getMass()
			       * (p2.getVelocity().nrm2() 
				  - retVal.particle2_.getOldVel().nrm2()));

  return retVal;
}


PairEventData 
LNewtonian::SmoothSpheresCollInfMassSafe(const IntEvent& event, const Iflt& e,
				       const Iflt&, const EEventType& eType) const
{
  const Particle& particle1 = Sim->particleList[event.getParticle1ID()];
  const Particle& particle2 = Sim->particleList[event.getParticle2ID()];

  updateParticlePair(particle1, particle2);  

  PairEventData retVal(particle1, particle2,
			Sim->dynamics.getSpecies(particle1),
			Sim->dynamics.getSpecies(particle2),
			eType);
    
  Sim->dynamics.BCs().applyBC(retVal.rij, retVal.vijold);
  
  Iflt p1Mass = retVal.particle1_.getSpecies().getMass(); 
  Iflt p2Mass = retVal.particle2_.getSpecies().getMass();
 
  retVal.rvdot = (retVal.rij | retVal.vijold);

#ifdef DYNAMO_DEBUG
  if ((p1Mass == 0.0) && (p2Mass == 0.0))
    D_throw() << "Both particles have infinite mass";
#endif

   if ((p1Mass != 0.0) && (p2Mass != 0.0))
    {
      Iflt mu = p1Mass * p2Mass / (p1Mass + p2Mass);

      retVal.dP = retVal.rij * ((1.0 + e) * mu * retVal.rvdot / retVal.rij.nrm2());  

      //This function must edit particles so it overrides the const!
      const_cast<Particle&>(particle1).getVelocity() -= retVal.dP / p1Mass;
      const_cast<Particle&>(particle2).getVelocity() += retVal.dP / p2Mass;
    }
  else if (p1Mass == 0.0)
    {
      retVal.dP = p2Mass * retVal.rij * ((1.0 + e) * retVal.rvdot / retVal.rij.nrm2());  
      //This function must edit particles so it overrides the const!
      const_cast<Particle&>(particle2).getVelocity() += retVal.dP / p2Mass;
    }
  else
    {
      retVal.dP = p1Mass * retVal.rij * ((1.0 + e) * retVal.rvdot / retVal.rij.nrm2());  
      //This function must edit particles so it overrides the const!
      const_cast<Particle&>(particle1).getVelocity() -= retVal.dP / p1Mass;
    }

  retVal.particle1_.setDeltaKE(0.5 * retVal.particle1_.getSpecies().getMass()
			       * (particle1.getVelocity().nrm2()
				  - retVal.particle1_.getOldVel().nrm2()));
  
  retVal.particle2_.setDeltaKE(0.5 * retVal.particle2_.getSpecies().getMass()
			       * (particle2.getVelocity().nrm2() 
				  - retVal.particle2_.getOldVel().nrm2()));

  return retVal;
}

PairEventData 
LNewtonian::SmoothSpheresColl(const IntEvent& event, const Iflt& e,
			    const Iflt&, const EEventType& eType) const
{
  const Particle& particle1 = Sim->particleList[event.getParticle1ID()];
  const Particle& particle2 = Sim->particleList[event.getParticle2ID()];

  updateParticlePair(particle1, particle2);  

  PairEventData retVal(particle1, particle2,
			Sim->dynamics.getSpecies(particle1),
			Sim->dynamics.getSpecies(particle2),
			eType);
    
  Sim->dynamics.BCs().applyBC(retVal.rij, retVal.vijold);
  
  Iflt p1Mass = retVal.particle1_.getSpecies().getMass(); 
  Iflt p2Mass = retVal.particle2_.getSpecies().getMass();
  Iflt mu = p1Mass * p2Mass/(p1Mass+p2Mass);
  
  retVal.rvdot = (retVal.rij | retVal.vijold);
  retVal.dP = retVal.rij * ((1.0 + e) * mu * retVal.rvdot / retVal.rij.nrm2());  

  //This function must edit particles so it overrides the const!
  const_cast<Particle&>(particle1).getVelocity() -= retVal.dP / p1Mass;
  const_cast<Particle&>(particle2).getVelocity() += retVal.dP / p2Mass;

  retVal.particle1_.setDeltaKE(0.5 * retVal.particle1_.getSpecies().getMass()
			       * (particle1.getVelocity().nrm2() 
				  - retVal.particle1_.getOldVel().nrm2()));
  
  retVal.particle2_.setDeltaKE(0.5 * retVal.particle2_.getSpecies().getMass()
			       * (particle2.getVelocity().nrm2() 
				  - retVal.particle2_.getOldVel().nrm2()));

  return retVal;
}

PairEventData 
LNewtonian::parallelCubeColl(const IntEvent& event, const Iflt& e,
			   const Iflt&, const EEventType& eType) const
{
  const Particle& particle1 = Sim->particleList[event.getParticle1ID()];
  const Particle& particle2 = Sim->particleList[event.getParticle2ID()];

  updateParticlePair(particle1, particle2);

  PairEventData retVal(particle1, particle2,
			Sim->dynamics.getSpecies(particle1),
			Sim->dynamics.getSpecies(particle2),
			eType);
    
  Sim->dynamics.BCs().applyBC(retVal.rij, retVal.vijold);
  
  size_t dim(0);
   
  for (size_t iDim(1); iDim < NDIM; ++iDim)
    if (fabs(retVal.rij[dim]) < fabs(retVal.rij[iDim])) dim = iDim;

  Iflt p1Mass = retVal.particle1_.getSpecies().getMass(); 
  Iflt p2Mass = retVal.particle2_.getSpecies().getMass();
  Iflt mu = p1Mass * p2Mass/(p1Mass+p2Mass);
  
  Vector collvec(0,0,0);

  if (retVal.rij[dim] < 0)
    collvec[dim] = -1;
  else
    collvec[dim] = 1;

  retVal.rvdot = (retVal.rij | retVal.vijold);

  retVal.dP = collvec * (1.0 + e) * mu * (collvec | retVal.vijold);  

  //This function must edit particles so it overrides the const!
  const_cast<Particle&>(particle1).getVelocity() -= retVal.dP / p1Mass;
  const_cast<Particle&>(particle2).getVelocity() += retVal.dP / p2Mass;

  retVal.particle1_.setDeltaKE(0.5 * retVal.particle1_.getSpecies().getMass()
			       * (particle1.getVelocity().nrm2() 
				  - retVal.particle1_.getOldVel().nrm2()));
  
  retVal.particle2_.setDeltaKE(0.5 * retVal.particle2_.getSpecies().getMass()
			       * (particle2.getVelocity().nrm2() 
				  - retVal.particle2_.getOldVel().nrm2()));

  return retVal;
}

PairEventData 
LNewtonian::parallelCubeColl(const IntEvent& event, const Iflt& e,
			   const Iflt&, const Matrix& rot,
			   const EEventType& eType) const
{
  const Particle& particle1 = Sim->particleList[event.getParticle1ID()];
  const Particle& particle2 = Sim->particleList[event.getParticle2ID()];

  updateParticlePair(particle1, particle2);

  PairEventData retVal(particle1, particle2,
			Sim->dynamics.getSpecies(particle1),
			Sim->dynamics.getSpecies(particle2),
			eType);
    
  Sim->dynamics.BCs().applyBC(retVal.rij, retVal.vijold);
  
  retVal.rij = rot * Vector(retVal.rij);
  retVal.vijold = rot * Vector(retVal.vijold);

  size_t dim(0);
   
  for (size_t iDim(1); iDim < NDIM; ++iDim)
    if (fabs(retVal.rij[dim]) < fabs(retVal.rij[iDim])) dim = iDim;

  Iflt p1Mass = retVal.particle1_.getSpecies().getMass(); 
  Iflt p2Mass = retVal.particle2_.getSpecies().getMass();
  Iflt mu = p1Mass * p2Mass/ (p1Mass + p2Mass);
  
  Vector collvec(0,0,0);

  if (retVal.rij[dim] < 0)
    collvec[dim] = -1;
  else
    collvec[dim] = 1;

  retVal.rvdot = (retVal.rij | retVal.vijold);

  retVal.dP = collvec * (1.0 + e) * mu * (collvec | retVal.vijold);  

  retVal.dP = Transpose(rot) * Vector(retVal.dP);
  retVal.rij = Transpose(rot) * Vector(retVal.rij);
  retVal.vijold = Transpose(rot) * Vector(retVal.vijold);

  //This function must edit particles so it overrides the const!
  const_cast<Particle&>(particle1).getVelocity() -= retVal.dP / p1Mass;
  const_cast<Particle&>(particle2).getVelocity() += retVal.dP / p2Mass;

  retVal.particle1_.setDeltaKE(0.5 * retVal.particle1_.getSpecies().getMass()
			       * (particle1.getVelocity().nrm2() 
				  - retVal.particle1_.getOldVel().nrm2()));
  
  retVal.particle2_.setDeltaKE(0.5 * retVal.particle2_.getSpecies().getMass()
			       * (particle2.getVelocity().nrm2() 
				  - retVal.particle2_.getOldVel().nrm2()));

  return retVal;
}

NEventData 
LNewtonian::multibdyCollision(const CRange& range1, const CRange& range2, 
			    const Iflt&, const EEventType& eType) const
{
  Vector COMVel1(0,0,0), COMVel2(0,0,0), COMPos1(0,0,0), COMPos2(0,0,0);
  
  Iflt structmass1(0), structmass2(0);
  
  BOOST_FOREACH(const size_t& ID, range1)
    {
      updateParticle(Sim->particleList[ID]);
      
      structmass1 += 
	Sim->dynamics.getSpecies(Sim->particleList[ID]).getMass();
      
      Vector pos(Sim->particleList[ID].getPosition()),
	vel(Sim->particleList[ID].getVelocity());

      Sim->dynamics.BCs().applyBC(pos, vel);

      COMVel1 += vel
	* Sim->dynamics.getSpecies(Sim->particleList[ID]).getMass();      

      COMPos1 += pos
	* Sim->dynamics.getSpecies(Sim->particleList[ID]).getMass();
    }
  
  BOOST_FOREACH(const size_t& ID, range2)
    {
      updateParticle(Sim->particleList[ID]);

      structmass2 += 
	Sim->dynamics.getSpecies(Sim->particleList[ID]).getMass();
      
      Vector pos(Sim->particleList[ID].getPosition()),
	vel(Sim->particleList[ID].getVelocity());

      Sim->dynamics.BCs().applyBC(pos, vel);

      COMVel2 += vel
	* Sim->dynamics.getSpecies(Sim->particleList[ID]).getMass();      

      COMPos2 += pos
	* Sim->dynamics.getSpecies(Sim->particleList[ID]).getMass();
    }
  
  COMVel1 /= structmass1;
  COMVel2 /= structmass2;
  
  COMPos1 /= structmass1;
  COMPos2 /= structmass2;
  
  Vector  rij = COMPos1 - COMPos2, vij = COMVel1 - COMVel2;
  Sim->dynamics.BCs().applyBC(rij, vij);
  Iflt rvdot = (rij | vij);

  Iflt mu = structmass1 * structmass2 / (structmass1 + structmass2);

  static const Iflt e = 1.0;
  Vector  dP = rij * ((1.0 + e) * mu * rvdot / rij.nrm2());

  NEventData retVal;
  BOOST_FOREACH(const size_t& ID, range1)
    {
      ParticleEventData tmpval
	(Sim->particleList[ID],
	 Sim->dynamics.getSpecies(Sim->particleList[ID]),
	 eType);

      const_cast<Particle&>(tmpval.getParticle()).getVelocity()
	-= dP / structmass1;
      
      tmpval.setDeltaKE(0.5 * tmpval.getSpecies().getMass()
			* (tmpval.getParticle().getVelocity().nrm2() 
			   - tmpval.getOldVel().nrm2()));
      
      retVal.L1partChanges.push_back(tmpval);
    }

  BOOST_FOREACH(const size_t& ID, range2)
    {
      ParticleEventData tmpval
	(Sim->particleList[ID],
	 Sim->dynamics.getSpecies(Sim->particleList[ID]),
	 eType);

      const_cast<Particle&>(tmpval.getParticle()).getVelocity()
	+= dP / structmass2;
      
      tmpval.setDeltaKE(0.5 * tmpval.getSpecies().getMass()
			* (tmpval.getParticle().getVelocity().nrm2() 
			   - tmpval.getOldVel().nrm2()));
  
      retVal.L1partChanges.push_back(tmpval);
    }
  
  return retVal;
}

NEventData 
LNewtonian::multibdyWellEvent(const CRange& range1, const CRange& range2, 
			    const Iflt&, const Iflt& deltaKE, EEventType& eType) const
{
  Vector  COMVel1(0,0,0), COMVel2(0,0,0), COMPos1(0,0,0), COMPos2(0,0,0);
  
  Iflt structmass1(0), structmass2(0);
  
  BOOST_FOREACH(const size_t& ID, range1)
    {
      updateParticle(Sim->particleList[ID]);
      
      structmass1 += 
	Sim->dynamics.getSpecies(Sim->particleList[ID]).getMass();

      Vector pos(Sim->particleList[ID].getPosition()),
	vel(Sim->particleList[ID].getVelocity());
      
      Sim->dynamics.BCs().applyBC(pos, vel);

      COMVel1 += vel
	* Sim->dynamics.getSpecies(Sim->particleList[ID]).getMass();      

      COMPos1 += pos
	* Sim->dynamics.getSpecies(Sim->particleList[ID]).getMass();
    }
  
  BOOST_FOREACH(const size_t& ID, range2)
    {
      updateParticle(Sim->particleList[ID]);

      structmass2 += 
	Sim->dynamics.getSpecies(Sim->particleList[ID]).getMass();
      
      Vector pos(Sim->particleList[ID].getPosition()),
	vel(Sim->particleList[ID].getVelocity());

      Sim->dynamics.BCs().applyBC(pos, vel);

      COMVel2 += vel
	* Sim->dynamics.getSpecies(Sim->particleList[ID]).getMass();      

      COMPos2 += pos
	* Sim->dynamics.getSpecies(Sim->particleList[ID]).getMass();
    }
  
  COMVel1 /= structmass1;
  COMVel2 /= structmass2;
  
  COMPos1 /= structmass1;
  COMPos2 /= structmass2;
  
  Vector  rij = COMPos1 - COMPos2, vij = COMVel1 - COMVel2;
  Sim->dynamics.BCs().applyBC(rij, vij);
  Iflt rvdot = (rij | vij);

  Iflt mu = structmass1 * structmass2 / (structmass1 + structmass2);

  Iflt R2 = rij.nrm2();
  Iflt sqrtArg = rvdot * rvdot + 2.0 * R2 * deltaKE / mu;

  Vector  dP(0,0,0);

  if ((deltaKE < 0) && (sqrtArg < 0))
    {
      eType = BOUNCE;
      dP = rij * 2.0 * mu * rvdot / R2;
    }
  else
    {
      if (deltaKE < 0)
	eType = WELL_KEDOWN;
      else
	eType = WELL_KEUP;
	  
      if (rvdot < 0)
	dP = rij 
	  * (2.0 * deltaKE / (std::sqrt(sqrtArg) - rvdot));
      else
	dP = rij 
	  * (-2.0 * deltaKE / (rvdot + std::sqrt(sqrtArg)));
    }
  
  NEventData retVal;
  BOOST_FOREACH(const size_t& ID, range1)
    {
      ParticleEventData tmpval
	(Sim->particleList[ID],
	 Sim->dynamics.getSpecies(Sim->particleList[ID]),
	 eType);

      const_cast<Particle&>(tmpval.getParticle()).getVelocity()
	-= dP / structmass1;
      
      tmpval.setDeltaKE(0.5 * tmpval.getSpecies().getMass()
			* (tmpval.getParticle().getVelocity().nrm2() 
			   - tmpval.getOldVel().nrm2()));
        
      retVal.L1partChanges.push_back(tmpval);
    }

  BOOST_FOREACH(const size_t& ID, range2)
    {
      ParticleEventData tmpval
	(Sim->particleList[ID],
	 Sim->dynamics.getSpecies(Sim->particleList[ID]),
	 eType);

      const_cast<Particle&>(tmpval.getParticle()).getVelocity()
	+= dP / structmass2;
      
      tmpval.setDeltaKE(0.5 * tmpval.getSpecies().getMass()
			* (tmpval.getParticle().getVelocity().nrm2() 
			   - tmpval.getOldVel().nrm2()));
      
      retVal.L1partChanges.push_back(tmpval);
    }
  
  return retVal;
}

PairEventData 
LNewtonian::SphereWellEvent(const IntEvent& event, const Iflt& deltaKE, 
			  const Iflt &) const
{
  const Particle& particle1 = Sim->particleList[event.getParticle1ID()];
  const Particle& particle2 = Sim->particleList[event.getParticle2ID()];

  updateParticlePair(particle1, particle2);  

  PairEventData retVal(particle1, particle2,
			Sim->dynamics.getSpecies(particle1),
			Sim->dynamics.getSpecies(particle2),
			event.getType());
    
  Sim->dynamics.BCs().applyBC(retVal.rij,retVal.vijold);
  
  retVal.rvdot = (retVal.rij | retVal.vijold);
  
  Iflt p1Mass = retVal.particle1_.getSpecies().getMass();
  Iflt p2Mass = retVal.particle2_.getSpecies().getMass();
  Iflt mu = p1Mass * p2Mass / (p1Mass + p2Mass);  
  Iflt R2 = retVal.rij.nrm2();
  Iflt sqrtArg = retVal.rvdot * retVal.rvdot + 2.0 * R2 * deltaKE / mu;
  
  if ((deltaKE < 0) && (sqrtArg < 0))
    {
      event.setType(BOUNCE);
      retVal.setType(BOUNCE);
      retVal.dP = retVal.rij * 2.0 * mu * retVal.rvdot / R2;
    }
  else if (deltaKE==0)
    {
      event.setType(NON_EVENT);
      retVal.setType(NON_EVENT);
      retVal.dP = Vector(0,0,0);
    }
  else
    {
      if (deltaKE < 0)
	{
	  event.setType(WELL_KEDOWN);
	  retVal.setType(WELL_KEDOWN);
	}
      else
	{
	  event.setType(WELL_KEUP);
	  retVal.setType(WELL_KEUP);	  
	}
	  
      retVal.particle1_.setDeltaU(-0.5 * deltaKE);
      retVal.particle2_.setDeltaU(-0.5 * deltaKE);	  
      
      if (retVal.rvdot < 0)
	retVal.dP = retVal.rij 
	  * (2.0 * deltaKE / (std::sqrt(sqrtArg) - retVal.rvdot));
      else
	retVal.dP = retVal.rij 
	  * (-2.0 * deltaKE / (retVal.rvdot + std::sqrt(sqrtArg)));
    }
  
#ifdef DYNAMO_DEBUG
  if (isnan(retVal.dP[0]))
    D_throw() << "A nan dp has ocurred";
#endif
  
  //This function must edit particles so it overrides the const!
  const_cast<Particle&>(particle1).getVelocity() -= retVal.dP / p1Mass;
  const_cast<Particle&>(particle2).getVelocity() += retVal.dP / p2Mass;
  
  retVal.particle1_.setDeltaKE(0.5 * retVal.particle1_.getSpecies().getMass()
			       * (particle1.getVelocity().nrm2() 
				  - retVal.particle1_.getOldVel().nrm2()));
  
  retVal.particle2_.setDeltaKE(0.5 * retVal.particle2_.getSpecies().getMass()
			       * (particle2.getVelocity().nrm2() 
				  - retVal.particle2_.getOldVel().nrm2()));

  return retVal;
}

void 
LNewtonian::outputXML(xml::XmlStream& XML) const
{
  XML << xml::attr("Type") 
      << "Newtonian";
}

Iflt 
LNewtonian::getPBCSentinelTime(const Particle& part, const Iflt& lMax) const
{
#ifdef DYNAMO_DEBUG
  if (!isUpToDate(part))
    D_throw() << "Particle is not up to date";
#endif

  Vector pos(part.getPosition()), vel(part.getVelocity());

  Sim->dynamics.BCs().applyBC(pos, vel);

  Iflt retval = (0.5 * Sim->aspectRatio[0] - lMax) / fabs(vel[0]);

  for (size_t i(1); i < NDIM; ++i)
    {
      Iflt tmp = (0.5 * Sim->aspectRatio[i] - lMax) / fabs(vel[i]);

      if (tmp < retval)
	retval = tmp;
    }

  return retval;
}

Iflt
LNewtonian::getPointPlateCollision(const Particle& part, const Vector& nrw0,
				 const Vector& nhat, const Iflt& Delta,
				 const Iflt& Omega, const Iflt& Sigma,
				 const Iflt& t, bool lastpart) const
{
#ifdef DYNAMO_DEBUG
  if (!isUpToDate(part))
    D_throw() << "Particle1 " << part.getID() << " is not up to date";
#endif
  
  Vector pos(part.getPosition() - nrw0), vel(part.getVelocity());
  Sim->dynamics.BCs().applyBC(pos, vel);

  Iflt t_high;
  Iflt surfaceOffset = pos | nhat;
  Iflt surfaceVel = vel | nhat;
  
  if (surfaceVel > 0)
    t_high = (Sigma + Delta - surfaceOffset) / surfaceVel;
  else
    t_high = -(Sigma + Delta + surfaceOffset) / surfaceVel;
  

  COscillatingPlateFunc fL(vel, nhat, pos, t, Delta, Omega, Sigma);
  
#ifdef DYNAMO_DEBUG
  if (Sigma < 0) D_throw() << "Assuming a positive Sigma here";
#endif

  //A particle has penetrated the plate, probably due to some small numerical error
  //We can just adjust the seperation vector till the particle is on the surface of the plate
  if (fL.F_zeroDeriv() > 0)
    {
#ifdef DYNAMO_DEBUG
      I_cerr() << "Particle is penetrating the \"upper\" plate"
	       << "\nTo avoid rediscovering the root we're adjusting the relative position vector to just touching."
	       << "\nThis is fine if it is a rare event.";
#endif
      fL.fixFZeroSign(false);

#ifdef DYNAMO_DEBUG
      //This is just incase the oscillating plate shape function is broken
      if (fL.F_zeroDeriv() > 0)
	D_throw() << "Failed to adjust the plate position";
#endif
    }
      
  Iflt t_low1 = 0, t_low2 = 0;
  if (lastpart)
    {
      if (-fL.F_zeroDeriv() < fL.F_zeroDerivFlip())
	//Shift the lower bound up so we don't find the same root again
	t_low1 = fabs(2.0 * fL.F_firstDeriv())
	  / fL.F_secondDeriv_max(0.0);
      else
	t_low2 = fabs(2.0 * fL.F_firstDeriv())
	  / fL.F_secondDeriv_max(0.0);
    }


  //Must be careful with collisions at the end of the interval
  t_high *= 1.01;
  
  Iflt root1 = frenkelRootSearch(fL, Sigma, t_low1, t_high, 1e-12);
  fL.flipSigma();

  if (fL.F_zeroDeriv() < 0)
    {
#ifdef DYNAMO_DEBUG
      I_cerr() << "Particle is penetrating the \"lower\" plate"
	       << "\nTo avoid rediscovering the root we're adjusting the relative position vector to just touching."
	       << "\nThis is fine if it is a rare event.";
#endif
      fL.fixFZeroSign(true);

#ifdef DYNAMO_DEBUG
      //This is just incase the oscillating plate shape function is broken
      if (fL.F_zeroDeriv() < 0)
	D_throw() << "Failed to adjust the plate position";
#endif
    }

  Iflt root2 = frenkelRootSearch(fL, Sigma, t_low2, t_high, 1e-12);

  
  if ((fabs(surfaceOffset - (nhat | fL.wallPosition())) > Sigma)
      || (((root1 == HUGE_VAL) && (root2 == HUGE_VAL)) 
	  || ((t_low1 > t_high) && (t_low2 > t_high))))
    {
      //This can be a problem
#ifdef DYNAMO_DEBUG      
      I_cerr() << "Particle " << part.getID() 
	       << " may be outside/heading out of the plates"
	       << "\nerror = "
	       << (fabs(surfaceOffset - (nhat | fL.wallPosition())) - Sigma) 
	/ Sim->dynamics.units().unitLength()
	       << "\n Root1 = " << root1 / Sim->dynamics.units().unitTime()
	       << "\n Root2 = " << root2 / Sim->dynamics.units().unitTime();
#endif
      
      //If the particle is going out of bounds, collide now
      if (fL.test_root(Sigma))
	{
#ifdef DYNAMO_DEBUG
	  {
	    COscillatingPlateFunc ftmp(fL);
	    COscillatingPlateFunc ftmp2(fL);
	    ftmp.flipSigma();
	    
	    Iflt fl01(ftmp.F_zeroDeriv());
	    ftmp.stream(t_low1);
	    Iflt flt_low1(ftmp.F_zeroDeriv());
	    ftmp.stream(t_high - t_low1);
	    Iflt flt_high1(ftmp.F_zeroDeriv());
	    
	    Iflt fl02(ftmp2.F_zeroDeriv());
	    ftmp2.stream(t_low2);
	    Iflt flt_low2(ftmp2.F_zeroDeriv());
	    ftmp2.stream(t_high - t_low2);
	    Iflt flt_high2(ftmp2.F_zeroDeriv());
	    
	    I_cerr() << "****Forcing collision"
		     << "\ndSysTime = " << Sim->dSysTime
		     << "\nlNColl = " << Sim->eventCount
		     << "\nlast part = " << (lastpart ? (std::string("True")) : (std::string("False")))
		     << "\nVel = " << part.getVelocity()[0]
		     << "\nPos = " << part.getPosition()[0]
		     << "\nVwall[0] = " << fL.wallVelocity()[0]
		     << "\nRwall[0] = " << fL.wallPosition()[0]
		     << "\nRwall[0]+Sigma = " << fL.wallPosition()[0] + Sigma
		     << "\nRwall[0]-Sigma = " << fL.wallPosition()[0] - Sigma
		     << "\nSigma + Del = " << Sigma+Delta
		     << "\nGood root = " << fL.test_root(Sigma)
		     << "\nt_low1 = " << t_low1
		     << "\nt_low2 = " << t_low2
		     << "\nt_high = " << t_high
		     << "\nroot1 = " << root1
		     << "\nroot2 = " << root2
		     << "\nf1(0) = " << fl01
		     << "\nf1(t_low1) = " << flt_low1
		     << "\nf1(t_high) = " << flt_high1
		     << "\nf2(0)_1 = " << fl02
		     << "\nf2(t_low2) = " << flt_low2
		     << "\nf2(t_high) = " << flt_high2
		     << "\nf'(0) =" << fL.F_firstDeriv()
		     << "\nf''(Max) =" << fL.F_secondDeriv_max(0)
		     << "\nf(x)=" << (pos | nhat)
		     << "+" << (part.getVelocity() | nhat)
		     << " * x - "
		     << Delta 
		     << " * cos(("
		     << t + Sim->dSysTime << "+ x) * "
		     << Omega << ") - "
		     << Sigma
		     << "; set xrange[0:" << t_high << "]; plot f(x)";
	    ;
	  }
#endif
	  return 0;
	}
      else
	{
	  //The particle and plate are approaching but might not be
	  //before the overlap is fixed, schedule another test later
	  //on
	  Iflt currRoot = (root1 < root2) ? root1 : root2;
	  //
	  Iflt tmpt = fabs(surfaceVel - fL.velnHatWall());
	  //This next line sets what the recoil velocity should be
	  //We choose the velocity that gives elastic collisions!
	  tmpt += fL.maxWallVel() * 0.002;
	  tmpt /= fL.F_secondDeriv_max(Sigma);
	  if (tmpt < currRoot)
	    {
	      I_cout() << "Making a fake collision at " << tmpt << "for particle " << part.getID();

	      return tmpt;
	    }
	  else
	    I_cout() << "The current root is lower than the fake one";	    
	}
    }

  return (root1 < root2) ? root1 : root2;
}

ParticleEventData 
LNewtonian::runOscilatingPlate
(const Particle& part, const Vector& rw0, const Vector& nhat, Iflt& delta, 
 const Iflt& omega0, const Iflt& sigma, const Iflt& mass, const Iflt& e, 
 Iflt& t, bool strongPlate) const
{
  std::cout.flush();
  updateParticle(part);

  ParticleEventData retVal(part, Sim->dynamics.getSpecies(part), WALL);

  COscillatingPlateFunc fL(part.getVelocity(), nhat, part.getPosition(),
			   t + Sim->dSysTime, delta, omega0, sigma);

  //Should force the particle to the plate surface

  Vector pos(part.getPosition() - fL.wallPosition()), vel(part.getVelocity());

  Sim->dynamics.BCs().applyBC(pos, vel);
  
  Iflt pmass = retVal.getSpecies().getMass();
  Iflt mu = (pmass * mass) / (mass + pmass);

  Vector vwall(fL.wallVelocity());

//  I_cerr() << "Running event for part " << part.getID() <<
//	   "\ndSysTime = " << Sim->dSysTime << "\nlNColl = " <<
//	   Sim->lNColl << "\nVel = " << part.getVelocity()[0] <<
//	   "\nPos = " << part.getPosition()[0] << "\nVwall[0] = " <<
//	   fL.wallVelocity()[0] << "\nRwall[0] = " <<
//	   fL.wallPosition()[0] << "\nRwall[0]+sigma = " <<
//	   fL.wallPosition()[0] + sigma << "\nRwall[0]-sigma = " <<
//	   fL.wallPosition()[0] - sigma << "\nsigma + Del = " <<
//	   sigma+delta << "\nf(0)* = " << fL.F_zeroDeriv() << "\nf'(0)
//	   =" << fL.F_firstDeriv() << "\nf''(Max) =" <<
//	   fL.F_secondDeriv_max(0) << "\nf(x)=" << pos[0] << "+" <<
//	   part.getVelocity()[0] << " * x - " << delta << " * cos(("
//	   << t << "+ x) * " << omega0 << ") - " << sigma;

  
  //Check the root is valid
  if (!fL.test_root(sigma))
    {
      Iflt f0 = fL.F_zeroDeriv(), f1 = fL.F_firstDeriv(),
	f2 = fL.F_secondDeriv_max(0);
      fL.flipSigma();
      
      I_cerr() <<"Particle " << part.getID()
	       << ", is pulling on the oscillating plate!"
	       << "\nRunning event for part " << part.getID()
	       << "\ndSysTime = " << Sim->dSysTime
	       << "\nlNColl = " << Sim->eventCount
	       << "\nVel = " << part.getVelocity()[0]
	       << "\nPos = " << part.getPosition()[0]
	       << "\nVwall[0] = " << fL.wallVelocity()[0]
	       << "\nRwall[0] = " << fL.wallPosition()[0]
	       << "\nRwall[0]+sigma = " << fL.wallPosition()[0] + sigma
	       << "\nRwall[0]-sigma = " << fL.wallPosition()[0] - sigma
	       << "\nGood root " << fL.test_root(sigma)
	       << "\nsigma + Del = " << sigma+delta
	       << "\nf1(0)* = " << fL.F_zeroDeriv()
	       << "\nf1'(0) =" << fL.F_firstDeriv()
	       << "\nf1''(Max) =" << fL.F_secondDeriv_max(0)
	       << "\nf2(0)* = " << f0
	       << "\nf2'(0) =" << f1
	       << "\nf2''(Max) =" << f2
	       << "\nf(x)=" << (pos | nhat)
	       << "+" << (part.getVelocity() | nhat)
	       << " * x - "
	       << delta
	       << " * cos(("
	       << t + Sim->dSysTime << "+ x) * "
	       << omega0 << ") - "
	       << sigma;
      
      return retVal;
    }

  //static size_t elascount(0);

  Iflt inelas = e;

  Iflt rvdot = ((vel - vwall) | nhat);
  if (fabs(rvdot / fL.maxWallVel()) < 0.002)
    {
      /*
      I_cerr() <<"<<!!!>>Particle " << part.getID() 
	       << " gone elastic!\nratio is " << fabs(rvdot / vwall.nrm())
	       << "\nCount is " << ++elascount;
      */
      inelas = 1.0;
      if (fabs(rvdot / fL.maxWallVel()) < 0.001)
	if (rvdot < 0)
	  rvdot = -fL.maxWallVel() * 0.01;
	else
	  rvdot = fL.maxWallVel() * 0.01;
    }

  Vector delP =  nhat * mu * (1.0 + inelas) * rvdot;

  const_cast<Particle&>(part).getVelocity() -=  delP / pmass;

  retVal.setDeltaKE(0.5 * retVal.getSpecies().getMass()
		    * (part.getVelocity().nrm2() 
		       - retVal.getOldVel().nrm2()));
  
  //Don't progress if you want to not change the plate data
  if (strongPlate) return retVal;

  Iflt numerator = -nhat | ((delP / mass) + vwall);

  Iflt reducedt = Sim->dSysTime 
    - 2.0 * PI * int(Sim->dSysTime * omega0 / (2.0*PI)) / omega0;
  
  Iflt denominator = omega0 * delta * std::cos(omega0 * (reducedt + t));
  

  Iflt newt = std::atan2(numerator, denominator)/ omega0 
    - Sim->dSysTime;
  
  delta *= std::cos(omega0 * (Sim->dSysTime + t)) 
    / std::cos(omega0 * (Sim->dSysTime + newt));
  
  t = newt;

  t -= 2.0 * PI * int(t * omega0 / (2.0*PI)) / omega0;

  return retVal; 
}

Iflt 
LNewtonian::getCylinderWallCollision(const Particle& part, 
				   const Vector& wallLoc, 
				   const Vector& wallNorm,
				   const Iflt& radius) const
{
  Vector  rij = part.getPosition() - wallLoc,
    vel = part.getVelocity();

  Sim->dynamics.BCs().applyBC(rij, vel);

  rij -= Vector((rij | wallNorm) * wallNorm);

  vel -= Vector((vel | wallNorm) * wallNorm);

  Iflt B = (vel | rij),
    A = vel.nrm2(),
    C = rij.nrm2() - radius * radius;

  Iflt t = (std::sqrt(B*B - A*C) - B) / A;

  if (std::isnan(t))
    return HUGE_VAL;
  else
    return t;
}

ParticleEventData 
LNewtonian::runCylinderWallCollision(const Particle& part, 
				   const Vector& origin,
				   const Vector& vNorm,
				   const Iflt& e
				   ) const
{
  updateParticle(part);

  ParticleEventData retVal(part, Sim->dynamics.getSpecies(part), WALL);
  
  Vector rij =  origin - part.getPosition();

  Sim->dynamics.BCs().applyBC(rij);

  rij -= Vector((rij | vNorm) * vNorm);

  rij /= rij.nrm();

  const_cast<Particle&>(part).getVelocity()
    -= (1+e) * (rij | part.getVelocity()) * rij;
  
  retVal.setDeltaKE(0.5 * retVal.getSpecies().getMass()
		    * (part.getVelocity().nrm2() 
		       - retVal.getOldVel().nrm2()));
  
  return retVal; 
}

ParticleEventData 
LNewtonian::runSphereWallCollision(const Particle& part, 
				   const Vector& origin,
				   const Iflt& e
				   ) const
{
  updateParticle(part);

  ParticleEventData retVal(part, Sim->dynamics.getSpecies(part), WALL);
  
  Vector rij =  origin - part.getPosition();

  Sim->dynamics.BCs().applyBC(rij);

  rij /= rij.nrm();

  const_cast<Particle&>(part).getVelocity()
    -= (1+e) * (rij | part.getVelocity()) * rij;
  
  retVal.setDeltaKE(0.5 * retVal.getSpecies().getMass()
		    * (part.getVelocity().nrm2() 
		       - retVal.getOldVel().nrm2()));
  
  return retVal; 
}
