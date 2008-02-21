// -*- C++ -*-
//
// Package:    EgammaElectronAlgos
// Class:      ForwardMeasurementEstimator
// 
/**\class ElectronPixelSeedProducer EgammaElectronAlgos/ForwardMeasurementEstimator

 Description: MeasurementEstimator for Pixel Endcap, ported from ORCA

 Implementation:
     <Notes on implementation>
*/
//
// Original Author:  Ursula Berthon, Claude Charlot
//         Created:  Mon Mar 27 13:22:06 CEST 2006
// $Id: ForwardMeasurementEstimator.cc,v 1.3 2007/02/05 17:53:52 uberthon Exp $
//
//
#include "RecoEgamma/EgammaElectronAlgos/interface/ForwardMeasurementEstimator.h"
#include "CLHEP/Units/PhysicalConstants.h"
#include "TrackingTools/TrajectoryParametrization/interface/GlobalTrajectoryParameters.h"
#include "RecoTracker/TkTrackingRegions/interface/GlobalDetRangeRPhi.h"
#include "TrackingTools/DetLayers/interface/rangesIntersect.h"
#include "TrackingTools/DetLayers/interface/PhiLess.h"

// zero value indicates incompatible ts - hit pair
std::pair<bool,double> ForwardMeasurementEstimator::estimate( const TrajectoryStateOnSurface& ts, 
							      const TransientTrackingRecHit& hit) const {

  float tsR = ts.globalParameters().position().perp();
  float tsPhi = ts.globalParameters().position().phi();
  LocalPoint lp = hit.localPosition();
  GlobalPoint gp = hit.det()->surface().toGlobal( lp);
  float rhPhi = gp.phi();
  float rhR = gp.perp();

  //  float myR = gp.perp();
  float myZ = gp.z();

  
  float rMin = theZRangeMin;
  float rMax = theZRangeMax;
  float myPhimin = thePhiRangeMin;
  float myPhimax = thePhiRangeMax;

  
//   if(fabs(myZ)> 70. &&  fabs(myZ)<170.)
//     {
//       rMin = -0.2;
//       rMax = 0.2;
//     }

  if(fabs(myZ)> 70. &&  fabs(myZ)<170.)
    {
      rMin = rMin_;
      rMax = rMax_;
    }

  float phiDiff = tsPhi - rhPhi;
  if (phiDiff > pi) phiDiff -= twopi;
  if (phiDiff < -pi) phiDiff += twopi; 
  
  //  float rDiff = rMax -rMin;

  float rDiff = tsR - rhR;

   
  if ( phiDiff < myPhimax && phiDiff > myPhimin && 
       rDiff < rMax && rDiff > rMin) {
     return std::pair<bool,double>(true,1.);
   } else {
    return std::pair<bool,double>(false,0.);
    }
}

bool ForwardMeasurementEstimator::estimate( const TrajectoryStateOnSurface& ts, 
					    const BoundPlane& plane) const {

  typedef std::pair<float,float>   Range;

  GlobalPoint trajPos(ts.globalParameters().position());
  GlobalDetRangeRPhi detRange(plane);

  float theRMin = 0.;
  float theRMax = 40.; 

  Range trajRRange(trajPos.perp() - theRMin, trajPos.perp() + theRMax);
  Range trajPhiRange(trajPos.phi() - fabs(thePhiRangeMin), trajPos.phi() + fabs(thePhiRangeMax));

  if(rangesIntersect(trajRRange, detRange.rRange()) &&
     rangesIntersect(trajPhiRange, detRange.phiRange(), PhiLess())) {
    return true;
  } else {
    return false;
  }
}

MeasurementEstimator::Local2DVector 
ForwardMeasurementEstimator::maximalLocalDisplacement( const TrajectoryStateOnSurface& ts,
						       const BoundPlane& plane) const
{
  // temporary version
  float nSigmaCut = 3.;
  if ( ts.hasError()) {
    LocalError le = ts.localError().positionError();
    //    return Local2DVector( sqrt(le.xx())*nSigmaCut(), sqrt(le.yy())*nSigmaCut());
    return Local2DVector( sqrt(le.xx())*nSigmaCut, sqrt(le.yy())*nSigmaCut);
  }
  //UB FIXME!!!
  else return Local2DVector(999999,999999);
}



