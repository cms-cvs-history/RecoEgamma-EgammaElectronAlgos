// Package:    EgammaElectronAlgos
// Class:      SeedFilter.

#include "RecoEgamma/EgammaElectronAlgos/interface/SeedFilter.h" 
#include "DataFormats/TrajectorySeed/interface/TrajectorySeedCollection.h"
#include "DataFormats/TrajectorySeed/interface/TrajectorySeed.h"
#include "RecoTracker/Record/interface/TrackerRecoGeometryRecord.h"
#include "Geometry/TrackerGeometryBuilder/interface/TrackerGeometry.h"
#include "RecoTracker/TkDetLayers/interface/GeometricSearchTracker.h"


#include "DataFormats/GeometryVector/interface/GlobalPoint.h"
#include "DataFormats/GeometryVector/interface/GlobalVector.h"
#include "DataFormats/Math/interface/Vector3D.h"

#include "FWCore/MessageLogger/interface/MessageLogger.h"
#include "DataFormats/Common/interface/Handle.h"
#include "FWCore/Framework/interface/ESHandle.h"
#include <vector>

#include "MagneticField/Engine/interface/MagneticField.h"
#include "MagneticField/Records/interface/IdealMagneticFieldRecord.h"

#include "RecoTracker/TkTrackingRegions/interface/OrderedHitsGeneratorFactory.h"
#include "RecoTracker/TkTrackingRegions/interface/OrderedHitsGenerator.h"
#include "RecoTracker/TkSeedGenerator/interface/SeedGeneratorFromRegionHits.h"

#include "CondFormats/DataRecord/interface/BeamSpotObjectsRcd.h"
#include "CondFormats/BeamSpotObjects/interface/BeamSpotObjects.h"

#include "DataFormats/VertexReco/interface/Vertex.h"
#include "DataFormats/VertexReco/interface/VertexFwd.h"


#include "RecoTracker/TkTrackingRegions/interface/RectangularEtaPhiTrackingRegion.h"

#include "DataFormats/TrackingRecHit/interface/TrackingRecHit.h"

#include "DataFormats/BeamSpot/interface/BeamSpot.h"
#include "DataFormats/Math/interface/Point3D.h"

using namespace std;
using namespace reco;

SeedFilter::SeedFilter(const edm::ParameterSet& conf) {

  edm::LogInfo ("EtaPhiRegionSeedFactory") << "Enter the EtaPhiRegionSeedFactory";
  edm::ParameterSet regionPSet = conf.getParameter<edm::ParameterSet>("RegionPSet");
  
  ptmin_        = regionPSet.getParameter<double>("ptMin");
  originradius_ = regionPSet.getParameter<double>("originRadius");
  halflength_   = regionPSet.getParameter<double>("originHalfLength");
  deltaEta_     = regionPSet.getParameter<double>("deltaEtaRegion");
  deltaPhi_     = regionPSet.getParameter<double>("deltaPhiRegion");
  useZvertex_   = regionPSet.getParameter<bool>("useZInVertex");
  vertexSrc_    = regionPSet.getParameter<edm::InputTag> ("VertexProducer");
  
  // setup orderedhits setup (in order to tell seed generator to use pairs/triplets, which layers)
  edm::ParameterSet hitsfactoryPSet = conf.getParameter<edm::ParameterSet>("OrderedHitsFactoryPSet");
  std::string hitsfactoryName = hitsfactoryPSet.getParameter<std::string>("ComponentName");
 
  // get orderd hits generator from factory
  OrderedHitsGenerator*  hitsGenerator = OrderedHitsGeneratorFactory::get()->create(hitsfactoryName, hitsfactoryPSet);
 
  // start seed generator
  combinatorialSeedGenerator = new SeedGeneratorFromRegionHits(hitsGenerator, conf);
}

SeedFilter::~SeedFilter() {
  delete combinatorialSeedGenerator;
}

void SeedFilter::seeds(edm::Event& e, const edm::EventSetup& setup, const reco::SuperClusterRef &scRef, TrajectorySeedCollection *output) {

  setup.get<IdealMagneticFieldRecord>().get(theMagField);
  std::auto_ptr<TrajectorySeedCollection> seedColl(new TrajectorySeedCollection());    
 
  GlobalPoint vtxPos;
  double deltaZVertex;
  
  //edm::Handle<reco::BeamSpot> recoBeamSpotHandle;
  //e.getByType(recoBeamSpotHandle);
    
  // gets its position
  //const reco::BeamSpot::Point& BSPosition = recoBeamSpotHandle->position();
  //vtxPos = GlobalPoint(BSPosition.x(), BSPosition.y(), BSPosition.z());
  //double sigmaZ = recoBeamSpotHandle->sigmaZ();
  //double sigmaZ0Error = recoBeamSpotHandle->sigmaZ0Error();
  //double sq=sqrt(sigmaZ*sigmaZ+sigmaZ0Error*sigmaZ0Error);
  //deltaZVertex = 3*sq; //halflength_;
      
  // get the primary vertex
  edm::Handle<reco::VertexCollection> h_vertices;
  e.getByLabel(vertexSrc_, h_vertices);
  
  const reco::VertexCollection & vertices = * h_vertices;
  
  if (!vertices.empty() && useZvertex_) {
    vtxPos = GlobalPoint(vertices.front().x(), vertices.front().y(), vertices.front().z());
    deltaZVertex = halflength_;
  } else {
    edm::Handle<reco::BeamSpot> recoBeamSpotHandle;
    e.getByType(recoBeamSpotHandle);
    // gets its position
    const reco::BeamSpot::Point& BSPosition = recoBeamSpotHandle->position();
    double sigmaZ = recoBeamSpotHandle->sigmaZ();
    double sigmaZ0Error = recoBeamSpotHandle->sigmaZ0Error();
    double sq=sqrt(sigmaZ*sigmaZ+sigmaZ0Error*sigmaZ0Error);
    vtxPos = GlobalPoint(BSPosition.x(), BSPosition.y(), BSPosition.z());
    deltaZVertex = 3*sq;
  }
 
  //seeds selection
  float energy = scRef->energy(); 
  
  const GlobalPoint clusterPos(scRef->position().x(), scRef->position().y(), scRef->position().z());   
  
  // EtaPhiRegion and seeds for electron hypothesis    
  TrackCharge aCharge = -1.; 
  FreeTrajectoryState fts = myFTS(&(*theMagField), clusterPos, vtxPos, energy, aCharge);

  RectangularEtaPhiTrackingRegion etaphiRegionMinus(fts.momentum(),
                                                    vtxPos, 
                                                    ptmin_,
                                                    originradius_,
                                                    deltaZVertex,
						    //                                                    deltaEta_,
						    //                                                    deltaPhi_,-1);
						    RectangularEtaPhiTrackingRegion::Margin(fabs(deltaEta_),fabs(deltaEta_)), 
						    RectangularEtaPhiTrackingRegion::Margin(fabs(deltaPhi_),fabs(deltaPhi_))
						    ,-1);
  combinatorialSeedGenerator->run(*seedColl, etaphiRegionMinus, e, setup);
  
  for (unsigned int i = 0; i<seedColl->size(); ++i)
    output->push_back((*seedColl)[i]); 
  
  seedColl->clear();

  // EtaPhiRegion and seeds for positron hypothesis
  TrackCharge aChargep = 1.; 
  fts = myFTS(&(*theMagField), clusterPos, vtxPos, energy, aChargep);
  
  RectangularEtaPhiTrackingRegion etaphiRegionPlus(fts.momentum(),
                                                   vtxPos, 
                                                   ptmin_,
                                                   originradius_,
                                                   deltaZVertex,
						   //                                                   deltaEta_,
						   //                                                   deltaPhi_,-1);
						    RectangularEtaPhiTrackingRegion::Margin(fabs(deltaEta_),fabs(deltaEta_)), 
						    RectangularEtaPhiTrackingRegion::Margin(fabs(deltaPhi_),fabs(deltaPhi_))
						    ,-1);
  
  combinatorialSeedGenerator->run(*seedColl, etaphiRegionPlus, e, setup);
  
  for (unsigned int i = 0; i<seedColl->size(); ++i){
    TrajectorySeed::range r = (*seedColl)[i].recHits();
    // first Hit 
    TrajectorySeed::const_iterator it = r.first;
    const TrackingRecHit* a1 = &(*it); 
    // now second Hit
    it++;
    const TrackingRecHit* a2 = &(*it);
    // check if the seed is already in the collection
    bool isInCollection = false;
    for(unsigned int j=0; j<output->size(); ++j) {
      TrajectorySeed::range r = (*seedColl)[i].recHits();
      // first Hit       
      TrajectorySeed::const_iterator it = r.first;
      const TrackingRecHit* b1 = &(*it);
      // now second Hit
      it++;
      const TrackingRecHit* b2 = &(*it); 
      if ((b1->sharesInput(a1, TrackingRecHit::all)) && (b2->sharesInput(a2, TrackingRecHit::all))) {
	isInCollection = true;
	break;
      }
    }
    if (!isInCollection)
      output->push_back((*seedColl)[i]); 
  }
  
  seedColl->clear();
}
