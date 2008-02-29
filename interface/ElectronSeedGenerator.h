#ifndef ElectronSeedGenerator_H
#define ElectronSeedGenerator_H

/** \class ElectronSeedGenerator
 * base class for ElectronPixelSeedGenerator and SubSeedGenerator
 ************************************************************/

#include "DataFormats/EgammaReco/interface//ElectronPixelSeed.h"  
#include "DataFormats/EgammaReco/interface/ElectronPixelSeedFwd.h"  
#include "DataFormats/EgammaReco/interface/SuperClusterFwd.h"  
#include "DataFormats/EgammaReco/interface/SuperCluster.h"

#include "FWCore/Framework/interface/EventSetup.h"
#include "FWCore/Framework/interface/Event.h"

class ElectronSeedGenerator
{
public:

  
  ElectronSeedGenerator() {;}

  virtual ~ElectronSeedGenerator() {;}

  virtual void setupES(const edm::EventSetup& setup) {;}
  // temporary, will be cleaned up...
  virtual void run(edm::Event&, const edm::EventSetup& setup, const reco::SuperClusterRefVector &, reco::ElectronPixelSeedCollection&) {;}
  virtual void run(edm::Event&, const edm::EventSetup& setup, const edm::Handle<reco::SuperClusterCollection>&, reco::ElectronPixelSeedCollection&) {;}
 private:
};

#endif // ElectronSeedGenerator_H


